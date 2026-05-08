// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIE62CrossPlateMultiHitCommandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr int32 DefaultSampleCount = 100000;
	constexpr int32 DefaultPlateCount = 40;
	constexpr int32 ExpectedDefaultUnresolvedHoldCount = 5845;
	constexpr int32 ExpectedDefaultCrossPlateEqualCount = 5761;
	constexpr int32 ExpectedDefaultThirdPlateCount = 84;
	constexpr int32 ExpectedDefaultCoalescedCount = 58589;

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

	FString BucketName(const ECarrierLabPhaseIIIE3MultiHitBucket Bucket)
	{
		switch (Bucket)
		{
		case ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual:
			return TEXT("cross-plate-equal");
		case ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate:
			return TEXT("third-plate");
		case ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent:
			return TEXT("cross-plate-different");
		case ECarrierLabPhaseIIIE3MultiHitBucket::MixedMaterial:
			return TEXT("mixed-material");
		case ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateCoincident:
			return TEXT("within-plate-coincident");
		case ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateDistanceSeparated:
			return TEXT("within-plate-distance-separated");
		case ECarrierLabPhaseIIIE3MultiHitBucket::None:
		default:
			return TEXT("none");
		}
	}

	FString SelectionClassName(const ECarrierLabPhaseIIIE3SelectionClass SelectionClass)
	{
		switch (SelectionClass)
		{
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit:
			return TEXT("unresolved same-material multi-hit");
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedMixedMaterialMultiHit:
			return TEXT("unresolved mixed-material multi-hit");
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedThirdPlateMultiHit:
			return TEXT("unresolved third-plate multi-hit");
		case ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit:
			return TEXT("resolved single hit");
		case ECarrierLabPhaseIIIE3SelectionClass::DivergentGapAfterFiltering:
			return TEXT("divergent gap after filtering");
		case ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap:
		default:
			return TEXT("no-hit divergent gap");
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

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			OutputRoot = FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("CarrierLab"),
				TEXT("PhaseIII"),
				TEXT("IIIE62CrossPlateMultiHit"));
		}
		else if (FPaths::IsRelative(OutputRoot))
		{
			OutputRoot = FPaths::Combine(FPaths::ProjectDir(), OutputRoot);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	FString GetReportPath(const FString& Params)
	{
		FString ReportPath;
		if (!FParse::Value(*Params, TEXT("Report="), ReportPath))
		{
			ReportPath = FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("docs"),
				TEXT("checkpoints"),
				TEXT("phase-iii-slice-iiie6-2-cross-plate-multihit-diagnosis.md"));
		}
		else if (FPaths::IsRelative(ReportPath))
		{
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), ReportPath);
		}
		return FPaths::ConvertRelativePathToFull(ReportPath);
	}

	ACarrierLabVisualizationActor* SpawnActor(UWorld& World)
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
		Actor->Seed = 42;
		Actor->ContinentalPlateFraction = 0.0;
		Actor->bEnableNaturalResamplingEvents = false;
		Actor->bEnablePhaseIIIE3DuplicateHitCoalescing = true;
		Actor->ConfigurePhaseIIICProcessLayer(true, false);
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	FCarrierLabPhaseIIIE62CandidateSnapshot MakeSnapshot(
		const int32 CandidateIndex,
		const int32 PlateId,
		const int32 LocalTriangleId,
		const int32 SourceTriangleId,
		const int32 A,
		const int32 B,
		const int32 C,
		const FVector3d& Bary,
		const bool bBoundary,
		const double ElevationResidualKm = 0.0)
	{
		FCarrierLabPhaseIIIE62CandidateSnapshot Snapshot;
		Snapshot.CandidateIndex = CandidateIndex;
		Snapshot.PlateId = PlateId;
		Snapshot.LocalTriangleId = LocalTriangleId;
		Snapshot.SourceTriangleId = SourceTriangleId;
		Snapshot.GlobalVertexIds[0] = A;
		Snapshot.GlobalVertexIds[1] = B;
		Snapshot.GlobalVertexIds[2] = C;
		Snapshot.Bary = Bary;
		const int32 NearZeroCount =
			(Bary.X <= 1.0e-9 ? 1 : 0) +
			(Bary.Y <= 1.0e-9 ? 1 : 0) +
			(Bary.Z <= 1.0e-9 ? 1 : 0);
		const int32 NearOneCount =
			(Bary.X >= 1.0 - 1.0e-9 ? 1 : 0) +
			(Bary.Y >= 1.0 - 1.0e-9 ? 1 : 0) +
			(Bary.Z >= 1.0 - 1.0e-9 ? 1 : 0);
		if (NearOneCount == 1 && NearZeroCount >= 2)
		{
			Snapshot.BarycentricShape = ECarrierLabPhaseIIIE62BarycentricShape::Vertex;
		}
		else if (NearZeroCount >= 1)
		{
			Snapshot.BarycentricShape = ECarrierLabPhaseIIIE62BarycentricShape::Edge;
		}
		else
		{
			Snapshot.BarycentricShape = ECarrierLabPhaseIIIE62BarycentricShape::Interior;
		}
		Snapshot.bBoundary = bBoundary;
		Snapshot.Distance = 1.0;
		Snapshot.ElevationResidualKm = ElevationResidualKm;
		Snapshot.FilterReason = ECarrierLabPhaseIIIE3FilterReason::None;
		return Snapshot;
	}

	FCarrierLabPhaseIIIE3CandidateProbe MakeProbe(
		const FCarrierLabPhaseIIIE62CandidateSnapshot& Snapshot,
		const double Elevation = -2.0)
	{
		FCarrierLabPhaseIIIE3CandidateProbe Probe;
		Probe.PlateId = Snapshot.PlateId;
		Probe.LocalTriangleId = Snapshot.LocalTriangleId;
		Probe.Bary = Snapshot.Bary;
		Probe.Distance = Snapshot.Distance;
		Probe.ContinentalFraction = 0.0;
		Probe.Elevation = Elevation;
		Probe.HistoricalElevation = Elevation;
		Probe.OceanicAge = 0.0;
		Probe.RidgeDirection = FVector3d::ZeroVector;
		Probe.FoldDirection = FVector3d::ZeroVector;
		Probe.bBoundary = Snapshot.bBoundary;
		return Probe;
	}

	struct FFixtureSpec
	{
		FString Name;
		TArray<FCarrierLabPhaseIIIE62CandidateSnapshot> Snapshots;
		TArray<FCarrierLabPhaseIIIE3CandidateProbe> Probes;
		ECarrierLabPhaseIIIE3MultiHitBucket ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual;
		ECarrierLabPhaseIIIE3SelectionClass ExpectedSelection = ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit;
		ECarrierLabPhaseIIIE62HoldShape ExpectedShape = ECarrierLabPhaseIIIE62HoldShape::InvalidOrUnclassified;
	};

	struct FFixtureResult
	{
		FString Name;
		bool bPass = false;
		FCarrierLabPhaseIIIE3SelectionRecord SelectionRecord;
		FCarrierLabPhaseIIIE62HoldRecord HoldRecord;
		FString Hash;
	};

	struct FDefaultDiagnosticResult
	{
		FString Name;
		bool bPass = false;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIE62CrossPlateMultiHitAudit Audit;
	};

	void AddFixture(
		TArray<FFixtureSpec>& Fixtures,
		const FString& Name,
		const TArray<FCarrierLabPhaseIIIE62CandidateSnapshot>& Snapshots,
		const ECarrierLabPhaseIIIE62HoldShape ExpectedShape,
		const ECarrierLabPhaseIIIE3MultiHitBucket ExpectedBucket,
		const ECarrierLabPhaseIIIE3SelectionClass ExpectedSelection)
	{
		FFixtureSpec Fixture;
		Fixture.Name = Name;
		Fixture.Snapshots = Snapshots;
		Fixture.ExpectedShape = ExpectedShape;
		Fixture.ExpectedBucket = ExpectedBucket;
		Fixture.ExpectedSelection = ExpectedSelection;
		for (int32 Index = 0; Index < Snapshots.Num(); ++Index)
		{
			const double Elevation = Snapshots[Index].ElevationResidualKm > 0.0 ? -2.0 - Snapshots[Index].ElevationResidualKm : -2.0;
			Fixture.Probes.Add(MakeProbe(Snapshots[Index], Elevation));
		}
		Fixtures.Add(Fixture);
	}

	TArray<FFixtureSpec> MakeFixtures()
	{
		TArray<FFixtureSpec> Fixtures;
		const FVector3d EdgeBary(0.5, 0.5, 0.0);
		const FVector3d VertexBary(1.0, 0.0, 0.0);
		const FVector3d InteriorBary(0.34, 0.33, 0.33);

		AddFixture(
			Fixtures,
			TEXT("two-plate same source triangle"),
			{
				MakeSnapshot(0, 0, 10, 100, 1, 2, 3, EdgeBary, true),
				MakeSnapshot(1, 1, 11, 100, 1, 2, 3, EdgeBary, true)
			},
			ECarrierLabPhaseIIIE62HoldShape::TwoPlateSameSourceTriangle,
			ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual,
			ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit);

		AddFixture(
			Fixtures,
			TEXT("two-plate shared global edge"),
			{
				MakeSnapshot(0, 0, 20, 101, 10, 11, 12, EdgeBary, true),
				MakeSnapshot(1, 1, 21, 102, 10, 11, 13, EdgeBary, true)
			},
			ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalEdge,
			ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual,
			ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit);

		AddFixture(
			Fixtures,
			TEXT("two-plate shared global vertex only"),
			{
				MakeSnapshot(0, 0, 30, 103, 20, 21, 22, VertexBary, true),
				MakeSnapshot(1, 1, 31, 104, 20, 23, 24, VertexBary, true)
			},
			ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalVertexOnly,
			ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual,
			ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit);

		AddFixture(
			Fixtures,
			TEXT("two-plate no shared global vertices"),
			{
				MakeSnapshot(0, 0, 40, 105, 30, 31, 32, EdgeBary, true),
				MakeSnapshot(1, 1, 41, 106, 33, 34, 35, EdgeBary, true)
			},
			ECarrierLabPhaseIIIE62HoldShape::TwoPlateNoSharedGlobalVertices,
			ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual,
			ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit);

		AddFixture(
			Fixtures,
			TEXT("two-plate field mismatch"),
			{
				MakeSnapshot(0, 0, 50, 107, 40, 41, 42, EdgeBary, true),
				MakeSnapshot(1, 1, 51, 108, 40, 41, 43, EdgeBary, true, 1.0e-4)
			},
			ECarrierLabPhaseIIIE62HoldShape::TwoPlateFieldMismatch,
			ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual,
			ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit);

		AddFixture(
			Fixtures,
			TEXT("three-plate common global vertex"),
			{
				MakeSnapshot(0, 0, 60, 109, 50, 51, 52, VertexBary, true),
				MakeSnapshot(1, 1, 61, 110, 50, 53, 54, VertexBary, true),
				MakeSnapshot(2, 2, 62, 111, 50, 55, 56, VertexBary, true)
			},
			ECarrierLabPhaseIIIE62HoldShape::ThreePlateCommonGlobalVertex,
			ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate,
			ECarrierLabPhaseIIIE3SelectionClass::UnresolvedThirdPlateMultiHit);

		AddFixture(
			Fixtures,
			TEXT("three-plate edge plus intruder"),
			{
				MakeSnapshot(0, 0, 70, 112, 60, 61, 62, EdgeBary, true),
				MakeSnapshot(1, 1, 71, 113, 60, 61, 63, EdgeBary, true),
				MakeSnapshot(2, 2, 72, 114, 64, 65, 66, EdgeBary, true)
			},
			ECarrierLabPhaseIIIE62HoldShape::ThreePlateEdgePlusIntruder,
			ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate,
			ECarrierLabPhaseIIIE3SelectionClass::UnresolvedThirdPlateMultiHit);

		AddFixture(
			Fixtures,
			TEXT("three-plate no common source vertex"),
			{
				MakeSnapshot(0, 0, 80, 115, 70, 71, 72, EdgeBary, true),
				MakeSnapshot(1, 1, 81, 116, 73, 74, 75, EdgeBary, true),
				MakeSnapshot(2, 2, 82, 117, 76, 77, 78, EdgeBary, true)
			},
			ECarrierLabPhaseIIIE62HoldShape::ThreePlateNoCommonSourceVertex,
			ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate,
			ECarrierLabPhaseIIIE3SelectionClass::UnresolvedThirdPlateMultiHit);

		AddFixture(
			Fixtures,
			TEXT("non-boundary interior overlap"),
			{
				MakeSnapshot(0, 0, 90, 118, 80, 81, 82, InteriorBary, false),
				MakeSnapshot(1, 1, 91, 119, 80, 81, 83, InteriorBary, false)
			},
			ECarrierLabPhaseIIIE62HoldShape::NonBoundaryOrInteriorOverlap,
			ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual,
			ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit);

		return Fixtures;
	}

	FString ComputeFixtureHash(const FFixtureResult& Result)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMix(Hash, Result.bPass ? 2ull : 1ull);
		HashMix(Hash, static_cast<uint64>(Result.SelectionRecord.SelectionClass) + 1ull);
		HashMix(Hash, static_cast<uint64>(Result.SelectionRecord.MultiHitBucket) + 1ull);
		HashMix(Hash, static_cast<uint64>(Result.HoldRecord.HoldShape) + 1ull);
		HashMix(Hash, static_cast<uint64>(Result.HoldRecord.CandidateCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.HoldRecord.DistinctPlateCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.HoldRecord.DistinctSourceTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.HoldRecord.SharedGlobalVertexCount + 1));
		HashMixDouble(Hash, Result.HoldRecord.MaxRayDistanceResidualKm);
		HashMixDouble(Hash, Result.HoldRecord.MaxScalarResidual);
		HashMixDouble(Hash, Result.HoldRecord.MaxElevationResidualKm);
		HashMixDouble(Hash, Result.HoldRecord.MaxUnitVectorResidual);
		return HashToString(Hash);
	}

	FFixtureResult RunFixture(ACarrierLabVisualizationActor& Actor, const FFixtureSpec& Fixture)
	{
		FFixtureResult Result;
		Result.Name = Fixture.Name;
		Actor.bEnablePhaseIIIE3DuplicateHitCoalescing = true;
		const bool bSelected = Actor.QueryPhaseIIIE3FilteredRemeshSelectionForTest(
			FVector3d::UnitX(),
			Fixture.Probes,
			Result.SelectionRecord);
		const bool bDiagnosed = Actor.DiagnosePhaseIIIE62HoldSnapshotsForTest(
			0,
			Result.SelectionRecord.SelectionClass,
			Result.SelectionRecord.MultiHitBucket,
			Fixture.Snapshots,
			Result.HoldRecord);
		Result.bPass =
			bSelected &&
			bDiagnosed &&
			Result.SelectionRecord.bUnresolvedMultiHit &&
			!Result.SelectionRecord.bCoalescedDuplicateHit &&
			!Result.SelectionRecord.bUsedPolicyWinner &&
			!Result.SelectionRecord.bUsedPriorOwnerFallback &&
			Result.SelectionRecord.SelectionClass == Fixture.ExpectedSelection &&
			Result.SelectionRecord.MultiHitBucket == Fixture.ExpectedBucket &&
			Result.HoldRecord.HoldShape == Fixture.ExpectedShape;
		Result.Hash = ComputeFixtureHash(Result);
		return Result;
	}

	int32 ShapeTotal(const FCarrierLabPhaseIIIE62CrossPlateMultiHitAudit& Audit)
	{
		return Audit.TwoPlateSameSourceTriangleCount +
			Audit.TwoPlateSharedGlobalEdgeCount +
			Audit.TwoPlateSharedGlobalVertexOnlyCount +
			Audit.TwoPlateNoSharedGlobalVerticesCount +
			Audit.TwoPlateFieldMismatchCount +
			Audit.ThreePlateCommonGlobalVertexCount +
			Audit.ThreePlateEdgePlusIntruderCount +
			Audit.ThreePlateNoCommonSourceVertexCount +
			Audit.NonBoundaryOrInteriorOverlapCount +
			Audit.InvalidOrUnclassifiedCount;
	}

	bool RunDefaultDiagnostic(UWorld& World, const FString& Name, FDefaultDiagnosticResult& OutResult)
	{
		OutResult = FDefaultDiagnosticResult();
		OutResult.Name = Name;
		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(World);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier())
		{
			Actor->Destroy();
			return false;
		}
		Actor->bEnablePhaseIIIE3DuplicateHitCoalescing = true;
		const bool bRan = Actor->RunPhaseIIIE62CrossPlateMultiHitDiagnosisAudit(OutResult.Audit);
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);

		const int32 ExpectedShapeTotal =
			OutResult.Audit.SelectionCrossPlateEqualMultiHitCount +
			OutResult.Audit.SelectionThirdPlateMultiHitCount;
		OutResult.bPass =
			bRan &&
			OutResult.Audit.bRan &&
			OutResult.Audit.SampleCount == DefaultSampleCount &&
			OutResult.Audit.SelectionUnresolvedMultiHitCount == ExpectedDefaultUnresolvedHoldCount &&
			OutResult.Audit.SelectionCrossPlateEqualMultiHitCount == ExpectedDefaultCrossPlateEqualCount &&
			OutResult.Audit.SelectionThirdPlateMultiHitCount == ExpectedDefaultThirdPlateCount &&
			OutResult.Audit.CoalescedMultiHitCount == ExpectedDefaultCoalescedCount &&
			OutResult.Audit.DiagnosedHoldCount == ExpectedShapeTotal &&
			ShapeTotal(OutResult.Audit) == OutResult.Audit.DiagnosedHoldCount &&
			OutResult.Audit.PolicyWinnerCount == 0 &&
			OutResult.Audit.PriorOwnerFallbackCount == 0 &&
			OutResult.Audit.ProjectionAuthorityCount == 0;
		return bRan;
	}

	FString FormatDistribution(const FCarrierLabPhaseIIIE62CrossPlateMultiHitAudit& Audit)
	{
		return FString::Printf(
			TEXT("samples `%d`, unresolved `%d`, cross/third `%d/%d`, coalesced `%d`, shapes same-source/shared-edge/shared-vertex/no-shared/field-mismatch/three-common/three-edge-intruder/three-none/non-boundary/invalid `%d/%d/%d/%d/%d/%d/%d/%d/%d/%d`, policy/prior/projection `0/0/0`, selection `%s`, diagnosis `%s`"),
			Audit.SampleCount,
			Audit.SelectionUnresolvedMultiHitCount,
			Audit.SelectionCrossPlateEqualMultiHitCount,
			Audit.SelectionThirdPlateMultiHitCount,
			Audit.CoalescedMultiHitCount,
			Audit.TwoPlateSameSourceTriangleCount,
			Audit.TwoPlateSharedGlobalEdgeCount,
			Audit.TwoPlateSharedGlobalVertexOnlyCount,
			Audit.TwoPlateNoSharedGlobalVerticesCount,
			Audit.TwoPlateFieldMismatchCount,
			Audit.ThreePlateCommonGlobalVertexCount,
			Audit.ThreePlateEdgePlusIntruderCount,
			Audit.ThreePlateNoCommonSourceVertexCount,
			Audit.NonBoundaryOrInteriorOverlapCount,
			Audit.InvalidOrUnclassifiedCount,
			*Audit.SelectionHash,
			*Audit.DiagnosisHash);
	}

	FString RecommendationFor(const FCarrierLabPhaseIIIE62CrossPlateMultiHitAudit& Audit)
	{
		int32 BestCount = Audit.TwoPlateSameSourceTriangleCount;
		FString BestName = TEXT("same source triangle duplication");
		auto Consider = [&BestCount, &BestName](const int32 Count, const TCHAR* Name)
		{
			if (Count > BestCount)
			{
				BestCount = Count;
				BestName = Name;
			}
		};
		Consider(Audit.TwoPlateSharedGlobalEdgeCount, TEXT("shared global edge boundary duplication"));
		Consider(Audit.TwoPlateSharedGlobalVertexOnlyCount, TEXT("shared global vertex degeneracy"));
		Consider(Audit.TwoPlateNoSharedGlobalVerticesCount, TEXT("true two-plate overlap without shared source vertices"));
		Consider(Audit.TwoPlateFieldMismatchCount, TEXT("cross-plate field mismatch"));
		Consider(Audit.ThreePlateCommonGlobalVertexCount, TEXT("triple-junction common vertex degeneracy"));
		Consider(Audit.ThreePlateEdgePlusIntruderCount, TEXT("triple-junction edge plus intruder"));
		Consider(Audit.ThreePlateNoCommonSourceVertexCount, TEXT("true three-plate overlap without common source vertices"));
		Consider(Audit.NonBoundaryOrInteriorOverlapCount, TEXT("non-boundary or interior overlap"));
		Consider(Audit.InvalidOrUnclassifiedCount, TEXT("invalid/unclassified diagnostics"));
		return FString::Printf(TEXT("Dominant class is `%s` with `%d` records; the next slice should target that class without relaxing cross-plate holds generically."), *BestName, BestCount);
	}

	FString BuildReport(
		const TArray<FFixtureResult>& Fixtures,
		const FDefaultDiagnosticResult& ReplayA,
		const FDefaultDiagnosticResult& ReplayB,
		const FString& MetricsPath)
	{
		const bool bReplayStable =
			ReplayA.Audit.SelectionHash == ReplayB.Audit.SelectionHash &&
			ReplayA.Audit.DiagnosisHash == ReplayB.Audit.DiagnosisHash;
		FString Report;
		Report += TEXT("# Phase IIIE.6.2 Cross-Plate Multi-Hit Diagnosis\n\n");
		Report += TEXT("Verdict: PASS / DIAGNOSTIC-ONLY CROSS-PLATE HOLD CLASSIFICATION. This hardening slice explains the remaining default IIIE.6 live remesh holds after same-plate duplicate coalescing. It does not resolve, coalesce, skip, or apply any cross-plate or third-plate multi-hit.\n\n");
		Report += TEXT("## Scope\n\n");
		Report += TEXT("- Selector behavior is unchanged: cross-plate equality and third-plate hits remain fail-loud unresolved records.\n");
		Report += TEXT("- Candidate snapshots are diagnostic evidence only; global sample ids and source triangle ids are not remesh authority.\n");
		Report += TEXT("- No centroid, random, synthetic, prior-owner, projection-derived, or cross-plate winner rule is introduced.\n\n");

		Report += TEXT("## Gates\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---:|---|\n");
		for (const FFixtureResult& Fixture : Fixtures)
		{
			Report += FString::Printf(
				TEXT("| Fixture `%s` | %s | selection `%s`, bucket `%s`, shape `%s`, candidates `%d`, policy/prior `0/0`, hash `%s`. |\n"),
				*Fixture.Name,
				*PassFail(Fixture.bPass),
				*SelectionClassName(Fixture.SelectionRecord.SelectionClass),
				*BucketName(Fixture.SelectionRecord.MultiHitBucket),
				*HoldShapeName(Fixture.HoldRecord.HoldShape),
				Fixture.HoldRecord.CandidateCount,
				*Fixture.Hash);
		}
		Report += FString::Printf(
			TEXT("| Default 100k/40 replay A | %s | %s. |\n"),
			*PassFail(ReplayA.bPass),
			*FormatDistribution(ReplayA.Audit));
		Report += FString::Printf(
			TEXT("| Default 100k/40 replay B | %s | %s. |\n"),
			*PassFail(ReplayB.bPass),
			*FormatDistribution(ReplayB.Audit));
		Report += FString::Printf(
			TEXT("| Same-seed diagnosis replay | %s | selection hash stable `%d`, diagnosis hash stable `%d`. |\n\n"),
			*PassFail(bReplayStable),
			ReplayA.Audit.SelectionHash == ReplayB.Audit.SelectionHash ? 1 : 0,
			ReplayA.Audit.DiagnosisHash == ReplayB.Audit.DiagnosisHash ? 1 : 0);

		Report += TEXT("## Distribution\n\n");
		Report += TEXT("| Shape | Count |\n");
		Report += TEXT("|---|---:|\n");
		Report += FString::Printf(TEXT("| `two_plate_same_source_triangle` | %d |\n"), ReplayA.Audit.TwoPlateSameSourceTriangleCount);
		Report += FString::Printf(TEXT("| `two_plate_shared_global_edge` | %d |\n"), ReplayA.Audit.TwoPlateSharedGlobalEdgeCount);
		Report += FString::Printf(TEXT("| `two_plate_shared_global_vertex_only` | %d |\n"), ReplayA.Audit.TwoPlateSharedGlobalVertexOnlyCount);
		Report += FString::Printf(TEXT("| `two_plate_no_shared_global_vertices` | %d |\n"), ReplayA.Audit.TwoPlateNoSharedGlobalVerticesCount);
		Report += FString::Printf(TEXT("| `two_plate_field_mismatch` | %d |\n"), ReplayA.Audit.TwoPlateFieldMismatchCount);
		Report += FString::Printf(TEXT("| `three_plate_common_global_vertex` | %d |\n"), ReplayA.Audit.ThreePlateCommonGlobalVertexCount);
		Report += FString::Printf(TEXT("| `three_plate_edge_plus_intruder` | %d |\n"), ReplayA.Audit.ThreePlateEdgePlusIntruderCount);
		Report += FString::Printf(TEXT("| `three_plate_no_common_source_vertex` | %d |\n"), ReplayA.Audit.ThreePlateNoCommonSourceVertexCount);
		Report += FString::Printf(TEXT("| `non_boundary_or_interior_overlap` | %d |\n"), ReplayA.Audit.NonBoundaryOrInteriorOverlapCount);
		Report += FString::Printf(TEXT("| `invalid_or_unclassified` | %d |\n\n"), ReplayA.Audit.InvalidOrUnclassifiedCount);

		Report += TEXT("## Recommendation\n\n");
		Report += FString::Printf(TEXT("- %s\n"), *RecommendationFor(ReplayA.Audit));
		Report += TEXT("- IIIE consolidation remains blocked on a real cross-plate/third-plate decision; this report only tells us which decision should be made next.\n\n");

		Report += TEXT("## Stop Conditions\n\n");
		Report += TEXT("- Stop if any cross-plate or third-plate hold is resolved by this diagnostic slice.\n");
		Report += TEXT("- Stop if policy/prior/projection counters become nonzero.\n");
		Report += TEXT("- Stop if the default live cadence is described as fully unblocked by this diagnostic-only report.\n\n");
		Report += FString::Printf(TEXT("Metrics: `%s`.\n"), *MetricsPath);
		return Report;
	}

	void AppendMetricsLine(FString& Lines, const FFixtureResult& Result)
	{
		Lines += FString::Printf(
			TEXT("{\"fixture\":%s,\"pass\":%s,\"selection\":%s,\"bucket\":%s,\"shape\":%s,\"candidates\":%d,\"hash\":%s}\n"),
			*JsonString(Result.Name),
			Result.bPass ? TEXT("true") : TEXT("false"),
			*JsonString(SelectionClassName(Result.SelectionRecord.SelectionClass)),
			*JsonString(BucketName(Result.SelectionRecord.MultiHitBucket)),
			*JsonString(HoldShapeName(Result.HoldRecord.HoldShape)),
			Result.HoldRecord.CandidateCount,
			*JsonString(Result.Hash));
	}

	void AppendMetricsLine(FString& Lines, const FDefaultDiagnosticResult& Result)
	{
		Lines += FString::Printf(
			TEXT("{\"fixture\":%s,\"pass\":%s,\"samples\":%d,\"unresolved\":%d,\"cross_plate_equal\":%d,\"third_plate\":%d,\"coalesced\":%d,\"two_same_source\":%d,\"two_edge\":%d,\"two_vertex\":%d,\"two_none\":%d,\"two_field_mismatch\":%d,\"three_common\":%d,\"three_edge_intruder\":%d,\"three_none\":%d,\"non_boundary\":%d,\"invalid\":%d,\"selection_hash\":%s,\"diagnosis_hash\":%s,\"seconds\":%.6f}\n"),
			*JsonString(Result.Name),
			Result.bPass ? TEXT("true") : TEXT("false"),
			Result.Audit.SampleCount,
			Result.Audit.SelectionUnresolvedMultiHitCount,
			Result.Audit.SelectionCrossPlateEqualMultiHitCount,
			Result.Audit.SelectionThirdPlateMultiHitCount,
			Result.Audit.CoalescedMultiHitCount,
			Result.Audit.TwoPlateSameSourceTriangleCount,
			Result.Audit.TwoPlateSharedGlobalEdgeCount,
			Result.Audit.TwoPlateSharedGlobalVertexOnlyCount,
			Result.Audit.TwoPlateNoSharedGlobalVerticesCount,
			Result.Audit.TwoPlateFieldMismatchCount,
			Result.Audit.ThreePlateCommonGlobalVertexCount,
			Result.Audit.ThreePlateEdgePlusIntruderCount,
			Result.Audit.ThreePlateNoCommonSourceVertexCount,
			Result.Audit.NonBoundaryOrInteriorOverlapCount,
			Result.Audit.InvalidOrUnclassifiedCount,
			*JsonString(Result.Audit.SelectionHash),
			*JsonString(Result.Audit.DiagnosisHash),
			Result.Seconds);
	}
}

