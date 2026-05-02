// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIB6Commandlet.h"

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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIB6"), Stamp);
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
				TEXT("phase-iii-slice-iiib6-report.md"));
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

	FString AuditJsonFields(const FCarrierLabPhaseIIIB6NeighborPropagationAudit& Audit, const TCHAR* Prefix)
	{
		return FString::Printf(
			TEXT("\"%s_step\":%d,\"%s_active\":%d,\"%s_total_local_triangles\":%d,\"%s_records\":%d,\"%s_non_boundary_active\":%d,\"%s_over_threshold\":%d,\"%s_seed_hits\":%d,\"%s_added\":%d,\"%s_duplicates\":%d,\"%s_rejected_distance\":%d,\"%s_invalid\":%d,\"%s_active_plates\":%d,\"%s_max_active_on_plate\":%d,\"%s_max_distance_km\":%.12f,\"%s_hash\":%s"),
			Prefix, Audit.Step,
			Prefix, Audit.ActiveTriangleCount,
			Prefix, Audit.TotalPlateLocalTriangleCount,
			Prefix, Audit.DistanceRecordCount,
			Prefix, Audit.NonBoundaryActiveTriangleCount,
			Prefix, Audit.OverThresholdActiveTriangleCount,
			Prefix, Audit.PropagationSeedHitCount,
			Prefix, Audit.PropagationAddedCount,
			Prefix, Audit.PropagationDuplicateCount,
			Prefix, Audit.PropagationDistanceRejectedCount,
			Prefix, Audit.PropagationInvalidCount,
			Prefix, Audit.ActivePlateCount,
			Prefix, Audit.MaxActiveTrianglesOnPlate,
			Prefix, Audit.MaxDistanceKm,
			Prefix, *JsonString(Audit.ConvergenceTrackingHash));
	}

	struct FNeighborReplayResult
	{
		FString Fixture;
		int32 Replay = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		double TotalSeconds = 0.0;
		bool bSeeded = false;
		bool bCompleted = false;
		FCarrierLabPhaseIIIB6SeedMetrics SeedMetrics;
		FCarrierLabPhaseIIIB6NeighborPropagationAudit InitialAudit;
		FCarrierLabPhaseIIIB6NeighborPropagationAudit FinalAudit;
		FString ProjectionHashAfter;
		FString StateHashAfter;
		FString CrustStateHashAfter;
	};

	bool RunNeighborReplay(
		const FString& Fixture,
		const ECarrierLabPhaseIIMotionFixture MotionFixture,
		const EMaterialFixture MaterialFixture,
		const FAgeFixture AgeFixture,
		const bool bSeedSingleTriangle,
		const int32 PreferredUnderPlateId,
		const int32 Replay,
		const int32 SampleCount,
		const int32 PlateCount,
		const int32 StepCount,
		FNeighborReplayResult& OutResult)
	{
		OutResult = FNeighborReplayResult();
		OutResult.Fixture = Fixture;
		OutResult.Replay = Replay;
		OutResult.SampleCount = SampleCount;
		OutResult.PlateCount = PlateCount;
		OutResult.StepCount = StepCount;

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
		if (bSeedSingleTriangle)
		{
			OutResult.bSeeded = Actor->SeedPhaseIIIB6SingleConvergentTriangleForTest(PreferredUnderPlateId, OutResult.SeedMetrics);
			if (!OutResult.bSeeded)
			{
				Actor->Destroy();
				return false;
			}
		}
		if (!Actor->GetPhaseIIIB6NeighborPropagationAudit(OutResult.InitialAudit))
		{
			Actor->Destroy();
			return false;
		}

		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			Actor->StepOnce();
		}
		if (!Actor->GetPhaseIIIB6NeighborPropagationAudit(OutResult.FinalAudit))
		{
			Actor->Destroy();
			return false;
		}

		OutResult.ProjectionHashAfter = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashAfter = Actor->CurrentMetrics.StateHash;
		OutResult.CrustStateHashAfter = Actor->CurrentMetrics.CrustStateHash;
		OutResult.TotalSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
		OutResult.bCompleted = true;
		Actor->Destroy();
		return true;
	}

	bool ReplayHashesMatch(const FNeighborReplayResult& A, const FNeighborReplayResult& B)
	{
		return A.bCompleted && B.bCompleted &&
			A.InitialAudit.ConvergenceTrackingHash == B.InitialAudit.ConvergenceTrackingHash &&
			A.FinalAudit.ConvergenceTrackingHash == B.FinalAudit.ConvergenceTrackingHash &&
			A.ProjectionHashAfter == B.ProjectionHashAfter &&
			A.StateHashAfter == B.StateHashAfter &&
			A.CrustStateHashAfter == B.CrustStateHashAfter;
	}

	FString ReplayJson(const FNeighborReplayResult& Result)
	{
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"completed\":%s,\"seeded\":%s,\"samples\":%d,\"plates\":%d,\"steps\":%d,\"seconds\":%.6f,\"seed_plate\":%d,\"seed_other_plate\":%d,\"seed_triangle\":%d,\"seed_neighbor_candidates\":%d,\"seed_step_distance_km\":%.12f,\"seed_max_neighbor_distance_km\":%.12f,%s,%s,\"projection_hash_after\":%s,\"state_hash_after\":%s,\"crust_state_hash_after\":%s,\"memory_gb\":%.12f}"),
			*JsonString(Result.Fixture),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.bSeeded ? TEXT("true") : TEXT("false"),
			Result.SampleCount,
			Result.PlateCount,
			Result.StepCount,
			Result.TotalSeconds,
			Result.SeedMetrics.SeedPlateId,
			Result.SeedMetrics.SeedOtherPlateId,
			Result.SeedMetrics.SeedLocalTriangleId,
			Result.SeedMetrics.SeedNeighborCandidateCount,
			Result.SeedMetrics.SeedStepDistanceKm,
			Result.SeedMetrics.MaxSeedNeighborDistanceKm,
			*AuditJsonFields(Result.InitialAudit, TEXT("initial")),
			*AuditJsonFields(Result.FinalAudit, TEXT("final")),
			*JsonString(Result.ProjectionHashAfter),
			*JsonString(Result.StateHashAfter),
			*JsonString(Result.CrustStateHashAfter),
			MemoryGb);
	}

	bool IsSingleSeedGrowthExpected(const FNeighborReplayResult& Result)
	{
		return Result.bCompleted &&
			Result.bSeeded &&
			Result.SeedMetrics.SeedNeighborCandidateCount > 0 &&
			Result.FinalAudit.PropagationSeedHitCount == 1 &&
			Result.FinalAudit.PropagationAddedCount == Result.SeedMetrics.SeedNeighborCandidateCount &&
			Result.FinalAudit.ActiveTriangleCount == 1 + Result.SeedMetrics.SeedNeighborCandidateCount &&
			Result.FinalAudit.OverThresholdActiveTriangleCount == 0 &&
			Result.FinalAudit.PropagationInvalidCount == 0;
	}

	bool IsBounded(const FNeighborReplayResult& Result)
	{
		return Result.bCompleted &&
			Result.FinalAudit.ActiveTriangleCount <= Result.FinalAudit.TotalPlateLocalTriangleCount &&
			Result.FinalAudit.MaxActiveTrianglesOnPlate <= Result.FinalAudit.TotalPlateLocalTriangleCount &&
			Result.FinalAudit.OverThresholdActiveTriangleCount == 0 &&
			Result.FinalAudit.MaxDistanceKm <= Result.FinalAudit.DistanceThresholdKm + 1.0e-6;
	}

	bool IsDeferredNoGrowth(const FNeighborReplayResult& Result)
	{
		return Result.bCompleted &&
			Result.bSeeded &&
			Result.InitialAudit.ActiveTriangleCount == 1 &&
			Result.FinalAudit.ActiveTriangleCount == 1 &&
			Result.FinalAudit.PropagationSeedHitCount == 0 &&
			Result.FinalAudit.PropagationAddedCount == 0 &&
			Result.FinalAudit.PropagationInvalidCount == 0;
	}

	bool IsNoPropagationControl(const FNeighborReplayResult& Result)
	{
		return Result.bCompleted &&
			Result.FinalAudit.PropagationSeedHitCount == 0 &&
			Result.FinalAudit.PropagationAddedCount == 0 &&
			Result.FinalAudit.PropagationInvalidCount == 0 &&
			Result.FinalAudit.OverThresholdActiveTriangleCount == 0;
	}

	bool IsLocalPropagationObserved(const FNeighborReplayResult& Result)
	{
		return Result.bCompleted &&
			Result.FinalAudit.NonBoundaryActiveTriangleCount > 0 &&
			Result.FinalAudit.PropagationSeedHitCount > 0 &&
			Result.FinalAudit.PropagationAddedCount > 0 &&
			Result.FinalAudit.PropagationInvalidCount == 0 &&
			Result.FinalAudit.OverThresholdActiveTriangleCount == 0 &&
			Result.FinalAudit.MaxDistanceKm <= Result.FinalAudit.DistanceThresholdKm + 1.0e-6;
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FNeighborReplayResult& SingleA,
		const FNeighborReplayResult& SingleB,
		const FNeighborReplayResult& BoundedA,
		const FNeighborReplayResult& BoundedB,
		const FNeighborReplayResult& DeferredA,
		const FNeighborReplayResult& DeferredB,
		const FNeighborReplayResult& DivergenceA,
		const FNeighborReplayResult& DivergenceB,
		const FNeighborReplayResult& ZeroA,
		const FNeighborReplayResult& ZeroB)
	{
		const bool bSingleGrowth = IsSingleSeedGrowthExpected(SingleA) && ReplayHashesMatch(SingleA, SingleB);
		const bool bBounded = IsBounded(BoundedA) && IsBounded(BoundedB) && ReplayHashesMatch(BoundedA, BoundedB);
		const bool bDeferred = IsDeferredNoGrowth(DeferredA) && ReplayHashesMatch(DeferredA, DeferredB);
		const bool bDivergence = IsLocalPropagationObserved(DivergenceA) && ReplayHashesMatch(DivergenceA, DivergenceB);
		const bool bZero = IsNoPropagationControl(ZeroA) && ReplayHashesMatch(ZeroA, ZeroB) &&
			ZeroA.InitialAudit.ConvergenceTrackingHash == ZeroA.FinalAudit.ConvergenceTrackingHash;
		const bool bPhaseIIHashes = SingleA.ProjectionHashAfter == SingleB.ProjectionHashAfter &&
			SingleA.StateHashAfter == SingleB.StateHashAfter &&
			SingleA.CrustStateHashAfter == SingleB.CrustStateHashAfter;
		const bool bAllPass = bSingleGrowth && bBounded && bDeferred && bDivergence && bZero && bPhaseIIHashes;

		FString Report = TEXT("# Phase III Slice IIIB.6 Checkpoint: Neighbor Propagation\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This slice expands read-only convergence tracking from a subducting active triangle to its plate-local edge-neighbor triangles. Propagation only fires for matrix evidence with a subduction polarity decision (`OceanicUnderContinental` or `OlderOceanicUnderYoungerOceanic`), and each new neighbor receives a distance-to-front value bounded by `r_s = 1800 km`. It does not mark triangles, filter projection candidates, resample, or mutate crust material.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n|---|---|---|\n");
		Report += FString::Printf(TEXT("| Single-triangle seed grows by expected neighbors | %s | seed %d, candidates %d, active %d -> %d, added %d |\n"),
			*PassFail(bSingleGrowth),
			SingleA.SeedMetrics.SeedLocalTriangleId,
			SingleA.SeedMetrics.SeedNeighborCandidateCount,
			SingleA.InitialAudit.ActiveTriangleCount,
			SingleA.FinalAudit.ActiveTriangleCount,
			SingleA.FinalAudit.PropagationAddedCount);
		Report += FString::Printf(TEXT("| Active list remains bounded | %s | active %d / local %d, max distance %.6f km, rejected %d |\n"),
			*PassFail(bBounded),
			BoundedA.FinalAudit.ActiveTriangleCount,
			BoundedA.FinalAudit.TotalPlateLocalTriangleCount,
			BoundedA.FinalAudit.MaxDistanceKm,
			BoundedA.FinalAudit.PropagationDistanceRejectedCount);
		Report += FString::Printf(TEXT("| Equal-age ocean-ocean defers and does not propagate | %s | active %d -> %d, added %d |\n"),
			*PassFail(bDeferred),
			DeferredA.InitialAudit.ActiveTriangleCount,
			DeferredA.FinalAudit.ActiveTriangleCount,
			DeferredA.FinalAudit.PropagationAddedCount);
		Report += FString::Printf(TEXT("| Forced divergence admits local convergent propagation | %s | non-boundary %d, seed hits %d, added %d |\n"),
			*PassFail(bDivergence),
			DivergenceA.FinalAudit.NonBoundaryActiveTriangleCount,
			DivergenceA.FinalAudit.PropagationSeedHitCount,
			DivergenceA.FinalAudit.PropagationAddedCount);
		Report += FString::Printf(TEXT("| Zero-motion no-op remains hash-stable | %s | `%s` -> `%s` |\n"),
			*PassFail(bZero),
			*ZeroA.InitialAudit.ConvergenceTrackingHash,
			*ZeroA.FinalAudit.ConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| Phase II hashes replay unchanged | %s | projection `%s`, state `%s`, crust `%s` |\n"),
			*PassFail(bPhaseIIHashes),
			*SingleA.ProjectionHashAfter,
			*SingleA.StateHashAfter,
			*SingleA.CrustStateHashAfter);

		Report += TEXT("\n## Neighbor Propagation Audits\n\n");
		Report += TEXT("| Fixture | Replay | Step | Active | Records | Non-boundary | Seed hits | Added | Dup | Rejected | Invalid | Max km | Hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		auto AddRow = [&Report](const FNeighborReplayResult& Result)
		{
			const FCarrierLabPhaseIIIB6NeighborPropagationAudit& Audit = Result.FinalAudit;
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %.6f | `%s` |\n"),
				*Result.Fixture,
				Result.Replay,
				Audit.Step,
				Audit.ActiveTriangleCount,
				Audit.DistanceRecordCount,
				Audit.NonBoundaryActiveTriangleCount,
				Audit.PropagationSeedHitCount,
				Audit.PropagationAddedCount,
				Audit.PropagationDuplicateCount,
				Audit.PropagationDistanceRejectedCount,
				Audit.PropagationInvalidCount,
				Audit.MaxDistanceKm,
				*Audit.ConvergenceTrackingHash);
		};
		AddRow(SingleA);
		AddRow(SingleB);
		AddRow(BoundedA);
		AddRow(BoundedB);
		AddRow(DeferredA);
		AddRow(DeferredB);
		AddRow(DivergenceA);
		AddRow(DivergenceB);
		AddRow(ZeroA);
		AddRow(ZeroB);

		Report += TEXT("\n## Notes\n\n");
		Report += TEXT("- The active list is still plate-local tracking state. It is not global sample ownership and is not material authority.\n");
		Report += TEXT("- Collision candidates and equal-age ocean-ocean deferrals do not propagate because they do not have a subducting under-plate yet.\n");
		Report += TEXT("- The forced-divergence fixture is a closed-sphere local-evidence control: backside local convergence can seed propagation, so the gate checks local polarity evidence instead of global pair divergence.\n");
		Report += TEXT("- Newly added neighbors are bounded by parent distance plus plate-local triangle-barycenter distance; over-budget candidates are logged and rejected.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIB.6 passes. Pause for user review before IIIB.7 (replay/event hash closure).\n")
			: TEXT("IIIB.6 does not pass. Investigate before IIIB.7.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIB6Commandlet::UCarrierLabPhaseIIIB6Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIB6Commandlet::Main(const FString& Params)
{
	const int32 FixtureSamples = FMath::Max(12, ParseIntParam(Params, TEXT("FixtureSamples="), 10000));
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	const FAgeFixture NoAges{ false, 0.0, 0.0 };
	const FAgeFixture EqualAges{ true, 64.0, 64.0 };

	FNeighborReplayResult SingleA;
	FNeighborReplayResult SingleB;
	const bool bSingleA = RunNeighborReplay(TEXT("single_seed_growth"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::MixedPlate0Continental, NoAges, true, 1, 0, FixtureSamples, 2, 0, SingleA);
	const bool bSingleB = RunNeighborReplay(TEXT("single_seed_growth"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::MixedPlate0Continental, NoAges, true, 1, 1, FixtureSamples, 2, 0, SingleB);

	FNeighborReplayResult BoundedA;
	FNeighborReplayResult BoundedB;
	const bool bBoundedA = RunNeighborReplay(TEXT("bounded_growth"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::MixedPlate0Continental, NoAges, false, INDEX_NONE, 0, FixtureSamples, 2, 8, BoundedA);
	const bool bBoundedB = RunNeighborReplay(TEXT("bounded_growth"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::MixedPlate0Continental, NoAges, false, INDEX_NONE, 1, FixtureSamples, 2, 8, BoundedB);

	FNeighborReplayResult DeferredA;
	FNeighborReplayResult DeferredB;
	const bool bDeferredA = RunNeighborReplay(TEXT("equal_age_ocean_ocean_deferred"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllOceanic, EqualAges, true, 0, 0, FixtureSamples, 2, 0, DeferredA);
	const bool bDeferredB = RunNeighborReplay(TEXT("equal_age_ocean_ocean_deferred"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllOceanic, EqualAges, true, 0, 1, FixtureSamples, 2, 0, DeferredB);

	FNeighborReplayResult DivergenceA;
	FNeighborReplayResult DivergenceB;
	const bool bDivergenceA = RunNeighborReplay(TEXT("forced_divergence_local_convergence"), ECarrierLabPhaseIIMotionFixture::ForcedDivergence, EMaterialFixture::MixedPlate0Continental, NoAges, false, INDEX_NONE, 0, FixtureSamples, 2, 1, DivergenceA);
	const bool bDivergenceB = RunNeighborReplay(TEXT("forced_divergence_local_convergence"), ECarrierLabPhaseIIMotionFixture::ForcedDivergence, EMaterialFixture::MixedPlate0Continental, NoAges, false, INDEX_NONE, 1, FixtureSamples, 2, 1, DivergenceB);

	FNeighborReplayResult ZeroA;
	FNeighborReplayResult ZeroB;
	const bool bZeroA = RunNeighborReplay(TEXT("zero_motion_noop"), ECarrierLabPhaseIIMotionFixture::Zero, EMaterialFixture::Default, NoAges, false, INDEX_NONE, 0, FixtureSamples, 40, 10, ZeroA);
	const bool bZeroB = RunNeighborReplay(TEXT("zero_motion_noop"), ECarrierLabPhaseIIMotionFixture::Zero, EMaterialFixture::Default, NoAges, false, INDEX_NONE, 1, FixtureSamples, 40, 10, ZeroB);

	FString MetricsJsonl;
	MetricsJsonl += ReplayJson(SingleA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(SingleB) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(BoundedA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(BoundedB) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(DeferredA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(DeferredB) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(DivergenceA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(DivergenceB) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(ZeroA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(ZeroB) + LINE_TERMINATOR;
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);

	const FString Report = BuildReport(OutputRoot, SingleA, SingleB, BoundedA, BoundedB, DeferredA, DeferredB, DivergenceA, DivergenceB, ZeroA, ZeroB);
	const FString ReportPath = ResolveReportPath(Params);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.6 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.6 report: %s"), *ReportPath);

	const bool bSingleGrowth = bSingleA && bSingleB && IsSingleSeedGrowthExpected(SingleA) && ReplayHashesMatch(SingleA, SingleB);
	const bool bBounded = bBoundedA && bBoundedB && IsBounded(BoundedA) && IsBounded(BoundedB) && ReplayHashesMatch(BoundedA, BoundedB);
	const bool bDeferred = bDeferredA && bDeferredB && IsDeferredNoGrowth(DeferredA) && ReplayHashesMatch(DeferredA, DeferredB);
	const bool bDivergence = bDivergenceA && bDivergenceB && IsLocalPropagationObserved(DivergenceA) && ReplayHashesMatch(DivergenceA, DivergenceB);
	const bool bZero = bZeroA && bZeroB && IsNoPropagationControl(ZeroA) && ReplayHashesMatch(ZeroA, ZeroB) &&
		ZeroA.InitialAudit.ConvergenceTrackingHash == ZeroA.FinalAudit.ConvergenceTrackingHash;
	const bool bPhaseIIHashes = SingleA.ProjectionHashAfter == SingleB.ProjectionHashAfter &&
		SingleA.StateHashAfter == SingleB.StateHashAfter &&
		SingleA.CrustStateHashAfter == SingleB.CrustStateHashAfter;

	if (!(bSingleGrowth && bBounded && bDeferred && bDivergence && bZero && bPhaseIIHashes))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab Phase IIIB.6 gates failed."));
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.6 gates passed."));
	return 0;
}
