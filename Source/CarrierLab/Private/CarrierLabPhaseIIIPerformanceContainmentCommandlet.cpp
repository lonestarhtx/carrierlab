// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIPerformanceContainmentCommandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr int32 Seed = 42;
	constexpr int32 SliceFixtureSamples = 10000;
	constexpr int32 TinyFixtureSamples = 2000;
	constexpr int32 FixturePlates = 2;
	constexpr int32 MaxThresholdSteps = 12;
	constexpr double DefaultVelocityMmPerYear = 66.6666666667;
	constexpr double CollisionThresholdKm = 300.0;
	constexpr double DestinationMassThresholdRatio = 0.5;
	constexpr double PaperTable2TotalTectonicsSecondsPerStep60k40 = 0.19;
	constexpr double PaperTable2OceanicCrustSecondsPerEvent = 0.58;
	constexpr double PaperTable2PlateRiftingSecondsPerEvent = 0.23;
	constexpr double SoftPaperRatioTarget = 10.0;
	constexpr double IIID8Replay0WallSeconds = 1151.130;
	constexpr int32 IIID8IntegratedStepCount = 32;

	enum class EValidationTier : uint8
	{
		Tiny,
		Slice,
		Integrated,
		Benchmark
	};

	FString TierName(const EValidationTier Tier)
	{
		switch (Tier)
		{
		case EValidationTier::Tiny:
			return TEXT("Tiny");
		case EValidationTier::Integrated:
			return TEXT("Integrated");
		case EValidationTier::Benchmark:
			return TEXT("Benchmark");
		case EValidationTier::Slice:
		default:
			return TEXT("Slice");
		}
	}

	EValidationTier ParseValidationTier(const FString& Params)
	{
		FString TierValue;
		if (!FParse::Value(*Params, TEXT("ValidationTier="), TierValue))
		{
			return EValidationTier::Slice;
		}
		if (TierValue.Equals(TEXT("Tiny"), ESearchCase::IgnoreCase))
		{
			return EValidationTier::Tiny;
		}
		if (TierValue.Equals(TEXT("Integrated"), ESearchCase::IgnoreCase))
		{
			return EValidationTier::Integrated;
		}
		if (TierValue.Equals(TEXT("Benchmark"), ESearchCase::IgnoreCase))
		{
			return EValidationTier::Benchmark;
		}
		return EValidationTier::Slice;
	}

	double TierTargetSeconds(const EValidationTier Tier)
	{
		switch (Tier)
		{
		case EValidationTier::Tiny:
			return 15.0;
		case EValidationTier::Slice:
			return 120.0;
		case EValidationTier::Integrated:
		case EValidationTier::Benchmark:
		default:
			return 0.0;
		}
	}

	int32 TierSampleCount(const EValidationTier Tier)
	{
		return Tier == EValidationTier::Tiny ? TinyFixtureSamples : SliceFixtureSamples;
	}

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

	FString RatioString(const double Ratio)
	{
		return FMath::IsFinite(Ratio) ? FString::Printf(TEXT("%.2fx"), Ratio) : TEXT("n/a");
	}

	FString TargetStatus(const double Ratio)
	{
		if (!FMath::IsFinite(Ratio))
		{
			return TEXT("n/a");
		}
		return Ratio <= SoftPaperRatioTarget ? TEXT("within_target") : TEXT("over_target");
	}

	double SafeRatio(const double Numerator, const double Denominator)
	{
		return FMath::Abs(Denominator) > UE_SMALL_NUMBER ? Numerator / Denominator : 0.0;
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("PreIIIEPerformanceContainment"), Stamp);
		}
		else if (FPaths::IsRelative(OutputRoot))
		{
			OutputRoot = FPaths::Combine(FPaths::ProjectDir(), OutputRoot);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	FString ResolveContainmentReportPath(const FString& Params)
	{
		FString ReportPath;
		if (!FParse::Value(*Params, TEXT("Report="), ReportPath))
		{
			ReportPath = FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("docs"),
				TEXT("checkpoints"),
				TEXT("phase-iii-pre-iiie-performance-containment.md"));
		}
		else if (FPaths::IsRelative(ReportPath))
		{
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), ReportPath);
		}
		return FPaths::ConvertRelativePathToFull(ReportPath);
	}

	FString ResolveCostDriverReportPath()
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectDir(),
			TEXT("docs"),
			TEXT("checkpoints"),
			TEXT("phase-iii-pre-iiie-cost-driver-identification.md")));
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

	ACarrierLabVisualizationActor* SpawnActor(UWorld& World, const int32 SampleCount)
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
		Actor->PlateCount = FixturePlates;
		Actor->Seed = Seed;
		Actor->ContinentalPlateFraction = 0.50;
		Actor->VelocityMmPerYear = DefaultVelocityMmPerYear;
		Actor->MultiHitPolicy = ECarrierLabMultiHitPolicy::Centroid;
		Actor->VisualizationLayer = ECarrierLabVisualizationLayer::PlateId;
		Actor->ConfigurePhaseIIICProcessLayer(false, false);
		Actor->bEnableNaturalResamplingEvents = false;
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	struct FCollisionTimingRow
	{
		FString Name;
		FString Scope;
		double Seconds = 0.0;
		double SecondsPerStep = 0.0;
		double PaperSecondsPerStep = PaperTable2TotalTectonicsSecondsPerStep60k40;
		double PaperRatio = 0.0;
		int32 ProbeCount = 0;
		int32 CandidateCount = 0;
		int32 MutationCount = 0;
	};

	struct FCollisionTimingResult
	{
		EValidationTier Tier = EValidationTier::Slice;
		bool bCompleted = false;
		bool bFixtureReady = false;
		int32 SampleCount = 0;
		int32 PlateCount = FixturePlates;
		int32 StepCount = 0;
		int32 PatchSeedTriangleId = INDEX_NONE;
		int32 PatchTriangleCount = 0;
		double FixtureSetupSeconds = 0.0;
		double TotalMeasuredCollisionSeconds = 0.0;
		int32 ProbeCount = 0;
		int32 CandidateCount = 0;
		int32 MutationCount = 0;
		int32 PolicyResolvedMultiHitCount = 0;
		TArray<FCollisionTimingRow> Rows;
	};

	void FinalizeTimingRow(FCollisionTimingRow& Row, const int32 StepCount)
	{
		Row.SecondsPerStep = StepCount > 0 ? Row.Seconds / static_cast<double>(StepCount) : Row.Seconds;
		Row.PaperRatio = SafeRatio(Row.SecondsPerStep, Row.PaperSecondsPerStep);
	}

	template <typename FuncType>
	bool TimeCall(FCollisionTimingRow& Row, const int32 StepCount, FuncType&& Func)
	{
		const double StartSeconds = FPlatformTime::Seconds();
		const bool bOk = Func();
		Row.Seconds = FPlatformTime::Seconds() - StartSeconds;
		FinalizeTimingRow(Row, StepCount);
		return bOk;
	}

	bool PrepareCollisionActor(
		UWorld& World,
		const int32 SampleCount,
		ACarrierLabVisualizationActor*& OutActor,
		int32& OutStepCount,
		int32& OutPatchSeedTriangleId,
		int32& OutPatchTriangleCount,
		double& OutSetupSeconds)
	{
		OutActor = nullptr;
		OutStepCount = 0;
		OutPatchSeedTriangleId = INDEX_NONE;
		OutPatchTriangleCount = 0;

		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(World, SampleCount);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier())
		{
			Actor->Destroy();
			return false;
		}
		Actor->ConfigurePhaseIIMotionFixture(ECarrierLabPhaseIIMotionFixture::ForcedConvergence);
		if (!Actor->SetPlateContinentalForTest(0, true) ||
			!Actor->SetPlateContinentalForTest(1, true))
		{
			Actor->Destroy();
			return false;
		}

		for (int32 Step = 0; Step <= MaxThresholdSteps; ++Step)
		{
			if (Step > 0)
			{
				Actor->StepOnce();
			}

			FCarrierLabPhaseIIID2CollisionGroupingAudit ProbeGrouping;
			if (!Actor->DetectPhaseIIID2CollisionGroups(ProbeGrouping, CollisionThresholdKm))
			{
				Actor->Destroy();
				return false;
			}
			if (ProbeGrouping.AcceptedGroupCount <= 0)
			{
				continue;
			}
			if (!Actor->SetPhaseIIID3DestinationPatchForTest(
				0,
				1,
				0,
				OutPatchSeedTriangleId,
				OutPatchTriangleCount))
			{
				Actor->Destroy();
				return false;
			}
			OutActor = Actor;
			OutStepCount = Step;
			OutSetupSeconds = FPlatformTime::Seconds() - StartSeconds;
			return true;
		}

		OutSetupSeconds = FPlatformTime::Seconds() - StartSeconds;
		Actor->Destroy();
		return false;
	}

	bool RunCollisionTimingFixture(const EValidationTier Tier, FCollisionTimingResult& OutResult)
	{
		OutResult = FCollisionTimingResult();
		OutResult.Tier = Tier;
		OutResult.SampleCount = TierSampleCount(Tier);

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		ACarrierLabVisualizationActor* Actor = nullptr;
		if (!PrepareCollisionActor(
			*World,
			OutResult.SampleCount,
			Actor,
			OutResult.StepCount,
			OutResult.PatchSeedTriangleId,
			OutResult.PatchTriangleCount,
			OutResult.FixtureSetupSeconds))
		{
			return false;
		}
		OutResult.bFixtureReady = true;

		FCarrierLabPhaseIIID1TerraneAudit DetectionAudit;
		FCollisionTimingRow DetectionRow;
		DetectionRow.Name = TEXT("collision_detection_iiid1");
		DetectionRow.Scope = TEXT("DetectPhaseIIID1ConnectedTerranes inclusive API timing");
		if (!TimeCall(DetectionRow, OutResult.StepCount, [&]() { return Actor->DetectPhaseIIID1ConnectedTerranes(DetectionAudit); }))
		{
			Actor->Destroy();
			return false;
		}
		DetectionRow.ProbeCount = DetectionAudit.CollisionCandidateHitCount;
		DetectionRow.CandidateCount = DetectionAudit.TerraneRecordCount;
		OutResult.Rows.Add(DetectionRow);

		FCarrierLabPhaseIIID2CollisionGroupingAudit GroupingAudit;
		FCollisionTimingRow GroupingRow;
		GroupingRow.Name = TEXT("collision_grouping_iiid2");
		GroupingRow.Scope = TEXT("DetectPhaseIIID2CollisionGroups inclusive API timing");
		if (!TimeCall(GroupingRow, OutResult.StepCount, [&]() { return Actor->DetectPhaseIIID2CollisionGroups(GroupingAudit, CollisionThresholdKm); }))
		{
			Actor->Destroy();
			return false;
		}
		GroupingRow.ProbeCount = GroupingAudit.TerraneRecordCount;
		GroupingRow.CandidateCount = GroupingAudit.GroupCount;
		OutResult.Rows.Add(GroupingRow);

		FCarrierLabPhaseIIID3DestinationMassAudit DestinationAudit;
		FCollisionTimingRow SelectionRow;
		SelectionRow.Name = TEXT("event_selection_iiid3");
		SelectionRow.Scope = TEXT("DetectPhaseIIID3DestinationMass inclusive API timing");
		if (!TimeCall(SelectionRow, OutResult.StepCount, [&]() { return Actor->DetectPhaseIIID3DestinationMass(DestinationAudit, CollisionThresholdKm, DestinationMassThresholdRatio); }))
		{
			Actor->Destroy();
			return false;
		}
		SelectionRow.ProbeCount = DestinationAudit.SourceGroupCount;
		SelectionRow.CandidateCount = DestinationAudit.AcceptedMassRecordCount;
		OutResult.Rows.Add(SelectionRow);

		FCarrierLabPhaseIIID4SlabBreakPlanAudit SlabBreakAudit;
		FCollisionTimingRow SlabBreakRow;
		SlabBreakRow.Name = TEXT("slab_break_plan_iiid4");
		SlabBreakRow.Scope = TEXT("PlanPhaseIIID4SlabBreak inclusive API timing");
		if (!TimeCall(SlabBreakRow, OutResult.StepCount, [&]() { return Actor->PlanPhaseIIID4SlabBreak(SlabBreakAudit, CollisionThresholdKm, DestinationMassThresholdRatio); }))
		{
			Actor->Destroy();
			return false;
		}
		SlabBreakRow.ProbeCount = SlabBreakAudit.DestinationMassRecordCount;
		SlabBreakRow.CandidateCount = SlabBreakAudit.ValidPlanCount;
		OutResult.Rows.Add(SlabBreakRow);

		FCarrierLabPhaseIIID5SuturePlanAudit SutureAudit;
		FCollisionTimingRow SutureRow;
		SutureRow.Name = TEXT("suture_plan_iiid5");
		SutureRow.Scope = TEXT("PlanPhaseIIID5Suture inclusive API timing");
		if (!TimeCall(SutureRow, OutResult.StepCount, [&]() { return Actor->PlanPhaseIIID5Suture(SutureAudit, CollisionThresholdKm, DestinationMassThresholdRatio); }))
		{
			Actor->Destroy();
			return false;
		}
		SutureRow.ProbeCount = SutureAudit.SlabBreakPlanCount;
		SutureRow.CandidateCount = SutureAudit.ValidSuturePlanCount;
		OutResult.Rows.Add(SutureRow);

		FCarrierLabPhaseIIID7CollisionUpliftAudit UpliftPlanAudit;
		FCollisionTimingRow UpliftPlanRow;
		UpliftPlanRow.Name = TEXT("uplift_plan_iiid7");
		UpliftPlanRow.Scope = TEXT("PlanPhaseIIID7CollisionUplift inclusive API timing");
		if (!TimeCall(UpliftPlanRow, OutResult.StepCount, [&]() { return Actor->PlanPhaseIIID7CollisionUplift(UpliftPlanAudit, CollisionThresholdKm, DestinationMassThresholdRatio); }))
		{
			Actor->Destroy();
			return false;
		}
		UpliftPlanRow.ProbeCount = UpliftPlanAudit.TopologyAudit.ValidSuturePlanCount;
		UpliftPlanRow.CandidateCount = UpliftPlanAudit.UpliftRecordCount;
		OutResult.Rows.Add(UpliftPlanRow);

		FCarrierLabPhaseIIID7CollisionUpliftAudit UpliftApplyAudit;
		FCollisionTimingRow UpliftApplyRow;
		UpliftApplyRow.Name = TEXT("uplift_apply_iiid7");
		UpliftApplyRow.Scope = TEXT("ApplyPhaseIIID7CollisionUplift inclusive API timing");
		if (!TimeCall(UpliftApplyRow, OutResult.StepCount, [&]() { return Actor->ApplyPhaseIIID7CollisionUplift(UpliftApplyAudit, CollisionThresholdKm, DestinationMassThresholdRatio); }))
		{
			Actor->Destroy();
			return false;
		}
		UpliftApplyRow.ProbeCount = UpliftApplyAudit.TopologyAudit.ValidSuturePlanCount;
		UpliftApplyRow.CandidateCount = UpliftApplyAudit.UpliftRecordCount;
		UpliftApplyRow.MutationCount = UpliftApplyAudit.TopologyAudit.AppliedMutationCount;
		OutResult.Rows.Add(UpliftApplyRow);

		OutResult.PolicyResolvedMultiHitCount += Actor->CurrentMetrics.PolicyResolvedMultiHitCount;
		Actor->Destroy();

		ACarrierLabVisualizationActor* MutationActor = nullptr;
		int32 MutationStep = 0;
		int32 MutationPatchSeed = INDEX_NONE;
		int32 MutationPatchCount = 0;
		double MutationSetupSeconds = 0.0;
		if (!PrepareCollisionActor(
			*World,
			OutResult.SampleCount,
			MutationActor,
			MutationStep,
			MutationPatchSeed,
			MutationPatchCount,
			MutationSetupSeconds))
		{
			return false;
		}

		FCarrierLabPhaseIIID6TopologyMutationAudit MutationAudit;
		FCollisionTimingRow MutationRow;
		MutationRow.Name = TEXT("topology_mutation_iiid6");
		MutationRow.Scope = TEXT("ApplyPhaseIIID6DetachAndSuture inclusive API timing on a fresh matching fixture");
		if (!TimeCall(MutationRow, MutationStep, [&]() { return MutationActor->ApplyPhaseIIID6DetachAndSuture(MutationAudit, CollisionThresholdKm, DestinationMassThresholdRatio); }))
		{
			MutationActor->Destroy();
			return false;
		}
		MutationRow.ProbeCount = MutationAudit.ValidSuturePlanCount;
		MutationRow.CandidateCount = MutationAudit.SuturePlanCount;
		MutationRow.MutationCount = MutationAudit.AppliedMutationCount;
		OutResult.PolicyResolvedMultiHitCount += MutationActor->CurrentMetrics.PolicyResolvedMultiHitCount;
		OutResult.Rows.Add(MutationRow);

		MutationActor->Destroy();
		CollectGarbage(RF_NoFlags);

		for (const FCollisionTimingRow& Row : OutResult.Rows)
		{
			OutResult.TotalMeasuredCollisionSeconds += Row.Seconds;
			OutResult.ProbeCount += Row.ProbeCount;
			OutResult.CandidateCount += Row.CandidateCount;
			OutResult.MutationCount += Row.MutationCount;
		}

		FCollisionTimingRow TotalRow;
		TotalRow.Name = TEXT("total_measured_collision_surface");
		TotalRow.Scope = TEXT("sum of inclusive diagnostic rows; not an exclusive callgraph profile");
		TotalRow.Seconds = OutResult.TotalMeasuredCollisionSeconds;
		FinalizeTimingRow(TotalRow, OutResult.StepCount);
		TotalRow.ProbeCount = OutResult.ProbeCount;
		TotalRow.CandidateCount = OutResult.CandidateCount;
		TotalRow.MutationCount = OutResult.MutationCount;
		OutResult.Rows.Add(TotalRow);

		OutResult.bCompleted =
			OutResult.bFixtureReady &&
			OutResult.StepCount > 0 &&
			OutResult.MutationCount > 0 &&
			OutResult.PolicyResolvedMultiHitCount == 0;
		return OutResult.bCompleted;
	}

	void AppendTimingJson(const FCollisionTimingResult& Result, TArray<FString>& Lines)
	{
		for (const FCollisionTimingRow& Row : Result.Rows)
		{
			Lines.Add(FString::Printf(
				TEXT("{\"kind\":\"collision_timing\",\"tier\":%s,\"row\":%s,\"scope\":%s,\"seconds\":%.9f,\"seconds_per_step\":%.9f,\"paper_table2_seconds_per_step\":%.9f,\"paper_ratio\":%.6f,\"target_ratio\":%.6f,\"target_status\":%s,\"probe_count\":%d,\"candidate_count\":%d,\"mutation_count\":%d}"),
				*JsonString(TierName(Result.Tier)),
				*JsonString(Row.Name),
				*JsonString(Row.Scope),
				Row.Seconds,
				Row.SecondsPerStep,
				Row.PaperSecondsPerStep,
				Row.PaperRatio,
				SoftPaperRatioTarget,
				*JsonString(TargetStatus(Row.PaperRatio)),
				Row.ProbeCount,
				Row.CandidateCount,
				Row.MutationCount));
		}
	}

	FString BuildTimingTable(const FCollisionTimingResult& Result)
	{
		FString Table;
		Table += TEXT("| Row | Measured cost | Per-step cost | Paper Table 2 baseline | Ratio | Target | Probes | Candidates | Mutations |\n");
		Table += TEXT("|---|---:|---:|---:|---:|---|---:|---:|---:|\n");
		for (const FCollisionTimingRow& Row : Result.Rows)
		{
			Table += FString::Printf(
				TEXT("| %s | %.6fs | %.6fs/step | %.3fs/step | %s | %s | %d | %d | %d |\n"),
				*Row.Name,
				Row.Seconds,
				Row.SecondsPerStep,
				Row.PaperSecondsPerStep,
				*RatioString(Row.PaperRatio),
				*TargetStatus(Row.PaperRatio),
				Row.ProbeCount,
				Row.CandidateCount,
				Row.MutationCount);
		}
		return Table;
	}

	FString BuildContainmentReport(
		const FString& OutputRoot,
		const FCollisionTimingResult& Timing,
		const double CommandletSeconds)
	{
		const double IIID8SecondsPerStep = IIID8Replay0WallSeconds / static_cast<double>(IIID8IntegratedStepCount);
		const double IIID8PaperRatio = SafeRatio(IIID8SecondsPerStep, PaperTable2TotalTectonicsSecondsPerStep60k40);
		const bool bTierWithinTarget = TierTargetSeconds(Timing.Tier) <= 0.0 || CommandletSeconds <= TierTargetSeconds(Timing.Tier);

		FString Report;
		Report += TEXT("# Phase III Pre-IIIE Performance Containment\n\n");
		Report += FString::Printf(
			TEXT("Status: %s. This checkpoint installs validation-tier policy and diagnostic timing before IIIE. It does not optimize collision, remesh, or projection code.\n\n"),
			*PassFail(Timing.bCompleted && bTierWithinTarget));
		Report += FString::Printf(TEXT("Output root: `%s`\n\n"), *OutputRoot);

		Report += TEXT("## Paper Table 2 Grounding\n\n");
		Report += FString::Printf(
			TEXT("The paper reports total tectonic-process cost of `%.2fs/step` at `60k` samples / `40` plates in Table 2 (page 9, `docs/ProceduralTectonicPlanets/ProceduralTectonicPlanets.pdf`). The current IIID.8 integrated replay 0 took `%.3fs` for `%d` steps, or `%.6fs/step`, which is `%s` the paper total-process baseline. That is not a fact of nature to absorb with tiering; it is a tracked structural performance finding. A `<= %.0fx` paper-baseline ratio is the current soft target. Exceeding it flags a finding, not an implementation blocker.\n\n"),
			PaperTable2TotalTectonicsSecondsPerStep60k40,
			IIID8Replay0WallSeconds,
			IIID8IntegratedStepCount,
			IIID8SecondsPerStep,
			*RatioString(IIID8PaperRatio),
			SoftPaperRatioTarget);
		Report += FString::Printf(
			TEXT("Paper Table 2 also lists oceanic crust generation at `%.2fs` and plate rifting at `%.2fs`; those rows become active comparison baselines once IIIE/IIIF land.\n\n"),
			PaperTable2OceanicCrustSecondsPerEvent,
			PaperTable2PlateRiftingSecondsPerEvent);

		Report += TEXT("## Validation Tiers\n\n");
		Report += TEXT("| Tier | Default use | Target | Replay policy |\n");
		Report += TEXT("|---|---|---:|---|\n");
		Report += TEXT("| Tiny | focused deterministic semantic fixtures | <15s | optional replay 1 unless claiming determinism |\n");
		Report += TEXT("| Slice | default per-slice validation | <120s | replay 1 opt-in for known-expensive or failed-replay-0 investigation paths |\n");
		Report += TEXT("| Integrated | default sub-phase consolidation | mandatory | replay 1 default-on for gates expected to pass; failed replay 0 may skip replay 1 only with yellow-flag/investigation record |\n");
		Report += TEXT("| Benchmark | optional scaling/repeat diagnosis | none | explicit by operator |\n\n");

		Report += TEXT("Sub-phase consolidation integrated runs are mandatory. Per-slice work defaults to Slice tier. Consolidation defaults to Integrated tier. No soft-skip at consolidation: if an integrated run is skipped, failed, or interrupted, the consolidation report must dispatch the yellow flag by running the integrated test, deferring to a named future slice with rationale, or closing it with explicit reasoning.\n\n");
		Report += TEXT("Long collision+remesh replays currently exceed the paper Table 2 baseline by about `189x` and are tiered to opt-in for routine slice work pending cost-driver investigation. They remain mandatory at sub-phase consolidation.\n\n");

		Report += TEXT("## Diagnostic Timing\n\n");
		Report += FString::Printf(
			TEXT("Tier run: `%s`, fixture `%d` samples / `%d` plates, threshold step `%d`, commandlet wall time `%.6fs`, tier target `%.1fs`. Rows are inclusive API timings, not an exclusive callgraph profile.\n\n"),
			*TierName(Timing.Tier),
			Timing.SampleCount,
			Timing.PlateCount,
			Timing.StepCount,
			CommandletSeconds,
			TierTargetSeconds(Timing.Tier));
		Report += BuildTimingTable(Timing);
		Report += TEXT("\n");

		Report += TEXT("## Findings\n\n");
		Report += FString::Printf(
			TEXT("- `paper_table2_ratio`: IIID.8 replay 0 is `%s` paper Table 2 total-process baseline at the same 60k/40 configuration.\n"),
			*RatioString(IIID8PaperRatio));
		for (const FCollisionTimingRow& Row : Timing.Rows)
		{
			if (Row.PaperRatio > SoftPaperRatioTarget)
			{
				Report += FString::Printf(
					TEXT("- `%s`: `%s` exceeds the `<= %.0fx` soft target in this fixture.\n"),
					*Row.Name,
					*RatioString(Row.PaperRatio),
					SoftPaperRatioTarget);
			}
		}
		if (Timing.PolicyResolvedMultiHitCount == 0)
		{
			Report += TEXT("- `lab_policy_influence`: collision timing fixture did not require centroid/random multi-hit resolution.\n");
		}
		Report += TEXT("- `iiid8_replay1`: replay 1 was intentionally skipped in the IIID consolidation run after replay 0 failed the quantitative gate. That skip is an investigation record, not a passing determinism claim.\n\n");

		Report += TEXT("## Containment Rules\n\n");
		Report += TEXT("- Future commandlets must report their validation tier.\n");
		Report += TEXT("- If a commandlet emits a stop-and-investigate note, that note becomes a yellow flag.\n");
		Report += TEXT("- A yellow flag blocks sub-phase consolidation until it is dispatched by an integrated run, a named future-slice deferral with rationale, or explicit closure reasoning.\n");
		Report += TEXT("- Replay 1 is context-aware: default-on for consolidation gates expected to pass; opt-in or skipped only when replay 0 already failed, the path is investigation-only, or the report records the yellow flag/deferral.\n");
		Report += TEXT("- Lab remesh / Stage 1.5 remesh remains labeled as lab-policy until IIIE replaces the primary path.\n\n");

		Report += TEXT("## Cost Deferral, Not Cost Solution\n\n");
		Report += TEXT("This containment defers integrated cost to consolidation moments while preserving integrated measurement at sub-phase boundaries. Per-step cost reduction is a pending investigation. Treating the 189x gap as inevitable would accept a structural performance defect on a project whose paper reports interactive rates for this configuration.\n\n");

		Report += TEXT("## Next Slice\n\n");
		Report += TEXT("Queue `Pre-IIIE.2 Cost Driver Identification` before behavior work beyond IIIE.1. Scope: run against existing fixtures with this diagnostic timing instrumentation, add no new simulation behavior, identify the dominant per-step cost source, and propose targeted remediation. Likely suspects to time explicitly: IIIB.3 matrix construction, IIIB.6 neighbor propagation expansion, full audit struct construction every step, per-step BVH rebuilds for all plates, and one-time setup costs amortized over only 32 IIID.8 steps.\n\n");

		Report += TEXT("## Scope Notes\n\n");
		Report += TEXT("This slice adds diagnostics and policy only. It does not add carrier authority state, global sample ownership, repair, recovery, backfill, cache-as-authority, projection-derived state, remesh replacement, or collision optimization.\n");
		return Report;
	}

	FString BuildCostDriverReport(const FCollisionTimingResult& Timing)
	{
		const double IIID8SecondsPerStep = IIID8Replay0WallSeconds / static_cast<double>(IIID8IntegratedStepCount);
		const double IIID8PaperRatio = SafeRatio(IIID8SecondsPerStep, PaperTable2TotalTectonicsSecondsPerStep60k40);

		FString Report;
		Report += TEXT("# Phase III Pre-IIIE Cost Driver Identification\n\n");
		Report += TEXT("Status: queued. This is the next-after-containment investigation slice. No optimization has landed here.\n\n");
		Report += TEXT("## Initial Paper-Comparison Table\n\n");
		Report += TEXT("| Surface | Our measured cost | Paper Table 2 comparison | Ratio | Notes |\n");
		Report += TEXT("|---|---:|---:|---:|---|\n");
		Report += FString::Printf(
			TEXT("| IIID.8 integrated total | %.6fs/step | %.3fs/step total tectonic processes | %s | replay 0 only; replay 1 skipped after failed gate |\n"),
			IIID8SecondsPerStep,
			PaperTable2TotalTectonicsSecondsPerStep60k40,
			*RatioString(IIID8PaperRatio));
		for (const FCollisionTimingRow& Row : Timing.Rows)
		{
			Report += FString::Printf(
				TEXT("| %s | %.6fs/step | %.3fs/step total tectonic processes | %s | %s |\n"),
				*Row.Name,
				Row.SecondsPerStep,
				Row.PaperSecondsPerStep,
				*RatioString(Row.PaperRatio),
				*Row.Scope);
		}
		Report += FString::Printf(
			TEXT("| IIIE oceanic crust generation | not measured | %.3fs/event | pending | activate once IIIE lands |\n"),
			PaperTable2OceanicCrustSecondsPerEvent);
		Report += FString::Printf(
			TEXT("| IIIF plate rifting | not measured | %.3fs/event | pending | activate once IIIF lands |\n\n"),
			PaperTable2PlateRiftingSecondsPerEvent);

		Report += TEXT("## Investigation Scope\n\n");
		Report += TEXT("- Run existing fixtures only; do not add simulation behavior.\n");
		Report += TEXT("- Separate one-time setup from per-step process cost.\n");
		Report += TEXT("- Identify the dominant source before optimizing.\n");
		Report += TEXT("- Specific suspects: IIIB.3 matrix ray construction, IIIB.6 neighbor propagation, full audit struct construction, per-step all-plate BVH rebuilds, and setup costs amortized across short 32-step windows.\n\n");
		Report += TEXT("## Stop Condition\n\n");
		Report += TEXT("Do not normalize the current 189x integrated ratio as acceptable overhead. If the dominant cost driver cannot be isolated with existing instrumentation, write an investigation checkpoint before further integrated-performance claims.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIPerformanceContainmentCommandlet::UCarrierLabPhaseIIIPerformanceContainmentCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIPerformanceContainmentCommandlet::Main(const FString& Params)
{
	const double CommandletStartSeconds = FPlatformTime::Seconds();
	const EValidationTier Tier = ParseValidationTier(Params);
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIIPerformanceContainment tier=%s output=%s"), *TierName(Tier), *OutputRoot);

	FCollisionTimingResult Timing;
	const bool bTimingOk = RunCollisionTimingFixture(Tier, Timing);
	const double CommandletSeconds = FPlatformTime::Seconds() - CommandletStartSeconds;
	const bool bTierWithinTarget = TierTargetSeconds(Tier) <= 0.0 || CommandletSeconds <= TierTargetSeconds(Tier);

	TArray<FString> JsonLines;
	JsonLines.Add(FString::Printf(
		TEXT("{\"kind\":\"validation_tier\",\"tier\":%s,\"completed\":%s,\"target_seconds\":%.6f,\"commandlet_seconds\":%.6f,\"within_target\":%s,\"sample_count\":%d,\"plate_count\":%d,\"step_count\":%d}"),
		*JsonString(TierName(Tier)),
		bTimingOk ? TEXT("true") : TEXT("false"),
		TierTargetSeconds(Tier),
		CommandletSeconds,
		bTierWithinTarget ? TEXT("true") : TEXT("false"),
		Timing.SampleCount,
		Timing.PlateCount,
		Timing.StepCount));
	JsonLines.Add(FString::Printf(
		TEXT("{\"kind\":\"paper_table2_ratio\",\"surface\":\"iiid8_integrated_total\",\"wall_seconds\":%.6f,\"step_count\":%d,\"seconds_per_step\":%.9f,\"paper_table2_seconds_per_step\":%.9f,\"paper_ratio\":%.6f,\"target_ratio\":%.6f,\"target_status\":%s}"),
		IIID8Replay0WallSeconds,
		IIID8IntegratedStepCount,
		IIID8Replay0WallSeconds / static_cast<double>(IIID8IntegratedStepCount),
		PaperTable2TotalTectonicsSecondsPerStep60k40,
		SafeRatio(IIID8Replay0WallSeconds / static_cast<double>(IIID8IntegratedStepCount), PaperTable2TotalTectonicsSecondsPerStep60k40),
		SoftPaperRatioTarget,
		*JsonString(TargetStatus(SafeRatio(IIID8Replay0WallSeconds / static_cast<double>(IIID8IntegratedStepCount), PaperTable2TotalTectonicsSecondsPerStep60k40)))));
	AppendTimingJson(Timing, JsonLines);

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(
		FString::Join(JsonLines, LINE_TERMINATOR) + LINE_TERMINATOR,
		*MetricsPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	const FString ContainmentReport = BuildContainmentReport(OutputRoot, Timing, CommandletSeconds);
	const FString ContainmentReportPath = ResolveContainmentReportPath(Params);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ContainmentReportPath), true);
	FFileHelper::SaveStringToFile(
		ContainmentReport,
		*ContainmentReportPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	const FString CostDriverReport = BuildCostDriverReport(Timing);
	const FString CostDriverReportPath = ResolveCostDriverReportPath();
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(CostDriverReportPath), true);
	FFileHelper::SaveStringToFile(
		CostDriverReport,
		*CostDriverReportPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIIPerformanceContainment report: %s"), *ContainmentReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIIPerformanceContainment cost-driver report: %s"), *CostDriverReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIIPerformanceContainment metrics: %s"), *MetricsPath);

	return bTimingOk && bTierWithinTarget ? 0 : 1;
}
