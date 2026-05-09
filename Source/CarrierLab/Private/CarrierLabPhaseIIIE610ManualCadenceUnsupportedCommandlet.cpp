// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIE610ManualCadenceUnsupportedCommandlet.h"

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
	constexpr double EditorDefaultContinentalFraction = 0.30;
	constexpr double RayDistanceCoincidenceToleranceKm = 1.0e-9;
	constexpr double ScalarFieldTolerance = 1.0e-9;
	constexpr double ElevationFieldToleranceKm = 1.0e-9;
	constexpr double UnitVectorFieldTolerance = 1.0e-8;

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

	FString BoolText(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	FString PassFail(const bool bPass)
	{
		return bPass ? TEXT("pass") : TEXT("fail");
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

	FString HoldShapeName(const ECarrierLabPhaseIIIE62HoldShape Shape)
	{
		switch (Shape)
		{
		case ECarrierLabPhaseIIIE62HoldShape::TwoPlateSameSourceTriangle:
			return TEXT("two_plate_same_source_triangle");
		case ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalEdge:
			return TEXT("two_plate_shared_global_edge");
		case ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalVertexOnly:
			return TEXT("two_plate_shared_global_vertex_only");
		case ECarrierLabPhaseIIIE62HoldShape::TwoPlateNoSharedGlobalVertices:
			return TEXT("two_plate_no_shared_global_vertices");
		case ECarrierLabPhaseIIIE62HoldShape::TwoPlateFieldMismatch:
			return TEXT("two_plate_field_mismatch");
		case ECarrierLabPhaseIIIE62HoldShape::ThreePlateCommonGlobalVertex:
			return TEXT("three_plate_common_global_vertex");
		case ECarrierLabPhaseIIIE62HoldShape::ThreePlateEdgePlusIntruder:
			return TEXT("three_plate_edge_plus_intruder");
		case ECarrierLabPhaseIIIE62HoldShape::ThreePlateNoCommonSourceVertex:
			return TEXT("three_plate_no_common_source_vertex");
		case ECarrierLabPhaseIIIE62HoldShape::NonBoundaryOrInteriorOverlap:
			return TEXT("non_boundary_or_interior_overlap");
		case ECarrierLabPhaseIIIE62HoldShape::InvalidOrUnclassified:
		default:
			return TEXT("invalid_or_unclassified");
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
				TEXT("IIIE611MixedMaterialNearestHit")));
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
				TEXT("phase-iii-slice-iiie6-11-mixed-material-resolution.md")));
	}

	int32 ShapeIndex(const ECarrierLabPhaseIIIE62HoldShape Shape)
	{
		return static_cast<int32>(Shape);
	}

	int32 BucketIndex(const ECarrierLabPhaseIIIE3MultiHitBucket Bucket)
	{
		return static_cast<int32>(Bucket);
	}

	int32 CountSharedVertices(
		const FCarrierLabPhaseIIIE64CandidateDiagnostic& A,
		const FCarrierLabPhaseIIIE64CandidateDiagnostic& B)
	{
		int32 Count = 0;
		for (const int32 VertexA : A.Snapshot.GlobalVertexIds)
		{
			if (VertexA == INDEX_NONE)
			{
				continue;
			}
			for (const int32 VertexB : B.Snapshot.GlobalVertexIds)
			{
				if (VertexA == VertexB)
				{
					++Count;
					break;
				}
			}
		}
		return Count;
	}

	bool HasCommonVertex(const TArray<FCarrierLabPhaseIIIE64CandidateDiagnostic>& Candidates)
	{
		if (Candidates.Num() < 2)
		{
			return false;
		}
		for (const int32 CandidateVertex : Candidates[0].Snapshot.GlobalVertexIds)
		{
			if (CandidateVertex == INDEX_NONE)
			{
				continue;
			}
			bool bPresentInEveryCandidate = true;
			for (int32 CandidateIndex = 1; CandidateIndex < Candidates.Num(); ++CandidateIndex)
			{
				bool bPresent = false;
				for (const int32 OtherVertex : Candidates[CandidateIndex].Snapshot.GlobalVertexIds)
				{
					if (OtherVertex == CandidateVertex)
					{
						bPresent = true;
						break;
					}
				}
				if (!bPresent)
				{
					bPresentInEveryCandidate = false;
					break;
				}
			}
			if (bPresentInEveryCandidate)
			{
				return true;
			}
		}
		return false;
	}

	ECarrierLabPhaseIIIE62HoldShape ClassifyHoldShape(const FCarrierLabPhaseIIIE64HoldRecord& Record)
	{
		if (Record.Candidates.Num() < 2)
		{
			return ECarrierLabPhaseIIIE62HoldShape::InvalidOrUnclassified;
		}

		bool bAllBoundary = true;
		bool bHasInterior = false;
		bool bFieldMismatch = false;
		TSet<int32> PlateIds;
		TSet<int32> SourceTriangleIds;
		for (const FCarrierLabPhaseIIIE64CandidateDiagnostic& Candidate : Record.Candidates)
		{
			const FCarrierLabPhaseIIIE62CandidateSnapshot& Snapshot = Candidate.Snapshot;
			if (Snapshot.PlateId == INDEX_NONE || Snapshot.LocalTriangleId == INDEX_NONE)
			{
				return ECarrierLabPhaseIIIE62HoldShape::InvalidOrUnclassified;
			}
			PlateIds.Add(Snapshot.PlateId);
			SourceTriangleIds.Add(Snapshot.SourceTriangleId);
			bAllBoundary = bAllBoundary && Snapshot.bBoundary;
			bHasInterior = bHasInterior || Snapshot.BarycentricShape == ECarrierLabPhaseIIIE62BarycentricShape::Interior ||
				Snapshot.BarycentricShape == ECarrierLabPhaseIIIE62BarycentricShape::Unknown;
			bFieldMismatch = bFieldMismatch ||
				Snapshot.ScalarResidual > ScalarFieldTolerance ||
				Snapshot.ElevationResidualKm > ElevationFieldToleranceKm ||
				Snapshot.UnitVectorResidual > UnitVectorFieldTolerance;
		}

		if (!bAllBoundary || bHasInterior)
		{
			return ECarrierLabPhaseIIIE62HoldShape::NonBoundaryOrInteriorOverlap;
		}

		if (PlateIds.Num() == 2)
		{
			if (bFieldMismatch)
			{
				return ECarrierLabPhaseIIIE62HoldShape::TwoPlateFieldMismatch;
			}
			if (SourceTriangleIds.Num() == 1)
			{
				return ECarrierLabPhaseIIIE62HoldShape::TwoPlateSameSourceTriangle;
			}
			int32 MaxSharedVertices = 0;
			for (int32 A = 0; A < Record.Candidates.Num(); ++A)
			{
				for (int32 B = A + 1; B < Record.Candidates.Num(); ++B)
				{
					if (Record.Candidates[A].Snapshot.PlateId == Record.Candidates[B].Snapshot.PlateId)
					{
						continue;
					}
					MaxSharedVertices = FMath::Max(MaxSharedVertices, CountSharedVertices(Record.Candidates[A], Record.Candidates[B]));
				}
			}
			if (MaxSharedVertices >= 2)
			{
				return ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalEdge;
			}
			if (MaxSharedVertices == 1)
			{
				return ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalVertexOnly;
			}
			return ECarrierLabPhaseIIIE62HoldShape::TwoPlateNoSharedGlobalVertices;
		}

		if (PlateIds.Num() >= 3)
		{
			if (HasCommonVertex(Record.Candidates))
			{
				return ECarrierLabPhaseIIIE62HoldShape::ThreePlateCommonGlobalVertex;
			}
			int32 MaxSharedVertices = 0;
			for (int32 A = 0; A < Record.Candidates.Num(); ++A)
			{
				for (int32 B = A + 1; B < Record.Candidates.Num(); ++B)
				{
					if (Record.Candidates[A].Snapshot.PlateId == Record.Candidates[B].Snapshot.PlateId)
					{
						continue;
					}
					MaxSharedVertices = FMath::Max(MaxSharedVertices, CountSharedVertices(Record.Candidates[A], Record.Candidates[B]));
				}
			}
			if (MaxSharedVertices >= 2)
			{
				return ECarrierLabPhaseIIIE62HoldShape::ThreePlateEdgePlusIntruder;
			}
			return ECarrierLabPhaseIIIE62HoldShape::ThreePlateNoCommonSourceVertex;
		}

		return ECarrierLabPhaseIIIE62HoldShape::InvalidOrUnclassified;
	}

	struct FShapeStats
	{
		int32 Counts[10] = {};
	};

	struct FBucketStats
	{
		int32 Counts[7] = {};
	};

	struct FScenario
	{
		FString Name;
		int32 TargetStep = 0;
		bool bAutomatic = false;
		bool bAttemptApply = false;
		bool bStep16HeldAttemptBeforeTarget = false;
		bool bDisableMixedMaterialNearestHit = false;
	};

	struct FScenarioResult
	{
		FString Name;
		int32 TargetStep = 0;
		bool bAutomatic = false;
		bool bAttemptApply = false;
		bool bStep16HeldAttemptBeforeTarget = false;
		bool bDisableMixedMaterialNearestHit = false;
		bool bRan = false;
		bool bPass = false;
		bool bApplied = false;
		int32 StepBefore = 0;
		int32 StepAfter = 0;
		int32 EventCountBefore = 0;
		int32 EventCountAfter = 0;
		int32 NextResampleBefore = 0;
		int32 NextResampleAfter = 0;
		FString ProjectionHashBefore;
		FString ProjectionHashAfter;
		FString StateHashBefore;
		FString StateHashAfter;
		FString CrustHashBefore;
		FString CrustHashAfter;
		FString LastRemeshMode;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIE64PostMotionMultiHitAudit Audit;
		FCarrierLabPhaseIIIE3RemeshSelectionAudit SelectionAudit;
		FShapeStats ShapeStats;
		FBucketStats BucketStats;
		int32 UnsupportedLikeHoldCount = 0;
		int32 ProcessMarkedUnsupportedCount = 0;
		double MaxRayResidualKm = 0.0;
		double MaxScalarResidual = 0.0;
		double MaxElevationResidualKm = 0.0;
		double MaxUnitVectorResidual = 0.0;
		FString ScenarioHash;
	};

	ACarrierLabVisualizationActor* SpawnEditorDefaultActor(
		UWorld& World,
		const bool bAutomatic,
		const bool bDisableMixedMaterialNearestHit)
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
		Actor->ContinentalPlateFraction = EditorDefaultContinentalFraction;
		Actor->VelocityMmPerYear = DefaultVelocityMmPerYear;
		Actor->bEnableNaturalResamplingEvents = bAutomatic;
		Actor->bEnablePhaseIIIE3DuplicateHitCoalescing = true;
		Actor->bEnablePhaseIIIE3SharedBoundaryTieBreak = true;
		Actor->bEnablePhaseIIIE3NearestHitTieBreak = true;
		Actor->bExtendPhaseIIIE3NearestHitToMixedMaterial = !bDisableMixedMaterialNearestHit;
		Actor->bEnablePhaseIIIE3DistanceTieFallback = true;
		Actor->bRestoreNonSeparatingAnomalyVeto = false;
		Actor->ConfigurePhaseIIICProcessLayer(true, false);
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	void AccumulateHoldDiagnostics(const FCarrierLabPhaseIIIE64HoldRecord& Record, FScenarioResult& Result)
	{
		const ECarrierLabPhaseIIIE62HoldShape Shape = ClassifyHoldShape(Record);
		Result.ShapeStats.Counts[ShapeIndex(Shape)]++;
		Result.BucketStats.Counts[BucketIndex(Record.MultiHitBucket)]++;
		if (Record.bAnyCandidateProcessMarked)
		{
			++Result.ProcessMarkedUnsupportedCount;
		}
		for (const FCarrierLabPhaseIIIE64CandidateDiagnostic& Candidate : Record.Candidates)
		{
			Result.MaxRayResidualKm = FMath::Max(Result.MaxRayResidualKm, Candidate.Snapshot.RayDistanceResidualKm);
			Result.MaxScalarResidual = FMath::Max(Result.MaxScalarResidual, Candidate.Snapshot.ScalarResidual);
			Result.MaxElevationResidualKm = FMath::Max(Result.MaxElevationResidualKm, Candidate.Snapshot.ElevationResidualKm);
			Result.MaxUnitVectorResidual = FMath::Max(Result.MaxUnitVectorResidual, Candidate.Snapshot.UnitVectorResidual);
		}
	}

	uint64 ComputeScenarioHash(const FScenarioResult& Result)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMix(Hash, static_cast<uint64>(Result.TargetStep + 1));
		HashMix(Hash, Result.bAutomatic ? 3ull : 1ull);
		HashMix(Hash, Result.bAttemptApply ? 5ull : 1ull);
		HashMix(Hash, Result.bDisableMixedMaterialNearestHit ? 7ull : 1ull);
		HashMix(Hash, static_cast<uint64>(Result.Audit.SelectionUnresolvedMultiHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.Audit.SelectionCrossPlateDifferentMultiHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.Audit.SelectionThirdPlateMultiHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.Audit.SelectionCrossPlateEqualMultiHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.SelectionAudit.NearestHitCrossPlateDifferentResolvedCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.SelectionAudit.NearestHitThirdPlateResolvedCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.SelectionAudit.NearestHitMixedMaterialResolvedCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.SelectionAudit.NearestHitDistanceTieHeldCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.SelectionAudit.NearestHitUnsupportedHeldCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.SelectionAudit.DistanceTieFallbackCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.SelectionAudit.DistanceTieFallbackMixedMaterialCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.UnsupportedLikeHoldCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.ProcessMarkedUnsupportedCount + 1));
		for (const int32 Count : Result.ShapeStats.Counts)
		{
			HashMix(Hash, static_cast<uint64>(Count + 1));
		}
		for (const int32 Count : Result.BucketStats.Counts)
		{
			HashMix(Hash, static_cast<uint64>(Count + 1));
		}
		HashMixDouble(Hash, Result.MaxRayResidualKm);
		HashMixDouble(Hash, Result.MaxScalarResidual);
		HashMixDouble(Hash, Result.MaxElevationResidualKm);
		HashMixDouble(Hash, Result.MaxUnitVectorResidual);
		for (const TCHAR Ch : Result.Audit.SelectionHash)
		{
			HashMix(Hash, static_cast<uint64>(Ch));
		}
		for (const TCHAR Ch : Result.SelectionAudit.SelectionHash)
		{
			HashMix(Hash, static_cast<uint64>(Ch));
		}
		for (const TCHAR Ch : Result.Audit.DiagnosisHash)
		{
			HashMix(Hash, static_cast<uint64>(Ch));
		}
		return Hash;
	}

	void AnalyzeScenarioRecords(FScenarioResult& Result)
	{
		Result.ShapeStats = FShapeStats();
		Result.BucketStats = FBucketStats();
		Result.UnsupportedLikeHoldCount = 0;
		Result.ProcessMarkedUnsupportedCount = 0;
		Result.MaxRayResidualKm = 0.0;
		Result.MaxScalarResidual = 0.0;
		Result.MaxElevationResidualKm = 0.0;
		Result.MaxUnitVectorResidual = 0.0;
		for (const FCarrierLabPhaseIIIE64HoldRecord& Record : Result.Audit.Records)
		{
			++Result.UnsupportedLikeHoldCount;
			AccumulateHoldDiagnostics(Record, Result);
		}
		Result.ScenarioHash = HashToString(ComputeScenarioHash(Result));
	}

	void RunScenario(UWorld& World, const FScenario& Scenario, FScenarioResult& OutResult)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("CarrierLabPhaseIIIE611MixedMaterialNearestHit: scenario start %s step=%d auto=%d apply=%d step16_hold_before_target=%d mixed_extension_disabled=%d"),
			*Scenario.Name,
			Scenario.TargetStep,
			Scenario.bAutomatic ? 1 : 0,
			Scenario.bAttemptApply ? 1 : 0,
			Scenario.bStep16HeldAttemptBeforeTarget ? 1 : 0,
			Scenario.bDisableMixedMaterialNearestHit ? 1 : 0);

		OutResult = FScenarioResult();
		OutResult.Name = Scenario.Name;
		OutResult.TargetStep = Scenario.TargetStep;
		OutResult.bAutomatic = Scenario.bAutomatic;
		OutResult.bAttemptApply = Scenario.bAttemptApply;
		OutResult.bStep16HeldAttemptBeforeTarget = Scenario.bStep16HeldAttemptBeforeTarget;
		OutResult.bDisableMixedMaterialNearestHit = Scenario.bDisableMixedMaterialNearestHit;
		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnEditorDefaultActor(
			World,
			Scenario.bAutomatic,
			Scenario.bDisableMixedMaterialNearestHit);
		if (Actor == nullptr || !Actor->InitializeCarrier())
		{
			if (Actor != nullptr)
			{
				Actor->Destroy();
			}
			return;
		}

		if (Scenario.bStep16HeldAttemptBeforeTarget)
		{
			for (int32 StepIndex = 0; StepIndex < 16; ++StepIndex)
			{
				Actor->StepOnce();
			}
			(void)Actor->ApplyPhaseIIIELiveRemeshEvent();
			for (int32 StepIndex = 16; StepIndex < Scenario.TargetStep; ++StepIndex)
			{
				Actor->StepOnce();
			}
		}
		else
		{
			const int32 WarmupSteps = Scenario.bAutomatic ? Scenario.TargetStep - 1 : Scenario.TargetStep;
			for (int32 StepIndex = 0; StepIndex < WarmupSteps; ++StepIndex)
			{
				Actor->StepOnce();
			}
		}

		OutResult.StepBefore = Actor->CurrentMetrics.Step;
		OutResult.EventCountBefore = Actor->CurrentMetrics.EventCount;
		OutResult.NextResampleBefore = Actor->CurrentMetrics.NextResampleStep;
		OutResult.ProjectionHashBefore = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashBefore = Actor->CurrentMetrics.StateHash;
		OutResult.CrustHashBefore = Actor->CurrentMetrics.CrustStateHash;

		if (Scenario.bAutomatic)
		{
			Actor->StepOnce();
			OutResult.bApplied = Actor->CurrentMetrics.EventCount > OutResult.EventCountBefore;
			OutResult.StepAfter = Actor->CurrentMetrics.Step;
			OutResult.EventCountAfter = Actor->CurrentMetrics.EventCount;
			OutResult.NextResampleAfter = Actor->CurrentMetrics.NextResampleStep;
			OutResult.ProjectionHashAfter = Actor->CurrentMetrics.LastHash;
			OutResult.StateHashAfter = Actor->CurrentMetrics.StateHash;
			OutResult.CrustHashAfter = Actor->CurrentMetrics.CrustStateHash;
			OutResult.LastRemeshMode = Actor->CurrentMetrics.LastRemeshMode;
			Actor->RunPhaseIIIE3FilteredRemeshSelectionAudit(OutResult.SelectionAudit);
			OutResult.bRan = Actor->RunPhaseIIIE64PostMotionMultiHitDiagnosisAudit(OutResult.Audit);
		}
		else
		{
			Actor->RunPhaseIIIE3FilteredRemeshSelectionAudit(OutResult.SelectionAudit);
			OutResult.bRan = Actor->RunPhaseIIIE64PostMotionMultiHitDiagnosisAudit(OutResult.Audit);
			if (Scenario.bAttemptApply)
			{
				UE_LOG(
					LogTemp,
					Display,
					TEXT("CarrierLabPhaseIIIE611MixedMaterialNearestHit: apply probe begin %s unresolved=%d unsupported=%d nearest_mixed=%d dtie_mixed=%d"),
					*Scenario.Name,
					OutResult.SelectionAudit.UnresolvedMultiHitCount,
					OutResult.SelectionAudit.NearestHitUnsupportedHeldCount,
					OutResult.SelectionAudit.NearestHitMixedMaterialResolvedCount,
					OutResult.SelectionAudit.DistanceTieFallbackMixedMaterialCount);
				OutResult.bApplied = Actor->ApplyPhaseIIIELiveRemeshEvent();
			}
			OutResult.StepAfter = Actor->CurrentMetrics.Step;
			OutResult.EventCountAfter = Actor->CurrentMetrics.EventCount;
			OutResult.NextResampleAfter = Actor->CurrentMetrics.NextResampleStep;
			OutResult.ProjectionHashAfter = Actor->CurrentMetrics.LastHash;
			OutResult.StateHashAfter = Actor->CurrentMetrics.StateHash;
			OutResult.CrustHashAfter = Actor->CurrentMetrics.CrustStateHash;
			OutResult.LastRemeshMode = Scenario.bAttemptApply ? Actor->CurrentMetrics.LastRemeshMode : TEXT("selection_only");
		}

		AnalyzeScenarioRecords(OutResult);
		const bool bForbiddenZero =
			OutResult.Audit.PolicyWinnerCount == 0 &&
			OutResult.Audit.PriorOwnerFallbackCount == 0 &&
			OutResult.Audit.ProjectionAuthorityCount == 0;
		const bool bSelectionClosed =
			OutResult.SelectionAudit.UnresolvedMultiHitCount == 0 &&
			OutResult.SelectionAudit.NearestHitUnsupportedHeldCount == 0 &&
			OutResult.Audit.SelectionUnresolvedMultiHitCount == 0 &&
			OutResult.UnsupportedLikeHoldCount == 0;
		const bool bLiveApplySucceeded =
			!Scenario.bAttemptApply ||
			(OutResult.bApplied &&
				OutResult.EventCountAfter == OutResult.EventCountBefore + 1 &&
				OutResult.LastRemeshMode.StartsWith(TEXT("phase_iii_e6_live_apply")));
		const bool bBaselineHoldModeHonest =
			!Scenario.bAttemptApply ||
			OutResult.LastRemeshMode.StartsWith(TEXT("phase_iii_e6_live_hold_unresolved_multi_hit"));
		const bool bBaselineNoMutation =
			!Scenario.bAttemptApply ||
			(OutResult.EventCountAfter == OutResult.EventCountBefore &&
				OutResult.ProjectionHashAfter == OutResult.ProjectionHashBefore &&
				OutResult.StateHashAfter == OutResult.StateHashBefore &&
				OutResult.CrustHashAfter == OutResult.CrustHashBefore);
		const bool bBaselineReproduced =
			Scenario.bDisableMixedMaterialNearestHit &&
			OutResult.bRan &&
			bForbiddenZero &&
			OutResult.Audit.DiagnosedHoldCount == OutResult.Audit.SelectionUnresolvedMultiHitCount &&
			OutResult.UnsupportedLikeHoldCount == OutResult.Audit.SelectionUnresolvedMultiHitCount &&
			OutResult.UnsupportedLikeHoldCount == OutResult.SelectionAudit.NearestHitUnsupportedHeldCount &&
			OutResult.ProcessMarkedUnsupportedCount == 0 &&
			bBaselineNoMutation &&
			bBaselineHoldModeHonest;
		OutResult.bPass = Scenario.bDisableMixedMaterialNearestHit
			? bBaselineReproduced
			: (OutResult.bRan && bForbiddenZero && bSelectionClosed && bLiveApplySucceeded);
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		UE_LOG(
			LogTemp,
			Display,
			TEXT("CarrierLabPhaseIIIE611MixedMaterialNearestHit: scenario done %s unresolved=%d unsupported=%d nearest_mixed=%d dtie_mixed=%d pass=%d seconds=%.3f"),
			*Scenario.Name,
			OutResult.Audit.SelectionUnresolvedMultiHitCount,
			OutResult.UnsupportedLikeHoldCount,
			OutResult.SelectionAudit.NearestHitMixedMaterialResolvedCount,
			OutResult.SelectionAudit.DistanceTieFallbackMixedMaterialCount,
			OutResult.bPass ? 1 : 0,
			OutResult.Seconds);
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
	}

	FString CandidateJson(const FCarrierLabPhaseIIIE64CandidateDiagnostic& Candidate)
	{
		return FString::Printf(
			TEXT("{\"candidate_index\":%d,\"plate_id\":%d,\"local_triangle_id\":%d,\"source_triangle_id\":%d,\"global_vertices\":[%d,%d,%d],\"barycentric_shape\":%s,\"boundary\":%s,\"distance\":%.17g,\"ray_residual_km\":%.17g,\"scalar_residual\":%.17g,\"elevation_residual_km\":%.17g,\"unit_vector_residual\":%.17g,\"subducting\":%s,\"obduction_pending\":%s,\"collision_pending\":%s,\"nearest\":%s,\"plate_continental_fraction\":%.17g,\"plate_oceanic_age\":%.17g}"),
			Candidate.Snapshot.CandidateIndex,
			Candidate.Snapshot.PlateId,
			Candidate.Snapshot.LocalTriangleId,
			Candidate.Snapshot.SourceTriangleId,
			Candidate.Snapshot.GlobalVertexIds[0],
			Candidate.Snapshot.GlobalVertexIds[1],
			Candidate.Snapshot.GlobalVertexIds[2],
			*JsonString(BarycentricShapeName(Candidate.Snapshot.BarycentricShape)),
			*BoolText(Candidate.Snapshot.bBoundary),
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

	FString HoldJsonLine(const FScenarioResult& Scenario, const FCarrierLabPhaseIIIE64HoldRecord& Record)
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
		const ECarrierLabPhaseIIIE62HoldShape Shape = ClassifyHoldShape(Record);
		return FString::Printf(
			TEXT("{\"type\":\"hold\",\"scenario\":%s,\"step\":%d,\"sample_id\":%d,\"bucket\":%s,\"shape\":%s,\"candidate_count\":%d,\"distinct_plate_count\":%d,\"sample_unit\":[%.17g,%.17g,%.17g],\"unique_nearest\":%s,\"nearest_distance_tie\":%s,\"nearest_gap_km\":%.17g,\"any_process_marked\":%s,\"subducting_marked\":%s,\"obduction_pending_marked\":%s,\"collision_pending_marked\":%s,\"nearest_more_continental\":%s,\"nearest_older_oceanic\":%s,\"nearest_lower_plate_id\":%s,\"continental_tie\":%s,\"oceanic_age_tie\":%s,\"candidates\":[%s]}"),
			*JsonString(Scenario.Name),
			Record.Step,
			Record.SampleId,
			*JsonString(BucketName(Record.MultiHitBucket)),
			*JsonString(HoldShapeName(Shape)),
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
			TEXT("{\"type\":\"scenario\",\"scenario\":%s,\"pass\":%s,\"automatic\":%s,\"attempt_apply\":%s,\"step16_held_attempt_before_target\":%s,\"mixed_material_extension_disabled\":%s,\"target_step\":%d,\"step_before\":%d,\"step_after\":%d,\"events_before\":%d,\"events_after\":%d,\"next_before\":%d,\"next_after\":%d,\"unresolved\":%d,\"unsupported_like_holds\":%d,\"cross_plate_different\":%d,\"third_plate\":%d,\"cross_plate_equal\":%d,\"mixed_material_hold_bucket\":%d,\"mixed_material_selection_bucket\":%d,\"coalesced\":%d,\"shared_tiebreak\":%d,\"nearest_cross\":%d,\"nearest_third\":%d,\"nearest_mixed\":%d,\"nearest_tie\":%d,\"nearest_unsupported\":%d,\"distance_tie_fallback\":%d,\"distance_tie_fallback_mixed\":%d,\"process_marked\":%d,\"shape_edge\":%d,\"shape_vertex\":%d,\"shape_field_mismatch\":%d,\"shape_common_vertex\":%d,\"shape_non_boundary\":%d,\"max_ray_residual_km\":%.17g,\"max_scalar_residual\":%.17g,\"max_elevation_residual_km\":%.17g,\"max_unit_vector_residual\":%.17g,\"selection_hash\":%s,\"diagnosis_hash\":%s,\"scenario_hash\":%s,\"last_remesh_mode\":%s,\"seconds\":%.6f}"),
			*JsonString(Result.Name),
			*BoolText(Result.bPass),
			*BoolText(Result.bAutomatic),
			*BoolText(Result.bAttemptApply),
			*BoolText(Result.bStep16HeldAttemptBeforeTarget),
			*BoolText(Result.bDisableMixedMaterialNearestHit),
			Result.TargetStep,
			Result.StepBefore,
			Result.StepAfter,
			Result.EventCountBefore,
			Result.EventCountAfter,
			Result.NextResampleBefore,
			Result.NextResampleAfter,
			Result.Audit.SelectionUnresolvedMultiHitCount,
			Result.UnsupportedLikeHoldCount,
			Result.Audit.SelectionCrossPlateDifferentMultiHitCount,
			Result.Audit.SelectionThirdPlateMultiHitCount,
			Result.Audit.SelectionCrossPlateEqualMultiHitCount,
			Result.BucketStats.Counts[BucketIndex(ECarrierLabPhaseIIIE3MultiHitBucket::MixedMaterial)],
			Result.SelectionAudit.MixedMaterialMultiHitCount,
			Result.Audit.CoalescedMultiHitCount,
			Result.Audit.SharedBoundaryTieBreakCount,
			Result.SelectionAudit.NearestHitCrossPlateDifferentResolvedCount,
			Result.SelectionAudit.NearestHitThirdPlateResolvedCount,
			Result.SelectionAudit.NearestHitMixedMaterialResolvedCount,
			Result.SelectionAudit.NearestHitDistanceTieHeldCount,
			Result.SelectionAudit.NearestHitUnsupportedHeldCount,
			Result.SelectionAudit.DistanceTieFallbackCount,
			Result.SelectionAudit.DistanceTieFallbackMixedMaterialCount,
			Result.ProcessMarkedUnsupportedCount,
			Result.ShapeStats.Counts[ShapeIndex(ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalEdge)],
			Result.ShapeStats.Counts[ShapeIndex(ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalVertexOnly)],
			Result.ShapeStats.Counts[ShapeIndex(ECarrierLabPhaseIIIE62HoldShape::TwoPlateFieldMismatch)],
			Result.ShapeStats.Counts[ShapeIndex(ECarrierLabPhaseIIIE62HoldShape::ThreePlateCommonGlobalVertex)],
			Result.ShapeStats.Counts[ShapeIndex(ECarrierLabPhaseIIIE62HoldShape::NonBoundaryOrInteriorOverlap)],
			Result.MaxRayResidualKm,
			Result.MaxScalarResidual,
			Result.MaxElevationResidualKm,
			Result.MaxUnitVectorResidual,
			*JsonString(Result.SelectionAudit.SelectionHash),
			*JsonString(Result.Audit.DiagnosisHash),
			*JsonString(Result.ScenarioHash),
			*JsonString(Result.LastRemeshMode),
			Result.Seconds);
	}

	FString BuildReport(const TArray<FScenarioResult>& Results, const FString& JsonPath)
	{
		bool bAllPass = true;
		int32 MaxPromotedUnsupported = 0;
		int32 MaxPromotedMixedHoldBucket = 0;
		int32 MaxPromotedMixedSelectionBucket = 0;
		int32 MaxPromotedProcessMarked = 0;
		int32 MaxBaselineUnsupported = 0;
		for (const FScenarioResult& Result : Results)
		{
			bAllPass = bAllPass && Result.bPass;
			if (Result.bDisableMixedMaterialNearestHit)
			{
				MaxBaselineUnsupported = FMath::Max(MaxBaselineUnsupported, Result.UnsupportedLikeHoldCount);
			}
			else
			{
				MaxPromotedUnsupported = FMath::Max(MaxPromotedUnsupported, Result.UnsupportedLikeHoldCount);
				MaxPromotedMixedHoldBucket = FMath::Max(
					MaxPromotedMixedHoldBucket,
					Result.BucketStats.Counts[BucketIndex(ECarrierLabPhaseIIIE3MultiHitBucket::MixedMaterial)]);
				MaxPromotedMixedSelectionBucket = FMath::Max(
					MaxPromotedMixedSelectionBucket,
					Result.SelectionAudit.MixedMaterialMultiHitCount);
				MaxPromotedProcessMarked = FMath::Max(MaxPromotedProcessMarked, Result.ProcessMarkedUnsupportedCount);
			}
		}

		FString Report;
		Report += TEXT("# Phase IIIE.6.11 Mixed-Material Nearest-Hit Resolution\n\n");
		Report += FString::Printf(
			TEXT("**Verdict:** %s. IIIE.6.11 amends the existing nearest-hit lab policy to include mixed-material strict unique-nearest records, while mixed-material distance ties flow through the existing IIIE.6.6 fallback hierarchy. Baseline rows keep the IIIE.6.10 hold reproducible with the extension disabled.\n\n"),
			bAllPass ? TEXT("PASS") : TEXT("FAIL"));

		Report += TEXT("## Scope\n\n");
		Report += TEXT("- IIIE.6.11 changes remesh source selection semantics only for the existing nearest-hit lab policy's mixed-material bucket coverage.\n");
		Report += TEXT("- The diagnostic uses the live actor editor defaults that exposed the problem: `100000` samples, `40` plates, seed `42`, speed `66.6666666667 mm/yr`, and `ContinentalPlateFraction = 0.30`.\n");
		Report += TEXT("- Earlier IIIE.6.4/6.5/6.6 default commandlets forced `ContinentalPlateFraction = 0.0`, which made them all-ocean harnesses and did not cover the mixed-material editor path visible in the live actor.\n");
		Report += TEXT("- With `bExtendPhaseIIIE3NearestHitToMixedMaterial = false`, held remesh remains honest: event count and hashes do not change on manual hold, and the mode string stays `phase_iii_e6_live_hold_unresolved_multi_hit_*`.\n\n");
		Report += TEXT("- The default commandlet runs a cadence selection sweep. Passing `-ApplyProbeStep=N` adds one default-scale manual live-apply proof at that step; `-AutoApplyProbe` adds the natural-cadence apply proof; `-FullApplySweep` upgrades every swept manual row to live apply. Use the monitored runner for apply modes.\n\n");

		Report += TEXT("## Default Parity Gate\n\n");
		Report += TEXT("| Field | Value | Gate |\n");
		Report += TEXT("|---|---:|---|\n");
		Report += FString::Printf(TEXT("| SampleCount | `%d` | editor-default live scale |\n"), DefaultSampleCount);
		Report += FString::Printf(TEXT("| PlateCount | `%d` | editor-default live scale |\n"), DefaultPlateCount);
		Report += FString::Printf(TEXT("| Seed | `%d` | editor-default seed |\n"), DefaultSeed);
		Report += FString::Printf(TEXT("| ContinentalPlateFraction | `%.2f` | editor default; `0.0` is ocean-only negative/control coverage, not default |\n"), EditorDefaultContinentalFraction);
		Report += FString::Printf(TEXT("| VelocityMmPerYear | `%.10g` | observed-speed default used by the workbench |\n"), DefaultVelocityMmPerYear);
		Report += TEXT("| Phase III process layer | `on` | matches workbench default; slab pull remains off |\n");
		Report += TEXT("| IIIE resolvers | `coalescing/shared/nearest/distance-tie on` | audited IIIE.6 selection chain, not Stage 1.5 |\n");
		Report += TEXT("| Mixed-material nearest-hit extension | `on` for promoted rows; `off` for baseline rows | preserves IIIE.6.10 evidence while closing live default path |\n");
		Report += TEXT("| Non-separating veto | `off` | IIIE.6.9 paper-literal zero-hit generation restored |\n\n");

		Report += TEXT("## Cadence Sweep\n\n");
		Report += TEXT("| Scenario | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		for (const FScenarioResult& Result : Results)
		{
			Report += FString::Printf(
				TEXT("| %s | %s | auto `%d`, apply `%d`, extension_disabled `%d`, step `%d->%d`, events `%d->%d`, next `%d->%d`, unresolved `%d`, unsupported `%d`, buckets cross/third/equal/mixed `%d/%d/%d/%d`, nearest cross/third/mixed/tie/unsupported `%d/%d/%d/%d/%d`, dtie fallback total/mixed `%d/%d`, process `%d`, mode `%s`, hash `%s`, %.2fs |\n"),
				*Result.Name,
				*PassFail(Result.bPass),
				Result.bAutomatic ? 1 : 0,
				Result.bAttemptApply ? 1 : 0,
				Result.bDisableMixedMaterialNearestHit ? 1 : 0,
				Result.StepBefore,
				Result.StepAfter,
				Result.EventCountBefore,
				Result.EventCountAfter,
				Result.NextResampleBefore,
				Result.NextResampleAfter,
				Result.Audit.SelectionUnresolvedMultiHitCount,
				Result.UnsupportedLikeHoldCount,
				Result.Audit.SelectionCrossPlateDifferentMultiHitCount,
				Result.Audit.SelectionThirdPlateMultiHitCount,
				Result.Audit.SelectionCrossPlateEqualMultiHitCount,
				Result.BucketStats.Counts[BucketIndex(ECarrierLabPhaseIIIE3MultiHitBucket::MixedMaterial)],
				Result.SelectionAudit.NearestHitCrossPlateDifferentResolvedCount,
				Result.SelectionAudit.NearestHitThirdPlateResolvedCount,
				Result.SelectionAudit.NearestHitMixedMaterialResolvedCount,
				Result.SelectionAudit.NearestHitDistanceTieHeldCount,
				Result.SelectionAudit.NearestHitUnsupportedHeldCount,
				Result.SelectionAudit.DistanceTieFallbackCount,
				Result.SelectionAudit.DistanceTieFallbackMixedMaterialCount,
				Result.ProcessMarkedUnsupportedCount,
				*Result.LastRemeshMode,
				*Result.ScenarioHash,
				Result.Seconds);
		}
		Report += TEXT("\n");

		Report += TEXT("## Unsupported Shape Distribution\n\n");
		Report += TEXT("| Scenario | same source | shared edge | shared vertex | no shared vertices | field mismatch | three common vertex | three edge intruder | three no common | non-boundary/interior | invalid |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FScenarioResult& Result : Results)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d |\n"),
				*Result.Name,
				Result.ShapeStats.Counts[ShapeIndex(ECarrierLabPhaseIIIE62HoldShape::TwoPlateSameSourceTriangle)],
				Result.ShapeStats.Counts[ShapeIndex(ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalEdge)],
				Result.ShapeStats.Counts[ShapeIndex(ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalVertexOnly)],
				Result.ShapeStats.Counts[ShapeIndex(ECarrierLabPhaseIIIE62HoldShape::TwoPlateNoSharedGlobalVertices)],
				Result.ShapeStats.Counts[ShapeIndex(ECarrierLabPhaseIIIE62HoldShape::TwoPlateFieldMismatch)],
				Result.ShapeStats.Counts[ShapeIndex(ECarrierLabPhaseIIIE62HoldShape::ThreePlateCommonGlobalVertex)],
				Result.ShapeStats.Counts[ShapeIndex(ECarrierLabPhaseIIIE62HoldShape::ThreePlateEdgePlusIntruder)],
				Result.ShapeStats.Counts[ShapeIndex(ECarrierLabPhaseIIIE62HoldShape::ThreePlateNoCommonSourceVertex)],
				Result.ShapeStats.Counts[ShapeIndex(ECarrierLabPhaseIIIE62HoldShape::NonBoundaryOrInteriorOverlap)],
				Result.ShapeStats.Counts[ShapeIndex(ECarrierLabPhaseIIIE62HoldShape::InvalidOrUnclassified)]);
		}
		Report += TEXT("\n");

		Report += TEXT("## Process And Field Cross-Reference\n\n");
		Report += TEXT("| Scenario | process-marked | max ray residual km | max scalar residual | max elevation residual km | max unit-vector residual |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|\n");
		for (const FScenarioResult& Result : Results)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %.6g | %.6g | %.6g | %.6g |\n"),
				*Result.Name,
				Result.ProcessMarkedUnsupportedCount,
				Result.MaxRayResidualKm,
				Result.MaxScalarResidual,
				Result.MaxElevationResidualKm,
				Result.MaxUnitVectorResidual);
		}
		Report += TEXT("\n");

		Report += TEXT("## Interpretation\n\n");
		Report += FString::Printf(
			TEXT("- Max promoted-row unsupported count across the sweep: `%d`.\n- Max promoted-row held mixed-material bucket count across the sweep: `%d`.\n- Max promoted-row mixed-material selection bucket count resolved by the amendment: `%d`.\n- Max promoted-row process-marked unsupported count across the sweep: `%d`.\n- Max extension-disabled baseline unsupported count: `%d`.\n"),
			MaxPromotedUnsupported,
			MaxPromotedMixedHoldBucket,
			MaxPromotedMixedSelectionBucket,
			MaxPromotedProcessMarked,
			MaxBaselineUnsupported);
		Report += TEXT("- Because the harness now uses `ContinentalPlateFraction = 0.30`, IIIE.6.11 directly covers the live editor path from the screenshots rather than the older all-ocean diagnostic path.\n");
		Report += TEXT("- The promoted rows must show `unsupported == 0`, `unresolved == 0`, and live apply mode. The extension-disabled rows must reproduce the IIIE.6.10 mixed-material hold as historical evidence.\n");
		Report += TEXT("- Mixed-material nearest-hit remains an amendment to the existing nearest-hit lab policy, not a new paper-cited rule and not an eighth IIIE lab policy.\n\n");

		Report += TEXT("## Stop Conditions Preserved\n\n");
		Report += TEXT("- Stop if any promoted-row unsupported record remains after the amendment.\n");
		Report += TEXT("- Stop if the extension-disabled baseline mutates topology, uses prior-owner fallback, uses projection-derived authority, or routes through Stage 1.5.\n");
		Report += TEXT("- Stop if a baseline held remesh increments events or changes projection/state/crust hashes.\n");
		Report += TEXT("- Stop if process-state marks explain a substantial fraction of any remaining unsupported records; that would redirect to IIIB/IIIC/IIID marking rather than remesh tie-break policy.\n\n");

		Report += FString::Printf(TEXT("## Artifacts\n\n- JSONL metrics: `%s`\n"), *JsonPath);
		return Report;
	}
}

