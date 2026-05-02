// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIA1Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr double ElevationToleranceKm = 1.0e-12;

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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIA1"), Stamp);
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
				TEXT("phase-iii-slice-iiia1-report.md"));
		}
		else if (FPaths::IsRelative(ReportPath))
		{
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), ReportPath);
		}
		return FPaths::ConvertRelativePathToFull(ReportPath);
	}

	FString ExtractJsonStringValue(const FString& Line, const FString& Key)
	{
		const FString Needle = FString::Printf(TEXT("\"%s\":\""), *Key);
		const int32 Start = Line.Find(Needle);
		if (Start == INDEX_NONE)
		{
			return FString();
		}

		const int32 ValueStart = Start + Needle.Len();
		const int32 ValueEnd = Line.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
		if (ValueEnd == INDEX_NONE || ValueEnd <= ValueStart)
		{
			return FString();
		}
		return Line.Mid(ValueStart, ValueEnd - ValueStart);
	}

	struct FPhaseIIIA1Baseline
	{
		bool bLoaded = false;
		FString Path;
		FString StateHashAfter;
		FString MaterialLedgerHash;
	};

	FPhaseIIIA1Baseline LoadPhaseIIBaseline()
	{
		FPhaseIIIA1Baseline Baseline;
		Baseline.Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("CarrierLab"),
			TEXT("PhaseII"),
			TEXT("Slice55"),
			TEXT("verify_60k_20260502"),
			TEXT("metrics.jsonl")));

		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *Baseline.Path))
		{
			return Baseline;
		}

		for (const FString& Line : Lines)
		{
			if (!Line.Contains(TEXT("\"fixture\":\"cadence_60k_primary\"")) ||
				!Line.Contains(TEXT("\"replay\":0")))
			{
				continue;
			}

			Baseline.StateHashAfter = ExtractJsonStringValue(Line, TEXT("state_hash_after"));
			Baseline.MaterialLedgerHash = ExtractJsonStringValue(Line, TEXT("material_ledger_hash"));
			Baseline.bLoaded = !Baseline.StateHashAfter.IsEmpty() && !Baseline.MaterialLedgerHash.IsEmpty();
			return Baseline;
		}
		return Baseline;
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

	struct FPhaseIIIA1Audit
	{
		int32 SampleCount = 0;
		int32 PlateVertexCount = 0;
		double MaxAbsSampleElevation = 0.0;
		double MaxAbsPlateVertexElevation = 0.0;
	};

	struct FPhaseIIIA1ReplayResult
	{
		int32 Replay = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		double TotalSeconds = 0.0;
		double StepProjectionSecondsTotal = 0.0;
		double StepWallSecondsTotal = 0.0;
		FString ProjectionHashBefore;
		FString ProjectionHashAfter;
		FString StateHashBefore;
		FString StateHashAfter;
		FString CrustStateHashBefore;
		FString CrustStateHashAfter;
		FString MaterialLedgerHash;
		FPhaseIIIA1Audit AuditBefore;
		FPhaseIIIA1Audit AuditAfter;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterMetrics;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerMetrics;
		bool bCompleted = false;
	};

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

	FPhaseIIIA1Audit ReadAudit(const ACarrierLabVisualizationActor& Actor)
	{
		FPhaseIIIA1Audit Audit;
		Actor.GetPhaseIIIA1ElevationAudit(
			Audit.SampleCount,
			Audit.PlateVertexCount,
			Audit.MaxAbsSampleElevation,
			Audit.MaxAbsPlateVertexElevation);
		return Audit;
	}

	bool RunReplay(const int32 Replay, const int32 SampleCount, const int32 PlateCount, const int32 StepCount, FPhaseIIIA1ReplayResult& OutResult)
	{
		OutResult = FPhaseIIIA1ReplayResult();
		OutResult.Replay = Replay;
		OutResult.SampleCount = SampleCount;
		OutResult.PlateCount = PlateCount;
		OutResult.StepCount = StepCount;

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("CarrierLab Phase IIIA.1 could not find a commandlet world."));
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
		Actor->ConfigurePhaseIIMotionFixture(ECarrierLabPhaseIIMotionFixture::Default);

		OutResult.ProjectionHashBefore = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashBefore = Actor->CurrentMetrics.StateHash;
		OutResult.CrustStateHashBefore = Actor->CurrentMetrics.CrustStateHash;
		OutResult.AuditBefore = ReadAudit(*Actor);

		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			const double StepStartSeconds = FPlatformTime::Seconds();
			Actor->StepOnce();
			OutResult.StepWallSecondsTotal += FPlatformTime::Seconds() - StepStartSeconds;
			OutResult.StepProjectionSecondsTotal += Actor->CurrentMetrics.ProjectionSeconds;
		}

		TArray<FCarrierLabPhaseIIContactRecord> Contacts;
		if (!Actor->DetectPhaseIIContacts(Contacts, OutResult.ContactMetrics))
		{
			Actor->Destroy();
			return false;
		}

		FCarrierLabPhaseIITriangleLabelConfig LabelConfig;
		TArray<FCarrierLabPhaseIITriangleLabelRecord> Labels;
		if (!Actor->BuildPhaseIITriangleLabels(Contacts, LabelConfig, Labels, OutResult.LabelMetrics))
		{
			Actor->Destroy();
			return false;
		}

		TArray<FCarrierLabPhaseIIFilterDecisionRecord> Decisions;
		TArray<FCarrierLabPhaseIIMaterialRecord> MaterialRecords;
		if (!Actor->ApplyPhaseIIResamplingFilterEvent(Labels, Decisions, OutResult.FilterMetrics, &MaterialRecords, &OutResult.LedgerMetrics))
		{
			Actor->Destroy();
			return false;
		}

		OutResult.ProjectionHashAfter = OutResult.FilterMetrics.ProjectionHashAfter;
		OutResult.StateHashAfter = OutResult.FilterMetrics.StateHashAfter;
		OutResult.CrustStateHashAfter = Actor->CurrentMetrics.CrustStateHash;
		OutResult.MaterialLedgerHash = OutResult.LedgerMetrics.MaterialLedgerHash;
		OutResult.AuditAfter = ReadAudit(*Actor);
		OutResult.TotalSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
		OutResult.bCompleted = true;

		Actor->Destroy();
		return true;
	}

	bool AuditIsZero(const FPhaseIIIA1Audit& Audit)
	{
		return Audit.MaxAbsSampleElevation <= ElevationToleranceKm &&
			Audit.MaxAbsPlateVertexElevation <= ElevationToleranceKm;
	}

	FString ReplayJson(const FPhaseIIIA1ReplayResult& Result)
	{
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		const double AvgStepProjection = Result.StepCount > 0 ? Result.StepProjectionSecondsTotal / static_cast<double>(Result.StepCount) : 0.0;
		return FString::Printf(
			TEXT("{\"fixture\":\"iiia1_60k_primary\",\"replay\":%d,\"step_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"avg_step_projection_seconds\":%.12f,\"total_replay_seconds\":%.12f,\"projection_hash_before\":%s,\"projection_hash_after\":%s,\"state_hash_before\":%s,\"state_hash_after\":%s,\"crust_state_hash_before\":%s,\"crust_state_hash_after\":%s,\"material_ledger_hash\":%s,\"contact_hash\":%s,\"label_hash\":%s,\"filter_decision_hash\":%s,\"max_abs_sample_elevation_before\":%.15f,\"max_abs_plate_vertex_elevation_before\":%.15f,\"max_abs_sample_elevation_after\":%.15f,\"max_abs_plate_vertex_elevation_after\":%.15f,\"sample_audit_count\":%d,\"plate_vertex_audit_count\":%d,\"material_record_count\":%d,\"changed_record_count\":%d,\"memory_gb\":%.12f}"),
			Result.Replay,
			Result.StepCount,
			Result.SampleCount,
			Result.PlateCount,
			AvgStepProjection,
			Result.TotalSeconds,
			*JsonString(Result.ProjectionHashBefore),
			*JsonString(Result.ProjectionHashAfter),
			*JsonString(Result.StateHashBefore),
			*JsonString(Result.StateHashAfter),
			*JsonString(Result.CrustStateHashBefore),
			*JsonString(Result.CrustStateHashAfter),
			*JsonString(Result.MaterialLedgerHash),
			*JsonString(Result.ContactMetrics.ContactLogHash),
			*JsonString(Result.LabelMetrics.TriangleLabelHash),
			*JsonString(Result.FilterMetrics.FilterDecisionHash),
			Result.AuditBefore.MaxAbsSampleElevation,
			Result.AuditBefore.MaxAbsPlateVertexElevation,
			Result.AuditAfter.MaxAbsSampleElevation,
			Result.AuditAfter.MaxAbsPlateVertexElevation,
			Result.AuditAfter.SampleCount,
			Result.AuditAfter.PlateVertexCount,
			Result.LedgerMetrics.RecordCount,
			Result.LedgerMetrics.ChangedRecordCount,
			MemoryGb);
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FPhaseIIIA1Baseline& Baseline,
		const FPhaseIIIA1ReplayResult& A,
		const FPhaseIIIA1ReplayResult& B)
	{
		const bool bProjectionReplay = A.bCompleted && B.bCompleted && A.ProjectionHashAfter == B.ProjectionHashAfter;
		const bool bStateReplay = A.bCompleted && B.bCompleted && A.StateHashAfter == B.StateHashAfter;
		const bool bMaterialReplay = A.bCompleted && B.bCompleted && A.MaterialLedgerHash == B.MaterialLedgerHash;
		const bool bCrustReplay = A.bCompleted && B.bCompleted && A.CrustStateHashAfter == B.CrustStateHashAfter;
		const bool bElevationZero = AuditIsZero(A.AuditBefore) && AuditIsZero(B.AuditBefore) && AuditIsZero(A.AuditAfter) && AuditIsZero(B.AuditAfter);
		const bool bStateBaseline = Baseline.bLoaded && A.StateHashAfter == Baseline.StateHashAfter;
		const bool bMaterialBaseline = Baseline.bLoaded && A.MaterialLedgerHash == Baseline.MaterialLedgerHash;
		const bool bAllPass = bProjectionReplay && bStateReplay && bMaterialReplay && bCrustReplay && bElevationZero && bStateBaseline && bMaterialBaseline;

		FString Report = TEXT("# Phase III Slice IIIA.1 Checkpoint: Inert Elevation Field\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This slice adds an inert `Elevation` scalar, in kilometers, to global samples and plate-local carrier vertices. It does not add uplift, collision, rifting, erosion, amplification, or any new mutation behavior. The existing Phase II `projection_hash`, `state_hash`, and material ledger are intentionally kept unchanged; `crust_state_hash` is an additive audit hash that includes `Elevation`.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		Report += FString::Printf(TEXT("| Projection replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bProjectionReplay), *A.ProjectionHashAfter, *B.ProjectionHashAfter);
		Report += FString::Printf(TEXT("| Phase II state replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bStateReplay), *A.StateHashAfter, *B.StateHashAfter);
		Report += FString::Printf(TEXT("| Phase II material ledger replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bMaterialReplay), *A.MaterialLedgerHash, *B.MaterialLedgerHash);
		Report += FString::Printf(TEXT("| New crust state replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bCrustReplay), *A.CrustStateHashAfter, *B.CrustStateHashAfter);
		Report += FString::Printf(TEXT("| Elevation remains zero before/after resampling | %s | max sample %.3e km, max plate vertex %.3e km |\n"), *PassFail(bElevationZero), A.AuditAfter.MaxAbsSampleElevation, A.AuditAfter.MaxAbsPlateVertexElevation);
		Report += FString::Printf(TEXT("| Phase II baseline state hash unchanged | %s | baseline `%s`, IIIA.1 `%s` |\n"), *PassFail(bStateBaseline), Baseline.bLoaded ? *Baseline.StateHashAfter : TEXT("baseline unavailable"), *A.StateHashAfter);
		Report += FString::Printf(TEXT("| Phase II baseline material ledger unchanged | %s | baseline `%s`, IIIA.1 `%s` |\n"), *PassFail(bMaterialBaseline), Baseline.bLoaded ? *Baseline.MaterialLedgerHash : TEXT("baseline unavailable"), *A.MaterialLedgerHash);
		Report += TEXT("| Phase II baseline projection hash | no baseline available | Slice 5.5 artifact did not record `projection_hash_after`; replay match above covers determinism for this slice. |\n\n");

		Report += TEXT("## Hashes\n\n");
		Report += TEXT("| Replay | Projection before | Projection after | State before | State after | Crust before | Crust after | Material ledger |\n");
		Report += TEXT("|---:|---|---|---|---|---|---|---|\n");
		for (const FPhaseIIIA1ReplayResult* Result : {&A, &B})
		{
			Report += FString::Printf(
				TEXT("| %d | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` |\n"),
				Result->Replay,
				*Result->ProjectionHashBefore,
				*Result->ProjectionHashAfter,
				*Result->StateHashBefore,
				*Result->StateHashAfter,
				*Result->CrustStateHashBefore,
				*Result->CrustStateHashAfter,
				*Result->MaterialLedgerHash);
		}

		Report += TEXT("\n## Elevation Audit\n\n");
		Report += TEXT("| Replay | Samples | Plate-local vertices | Max sample before km | Max plate vertex before km | Max sample after km | Max plate vertex after km |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FPhaseIIIA1ReplayResult* Result : {&A, &B})
		{
			Report += FString::Printf(
				TEXT("| %d | %d | %d | %.3e | %.3e | %.3e | %.3e |\n"),
				Result->Replay,
				Result->AuditAfter.SampleCount,
				Result->AuditAfter.PlateVertexCount,
				Result->AuditBefore.MaxAbsSampleElevation,
				Result->AuditBefore.MaxAbsPlateVertexElevation,
				Result->AuditAfter.MaxAbsSampleElevation,
				Result->AuditAfter.MaxAbsPlateVertexElevation);
		}

		Report += TEXT("\n## Phase II Baseline\n\n");
		Report += FString::Printf(TEXT("Baseline artifact: `%s`\n\n"), *Baseline.Path);
		Report += TEXT("| Metric | Baseline | IIIA.1 replay 0 | Result |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += FString::Printf(TEXT("| State hash after resampling | `%s` | `%s` | %s |\n"), Baseline.bLoaded ? *Baseline.StateHashAfter : TEXT("unavailable"), *A.StateHashAfter, *PassFail(bStateBaseline));
		Report += FString::Printf(TEXT("| Material ledger hash | `%s` | `%s` | %s |\n"), Baseline.bLoaded ? *Baseline.MaterialLedgerHash : TEXT("unavailable"), *A.MaterialLedgerHash, *PassFail(bMaterialBaseline));

		Report += TEXT("\n## Notes\n\n");
		Report += TEXT("- `Elevation` is stored on `FSphereSample` and copied into `FCarrierVertex` during plate-local triangulation rebuilds.\n");
		Report += TEXT("- No Phase IIIA.2+ fields or consumers are present in this slice.\n");
		Report += TEXT("- `state_hash` deliberately excludes `Elevation` so prior Phase II checkpoints stay comparable. `crust_state_hash` is the first Phase III field-aware hash.\n");
		Report += TEXT("- The resampling event used here is the accepted Phase II filtered resampling path after 32 rigid-motion steps at 60k/40/seed 42.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIA.1 passes. Pause for user review before IIIA.2 (inert oceanic age).\n")
			: TEXT("Pause before IIIA.2. One or more inert-field or baseline-preservation gates require investigation.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIA1Commandlet::UCarrierLabPhaseIIIA1Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIA1Commandlet::Main(const FString& Params)
{
	const int32 SampleCount = FMath::Max(12, ParseIntParam(Params, TEXT("Samples="), 60000));
	const int32 PlateCount = FMath::Clamp(ParseIntParam(Params, TEXT("Plates="), 40), 1, 63);
	const int32 StepCount = FMath::Max(1, ParseIntParam(Params, TEXT("Steps="), 32));
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FPhaseIIIA1ReplayResult A;
	FPhaseIIIA1ReplayResult B;
	const bool bRunA = RunReplay(0, SampleCount, PlateCount, StepCount, A);
	const bool bRunB = RunReplay(1, SampleCount, PlateCount, StepCount, B);
	const FPhaseIIIA1Baseline Baseline = LoadPhaseIIBaseline();

	FString MetricsJsonl;
	MetricsJsonl += ReplayJson(A) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(B) + LINE_TERMINATOR;
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);

	const FString Report = BuildReport(OutputRoot, Baseline, A, B);
	const FString ReportPath = ResolveReportPath(Params);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIA.1 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIA.1 report: %s"), *ReportPath);

	const bool bProjectionReplay = A.bCompleted && B.bCompleted && A.ProjectionHashAfter == B.ProjectionHashAfter;
	const bool bStateReplay = A.bCompleted && B.bCompleted && A.StateHashAfter == B.StateHashAfter;
	const bool bMaterialReplay = A.bCompleted && B.bCompleted && A.MaterialLedgerHash == B.MaterialLedgerHash;
	const bool bCrustReplay = A.bCompleted && B.bCompleted && A.CrustStateHashAfter == B.CrustStateHashAfter;
	const bool bElevationZero = AuditIsZero(A.AuditBefore) && AuditIsZero(B.AuditBefore) && AuditIsZero(A.AuditAfter) && AuditIsZero(B.AuditAfter);
	const bool bStateBaseline = Baseline.bLoaded && A.StateHashAfter == Baseline.StateHashAfter;
	const bool bMaterialBaseline = Baseline.bLoaded && A.MaterialLedgerHash == Baseline.MaterialLedgerHash;
	return (bRunA && bRunB && bProjectionReplay && bStateReplay && bMaterialReplay && bCrustReplay && bElevationZero && bStateBaseline && bMaterialBaseline) ? 0 : 2;
}
