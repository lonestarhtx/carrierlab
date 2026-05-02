// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIISlice4Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
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

	const TCHAR* FixtureName(const ECarrierLabPhaseIIMotionFixture Fixture)
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

	const TCHAR* DecisionClassName(const ECarrierLabPhaseIIFilterDecisionClass DecisionClass)
	{
		switch (DecisionClass)
		{
		case ECarrierLabPhaseIIFilterDecisionClass::CandidateFiltered: return TEXT("candidate_filtered");
		case ECarrierLabPhaseIIFilterDecisionClass::GapFill: return TEXT("gap_fill");
		case ECarrierLabPhaseIIFilterDecisionClass::UnresolvedMultiHit: return TEXT("unresolved_multi_hit");
		case ECarrierLabPhaseIIFilterDecisionClass::FilterExhausted: return TEXT("filter_exhausted");
		case ECarrierLabPhaseIIFilterDecisionClass::ResolvedSingle:
		default: return TEXT("resolved_single");
		}
	}

	const TCHAR* MaterialClassName(const ECarrierLabPhaseIIMaterialEventClass EventClass)
	{
		switch (EventClass)
		{
		case ECarrierLabPhaseIIMaterialEventClass::SingleHitTransfer: return TEXT("single_hit_transfer");
		case ECarrierLabPhaseIIMaterialEventClass::ConsumedBySubduction: return TEXT("consumed_by_subduction");
		case ECarrierLabPhaseIIMaterialEventClass::OverwrittenByGapFill: return TEXT("overwritten_by_gap_fill");
		case ECarrierLabPhaseIIMaterialEventClass::UnresolvedSameMaterialMultiHit: return TEXT("unresolved_same_material_multi_hit");
		case ECarrierLabPhaseIIMaterialEventClass::UnresolvedTripleJunctionMultiHit: return TEXT("unresolved_triple_junction_multi_hit");
		case ECarrierLabPhaseIIMaterialEventClass::UnresolvedMixedMaterialMultiHit: return TEXT("unresolved_mixed_material_multi_hit");
		case ECarrierLabPhaseIIMaterialEventClass::FilterExhaustedUnknown: return TEXT("filter_exhausted_unknown");
		case ECarrierLabPhaseIIMaterialEventClass::NumericResidual: return TEXT("numeric_residual");
		case ECarrierLabPhaseIIMaterialEventClass::Preserved:
		default: return TEXT("preserved");
		}
	}

	const TCHAR* SourceTriangleUniformityName(const ECarrierLabPhaseIISourceTriangleUniformity Uniformity)
	{
		switch (Uniformity)
		{
		case ECarrierLabPhaseIISourceTriangleUniformity::UniformOceanic: return TEXT("uniform_oceanic");
		case ECarrierLabPhaseIISourceTriangleUniformity::UniformContinental: return TEXT("uniform_continental");
		case ECarrierLabPhaseIISourceTriangleUniformity::Mixed: return TEXT("mixed");
		case ECarrierLabPhaseIISourceTriangleUniformity::Unknown:
		default: return TEXT("unknown");
		}
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseII"), TEXT("Slice4"), Stamp);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	int32 ParseIntParam(const FString& Params, const TCHAR* Key, const int32 DefaultValue)
	{
		int32 Value = DefaultValue;
		FParse::Value(*Params, Key, Value);
		return Value;
	}

	struct FSlice4FixtureConfig
	{
		FString Name;
		int32 SampleCount = 10000;
		int32 PlateCount = 40;
		int32 StepCount = 32;
		double ContinentalPlateFraction = 0.30;
		ECarrierLabPhaseIIMotionFixture Fixture = ECarrierLabPhaseIIMotionFixture::Default;
		bool bUseFixturePolarity = false;
		int32 FixtureUnderPlate = INDEX_NONE;
		int32 FixtureOverPlate = INDEX_NONE;
		bool bExpectNoMaterialChange = false;
		bool bExpectSubductionRecords = false;
	};

	struct FSlice4ReplayResult
	{
		FString FixtureName;
		int32 Replay = 0;
		int32 StepCount = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterMetrics;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerMetrics;
		FCarrierLabVisualizationMetrics ProjectionMetrics;
		FString MaterialJsonl;
		FString CanonicalMaterialJsonl;
		bool bCompleted = false;
	};

	struct FSlice4FixtureSummary
	{
		FString FixtureName;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		bool bCompleted = false;
		bool bContactReplayHash = false;
		bool bLabelReplayHash = false;
		bool bFilterReplayHash = false;
		bool bMaterialReplayHash = false;
		bool bMaterialLogByteMatch = false;
		bool bPostStateHashMatch = false;
		bool bGlobalMaterialReconcile = false;
		bool bPerPlateMaterialReconcile = false;
		bool bNoHiddenMaterialChange = false;
		bool bControlStable = true;
		bool bExpectedSubductionRecords = true;
		FCarrierLabPhaseIIContactMetrics ContactA;
		FCarrierLabPhaseIITriangleLabelMetrics LabelA;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterA;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerA;
		FString Notes;
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

	ACarrierLabVisualizationActor* SpawnSlice4Actor(UWorld& World, const FSlice4FixtureConfig& Config)
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

	FString MaterialRecordJson(const FString& FixtureName, const int32 Replay, const FCarrierLabPhaseIIMaterialRecord& Record)
	{
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"record_id\":%d,\"event_id\":%d,\"step\":%d,\"sample_id\":%d,\"source_plate_id\":%d,\"target_plate_id\":%d,\"source_contact_id\":%d,\"source_label_id\":%d,\"hit_plate_id\":%d,\"hit_local_triangle_id\":%d,\"hit_triangle_continental_vertex_count\":%d,\"hit_triangle_uniformity\":%s,\"raw_plate_count\":%d,\"post_filter_plate_count\":%d,\"area_weight\":%.15f,\"continental_before\":%.15f,\"continental_after\":%.15f,\"continental_delta\":%.15f,\"oceanic_before\":%.15f,\"oceanic_after\":%.15f,\"oceanic_delta\":%.15f,\"material_changed\":%s,\"plate_changed\":%s,\"third_plate_evidence\":%s,\"non_separating_gap\":%s,\"event_class\":%s,\"decision_class\":%s}"),
			*JsonString(FixtureName),
			Replay,
			Record.RecordId,
			Record.EventId,
			Record.Step,
			Record.SampleId,
			Record.SourcePlateId,
			Record.TargetPlateId,
			Record.SourceContactId,
			Record.SourceLabelId,
			Record.HitPlateId,
			Record.HitLocalTriangleId,
			Record.HitTriangleContinentalVertexCount,
			*JsonString(SourceTriangleUniformityName(Record.HitTriangleUniformity)),
			Record.RawPlateCount,
			Record.PostFilterPlateCount,
			Record.AreaWeight,
			Record.ContinentalBefore,
			Record.ContinentalAfter,
			Record.ContinentalDelta,
			Record.OceanicBefore,
			Record.OceanicAfter,
			Record.OceanicDelta,
			Record.bMaterialChanged ? TEXT("true") : TEXT("false"),
			Record.bPlateChanged ? TEXT("true") : TEXT("false"),
			Record.bThirdPlateEvidence ? TEXT("true") : TEXT("false"),
			Record.bNonSeparatingGap ? TEXT("true") : TEXT("false"),
			*JsonString(MaterialClassName(Record.EventClass)),
			*JsonString(DecisionClassName(Record.DecisionClass)));
	}

	FString ReplayMetricJson(const FSlice4ReplayResult& Result)
	{
		const FCarrierLabPhaseIIContactMetrics& Contact = Result.ContactMetrics;
		const FCarrierLabPhaseIITriangleLabelMetrics& Label = Result.LabelMetrics;
		const FCarrierLabPhaseIIResamplingFilterMetrics& Filter = Result.FilterMetrics;
		const FCarrierLabPhaseIIMaterialLedgerMetrics& Ledger = Result.LedgerMetrics;
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"step_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"contact_hash\":%s,\"contact_record_count\":%d,\"label_hash\":%s,\"label_record_count\":%d,\"filter_decision_hash\":%s,\"material_ledger_hash\":%s,\"material_record_count\":%d,\"changed_record_count\":%d,\"plate_changed_record_count\":%d,\"single_hit_transfer_count\":%d,\"subduction_count\":%d,\"gap_fill_count\":%d,\"non_separating_gap_fill_count\":%d,\"unresolved_same_material_count\":%d,\"unresolved_triple_junction_count\":%d,\"unresolved_mixed_material_count\":%d,\"filter_exhausted_count\":%d,\"continental_mass_before\":%.15f,\"continental_mass_after\":%.15f,\"ledger_continental_delta\":%.15f,\"continental_delta_residual\":%.15f,\"max_per_plate_continental_residual\":%.15f,\"subduction_continental_loss\":%.15f,\"subduction_continental_gain\":%.15f,\"gap_fill_continental_loss\":%.15f,\"gap_fill_continental_gain\":%.15f,\"single_hit_transfer_continental_loss\":%.15f,\"single_hit_transfer_continental_gain\":%.15f,\"auth_caf_before\":%.12f,\"auth_caf_after\":%.12f,\"projected_caf_before\":%.12f,\"projected_caf_after\":%.12f,\"state_hash_after\":%s,\"memory_gb\":%.12f}"),
			*JsonString(Result.FixtureName),
			Result.Replay,
			Result.StepCount,
			Result.SampleCount,
			Result.PlateCount,
			*JsonString(Contact.ContactLogHash),
			Contact.ContactRecordCount,
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
			Ledger.SubductionContinentalLoss,
			Ledger.SubductionContinentalGain,
			Ledger.GapFillContinentalLoss,
			Ledger.GapFillContinentalGain,
			Ledger.SingleHitTransferContinentalLoss,
			Ledger.SingleHitTransferContinentalGain,
			Filter.AuthoritativeCAFBefore,
			Filter.AuthoritativeCAFAfter,
			Filter.ProjectedCAFBefore,
			Filter.ProjectedCAFAfter,
			*JsonString(Filter.StateHashAfter),
			MemoryGb);
	}

	bool RunFixtureReplay(const FSlice4FixtureConfig& Config, const int32 Replay, FSlice4ReplayResult& OutResult)
	{
		OutResult = FSlice4ReplayResult();
		OutResult.FixtureName = Config.Name;
		OutResult.Replay = Replay;
		OutResult.StepCount = Config.StepCount;
		OutResult.SampleCount = Config.SampleCount;
		OutResult.PlateCount = Config.PlateCount;

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 4 fixture %s replay %d could not find a world."), *Config.Name, Replay);
			return false;
		}

		ACarrierLabVisualizationActor* Actor = SpawnSlice4Actor(*World, Config);
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
			Actor->StepOnce();
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
		TArray<FCarrierLabPhaseIIMaterialRecord> Materials;
		if (!Actor->ApplyPhaseIIResamplingFilterEvent(Labels, Decisions, OutResult.FilterMetrics, &Materials, &OutResult.LedgerMetrics))
		{
			Actor->Destroy();
			return false;
		}
		OutResult.ProjectionMetrics = Actor->CurrentMetrics;

		for (const FCarrierLabPhaseIIMaterialRecord& Record : Materials)
		{
			OutResult.MaterialJsonl += MaterialRecordJson(Config.Name, Replay, Record) + LINE_TERMINATOR;
			OutResult.CanonicalMaterialJsonl += MaterialRecordJson(Config.Name, -1, Record) + LINE_TERMINATOR;
		}

		OutResult.bCompleted = true;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return true;
	}

	FSlice4FixtureSummary SummarizeFixture(const FSlice4FixtureConfig& Config, const FSlice4ReplayResult& A, const FSlice4ReplayResult& B)
	{
		FSlice4FixtureSummary Summary;
		Summary.FixtureName = Config.Name;
		Summary.SampleCount = Config.SampleCount;
		Summary.PlateCount = Config.PlateCount;
		Summary.StepCount = Config.StepCount;
		Summary.bCompleted = A.bCompleted && B.bCompleted;
		Summary.ContactA = A.ContactMetrics;
		Summary.LabelA = A.LabelMetrics;
		Summary.FilterA = A.FilterMetrics;
		Summary.LedgerA = A.LedgerMetrics;
		Summary.bContactReplayHash = A.bCompleted && B.bCompleted && A.ContactMetrics.ContactLogHash == B.ContactMetrics.ContactLogHash;
		Summary.bLabelReplayHash = A.bCompleted && B.bCompleted && A.LabelMetrics.TriangleLabelHash == B.LabelMetrics.TriangleLabelHash;
		Summary.bFilterReplayHash = A.bCompleted && B.bCompleted && A.FilterMetrics.FilterDecisionHash == B.FilterMetrics.FilterDecisionHash;
		Summary.bMaterialReplayHash = A.bCompleted && B.bCompleted && A.LedgerMetrics.MaterialLedgerHash == B.LedgerMetrics.MaterialLedgerHash;
		Summary.bMaterialLogByteMatch = A.bCompleted && B.bCompleted && A.CanonicalMaterialJsonl == B.CanonicalMaterialJsonl;
		Summary.bPostStateHashMatch = A.bCompleted && B.bCompleted && A.FilterMetrics.StateHashAfter == B.FilterMetrics.StateHashAfter;
		Summary.bGlobalMaterialReconcile =
			FMath::Abs(A.LedgerMetrics.ContinentalDeltaResidual) <= 1.0e-12 &&
			FMath::Abs(B.LedgerMetrics.ContinentalDeltaResidual) <= 1.0e-12 &&
			FMath::Abs(A.LedgerMetrics.OceanicDeltaResidual) <= 1.0e-12 &&
			FMath::Abs(B.LedgerMetrics.OceanicDeltaResidual) <= 1.0e-12;
		Summary.bPerPlateMaterialReconcile =
			FMath::Abs(A.LedgerMetrics.MaxPerPlateContinentalResidual) <= 1.0e-12 &&
			FMath::Abs(B.LedgerMetrics.MaxPerPlateContinentalResidual) <= 1.0e-12;
		Summary.bNoHiddenMaterialChange = Summary.bGlobalMaterialReconcile && Summary.bPerPlateMaterialReconcile;
		Summary.bControlStable = !Config.bExpectNoMaterialChange ||
			(A.LedgerMetrics.ChangedRecordCount == 0 && B.LedgerMetrics.ChangedRecordCount == 0 &&
				FMath::Abs(A.LedgerMetrics.ContinentalMassAfter - A.LedgerMetrics.ContinentalMassBefore) <= 1.0e-12 &&
				FMath::Abs(B.LedgerMetrics.ContinentalMassAfter - B.LedgerMetrics.ContinentalMassBefore) <= 1.0e-12);
		Summary.bExpectedSubductionRecords = !Config.bExpectSubductionRecords ||
			(A.LedgerMetrics.SubductionRecordCount > 0 && B.LedgerMetrics.SubductionRecordCount > 0);

		if (Config.Name == TEXT("cadence_60k_primary"))
		{
			Summary.Notes = TEXT("Primary attribution run: remaining CAF delta is now named by ledger categories, not hidden in aggregate CAF.");
		}
		else if (Config.bExpectNoMaterialChange)
		{
			Summary.Notes = TEXT("No-material-change control.");
		}
		else if (Config.bExpectSubductionRecords)
		{
			Summary.Notes = TEXT("Fixture polarity enables subduction records; ledger must reconcile the resulting transfer.");
		}
		return Summary;
	}

	bool SummaryPasses(const FSlice4FixtureSummary& Summary)
	{
		return Summary.bCompleted &&
			Summary.bContactReplayHash &&
			Summary.bLabelReplayHash &&
			Summary.bFilterReplayHash &&
			Summary.bMaterialReplayHash &&
			Summary.bMaterialLogByteMatch &&
			Summary.bPostStateHashMatch &&
			Summary.bGlobalMaterialReconcile &&
			Summary.bPerPlateMaterialReconcile &&
			Summary.bNoHiddenMaterialChange &&
			Summary.bControlStable &&
			Summary.bExpectedSubductionRecords;
	}

	FString BuildReport(const FString& OutputRoot, const TArray<FSlice4FixtureSummary>& Summaries)
	{
		FString Report = TEXT("# Phase II Slice 4 Checkpoint: Material Accounting\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This checkpoint adds a material ledger to the Slice 3 resampling filter event. It does not change carrier behavior, add terrain processes, or make unresolved contacts resolve. The goal is attribution: every authoritative continental/oceanic mass delta must reconcile to named records.\n\n");
		Report += TEXT("Audit equation: `active_after = active_before + single_hit_transfer + consumed_by_subduction + overwritten_by_gap_fill + unresolved_same_material + unresolved_triple_junction + unresolved_mixed_material + filter_exhausted_unknown + numeric_residual`.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Fixture | Samples | Plates | Steps | Contact replay | Label replay | Filter replay | Material replay | Material log | Post-state replay | Global reconcile | Per-plate reconcile | No hidden change | Control stable | Expected subduction | Verdict |\n");
		Report += TEXT("|---|---:|---:|---:|---|---|---|---|---|---|---|---|---|---|---|---|\n");
		for (const FSlice4FixtureSummary& Summary : Summaries)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |\n"),
				*Summary.FixtureName,
				Summary.SampleCount,
				Summary.PlateCount,
				Summary.StepCount,
				Summary.bContactReplayHash ? TEXT("pass") : TEXT("fail"),
				Summary.bLabelReplayHash ? TEXT("pass") : TEXT("fail"),
				Summary.bFilterReplayHash ? TEXT("pass") : TEXT("fail"),
				Summary.bMaterialReplayHash ? TEXT("pass") : TEXT("fail"),
				Summary.bMaterialLogByteMatch ? TEXT("pass") : TEXT("fail"),
				Summary.bPostStateHashMatch ? TEXT("pass") : TEXT("fail"),
				Summary.bGlobalMaterialReconcile ? TEXT("pass") : TEXT("fail"),
				Summary.bPerPlateMaterialReconcile ? TEXT("pass") : TEXT("fail"),
				Summary.bNoHiddenMaterialChange ? TEXT("pass") : TEXT("fail"),
				Summary.bControlStable ? TEXT("pass") : TEXT("fail"),
				Summary.bExpectedSubductionRecords ? TEXT("pass") : TEXT("fail"),
				SummaryPasses(Summary) ? TEXT("pass") : TEXT("investigate"));
		}

		Report += TEXT("\n## Material Ledger Metrics\n\n");
		Report += TEXT("| Fixture | Records | Material changed | Plate changed | Single-hit | Subduction | Gap fill | Non-sep gap | Unresolved same | Unresolved triple | Unresolved mixed | Filter exhausted | C before | C after | Ledger C delta | C residual | Max plate residual | Subduction loss | Gap-fill loss | Single-hit loss | Auth CAF before | Auth CAF after | Ledger hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FSlice4FixtureSummary& Summary : Summaries)
		{
			const FCarrierLabPhaseIIMaterialLedgerMetrics& Ledger = Summary.LedgerA;
			const FCarrierLabPhaseIIResamplingFilterMetrics& Filter = Summary.FilterA;
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %.12f | %.12f | %.12f | %.15f | %.15f | %.12f | %.12f | %.12f | %.12f | %.12f | `%s` |\n"),
				*Summary.FixtureName,
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
				Ledger.SubductionContinentalLoss,
				Ledger.GapFillContinentalLoss,
				Ledger.SingleHitTransferContinentalLoss,
				Filter.AuthoritativeCAFBefore,
				Filter.AuthoritativeCAFAfter,
				*Ledger.MaterialLedgerHash);
		}

		Report += TEXT("\n## Notes\n\n");
		for (const FSlice4FixtureSummary& Summary : Summaries)
		{
			if (!Summary.Notes.IsEmpty())
			{
				Report += FString::Printf(TEXT("- `%s`: %s\n"), *Summary.FixtureName, *Summary.Notes);
			}
		}
		Report += TEXT("- Slice 4 is an accounting slice. A CAF change is allowed only when the ledger names and reconciles it; it is not treated as proof that material conservation is solved.\n");
		Report += TEXT("- `material_ledger.jsonl` is the replay-sufficient material attribution artifact. The ledger hash is included in `metrics.jsonl` and the resampling filter metrics.\n");
		Report += TEXT("- The primary 60k run should now be read as a destruction-source breakdown: subduction filtering, q1/q2 gap fill, single-hit transfer, and unresolved classes are separate categories.\n");

		Report += TEXT("\n## Recommendation\n\n");
		bool bAllPass = true;
		for (const FSlice4FixtureSummary& Summary : Summaries)
		{
			bAllPass &= SummaryPasses(Summary);
		}
		Report += bAllPass
			? TEXT("Slice 4 material accounting gates pass. Pause for user review before Slice 5 scaling.\n")
			: TEXT("Pause before Slice 5. One or more material accounting gates require investigation.\n");
		return Report;
	}
}