UCarrierLabPhaseIIIE610ManualCadenceUnsupportedCommandlet::UCarrierLabPhaseIIIE610ManualCadenceUnsupportedCommandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIE610ManualCadenceUnsupportedCommandlet::Main(const FString& Params)
{
	UWorld* World = GetCommandletWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPhaseIIIE610ManualCadenceUnsupported: no world available"));
		return 1;
	}

	const FString OutputRoot = GetOutputRoot(Params);
	const FString JsonPath = FPaths::Combine(OutputRoot, TEXT("phase-iii-slice-iiie6-11-mixed-material-resolution.jsonl"));
	const FString ReportPath = GetReportPath(Params);
	const bool bFullApplySweep = FParse::Param(*Params, TEXT("FullApplySweep"));
	const bool bAutoApplyProbe = FParse::Param(*Params, TEXT("AutoApplyProbe"));
	const bool bPostStep16ApplyProbe = FParse::Param(*Params, TEXT("PostStep16ApplyProbe"));
	const bool bBaselineApply = FParse::Param(*Params, TEXT("BaselineApply"));
	const bool bOnlyApplyProbe = FParse::Param(*Params, TEXT("OnlyApplyProbe"));
	int32 ApplyProbeStep = INDEX_NONE;
	FParse::Value(*Params, TEXT("ApplyProbeStep="), ApplyProbeStep);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);

	auto ShouldApplyStep = [bFullApplySweep, ApplyProbeStep](const int32 Step)
	{
		return bFullApplySweep || ApplyProbeStep == Step;
	};

	TArray<FScenario> Scenarios;
	const int32 SweepSteps[] = { 1, 8, 15, 16, 17, 20, 24, 32, 33, 40, 48, 60 };
	if (bOnlyApplyProbe && ApplyProbeStep > 0)
	{
		Scenarios.Add({
			FString::Printf(TEXT("manual_step_%d_apply_probe_only"), ApplyProbeStep),
			ApplyProbeStep,
			false,
			true,
			false,
			false });
	}
	else
	{
		for (const int32 Step : SweepSteps)
		{
			const bool bApply = ShouldApplyStep(Step);
			Scenarios.Add({
				FString::Printf(TEXT("manual_step_%d_%s"), Step, bApply ? TEXT("apply_probe") : TEXT("selection")),
				Step,
				false,
				bApply,
				false,
				false });
		}
		Scenarios.Add({ TEXT("manual_step_32_replay_selection"), 32, false, false, false, false });
		if (bPostStep16ApplyProbe)
		{
			Scenarios.Add({ TEXT("manual_step_32_after_step16_apply_probe"), 32, false, true, true, false });
		}
		if (bAutoApplyProbe)
		{
			Scenarios.Add({ TEXT("auto_cadence_step_32_apply_probe"), 32, true, true, false, false });
		}
		Scenarios.Add({ bBaselineApply ? TEXT("baseline_step_16_mixed_material_extension_disabled_apply") : TEXT("baseline_step_16_mixed_material_extension_disabled_selection"), 16, false, bBaselineApply, false, true });
		Scenarios.Add({ bBaselineApply ? TEXT("baseline_step_32_mixed_material_extension_disabled_apply") : TEXT("baseline_step_32_mixed_material_extension_disabled_selection"), 32, false, bBaselineApply, false, true });
	}

	TArray<FScenarioResult> Results;
	Results.Reserve(Scenarios.Num());
	for (const FScenario& Scenario : Scenarios)
	{
		FScenarioResult& Result = Results.AddDefaulted_GetRef();
		RunScenario(*World, Scenario, Result);
	}

	bool bSameSeedReplay = bOnlyApplyProbe;
	const FScenarioResult* Step32Primary = nullptr;
	const FScenarioResult* Step32Replay = nullptr;
	for (const FScenarioResult& Result : Results)
	{
		if (Result.TargetStep == 32 &&
			!Result.bAutomatic &&
			!Result.bStep16HeldAttemptBeforeTarget &&
			!Result.bDisableMixedMaterialNearestHit &&
			Step32Primary == nullptr)
		{
			Step32Primary = &Result;
		}
		else if (Result.Name == TEXT("manual_step_32_replay_selection"))
		{
			Step32Replay = &Result;
		}
	}
	if (Step32Primary != nullptr && Step32Replay != nullptr)
	{
		bSameSeedReplay =
			Step32Primary->SelectionAudit.SelectionHash == Step32Replay->SelectionAudit.SelectionHash &&
			Step32Primary->Audit.DiagnosisHash == Step32Replay->Audit.DiagnosisHash &&
			Step32Primary->UnsupportedLikeHoldCount == Step32Replay->UnsupportedLikeHoldCount;
	}

	FString JsonLines;
	for (const FScenarioResult& Result : Results)
	{
		JsonLines += ScenarioJsonLine(Result) + LINE_TERMINATOR;
		for (const FCarrierLabPhaseIIIE64HoldRecord& Record : Result.Audit.Records)
		{
			JsonLines += HoldJsonLine(Result, Record) + LINE_TERMINATOR;
		}
	}
	FFileHelper::SaveStringToFile(JsonLines, *JsonPath);

	FString Report = BuildReport(Results, JsonPath);
	Report += FString::Printf(
		TEXT("\n## Replay Check\n\n- Manual step-32 primary row vs selection replay: `%s`.\n"),
		bSameSeedReplay ? TEXT("selection and diagnosis hashes match") : TEXT("MISMATCH"));
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	bool bAllPass = bSameSeedReplay;
	for (const FScenarioResult& Result : Results)
	{
		bAllPass = bAllPass && Result.bPass;
	}
	UE_LOG(
		LogTemp,
		Display,
		TEXT("CarrierLabPhaseIIIE610ManualCadenceUnsupported: report=%s json=%s pass=%d"),
		*ReportPath,
		*JsonPath,
		bAllPass ? 1 : 0);
	return bAllPass ? 0 : 1;
}
