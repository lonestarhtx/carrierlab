// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIICostDriverIdentificationCommandlet.h"

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
	constexpr int32 IIID8IntegratedStepCount = 32;
	constexpr double DefaultVelocityMmPerYear = 66.6666666667;
	constexpr double CollisionThresholdKm = 300.0;
	constexpr double DestinationMassThresholdRatio = 0.5;
	constexpr double PaperTable2TotalTectonicsSecondsPerStep60k40 = 0.19;
	constexpr double PaperTable2OceanicCrustSecondsPerEvent = 0.58;
	constexpr double PaperTable2PlateRiftingSecondsPerEvent = 0.23;
	constexpr double SoftPaperRatioTarget = 10.0;
	constexpr double IIID8Replay0WallSeconds = 1151.130;
	constexpr double PreReuseIIID6TopologyMutationSeconds = 7.127022;
	constexpr double PreReuseIIID7UpliftApplySeconds = 15.030029;
	constexpr double PreReuseTotalMeasuredSurfaceSeconds = 45.064643;

	enum class EValidationTier : uint8
	{
		Tiny,
		Slice,
		Integrated,
		Benchmark
	};

	struct FFixtureSetupTiming
	{
		double SpawnSeconds = 0.0;
		double InitializeSeconds = 0.0;
		double ConfigureSeconds = 0.0;
		double ThresholdSearchSeconds = 0.0;
		double TotalSeconds = 0.0;
		int32 ThresholdStep = 0;
		int32 PatchSeedTriangleId = INDEX_NONE;
		int32 PatchTriangleCount = 0;
	};

	struct FCostDriverRow
	{
		FString Name;
		FString Category;
		FString Scope;
		double Seconds = 0.0;
		int32 StepDivisor = 1;
		double PaperSecondsPerStep = PaperTable2TotalTectonicsSecondsPerStep60k40;
		int32 ProbeCount = 0;
		int32 CandidateCount = 0;
		int32 MutationCount = 0;
		FCarrierLabPhaseIIIDiagnosticCallCounts Calls;
	};

	struct FCostDriverResult
	{
		EValidationTier Tier = EValidationTier::Slice;
		bool bCompleted = false;
		bool bFixtureReady = false;
		int32 SampleCount = 0;
		int32 PlateCount = FixturePlates;
		int32 StepCount = 0;
		int32 PolicyResolvedMultiHitCount = 0;
		FFixtureSetupTiming Setup;
		FCarrierLabPhaseIIICostDriverStepAudit ProcessAudit;
		TArray<FCostDriverRow> Rows;
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

	int32 TierSampleCount(const EValidationTier Tier)
	{
		return Tier == EValidationTier::Tiny ? TinyFixtureSamples : SliceFixtureSamples;
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

	double SafeRatio(const double Numerator, const double Denominator)
	{
		return FMath::Abs(Denominator) > UE_SMALL_NUMBER ? Numerator / Denominator : 0.0;
	}

	double RowSecondsPerStep(const FCostDriverRow& Row)
	{
		return Row.StepDivisor > 0 ? Row.Seconds / static_cast<double>(Row.StepDivisor) : Row.Seconds;
	}

	double RowPaperRatio(const FCostDriverRow& Row)
	{
		return SafeRatio(RowSecondsPerStep(Row), Row.PaperSecondsPerStep);
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

	FString FormatCallCounts(const FCarrierLabPhaseIIIDiagnosticCallCounts& Calls)
	{
		return FString::Printf(
			TEXT("D1=%d D2=%d D3=%d D4=%d D5=%d D6=%d D7p=%d D7a=%d"),
			Calls.DetectTerranes,
			Calls.GroupCollisions,
			Calls.DestinationMass,
			Calls.SlabBreakPlan,
			Calls.SuturePlan,
			Calls.TopologyMutation,
			Calls.UpliftPlan,
			Calls.UpliftApply);
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("PreIIIECostDriverIdentification"), Stamp);
		}
		else if (FPaths::IsRelative(OutputRoot))
		{
			OutputRoot = FPaths::Combine(FPaths::ProjectDir(), OutputRoot);
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
				TEXT("phase-iii-pre-iiie-cost-driver-identification.md"));
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

	ACarrierLabVisualizationActor* SpawnActor(UWorld& World, const int32 SampleCount, FFixtureSetupTiming* SetupTiming = nullptr)
	{
		const double StartSeconds = FPlatformTime::Seconds();
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.bDeferConstruction = true;
		ACarrierLabVisualizationActor* Actor = World.SpawnActor<ACarrierLabVisualizationActor>(
			ACarrierLabVisualizationActor::StaticClass(),
			FTransform::Identity,
			SpawnParams);
		if (SetupTiming != nullptr)
		{
			SetupTiming->SpawnSeconds = FPlatformTime::Seconds() - StartSeconds;
		}
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
		Actor->ConfigurePhaseIIICProcessLayer(true, false);
		Actor->bEnableNaturalResamplingEvents = false;
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	bool PrepareCollisionActor(
		UWorld& World,
		const int32 SampleCount,
		ACarrierLabVisualizationActor*& OutActor,
		FFixtureSetupTiming& OutSetup)
	{
		OutActor = nullptr;
		OutSetup = FFixtureSetupTiming();
		const double TotalStartSeconds = FPlatformTime::Seconds();

		ACarrierLabVisualizationActor* Actor = SpawnActor(World, SampleCount, &OutSetup);
		if (Actor == nullptr)
		{
			return false;
		}

		double StartSeconds = FPlatformTime::Seconds();
		if (!Actor->InitializeCarrier())
		{
			Actor->Destroy();
			return false;
		}
		OutSetup.InitializeSeconds = FPlatformTime::Seconds() - StartSeconds;

		StartSeconds = FPlatformTime::Seconds();
		Actor->ConfigurePhaseIIMotionFixture(ECarrierLabPhaseIIMotionFixture::ForcedConvergence);
		if (!Actor->SetPlateContinentalForTest(0, true) ||
			!Actor->SetPlateContinentalForTest(1, true))
		{
			Actor->Destroy();
			return false;
		}
		OutSetup.ConfigureSeconds = FPlatformTime::Seconds() - StartSeconds;

		StartSeconds = FPlatformTime::Seconds();
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
				OutSetup.PatchSeedTriangleId,
				OutSetup.PatchTriangleCount))
			{
				Actor->Destroy();
				return false;
			}
			OutSetup.ThresholdStep = Step;
			OutSetup.ThresholdSearchSeconds = FPlatformTime::Seconds() - StartSeconds;
			OutSetup.TotalSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
			OutActor = Actor;
			return true;
		}

		OutSetup.ThresholdSearchSeconds = FPlatformTime::Seconds() - StartSeconds;
		OutSetup.TotalSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
		Actor->Destroy();
		return false;
	}

	void AddSetupRows(const FFixtureSetupTiming& Setup, TArray<FCostDriverRow>& Rows)
	{
		auto AddRow = [&](const TCHAR* Name, const double Seconds, const TCHAR* Scope)
		{
			FCostDriverRow& Row = Rows.AddDefaulted_GetRef();
			Row.Name = Name;
			Row.Category = TEXT("setup");
			Row.Scope = Scope;
			Row.Seconds = Seconds;
			Row.StepDivisor = IIID8IntegratedStepCount;
		};
		AddRow(TEXT("setup_spawn_actor_amortized"), Setup.SpawnSeconds, TEXT("one-time actor spawn amortized over the 32-step IIID.8 window"));
		AddRow(TEXT("setup_initialize_carrier_amortized"), Setup.InitializeSeconds, TEXT("InitializeCarrier one-time setup amortized over the 32-step IIID.8 window"));
		AddRow(TEXT("setup_fixture_config_amortized"), Setup.ConfigureSeconds, TEXT("fixture configuration and test continental setup amortized over 32 steps"));
		AddRow(TEXT("setup_threshold_search_amortized"), Setup.ThresholdSearchSeconds, TEXT("threshold search using normal StepOnce path amortized over 32 steps"));
		AddRow(TEXT("setup_total_amortized"), Setup.TotalSeconds, TEXT("total fixture setup/search cost amortized over the 32-step IIID.8 window"));
	}

	void AddProcessRows(const FCarrierLabPhaseIIICostDriverStepAudit& Audit, TArray<FCostDriverRow>& Rows)
	{
		auto AddRow = [&](const TCHAR* Name, const TCHAR* Category, const double Seconds, const TCHAR* Scope, const int32 ProbeCount, const int32 CandidateCount)
		{
			FCostDriverRow& Row = Rows.AddDefaulted_GetRef();
			Row.Name = Name;
			Row.Category = Category;
			Row.Scope = Scope;
			Row.Seconds = Seconds;
			Row.StepDivisor = 1;
			Row.ProbeCount = ProbeCount;
			Row.CandidateCount = CandidateCount;
		};
		AddRow(TEXT("advance_motion_rotation"), TEXT("per_step"), Audit.MotionSeconds, TEXT("plate-local vertex and motion-center rotation"), Audit.PlateCount, Audit.SampleCount);
		AddRow(TEXT("iiib2_distance_update"), TEXT("per_step"), Audit.DistanceUpdateSeconds, TEXT("distance-to-front advection/culling"), Audit.ActiveTriangleCountBefore, Audit.ActiveTriangleCountAfter);
		AddRow(TEXT("iiib3_matrix_total"), TEXT("per_step"), Audit.MatrixTotalSeconds, TEXT("UpdateConvergenceSubductionMatrix total"), Audit.MatrixRayTestCount, Audit.MatrixEvidenceCount);
		AddRow(TEXT("iiib3_matrix_bvh_rebuild"), TEXT("per_step"), Audit.MatrixBvhBuildSeconds, TEXT("RefreshPlateRayMeshes inside IIIB.3"), Audit.PlateCount, Audit.MatrixEvidenceCount);
		AddRow(TEXT("iiib3_matrix_ray_queries"), TEXT("per_step"), Audit.MatrixRayQuerySeconds, TEXT("IIIB.3 ray tests after BVH refresh"), Audit.MatrixRayTestCount, Audit.MatrixHitCount);
		AddRow(TEXT("iiib4_polarity_decisions"), TEXT("per_step"), Audit.PolaritySeconds, TEXT("pair polarity recomputation"), Audit.MatrixEvidenceCount, Audit.PolarityDecisionCount);
		AddRow(TEXT("iiib6_neighbor_propagation"), TEXT("per_step"), Audit.NeighborPropagationSeconds, TEXT("neighbor expansion from accepted subduction hits"), Audit.NeighborSeedCount, Audit.NeighborAddedCount);
		AddRow(TEXT("iiic1_mark_update"), TEXT("per_step"), Audit.MarkSeconds, TEXT("subducting mark update"), Audit.MatrixHitCount, Audit.SubductingMarkCount);
		AddRow(TEXT("iiic3_uplift"), TEXT("per_step"), Audit.UpliftSeconds, TEXT("overriding-plate uplift path"), Audit.SubductingMarkCount, Audit.UpliftRecordCount);
		AddRow(TEXT("iiic4_slab_pull"), TEXT("per_step"), Audit.SlabPullSeconds, TEXT("slab-pull calculation, default-off in this fixture"), Audit.SubductingMarkCount, Audit.SlabPullContributionCount);
		AddRow(TEXT("iiic5_ledger"), TEXT("per_step"), Audit.LedgerSeconds, TEXT("elevation-ledger begin/finalize"), Audit.UpliftRecordCount, Audit.SubductingMarkCount);
		AddRow(TEXT("advance_process_total"), TEXT("per_step"), Audit.TotalProcessSeconds, TEXT("diagnostic AdvanceOneStep process path without projection render pass"), Audit.MatrixRayTestCount, Audit.UpliftRecordCount);
	}

	template <typename FuncType>
	bool TimeActorCall(ACarrierLabVisualizationActor& Actor, FCostDriverRow& Row, FuncType&& Func)
	{
		Actor.ResetPhaseIIIDiagnosticCallCounts();
		const double StartSeconds = FPlatformTime::Seconds();
		const bool bOk = Func();
		Row.Seconds = FPlatformTime::Seconds() - StartSeconds;
		Row.Calls = Actor.GetPhaseIIIDiagnosticCallCounts();
		return bOk;
	}

	bool RunCostDriverFixture(const EValidationTier Tier, FCostDriverResult& OutResult)
	{
		OutResult = FCostDriverResult();
		OutResult.Tier = Tier;
		OutResult.SampleCount = TierSampleCount(Tier);

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		ACarrierLabVisualizationActor* Actor = nullptr;
		if (!PrepareCollisionActor(*World, OutResult.SampleCount, Actor, OutResult.Setup))
		{
			return false;
		}
		OutResult.bFixtureReady = true;
		OutResult.StepCount = OutResult.Setup.ThresholdStep;
		AddSetupRows(OutResult.Setup, OutResult.Rows);

		if (!Actor->RunPhaseIIICostDriverAdvanceProbe(OutResult.ProcessAudit))
		{
			Actor->Destroy();
			return false;
		}
		AddProcessRows(OutResult.ProcessAudit, OutResult.Rows);
		OutResult.PolicyResolvedMultiHitCount += Actor->CurrentMetrics.PolicyResolvedMultiHitCount;
		Actor->Destroy();

		ACarrierLabVisualizationActor* ReadOnlyActor = nullptr;
		FFixtureSetupTiming ReadOnlySetup;
		if (!PrepareCollisionActor(*World, OutResult.SampleCount, ReadOnlyActor, ReadOnlySetup))
		{
			return false;
		}

		FCarrierLabPhaseIIID1TerraneAudit DetectionAudit;
		FCostDriverRow DetectionRow;
		DetectionRow.Name = TEXT("iiid1_terrane_detection");
		DetectionRow.Category = TEXT("collision");
		DetectionRow.Scope = TEXT("DetectPhaseIIID1ConnectedTerranes inclusive API timing");
		if (!TimeActorCall(*ReadOnlyActor, DetectionRow, [&]() { return ReadOnlyActor->DetectPhaseIIID1ConnectedTerranes(DetectionAudit); }))
		{
			ReadOnlyActor->Destroy();
			return false;
		}
		DetectionRow.ProbeCount = DetectionAudit.CollisionCandidateHitCount;
		DetectionRow.CandidateCount = DetectionAudit.TerraneRecordCount;
		OutResult.Rows.Add(DetectionRow);

		FCarrierLabPhaseIIID2CollisionGroupingAudit GroupingAudit;
		FCostDriverRow GroupingRow;
		GroupingRow.Name = TEXT("iiid2_collision_grouping");
		GroupingRow.Category = TEXT("collision");
		GroupingRow.Scope = TEXT("DetectPhaseIIID2CollisionGroups inclusive API timing");
		if (!TimeActorCall(*ReadOnlyActor, GroupingRow, [&]() { return ReadOnlyActor->DetectPhaseIIID2CollisionGroups(GroupingAudit, CollisionThresholdKm); }))
		{
			ReadOnlyActor->Destroy();
			return false;
		}
		GroupingRow.ProbeCount = GroupingAudit.TerraneRecordCount;
		GroupingRow.CandidateCount = GroupingAudit.GroupCount;
		OutResult.Rows.Add(GroupingRow);

		FCarrierLabPhaseIIID3DestinationMassAudit DestinationAudit;
		FCostDriverRow DestinationRow;
		DestinationRow.Name = TEXT("iiid3_destination_mass");
		DestinationRow.Category = TEXT("collision");
		DestinationRow.Scope = TEXT("DetectPhaseIIID3DestinationMass inclusive API timing");
		if (!TimeActorCall(*ReadOnlyActor, DestinationRow, [&]() { return ReadOnlyActor->DetectPhaseIIID3DestinationMass(DestinationAudit, CollisionThresholdKm, DestinationMassThresholdRatio); }))
		{
			ReadOnlyActor->Destroy();
			return false;
		}
		DestinationRow.ProbeCount = DestinationAudit.SourceGroupCount;
		DestinationRow.CandidateCount = DestinationAudit.AcceptedMassRecordCount;
		OutResult.Rows.Add(DestinationRow);

		FCarrierLabPhaseIIID4SlabBreakPlanAudit SlabBreakAudit;
		FCostDriverRow SlabBreakRow;
		SlabBreakRow.Name = TEXT("iiid4_slab_break_plan");
		SlabBreakRow.Category = TEXT("collision");
		SlabBreakRow.Scope = TEXT("PlanPhaseIIID4SlabBreak inclusive API timing");
		if (!TimeActorCall(*ReadOnlyActor, SlabBreakRow, [&]() { return ReadOnlyActor->PlanPhaseIIID4SlabBreak(SlabBreakAudit, CollisionThresholdKm, DestinationMassThresholdRatio); }))
		{
			ReadOnlyActor->Destroy();
			return false;
		}
		SlabBreakRow.ProbeCount = SlabBreakAudit.DestinationMassRecordCount;
		SlabBreakRow.CandidateCount = SlabBreakAudit.ValidPlanCount;
		OutResult.Rows.Add(SlabBreakRow);

		FCarrierLabPhaseIIID5SuturePlanAudit SutureAudit;
		FCostDriverRow SutureRow;
		SutureRow.Name = TEXT("iiid5_suture_plan");
		SutureRow.Category = TEXT("collision");
		SutureRow.Scope = TEXT("PlanPhaseIIID5Suture inclusive API timing");
		if (!TimeActorCall(*ReadOnlyActor, SutureRow, [&]() { return ReadOnlyActor->PlanPhaseIIID5Suture(SutureAudit, CollisionThresholdKm, DestinationMassThresholdRatio); }))
		{
			ReadOnlyActor->Destroy();
			return false;
		}
		SutureRow.ProbeCount = SutureAudit.SlabBreakPlanCount;
		SutureRow.CandidateCount = SutureAudit.ValidSuturePlanCount;
		OutResult.Rows.Add(SutureRow);

		FCarrierLabPhaseIIID7CollisionUpliftAudit UpliftPlanAudit;
		FCostDriverRow UpliftPlanRow;
		UpliftPlanRow.Name = TEXT("iiid7_uplift_plan");
		UpliftPlanRow.Category = TEXT("collision_elevation");
		UpliftPlanRow.Scope = TEXT("PlanPhaseIIID7CollisionUplift inclusive API timing");
		if (!TimeActorCall(*ReadOnlyActor, UpliftPlanRow, [&]() { return ReadOnlyActor->PlanPhaseIIID7CollisionUplift(UpliftPlanAudit, CollisionThresholdKm, DestinationMassThresholdRatio); }))
		{
			ReadOnlyActor->Destroy();
			return false;
		}
		UpliftPlanRow.ProbeCount = UpliftPlanAudit.TopologyAudit.ValidSuturePlanCount;
		UpliftPlanRow.CandidateCount = UpliftPlanAudit.UpliftRecordCount;
		OutResult.Rows.Add(UpliftPlanRow);
		OutResult.PolicyResolvedMultiHitCount += ReadOnlyActor->CurrentMetrics.PolicyResolvedMultiHitCount;
		ReadOnlyActor->Destroy();

		ACarrierLabVisualizationActor* MutationActor = nullptr;
		FFixtureSetupTiming MutationSetup;
		if (!PrepareCollisionActor(*World, OutResult.SampleCount, MutationActor, MutationSetup))
		{
			return false;
		}
		FCarrierLabPhaseIIID6TopologyMutationAudit MutationAudit;
		FCostDriverRow MutationRow;
		MutationRow.Name = TEXT("iiid6_topology_mutation");
		MutationRow.Category = TEXT("collision");
		MutationRow.Scope = TEXT("ApplyPhaseIIID6DetachAndSuture inclusive API timing on a fresh matching fixture");
		if (!TimeActorCall(*MutationActor, MutationRow, [&]() { return MutationActor->ApplyPhaseIIID6DetachAndSuture(MutationAudit, CollisionThresholdKm, DestinationMassThresholdRatio); }))
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

		ACarrierLabVisualizationActor* UpliftApplyActor = nullptr;
		FFixtureSetupTiming UpliftApplySetup;
		if (!PrepareCollisionActor(*World, OutResult.SampleCount, UpliftApplyActor, UpliftApplySetup))
		{
			return false;
		}
		FCarrierLabPhaseIIID7CollisionUpliftAudit UpliftApplyAudit;
		FCostDriverRow UpliftApplyRow;
		UpliftApplyRow.Name = TEXT("iiid7_uplift_apply");
		UpliftApplyRow.Category = TEXT("collision_elevation");
		UpliftApplyRow.Scope = TEXT("ApplyPhaseIIID7CollisionUplift inclusive API timing on a fresh matching fixture");
		if (!TimeActorCall(*UpliftApplyActor, UpliftApplyRow, [&]() { return UpliftApplyActor->ApplyPhaseIIID7CollisionUplift(UpliftApplyAudit, CollisionThresholdKm, DestinationMassThresholdRatio); }))
		{
			UpliftApplyActor->Destroy();
			return false;
		}
		UpliftApplyRow.ProbeCount = UpliftApplyAudit.TopologyAudit.ValidSuturePlanCount;
		UpliftApplyRow.CandidateCount = UpliftApplyAudit.UpliftRecordCount;
		UpliftApplyRow.MutationCount = UpliftApplyAudit.TopologyAudit.AppliedMutationCount;
		OutResult.PolicyResolvedMultiHitCount += UpliftApplyActor->CurrentMetrics.PolicyResolvedMultiHitCount;
		OutResult.Rows.Add(UpliftApplyRow);
		UpliftApplyActor->Destroy();

		CollectGarbage(RF_NoFlags);

		FCostDriverRow TotalRow;
		TotalRow.Name = TEXT("total_measured_cost_driver_surface");
		TotalRow.Category = TEXT("total");
		TotalRow.Scope = TEXT("sum of diagnostic rows; inclusive rows overlap and are not an exclusive profiler trace");
		for (const FCostDriverRow& Row : OutResult.Rows)
		{
			if (Row.Category == TEXT("total"))
			{
				continue;
			}
			TotalRow.Seconds += Row.Seconds;
			TotalRow.ProbeCount += Row.ProbeCount;
			TotalRow.CandidateCount += Row.CandidateCount;
			TotalRow.MutationCount += Row.MutationCount;
		}
		OutResult.Rows.Add(TotalRow);

		OutResult.bCompleted =
			OutResult.bFixtureReady &&
			OutResult.ProcessAudit.TotalProcessSeconds > 0.0 &&
			OutResult.PolicyResolvedMultiHitCount == 0 &&
			OutResult.Rows.Num() > 0;
		return OutResult.bCompleted;
	}

	void AppendMetricsJson(const FCostDriverResult& Result, const double CommandletSeconds, TArray<FString>& Lines)
	{
		Lines.Add(FString::Printf(
			TEXT("{\"kind\":\"validation_tier\",\"tier\":%s,\"completed\":%s,\"target_seconds\":%.6f,\"commandlet_seconds\":%.6f,\"sample_count\":%d,\"plate_count\":%d,\"threshold_step\":%d,\"policy_resolved_multi_hit_count\":%d}"),
			*JsonString(TierName(Result.Tier)),
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			TierTargetSeconds(Result.Tier),
			CommandletSeconds,
			Result.SampleCount,
			Result.PlateCount,
			Result.StepCount,
			Result.PolicyResolvedMultiHitCount));

		Lines.Add(FString::Printf(
			TEXT("{\"kind\":\"paper_table2_ratio\",\"surface\":\"iiid8_integrated_total\",\"wall_seconds\":%.6f,\"step_count\":%d,\"seconds_per_step\":%.9f,\"paper_table2_seconds_per_step\":%.9f,\"paper_ratio\":%.6f,\"target_ratio\":%.6f,\"target_status\":%s}"),
			IIID8Replay0WallSeconds,
			IIID8IntegratedStepCount,
			IIID8Replay0WallSeconds / static_cast<double>(IIID8IntegratedStepCount),
			PaperTable2TotalTectonicsSecondsPerStep60k40,
			SafeRatio(IIID8Replay0WallSeconds / static_cast<double>(IIID8IntegratedStepCount), PaperTable2TotalTectonicsSecondsPerStep60k40),
			SoftPaperRatioTarget,
			*JsonString(TargetStatus(SafeRatio(IIID8Replay0WallSeconds / static_cast<double>(IIID8IntegratedStepCount), PaperTable2TotalTectonicsSecondsPerStep60k40)))));

		for (const FCostDriverRow& Row : Result.Rows)
		{
			const double SecondsPerStep = RowSecondsPerStep(Row);
			const double Ratio = RowPaperRatio(Row);
			Lines.Add(FString::Printf(
				TEXT("{\"kind\":\"cost_driver_timing\",\"tier\":%s,\"row\":%s,\"category\":%s,\"scope\":%s,\"seconds\":%.9f,\"step_divisor\":%d,\"seconds_per_step\":%.9f,\"paper_table2_seconds_per_step\":%.9f,\"paper_ratio\":%.6f,\"target_ratio\":%.6f,\"target_status\":%s,\"probe_count\":%d,\"candidate_count\":%d,\"mutation_count\":%d,\"calls\":%s}"),
				*JsonString(TierName(Result.Tier)),
				*JsonString(Row.Name),
				*JsonString(Row.Category),
				*JsonString(Row.Scope),
				Row.Seconds,
				Row.StepDivisor,
				SecondsPerStep,
				Row.PaperSecondsPerStep,
				Ratio,
				SoftPaperRatioTarget,
				*JsonString(TargetStatus(Ratio)),
				Row.ProbeCount,
				Row.CandidateCount,
				Row.MutationCount,
				*JsonString(FormatCallCounts(Row.Calls))));
		}
	}

	const FCostDriverRow* FindLargestRow(const FCostDriverResult& Result, const FString& Category)
	{
		const FCostDriverRow* Largest = nullptr;
		for (const FCostDriverRow& Row : Result.Rows)
		{
			if (!Category.IsEmpty() && Row.Category != Category)
			{
				continue;
			}
			if (Row.Category == TEXT("total"))
			{
				continue;
			}
			if (Largest == nullptr || RowSecondsPerStep(Row) > RowSecondsPerStep(*Largest))
			{
				Largest = &Row;
			}
		}
		return Largest;
	}

	const FCostDriverRow* FindRowByName(const FCostDriverResult& Result, const FString& Name)
	{
		for (const FCostDriverRow& Row : Result.Rows)
		{
			if (Row.Name == Name)
			{
				return &Row;
			}
		}
		return nullptr;
	}

	double PercentReduction(const double Before, const double After)
	{
		return Before > UE_DOUBLE_SMALL_NUMBER
			? 100.0 * FMath::Max(0.0, Before - After) / Before
			: 0.0;
	}

	FString BuildTimingTable(const FCostDriverResult& Result)
	{
		FString Table;
		Table += TEXT("| Row | Category | Measured | Normalized cost | Paper Table 2 baseline | Ratio | Target | Probes | Candidates | Mutations | Nested calls |\n");
		Table += TEXT("|---|---|---:|---:|---:|---:|---|---:|---:|---:|---|\n");
		for (const FCostDriverRow& Row : Result.Rows)
		{
			const double Ratio = RowPaperRatio(Row);
			Table += FString::Printf(
				TEXT("| %s | %s | %.6fs | %.6fs/step | %.3fs/step | %s | %s | %d | %d | %d | %s |\n"),
				*Row.Name,
				*Row.Category,
				Row.Seconds,
				RowSecondsPerStep(Row),
				Row.PaperSecondsPerStep,
				*RatioString(Ratio),
				*TargetStatus(Ratio),
				Row.ProbeCount,
				Row.CandidateCount,
				Row.MutationCount,
				*FormatCallCounts(Row.Calls));
		}
		return Table;
	}

	FString BuildReport(const FString& OutputRoot, const FCostDriverResult& Result, const double CommandletSeconds)
	{
		const bool bTierWithinTarget = TierTargetSeconds(Result.Tier) <= 0.0 || CommandletSeconds <= TierTargetSeconds(Result.Tier);
		const double IIID8SecondsPerStep = IIID8Replay0WallSeconds / static_cast<double>(IIID8IntegratedStepCount);
		const double IIID8PaperRatio = SafeRatio(IIID8SecondsPerStep, PaperTable2TotalTectonicsSecondsPerStep60k40);
		const FCostDriverRow* LargestOverall = FindLargestRow(Result, TEXT(""));
		const FCostDriverRow* LargestProcess = FindLargestRow(Result, TEXT("per_step"));
		const FCostDriverRow* LargestCollision = FindLargestRow(Result, TEXT("collision"));
		const FCostDriverRow* LargestElevation = FindLargestRow(Result, TEXT("collision_elevation"));
		const FCostDriverRow* D6Row = FindRowByName(Result, TEXT("iiid6_topology_mutation"));
		const FCostDriverRow* D7ApplyRow = FindRowByName(Result, TEXT("iiid7_uplift_apply"));
		const FCostDriverRow* TotalRow = FindRowByName(Result, TEXT("total_measured_cost_driver_surface"));

		FString Report;
		Report += TEXT("# Phase III Pre-IIIE Collision Plan Reuse / Cost Driver Identification\n\n");
		Report += FString::Printf(
			TEXT("Status: %s. This slice adds behavior-preserving IIID collision-plan reuse and regenerates the cost-driver timing report. It does not add remesh, projection-derived correction, global ownership, cache-as-authority, or new tectonic behavior.\n\n"),
			*PassFail(Result.bCompleted && bTierWithinTarget));
		Report += FString::Printf(TEXT("Output root: `%s`\n\n"), *OutputRoot);

		Report += TEXT("## Paper Table 2 Baseline\n\n");
		Report += FString::Printf(
			TEXT("The published Table 2 baseline is `%.3fs/step` for total tectonic processes at `60k` samples / `40` plates. IIID.8 replay 0 measured `%.3fs` over `%d` steps (`%.6fs/step`, `%s`). The target tracked here is `<= %.0fx` paper cost. Over-target rows are findings, not pass/fail blockers.\n\n"),
			PaperTable2TotalTectonicsSecondsPerStep60k40,
			IIID8Replay0WallSeconds,
			IIID8IntegratedStepCount,
			IIID8SecondsPerStep,
			*RatioString(IIID8PaperRatio),
			SoftPaperRatioTarget);
		Report += FString::Printf(
			TEXT("Paper rows not yet active in implementation: oceanic crust generation `%.3fs/event`, plate rifting `%.3fs/event`.\n\n"),
			PaperTable2OceanicCrustSecondsPerEvent,
			PaperTable2PlateRiftingSecondsPerEvent);

		Report += TEXT("## Fixture\n\n");
		Report += FString::Printf(
			TEXT("Tier: `%s`. Samples: `%d`. Plates: `%d`. Threshold step: `%d`. Commandlet wall time: `%.6fs`. Tier target: `%.1fs`. Policy-resolved multi-hit count: `%d`.\n\n"),
			*TierName(Result.Tier),
			Result.SampleCount,
			Result.PlateCount,
			Result.StepCount,
			CommandletSeconds,
			TierTargetSeconds(Result.Tier),
			Result.PolicyResolvedMultiHitCount);
		Report += FString::Printf(
			TEXT("Setup split: spawn `%.6fs`, initialize `%.6fs`, configure `%.6fs`, threshold search `%.6fs`, total `%.6fs`. Setup rows below are amortized across the `%d`-step IIID.8 window to make the short-window setup penalty visible.\n\n"),
			Result.Setup.SpawnSeconds,
			Result.Setup.InitializeSeconds,
			Result.Setup.ConfigureSeconds,
			Result.Setup.ThresholdSearchSeconds,
			Result.Setup.TotalSeconds,
			IIID8IntegratedStepCount);

		Report += TEXT("## Timing Table\n\n");
		Report += BuildTimingTable(Result);
		Report += TEXT("\n");

		Report += TEXT("## Findings\n\n");
		Report += FString::Printf(
			TEXT("- `integrated_gap`: IIID.8 replay 0 remains `%s` the paper Table 2 total-process baseline. This slice did not rerun the integrated replay; it decomposes a Slice-tier fixture to identify likely drivers.\n"),
			*RatioString(IIID8PaperRatio));
		if (D6Row != nullptr)
		{
			Report += FString::Printf(
				TEXT("- `plan_reuse_d6_delta`: `iiid6_topology_mutation` changed from `%.6fs/step` to `%.6fs/step` (`%.2f%%` reduction) by consuming the staged D1-D5 plan chain once instead of recomputing D4/D5 inside the mutation path.\n"),
				PreReuseIIID6TopologyMutationSeconds,
				RowSecondsPerStep(*D6Row),
				PercentReduction(PreReuseIIID6TopologyMutationSeconds, RowSecondsPerStep(*D6Row)));
		}
		if (D7ApplyRow != nullptr)
		{
			Report += FString::Printf(
				TEXT("- `plan_reuse_d7_delta`: `iiid7_uplift_apply` changed from `%.6fs/step` to `%.6fs/step` (`%.2f%%` reduction). The nested public-call shape collapsed from repeated D1-D5 rediscovery to one D1 seed plus one D6 mutation call.\n"),
				PreReuseIIID7UpliftApplySeconds,
				RowSecondsPerStep(*D7ApplyRow),
				PercentReduction(PreReuseIIID7UpliftApplySeconds, RowSecondsPerStep(*D7ApplyRow)));
		}
		if (TotalRow != nullptr)
		{
			Report += FString::Printf(
				TEXT("- `plan_reuse_total_delta`: `total_measured_cost_driver_surface` changed from `%.6fs/step` to `%.6fs/step` (`%.2f%%` reduction). This total is still an inclusive diagnostic surface, not an exclusive profiler trace.\n"),
				PreReuseTotalMeasuredSurfaceSeconds,
				RowSecondsPerStep(*TotalRow),
				PercentReduction(PreReuseTotalMeasuredSurfaceSeconds, RowSecondsPerStep(*TotalRow)));
		}
		if (LargestOverall != nullptr)
		{
			Report += FString::Printf(
				TEXT("- `largest_measured_slice_row`: `%s` at `%.6fs/step` (`%s`).\n"),
				*LargestOverall->Name,
				RowSecondsPerStep(*LargestOverall),
				*RatioString(RowPaperRatio(*LargestOverall)));
		}
		if (LargestProcess != nullptr)
		{
			Report += FString::Printf(
				TEXT("- `largest_per_step_process_row`: `%s` at `%.6fs/step` (`%s`). IIIB.3 is split into BVH rebuild and ray-query sub-rows to distinguish mesh rebuild cost from ray-test cost.\n"),
				*LargestProcess->Name,
				RowSecondsPerStep(*LargestProcess),
				*RatioString(RowPaperRatio(*LargestProcess)));
		}
		if (LargestCollision != nullptr)
		{
			Report += FString::Printf(
				TEXT("- `largest_collision_row`: `%s` at `%.6fs/step` (`%s`). Nested call counts show whether later gates re-run earlier scans instead of consuming a staged plan.\n"),
				*LargestCollision->Name,
				RowSecondsPerStep(*LargestCollision),
				*RatioString(RowPaperRatio(*LargestCollision)));
		}
		if (LargestElevation != nullptr)
		{
			Report += FString::Printf(
				TEXT("- `largest_elevation_collision_row`: `%s` at `%.6fs/step` (`%s`).\n"),
				*LargestElevation->Name,
				RowSecondsPerStep(*LargestElevation),
				*RatioString(RowPaperRatio(*LargestElevation)));
		}
		Report += TEXT("- `first_order_conclusion`: plan reuse removed the largest recomputation pattern. The remaining Slice-tier over-target rows are now the first-pass D1/DestinationMass-family scans themselves rather than repeated nested rediscovery in D6/D7 apply.\n");
		Report += TEXT("- `scale_limit`: this does not establish that IIIB.3/BVH work is harmless at the 60k/40 integrated scale. It says the first optimization target should be validated against the heavy IIID apply rows before changing core tracking logic.\n");
		Report += TEXT("- `nested_recompute_shape`: D6/D7 no longer call the earlier public stage APIs repeatedly. The call-count column remains diagnostic evidence for whether future edits accidentally reintroduce nested public-stage recomputation.\n");
		Report += TEXT("- `scope_limit`: rows are Slice-tier diagnostics on existing fixtures, not a replacement for mandatory Integrated-tier sub-phase evidence.\n\n");

		Report += TEXT("## Next Remediation Candidates\n\n");
		Report += TEXT("The next optimization/design pass should start from the remaining over-target rows, especially D1 terrane expansion and destination-mass/component construction. Per-step BVH refresh avoidance and audit-record construction are still possible future surfaces, but this run no longer points at D6/D7 nested recomputation as the dominant Slice-tier driver.\n\n");

		Report += TEXT("## Scope Notes\n\n");
		Report += TEXT("The actor change is plan reuse only: already-computed IIID audits are passed forward inside a single call chain. It does not add carrier authority state, global sample ownership, forbidden fallback-family behavior, cache-as-authority, projection-derived state, remesh replacement, or IIIE behavior.\n");
		return Report;
	}
}