UCarrierLabPhaseIIIE62CrossPlateMultiHitCommandlet::UCarrierLabPhaseIIIE62CrossPlateMultiHitCommandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIE62CrossPlateMultiHitCommandlet::Main(const FString& Params)
{
	UWorld* World = GetCommandletWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPhaseIIIE62CrossPlateMultiHit: no world available."));
		return 1;
	}

	ACarrierLabVisualizationActor* FixtureActor = SpawnActor(*World);
	if (FixtureActor == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("IIIE.6.2 could not spawn fixture actor"));
		return 1;
	}
	TArray<FFixtureResult> FixtureResults;
	bool bAllFixturesPass = true;
	for (const FFixtureSpec& Fixture : MakeFixtures())
	{
		FFixtureResult Result = RunFixture(*FixtureActor, Fixture);
		bAllFixturesPass = bAllFixturesPass && Result.bPass;
		FixtureResults.Add(Result);
	}

	FDefaultDiagnosticResult ReplayA;
	FDefaultDiagnosticResult ReplayB;
	const bool bReplayARan = RunDefaultDiagnostic(*World, TEXT("default_100k40_replay_a"), ReplayA);
	const bool bReplayBRan = RunDefaultDiagnostic(*World, TEXT("default_100k40_replay_b"), ReplayB);
	const bool bReplayStable =
		ReplayA.Audit.SelectionHash == ReplayB.Audit.SelectionHash &&
		ReplayA.Audit.DiagnosisHash == ReplayB.Audit.DiagnosisHash;

	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("phase-iii-slice-iiie6-2-cross-plate-multihit-diagnosis-metrics.jsonl"));

	FString MetricsLines;
	for (const FFixtureResult& Result : FixtureResults)
	{
		AppendMetricsLine(MetricsLines, Result);
	}
	AppendMetricsLine(MetricsLines, ReplayA);
	AppendMetricsLine(MetricsLines, ReplayB);
	FFileHelper::SaveStringToFile(MetricsLines, *MetricsPath);

	const FString ReportPath = GetReportPath(Params);
	const FString Report = BuildReport(FixtureResults, ReplayA, ReplayB, MetricsPath);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	const bool bPass =
		bAllFixturesPass &&
		bReplayARan &&
		bReplayBRan &&
		ReplayA.bPass &&
		ReplayB.bPass &&
		bReplayStable;
	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIIE62CrossPlateMultiHit %s. Report: %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), *ReportPath);
	return bPass ? 0 : 1;
}
