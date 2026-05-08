// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIE64LiveCadencePostMotionMultiHitCommandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace
{
	constexpr int32 DefaultSampleCount = 100000;
	constexpr int32 DefaultPlateCount = 40;
	constexpr int32 DefaultSeed = 42;
	constexpr double ExpectedDefaultSpeedMmPerYear = 66.6666666667;
	constexpr double SpeedToleranceMmPerYear = 1.0e-3;
	constexpr double CadenceToleranceMa = 1.0e-6;
	constexpr int32 PanelWidth = 512;
	constexpr int32 PanelHeight = 256;
	constexpr int32 SheetColumns = 2;

	void HashMix(uint64& Hash, const uint64 Value)
	{
		Hash ^= Value;
		Hash *= 1099511628211ull;
	}

	void HashMixDouble(uint64& Hash, const double Value)
	{
		HashMix(Hash, static_cast<uint64>(FMath::RoundToInt64(Value * 1000000000.0)));
	}

	FString HashToString(const uint64 Hash)
	{
		return FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Hash));
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

	FString BoolText(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	FString BucketName(const ECarrierLabPhaseIIIE3MultiHitBucket Bucket)
	{
		switch (Bucket)
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

	FString BarycentricShapeName(const ECarrierLabPhaseIIIE62BarycentricShape Shape)
	{
		switch (Shape)
		{
		case ECarrierLabPhaseIIIE62BarycentricShape::Vertex:
			return TEXT("vertex");
		case ECarrierLabPhaseIIIE62BarycentricShape::Edge:
			return TEXT("edge");
		case ECarrierLabPhaseIIIE62BarycentricShape::Interior:
			return TEXT("interior");
		case ECarrierLabPhaseIIIE62BarycentricShape::Unknown:
		default:
			return TEXT("unknown");
		}
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

	FString ResolvePath(const FString& Params, const TCHAR* Key, const FString& DefaultPath)
	{
		FString Value;
		if (!FParse::Value(*Params, Key, Value))
		{
			Value = DefaultPath;
		}
		else if (FPaths::IsRelative(Value))
		{
			Value = FPaths::Combine(FPaths::ProjectDir(), Value);
		}
		return FPaths::ConvertRelativePathToFull(Value);
	}

	FString GetOutputRoot(const FString& Params)
	{
		return ResolvePath(
			Params,
			TEXT("Out="),
			FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("CarrierLab"),
				TEXT("PhaseIII"),
				TEXT("IIIE64LiveCadencePostMotionMultiHit")));
	}

	FString GetReportPath(const FString& Params)
	{
		return ResolvePath(
			Params,
			TEXT("Report="),
			FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("docs"),
				TEXT("checkpoints"),
				TEXT("phase-iii-slice-iiie6-4-live-cadence-post-motion-multihit-diagnosis.md")));
	}

	FString GetPngPath(const FString& Params)
	{
		return ResolvePath(
			Params,
			TEXT("Png="),
			FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("docs"),
				TEXT("checkpoints"),
				TEXT("phase-iii-slice-iiie6-4-spatial-distribution.png")));
	}

	ACarrierLabVisualizationActor* SpawnActor(UWorld& World, const int32 SampleCount, const int32 PlateCount, const double VelocityMmPerYear)
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
		Actor->Seed = DefaultSeed;
		Actor->ContinentalPlateFraction = 0.0;
		Actor->VelocityMmPerYear = VelocityMmPerYear;
		Actor->bEnableNaturalResamplingEvents = false;
		Actor->bEnablePhaseIIIE3DuplicateHitCoalescing = true;
		Actor->bEnablePhaseIIIE3SharedBoundaryTieBreak = true;
		Actor->ConfigurePhaseIIICProcessLayer(true, false);
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	struct FCadenceGateResult
	{
		FString Name;
		bool bRan = false;
		bool bPass = false;
		double CurrentSpeedMmPerYear = 0.0;
		double ObservedWindowSpeedMmPerYear = 0.0;
		double CadenceDeltaTMa = 0.0;
		int32 CadenceSteps = 0;
		int32 NextResampleStep = 0;
	};

	struct FScenarioResult
	{
		FString Name;
		bool bAutomatic = false;
		bool bRan = false;
		bool bPass = false;
		bool bApplied = false;
		bool bAttempted = false;
		int32 TargetStep = 0;
		int32 StepBeforeAttempt = 0;
		int32 StepAfterAttempt = 0;
		int32 EventCountBefore = 0;
		int32 EventCountAfter = 0;
		int32 NextResampleStepBefore = 0;
		int32 NextResampleStepAfter = 0;
		double CurrentSpeedMmPerYear = 0.0;
		double ObservedWindowSpeedMmPerYear = 0.0;
		double CadenceDeltaTMa = 0.0;
		int32 CadenceSteps = 0;
		FString ProjectionHashBefore;
		FString ProjectionHashAfter;
		FString StateHashBefore;
		FString StateHashAfter;
		FString CrustHashBefore;
		FString CrustHashAfter;
		FString LastRemeshMode;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIE64PostMotionMultiHitAudit Audit;
	};

	bool EvaluateCadenceGate(UWorld& World, const FString& Name, const double VelocityMmPerYear, const int32 ExpectedSteps, const double ExpectedDeltaTMa, FCadenceGateResult& OutResult)
	{
		OutResult = FCadenceGateResult();
		OutResult.Name = Name;
		ACarrierLabVisualizationActor* Actor = SpawnActor(World, 256, 8, VelocityMmPerYear);
		if (Actor == nullptr || !Actor->InitializeCarrier())
		{
			if (Actor != nullptr)
			{
				Actor->Destroy();
			}
			return false;
		}
		OutResult.CurrentSpeedMmPerYear = Actor->CurrentMetrics.ObservedMaxPlateSpeedMmPerYear;
		OutResult.ObservedWindowSpeedMmPerYear = Actor->CurrentMetrics.ObservedMaxPlateSpeedSinceLastRemeshMmPerYear;
		OutResult.CadenceDeltaTMa = Actor->CurrentMetrics.CadenceDeltaTMa;
		OutResult.CadenceSteps = Actor->CurrentMetrics.CadenceSteps;
		OutResult.NextResampleStep = Actor->CurrentMetrics.NextResampleStep;
		OutResult.bRan = true;
		OutResult.bPass =
			OutResult.CadenceSteps == ExpectedSteps &&
			FMath::Abs(OutResult.CadenceDeltaTMa - ExpectedDeltaTMa) <= CadenceToleranceMa &&
			OutResult.NextResampleStep == ExpectedSteps;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return true;
	}

	bool RunManualScenario(UWorld& World, const FString& Name, const int32 WarmupSteps, FScenarioResult& OutResult)
	{
		OutResult = FScenarioResult();
		OutResult.Name = Name;
		OutResult.TargetStep = WarmupSteps;
		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(World, DefaultSampleCount, DefaultPlateCount, ExpectedDefaultSpeedMmPerYear);
		if (Actor == nullptr || !Actor->InitializeCarrier())
		{
			if (Actor != nullptr)
			{
				Actor->Destroy();
			}
			return false;
		}
		Actor->ConfigurePhaseIIICProcessLayer(true, false);
		for (int32 StepIndex = 0; StepIndex < WarmupSteps; ++StepIndex)
		{
			Actor->StepOnce();
		}

		OutResult.StepBeforeAttempt = Actor->CurrentMetrics.Step;
		OutResult.EventCountBefore = Actor->CurrentMetrics.EventCount;
		OutResult.NextResampleStepBefore = Actor->CurrentMetrics.NextResampleStep;
		OutResult.CurrentSpeedMmPerYear = Actor->CurrentMetrics.ObservedMaxPlateSpeedMmPerYear;
		OutResult.ObservedWindowSpeedMmPerYear = Actor->CurrentMetrics.ObservedMaxPlateSpeedSinceLastRemeshMmPerYear;
		OutResult.CadenceDeltaTMa = Actor->CurrentMetrics.CadenceDeltaTMa;
		OutResult.CadenceSteps = Actor->CurrentMetrics.CadenceSteps;
		OutResult.ProjectionHashBefore = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashBefore = Actor->CurrentMetrics.StateHash;
		OutResult.CrustHashBefore = Actor->CurrentMetrics.CrustStateHash;
		OutResult.bRan = Actor->RunPhaseIIIE64PostMotionMultiHitDiagnosisAudit(OutResult.Audit);
		OutResult.bAttempted = true;
		OutResult.bApplied = Actor->ApplyPhaseIIIELiveRemeshEvent();
		OutResult.StepAfterAttempt = Actor->CurrentMetrics.Step;
		OutResult.EventCountAfter = Actor->CurrentMetrics.EventCount;
		OutResult.NextResampleStepAfter = Actor->CurrentMetrics.NextResampleStep;
		OutResult.ProjectionHashAfter = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashAfter = Actor->CurrentMetrics.StateHash;
		OutResult.CrustHashAfter = Actor->CurrentMetrics.CrustStateHash;
		OutResult.LastRemeshMode = Actor->CurrentMetrics.LastRemeshMode;
		const bool bHashesUnchanged =
			OutResult.ProjectionHashBefore == OutResult.ProjectionHashAfter &&
			OutResult.StateHashBefore == OutResult.StateHashAfter &&
			OutResult.CrustHashBefore == OutResult.CrustHashAfter;
		OutResult.bPass =
			OutResult.bRan &&
			!OutResult.bApplied &&
			OutResult.Audit.PolicyWinnerCount == 0 &&
			OutResult.Audit.PriorOwnerFallbackCount == 0 &&
			OutResult.Audit.ProjectionAuthorityCount == 0 &&
			OutResult.EventCountAfter == OutResult.EventCountBefore &&
			bHashesUnchanged &&
			OutResult.Audit.DiagnosedHoldCount == OutResult.Audit.SelectionUnresolvedMultiHitCount;
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bRan;
	}

	bool RunAutomaticScenario(UWorld& World, const FString& Name, const int32 TargetStep, FScenarioResult& OutResult)
	{
		OutResult = FScenarioResult();
		OutResult.Name = Name;
		OutResult.bAutomatic = true;
		OutResult.TargetStep = TargetStep;
		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(World, DefaultSampleCount, DefaultPlateCount, ExpectedDefaultSpeedMmPerYear);
		if (Actor == nullptr || !Actor->InitializeCarrier())
		{
			if (Actor != nullptr)
			{
				Actor->Destroy();
			}
			return false;
		}
		Actor->ConfigurePhaseIIICProcessLayer(true, false);
		Actor->bEnableNaturalResamplingEvents = true;
		for (int32 StepIndex = 0; StepIndex < TargetStep - 1; ++StepIndex)
		{
			Actor->StepOnce();
		}
		OutResult.StepBeforeAttempt = Actor->CurrentMetrics.Step;
		OutResult.EventCountBefore = Actor->CurrentMetrics.EventCount;
		OutResult.NextResampleStepBefore = Actor->CurrentMetrics.NextResampleStep;
		OutResult.ProjectionHashBefore = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashBefore = Actor->CurrentMetrics.StateHash;
		OutResult.CrustHashBefore = Actor->CurrentMetrics.CrustStateHash;
		Actor->StepOnce();
		OutResult.bAttempted = true;
		OutResult.StepAfterAttempt = Actor->CurrentMetrics.Step;
		OutResult.EventCountAfter = Actor->CurrentMetrics.EventCount;
		OutResult.NextResampleStepAfter = Actor->CurrentMetrics.NextResampleStep;
		OutResult.CurrentSpeedMmPerYear = Actor->CurrentMetrics.ObservedMaxPlateSpeedMmPerYear;
		OutResult.ObservedWindowSpeedMmPerYear = Actor->CurrentMetrics.ObservedMaxPlateSpeedSinceLastRemeshMmPerYear;
		OutResult.CadenceDeltaTMa = Actor->CurrentMetrics.CadenceDeltaTMa;
		OutResult.CadenceSteps = Actor->CurrentMetrics.CadenceSteps;
		OutResult.ProjectionHashAfter = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashAfter = Actor->CurrentMetrics.StateHash;
		OutResult.CrustHashAfter = Actor->CurrentMetrics.CrustStateHash;
		OutResult.LastRemeshMode = Actor->CurrentMetrics.LastRemeshMode;
		OutResult.bApplied = OutResult.EventCountAfter > OutResult.EventCountBefore;
		OutResult.bRan = Actor->RunPhaseIIIE64PostMotionMultiHitDiagnosisAudit(OutResult.Audit);
		OutResult.bPass =
			OutResult.bRan &&
			!OutResult.bApplied &&
			OutResult.Audit.PolicyWinnerCount == 0 &&
			OutResult.Audit.PriorOwnerFallbackCount == 0 &&
			OutResult.Audit.ProjectionAuthorityCount == 0 &&
			OutResult.EventCountAfter == OutResult.EventCountBefore &&
			OutResult.NextResampleStepAfter == OutResult.NextResampleStepBefore &&
			OutResult.LastRemeshMode.StartsWith(TEXT("phase_iii_e6_live_hold_unresolved_multi_hit")) &&
			OutResult.Audit.DiagnosedHoldCount == OutResult.Audit.SelectionUnresolvedMultiHitCount;
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bRan;
	}

	uint64 HashScenario(const FScenarioResult& Result)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMix(Hash, static_cast<uint64>(Result.TargetStep + 1));
		HashMix(Hash, Result.bAutomatic ? 2ull : 1ull);
		HashMix(Hash, static_cast<uint64>(Result.Audit.SelectionUnresolvedMultiHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.Audit.SelectionCrossPlateDifferentMultiHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.Audit.SelectionThirdPlateMultiHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.Audit.ProcessMarkedHoldCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.Audit.UniqueNearestCrossPlateDifferentCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.Audit.DistanceTieCrossPlateDifferentCount + 1));
		HashMixDouble(Hash, Result.Audit.MedianNearestDistanceGapKm);
		HashMixDouble(Hash, Result.Audit.P95NearestDistanceGapKm);
		for (const TCHAR Ch : Result.Audit.SelectionHash)
		{
			HashMix(Hash, static_cast<uint64>(Ch));
		}
		for (const TCHAR Ch : Result.Audit.DiagnosisHash)
		{
			HashMix(Hash, static_cast<uint64>(Ch));
		}
		return Hash;
	}

	FString CandidateJson(const FCarrierLabPhaseIIIE64CandidateDiagnostic& Candidate)
	{
		return FString::Printf(
			TEXT("{\"candidate_index\":%d,\"plate_id\":%d,\"local_triangle_id\":%d,\"source_triangle_id\":%d,\"global_vertices\":[%d,%d,%d],\"barycentric_shape\":%s,\"distance\":%.17g,\"ray_residual_km\":%.17g,\"scalar_residual\":%.17g,\"elevation_residual_km\":%.17g,\"unit_vector_residual\":%.17g,\"subducting\":%s,\"obduction_pending\":%s,\"collision_pending\":%s,\"nearest\":%s,\"plate_continental_fraction\":%.17g,\"plate_oceanic_age\":%.17g}"),
			Candidate.Snapshot.CandidateIndex,
			Candidate.Snapshot.PlateId,
			Candidate.Snapshot.LocalTriangleId,
			Candidate.Snapshot.SourceTriangleId,
			Candidate.Snapshot.GlobalVertexIds[0],
			Candidate.Snapshot.GlobalVertexIds[1],
			Candidate.Snapshot.GlobalVertexIds[2],
			*JsonString(BarycentricShapeName(Candidate.Snapshot.BarycentricShape)),
			Candidate.Snapshot.Distance,
			Candidate.Snapshot.RayDistanceResidualKm,
			Candidate.Snapshot.ScalarResidual,
			Candidate.Snapshot.ElevationResidualKm,
			Candidate.Snapshot.UnitVectorResidual,
			*BoolText(Candidate.bSubductingMarked),
			*BoolText(Candidate.bObductionPendingMarked),
			*BoolText(Candidate.bCollisionPendingMarked),
			*BoolText(Candidate.bNearestCandidate),
			Candidate.PlateContinentalFraction,
			Candidate.PlateOceanicAge);
	}

	FString HoldJsonLine(const FString& ScenarioName, const FCarrierLabPhaseIIIE64HoldRecord& Record)
	{
		FString CandidateArray;
		for (int32 Index = 0; Index < Record.Candidates.Num(); ++Index)
		{
			if (Index > 0)
			{
				CandidateArray += TEXT(",");
			}
			CandidateArray += CandidateJson(Record.Candidates[Index]);
		}
		return FString::Printf(
			TEXT("{\"type\":\"hold\",\"scenario\":%s,\"sample_id\":%d,\"step\":%d,\"bucket\":%s,\"candidate_count\":%d,\"distinct_plate_count\":%d,\"sample_unit\":[%.17g,%.17g,%.17g],\"unique_nearest\":%s,\"nearest_distance_tie\":%s,\"nearest_gap_km\":%.17g,\"any_process_marked\":%s,\"subducting_marked\":%s,\"obduction_pending_marked\":%s,\"collision_pending_marked\":%s,\"nearest_more_continental\":%s,\"nearest_older_oceanic\":%s,\"nearest_lower_plate_id\":%s,\"continental_tie\":%s,\"oceanic_age_tie\":%s,\"candidates\":[%s]}"),
			*JsonString(ScenarioName),
			Record.SampleId,
			Record.Step,
			*JsonString(BucketName(Record.MultiHitBucket)),
			Record.CandidateCount,
			Record.DistinctPlateCount,
			Record.SampleUnitPosition.X,
			Record.SampleUnitPosition.Y,
			Record.SampleUnitPosition.Z,
			*BoolText(Record.bHasUniqueNearest),
			*BoolText(Record.bNearestDistanceTie),
			Record.NearestDistanceGapKm,
			*BoolText(Record.bAnyCandidateProcessMarked),
			*BoolText(Record.bAnySubductingMarked),
			*BoolText(Record.bAnyObductionPendingMarked),
			*BoolText(Record.bAnyCollisionPendingMarked),
			*BoolText(Record.bNearestIsMostContinentalPlate),
			*BoolText(Record.bNearestIsOlderOceanicPlate),
			*BoolText(Record.bNearestIsLowerPlateId),
			*BoolText(Record.bContinentalPlateTie),
			*BoolText(Record.bOceanicAgePlateTie),
			*CandidateArray);
	}

	FString ScenarioJsonLine(const FScenarioResult& Result)
	{
		return FString::Printf(
			TEXT("{\"type\":\"scenario\",\"scenario\":%s,\"automatic\":%s,\"pass\":%s,\"applied\":%s,\"target_step\":%d,\"step_before\":%d,\"step_after\":%d,\"events_before\":%d,\"events_after\":%d,\"next_before\":%d,\"next_after\":%d,\"speed_mm_per_year\":%.17g,\"observed_window_speed_mm_per_year\":%.17g,\"cadence_delta_t_ma\":%.17g,\"cadence_steps\":%d,\"unresolved\":%d,\"cross_plate_different\":%d,\"third_plate\":%d,\"cross_plate_equal\":%d,\"coalesced\":%d,\"shared_tiebreak\":%d,\"process_marked\":%d,\"unique_nearest_cross_plate_different\":%d,\"distance_tie_cross_plate_different\":%d,\"nearest_more_continental\":%d,\"nearest_older_oceanic\":%d,\"nearest_lower_plate_id\":%d,\"median_nearest_gap_km\":%.17g,\"p95_nearest_gap_km\":%.17g,\"selection_hash\":%s,\"diagnosis_hash\":%s,\"last_remesh_mode\":%s}"),
			*JsonString(Result.Name),
			*BoolText(Result.bAutomatic),
			*BoolText(Result.bPass),
			*BoolText(Result.bApplied),
			Result.TargetStep,
			Result.StepBeforeAttempt,
			Result.StepAfterAttempt,
			Result.EventCountBefore,
			Result.EventCountAfter,
			Result.NextResampleStepBefore,
			Result.NextResampleStepAfter,
			Result.CurrentSpeedMmPerYear,
			Result.ObservedWindowSpeedMmPerYear,
			Result.CadenceDeltaTMa,
			Result.CadenceSteps,
			Result.Audit.SelectionUnresolvedMultiHitCount,
			Result.Audit.SelectionCrossPlateDifferentMultiHitCount,
			Result.Audit.SelectionThirdPlateMultiHitCount,
			Result.Audit.SelectionCrossPlateEqualMultiHitCount,
			Result.Audit.CoalescedMultiHitCount,
			Result.Audit.SharedBoundaryTieBreakCount,
			Result.Audit.ProcessMarkedHoldCount,
			Result.Audit.UniqueNearestCrossPlateDifferentCount,
			Result.Audit.DistanceTieCrossPlateDifferentCount,
			Result.Audit.NearestMostContinentalCount,
			Result.Audit.NearestOlderOceanicCount,
			Result.Audit.NearestLowerPlateIdCount,
			Result.Audit.MedianNearestDistanceGapKm,
			Result.Audit.P95NearestDistanceGapKm,
			*JsonString(Result.Audit.SelectionHash),
			*JsonString(Result.Audit.DiagnosisHash),
			*JsonString(Result.LastRemeshMode));
	}

	bool ForwardMollweidePixel(const FVector3d& UnitPosition, const int32 XOffset, const int32 YOffset, int32& OutX, int32& OutY)
	{
		const FVector3d Unit = UnitPosition.GetSafeNormal();
		if (!Unit.IsNormalized())
		{
			return false;
		}
		const double Lon = FMath::Atan2(Unit.Y, Unit.X);
		const double Lat = FMath::Asin(FMath::Clamp(Unit.Z, -1.0, 1.0));
		double Theta = Lat;
		for (int32 Iter = 0; Iter < 12; ++Iter)
		{
			const double F = 2.0 * Theta + FMath::Sin(2.0 * Theta) - UE_PI * FMath::Sin(Lat);
			const double Df = 2.0 + 2.0 * FMath::Cos(2.0 * Theta);
			if (FMath::Abs(Df) <= UE_DOUBLE_SMALL_NUMBER)
			{
				break;
			}
			const double Delta = F / Df;
			Theta -= Delta;
			if (FMath::Abs(Delta) <= 1.0e-12)
			{
				break;
			}
		}
		const double X = (2.0 * FMath::Sqrt(2.0) / UE_PI) * Lon * FMath::Cos(Theta);
		const double Y = FMath::Sqrt(2.0) * FMath::Sin(Theta);
		const double NormalizedX = (X / (2.0 * FMath::Sqrt(2.0)) + 0.5);
		const double NormalizedY = (0.5 - Y / (2.0 * FMath::Sqrt(2.0)));
		OutX = XOffset + FMath::Clamp(FMath::RoundToInt(NormalizedX * static_cast<double>(PanelWidth - 1)), 0, PanelWidth - 1);
		OutY = YOffset + FMath::Clamp(FMath::RoundToInt(NormalizedY * static_cast<double>(PanelHeight - 1)), 0, PanelHeight - 1);
		return true;
	}

	void PaintPixel(TArray<FColor>& Pixels, const int32 Width, const int32 Height, const int32 X, const int32 Y, const FColor Color)
	{
		for (int32 Dy = -1; Dy <= 1; ++Dy)
		{
			for (int32 Dx = -1; Dx <= 1; ++Dx)
			{
				const int32 PX = X + Dx;
				const int32 PY = Y + Dy;
				if (PX >= 0 && PX < Width && PY >= 0 && PY < Height)
				{
					Pixels[PY * Width + PX] = Color;
				}
			}
		}
	}

	void FillPanelBackground(TArray<FColor>& Pixels, const int32 Width, const int32 XOffset, const int32 YOffset)
	{
		for (int32 Y = 0; Y < PanelHeight; ++Y)
		{
			for (int32 X = 0; X < PanelWidth; ++X)
			{
				const double NX = 2.0 * (static_cast<double>(X) + 0.5) / static_cast<double>(PanelWidth) - 1.0;
				const double NY = 2.0 * (static_cast<double>(Y) + 0.5) / static_cast<double>(PanelHeight) - 1.0;
				const bool bInside = NX * NX + NY * NY <= 1.0;
				Pixels[(YOffset + Y) * Width + XOffset + X] = bInside ? FColor(18, 18, 24, 255) : FColor::Black;
			}
		}
	}

	bool SavePng(const FString& Path, const TArray<FColor>& Pixels, const int32 Width, const int32 Height)
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		if (!ImageWrapper.IsValid())
		{
			return false;
		}
		if (!ImageWrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
		{
			return false;
		}
		return FFileHelper::SaveArrayToFile(ImageWrapper->GetCompressed(90), *Path);
	}

	bool SaveSpatialDistributionPng(const FString& Path, const TArray<FScenarioResult>& Results)
	{
		const int32 SheetRows = Results.Num();
		const int32 Width = PanelWidth * SheetColumns;
		const int32 Height = PanelHeight * SheetRows;
		TArray<FColor> Pixels;
		Pixels.Init(FColor::Black, Width * Height);
		for (int32 Row = 0; Row < Results.Num(); ++Row)
		{
			const int32 LeftX = 0;
			const int32 RightX = PanelWidth;
			const int32 YOffset = Row * PanelHeight;
			FillPanelBackground(Pixels, Width, LeftX, YOffset);
			FillPanelBackground(Pixels, Width, RightX, YOffset);
			for (const FCarrierLabPhaseIIIE64HoldRecord& Record : Results[Row].Audit.Records)
			{
				int32 X = 0;
				int32 Y = 0;
				if (!ForwardMollweidePixel(Record.SampleUnitPosition, LeftX, YOffset, X, Y))
				{
					continue;
				}
				const FColor ClassColor = Record.MultiHitBucket == ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate
					? FColor(255, 0, 255, 255)
					: FColor(255, 64, 64, 255);
				PaintPixel(Pixels, Width, Height, X, Y, ClassColor);

				if (!ForwardMollweidePixel(Record.SampleUnitPosition, RightX, YOffset, X, Y))
				{
					continue;
				}
				const FColor DiagnosticColor = Record.bAnyCandidateProcessMarked
					? FColor(255, 220, 0, 255)
					: (Record.bHasUniqueNearest ? FColor(0, 220, 255, 255) : FColor(80, 120, 255, 255));
				PaintPixel(Pixels, Width, Height, X, Y, DiagnosticColor);
			}
		}
		return SavePng(Path, Pixels, Width, Height);
	}

	FString BuildReport(
		const TArray<FCadenceGateResult>& CadenceGates,
		const TArray<FScenarioResult>& Scenarios,
		const FString& JsonPath,
		const FString& PngPath,
		const FString& ReplayHash)
	{
		bool bCadencePass = true;
		for (const FCadenceGateResult& Gate : CadenceGates)
		{
			bCadencePass = bCadencePass && Gate.bPass;
		}
		bool bScenarioPass = true;
		for (const FScenarioResult& Scenario : Scenarios)
		{
			bScenarioPass = bScenarioPass && Scenario.bPass;
		}
		const bool bAllPass = bCadencePass && bScenarioPass;

		FString Report;
		Report += TEXT("# Phase IIIE.6.4 Live Cadence + Post-Motion Multi-Hit Diagnosis\n\n");
		Report += FString::Printf(
			TEXT("**Verdict:** %s / DIAGNOSTIC ONLY. The live remesh blocker is still held fail-loud; this slice records why manual and automatic remesh attempts hold after motion and does not resolve cross-plate or third-plate cases.\n\n"),
			bAllPass ? TEXT("PASS") : TEXT("FAIL"));
		Report += TEXT("## Scope\n\n");
		Report += TEXT("IIIE.6.4 adds cadence telemetry and post-motion multi-hit diagnosis only. It does not assign `cross_plate_different` or `third_plate` samples, does not use nearest-hit as authority, does not increment remesh event count on hold, and does not promote Stage 1.5 policy. The spatial PNG is diagnostic evidence only, not visual approval.\n\n");

		Report += TEXT("## Cadence Gates\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		for (const FCadenceGateResult& Gate : CadenceGates)
		{
			Report += FString::Printf(
				TEXT("| %s | %s | speed `%.6g`, observed-window `%.6g`, deltaT `%.6g Ma`, cadence `%d`, next `%d` |\n"),
				*Gate.Name,
				*PassFail(Gate.bPass),
				Gate.CurrentSpeedMmPerYear,
				Gate.ObservedWindowSpeedMmPerYear,
				Gate.CadenceDeltaTMa,
				Gate.CadenceSteps,
				Gate.NextResampleStep);
		}
		Report += TEXT("\n");
		Report += TEXT("Manual rows compare before/after a same-step remesh click. The automatic row spans the live step advance from 31 to 32, so projection/state/crust hash changes there are motion-step changes; the remesh-specific invariant is `events 0->0`, fail-loud mode, and overdue target preservation `next 32->32`.\n\n");

		Report += TEXT("## Manual And Automatic Remesh Diagnostics\n\n");
		Report += TEXT("| Scenario | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		for (const FScenarioResult& Scenario : Scenarios)
		{
			Report += FString::Printf(
				TEXT("| %s | %s | auto `%d`, step `%d->%d`, events `%d->%d`, next `%d->%d`, speed/window `%.6g/%.6g`, cadence `%d steps / %.6g Ma`, holds `%d`, crossDiff `%d`, third `%d`, process-marked `%d`, unique-nearest crossDiff `%d`, distance-tie crossDiff `%d`, nearest more-cont/older/lowerId `%d/%d/%d`, hashes `%s/%s/%s -> %s/%s/%s`, mode `%s` |\n"),
				*Scenario.Name,
				*PassFail(Scenario.bPass),
				Scenario.bAutomatic ? 1 : 0,
				Scenario.StepBeforeAttempt,
				Scenario.StepAfterAttempt,
				Scenario.EventCountBefore,
				Scenario.EventCountAfter,
				Scenario.NextResampleStepBefore,
				Scenario.NextResampleStepAfter,
				Scenario.CurrentSpeedMmPerYear,
				Scenario.ObservedWindowSpeedMmPerYear,
				Scenario.CadenceSteps,
				Scenario.CadenceDeltaTMa,
				Scenario.Audit.SelectionUnresolvedMultiHitCount,
				Scenario.Audit.SelectionCrossPlateDifferentMultiHitCount,
				Scenario.Audit.SelectionThirdPlateMultiHitCount,
				Scenario.Audit.ProcessMarkedHoldCount,
				Scenario.Audit.UniqueNearestCrossPlateDifferentCount,
				Scenario.Audit.DistanceTieCrossPlateDifferentCount,
				Scenario.Audit.NearestMostContinentalCount,
				Scenario.Audit.NearestOlderOceanicCount,
				Scenario.Audit.NearestLowerPlateIdCount,
				*Scenario.ProjectionHashBefore,
				*Scenario.StateHashBefore,
				*Scenario.CrustHashBefore,
				*Scenario.ProjectionHashAfter,
				*Scenario.StateHashAfter,
				*Scenario.CrustHashAfter,
				*Scenario.LastRemeshMode);
		}
		Report += TEXT("\n");

		Report += TEXT("## Nearest-Hit Diagnostic\n\n");
		Report += TEXT("Nearest-hit is computed as a parallel diagnostic for `cross_plate_different` holds only. It is not a remesh source-selection rule. A unique nearest requires the nearest and second-nearest ray distances to differ by more than `1e-9 km`; ties remain ties.\n\n");
		Report += TEXT("| Scenario | Unique nearest | Distance tie | More continental | Older oceanic | Lower plate id | Median gap km | P95 gap km |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FScenarioResult& Scenario : Scenarios)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d | %.6g | %.6g |\n"),
				*Scenario.Name,
				Scenario.Audit.UniqueNearestCrossPlateDifferentCount,
				Scenario.Audit.DistanceTieCrossPlateDifferentCount,
				Scenario.Audit.NearestMostContinentalCount,
				Scenario.Audit.NearestOlderOceanicCount,
				Scenario.Audit.NearestLowerPlateIdCount,
				Scenario.Audit.MedianNearestDistanceGapKm,
				Scenario.Audit.P95NearestDistanceGapKm);
		}
		Report += TEXT("\n");

		Report += TEXT("## Artifacts\n\n");
		Report += FString::Printf(TEXT("- JSONL metrics: `%s`\n"), *JsonPath);
		Report += FString::Printf(TEXT("- Spatial diagnostic PNG: `%s`\n"), *PngPath);
		Report += TEXT("- PNG layout: each row is a scenario in table order; left panel colors unresolved class (`cross_plate_different` red, `third_plate` magenta), right panel colors diagnostic shape (`unique nearest` cyan, `distance tie` blue, process-marked yellow).\n");
		Report += FString::Printf(TEXT("- Scenario replay hash: `%s`\n\n"), *ReplayHash);

		Report += TEXT("## Stop Conditions Preserved\n\n");
		Report += TEXT("- Stop if any `cross_plate_different` or `third_plate` sample is assigned by this slice.\n");
		Report += TEXT("- Stop if manual and automatic remesh paths diverge in policy counters or hold semantics.\n");
		Report += TEXT("- Stop if a same-step held manual remesh increments event count, changes projection/state/crust hashes, or advances the automatic cadence target as though remesh succeeded.\n");
		Report += TEXT("- Stop if process-state cross-references show marked triangles among visible holds; that would redirect the next slice toward IIIB/IIIC/IIID marking rather than a tie-break policy.\n");
		Report += TEXT("- Stop if nearest-hit diagnostics are treated as source-selection authority.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += TEXT("Use this report to choose the next resolver slice. If `cross_plate_different` holds are mostly unique-nearest with no process marks, the next design can evaluate a nearest-valid-hit lab policy. If process marks appear, fix upstream process-state filtering first. If third-plate or distance ties dominate, keep live remesh held and design a narrower policy or topology correction.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIE64LiveCadencePostMotionMultiHitCommandlet::UCarrierLabPhaseIIIE64LiveCadencePostMotionMultiHitCommandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIE64LiveCadencePostMotionMultiHitCommandlet::Main(const FString& Params)
{
	UWorld* World = GetCommandletWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPhaseIIIE64LiveCadencePostMotionMultiHit: no world available."));
		return 1;
	}

	const FString OutputRoot = GetOutputRoot(Params);
	const FString ReportPath = GetReportPath(Params);
	const FString PngPath = GetPngPath(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(PngPath), true);
	const FString JsonPath = FPaths::Combine(OutputRoot, TEXT("phase-iii-slice-iiie6-4-live-cadence-post-motion-multihit-diagnosis-metrics.jsonl"));

	TArray<FCadenceGateResult> CadenceGates;
	FCadenceGateResult CadenceDefault;
	EvaluateCadenceGate(*World, TEXT("Default observed-speed cadence"), ExpectedDefaultSpeedMmPerYear, 32, 64.0, CadenceDefault);
	CadenceDefault.bPass = CadenceDefault.bPass &&
		FMath::Abs(CadenceDefault.CurrentSpeedMmPerYear - ExpectedDefaultSpeedMmPerYear) <= SpeedToleranceMmPerYear;
	CadenceGates.Add(CadenceDefault);
	FCadenceGateResult CadenceZero;
	EvaluateCadenceGate(*World, TEXT("Zero-speed maximum cadence"), 0.0, 64, 128.0, CadenceZero);
	CadenceGates.Add(CadenceZero);
	FCadenceGateResult CadenceFast;
	EvaluateCadenceGate(*World, TEXT("Fast-speed minimum cadence"), 150.0, 16, 32.0, CadenceFast);
	CadenceGates.Add(CadenceFast);

	TArray<FScenarioResult> Scenarios;
	FScenarioResult Manual10;
	RunManualScenario(*World, TEXT("manual_step_10"), 10, Manual10);
	Scenarios.Add(Manual10);
	FScenarioResult Manual20;
	RunManualScenario(*World, TEXT("manual_step_20"), 20, Manual20);
	Scenarios.Add(Manual20);
	FScenarioResult Manual32;
	RunManualScenario(*World, TEXT("manual_step_32"), 32, Manual32);
	Scenarios.Add(Manual32);
	FScenarioResult Manual60;
	RunManualScenario(*World, TEXT("manual_step_60"), 60, Manual60);
	Scenarios.Add(Manual60);
	FScenarioResult Auto32;
	RunAutomaticScenario(*World, TEXT("auto_cadence_step_32"), 32, Auto32);
	Scenarios.Add(Auto32);

	uint64 ReplayHashValue = 1469598103934665603ull;
	for (const FScenarioResult& Scenario : Scenarios)
	{
		HashMix(ReplayHashValue, HashScenario(Scenario));
	}
	const FString ReplayHash = HashToString(ReplayHashValue);

	FString JsonLines;
	for (const FCadenceGateResult& Gate : CadenceGates)
	{
		JsonLines += FString::Printf(
			TEXT("{\"type\":\"cadence\",\"name\":%s,\"pass\":%s,\"speed_mm_per_year\":%.17g,\"observed_window_speed_mm_per_year\":%.17g,\"cadence_delta_t_ma\":%.17g,\"cadence_steps\":%d,\"next_resample_step\":%d}\n"),
			*JsonString(Gate.Name),
			*BoolText(Gate.bPass),
			Gate.CurrentSpeedMmPerYear,
			Gate.ObservedWindowSpeedMmPerYear,
			Gate.CadenceDeltaTMa,
			Gate.CadenceSteps,
			Gate.NextResampleStep);
	}
	for (const FScenarioResult& Scenario : Scenarios)
	{
		JsonLines += ScenarioJsonLine(Scenario) + LINE_TERMINATOR;
		for (const FCarrierLabPhaseIIIE64HoldRecord& Record : Scenario.Audit.Records)
		{
			JsonLines += HoldJsonLine(Scenario.Name, Record) + LINE_TERMINATOR;
		}
	}
	const bool bJsonSaved = FFileHelper::SaveStringToFile(JsonLines, *JsonPath);
	const bool bPngSaved = SaveSpatialDistributionPng(PngPath, Scenarios);
	const FString Report = BuildReport(CadenceGates, Scenarios, JsonPath, PngPath, ReplayHash);
	const bool bReportSaved = FFileHelper::SaveStringToFile(Report, *ReportPath);

	bool bAllPass = bJsonSaved && bPngSaved && bReportSaved;
	for (const FCadenceGateResult& Gate : CadenceGates)
	{
		bAllPass = bAllPass && Gate.bPass;
	}
	for (const FScenarioResult& Scenario : Scenarios)
	{
		bAllPass = bAllPass && Scenario.bPass;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("CarrierLabPhaseIIIE64LiveCadencePostMotionMultiHit %s. Report: %s"),
		bAllPass ? TEXT("PASS") : TEXT("FAIL"),
		*ReportPath);
	return bAllPass ? 0 : 1;
}