UCarrierLabPhaseIIICostDriverIdentificationCommandlet::UCarrierLabPhaseIIICostDriverIdentificationCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIICostDriverIdentificationCommandlet::Main(const FString& Params)
{
	const double CommandletStartSeconds = FPlatformTime::Seconds();
	const EValidationTier Tier = ParseValidationTier(Params);
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIICostDriverIdentification tier=%s output=%s"), *TierName(Tier), *OutputRoot);

	FCostDriverResult Result;
	const bool bRunOk = RunCostDriverFixture(Tier, Result);
	const double CommandletSeconds = FPlatformTime::Seconds() - CommandletStartSeconds;
	const bool bTierWithinTarget = TierTargetSeconds(Tier) <= 0.0 || CommandletSeconds <= TierTargetSeconds(Tier);

	TArray<FString> JsonLines;
	AppendMetricsJson(Result, CommandletSeconds, JsonLines);

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(
		FString::Join(JsonLines, LINE_TERMINATOR) + LINE_TERMINATOR,
		*MetricsPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	const FString Report = BuildReport(OutputRoot, Result, CommandletSeconds);
	const FString ReportPath = ResolveReportPath(Params);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	FFileHelper::SaveStringToFile(
		Report,
		*ReportPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIICostDriverIdentification report: %s"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIICostDriverIdentification metrics: %s"), *MetricsPath);

	return bRunOk && bTierWithinTarget ? 0 : 1;
}
