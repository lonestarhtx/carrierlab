// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIISlice5Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr double ReconcileTolerance = 1.0e-12;

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

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseII"), TEXT("Slice5"), Stamp);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	int32 ParseIntParam(const FString& Params, const TCHAR* Key, const int32 DefaultValue)
	{
		int32 Value = DefaultValue;
		FParse::Value(*Params, Key, Value);
		return Value;
	}

	double PaperTable2SecondsForSamples(const int32 SampleCount)
	{
		if (SampleCount <= 60000)
		{
			return 0.19;
		}
		if (SampleCount <= 100000)
		{
			return 0.28;
		}
		if (SampleCount <= 250000)
		{
			return 1.24;
		}
		return 1.90;
	}

	const TCHAR* MotionFixtureName(const ECarrierLabPhaseIIMotionFixture Fixture)
	{
		switch (Fixture)
		{
		case ECarrierLabPhaseIIMotionFixture::Zero: return TEXT("zero_motion");
		case ECarrierLabPhaseIIMotionFixture::ForcedConvergence: return TEXT("forced_convergence");
		case ECarrierLabPhaseIIMotionFixture::ForcedDivergence: return TEXT("forced_divergence");
		case ECarrierLabPhaseIIMotionFixture::TripleJunction: return TEXT("triple_junction");
		case ECarrierLabPhaseIIMotionFixture::Default:
		default: return TEXT("default");
		}
	}

	struct FSlice5FixtureConfig
	{
		FString Name;
		FString Family;
		int32 SampleCount = 60000;
		int32 PlateCount = 40;
		int32 StepCount = 32;
		double ContinentalPlateFraction = 0.30;
		ECarrierLabPhaseIIMotionFixture Fixture = ECarrierLabPhaseIIMotionFixture::Default;
		bool bUseFixturePolarity = false;
		int32 FixtureUnderPlate = INDEX_NONE;
		int32 FixtureOverPlate = INDEX_NONE;
		bool bExpectNoMaterialChange = false;
		bool bExpectSubductionRecords = false;
		bool bPrimaryScaling = false;
	};

	struct FSlice5ReplayResult
	{
		FString FixtureName;
		FString Family;
		int32 Replay = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		double TotalSeconds = 0.0;
		double StepWallSecondsTotal = 0.0;
		double StepProjectionSecondsTotal = 0.0;
		double StepBvhSecondsTotal = 0.0;
		double StepQuerySecondsTotal = 0.0;
		double StepDriftSecondsTotal = 0.0;
		double StepBoundarySecondsTotal = 0.0;
		double StepHashSecondsTotal = 0.0;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterMetrics;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerMetrics;
		FCarrierLabVisualizationMetrics ProjectionMetrics;
		bool bCompleted = false;
	};

	struct FSlice5FixtureSummary
	{
		FString FixtureName;
		FString Family;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		bool bPrimaryScaling = false;
		bool bCompleted = false;
		bool bContactReplayHash = false;
		bool bLabelReplayHash = false;
		bool bFilterReplayHash = false;
		bool bMaterialReplayHash = false;
		bool bPostStateHashMatch = false;
		bool bGlobalMaterialReconcile = false;
		bool bPerPlateMaterialReconcile = false;
		bool bNoHiddenMaterialChange = false;
		bool bControlStable = true;
		bool bExpectedSubductionRecords = true;
		bool bRuntimeWithinPaperBudget = true;
		double PaperBudgetSeconds = 0.0;
		double AvgStepWallSeconds = 0.0;
		double AvgStepKernelSeconds = 0.0;
		double PhaseIIEventSeconds = 0.0;
		double TotalReplaySeconds = 0.0;
		FCarrierLabPhaseIIContactMetrics ContactA;
		FCarrierLabPhaseIITriangleLabelMetrics LabelA;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterA;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerA;
	};

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

	ACarrierLabVisualizationActor* SpawnSlice5Actor(UWorld& World, const FSlice5FixtureConfig& Config)
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
		Actor->SampleCount = Config.SampleCount;
		Actor->PlateCount = Config.PlateCount;
		Actor->Seed = 42;
		Actor->ContinentalPlateFraction = Config.ContinentalPlateFraction;
		Actor->VelocityMmPerYear = 66.6666666667;
		Actor->MultiHitPolicy = ECarrierLabMultiHitPolicy::Centroid;
		Actor->VisualizationLayer = ECarrierLabVisualizationLayer::PlateId;
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	void AccumulateStepTimings(FSlice5ReplayResult& Result, const FCarrierLabVisualizationMetrics& Metrics, const double WallSeconds)
	{
		Result.StepWallSecondsTotal += WallSeconds;
		Result.StepProjectionSecondsTotal += Metrics.ProjectionSeconds;
		Result.StepBvhSecondsTotal += Metrics.BvhBuildSeconds;
		Result.StepQuerySecondsTotal += Metrics.ProjectionQuerySeconds;
		Result.StepDriftSecondsTotal += Metrics.DriftMetricsSeconds;
		Result.StepBoundarySecondsTotal += Metrics.BoundaryMaskSeconds;
		Result.StepHashSecondsTotal += Metrics.HashSeconds;
	}

	bool RunFixtureReplay(const FSlice5FixtureConfig& Config, const int32 Replay, FSlice5ReplayResult& OutResult)
	{
		OutResult = FSlice5ReplayResult();
		OutResult.FixtureName = Config.Name;
		OutResult.Family = Config.Family;
		OutResult.Replay = Replay;
		OutResult.SampleCount = Config.SampleCount;
		OutResult.PlateCount = Config.PlateCount;
		OutResult.StepCount = Config.StepCount;

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 5 fixture %s replay %d could not find a world."), *Config.Name, Replay);
			return false;
		}

		const double TotalStartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnSlice5Actor(*World, Config);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier())
		{
			Actor->Destroy();
			return false;
		}

		Actor->ConfigurePhaseIIMotionFixture(Config.Fixture);
		for (int32 Step = 0; Step < Config.StepCount; ++Step)
		{
			const double StepStartSeconds = FPlatformTime::Seconds();
			Actor->StepOnce();
			AccumulateStepTimings(OutResult, Actor->CurrentMetrics, FPlatformTime::Seconds() - StepStartSeconds);
		}

		TArray<FCarrierLabPhaseIIContactRecord> Contacts;
		if (!Actor->DetectPhaseIIContacts(Contacts, OutResult.ContactMetrics))
		{
			Actor->Destroy();
			return false;
		}

		FCarrierLabPhaseIITriangleLabelConfig LabelConfig;
		LabelConfig.bUseFixturePolarity = Config.bUseFixturePolarity;
		LabelConfig.FixtureUnderPlate = Config.FixtureUnderPlate;
		LabelConfig.FixtureOverPlate = Config.FixtureOverPlate;

		TArray<FCarrierLabPhaseIITriangleLabelRecord> Labels;
		if (!Actor->BuildPhaseIITriangleLabels(Contacts, LabelConfig, Labels, OutResult.LabelMetrics))
		{
			Actor->Destroy();
			return false;
		}

		TArray<FCarrierLabPhaseIIFilterDecisionRecord> Decisions;
		if (!Actor->ApplyPhaseIIResamplingFilterEvent(Labels, Decisions, OutResult.FilterMetrics, nullptr, &OutResult.LedgerMetrics))
		{
			Actor->Destroy();
			return false;
		}
		OutResult.ProjectionMetrics = Actor->CurrentMetrics;
		OutResult.TotalSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
		OutResult.bCompleted = true;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return true;
	}

	double NetDelta(const double Gain, const double Loss)
	{
		return Gain - Loss;
	}

	double PercentOfNetLoss(const double CategoryNetDelta, const double TotalNetDelta)
	{
		if (TotalNetDelta >= -KINDA_SMALL_NUMBER || CategoryNetDelta >= 0.0)
		{
			return 0.0;
		}
		return (-CategoryNetDelta / -TotalNetDelta) * 100.0;
	}

	double ReconcileToleranceForLedger(const FCarrierLabPhaseIIMaterialLedgerMetrics& Ledger)
	{
		return FMath::Max(ReconcileTolerance, Ledger.TotalArea * 1.0e-12);
	}

	FSlice5FixtureSummary SummarizeFixture(const FSlice5FixtureConfig& Config, const FSlice5ReplayResult& A, const FSlice5ReplayResult& B)
	{
		FSlice5FixtureSummary Summary;
		Summary.FixtureName = Config.Name;
		Summary.Family = Config.Family;
		Summary.SampleCount = Config.SampleCount;
		Summary.PlateCount = Config.PlateCount;
		Summary.StepCount = Config.StepCount;
		Summary.bPrimaryScaling = Config.bPrimaryScaling;
		Summary.bCompleted = A.bCompleted && B.bCompleted;
		Summary.ContactA = A.ContactMetrics;
		Summary.LabelA = A.LabelMetrics;
		Summary.FilterA = A.FilterMetrics;
		Summary.LedgerA = A.LedgerMetrics;
		Summary.bContactReplayHash = Summary.bCompleted && A.ContactMetrics.ContactLogHash == B.ContactMetrics.ContactLogHash;
		Summary.bLabelReplayHash = Summary.bCompleted && A.LabelMetrics.TriangleLabelHash == B.LabelMetrics.TriangleLabelHash;
		Summary.bFilterReplayHash = Summary.bCompleted && A.FilterMetrics.FilterDecisionHash == B.FilterMetrics.FilterDecisionHash;
		Summary.bMaterialReplayHash = Summary.bCompleted && A.LedgerMetrics.MaterialLedgerHash == B.LedgerMetrics.MaterialLedgerHash;
		Summary.bPostStateHashMatch = Summary.bCompleted && A.FilterMetrics.StateHashAfter == B.FilterMetrics.StateHashAfter;
		const double ToleranceA = ReconcileToleranceForLedger(A.LedgerMetrics);
		const double ToleranceB = ReconcileToleranceForLedger(B.LedgerMetrics);
		Summary.bGlobalMaterialReconcile =
			FMath::Abs(A.LedgerMetrics.ContinentalDeltaResidual) <= ToleranceA &&
			FMath::Abs(B.LedgerMetrics.ContinentalDeltaResidual) <= ToleranceB &&
			FMath::Abs(A.LedgerMetrics.OceanicDeltaResidual) <= ToleranceA &&
			FMath::Abs(B.LedgerMetrics.OceanicDeltaResidual) <= ToleranceB;
		Summary.bPerPlateMaterialReconcile =
			FMath::Abs(A.LedgerMetrics.MaxPerPlateContinentalResidual) <= ToleranceA &&
			FMath::Abs(B.LedgerMetrics.MaxPerPlateContinentalResidual) <= ToleranceB;
		Summary.bNoHiddenMaterialChange = Summary.bGlobalMaterialReconcile && Summary.bPerPlateMaterialReconcile;
		Summary.bControlStable = !Config.bExpectNoMaterialChange ||
			(A.LedgerMetrics.ChangedRecordCount == 0 && B.LedgerMetrics.ChangedRecordCount == 0 &&
				FMath::Abs(A.LedgerMetrics.ContinentalMassAfter - A.LedgerMetrics.ContinentalMassBefore) <= ReconcileTolerance &&
				FMath::Abs(B.LedgerMetrics.ContinentalMassAfter - B.LedgerMetrics.ContinentalMassBefore) <= ReconcileTolerance);
		Summary.bExpectedSubductionRecords = !Config.bExpectSubductionRecords ||
			(A.LedgerMetrics.SubductionRecordCount > 0 && B.LedgerMetrics.SubductionRecordCount > 0);
		Summary.PaperBudgetSeconds = PaperTable2SecondsForSamples(Config.SampleCount);
		Summary.AvgStepWallSeconds = Config.StepCount > 0 ? A.StepWallSecondsTotal / static_cast<double>(Config.StepCount) : 0.0;
		Summary.AvgStepKernelSeconds = Config.StepCount > 0 ? A.StepProjectionSecondsTotal / static_cast<double>(Config.StepCount) : 0.0;
		Summary.PhaseIIEventSeconds =
			A.ContactMetrics.ContactDetectionSeconds +
			A.LabelMetrics.TriangleLabelSeconds +
			A.FilterMetrics.ResampleEventSeconds;
		Summary.TotalReplaySeconds = A.TotalSeconds;
		Summary.bRuntimeWithinPaperBudget = !Config.bPrimaryScaling || Summary.AvgStepKernelSeconds <= Summary.PaperBudgetSeconds;
		return Summary;
	}

	bool SummaryPasses(const FSlice5FixtureSummary& Summary)
	{
		return Summary.bCompleted &&
			Summary.bContactReplayHash &&
			Summary.bLabelReplayHash &&
			Summary.bFilterReplayHash &&
			Summary.bMaterialReplayHash &&
			Summary.bPostStateHashMatch &&
			Summary.bGlobalMaterialReconcile &&
			Summary.bPerPlateMaterialReconcile &&
			Summary.bNoHiddenMaterialChange &&
			Summary.bControlStable &&
			Summary.bExpectedSubductionRecords;
	}

	FString PassFail(const bool bValue)
	{
		return bValue ? TEXT("pass") : TEXT("fail");
	}

	FString RuntimeVerdict(const FSlice5FixtureSummary& Summary)
	{
		if (!Summary.bPrimaryScaling)
		{
			return TEXT("control");
		}
		return Summary.bRuntimeWithinPaperBudget ? TEXT("pass") : TEXT("finding");
	}

	FString ReplayMetricJson(const FSlice5ReplayResult& Result)
	{
		const FCarrierLabPhaseIIContactMetrics& Contact = Result.ContactMetrics;
		const FCarrierLabPhaseIITriangleLabelMetrics& Label = Result.LabelMetrics;
		const FCarrierLabPhaseIIResamplingFilterMetrics& Filter = Result.FilterMetrics;
		const FCarrierLabPhaseIIMaterialLedgerMetrics& Ledger = Result.LedgerMetrics;
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		const double AvgStepKernel = Result.StepCount > 0 ? Result.StepProjectionSecondsTotal / static_cast<double>(Result.StepCount) : 0.0;
		const double AvgStepWall = Result.StepCount > 0 ? Result.StepWallSecondsTotal / static_cast<double>(Result.StepCount) : 0.0;
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"family\":%s,\"replay\":%d,\"step_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"avg_step_kernel_seconds\":%.12f,\"avg_step_wall_seconds\":%.12f,\"step_projection_total_seconds\":%.12f,\"step_bvh_total_seconds\":%.12f,\"step_query_total_seconds\":%.12f,\"step_drift_total_seconds\":%.12f,\"step_boundary_total_seconds\":%.12f,\"step_hash_total_seconds\":%.12f,\"contact_seconds\":%.12f,\"label_seconds\":%.12f,\"filter_seconds\":%.12f,\"resample_event_seconds\":%.12f,\"total_replay_seconds\":%.12f,\"contact_hash\":%s,\"contact_record_count\":%d,\"third_plate_contact_count\":%d,\"subduction_candidate_count\":%d,\"label_hash\":%s,\"label_record_count\":%d,\"filter_decision_hash\":%s,\"material_ledger_hash\":%s,\"material_record_count\":%d,\"changed_record_count\":%d,\"plate_changed_record_count\":%d,\"single_hit_transfer_count\":%d,\"subduction_count\":%d,\"gap_fill_count\":%d,\"non_separating_gap_fill_count\":%d,\"unresolved_same_material_count\":%d,\"unresolved_triple_junction_count\":%d,\"unresolved_mixed_material_count\":%d,\"filter_exhausted_count\":%d,\"continental_mass_before\":%.15f,\"continental_mass_after\":%.15f,\"ledger_continental_delta\":%.15f,\"continental_delta_residual\":%.15f,\"max_per_plate_continental_residual\":%.15f,\"single_hit_continental_loss\":%.15f,\"single_hit_continental_gain\":%.15f,\"single_hit_continental_net\":%.15f,\"single_hit_uniform_continental_count\":%d,\"single_hit_uniform_continental_loss\":%.15f,\"single_hit_uniform_continental_gain\":%.15f,\"single_hit_uniform_continental_net\":%.15f,\"single_hit_uniform_oceanic_count\":%d,\"single_hit_uniform_oceanic_loss\":%.15f,\"single_hit_uniform_oceanic_gain\":%.15f,\"single_hit_uniform_oceanic_net\":%.15f,\"single_hit_mixed_triangle_count\":%d,\"single_hit_mixed_triangle_loss\":%.15f,\"single_hit_mixed_triangle_gain\":%.15f,\"single_hit_mixed_triangle_net\":%.15f,\"single_hit_unknown_triangle_count\":%d,\"single_hit_unknown_triangle_loss\":%.15f,\"single_hit_unknown_triangle_gain\":%.15f,\"single_hit_unknown_triangle_net\":%.15f,\"subduction_continental_loss\":%.15f,\"subduction_continental_gain\":%.15f,\"subduction_continental_net\":%.15f,\"gap_fill_continental_loss\":%.15f,\"gap_fill_continental_gain\":%.15f,\"gap_fill_continental_net\":%.15f,\"unresolved_same_continental_loss\":%.15f,\"unresolved_same_continental_gain\":%.15f,\"unresolved_same_continental_net\":%.15f,\"unresolved_triple_continental_loss\":%.15f,\"unresolved_triple_continental_gain\":%.15f,\"unresolved_triple_continental_net\":%.15f,\"unresolved_mixed_continental_loss\":%.15f,\"unresolved_mixed_continental_gain\":%.15f,\"unresolved_mixed_continental_net\":%.15f,\"auth_caf_before\":%.12f,\"auth_caf_after\":%.12f,\"projected_caf_before\":%.12f,\"projected_caf_after\":%.12f,\"state_hash_after\":%s,\"memory_gb\":%.12f}"),
			*JsonString(Result.FixtureName),
			*JsonString(Result.Family),
			Result.Replay,
			Result.StepCount,
			Result.SampleCount,
			Result.PlateCount,
			AvgStepKernel,
			AvgStepWall,
			Result.StepProjectionSecondsTotal,
			Result.StepBvhSecondsTotal,
			Result.StepQuerySecondsTotal,
			Result.StepDriftSecondsTotal,
			Result.StepBoundarySecondsTotal,
			Result.StepHashSecondsTotal,
			Contact.ContactDetectionSeconds,
			Label.TriangleLabelSeconds,
			Filter.FilterSeconds,
			Filter.ResampleEventSeconds,
			Result.TotalSeconds,
			*JsonString(Contact.ContactLogHash),
			Contact.ContactRecordCount,
			Contact.ThirdPlateContactCount,
			Contact.SubductionCandidateCount,
			*JsonString(Label.TriangleLabelHash),
			Label.LabelRecordCount,
			*JsonString(Filter.FilterDecisionHash),
			*JsonString(Ledger.MaterialLedgerHash),
			Ledger.RecordCount,
			Ledger.ChangedRecordCount,
			Ledger.PlateChangedRecordCount,
			Ledger.SingleHitTransferRecordCount,
			Ledger.SubductionRecordCount,
			Ledger.GapFillRecordCount,
			Ledger.NonSeparatingGapFillRecordCount,
			Ledger.UnresolvedSameMaterialRecordCount,
			Ledger.UnresolvedTripleJunctionRecordCount,
			Ledger.UnresolvedMixedMaterialRecordCount,
			Ledger.FilterExhaustedRecordCount,
			Ledger.ContinentalMassBefore,
			Ledger.ContinentalMassAfter,
			Ledger.LedgerContinentalDelta,
			Ledger.ContinentalDeltaResidual,
			Ledger.MaxPerPlateContinentalResidual,
			Ledger.SingleHitTransferContinentalLoss,
			Ledger.SingleHitTransferContinentalGain,
			NetDelta(Ledger.SingleHitTransferContinentalGain, Ledger.SingleHitTransferContinentalLoss),
			Ledger.SingleHitUniformContinentalRecordCount,
			Ledger.SingleHitUniformContinentalLoss,
			Ledger.SingleHitUniformContinentalGain,
			NetDelta(Ledger.SingleHitUniformContinentalGain, Ledger.SingleHitUniformContinentalLoss),
			Ledger.SingleHitUniformOceanicRecordCount,
			Ledger.SingleHitUniformOceanicLoss,
			Ledger.SingleHitUniformOceanicGain,
			NetDelta(Ledger.SingleHitUniformOceanicGain, Ledger.SingleHitUniformOceanicLoss),
			Ledger.SingleHitMixedTriangleRecordCount,
			Ledger.SingleHitMixedTriangleLoss,
			Ledger.SingleHitMixedTriangleGain,
			NetDelta(Ledger.SingleHitMixedTriangleGain, Ledger.SingleHitMixedTriangleLoss),
			Ledger.SingleHitUnknownTriangleRecordCount,
			Ledger.SingleHitUnknownTriangleLoss,
			Ledger.SingleHitUnknownTriangleGain,
			NetDelta(Ledger.SingleHitUnknownTriangleGain, Ledger.SingleHitUnknownTriangleLoss),
			Ledger.SubductionContinentalLoss,
			Ledger.SubductionContinentalGain,
			NetDelta(Ledger.SubductionContinentalGain, Ledger.SubductionContinentalLoss),
			Ledger.GapFillContinentalLoss,
			Ledger.GapFillContinentalGain,
			NetDelta(Ledger.GapFillContinentalGain, Ledger.GapFillContinentalLoss),
			Ledger.UnresolvedSameMaterialContinentalLoss,
			Ledger.UnresolvedSameMaterialContinentalGain,
			NetDelta(Ledger.UnresolvedSameMaterialContinentalGain, Ledger.UnresolvedSameMaterialContinentalLoss),
			Ledger.UnresolvedTripleJunctionContinentalLoss,
			Ledger.UnresolvedTripleJunctionContinentalGain,
			NetDelta(Ledger.UnresolvedTripleJunctionContinentalGain, Ledger.UnresolvedTripleJunctionContinentalLoss),
			Ledger.UnresolvedMixedMaterialContinentalLoss,
			Ledger.UnresolvedMixedMaterialContinentalGain,
			NetDelta(Ledger.UnresolvedMixedMaterialContinentalGain, Ledger.UnresolvedMixedMaterialContinentalLoss),
			Filter.AuthoritativeCAFBefore,
			Filter.AuthoritativeCAFAfter,
			Filter.ProjectedCAFBefore,
			Filter.ProjectedCAFAfter,
			*JsonString(Filter.StateHashAfter),
			MemoryGb);
	}

	void AddFixtureSuiteForResolution(TArray<FSlice5FixtureConfig>& Fixtures, const FString& Label, const int32 SampleCount, const int32 DefaultSteps)
	{
		Fixtures.Add({FString::Printf(TEXT("cadence_%s_primary"), *Label), TEXT("primary"), SampleCount, 40, DefaultSteps, 0.30, ECarrierLabPhaseIIMotionFixture::Default, false, INDEX_NONE, INDEX_NONE, false, true, true});
		Fixtures.Add({FString::Printf(TEXT("%s_forced_convergence_under_1"), *Label), TEXT("forced_convergence_under_1"), SampleCount, 2, 40, 1.00, ECarrierLabPhaseIIMotionFixture::ForcedConvergence, true, 1, 0, false, true, false});
		Fixtures.Add({FString::Printf(TEXT("%s_forced_convergence_under_0"), *Label), TEXT("forced_convergence_under_0"), SampleCount, 2, 40, 1.00, ECarrierLabPhaseIIMotionFixture::ForcedConvergence, true, 0, 1, false, true, false});
		Fixtures.Add({FString::Printf(TEXT("%s_forced_divergence_step0"), *Label), TEXT("forced_divergence_step0"), SampleCount, 2, 0, 0.50, ECarrierLabPhaseIIMotionFixture::ForcedDivergence, false, INDEX_NONE, INDEX_NONE, true, false, false});
		Fixtures.Add({FString::Printf(TEXT("%s_same_pair_mixed_signal"), *Label), TEXT("same_pair_mixed_signal"), SampleCount, 2, 40, 1.00, ECarrierLabPhaseIIMotionFixture::ForcedDivergence, true, 1, 0, false, true, false});
		Fixtures.Add({FString::Printf(TEXT("%s_zero_motion"), *Label), TEXT("zero_motion"), SampleCount, 40, DefaultSteps, 0.30, ECarrierLabPhaseIIMotionFixture::Zero, false, INDEX_NONE, INDEX_NONE, true, false, false});
		Fixtures.Add({FString::Printf(TEXT("%s_single_plate"), *Label), TEXT("single_plate"), SampleCount, 1, DefaultSteps, 0.30, ECarrierLabPhaseIIMotionFixture::Default, false, INDEX_NONE, INDEX_NONE, true, false, false});
		Fixtures.Add({FString::Printf(TEXT("%s_all_continental_zero_motion"), *Label), TEXT("all_continental_zero_motion"), SampleCount, 40, DefaultSteps, 1.00, ECarrierLabPhaseIIMotionFixture::Zero, false, INDEX_NONE, INDEX_NONE, true, false, false});
		Fixtures.Add({FString::Printf(TEXT("%s_ocean_only_zero_motion"), *Label), TEXT("ocean_only_zero_motion"), SampleCount, 40, DefaultSteps, 0.00, ECarrierLabPhaseIIMotionFixture::Zero, false, INDEX_NONE, INDEX_NONE, true, false, false});
		Fixtures.Add({FString::Printf(TEXT("%s_ocean_only_forced_convergence_under_1"), *Label), TEXT("ocean_only_forced_convergence_under_1"), SampleCount, 2, 40, 0.00, ECarrierLabPhaseIIMotionFixture::ForcedConvergence, true, 1, 0, true, true, false});
	}

	FString BuildReport(const FString& OutputRoot, const TArray<FSlice5FixtureSummary>& Summaries)
	{
		FString Report = TEXT("# Phase II Slice 5 Checkpoint: Resolution Scaling\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This checkpoint runs the accepted Slice 3+4 contact-label-filter-material-accounting stack across the requested sample resolutions. It does not add new process behavior. The purpose is to test whether the Slice 4 audit equation and material-delta breakdown remain stable as sample density increases.\n\n");
		Report += TEXT("Audit equation: `active_after = active_before + single_hit_transfer + consumed_by_subduction + overwritten_by_gap_fill + unresolved_same_material + unresolved_triple_junction + unresolved_mixed_material + filter_exhausted_unknown + numeric_residual`.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Fixture | Family | Samples | Plates | Steps | Contact replay | Label replay | Filter replay | Material replay | Post-state replay | Global reconcile | Per-plate reconcile | Control stable | Expected subduction | Runtime | Verdict |\n");
		Report += TEXT("|---|---|---:|---:|---:|---|---|---|---|---|---|---|---|---|---|---|\n");
		for (const FSlice5FixtureSummary& Summary : Summaries)
		{
			Report += FString::Printf(
				TEXT("| %s | %s | %d | %d | %d | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |\n"),
				*Summary.FixtureName,
				*Summary.Family,
				Summary.SampleCount,
				Summary.PlateCount,
				Summary.StepCount,
				*PassFail(Summary.bContactReplayHash),
				*PassFail(Summary.bLabelReplayHash),
				*PassFail(Summary.bFilterReplayHash),
				*PassFail(Summary.bMaterialReplayHash),
				*PassFail(Summary.bPostStateHashMatch),
				*PassFail(Summary.bGlobalMaterialReconcile),
				*PassFail(Summary.bPerPlateMaterialReconcile),
				*PassFail(Summary.bControlStable),
				*PassFail(Summary.bExpectedSubductionRecords),
				*RuntimeVerdict(Summary),
				SummaryPasses(Summary) ? TEXT("pass") : TEXT("investigate"));
		}

		Report += TEXT("\n## Primary Scaling Metrics\n\n");
		Report += TEXT("| Resolution | Avg step kernel s | Avg step wall s | Paper Table 2 s | Contact s | Label s | Filter event s | Total replay s | Auth CAF before | Auth CAF after | Projected CAF before | Projected CAF after | Runtime |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FSlice5FixtureSummary& Summary : Summaries)
		{
			if (!Summary.bPrimaryScaling)
			{
				continue;
			}
			Report += FString::Printf(
				TEXT("| %d | %.6f | %.6f | %.2f | %.6f | %.6f | %.6f | %.6f | %.12f | %.12f | %.12f | %.12f | %s |\n"),
				Summary.SampleCount,
				Summary.AvgStepKernelSeconds,
				Summary.AvgStepWallSeconds,
				Summary.PaperBudgetSeconds,
				Summary.ContactA.ContactDetectionSeconds,
				Summary.LabelA.TriangleLabelSeconds,
				Summary.FilterA.ResampleEventSeconds,
				Summary.TotalReplaySeconds,
				Summary.FilterA.AuthoritativeCAFBefore,
				Summary.FilterA.AuthoritativeCAFAfter,
				Summary.FilterA.ProjectedCAFBefore,
				Summary.FilterA.ProjectedCAFAfter,
				*RuntimeVerdict(Summary));
		}

		Report += TEXT("\n## Primary Material Delta Breakdown\n\n");
		Report += TEXT("| Resolution | Net C delta | Single-hit loss/gain/net | Subduction loss/gain/net | Gap-fill loss/gain/net | Unresolved same loss/gain/net | Unresolved triple loss/gain/net | Unresolved mixed loss/gain/net | Gap-fill net % of net loss | Single-hit net % of net loss |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FSlice5FixtureSummary& Summary : Summaries)
		{
			if (!Summary.bPrimaryScaling)
			{
				continue;
			}
			const FCarrierLabPhaseIIMaterialLedgerMetrics& L = Summary.LedgerA;
			const double SingleNet = NetDelta(L.SingleHitTransferContinentalGain, L.SingleHitTransferContinentalLoss);
			const double SubductionNet = NetDelta(L.SubductionContinentalGain, L.SubductionContinentalLoss);
			const double GapNet = NetDelta(L.GapFillContinentalGain, L.GapFillContinentalLoss);
			const double SameNet = NetDelta(L.UnresolvedSameMaterialContinentalGain, L.UnresolvedSameMaterialContinentalLoss);
			const double TripleNet = NetDelta(L.UnresolvedTripleJunctionContinentalGain, L.UnresolvedTripleJunctionContinentalLoss);
			const double MixedNet = NetDelta(L.UnresolvedMixedMaterialContinentalGain, L.UnresolvedMixedMaterialContinentalLoss);
			Report += FString::Printf(
				TEXT("| %d | %.12f | %.6f / %.6f / %.6f | %.6f / %.6f / %.6f | %.6f / %.6f / %.6f | %.6f / %.6f / %.6f | %.6f / %.6f / %.6f | %.6f / %.6f / %.6f | %.2f%% | %.2f%% |\n"),
				Summary.SampleCount,
				L.LedgerContinentalDelta,
				L.SingleHitTransferContinentalLoss,
				L.SingleHitTransferContinentalGain,
				SingleNet,
				L.SubductionContinentalLoss,
				L.SubductionContinentalGain,
				SubductionNet,
				L.GapFillContinentalLoss,
				L.GapFillContinentalGain,
				GapNet,
				L.UnresolvedSameMaterialContinentalLoss,
				L.UnresolvedSameMaterialContinentalGain,
				SameNet,
				L.UnresolvedTripleJunctionContinentalLoss,
				L.UnresolvedTripleJunctionContinentalGain,
				TripleNet,
				L.UnresolvedMixedMaterialContinentalLoss,
				L.UnresolvedMixedMaterialContinentalGain,
				MixedNet,
				PercentOfNetLoss(GapNet, L.LedgerContinentalDelta),
				PercentOfNetLoss(SingleNet, L.LedgerContinentalDelta));
		}

		Report += TEXT("\n## Slice 5.5 Single-Hit Source Triangle Subdivision\n\n");
		Report += TEXT("This subdivision classifies the hit triangle used by each `single_hit_transfer` record. Uniform rows mean all three hit-triangle vertices share the same simplified material class; mixed rows are the only current evidence for interpolation across a carried material boundary.\n\n");
		Report += TEXT("| Resolution | Uniform continental count/net | Uniform oceanic count/net | Mixed triangle count/net | Unknown count/net |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|\n");
		for (const FSlice5FixtureSummary& Summary : Summaries)
		{
			if (!Summary.bPrimaryScaling)
			{
				continue;
			}
			const FCarrierLabPhaseIIMaterialLedgerMetrics& L = Summary.LedgerA;
			Report += FString::Printf(
				TEXT("| %d | %d / %.6f | %d / %.6f | %d / %.6f | %d / %.6f |\n"),
				Summary.SampleCount,
				L.SingleHitUniformContinentalRecordCount,
				NetDelta(L.SingleHitUniformContinentalGain, L.SingleHitUniformContinentalLoss),
				L.SingleHitUniformOceanicRecordCount,
				NetDelta(L.SingleHitUniformOceanicGain, L.SingleHitUniformOceanicLoss),
				L.SingleHitMixedTriangleRecordCount,
				NetDelta(L.SingleHitMixedTriangleGain, L.SingleHitMixedTriangleLoss),
				L.SingleHitUnknownTriangleRecordCount,
				NetDelta(L.SingleHitUnknownTriangleGain, L.SingleHitUnknownTriangleLoss));
		}

		Report += TEXT("\n## Primary Count Breakdown\n\n");
		Report += TEXT("| Resolution | Contacts | Third-plate contacts | Labels | Material records | Material changed | Plate changed | Subduction | Gap fill | Non-sep gap | Unresolved same | Unresolved triple | Unresolved mixed | C residual | Max plate residual | Ledger hash |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FSlice5FixtureSummary& Summary : Summaries)
		{
			if (!Summary.bPrimaryScaling)
			{
				continue;
			}
			const FCarrierLabPhaseIIMaterialLedgerMetrics& L = Summary.LedgerA;
			Report += FString::Printf(
				TEXT("| %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %.3e | %.3e | `%s` |\n"),
				Summary.SampleCount,
				Summary.ContactA.ContactRecordCount,
				Summary.ContactA.ThirdPlateContactCount,
				Summary.LabelA.LabelRecordCount,
				L.RecordCount,
				L.ChangedRecordCount,
				L.PlateChangedRecordCount,
				L.SubductionRecordCount,
				L.GapFillRecordCount,
				L.NonSeparatingGapFillRecordCount,
				L.UnresolvedSameMaterialRecordCount,
				L.UnresolvedTripleJunctionRecordCount,
				L.UnresolvedMixedMaterialRecordCount,
				L.ContinentalDeltaResidual,
				L.MaxPerPlateContinentalResidual,
				*L.MaterialLedgerHash);
		}

		Report += TEXT("\n## Notes\n\n");
		Report += TEXT("- Runtime `finding` means the average per-step projection kernel exceeded the paper Table 2 total for that resolution. It is reported as a scaling finding, not hidden by the gate labels.\n");
		Report += TEXT("- Gross loss/gain/net are reported together. Category priority should be based on net contribution and mechanism, not gross loss alone.\n");
		Report += TEXT("- Slice 5.5 provenance is observational only: hit-triangle uniformity is recorded after the resolver chooses a source triangle and does not influence resampling, filtering, labels, or carrier state.\n");
		Report += TEXT("- Material reconciliation uses `max(1e-12, total_area * 1e-12)` as the scaling gate. The residuals remain printed so floating-point summation noise cannot hide a real leak.\n");
		Report += TEXT("- Control fixtures are scaled with the target resolution in this commandlet so replay, label, filter, material, and no-material-change gates exercise the same sample density as the primary row.\n");
		Report += TEXT("- `metrics.jsonl` contains both replay rows for each fixture; the report tables use replay 0 after requiring replay 1 to match hashes and post-state.\n");

		bool bAllPass = true;
		bool bAnyRuntimeFinding = false;
		for (const FSlice5FixtureSummary& Summary : Summaries)
		{
			bAllPass &= SummaryPasses(Summary);
			bAnyRuntimeFinding |= Summary.bPrimaryScaling && !Summary.bRuntimeWithinPaperBudget;
		}
		Report += TEXT("\n## Recommendation\n\n");
		if (bAllPass)
		{
			Report += bAnyRuntimeFinding
				? TEXT("Slice 5 replay and material accounting gates pass, with runtime findings documented above. Pause for user review before selecting the next design slice.\n")
				: TEXT("Slice 5 replay, material accounting, and runtime gates pass. Pause for user review before selecting the next design slice.\n");
		}
		else
		{
			Report += TEXT("Pause before the next slice. One or more replay/material gates require investigation.\n");
		}
		return Report;
	}
}

UCarrierLabPhaseIISlice5Commandlet::UCarrierLabPhaseIISlice5Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIISlice5Commandlet::Main(const FString& Params)
{
	const int32 DefaultSteps = FMath::Max(1, ParseIntParam(Params, TEXT("Steps="), 32));
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	TArray<FSlice5FixtureConfig> Fixtures;
	FString ResolutionParam;
	if (FParse::Value(*Params, TEXT("Resolutions="), ResolutionParam))
	{
		TArray<FString> ResolutionTokens;
		ResolutionParam.ParseIntoArray(ResolutionTokens, TEXT(","), true);
		for (FString Token : ResolutionTokens)
		{
			Token.TrimStartAndEndInline();
			if (Token.Equals(TEXT("60k"), ESearchCase::IgnoreCase) || Token.Equals(TEXT("60000"), ESearchCase::IgnoreCase))
			{
				AddFixtureSuiteForResolution(Fixtures, TEXT("60k"), 60000, DefaultSteps);
			}
			else if (Token.Equals(TEXT("100k"), ESearchCase::IgnoreCase) || Token.Equals(TEXT("100000"), ESearchCase::IgnoreCase))
			{
				AddFixtureSuiteForResolution(Fixtures, TEXT("100k"), 100000, DefaultSteps);
			}
			else if (Token.Equals(TEXT("250k"), ESearchCase::IgnoreCase) || Token.Equals(TEXT("250000"), ESearchCase::IgnoreCase))
			{
				AddFixtureSuiteForResolution(Fixtures, TEXT("250k"), 250000, DefaultSteps);
			}
			else if (Token.Equals(TEXT("500k"), ESearchCase::IgnoreCase) || Token.Equals(TEXT("500000"), ESearchCase::IgnoreCase))
			{
				AddFixtureSuiteForResolution(Fixtures, TEXT("500k"), 500000, DefaultSteps);
			}
		}
	}
	else
	{
		AddFixtureSuiteForResolution(Fixtures, TEXT("60k"), 60000, DefaultSteps);
		AddFixtureSuiteForResolution(Fixtures, TEXT("100k"), 100000, DefaultSteps);
		AddFixtureSuiteForResolution(Fixtures, TEXT("250k"), 250000, DefaultSteps);
		AddFixtureSuiteForResolution(Fixtures, TEXT("500k"), 500000, DefaultSteps);
	}
	if (Fixtures.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab Phase II Slice 5: no valid resolution tokens in Resolutions=%s"), *ResolutionParam);
		return 2;
	}

	FString MetricsJsonl;
	TArray<FSlice5FixtureSummary> Summaries;
	bool bAllRunsCompleted = true;

	for (const FSlice5FixtureConfig& Fixture : Fixtures)
	{
		UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 5: fixture=%s samples=%d plates=%d steps=%d motion=%s"),
			*Fixture.Name,
			Fixture.SampleCount,
			Fixture.PlateCount,
			Fixture.StepCount,
			MotionFixtureName(Fixture.Fixture));

		FSlice5ReplayResult A;
		FSlice5ReplayResult B;
		bAllRunsCompleted &= RunFixtureReplay(Fixture, 0, A);
		bAllRunsCompleted &= RunFixtureReplay(Fixture, 1, B);
		MetricsJsonl += ReplayMetricJson(A) + LINE_TERMINATOR;
		MetricsJsonl += ReplayMetricJson(B) + LINE_TERMINATOR;
		Summaries.Add(SummarizeFixture(Fixture, A, B));
	}

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);
	const FString Report = BuildReport(OutputRoot, Summaries);
	FString ReportPath;
	if (!FParse::Value(*Params, TEXT("Report="), ReportPath))
	{
		ReportPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("docs"), TEXT("checkpoints"), TEXT("phase-ii-slice-5-report.md"));
	}
	else if (FPaths::IsRelative(ReportPath))
	{
		ReportPath = FPaths::Combine(FPaths::ProjectDir(), ReportPath);
	}
	ReportPath = FPaths::ConvertRelativePathToFull(ReportPath);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 5 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 5 report: %s"), *ReportPath);

	bool bAllSummariesPass = true;
	for (const FSlice5FixtureSummary& Summary : Summaries)
	{
		bAllSummariesPass &= SummaryPasses(Summary);
	}
	return (bAllRunsCompleted && bAllSummariesPass) ? 0 : 2;
}
