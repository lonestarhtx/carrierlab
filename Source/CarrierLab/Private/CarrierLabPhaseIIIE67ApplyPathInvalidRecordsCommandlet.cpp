// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIE67ApplyPathInvalidRecordsCommandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr int32 DefaultSampleCount = 100000;
	constexpr int32 DefaultPlateCount = 40;
	constexpr int32 DefaultSeed = 42;
	constexpr double DefaultVelocityMmPerYear = 66.6666666667;

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

	FString BoolText(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	FString PassFail(const bool bPass)
	{
		return bPass ? TEXT("pass") : TEXT("fail");
	}

	FString SelectionClassName(const ECarrierLabPhaseIIIE3SelectionClass Value)
	{
		switch (Value)
		{
		case ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap:
			return TEXT("no_hit_divergent_gap");
		case ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit:
			return TEXT("resolved_single_hit");
		case ECarrierLabPhaseIIIE3SelectionClass::DivergentGapAfterFiltering:
			return TEXT("divergent_gap_after_filtering");
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit:
			return TEXT("unresolved_same_material_multi_hit");
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedMixedMaterialMultiHit:
			return TEXT("unresolved_mixed_material_multi_hit");
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedThirdPlateMultiHit:
			return TEXT("unresolved_third_plate_multi_hit");
		default:
			return TEXT("unknown");
		}
	}

	FString BucketName(const ECarrierLabPhaseIIIE3MultiHitBucket Value)
	{
		switch (Value)
		{
		case ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateCoincident:
			return TEXT("within_plate_coincident");
		case ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateDistanceSeparated:
			return TEXT("within_plate_distance_separated");
		case ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual:
			return TEXT("cross_plate_equal");
		case ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent:
			return TEXT("cross_plate_different");
		case ECarrierLabPhaseIIIE3MultiHitBucket::MixedMaterial:
			return TEXT("mixed_material");
		case ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate:
			return TEXT("third_plate");
		case ECarrierLabPhaseIIIE3MultiHitBucket::None:
		default:
			return TEXT("none");
		}
	}

	FString ReasonName(const ECarrierLabPhaseIIIE67InvalidRecordReason Value)
	{
		switch (Value)
		{
		case ECarrierLabPhaseIIIE67InvalidRecordReason::InvalidSampleIndex:
			return TEXT("invalid_sample_index");
		case ECarrierLabPhaseIIIE67InvalidRecordReason::InvalidSampleUnitPosition:
			return TEXT("invalid_sample_unit_position");
		case ECarrierLabPhaseIIIE67InvalidRecordReason::SampleFieldOutOfRange:
			return TEXT("sample_field_out_of_range");
		case ECarrierLabPhaseIIIE67InvalidRecordReason::ResolvedHitInvalidPlate:
			return TEXT("resolved_hit_invalid_plate");
		case ECarrierLabPhaseIIIE67InvalidRecordReason::DivergentGapNoBoundaryPair:
			return TEXT("divergent_gap_no_boundary_pair");
		case ECarrierLabPhaseIIIE67InvalidRecordReason::DivergentGapNonSeparating:
			return TEXT("divergent_gap_nonseparating");
		case ECarrierLabPhaseIIIE67InvalidRecordReason::DivergentGapGenerationOtherFailure:
			return TEXT("divergent_gap_generation_other_failure");
		case ECarrierLabPhaseIIIE67InvalidRecordReason::GeneratedGapInvalidAssignedPlate:
			return TEXT("generated_gap_invalid_assigned_plate");
		case ECarrierLabPhaseIIIE67InvalidRecordReason::UnhandledSelectionClass:
			return TEXT("unhandled_selection_class");
		case ECarrierLabPhaseIIIE67InvalidRecordReason::None:
		default:
			return TEXT("none");
		}
	}

	int32 PrimaryReasonSum(const FCarrierLabPhaseIIIE67ApplyPathInvalidRecordsAudit& Audit)
	{
		return
			Audit.InvalidSampleIndexCount +
			Audit.ResolvedHitInvalidPlateCount +
			Audit.DivergentGapNoBoundaryPairCount +
			Audit.DivergentGapNonSeparatingCount +
			Audit.DivergentGapGenerationOtherFailureCount +
			Audit.GeneratedGapInvalidAssignedPlateCount +
			Audit.UnhandledSelectionClassCount;
	}

	ACarrierLabVisualizationActor* SpawnDefaultActor(UWorld& World)
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
		Actor->SampleCount = DefaultSampleCount;
		Actor->PlateCount = DefaultPlateCount;
		Actor->Seed = DefaultSeed;
		Actor->ContinentalPlateFraction = 0.0;
		Actor->VelocityMmPerYear = DefaultVelocityMmPerYear;
		Actor->bEnableNaturalResamplingEvents = false;
		Actor->bEnablePhaseIIIE3DuplicateHitCoalescing = true;
		Actor->bEnablePhaseIIIE3SharedBoundaryTieBreak = true;
		Actor->bEnablePhaseIIIE3NearestHitTieBreak = true;
		Actor->bEnablePhaseIIIE3DistanceTieFallback = true;
		Actor->ConfigurePhaseIIICProcessLayer(true, false);
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	struct FScenarioResult
	{
		FString Name;
		int32 TargetStep = 0;
		bool bRestoreNonSeparatingAnomalyVeto = false;
		bool bRan = false;
		bool bPass = false;
		double WarmupSeconds = 0.0;
		double TotalSeconds = 0.0;
		FCarrierLabPhaseIIIE67ApplyPathInvalidRecordsAudit Audit;
	};

	bool RunScenario(
		UWorld& World,
		const FString& Name,
		const int32 TargetStep,
		const bool bRestoreNonSeparatingAnomalyVeto,
		FScenarioResult& OutResult)
	{
		OutResult = FScenarioResult();
		OutResult.Name = Name;
		OutResult.TargetStep = TargetStep;
		OutResult.bRestoreNonSeparatingAnomalyVeto = bRestoreNonSeparatingAnomalyVeto;
		UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIIE67ApplyPathInvalidRecords: scenario start %s step=%d"), *Name, TargetStep);
		const double ScenarioStartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnDefaultActor(World);
		if (Actor == nullptr || !Actor->InitializeCarrier())
		{
			if (Actor != nullptr)
			{
				Actor->Destroy();
			}
			return false;
		}
		Actor->bRestoreNonSeparatingAnomalyVeto = bRestoreNonSeparatingAnomalyVeto;

		const double WarmupStartSeconds = FPlatformTime::Seconds();
		for (int32 StepIndex = 0; StepIndex < TargetStep; ++StepIndex)
		{
			Actor->StepOnce();
		}
		OutResult.WarmupSeconds = FPlatformTime::Seconds() - WarmupStartSeconds;
		OutResult.bRan = Actor->RunPhaseIIIE67ApplyPathInvalidRecordsDiagnosisAudit(OutResult.Audit);
		OutResult.TotalSeconds = FPlatformTime::Seconds() - ScenarioStartSeconds;
		OutResult.bPass =
			OutResult.bRan &&
			OutResult.Audit.SelectionUnresolvedMultiHitCount == 0 &&
			PrimaryReasonSum(OutResult.Audit) == OutResult.Audit.InvalidRecordCount;

		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		UE_LOG(
			LogTemp,
			Display,
			TEXT("CarrierLabPhaseIIIE67ApplyPathInvalidRecords: scenario done %s pass=%d restore_veto=%d invalid=%d no_boundary=%d nonsep=%d gen=%d nonpos_gen=%d applied=%d rift=%d select=%.3fs record=%.3fs query=%.3fs total=%.3fs"),
			*Name,
			OutResult.bPass ? 1 : 0,
			bRestoreNonSeparatingAnomalyVeto ? 1 : 0,
			OutResult.Audit.InvalidRecordCount,
			OutResult.Audit.DivergentGapNoBoundaryPairCount,
			OutResult.Audit.DivergentGapNonSeparatingCount,
			OutResult.Audit.GeneratedCandidateCount,
			OutResult.Audit.GeneratedWithNonPositiveSeparationCount,
			OutResult.Audit.AppliedGeneratedCount,
			OutResult.Audit.RiftingPendingCount,
			OutResult.Audit.SelectionSeconds,
			OutResult.Audit.RecordBuildSeconds,
			OutResult.Audit.DivergentQuerySeconds,
			OutResult.TotalSeconds);
		return OutResult.bRan;
	}

	bool ReplayMatches(const FScenarioResult& A, const FScenarioResult& B)
	{
		return A.bRan &&
			B.bRan &&
			A.Audit.SelectionHash == B.Audit.SelectionHash &&
			A.Audit.DiagnosisHash == B.Audit.DiagnosisHash &&
			A.Audit.InvalidRecordCount == B.Audit.InvalidRecordCount &&
			A.Audit.DivergentGapNoBoundaryPairCount == B.Audit.DivergentGapNoBoundaryPairCount &&
			A.Audit.DivergentGapNonSeparatingCount == B.Audit.DivergentGapNonSeparatingCount &&
			A.Audit.GeneratedWithNonPositiveSeparationCount == B.Audit.GeneratedWithNonPositiveSeparationCount &&
			A.Audit.NonPositiveSeparationSpatialHash == B.Audit.NonPositiveSeparationSpatialHash;
	}

	FString InvalidRecordJsonLine(const FString& ScenarioName, const FCarrierLabPhaseIIIE67InvalidRecord& Record)
	{
		return FString::Printf(
			TEXT("{\"type\":\"invalid_record\",\"scenario\":%s,\"sample_id\":%d,\"step\":%d,\"reason\":%s,\"selection_class\":%s,\"bucket\":%s,\"resolved_plate_id\":%d,\"oceanic_assigned_plate_id\":%d,\"raw_candidates\":%d,\"post_filter_candidates\":%d,\"filtered_subducting\":%d,\"filtered_obduction\":%d,\"filtered_collision\":%d,\"boundary_pair_found\":%s,\"nonseparating\":%s,\"generated_nonpositive_separation\":%s,\"generated_oceanic\":%s,\"signed_separation_velocity\":%.17g,\"sample_unit_valid\":%s,\"sample_fields_valid\":%s,\"continental_fraction\":%.17g,\"elevation\":%.17g,\"historical_elevation\":%.17g,\"oceanic_age\":%.17g,\"latitude\":%.17g,\"longitude\":%.17g,\"spatial_lon_bin\":%d,\"spatial_lat_bin\":%d}"),
			*JsonString(ScenarioName),
			Record.SampleId,
			Record.Step,
			*JsonString(ReasonName(Record.Reason)),
			*JsonString(SelectionClassName(Record.SelectionClass)),
			*JsonString(BucketName(Record.MultiHitBucket)),
			Record.ResolvedPlateId,
			Record.OceanicAssignedPlateId,
			Record.RawCandidateCount,
			Record.PostFilterCandidateCount,
			Record.FilteredSubductingCount,
			Record.FilteredObductionPendingCount,
			Record.FilteredCollisionPendingCount,
			*BoolText(Record.bBoundaryPairFound),
			*BoolText(Record.bNonSeparatingAnomaly),
			*BoolText(Record.bGeneratedWithNonPositiveSeparation),
			*BoolText(Record.bGeneratedOceanicCrust),
			Record.SignedSeparationVelocity,
			*BoolText(Record.bSampleUnitValid),
			*BoolText(Record.bSampleFieldsValid),
			Record.ContinentalFraction,
			Record.Elevation,
			Record.HistoricalElevation,
			Record.OceanicAge,
			Record.LatitudeDegrees,
			Record.LongitudeDegrees,
			Record.SpatialLonBin,
			Record.SpatialLatBin);
	}

	FString ScenarioJsonLine(const FScenarioResult& Result)
	{
		const FCarrierLabPhaseIIIE67ApplyPathInvalidRecordsAudit& A = Result.Audit;
		return FString::Printf(
			TEXT("{\"type\":\"scenario\",\"name\":%s,\"pass\":%s,\"restore_nonseparating_veto\":%s,\"target_step\":%d,\"sample_count\":%d,\"plate_count\":%d,\"selection_resolved\":%d,\"selection_gaps\":%d,\"selection_unresolved\":%d,\"invalid_records\":%d,\"reason_sum\":%d,\"invalid_sample_index\":%d,\"invalid_unit_anomaly\":%d,\"field_anomaly\":%d,\"resolved_invalid_plate\":%d,\"no_boundary_pair\":%d,\"nonseparating\":%d,\"generation_other_failure\":%d,\"invalid_assigned_plate\":%d,\"unhandled_selection\":%d,\"generated\":%d,\"generated_nonpositive_separation\":%d,\"nonpositive_separation_min_abs\":%.12g,\"nonpositive_separation_median_abs\":%.12g,\"nonpositive_separation_max_abs\":%.12g,\"nonpositive_separation_spatial_hash\":%s,\"applied_generated\":%d,\"rifting_pending\":%d,\"invalid_nohit_gap\":%d,\"invalid_filter_exhausted_gap\":%d,\"invalid_resolved_single\":%d,\"invalid_unhandled_selection_class\":%d,\"invalid_any_process_filter\":%d,\"invalid_subducting_filter\":%d,\"invalid_obduction_filter\":%d,\"invalid_collision_filter\":%d,\"rifting_any_process_filter\":%d,\"warmup_seconds\":%.6f,\"total_seconds\":%.6f,\"selection_seconds\":%.6f,\"record_build_seconds\":%.6f,\"divergent_query_seconds\":%.6f,\"resolved_record_seconds\":%.6f,\"validation_seconds\":%.6f,\"selection_hash\":%s,\"diagnosis_hash\":%s}"),
			*JsonString(Result.Name),
			*BoolText(Result.bPass),
			*BoolText(Result.bRestoreNonSeparatingAnomalyVeto),
			Result.TargetStep,
			A.SampleCount,
			A.PlateCount,
			A.SelectionResolvedSingleHitCount,
			A.SelectionDivergentGapRouteCount,
			A.SelectionUnresolvedMultiHitCount,
			A.InvalidRecordCount,
			PrimaryReasonSum(A),
			A.InvalidSampleIndexCount,
			A.InvalidSampleUnitPositionCount,
			A.SampleFieldOutOfRangeCount,
			A.ResolvedHitInvalidPlateCount,
			A.DivergentGapNoBoundaryPairCount,
			A.DivergentGapNonSeparatingCount,
			A.DivergentGapGenerationOtherFailureCount,
			A.GeneratedGapInvalidAssignedPlateCount,
			A.UnhandledSelectionClassCount,
			A.GeneratedCandidateCount,
			A.GeneratedWithNonPositiveSeparationCount,
			A.NonPositiveSeparationMinMagnitude,
			A.NonPositiveSeparationMedianMagnitude,
			A.NonPositiveSeparationMaxMagnitude,
			*JsonString(A.NonPositiveSeparationSpatialHash),
			A.AppliedGeneratedCount,
			A.RiftingPendingCount,
			A.InvalidNoHitDivergentGapCount,
			A.InvalidFilterExhaustedDivergentGapCount,
			A.InvalidResolvedSingleHitCount,
			A.InvalidUnhandledSelectionCount,
			A.InvalidWithAnyProcessFilterCount,
			A.InvalidWithSubductingFilterCount,
			A.InvalidWithObductionFilterCount,
			A.InvalidWithCollisionFilterCount,
			A.RiftingPendingWithAnyProcessFilterCount,
			Result.WarmupSeconds,
			Result.TotalSeconds,
			A.SelectionSeconds,
			A.RecordBuildSeconds,
			A.DivergentQuerySeconds,
			A.ResolvedRecordSeconds,
			A.ValidationSeconds,
			*JsonString(A.SelectionHash),
			*JsonString(A.DiagnosisHash));
	}

	struct FSpatialBinSummary
	{
		int32 Count = 0;
		int32 LonBin = 0;
		int32 LatBin = 0;
	};

	TArray<FSpatialBinSummary> GetTopSpatialBins(const FCarrierLabPhaseIIIE67ApplyPathInvalidRecordsAudit& Audit, const int32 MaxBins)
	{
		TArray<FSpatialBinSummary> Bins;
		for (int32 Index = 0; Index < Audit.SpatialInvalidCounts.Num(); ++Index)
		{
			const int32 Count = Audit.SpatialInvalidCounts[Index];
			if (Count <= 0)
			{
				continue;
			}
			FSpatialBinSummary& Bin = Bins.AddDefaulted_GetRef();
			Bin.Count = Count;
			Bin.LonBin = Index % 12;
			Bin.LatBin = Index / 12;
		}
		Bins.Sort([](const FSpatialBinSummary& A, const FSpatialBinSummary& B)
		{
			return A.Count > B.Count;
		});
		if (Bins.Num() > MaxBins)
		{
			Bins.SetNum(MaxBins);
		}
		return Bins;
	}

	FString SpatialBinLabel(const FSpatialBinSummary& Bin)
	{
		const int32 LonMin = -180 + Bin.LonBin * 30;
		const int32 LonMax = LonMin + 30;
		const int32 LatMin = -90 + Bin.LatBin * 30;
		const int32 LatMax = LatMin + 30;
		return FString::Printf(TEXT("lon %d..%d, lat %d..%d"), LonMin, LonMax, LatMin, LatMax);
	}

	FString BuildReport(const TArray<FScenarioResult>& Results, const bool bReplayPass, const FString& MetricsPath)
	{
		bool bAllScenariosPass = true;
		for (const FScenarioResult& Result : Results)
		{
			bAllScenariosPass = bAllScenariosPass && Result.bPass;
		}
		const bool bPass = bAllScenariosPass && bReplayPass;

		FString Report;
		Report += TEXT("# Phase IIIE.6.7 / IIIE.6.9 Apply-Path Invalid Records Diagnosis\n\n");
		Report += FString::Printf(
			TEXT("**Verdict:** %s / PAPER-LITERAL ZERO-HIT CHECK. IIIE.6.6 closes selection multi-hit holds; IIIE.6.9 demotes the CarrierLab-invented non-positive-separation veto to an opt-in historical baseline and records generated non-positive-separation samples diagnostically.\n\n"),
			bPass ? TEXT("PASS") : TEXT("FAIL"));

		Report += TEXT("## Exact Validation Check\n\n");
		Report += TEXT("The live apply path still increments `InvalidRecordCount` for true invalid vertex records and holds here. Under default IIIE.6.9 behavior, non-positive signed separation is no longer one of those invalid reasons; the restored-veto scenario below intentionally re-enables the old IIIE.4 hold for baseline reproducibility.\n\n");
		Report += TEXT("```cpp\n");
		Report += TEXT("CurrentMetrics.PhaseIIIELastInvalidRecordCount = InvalidRecordCount;\n");
		Report += TEXT("if (InvalidRecordCount > 0)\n");
		Report += TEXT("{\n");
		Report += TEXT("    CurrentMetrics.LastRemeshMode = FString::Printf(\n");
		Report += TEXT("        TEXT(\"phase_iii_e6_live_hold_invalid_records_%d_gen_%d_applied_%d_rift_%d_no_boundary_%d_nonsep_%d\"),\n");
		Report += TEXT("        InvalidRecordCount,\n");
		Report += TEXT("        GeneratedCandidateCount,\n");
		Report += TEXT("        AppliedGeneratedCount,\n");
		Report += TEXT("        RiftingPendingCount,\n");
		Report += TEXT("        NoBoundaryPairMissCount,\n");
		Report += TEXT("        NonSeparatingGapFillCount);\n");
		Report += TEXT("    CurrentMetrics.ResampleEventSeconds = FPlatformTime::Seconds() - StartSeconds;\n");
		Report += TEXT("    RebuildRenderMesh();\n");
		Report += TEXT("    return false;\n");
		Report += TEXT("}\n");
		Report += TEXT("```\n\n");
		Report += TEXT("Primary invalid reasons below correspond to the individual `++InvalidRecordCount; continue;` sites before that hold. Sample unit/field columns are diagnostic anomaly flags among those invalid records. `Generated with non-positive separation` is an observability counter, not an invalid-record reason under default behavior.\n\n");

		Report += TEXT("## Scenario Summary\n\n");
		Report += TEXT("| Scenario | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		for (const FScenarioResult& Result : Results)
		{
			const FCarrierLabPhaseIIIE67ApplyPathInvalidRecordsAudit& A = Result.Audit;
			Report += FString::Printf(
				TEXT("| %s | %s | step `%d`, restore-veto `%d`, selection resolved/gap/unresolved `%d/%d/%d`, invalid `%d`, primary sum `%d`, gen/nonpos/applied/rift `%d/%d/%d/%d`, noBoundary/nonsep/other `%d/%d/%d`, invalid assigned `%d`, unhandled `%d`, process any/sub/obd/coll `%d/%d/%d/%d`, nonpos min/median/max `%g/%g/%g`, spatial `%s`, hashes `%s/%s` |\n"),
				*Result.Name,
				*PassFail(Result.bPass),
				A.Step,
				Result.bRestoreNonSeparatingAnomalyVeto ? 1 : 0,
				A.SelectionResolvedSingleHitCount,
				A.SelectionDivergentGapRouteCount,
				A.SelectionUnresolvedMultiHitCount,
				A.InvalidRecordCount,
				PrimaryReasonSum(A),
				A.GeneratedCandidateCount,
				A.GeneratedWithNonPositiveSeparationCount,
				A.AppliedGeneratedCount,
				A.RiftingPendingCount,
				A.DivergentGapNoBoundaryPairCount,
				A.DivergentGapNonSeparatingCount,
				A.DivergentGapGenerationOtherFailureCount,
				A.GeneratedGapInvalidAssignedPlateCount,
				A.UnhandledSelectionClassCount,
				A.InvalidWithAnyProcessFilterCount,
				A.InvalidWithSubductingFilterCount,
				A.InvalidWithObductionFilterCount,
				A.InvalidWithCollisionFilterCount,
				A.NonPositiveSeparationMinMagnitude,
				A.NonPositiveSeparationMedianMagnitude,
				A.NonPositiveSeparationMaxMagnitude,
				*A.NonPositiveSeparationSpatialHash,
				*A.SelectionHash,
				*A.DiagnosisHash);
		}
		Report += FString::Printf(TEXT("\nSame-seed replay: **%s**.\n\n"), *PassFail(bReplayPass));

		Report += TEXT("## Reason Distribution\n\n");
		Report += TEXT("| Scenario | Invalid sample | Resolved bad plate | No boundary pair | Non-separating invalid | Generated with non-positive separation | Other generation failure | Generated bad plate | Unhandled class | Unit anomaly | Field anomaly |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FScenarioResult& Result : Results)
		{
			const FCarrierLabPhaseIIIE67ApplyPathInvalidRecordsAudit& A = Result.Audit;
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d |\n"),
				*Result.Name,
				A.InvalidSampleIndexCount,
				A.ResolvedHitInvalidPlateCount,
				A.DivergentGapNoBoundaryPairCount,
				A.DivergentGapNonSeparatingCount,
				A.GeneratedWithNonPositiveSeparationCount,
				A.DivergentGapGenerationOtherFailureCount,
				A.GeneratedGapInvalidAssignedPlateCount,
				A.UnhandledSelectionClassCount,
				A.InvalidSampleUnitPositionCount,
				A.SampleFieldOutOfRangeCount);
		}
		Report += TEXT("\n");

		Report += TEXT("## Time-Cost Breakdown\n\n");
		Report += TEXT("| Scenario | Warmup s | Selection s | Record build s | Continuous boundary-pair query s | Resolved-copy s | Validation s | Total s |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FScenarioResult& Result : Results)
		{
			const FCarrierLabPhaseIIIE67ApplyPathInvalidRecordsAudit& A = Result.Audit;
			Report += FString::Printf(
				TEXT("| %s | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f |\n"),
				*Result.Name,
				Result.WarmupSeconds,
				A.SelectionSeconds,
				A.RecordBuildSeconds,
				A.DivergentQuerySeconds,
				A.ResolvedRecordSeconds,
				A.ValidationSeconds,
				Result.TotalSeconds);
		}
		Report += TEXT("\nThe diagnostic stops before topology rebuild. If `Continuous boundary-pair query s` dominates, the minutes-scale editor apply cost is in record building/gap-field validation rather than downstream topology rebuild.\n\n");

		Report += TEXT("## Process-State Cross-Reference\n\n");
		Report += TEXT("| Scenario | Invalid with any process-filtered source | Subducting | Obduction-pending | Collision-pending | Rifting-pending with any process-filtered source |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|\n");
		for (const FScenarioResult& Result : Results)
		{
			const FCarrierLabPhaseIIIE67ApplyPathInvalidRecordsAudit& A = Result.Audit;
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d |\n"),
				*Result.Name,
				A.InvalidWithAnyProcessFilterCount,
				A.InvalidWithSubductingFilterCount,
				A.InvalidWithObductionFilterCount,
				A.InvalidWithCollisionFilterCount,
				A.RiftingPendingWithAnyProcessFilterCount);
		}
		Report += TEXT("\n");

		Report += TEXT("## Spatial Distribution\n\n");
		Report += TEXT("Invalid records are binned into 12 longitude by 6 latitude bands. This is diagnostic localization only, not visual approval.\n\n");
		Report += TEXT("| Scenario | Top bins |\n");
		Report += TEXT("|---|---|\n");
		for (const FScenarioResult& Result : Results)
		{
			TArray<FSpatialBinSummary> TopBins = GetTopSpatialBins(Result.Audit, 6);
			FString BinsText;
			for (int32 Index = 0; Index < TopBins.Num(); ++Index)
			{
				if (Index > 0)
				{
					BinsText += TEXT("; ");
				}
				BinsText += FString::Printf(TEXT("%s: %d"), *SpatialBinLabel(TopBins[Index]), TopBins[Index].Count);
			}
			Report += FString::Printf(TEXT("| %s | %s |\n"), *Result.Name, *BinsText);
		}
		Report += TEXT("\n");

		Report += TEXT("## Artifacts\n\n");
		Report += FString::Printf(TEXT("- JSONL metrics: `%s`\n"), *MetricsPath);
		Report += TEXT("- JSONL includes one `scenario` row per run and one `invalid_record` row per invalid sample with reason, process-filter counters, source selection class, and spatial bin. Default paper-literal runs may have zero `invalid_record` rows; restored-veto runs preserve the older non-separating invalid sample rows.\n\n");

		Report += TEXT("## Stop Conditions Preserved\n\n");
		Report += TEXT("- No Stage 1.5 fallback and no prior-owner retention. The only behavior change is removal of the default non-positive-separation veto from zero-hit gap generation.\n");
		Report += TEXT("- Stop if primary invalid reasons do not sum exactly to the live `InvalidRecordCount`.\n");
		Report += TEXT("- Stop if selection unresolved multi-hit reappears; that would regress IIIE.6.6 rather than diagnose apply-path records.\n");
		Report += TEXT("- Stop if invalid records correlate heavily with process-filtered candidates; that redirects the next slice toward IIIB/IIIC/IIID marking rather than continuous boundary-pair generation.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += TEXT("Default paper-literal runs should have zero invalid records from the former `divergent_gap_nonseparating` class while reporting how many samples generated with non-positive signed separation. If any default run still has invalid records, inspect its remaining reason distribution before treating live cadence as visually unblocked. The restored-veto scenario is a historical baseline only.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIE67ApplyPathInvalidRecordsCommandlet::UCarrierLabPhaseIIIE67ApplyPathInvalidRecordsCommandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIE67ApplyPathInvalidRecordsCommandlet::Main(const FString& Params)
{
	UWorld* World = GetCommandletWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPhaseIIIE67ApplyPathInvalidRecords: no world available."));
		return 1;
	}

	const FString OutputDir = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("CarrierLab/PhaseIII/IIIE67ApplyPathInvalidRecords"));
	IFileManager::Get().MakeDirectory(*OutputDir, true);
	const FString MetricsPath = OutputDir / TEXT("metrics.jsonl");

	struct FScenarioSpec
	{
		FString Name;
		int32 Step = 0;
		bool bRestoreNonSeparatingAnomalyVeto = false;
	};
	const TArray<FScenarioSpec> ScenarioSpecs = {
		{ TEXT("manual_step_60_paper_literal_record_builder"), 60, false },
		{ TEXT("manual_step_60_paper_literal_replay_record_builder"), 60, false },
		{ TEXT("manual_step_60_restored_veto_baseline_record_builder"), 60, true }
	};

	TArray<FScenarioResult> Results;
	for (const FScenarioSpec& ScenarioSpec : ScenarioSpecs)
	{
		FScenarioResult Result;
		RunScenario(
			*World,
			ScenarioSpec.Name,
			ScenarioSpec.Step,
			ScenarioSpec.bRestoreNonSeparatingAnomalyVeto,
			Result);
		Results.Add(Result);
	}

	const FScenarioResult* Step60 = Results.FindByPredicate([](const FScenarioResult& Result)
	{
		return Result.Name == TEXT("manual_step_60_paper_literal_record_builder");
	});
	const FScenarioResult* Step60Replay = Results.FindByPredicate([](const FScenarioResult& Result)
	{
		return Result.Name == TEXT("manual_step_60_paper_literal_replay_record_builder");
	});
	const bool bReplayPass = Step60 != nullptr && Step60Replay != nullptr && ReplayMatches(*Step60, *Step60Replay);

	FString JsonLines;
	for (const FScenarioResult& Result : Results)
	{
		JsonLines += ScenarioJsonLine(Result) + LINE_TERMINATOR;
		for (const FCarrierLabPhaseIIIE67InvalidRecord& Record : Result.Audit.Records)
		{
			JsonLines += InvalidRecordJsonLine(Result.Name, Record) + LINE_TERMINATOR;
		}
	}
	JsonLines += FString::Printf(
		TEXT("{\"type\":\"same_seed_replay\",\"pass\":%s,\"hash_a\":%s,\"hash_b\":%s}"),
		*BoolText(bReplayPass),
		Step60 != nullptr ? *JsonString(Step60->Audit.DiagnosisHash) : TEXT("\"\""),
		Step60Replay != nullptr ? *JsonString(Step60Replay->Audit.DiagnosisHash) : TEXT("\"\""));
	JsonLines += LINE_TERMINATOR;
	FFileHelper::SaveStringToFile(JsonLines, *MetricsPath);

	const FString ReportPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir() / TEXT("docs/checkpoints/phase-iii-slice-iiie6-7-apply-path-invalid-records-diagnosis.md"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	const FString Report = BuildReport(Results, bReplayPass, MetricsPath);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	bool bScenariosPass = true;
	for (const FScenarioResult& Result : Results)
	{
		bScenariosPass = bScenariosPass && Result.bPass;
	}
	const bool bAllPass = bScenariosPass && bReplayPass;
	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIIE67ApplyPathInvalidRecords %s. Report: %s"), bAllPass ? TEXT("PASS") : TEXT("FAIL"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIIE67ApplyPathInvalidRecords metrics: %s"), *MetricsPath);
	return bAllPass ? 0 : 1;
}