UCarrierLabPhaseIISlice4Commandlet::UCarrierLabPhaseIISlice4Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIISlice4Commandlet::Main(const FString& Params)
{
	const int32 DefaultSteps = FMath::Max(1, ParseIntParam(Params, TEXT("Steps="), 32));
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	const TArray<FSlice4FixtureConfig> Fixtures = {
		{TEXT("cadence_60k_primary"), 60000, 40, DefaultSteps, 0.30, ECarrierLabPhaseIIMotionFixture::Default, false, INDEX_NONE, INDEX_NONE, false, true},
		{TEXT("forced_convergence_under_1"), 10000, 2, 40, 1.00, ECarrierLabPhaseIIMotionFixture::ForcedConvergence, true, 1, 0, false, true},
		{TEXT("forced_convergence_under_0"), 10000, 2, 40, 1.00, ECarrierLabPhaseIIMotionFixture::ForcedConvergence, true, 0, 1, false, true},
		{TEXT("forced_divergence_step0"), 10000, 2, 0, 0.50, ECarrierLabPhaseIIMotionFixture::ForcedDivergence, false, INDEX_NONE, INDEX_NONE, true, false},
		{TEXT("same_pair_mixed_signal"), 10000, 2, 40, 1.00, ECarrierLabPhaseIIMotionFixture::ForcedDivergence, true, 1, 0, false, true},
		{TEXT("zero_motion"), 10000, 40, DefaultSteps, 0.30, ECarrierLabPhaseIIMotionFixture::Zero, false, INDEX_NONE, INDEX_NONE, true, false},
		{TEXT("single_plate"), 10000, 1, DefaultSteps, 0.30, ECarrierLabPhaseIIMotionFixture::Default, false, INDEX_NONE, INDEX_NONE, true, false},
		{TEXT("all_continental_zero_motion"), 10000, 40, DefaultSteps, 1.00, ECarrierLabPhaseIIMotionFixture::Zero, false, INDEX_NONE, INDEX_NONE, true, false},
		{TEXT("ocean_only_zero_motion"), 10000, 40, DefaultSteps, 0.00, ECarrierLabPhaseIIMotionFixture::Zero, false, INDEX_NONE, INDEX_NONE, true, false},
		{TEXT("ocean_only_forced_convergence_under_1"), 10000, 2, 40, 0.00, ECarrierLabPhaseIIMotionFixture::ForcedConvergence, true, 1, 0, true, true}
	};

	TArray<FSlice4ReplayResult> ReplayResults;
	TArray<FSlice4FixtureSummary> Summaries;
	FString MetricsJsonl;
	FString MaterialJsonl;
	bool bAllRunsCompleted = true;
	for (const FSlice4FixtureConfig& Fixture : Fixtures)
	{
		UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 4: fixture=%s samples=%d plates=%d steps=%d fixture_motion=%s"),
			*Fixture.Name,
			Fixture.SampleCount,
			Fixture.PlateCount,
			Fixture.StepCount,
			FixtureName(Fixture.Fixture));

		FSlice4ReplayResult A;
		FSlice4ReplayResult B;
		bAllRunsCompleted &= RunFixtureReplay(Fixture, 0, A);
		bAllRunsCompleted &= RunFixtureReplay(Fixture, 1, B);
		ReplayResults.Add(A);
		ReplayResults.Add(B);
		MetricsJsonl += ReplayMetricJson(A) + LINE_TERMINATOR;
		MetricsJsonl += ReplayMetricJson(B) + LINE_TERMINATOR;
		MaterialJsonl += A.MaterialJsonl;
		MaterialJsonl += B.MaterialJsonl;
		Summaries.Add(SummarizeFixture(Fixture, A, B));
	}

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	const FString MaterialPath = FPaths::Combine(OutputRoot, TEXT("material_ledger.jsonl"));
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);
	FFileHelper::SaveStringToFile(MaterialJsonl, *MaterialPath);
	const FString Report = BuildReport(OutputRoot, Summaries);
	const FString ReportPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("docs"), TEXT("checkpoints"), TEXT("phase-ii-slice-4-report.md"));
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 4 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 4 material ledger: %s"), *MaterialPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 4 report: %s"), *ReportPath);

	bool bAllSummariesPass = true;
	for (const FSlice4FixtureSummary& Summary : Summaries)
	{
		bAllSummariesPass &= SummaryPasses(Summary);
	}
	return (bAllRunsCompleted && bAllSummariesPass) ? 0 : 2;
}
