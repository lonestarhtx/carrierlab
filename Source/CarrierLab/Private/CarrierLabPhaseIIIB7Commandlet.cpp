// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIB7Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	enum class EMaterialFixture : uint8
	{
		Default,
		MixedPlate0Continental,
		AllOceanic
	};

	struct FAgeFixture
	{
		bool bApply = false;
		double Plate0AgeMa = 0.0;
		double Plate1AgeMa = 0.0;
	};

	struct FHashClosureStepRow
	{
		FString Fixture;
		int32 Replay = 0;
		FString Phase;
		FCarrierLabPhaseIIIB7HashClosureAudit Audit;
		double SecondsFromStart = 0.0;
	};

	struct FHashClosureReplayResult
	{
		FString Fixture;
		int32 Replay = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		bool bFireResampleEvent = false;
		bool bCompleted = false;
		double TotalSeconds = 0.0;
		TArray<FHashClosureStepRow> Rows;
	};

	FString JsonString(const FString& Value)
	{
		FString Escaped;
		Escaped.Reserve(Value.Len() + 2);
		for (const TCHAR Ch : Value)
		{
			switch (Ch)
			{
			case TEXT('\\'): Escaped += TEXT("\\\\"); break;
			case TEXT('"'): Escaped += TEXT("\\\""); break;
			case TEXT('\n'): Escaped += TEXT("\\n"); break;
			case TEXT('\r'): Escaped += TEXT("\\r"); break;
			case TEXT('\t'): Escaped += TEXT("\\t"); break;
			default: Escaped.AppendChar(Ch); break;
			}
		}
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}

	FString PassFail(const bool bPass)
	{
		return bPass ? TEXT("pass") : TEXT("fail");
	}

	int32 ParseIntParam(const FString& Params, const TCHAR* Key, const int32 DefaultValue)
	{
		int32 Value = DefaultValue;
		FParse::Value(*Params, Key, Value);
		return Value;
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIB7"), Stamp);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	FString ResolveReportPath(const FString& Params)
	{
		FString ReportPath;
		if (!FParse::Value(*Params, TEXT("Report="), ReportPath))
		{
			ReportPath = FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("docs"),
				TEXT("checkpoints"),
				TEXT("phase-iii-slice-iiib7-report.md"));
		}
		else if (FPaths::IsRelative(ReportPath))
		{
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), ReportPath);
		}
		return FPaths::ConvertRelativePathToFull(ReportPath);
	}

	UWorld* GetCommandletWorld()
	{
		if (GEngine != nullptr)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.World() != nullptr)
				{
					return Context.World();
				}
			}
		}
		return GWorld;
	}

	ACarrierLabVisualizationActor* SpawnActor(UWorld& World, const int32 SampleCount, const int32 PlateCount)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.bDeferConstruction = true;
		ACarrierLabVisualizationActor* Actor = World.SpawnActor<ACarrierLabVisualizationActor>(
			ACarrierLabVisualizationActor::StaticClass(),
			FTransform::Identity,
			SpawnParams);
		if (Actor == nullptr)
		{
			return nullptr;
		}

		Actor->bAutoInitialize = false;
		Actor->bPlayOnBegin = false;
		Actor->bShowHud = false;
		Actor->SampleCount = SampleCount;
		Actor->PlateCount = PlateCount;
		Actor->Seed = 42;
		Actor->ContinentalPlateFraction = 0.30;
		Actor->VelocityMmPerYear = 66.6666666667;
		Actor->MultiHitPolicy = ECarrierLabMultiHitPolicy::Centroid;
		Actor->VisualizationLayer = ECarrierLabVisualizationLayer::PlateId;
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	bool ApplyMaterialFixture(ACarrierLabVisualizationActor& Actor, const EMaterialFixture Fixture)
	{
		switch (Fixture)
		{
		case EMaterialFixture::MixedPlate0Continental:
			return Actor.SetPlateContinentalForTest(0, true) &&
				Actor.SetPlateContinentalForTest(1, false);
		case EMaterialFixture::AllOceanic:
			return Actor.SetPlateContinentalForTest(0, false) &&
				Actor.SetPlateContinentalForTest(1, false);
		case EMaterialFixture::Default:
		default:
			return true;
		}
	}

	bool ApplyAgeFixture(ACarrierLabVisualizationActor& Actor, const FAgeFixture& Fixture)
	{
		if (!Fixture.bApply)
		{
			return true;
		}
		return Actor.SetPlateOceanicAgeForTest(0, Fixture.Plate0AgeMa) &&
			Actor.SetPlateOceanicAgeForTest(1, Fixture.Plate1AgeMa);
	}

	FString RowKey(const FHashClosureStepRow& Row)
	{
		const FCarrierLabPhaseIIIB7HashClosureAudit& Audit = Row.Audit;
		return FString::Printf(
			TEXT("%s|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%d|%s|%s|%s|%s|%s|%s"),
			*Row.Phase,
			Audit.Step,
			Audit.EventCount,
			Audit.ResetSerial,
			Audit.ActiveTriangleCount,
			Audit.DistanceRecordCount,
			Audit.MatrixPairCount,
			Audit.MatrixRayTestCount,
			Audit.MatrixHitCount,
			Audit.MatrixBoundaryHitCount,
			Audit.MatrixNonConvergentHitCount,
			Audit.PolarityDecisionCount,
			Audit.TriangleHitCount,
			Audit.PropagationSeedHitCount,
			Audit.PropagationAddedCount,
			Audit.PropagationDuplicateCount,
			Audit.PropagationDistanceRejectedCount,
			Audit.PropagationInvalidCount,
			*Audit.ProjectionHash,
			*Audit.StateHash,
			*Audit.CrustStateHash,
			*Audit.MetricsConvergenceTrackingHash,
			*Audit.ComputedConvergenceTrackingHash,
			Audit.bMetricsHashMatchesComputed ? TEXT("closed") : TEXT("open"));
	}

	bool CaptureRow(
		ACarrierLabVisualizationActor& Actor,
		const FString& Fixture,
		const int32 Replay,
		const FString& Phase,
		const double StartSeconds,
		TArray<FHashClosureStepRow>& Rows)
	{
		FHashClosureStepRow& Row = Rows.AddDefaulted_GetRef();
		Row.Fixture = Fixture;
		Row.Replay = Replay;
		Row.Phase = Phase;
		Row.SecondsFromStart = FPlatformTime::Seconds() - StartSeconds;
		return Actor.GetPhaseIIIB7HashClosureAudit(Row.Audit);
	}

	bool RunHashClosureReplay(
		const FString& Fixture,
		const ECarrierLabPhaseIIMotionFixture MotionFixture,
		const EMaterialFixture MaterialFixture,
		const FAgeFixture AgeFixture,
		const bool bFireResampleEvent,
		const int32 Replay,
		const int32 SampleCount,
		const int32 PlateCount,
		const int32 StepCount,
		FHashClosureReplayResult& OutResult)
	{
		OutResult = FHashClosureReplayResult();
		OutResult.Fixture = Fixture;
		OutResult.Replay = Replay;
		OutResult.SampleCount = SampleCount;
		OutResult.PlateCount = PlateCount;
		OutResult.StepCount = StepCount;
		OutResult.bFireResampleEvent = bFireResampleEvent;

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double TotalStartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, SampleCount, PlateCount);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier())
		{
			Actor->Destroy();
			return false;
		}
		Actor->ConfigurePhaseIIMotionFixture(MotionFixture);
		if (!ApplyMaterialFixture(*Actor, MaterialFixture) ||
			!ApplyAgeFixture(*Actor, AgeFixture))
		{
			Actor->Destroy();
			return false;
		}

		if (!CaptureRow(*Actor, Fixture, Replay, TEXT("initial"), TotalStartSeconds, OutResult.Rows))
		{
			Actor->Destroy();
			return false;
		}
		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			Actor->StepOnce();
			if (!CaptureRow(*Actor, Fixture, Replay, FString::Printf(TEXT("step_%03d"), Step + 1), TotalStartSeconds, OutResult.Rows))
			{
				Actor->Destroy();
				return false;
			}
		}
		if (bFireResampleEvent)
		{
			Actor->ApplyResampleEvent();
			if (!CaptureRow(*Actor, Fixture, Replay, TEXT("after_resample_event"), TotalStartSeconds, OutResult.Rows))
			{
				Actor->Destroy();
				return false;
			}
		}

		OutResult.TotalSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
		OutResult.bCompleted = true;
		Actor->Destroy();
		return true;
	}

	bool AllRowsClosed(const FHashClosureReplayResult& Result)
	{
		if (!Result.bCompleted || Result.Rows.IsEmpty())
		{
			return false;
		}
		for (const FHashClosureStepRow& Row : Result.Rows)
		{
			if (!Row.Audit.bMetricsHashMatchesComputed)
			{
				return false;
			}
		}
		return true;
	}

	bool ReplayRowsMatch(const FHashClosureReplayResult& A, const FHashClosureReplayResult& B)
	{
		if (!A.bCompleted || !B.bCompleted || A.Rows.Num() != B.Rows.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Rows.Num(); ++Index)
		{
			if (RowKey(A.Rows[Index]) != RowKey(B.Rows[Index]))
			{
				return false;
			}
		}
		return true;
	}

	bool PhaseIIHashesReplayStable(const FHashClosureReplayResult& A, const FHashClosureReplayResult& B)
	{
		if (!A.bCompleted || !B.bCompleted || A.Rows.Num() != B.Rows.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Rows.Num(); ++Index)
		{
			if (A.Rows[Index].Audit.ProjectionHash != B.Rows[Index].Audit.ProjectionHash ||
				A.Rows[Index].Audit.StateHash != B.Rows[Index].Audit.StateHash ||
				A.Rows[Index].Audit.CrustStateHash != B.Rows[Index].Audit.CrustStateHash)
			{
				return false;
			}
		}
		return true;
	}

	bool TrackingStateExercised(const FHashClosureReplayResult& Result)
	{
		if (!Result.bCompleted || Result.Rows.IsEmpty())
		{
			return false;
		}
		const FCarrierLabPhaseIIIB7HashClosureAudit& Final = Result.Rows.Last().Audit;
		return Final.ActiveTriangleCount > 0 &&
			Final.DistanceRecordCount == Final.ActiveTriangleCount &&
			Final.MatrixPairCount > 0 &&
			Final.PolarityDecisionCount == Final.MatrixPairCount &&
			Final.TriangleHitCount > 0 &&
			Final.PropagationSeedHitCount > 0;
	}

	bool ZeroMotionNoop(const FHashClosureReplayResult& Result)
	{
		if (!Result.bCompleted || Result.Rows.Num() < 2)
		{
			return false;
		}
		const FCarrierLabPhaseIIIB7HashClosureAudit& Initial = Result.Rows[0].Audit;
		const FCarrierLabPhaseIIIB7HashClosureAudit& Final = Result.Rows.Last().Audit;
		return Final.EventCount == 0 &&
			Initial.ComputedConvergenceTrackingHash == Final.ComputedConvergenceTrackingHash &&
			Final.MatrixPairCount == 0 &&
			Final.PolarityDecisionCount == 0 &&
			Final.TriangleHitCount == 0 &&
			Final.PropagationAddedCount == 0;
	}

	bool EventClosureExercised(const FHashClosureReplayResult& A, const FHashClosureReplayResult& B)
	{
		if (!ReplayRowsMatch(A, B) || !AllRowsClosed(A) || A.Rows.IsEmpty())
		{
			return false;
		}
		const FCarrierLabPhaseIIIB7HashClosureAudit& Initial = A.Rows[0].Audit;
		const FCarrierLabPhaseIIIB7HashClosureAudit& Final = A.Rows.Last().Audit;
		return Final.EventCount == 1 &&
			Final.ResetSerial > Initial.ResetSerial &&
			Final.ComputedConvergenceTrackingHash != Initial.ComputedConvergenceTrackingHash;
	}

	FString RowJson(const FHashClosureStepRow& Row)
	{
		const FCarrierLabPhaseIIIB7HashClosureAudit& Audit = Row.Audit;
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"phase\":%s,\"seconds\":%.6f,\"step\":%d,\"events\":%d,\"samples\":%d,\"plates\":%d,\"reset_serial\":%d,\"active_triangles\":%d,\"distance_records\":%d,\"matrix_pairs\":%d,\"matrix_ray_tests\":%d,\"matrix_hits\":%d,\"matrix_boundary_hits\":%d,\"matrix_non_convergent_hits\":%d,\"polarity_decisions\":%d,\"triangle_hits\":%d,\"propagation_seed_hits\":%d,\"propagation_added\":%d,\"propagation_duplicates\":%d,\"propagation_rejected_distance\":%d,\"propagation_invalid\":%d,\"projection_hash\":%s,\"state_hash\":%s,\"crust_state_hash\":%s,\"metrics_convergence_tracking_hash\":%s,\"computed_convergence_tracking_hash\":%s,\"metrics_hash_matches_computed\":%s,\"row_key\":%s,\"memory_gb\":%.12f}"),
			*JsonString(Row.Fixture),
			Row.Replay,
			*JsonString(Row.Phase),
			Row.SecondsFromStart,
			Audit.Step,
			Audit.EventCount,
			Audit.SampleCount,
			Audit.PlateCount,
			Audit.ResetSerial,
			Audit.ActiveTriangleCount,
			Audit.DistanceRecordCount,
			Audit.MatrixPairCount,
			Audit.MatrixRayTestCount,
			Audit.MatrixHitCount,
			Audit.MatrixBoundaryHitCount,
			Audit.MatrixNonConvergentHitCount,
			Audit.PolarityDecisionCount,
			Audit.TriangleHitCount,
			Audit.PropagationSeedHitCount,
			Audit.PropagationAddedCount,
			Audit.PropagationDuplicateCount,
			Audit.PropagationDistanceRejectedCount,
			Audit.PropagationInvalidCount,
			*JsonString(Audit.ProjectionHash),
			*JsonString(Audit.StateHash),
			*JsonString(Audit.CrustStateHash),
			*JsonString(Audit.MetricsConvergenceTrackingHash),
			*JsonString(Audit.ComputedConvergenceTrackingHash),
			Audit.bMetricsHashMatchesComputed ? TEXT("true") : TEXT("false"),
			*JsonString(RowKey(Row)),
			MemoryGb);
	}

	void AppendRowsJson(FString& MetricsJsonl, const FHashClosureReplayResult& Result)
	{
		for (const FHashClosureStepRow& Row : Result.Rows)
		{
			MetricsJsonl += RowJson(Row) + LINE_TERMINATOR;
		}
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FHashClosureReplayResult& MixedA,
		const FHashClosureReplayResult& MixedB,
		const FHashClosureReplayResult& OceanA,
		const FHashClosureReplayResult& OceanB,
		const FHashClosureReplayResult& DivergenceA,
		const FHashClosureReplayResult& DivergenceB,
		const FHashClosureReplayResult& ZeroA,
		const FHashClosureReplayResult& ZeroB,
		const FHashClosureReplayResult& EventA,
		const FHashClosureReplayResult& EventB)
	{
		const bool bMixedReplay = ReplayRowsMatch(MixedA, MixedB) && AllRowsClosed(MixedA) && TrackingStateExercised(MixedA);
		const bool bOceanReplay = ReplayRowsMatch(OceanA, OceanB) && AllRowsClosed(OceanA) && TrackingStateExercised(OceanA);
		const bool bDivergenceReplay = ReplayRowsMatch(DivergenceA, DivergenceB) && AllRowsClosed(DivergenceA) &&
			DivergenceA.Rows.Last().Audit.EventCount == 0;
		const bool bZeroReplay = ReplayRowsMatch(ZeroA, ZeroB) && AllRowsClosed(ZeroA) && ZeroMotionNoop(ZeroA);
		const bool bEventClosure = EventClosureExercised(EventA, EventB);
		const bool bPhaseIIHashes = PhaseIIHashesReplayStable(MixedA, MixedB) &&
			PhaseIIHashesReplayStable(OceanA, OceanB) &&
			PhaseIIHashesReplayStable(DivergenceA, DivergenceB) &&
			PhaseIIHashesReplayStable(ZeroA, ZeroB) &&
			PhaseIIHashesReplayStable(EventA, EventB);
		const bool bAllPass = bMixedReplay && bOceanReplay && bDivergenceReplay && bZeroReplay && bEventClosure && bPhaseIIHashes;

		FString Report = TEXT("# Phase III Slice IIIB.7 Checkpoint: Replay/Event Hash Closure\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This slice closes the IIIB tracking hash around active triangle membership, distance-to-front records, convergence matrix evidence, polarity decisions, triangle-hit records, neighbor-propagation counters, and resampling reset events. It adds an audit surface and commandlet metrics only; it does not mark triangles, filter projection candidates, resample during normal steps, or mutate crust material beyond the explicit event fixture.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n|---|---|---|\n");
		Report += FString::Printf(TEXT("| Mixed-material replay hash closure | %s | final hash `%s`, active %d, matrix pairs %d, decisions %d, hits %d, seed hits %d |\n"),
			*PassFail(bMixedReplay),
			*MixedA.Rows.Last().Audit.ComputedConvergenceTrackingHash,
			MixedA.Rows.Last().Audit.ActiveTriangleCount,
			MixedA.Rows.Last().Audit.MatrixPairCount,
			MixedA.Rows.Last().Audit.PolarityDecisionCount,
			MixedA.Rows.Last().Audit.TriangleHitCount,
			MixedA.Rows.Last().Audit.PropagationSeedHitCount);
		Report += FString::Printf(TEXT("| Ocean-age replay hash closure | %s | final hash `%s`, active %d, matrix pairs %d, decisions %d, hits %d, seed hits %d |\n"),
			*PassFail(bOceanReplay),
			*OceanA.Rows.Last().Audit.ComputedConvergenceTrackingHash,
			OceanA.Rows.Last().Audit.ActiveTriangleCount,
			OceanA.Rows.Last().Audit.MatrixPairCount,
			OceanA.Rows.Last().Audit.PolarityDecisionCount,
			OceanA.Rows.Last().Audit.TriangleHitCount,
			OceanA.Rows.Last().Audit.PropagationSeedHitCount);
		Report += FString::Printf(TEXT("| Forced-divergence replay stays deterministic | %s | final hash `%s`, event count %d |\n"),
			*PassFail(bDivergenceReplay),
			*DivergenceA.Rows.Last().Audit.ComputedConvergenceTrackingHash,
			DivergenceA.Rows.Last().Audit.EventCount);
		Report += FString::Printf(TEXT("| Zero-motion no-op remains hash-stable | %s | `%s` -> `%s` |\n"),
			*PassFail(bZeroReplay),
			*ZeroA.Rows[0].Audit.ComputedConvergenceTrackingHash,
			*ZeroA.Rows.Last().Audit.ComputedConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| Explicit resample event is hash-closed | %s | reset %d -> %d, event count %d, hash `%s` -> `%s` |\n"),
			*PassFail(bEventClosure),
			EventA.Rows[0].Audit.ResetSerial,
			EventA.Rows.Last().Audit.ResetSerial,
			EventA.Rows.Last().Audit.EventCount,
			*EventA.Rows[0].Audit.ComputedConvergenceTrackingHash,
			*EventA.Rows.Last().Audit.ComputedConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| Phase II projection/state/crust hashes replay-stable | %s | mixed final projection `%s`, state `%s`, crust `%s` |\n"),
			*PassFail(bPhaseIIHashes),
			*MixedA.Rows.Last().Audit.ProjectionHash,
			*MixedA.Rows.Last().Audit.StateHash,
			*MixedA.Rows.Last().Audit.CrustStateHash);

		Report += TEXT("\n## Final Replay Rows\n\n");
		Report += TEXT("| Fixture | Replay | Phase | Step | Events | Reset | Active | Dist | Matrix | Decisions | Hits | Added | Metrics Hash | Computed Hash |\n");
		Report += TEXT("|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|\n");
		auto AddFinalRow = [&Report](const FHashClosureReplayResult& Result)
		{
			const FHashClosureStepRow& Row = Result.Rows.Last();
			const FCarrierLabPhaseIIIB7HashClosureAudit& Audit = Row.Audit;
			Report += FString::Printf(
				TEXT("| %s | %d | %s | %d | %d | %d | %d | %d | %d | %d | %d | %d | `%s` | `%s` |\n"),
				*Result.Fixture,
				Result.Replay,
				*Row.Phase,
				Audit.Step,
				Audit.EventCount,
				Audit.ResetSerial,
				Audit.ActiveTriangleCount,
				Audit.DistanceRecordCount,
				Audit.MatrixPairCount,
				Audit.PolarityDecisionCount,
				Audit.TriangleHitCount,
				Audit.PropagationAddedCount,
				*Audit.MetricsConvergenceTrackingHash,
				*Audit.ComputedConvergenceTrackingHash);
		};
		AddFinalRow(MixedA);
		AddFinalRow(MixedB);
		AddFinalRow(OceanA);
		AddFinalRow(OceanB);
		AddFinalRow(DivergenceA);
		AddFinalRow(DivergenceB);
		AddFinalRow(ZeroA);
		AddFinalRow(ZeroB);
		AddFinalRow(EventA);
		AddFinalRow(EventB);

		Report += TEXT("\n## Notes\n\n");
		Report += TEXT("- `MetricsConvergenceTrackingHash` is emitted by the normal actor metrics path; `ComputedConvergenceTrackingHash` is recomputed by the IIIB.7 audit at capture time. Every row requires equality.\n");
		Report += TEXT("- The tracking hash is Phase III diagnostic state. Phase II projection, state, and crust hashes are reported separately and replay-compared so this slice does not hide behavior changes behind a new tracking hash.\n");
		Report += TEXT("- The resample fixture exists only to prove reset/event closure. Normal tracking fixtures keep event count at zero.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIB.7 passes. Pause for user review before Phase IIIC planning or implementation.\n")
			: TEXT("IIIB.7 does not pass. Investigate hash closure before Phase IIIC.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIB7Commandlet::UCarrierLabPhaseIIIB7Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIB7Commandlet::Main(const FString& Params)
{
	const int32 FixtureSamples = FMath::Max(12, ParseIntParam(Params, TEXT("FixtureSamples="), 10000));
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	const FAgeFixture NoAges{ false, 0.0, 0.0 };
	const FAgeFixture OceanAgeDelta{ true, 96.0, 24.0 };

	FHashClosureReplayResult MixedA;
	FHashClosureReplayResult MixedB;
	const bool bMixedA = RunHashClosureReplay(TEXT("mixed_convergence_tracking"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::MixedPlate0Continental, NoAges, false, 0, FixtureSamples, 2, 8, MixedA);
	const bool bMixedB = RunHashClosureReplay(TEXT("mixed_convergence_tracking"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::MixedPlate0Continental, NoAges, false, 1, FixtureSamples, 2, 8, MixedB);

	FHashClosureReplayResult OceanA;
	FHashClosureReplayResult OceanB;
	const bool bOceanA = RunHashClosureReplay(TEXT("ocean_age_convergence_tracking"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllOceanic, OceanAgeDelta, false, 0, FixtureSamples, 2, 8, OceanA);
	const bool bOceanB = RunHashClosureReplay(TEXT("ocean_age_convergence_tracking"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllOceanic, OceanAgeDelta, false, 1, FixtureSamples, 2, 8, OceanB);

	FHashClosureReplayResult DivergenceA;
	FHashClosureReplayResult DivergenceB;
	const bool bDivergenceA = RunHashClosureReplay(TEXT("forced_divergence_tracking"), ECarrierLabPhaseIIMotionFixture::ForcedDivergence, EMaterialFixture::MixedPlate0Continental, NoAges, false, 0, FixtureSamples, 2, 2, DivergenceA);
	const bool bDivergenceB = RunHashClosureReplay(TEXT("forced_divergence_tracking"), ECarrierLabPhaseIIMotionFixture::ForcedDivergence, EMaterialFixture::MixedPlate0Continental, NoAges, false, 1, FixtureSamples, 2, 2, DivergenceB);

	FHashClosureReplayResult ZeroA;
	FHashClosureReplayResult ZeroB;
	const bool bZeroA = RunHashClosureReplay(TEXT("zero_motion_noop"), ECarrierLabPhaseIIMotionFixture::Zero, EMaterialFixture::Default, NoAges, false, 0, FixtureSamples, 40, 10, ZeroA);
	const bool bZeroB = RunHashClosureReplay(TEXT("zero_motion_noop"), ECarrierLabPhaseIIMotionFixture::Zero, EMaterialFixture::Default, NoAges, false, 1, FixtureSamples, 40, 10, ZeroB);

	FHashClosureReplayResult EventA;
	FHashClosureReplayResult EventB;
	const bool bEventA = RunHashClosureReplay(TEXT("resample_event_hash_closure"), ECarrierLabPhaseIIMotionFixture::ForcedDivergence, EMaterialFixture::MixedPlate0Continental, NoAges, true, 0, FixtureSamples, 2, 2, EventA);
	const bool bEventB = RunHashClosureReplay(TEXT("resample_event_hash_closure"), ECarrierLabPhaseIIMotionFixture::ForcedDivergence, EMaterialFixture::MixedPlate0Continental, NoAges, true, 1, FixtureSamples, 2, 2, EventB);

	FString MetricsJsonl;
	AppendRowsJson(MetricsJsonl, MixedA);
	AppendRowsJson(MetricsJsonl, MixedB);
	AppendRowsJson(MetricsJsonl, OceanA);
	AppendRowsJson(MetricsJsonl, OceanB);
	AppendRowsJson(MetricsJsonl, DivergenceA);
	AppendRowsJson(MetricsJsonl, DivergenceB);
	AppendRowsJson(MetricsJsonl, ZeroA);
	AppendRowsJson(MetricsJsonl, ZeroB);
	AppendRowsJson(MetricsJsonl, EventA);
	AppendRowsJson(MetricsJsonl, EventB);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);

	const FString Report = BuildReport(OutputRoot, MixedA, MixedB, OceanA, OceanB, DivergenceA, DivergenceB, ZeroA, ZeroB, EventA, EventB);
	const FString ReportPath = ResolveReportPath(Params);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.7 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.7 report: %s"), *ReportPath);

	const bool bMixedReplay = bMixedA && bMixedB && ReplayRowsMatch(MixedA, MixedB) && AllRowsClosed(MixedA) && TrackingStateExercised(MixedA);
	const bool bOceanReplay = bOceanA && bOceanB && ReplayRowsMatch(OceanA, OceanB) && AllRowsClosed(OceanA) && TrackingStateExercised(OceanA);
	const bool bDivergenceReplay = bDivergenceA && bDivergenceB && ReplayRowsMatch(DivergenceA, DivergenceB) && AllRowsClosed(DivergenceA) &&
		DivergenceA.Rows.Last().Audit.EventCount == 0;
	const bool bZeroReplay = bZeroA && bZeroB && ReplayRowsMatch(ZeroA, ZeroB) && AllRowsClosed(ZeroA) && ZeroMotionNoop(ZeroA);
	const bool bEventClosure = bEventA && bEventB && EventClosureExercised(EventA, EventB);
	const bool bPhaseIIHashes = PhaseIIHashesReplayStable(MixedA, MixedB) &&
		PhaseIIHashesReplayStable(OceanA, OceanB) &&
		PhaseIIHashesReplayStable(DivergenceA, DivergenceB) &&
		PhaseIIHashesReplayStable(ZeroA, ZeroB) &&
		PhaseIIHashesReplayStable(EventA, EventB);

	if (!(bMixedReplay && bOceanReplay && bDivergenceReplay && bZeroReplay && bEventClosure && bPhaseIIHashes))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab Phase IIIB.7 gates failed."));
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.7 gates passed."));
	return 0;
}
