// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabVisualizationActor.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/InputComponent.h"
#include "Async/ParallelFor.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "IndexTypes.h"
#include "InputCoreTypes.h"
#include "Spatial/SpatialInterfaces.h"

using UE::Geometry::FDynamicMesh3;
using UE::Geometry::FDynamicMeshAABBTree3;
using UE::Geometry::FDynamicMeshColorOverlay;
using UE::Geometry::FIndex3i;
using UE::Geometry::IMeshSpatial;

namespace
{
	constexpr double EarthRadiusKm = 6371.0;
	constexpr double DeltaTimeMa = 2.0;
	constexpr double BoundaryBarycentricEpsilon = 1.0e-7;
	constexpr double PhaseIIContactVelocityMargin = 1.0e-6;
	constexpr int32 BoundaryLonBins = 72;
	constexpr int32 BoundaryLatBins = 36;
	constexpr int32 BoundarySearchRadiusBins = 2;

	struct FCarrierLabVizCandidate
	{
		int32 PlateId = INDEX_NONE;
		int32 LocalTriangleId = INDEX_NONE;
		FVector3d Bary = FVector3d::ZeroVector;
		double Distance = 0.0;
		bool bBoundary = false;
	};

	struct FCarrierLabVizBoundaryPoint
	{
		int32 PlateId = INDEX_NONE;
		FVector3d UnitPosition = FVector3d::UnitZ();
		double ContinentalFraction = 0.0;
	};

	struct FCarrierLabVizBoundaryIndex
	{
		TArray<FCarrierLabVizBoundaryPoint> Points;
		TMap<int32, TArray<int32>> Bins;
	};

	FVector3d NormalizeOrFallback(const FVector3d& Vector, const FVector3d& Fallback)
	{
		const double Size = Vector.Size();
		return Size > UE_DOUBLE_SMALL_NUMBER ? Vector / Size : Fallback;
	}

	FVector3d RotateVector(const FVector3d& Vector, const FVector3d& Axis, const double AngleRadians)
	{
		const double C = FMath::Cos(AngleRadians);
		const double S = FMath::Sin(AngleRadians);
		return Vector * C + FVector3d::CrossProduct(Axis, Vector) * S + Axis * FVector3d::DotProduct(Axis, Vector) * (1.0 - C);
	}

	double AngularSpeedRadiansPerStep(const double VelocityMmPerYear)
	{
		const double DistanceKmPerStep = VelocityMmPerYear * 1.0e-6 * DeltaTimeMa * 1000000.0;
		return DistanceKmPerStep / EarthRadiusKm;
	}

	bool IsBoundaryHit(const FVector3d& Bary)
	{
		return Bary.X <= BoundaryBarycentricEpsilon || Bary.Y <= BoundaryBarycentricEpsilon || Bary.Z <= BoundaryBarycentricEpsilon;
	}

	int32 CountBits64(uint64 Value)
	{
		int32 Count = 0;
		while (Value != 0)
		{
			Value &= (Value - 1);
			++Count;
		}
		return Count;
	}

	int32 LowestSetBitIndex(const uint64 Value)
	{
		if (Value == 0)
		{
			return INDEX_NONE;
		}
		for (int32 Index = 0; Index < 64; ++Index)
		{
			if ((Value & (1ull << Index)) != 0)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

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

	bool IntersectRayWithTriangle(
		const FVector3d& RayDirection,
		const FVector3d& A,
		const FVector3d& B,
		const FVector3d& C,
		FVector3d& OutBarycentric)
	{
		constexpr double GeometryEpsilon = 1.0e-12;
		const FVector3d Normal = FVector3d::CrossProduct(B - A, C - A);
		const double PlaneDenominator = FVector3d::DotProduct(Normal, RayDirection);
		if (FMath::Abs(PlaneDenominator) <= GeometryEpsilon)
		{
			return false;
		}
		const double T = FVector3d::DotProduct(Normal, A) / PlaneDenominator;
		if (T <= GeometryEpsilon)
		{
			return false;
		}
		const FVector3d Intersection = RayDirection * T;
		const FVector3d V0 = B - A;
		const FVector3d V1 = C - A;
		const FVector3d V2 = Intersection - A;
		const double D00 = FVector3d::DotProduct(V0, V0);
		const double D01 = FVector3d::DotProduct(V0, V1);
		const double D11 = FVector3d::DotProduct(V1, V1);
		const double D20 = FVector3d::DotProduct(V2, V0);
		const double D21 = FVector3d::DotProduct(V2, V1);
		const double Denominator = D00 * D11 - D01 * D01;
		if (FMath::Abs(Denominator) <= GeometryEpsilon)
		{
			return false;
		}
		const double V = (D11 * D20 - D01 * D21) / Denominator;
		const double W = (D00 * D21 - D01 * D20) / Denominator;
		const double U = 1.0 - V - W;
		constexpr double BarycentricEpsilon = 1.0e-10;
		if (U < -BarycentricEpsilon || V < -BarycentricEpsilon || W < -BarycentricEpsilon)
		{
			return false;
		}
		OutBarycentric = FVector3d(U, V, W);
		return true;
	}

	bool BuildPlateRayMeshTopology(const CarrierLab::FCarrierState& State, TArray<FCarrierLabVizPlateMesh>& OutPlateMeshes, FString& OutError)
	{
		OutPlateMeshes.Reset(State.Plates.Num());
		OutPlateMeshes.SetNum(State.Plates.Num());
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			if (!OutPlateMeshes.IsValidIndex(Plate.PlateId))
			{
				OutError = FString::Printf(TEXT("Invalid plate id %d while building visualization ray meshes."), Plate.PlateId);
				return false;
			}

			FCarrierLabVizPlateMesh& PlateMesh = OutPlateMeshes[Plate.PlateId];
			PlateMesh.PlateId = Plate.PlateId;
			PlateMesh.Mesh.EnableTriangleGroups();
			for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
			{
				PlateMesh.Mesh.AppendVertex(Vertex.UnitPosition);
			}
			for (int32 LocalTriangleId = 0; LocalTriangleId < Plate.LocalTriangles.Num(); ++LocalTriangleId)
			{
				const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
				const int32 MeshTriangleId = PlateMesh.Mesh.AppendTriangle(Triangle.A, Triangle.B, Triangle.C, 0);
				if (MeshTriangleId < 0)
				{
					OutError = FString::Printf(TEXT("FDynamicMesh3 rejected visualization plate %d local triangle %d."), Plate.PlateId, LocalTriangleId);
					return false;
				}
				PlateMesh.MeshTriangleIdToLocalTriangleId.Add(MeshTriangleId, LocalTriangleId);
			}
			PlateMesh.Tree = MakeUnique<FDynamicMeshAABBTree3>(&PlateMesh.Mesh, false);
		}
		return true;
	}

	bool RefreshPlateRayMeshVerticesAndTrees(const CarrierLab::FCarrierState& State, TArray<FCarrierLabVizPlateMesh>& PlateMeshes, FString& OutError)
	{
		if (PlateMeshes.Num() != State.Plates.Num())
		{
			OutError = TEXT("Cached visualization plate mesh count does not match carrier plate count.");
			return false;
		}

		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			if (!PlateMeshes.IsValidIndex(Plate.PlateId))
			{
				OutError = FString::Printf(TEXT("Invalid plate id %d while refreshing visualization ray meshes."), Plate.PlateId);
				return false;
			}

			FCarrierLabVizPlateMesh& PlateMesh = PlateMeshes[Plate.PlateId];
			if (PlateMesh.PlateId != Plate.PlateId ||
				PlateMesh.Mesh.VertexCount() != Plate.Vertices.Num() ||
				PlateMesh.Mesh.TriangleCount() != Plate.LocalTriangles.Num() ||
				PlateMesh.MeshTriangleIdToLocalTriangleId.Num() != Plate.LocalTriangles.Num())
			{
				OutError = FString::Printf(TEXT("Cached visualization plate %d topology no longer matches carrier topology."), Plate.PlateId);
				return false;
			}

			for (int32 VertexId = 0; VertexId < Plate.Vertices.Num(); ++VertexId)
			{
				if (!PlateMesh.Mesh.IsVertex(VertexId))
				{
					OutError = FString::Printf(TEXT("Cached visualization plate %d missing local vertex %d."), Plate.PlateId, VertexId);
					return false;
				}
				PlateMesh.Mesh.SetVertex(VertexId, Plate.Vertices[VertexId].UnitPosition, false);
			}

			if (!PlateMesh.Tree.IsValid())
			{
				PlateMesh.Tree = MakeUnique<FDynamicMeshAABBTree3>(&PlateMesh.Mesh, false);
			}
			PlateMesh.Tree->Build();
		}
		return true;
	}

	bool BuildProjectionRayMeshTopology(const CarrierLab::FCarrierState& State, FCarrierLabVizProjectionMesh& OutProjectionMesh, FString& OutError)
	{
		OutProjectionMesh = FCarrierLabVizProjectionMesh();
		OutProjectionMesh.Mesh.EnableTriangleGroups();
		OutProjectionMesh.PlateVertexOffsets.Init(INDEX_NONE, State.Plates.Num());
		OutProjectionMesh.PlateCount = State.Plates.Num();

		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			if (!OutProjectionMesh.PlateVertexOffsets.IsValidIndex(Plate.PlateId))
			{
				OutError = FString::Printf(TEXT("Invalid plate id %d while building combined projection mesh."), Plate.PlateId);
				return false;
			}

			TArray<int32> LocalToMeshVertex;
			LocalToMeshVertex.SetNum(Plate.Vertices.Num());
			OutProjectionMesh.PlateVertexOffsets[Plate.PlateId] = OutProjectionMesh.Mesh.MaxVertexID();
			for (int32 LocalVertexId = 0; LocalVertexId < Plate.Vertices.Num(); ++LocalVertexId)
			{
				LocalToMeshVertex[LocalVertexId] = OutProjectionMesh.Mesh.AppendVertex(Plate.Vertices[LocalVertexId].UnitPosition);
			}

			for (int32 LocalTriangleId = 0; LocalTriangleId < Plate.LocalTriangles.Num(); ++LocalTriangleId)
			{
				const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
				if (!LocalToMeshVertex.IsValidIndex(Triangle.A) ||
					!LocalToMeshVertex.IsValidIndex(Triangle.B) ||
					!LocalToMeshVertex.IsValidIndex(Triangle.C))
				{
					OutError = FString::Printf(TEXT("Combined projection mesh rejected plate %d local triangle %d with invalid vertex ids."), Plate.PlateId, LocalTriangleId);
					return false;
				}

				const int32 MeshTriangleId = OutProjectionMesh.Mesh.AppendTriangle(
					LocalToMeshVertex[Triangle.A],
					LocalToMeshVertex[Triangle.B],
					LocalToMeshVertex[Triangle.C],
					0);
				if (MeshTriangleId < 0)
				{
					OutError = FString::Printf(TEXT("FDynamicMesh3 rejected combined projection plate %d local triangle %d."), Plate.PlateId, LocalTriangleId);
					return false;
				}

				if (!OutProjectionMesh.TriangleRefsByMeshTriangleId.IsValidIndex(MeshTriangleId))
				{
					OutProjectionMesh.TriangleRefsByMeshTriangleId.SetNum(MeshTriangleId + 1);
				}
				OutProjectionMesh.TriangleRefsByMeshTriangleId[MeshTriangleId].PlateId = Plate.PlateId;
				OutProjectionMesh.TriangleRefsByMeshTriangleId[MeshTriangleId].LocalTriangleId = LocalTriangleId;
			}
		}

		OutProjectionMesh.Tree = MakeUnique<FDynamicMeshAABBTree3>(&OutProjectionMesh.Mesh, false);
		return true;
	}

	bool RefreshProjectionRayMeshVerticesAndTree(const CarrierLab::FCarrierState& State, FCarrierLabVizProjectionMesh& ProjectionMesh, FString& OutError)
	{
		if (ProjectionMesh.PlateCount != State.Plates.Num() || ProjectionMesh.PlateVertexOffsets.Num() != State.Plates.Num())
		{
			OutError = TEXT("Combined projection mesh plate count does not match carrier plate count.");
			return false;
		}

		int32 ExpectedTriangleCount = 0;
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			ExpectedTriangleCount += Plate.LocalTriangles.Num();
			if (!ProjectionMesh.PlateVertexOffsets.IsValidIndex(Plate.PlateId))
			{
				OutError = FString::Printf(TEXT("Invalid plate id %d while refreshing combined projection mesh."), Plate.PlateId);
				return false;
			}

			const int32 VertexOffset = ProjectionMesh.PlateVertexOffsets[Plate.PlateId];
			if (VertexOffset < 0 || VertexOffset + Plate.Vertices.Num() > ProjectionMesh.Mesh.MaxVertexID())
			{
				OutError = FString::Printf(TEXT("Combined projection mesh vertex span for plate %d no longer matches carrier topology."), Plate.PlateId);
				return false;
			}

			for (int32 LocalVertexId = 0; LocalVertexId < Plate.Vertices.Num(); ++LocalVertexId)
			{
				const int32 MeshVertexId = VertexOffset + LocalVertexId;
				if (!ProjectionMesh.Mesh.IsVertex(MeshVertexId))
				{
					OutError = FString::Printf(TEXT("Combined projection mesh missing plate %d local vertex %d."), Plate.PlateId, LocalVertexId);
					return false;
				}
				ProjectionMesh.Mesh.SetVertex(MeshVertexId, Plate.Vertices[LocalVertexId].UnitPosition, false);
			}
		}

		if (ProjectionMesh.Mesh.TriangleCount() != ExpectedTriangleCount ||
			ProjectionMesh.TriangleRefsByMeshTriangleId.Num() < ProjectionMesh.Mesh.MaxTriangleID())
		{
			OutError = TEXT("Combined projection mesh triangle metadata no longer matches carrier topology.");
			return false;
		}

		if (!ProjectionMesh.Tree.IsValid())
		{
			ProjectionMesh.Tree = MakeUnique<FDynamicMeshAABBTree3>(&ProjectionMesh.Mesh, false);
		}
		ProjectionMesh.Tree->Build();
		return true;
	}

	double InterpolateContinentalFraction(const CarrierLab::FCarrierPlate& Plate, const FCarrierLabVizCandidate& Candidate)
	{
		if (!Plate.LocalTriangles.IsValidIndex(Candidate.LocalTriangleId))
		{
			return 0.0;
		}
		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[Candidate.LocalTriangleId];
		if (!Plate.Vertices.IsValidIndex(Triangle.A) || !Plate.Vertices.IsValidIndex(Triangle.B) || !Plate.Vertices.IsValidIndex(Triangle.C))
		{
			return 0.0;
		}
		return FMath::Clamp(
			Candidate.Bary.X * Plate.Vertices[Triangle.A].ContinentalFraction +
			Candidate.Bary.Y * Plate.Vertices[Triangle.B].ContinentalFraction +
			Candidate.Bary.Z * Plate.Vertices[Triangle.C].ContinentalFraction,
			0.0,
			1.0);
	}

	ECarrierLabPhaseIISourceTriangleUniformity ClassifyHitTriangleUniformity(
		const CarrierLab::FCarrierState& State,
		const int32 PlateId,
		const int32 LocalTriangleId,
		int32& OutContinentalVertexCount)
	{
		OutContinentalVertexCount = INDEX_NONE;
		if (!State.Plates.IsValidIndex(PlateId))
		{
			return ECarrierLabPhaseIISourceTriangleUniformity::Unknown;
		}

		const CarrierLab::FCarrierPlate& Plate = State.Plates[PlateId];
		if (!Plate.LocalTriangles.IsValidIndex(LocalTriangleId))
		{
			return ECarrierLabPhaseIISourceTriangleUniformity::Unknown;
		}

		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
		if (!Plate.Vertices.IsValidIndex(Triangle.A) ||
			!Plate.Vertices.IsValidIndex(Triangle.B) ||
			!Plate.Vertices.IsValidIndex(Triangle.C))
		{
			return ECarrierLabPhaseIISourceTriangleUniformity::Unknown;
		}

		OutContinentalVertexCount = 0;
		OutContinentalVertexCount += Plate.Vertices[Triangle.A].ContinentalFraction >= 0.5 ? 1 : 0;
		OutContinentalVertexCount += Plate.Vertices[Triangle.B].ContinentalFraction >= 0.5 ? 1 : 0;
		OutContinentalVertexCount += Plate.Vertices[Triangle.C].ContinentalFraction >= 0.5 ? 1 : 0;

		if (OutContinentalVertexCount == 0)
		{
			return ECarrierLabPhaseIISourceTriangleUniformity::UniformOceanic;
		}
		if (OutContinentalVertexCount == 3)
		{
			return ECarrierLabPhaseIISourceTriangleUniformity::UniformContinental;
		}
		return ECarrierLabPhaseIISourceTriangleUniformity::Mixed;
	}

	void QuerySampleCandidates(
		const CarrierLab::FCarrierState& State,
		const TArray<FCarrierLabVizPlateMesh>& PlateMeshes,
		const CarrierLab::FSphereSample& Sample,
		TArray<FCarrierLabVizCandidate>& Candidates,
		uint64& PlateMask,
		bool& bAnyBoundary)
	{
		Candidates.Reset();
		PlateMask = 0;
		bAnyBoundary = false;

		IMeshSpatial::FQueryOptions QueryOptions(2.0 + 1.0e-6);
		TArray<MeshIntersection::FHitIntersectionResult> Hits;
		const FRay3d Ray(FVector3d::Zero(), Sample.UnitPosition);
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			if (!PlateMeshes.IsValidIndex(Plate.PlateId) || !PlateMeshes[Plate.PlateId].Tree.IsValid())
			{
				continue;
			}

			Hits.Reset();
			PlateMeshes[Plate.PlateId].Tree->FindAllHitTriangles(Ray, Hits, QueryOptions);
			bool bAcceptedHit = false;
			for (const MeshIntersection::FHitIntersectionResult& Hit : Hits)
			{
				if (Hit.Distance <= 0.0 || Hit.Distance > 2.0 + 1.0e-6)
				{
					continue;
				}
				const int32* LocalTriangleId = PlateMeshes[Plate.PlateId].MeshTriangleIdToLocalTriangleId.Find(Hit.TriangleId);
				if (LocalTriangleId == nullptr)
				{
					continue;
				}

				FCarrierLabVizCandidate Candidate;
				Candidate.PlateId = Plate.PlateId;
				Candidate.LocalTriangleId = *LocalTriangleId;
				Candidate.Bary = Hit.BaryCoords;
				Candidate.Distance = Hit.Distance;
				Candidate.bBoundary = IsBoundaryHit(Hit.BaryCoords);
				bAnyBoundary = bAnyBoundary || Candidate.bBoundary;
				PlateMask |= (1ull << static_cast<uint64>(Plate.PlateId));
				Candidates.Add(Candidate);
				bAcceptedHit = true;
			}

			if (!bAcceptedHit)
			{
				double NearestDistSqr = TNumericLimits<double>::Max();
				const int32 NearestTriangleId = PlateMeshes[Plate.PlateId].Tree->FindNearestTriangle(Sample.UnitPosition, NearestDistSqr, IMeshSpatial::FQueryOptions(1.0e-8));
				const int32* LocalTriangleId = PlateMeshes[Plate.PlateId].MeshTriangleIdToLocalTriangleId.Find(NearestTriangleId);
				if (LocalTriangleId != nullptr && NearestDistSqr <= 1.0e-16)
				{
					const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[*LocalTriangleId];
					FVector3d Bary = FVector3d(1.0, 0.0, 0.0);
					if (Plate.Vertices.IsValidIndex(Triangle.A) &&
						Plate.Vertices.IsValidIndex(Triangle.B) &&
						Plate.Vertices.IsValidIndex(Triangle.C))
					{
						if (!IntersectRayWithTriangle(
							Sample.UnitPosition,
							Plate.Vertices[Triangle.A].UnitPosition,
							Plate.Vertices[Triangle.B].UnitPosition,
							Plate.Vertices[Triangle.C].UnitPosition,
							Bary))
						{
							const double DotA = FVector3d::DotProduct(Sample.UnitPosition, Plate.Vertices[Triangle.A].UnitPosition);
							const double DotB = FVector3d::DotProduct(Sample.UnitPosition, Plate.Vertices[Triangle.B].UnitPosition);
							const double DotC = FVector3d::DotProduct(Sample.UnitPosition, Plate.Vertices[Triangle.C].UnitPosition);
							Bary = DotB >= DotA && DotB >= DotC ? FVector3d(0.0, 1.0, 0.0) :
								(DotC >= DotA && DotC >= DotB ? FVector3d(0.0, 0.0, 1.0) : FVector3d(1.0, 0.0, 0.0));
						}
					}

					FCarrierLabVizCandidate Candidate;
					Candidate.PlateId = Plate.PlateId;
					Candidate.LocalTriangleId = *LocalTriangleId;
					Candidate.Bary = Bary;
					Candidate.Distance = 1.0;
					Candidate.bBoundary = true;
					bAnyBoundary = true;
					PlateMask |= (1ull << static_cast<uint64>(Plate.PlateId));
					Candidates.Add(Candidate);
				}
			}
		}
	}

	void QuerySampleCandidates(
		const CarrierLab::FCarrierState& State,
		const FCarrierLabVizProjectionMesh& ProjectionMesh,
		const CarrierLab::FSphereSample& Sample,
		TArray<FCarrierLabVizCandidate>& Candidates,
		uint64& PlateMask,
		bool& bAnyBoundary)
	{
		Candidates.Reset();
		PlateMask = 0;
		bAnyBoundary = false;

		if (!ProjectionMesh.Tree.IsValid())
		{
			return;
		}

		IMeshSpatial::FQueryOptions QueryOptions(2.0 + 1.0e-6);
		TArray<MeshIntersection::FHitIntersectionResult> Hits;
		const FRay3d Ray(FVector3d::Zero(), Sample.UnitPosition);
		ProjectionMesh.Tree->FindAllHitTriangles(Ray, Hits, QueryOptions);
		for (const MeshIntersection::FHitIntersectionResult& Hit : Hits)
		{
			if (Hit.Distance <= 0.0 || Hit.Distance > 2.0 + 1.0e-6 || !ProjectionMesh.TriangleRefsByMeshTriangleId.IsValidIndex(Hit.TriangleId))
			{
				continue;
			}

			const FCarrierLabVizProjectionTriangleRef& TriangleRef = ProjectionMesh.TriangleRefsByMeshTriangleId[Hit.TriangleId];
			if (!State.Plates.IsValidIndex(TriangleRef.PlateId) ||
				!State.Plates[TriangleRef.PlateId].LocalTriangles.IsValidIndex(TriangleRef.LocalTriangleId))
			{
				continue;
			}

			FCarrierLabVizCandidate Candidate;
			Candidate.PlateId = TriangleRef.PlateId;
			Candidate.LocalTriangleId = TriangleRef.LocalTriangleId;
			Candidate.Bary = Hit.BaryCoords;
			Candidate.Distance = Hit.Distance;
			Candidate.bBoundary = IsBoundaryHit(Hit.BaryCoords);
			bAnyBoundary = bAnyBoundary || Candidate.bBoundary;
			PlateMask |= (1ull << static_cast<uint64>(TriangleRef.PlateId));
			Candidates.Add(Candidate);
		}

		if (Candidates.IsEmpty())
		{
			double NearestDistSqr = TNumericLimits<double>::Max();
			const int32 NearestTriangleId = ProjectionMesh.Tree->FindNearestTriangle(Sample.UnitPosition, NearestDistSqr, IMeshSpatial::FQueryOptions(1.0e-8));
			if (ProjectionMesh.TriangleRefsByMeshTriangleId.IsValidIndex(NearestTriangleId) && NearestDistSqr <= 1.0e-16)
			{
				const FCarrierLabVizProjectionTriangleRef& TriangleRef = ProjectionMesh.TriangleRefsByMeshTriangleId[NearestTriangleId];
				if (State.Plates.IsValidIndex(TriangleRef.PlateId) && State.Plates[TriangleRef.PlateId].LocalTriangles.IsValidIndex(TriangleRef.LocalTriangleId))
				{
					const CarrierLab::FCarrierPlate& Plate = State.Plates[TriangleRef.PlateId];
					const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[TriangleRef.LocalTriangleId];
					FVector3d Bary = FVector3d(1.0, 0.0, 0.0);
					if (Plate.Vertices.IsValidIndex(Triangle.A) &&
						Plate.Vertices.IsValidIndex(Triangle.B) &&
						Plate.Vertices.IsValidIndex(Triangle.C))
					{
						if (!IntersectRayWithTriangle(
							Sample.UnitPosition,
							Plate.Vertices[Triangle.A].UnitPosition,
							Plate.Vertices[Triangle.B].UnitPosition,
							Plate.Vertices[Triangle.C].UnitPosition,
							Bary))
						{
							const double DotA = FVector3d::DotProduct(Sample.UnitPosition, Plate.Vertices[Triangle.A].UnitPosition);
							const double DotB = FVector3d::DotProduct(Sample.UnitPosition, Plate.Vertices[Triangle.B].UnitPosition);
							const double DotC = FVector3d::DotProduct(Sample.UnitPosition, Plate.Vertices[Triangle.C].UnitPosition);
							Bary = DotB >= DotA && DotB >= DotC ? FVector3d(0.0, 1.0, 0.0) :
								(DotC >= DotA && DotC >= DotB ? FVector3d(0.0, 0.0, 1.0) : FVector3d(1.0, 0.0, 0.0));
						}
					}

					FCarrierLabVizCandidate Candidate;
					Candidate.PlateId = TriangleRef.PlateId;
					Candidate.LocalTriangleId = TriangleRef.LocalTriangleId;
					Candidate.Bary = Bary;
					Candidate.Distance = 1.0;
					Candidate.bBoundary = true;
					bAnyBoundary = true;
					PlateMask |= (1ull << static_cast<uint64>(TriangleRef.PlateId));
					Candidates.Add(Candidate);
				}
			}
		}

		Candidates.Sort([](const FCarrierLabVizCandidate& Left, const FCarrierLabVizCandidate& Right)
		{
			if (Left.PlateId != Right.PlateId)
			{
				return Left.PlateId < Right.PlateId;
			}
			if (Left.LocalTriangleId != Right.LocalTriangleId)
			{
				return Left.LocalTriangleId < Right.LocalTriangleId;
			}
			return Left.Distance < Right.Distance;
		});
	}

	int32 ChooseNearestCandidatePlate(
		const CarrierLab::FSphereSample& Sample,
		const TArray<FCarrierLabVizCandidate>& Candidates,
		const TArray<FCarrierLabVisualizationMotion>& Motions)
	{
		int32 BestPlateId = INDEX_NONE;
		double BestDot = -TNumericLimits<double>::Max();
		for (const FCarrierLabVizCandidate& Candidate : Candidates)
		{
			if (!Motions.IsValidIndex(Candidate.PlateId))
			{
				continue;
			}
			const double Dot = FVector3d::DotProduct(Sample.UnitPosition, Motions[Candidate.PlateId].CurrentCenter);
			if (Dot > BestDot + 1.0e-12 ||
				(FMath::IsNearlyEqual(Dot, BestDot, 1.0e-12) && (BestPlateId == INDEX_NONE || Candidate.PlateId < BestPlateId)))
			{
				BestDot = Dot;
				BestPlateId = Candidate.PlateId;
			}
		}
		return BestPlateId;
	}

	int32 BoundaryBinKey(const FVector3d& UnitPosition)
	{
		const double Lon = FMath::Atan2(UnitPosition.Y, UnitPosition.X);
		const double Lat = FMath::Asin(FMath::Clamp(UnitPosition.Z, -1.0, 1.0));
		const int32 LonIndex = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble((Lon + UE_DOUBLE_PI) / (2.0 * UE_DOUBLE_PI) * BoundaryLonBins)), 0, BoundaryLonBins - 1);
		const int32 LatIndex = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble((Lat + 0.5 * UE_DOUBLE_PI) / UE_DOUBLE_PI * BoundaryLatBins)), 0, BoundaryLatBins - 1);
		return LatIndex * BoundaryLonBins + LonIndex;
	}

	void AddBoundaryPoint(FCarrierLabVizBoundaryIndex& Index, const int32 PlateId, const FVector3d& Position, const double ContinentalFraction)
	{
		FCarrierLabVizBoundaryPoint Point;
		Point.PlateId = PlateId;
		Point.UnitPosition = NormalizeOrFallback(Position, FVector3d::UnitZ());
		Point.ContinentalFraction = FMath::Clamp(ContinentalFraction, 0.0, 1.0);
		const int32 PointIndex = Index.Points.Add(Point);
		Index.Bins.FindOrAdd(BoundaryBinKey(Point.UnitPosition)).Add(PointIndex);
	}

	FCarrierLabVizBoundaryIndex BuildBoundaryIndex(const CarrierLab::FCarrierState& State)
	{
		FCarrierLabVizBoundaryIndex Index;
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			for (const CarrierLab::FCarrierPlateTriangle& Triangle : Plate.LocalTriangles)
			{
				if (!Plate.Vertices.IsValidIndex(Triangle.A) || !Plate.Vertices.IsValidIndex(Triangle.B) || !Plate.Vertices.IsValidIndex(Triangle.C))
				{
					continue;
				}
				const int32 Vertices[3] = {Triangle.A, Triangle.B, Triangle.C};
				for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
				{
					const CarrierLab::FCarrierVertex& A = Plate.Vertices[Vertices[EdgeIndex]];
					const CarrierLab::FCarrierVertex& B = Plate.Vertices[Vertices[(EdgeIndex + 1) % 3]];
					if (State.Samples.IsValidIndex(A.GlobalSampleId) &&
						State.Samples.IsValidIndex(B.GlobalSampleId) &&
						State.Samples[A.GlobalSampleId].PlateId != State.Samples[B.GlobalSampleId].PlateId)
					{
						AddBoundaryPoint(Index, Plate.PlateId, A.UnitPosition, A.ContinentalFraction);
						AddBoundaryPoint(Index, Plate.PlateId, B.UnitPosition, B.ContinentalFraction);
						AddBoundaryPoint(Index, Plate.PlateId, NormalizeOrFallback(A.UnitPosition + B.UnitPosition, A.UnitPosition), 0.5 * (A.ContinentalFraction + B.ContinentalFraction));
					}
				}
			}
		}
		return Index;
	}

	bool FindNearestBoundaryPair(
		const FCarrierLabVizBoundaryIndex& Index,
		const FVector3d& SamplePosition,
		FCarrierLabVizBoundaryPoint& OutQ1,
		FCarrierLabVizBoundaryPoint& OutQ2)
	{
		double BestDot = -TNumericLimits<double>::Max();
		double SecondDot = -TNumericLimits<double>::Max();
		const int32 CenterKey = BoundaryBinKey(SamplePosition);
		const int32 CenterLat = CenterKey / BoundaryLonBins;
		const int32 CenterLon = CenterKey % BoundaryLonBins;
		auto ConsiderPoint = [&](const int32 PointIndex)
		{
			if (!Index.Points.IsValidIndex(PointIndex))
			{
				return;
			}
			const FCarrierLabVizBoundaryPoint& Point = Index.Points[PointIndex];
			const double Dot = FVector3d::DotProduct(SamplePosition, Point.UnitPosition);
			if (Dot > BestDot)
			{
				if (Point.PlateId != OutQ1.PlateId)
				{
					OutQ2 = OutQ1;
					SecondDot = BestDot;
				}
				OutQ1 = Point;
				BestDot = Dot;
			}
			else if (Point.PlateId != OutQ1.PlateId && Dot > SecondDot)
			{
				OutQ2 = Point;
				SecondDot = Dot;
			}
		};

		for (int32 Radius = 0; Radius <= BoundarySearchRadiusBins; ++Radius)
		{
			for (int32 DLat = -Radius; DLat <= Radius; ++DLat)
			{
				const int32 Lat = CenterLat + DLat;
				if (Lat < 0 || Lat >= BoundaryLatBins)
				{
					continue;
				}
				for (int32 DLon = -Radius; DLon <= Radius; ++DLon)
				{
					const int32 Lon = (CenterLon + DLon + BoundaryLonBins) % BoundaryLonBins;
					if (const TArray<int32>* Bin = Index.Bins.Find(Lat * BoundaryLonBins + Lon))
					{
						for (const int32 PointIndex : *Bin)
						{
							ConsiderPoint(PointIndex);
						}
					}
				}
			}
			if (OutQ1.PlateId != INDEX_NONE && OutQ2.PlateId != INDEX_NONE)
			{
				return true;
			}
		}

		for (int32 PointIndex = 0; PointIndex < Index.Points.Num(); ++PointIndex)
		{
			ConsiderPoint(PointIndex);
		}
		return OutQ1.PlateId != INDEX_NONE && OutQ2.PlateId != INDEX_NONE;
	}

	double SignedPairSeparationVelocityForPlatePair(
		const FVector3d& Position,
		const TArray<FCarrierLabVisualizationMotion>& Motions,
		const int32 A,
		const int32 B)
	{
		if (!Motions.IsValidIndex(A) || !Motions.IsValidIndex(B))
		{
			return 0.0;
		}
		const FVector3d ToB = NormalizeOrFallback(
			Motions[B].CurrentCenter - FVector3d::DotProduct(Motions[B].CurrentCenter, Position) * Position,
			FVector3d::UnitX());
		const FVector3d VelocityA = FVector3d::CrossProduct(Motions[A].Axis, Position) * Motions[A].AngularSpeedRadiansPerStep;
		const FVector3d VelocityB = FVector3d::CrossProduct(Motions[B].Axis, Position) * Motions[B].AngularSpeedRadiansPerStep;
		return FVector3d::DotProduct(VelocityB - VelocityA, ToB);
	}

	const FCarrierLabVizCandidate* FindCandidateForPlate(const TArray<FCarrierLabVizCandidate>& Candidates, const int32 PlateId)
	{
		for (const FCarrierLabVizCandidate& Candidate : Candidates)
		{
			if (Candidate.PlateId == PlateId)
			{
				return &Candidate;
			}
		}
		return nullptr;
	}

	void ConfigureDefaultMotions(
		const CarrierLab::FCarrierState& State,
		const double VelocityMmPerYear,
		TArray<FCarrierLabVisualizationMotion>& OutMotions)
	{
		OutMotions.Reset(State.Plates.Num());
		OutMotions.SetNum(State.Plates.Num());
		const double AngularSpeed = AngularSpeedRadiansPerStep(VelocityMmPerYear);
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			const FVector3d SeedVector = NormalizeOrFallback(
				FVector3d(
					FMath::Sin(static_cast<double>(Plate.PlateId + 1) * 12.9898),
					FMath::Sin(static_cast<double>(Plate.PlateId + 1) * 78.233),
					FMath::Sin(static_cast<double>(Plate.PlateId + 1) * 37.719)),
				FVector3d::UnitZ());
			FVector3d Axis = FVector3d::CrossProduct(Plate.InitialCenter, SeedVector);
			if (Axis.SquaredLength() <= UE_SMALL_NUMBER)
			{
				Axis = FVector3d::CrossProduct(Plate.InitialCenter, FVector3d::UnitX());
			}
			OutMotions[Plate.PlateId].Axis = NormalizeOrFallback(Axis, FVector3d::UnitZ());
			OutMotions[Plate.PlateId].AngularSpeedRadiansPerStep = AngularSpeed;
			OutMotions[Plate.PlateId].CurrentCenter = Plate.InitialCenter;
		}
	}

	void ConfigureZeroMotions(
		const CarrierLab::FCarrierState& State,
		TArray<FCarrierLabVisualizationMotion>& OutMotions)
	{
		OutMotions.Reset(State.Plates.Num());
		OutMotions.SetNum(State.Plates.Num());
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			OutMotions[Plate.PlateId].Axis = FVector3d::UnitZ();
			OutMotions[Plate.PlateId].AngularSpeedRadiansPerStep = 0.0;
			OutMotions[Plate.PlateId].CurrentCenter = Plate.InitialCenter;
		}
	}

	void ConfigureForcedPairMotions(
		const CarrierLab::FCarrierState& State,
		const double VelocityMmPerYear,
		const bool bConvergent,
		TArray<FCarrierLabVisualizationMotion>& OutMotions)
	{
		ConfigureZeroMotions(State, OutMotions);
		if (State.Plates.Num() < 2)
		{
			return;
		}

		const double AngularSpeed = AngularSpeedRadiansPerStep(VelocityMmPerYear) * 1.8;
		for (int32 PlateId = 0; PlateId < 2; ++PlateId)
		{
			const CarrierLab::FCarrierPlate& Plate = State.Plates[PlateId];
			const FVector3d OtherCenter = State.Plates[1 - PlateId].InitialCenter;
			FVector3d Direction = OtherCenter - FVector3d::DotProduct(OtherCenter, Plate.InitialCenter) * Plate.InitialCenter;
			if (!bConvergent)
			{
				Direction *= -1.0;
			}
			const FVector3d Axis = FVector3d::CrossProduct(Plate.InitialCenter, Direction);
			OutMotions[PlateId].Axis = NormalizeOrFallback(Axis, FVector3d::UnitZ());
			OutMotions[PlateId].AngularSpeedRadiansPerStep = AngularSpeed;
			OutMotions[PlateId].CurrentCenter = Plate.InitialCenter;
		}
	}

	void ConfigureTripleJunctionMotions(
		const CarrierLab::FCarrierState& State,
		const double VelocityMmPerYear,
		TArray<FCarrierLabVisualizationMotion>& OutMotions)
	{
		ConfigureZeroMotions(State, OutMotions);
		if (State.Plates.Num() < 3)
		{
			return;
		}

		const FVector3d Target = NormalizeOrFallback(
			State.Plates[0].InitialCenter + State.Plates[1].InitialCenter + State.Plates[2].InitialCenter,
			State.Plates[0].InitialCenter);
		const double AngularSpeed = AngularSpeedRadiansPerStep(VelocityMmPerYear) * 2.25;
		for (int32 PlateId = 0; PlateId < 3; ++PlateId)
		{
			const CarrierLab::FCarrierPlate& Plate = State.Plates[PlateId];
			FVector3d Direction = Target - FVector3d::DotProduct(Target, Plate.InitialCenter) * Plate.InitialCenter;
			if (Direction.SquaredLength() <= UE_SMALL_NUMBER)
			{
				Direction = State.Plates[(PlateId + 1) % 3].InitialCenter - FVector3d::DotProduct(State.Plates[(PlateId + 1) % 3].InitialCenter, Plate.InitialCenter) * Plate.InitialCenter;
			}
			const FVector3d Axis = FVector3d::CrossProduct(Plate.InitialCenter, Direction);
			OutMotions[PlateId].Axis = NormalizeOrFallback(Axis, FVector3d::UnitZ());
			OutMotions[PlateId].AngularSpeedRadiansPerStep = AngularSpeed;
			OutMotions[PlateId].CurrentCenter = Plate.InitialCenter;
		}
	}

	TArray<int32> UniqueCandidatePlates(const TArray<FCarrierLabVizCandidate>& Candidates)
	{
		TArray<int32> PlateIds;
		for (const FCarrierLabVizCandidate& Candidate : Candidates)
		{
			PlateIds.AddUnique(Candidate.PlateId);
		}
		PlateIds.Sort();
		return PlateIds;
	}

	bool IsSeparatingKinematics(const FVector3d& Position, const TArray<FCarrierLabVisualizationMotion>& Motions)
	{
		int32 BestA = INDEX_NONE;
		int32 BestB = INDEX_NONE;
		double DotA = -TNumericLimits<double>::Max();
		double DotB = -TNumericLimits<double>::Max();
		for (int32 PlateId = 0; PlateId < Motions.Num(); ++PlateId)
		{
			const double Dot = FVector3d::DotProduct(Position, Motions[PlateId].CurrentCenter);
			if (Dot > DotA)
			{
				DotB = DotA;
				BestB = BestA;
				DotA = Dot;
				BestA = PlateId;
			}
			else if (Dot > DotB)
			{
				DotB = Dot;
				BestB = PlateId;
			}
		}
		return SignedPairSeparationVelocityForPlatePair(Position, Motions, BestA, BestB) > 0.0;
	}

	int32 ChooseSyntheticSubductionCandidatePlate(
		const CarrierLab::FSphereSample& Sample,
		const TArray<FCarrierLabVizCandidate>& Candidates,
		const TArray<FCarrierLabVisualizationMotion>& Motions)
	{
		const TArray<int32> PlateIds = UniqueCandidatePlates(Candidates);
		if (PlateIds.Num() <= 1 || IsSeparatingKinematics(Sample.UnitPosition, Motions))
		{
			return ChooseNearestCandidatePlate(Sample, Candidates, Motions);
		}

		const int32 LowerIdSubductingPlate = PlateIds[0];
		TArray<FCarrierLabVizCandidate> RemainingCandidates;
		for (const FCarrierLabVizCandidate& Candidate : Candidates)
		{
			if (Candidate.PlateId != LowerIdSubductingPlate)
			{
				RemainingCandidates.Add(Candidate);
			}
		}
		return RemainingCandidates.IsEmpty()
			? ChooseNearestCandidatePlate(Sample, Candidates, Motions)
			: ChooseNearestCandidatePlate(Sample, RemainingCandidates, Motions);
	}

	int32 ChooseRandomSeededCandidatePlate(
		const CarrierLab::FSphereSample& Sample,
		const TArray<FCarrierLabVizCandidate>& Candidates,
		const int32 TieBreakSeed,
		const int32 Step,
		const int32 EventId)
	{
		const TArray<int32> PlateIds = UniqueCandidatePlates(Candidates);
		if (PlateIds.IsEmpty())
		{
			return INDEX_NONE;
		}
		uint64 Hash = 1469598103934665603ull;
		HashMix(Hash, static_cast<uint64>(TieBreakSeed + 1));
		HashMix(Hash, static_cast<uint64>(Sample.Id + 1));
		HashMix(Hash, static_cast<uint64>(Step + 1));
		HashMix(Hash, static_cast<uint64>(EventId + 1));
		return PlateIds[static_cast<int32>(Hash % static_cast<uint64>(PlateIds.Num()))];
	}

	int32 ChooseCandidatePlateByPolicy(
		const CarrierLab::FSphereSample& Sample,
		const TArray<FCarrierLabVizCandidate>& Candidates,
		const TArray<FCarrierLabVisualizationMotion>& Motions,
		const ECarrierLabMultiHitPolicy Policy,
		const int32 TieBreakSeed,
		const int32 Step,
		const int32 EventId)
	{
		switch (Policy)
		{
		case ECarrierLabMultiHitPolicy::SyntheticSubduction:
			return ChooseSyntheticSubductionCandidatePlate(Sample, Candidates, Motions);
		case ECarrierLabMultiHitPolicy::RandomSeeded:
			return ChooseRandomSeededCandidatePlate(Sample, Candidates, TieBreakSeed, Step, EventId);
		case ECarrierLabMultiHitPolicy::Centroid:
		default:
			return ChooseNearestCandidatePlate(Sample, Candidates, Motions);
		}
	}

	FLinearColor PlateColor(const int32 PlateId)
	{
		if (PlateId == INDEX_NONE)
		{
			return FLinearColor(0.04f, 0.04f, 0.05f, 1.0f);
		}
		uint32 Hash = static_cast<uint32>(PlateId + 1) * 747796405u + 2891336453u;
		Hash = ((Hash >> ((Hash >> 28) + 4)) ^ Hash) * 277803737u;
		Hash = (Hash >> 22) ^ Hash;
		const uint8 Hue = static_cast<uint8>(Hash % 256u);
		return FLinearColor::MakeFromHSV8(Hue, 150, 230);
	}

	FLinearColor ContinentalColor(const double Fraction)
	{
		const double Alpha = FMath::Clamp(Fraction, 0.0, 1.0);
		const FLinearColor Ocean(0.02f, 0.18f, 0.44f, 1.0f);
		const FLinearColor Coast(0.12f, 0.52f, 0.40f, 1.0f);
		const FLinearColor Land(0.76f, 0.62f, 0.30f, 1.0f);
		return Alpha < 0.5
			? FMath::Lerp(Ocean, Coast, static_cast<float>(Alpha * 2.0))
			: FMath::Lerp(Coast, Land, static_cast<float>((Alpha - 0.5) * 2.0));
	}

	FVector4f ToVector4f(const FLinearColor& Color)
	{
		return FVector4f(Color.R, Color.G, Color.B, Color.A);
	}

	FLinearColor DriftColor(const double ErrorKm)
	{
		if (ErrorKm < 0.0)
		{
			return FLinearColor(0.02f, 0.05f, 0.09f, 1.0f);
		}
		const double Alpha = FMath::Clamp(ErrorKm / 1.0e-3, 0.0, 1.0);
		return FMath::Lerp(
			FLinearColor(0.08f, 0.20f, 0.36f, 1.0f),
			FLinearColor(1.0f, 0.10f, 0.02f, 1.0f),
			static_cast<float>(Alpha));
	}
}

ACarrierLabVisualizationActor::ACarrierLabVisualizationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoReceiveInput = EAutoReceiveInput::Player0;

	MeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("CarrierMesh"));
	RootComponent = MeshComponent;
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetColorOverrideMode(EDynamicMeshComponentColorOverrideMode::VertexColors);
	MeshComponent->SetVertexColorSpaceTransformMode(EDynamicMeshVertexColorTransformMode::NoTransform);
	MeshComponent->SetTwoSided(true);
	MeshComponent->SetEnableFlatShading(true);
	MeshComponent->SetEnableWireframeRenderPass(true);
}

void ACarrierLabVisualizationActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!HasAnyFlags(RF_ClassDefaultObject) && bAutoInitialize)
	{
		InitializeCarrier();
	}
}

void ACarrierLabVisualizationActor::BeginPlay()
{
	Super::BeginPlay();
	bPlaying = bPlayOnBegin;
	if (bAutoInitialize && !bInitialized)
	{
		InitializeCarrier();
	}
	BindInputControls();
}

void ACarrierLabVisualizationActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPlaying && bInitialized)
	{
		StepAccumulator += FMath::Max(0.0, DeltaSeconds) * StepsPerSecond;
		int32 StepsThisFrame = 0;
		while (StepAccumulator >= 1.0 && StepsThisFrame < 8)
		{
			AdvanceOneStep();
			StepAccumulator -= 1.0;
			++StepsThisFrame;
		}
		if (StepsThisFrame > 0)
		{
			ProjectCurrentCarrier();
		}
	}
	if (bShowHud)
	{
		ShowHud();
	}
}

bool ACarrierLabVisualizationActor::InitializeCarrier()
{
	CarrierLab::FStage0Config Config;
	Config.SampleCount = FMath::Max(12, SampleCount);
	Config.PlateCount = FMath::Clamp(PlateCount, 1, FMath::Min(63, Config.SampleCount));
	Config.Seed = Seed;
	Config.ContinentalPlateFraction = FMath::Clamp(ContinentalPlateFraction, 0.0, 1.0);

	FString Error;
	CarrierLab::FCarrierState NewState;
	if (!CarrierLab::FCarrierLabStage0::BuildColdStartCarrier(Config, NewState, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab visualization initialization failed: %s"), *Error);
		return false;
	}

	State = MoveTemp(NewState);
	CurrentMetrics = FCarrierLabVisualizationMetrics();
	RenderPlateIds.SetNum(State.Samples.Num());
	RenderContinentalFractions.SetNum(State.Samples.Num());
	DriftErrorKmBySample.SetNum(State.Samples.Num());
	MissMask.SetNum(State.Samples.Num());
	OverlapMask.SetNum(State.Samples.Num());
	BoundaryMask.SetNum(State.Samples.Num());
	PlateBoundaryMask.SetNum(State.Samples.Num());

	ConfigureDefaultMotions(State, VelocityMmPerYear, Motions);

	bInitialized = true;
	bPlateRayMeshTopologyDirty = true;
	PlateRayMeshes.Reset();
	bProjectionRayMeshTopologyDirty = true;
	ProjectionRayMesh = FCarrierLabVizProjectionMesh();
	bRenderMeshTopologyDirty = true;
	CachedRenderMeshSampleCount = 0;
	CachedRenderMeshTriangleCount = 0;
	StepAccumulator = 0.0;
	CurrentMetrics.NextResampleStep = GetNaturalCadenceSteps();
	CaptureDriftReference();
	ProjectCurrentCarrier();
	return true;
}

bool ACarrierLabVisualizationActor::ResetCarrier()
{
	bPlaying = false;
	return InitializeCarrier();
}

void ACarrierLabVisualizationActor::ConfigurePhaseIIMotionFixture(const ECarrierLabPhaseIIMotionFixture Fixture)
{
	if (!bInitialized)
	{
		return;
	}

	switch (Fixture)
	{
	case ECarrierLabPhaseIIMotionFixture::Zero:
		ConfigureZeroMotions(State, Motions);
		break;
	case ECarrierLabPhaseIIMotionFixture::ForcedConvergence:
		ConfigureForcedPairMotions(State, VelocityMmPerYear, true, Motions);
		break;
	case ECarrierLabPhaseIIMotionFixture::ForcedDivergence:
		ConfigureForcedPairMotions(State, VelocityMmPerYear, false, Motions);
		break;
	case ECarrierLabPhaseIIMotionFixture::TripleJunction:
		ConfigureTripleJunctionMotions(State, VelocityMmPerYear, Motions);
		break;
	case ECarrierLabPhaseIIMotionFixture::Default:
	default:
		ConfigureDefaultMotions(State, VelocityMmPerYear, Motions);
		break;
	}

	CaptureDriftReference();
}

bool ACarrierLabVisualizationActor::DetectPhaseIIContacts(TArray<FCarrierLabPhaseIIContactRecord>& OutContacts, FCarrierLabPhaseIIContactMetrics& OutMetrics)
{
	OutContacts.Reset();
	OutMetrics = FCarrierLabPhaseIIContactMetrics();
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}

	const double StartSeconds = FPlatformTime::Seconds();
	FString MeshError;
	if (!RefreshProjectionRayMesh(MeshError))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab Phase II contact detection failed: %s"), *MeshError);
		return false;
	}

	OutMetrics.Step = CurrentMetrics.Step;
	OutMetrics.SampleCount = State.Samples.Num();
	OutMetrics.PlateCount = State.Plates.Num();

	TArray<FCarrierLabVizCandidate> Candidates;
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (!FMath::IsFinite(Sample.UnitPosition.X) ||
			!FMath::IsFinite(Sample.UnitPosition.Y) ||
			!FMath::IsFinite(Sample.UnitPosition.Z))
		{
			++OutMetrics.NaNOrInfCount;
			continue;
		}

		uint64 PlateMask = 0;
		bool bAnyBoundary = false;
		QuerySampleCandidates(State, ProjectionRayMesh, Sample, Candidates, PlateMask, bAnyBoundary);
		const TArray<int32> PlateIds = UniqueCandidatePlates(Candidates);
		if (PlateIds.Num() < 2)
		{
			continue;
		}

		++OutMetrics.RawEvidenceSampleCount;
		const bool bThirdPlate = PlateIds.Num() >= 3;
		for (int32 AIndex = 0; AIndex < PlateIds.Num(); ++AIndex)
		{
			for (int32 BIndex = AIndex + 1; BIndex < PlateIds.Num(); ++BIndex)
			{
				const int32 PlateA = PlateIds[AIndex];
				const int32 PlateB = PlateIds[BIndex];
				const FCarrierLabVizCandidate* CandidateA = FindCandidateForPlate(Candidates, PlateA);
				const FCarrierLabVizCandidate* CandidateB = FindCandidateForPlate(Candidates, PlateB);

				FCarrierLabPhaseIIContactRecord Record;
				Record.ContactId = OutContacts.Num();
				Record.Step = CurrentMetrics.Step;
				Record.SampleId = Sample.Id;
				Record.PlateA = PlateA;
				Record.PlateB = PlateB;
				Record.PlateALocalTriangleId = CandidateA != nullptr ? CandidateA->LocalTriangleId : INDEX_NONE;
				Record.PlateBLocalTriangleId = CandidateB != nullptr ? CandidateB->LocalTriangleId : INDEX_NONE;
				Record.EvidenceUnitPosition = Sample.UnitPosition;
				Record.SignedConvergenceVelocity = -SignedPairSeparationVelocityForPlatePair(Sample.UnitPosition, Motions, PlateA, PlateB);
				Record.VelocityMargin = PhaseIIContactVelocityMargin;
				Record.bBoundaryEvidence = bAnyBoundary ||
					(CandidateA != nullptr && CandidateA->bBoundary) ||
					(CandidateB != nullptr && CandidateB->bBoundary);
				Record.bThirdPlate = bThirdPlate;
				const bool bBoundaryOnlyDegeneracy =
					Record.bBoundaryEvidence &&
					CandidateA != nullptr &&
					CandidateB != nullptr &&
					CandidateA->bBoundary &&
					CandidateB->bBoundary;

				if (bThirdPlate)
				{
					Record.ContactClass = ECarrierLabPhaseIIContactClass::ThirdPlate;
					++OutMetrics.ThirdPlateContactCount;
				}
				else if (bBoundaryOnlyDegeneracy)
				{
					Record.ContactClass = ECarrierLabPhaseIIContactClass::TransformLowMargin;
					++OutMetrics.TransformLowMarginContactCount;
				}
				else if (Record.SignedConvergenceVelocity > PhaseIIContactVelocityMargin)
				{
					Record.ContactClass = ECarrierLabPhaseIIContactClass::Convergent;
					++OutMetrics.ConvergentContactCount;
					++OutMetrics.SubductionCandidateCount;
				}
				else if (Record.SignedConvergenceVelocity < -PhaseIIContactVelocityMargin)
				{
					Record.ContactClass = ECarrierLabPhaseIIContactClass::Divergent;
					++OutMetrics.DivergentContactCount;
				}
				else
				{
					Record.ContactClass = ECarrierLabPhaseIIContactClass::TransformLowMargin;
					++OutMetrics.TransformLowMarginContactCount;
				}

				if (Record.bBoundaryEvidence)
				{
					++OutMetrics.BoundaryEvidenceCount;
				}
				OutContacts.Add(Record);
			}
		}
	}

	OutMetrics.ContactRecordCount = OutContacts.Num();
	uint64 ContactHash = 1469598103934665603ull;
	HashMix(ContactHash, static_cast<uint64>(OutMetrics.Step + 1));
	HashMix(ContactHash, static_cast<uint64>(OutMetrics.SampleCount + 1));
	HashMix(ContactHash, static_cast<uint64>(OutMetrics.PlateCount + 1));
	for (const FCarrierLabPhaseIIContactRecord& Record : OutContacts)
	{
		HashMix(ContactHash, static_cast<uint64>(Record.ContactId + 1));
		HashMix(ContactHash, static_cast<uint64>(Record.Step + 1));
		HashMix(ContactHash, static_cast<uint64>(Record.SampleId + 1));
		HashMix(ContactHash, static_cast<uint64>(Record.PlateA + 1));
		HashMix(ContactHash, static_cast<uint64>(Record.PlateB + 1));
		HashMix(ContactHash, static_cast<uint64>(Record.PlateALocalTriangleId + 1));
		HashMix(ContactHash, static_cast<uint64>(Record.PlateBLocalTriangleId + 1));
		HashMixDouble(ContactHash, Record.EvidenceUnitPosition.X);
		HashMixDouble(ContactHash, Record.EvidenceUnitPosition.Y);
		HashMixDouble(ContactHash, Record.EvidenceUnitPosition.Z);
		HashMixDouble(ContactHash, Record.SignedConvergenceVelocity);
		HashMixDouble(ContactHash, Record.VelocityMargin);
		HashMix(ContactHash, Record.bBoundaryEvidence ? 1 : 0);
		HashMix(ContactHash, Record.bThirdPlate ? 1 : 0);
		HashMix(ContactHash, static_cast<uint64>(Record.ContactClass) + 1);
	}
	OutMetrics.ContactLogHash = HashToString(ContactHash);
	OutMetrics.ContactDetectionSeconds = FPlatformTime::Seconds() - StartSeconds;
	return true;
}

bool ACarrierLabVisualizationActor::BuildPhaseIITriangleLabels(
	const TArray<FCarrierLabPhaseIIContactRecord>& Contacts,
	const FCarrierLabPhaseIITriangleLabelConfig& Config,
	TArray<FCarrierLabPhaseIITriangleLabelRecord>& OutLabels,
	FCarrierLabPhaseIITriangleLabelMetrics& OutMetrics) const
{
	OutLabels.Reset();
	OutMetrics = FCarrierLabPhaseIITriangleLabelMetrics();
	if (!bInitialized)
	{
		return false;
	}

	const double StartSeconds = FPlatformTime::Seconds();
	OutMetrics.Step = CurrentMetrics.Step;
	OutMetrics.ContactRecordCount = Contacts.Num();

	TMap<int32, int32> LabelsPerContact;
	TSet<uint64> UniqueTriangles;
	double AreaProxy = 0.0;

	auto IsPlateContinental = [this](const int32 PlateId) -> bool
	{
		return State.Plates.IsValidIndex(PlateId) && State.Plates[PlateId].bContinental;
	};

	auto ContactUsesFixturePolarity = [&Config](const FCarrierLabPhaseIIContactRecord& Contact) -> bool
	{
		return Config.bUseFixturePolarity &&
			Config.FixtureUnderPlate != INDEX_NONE &&
			Config.FixtureOverPlate != INDEX_NONE &&
			((Contact.PlateA == Config.FixtureUnderPlate && Contact.PlateB == Config.FixtureOverPlate) ||
				(Contact.PlateB == Config.FixtureUnderPlate && Contact.PlateA == Config.FixtureOverPlate));
	};

	auto TriangleAreaProxy = [this](const int32 PlateId, const int32 LocalTriangleId) -> double
	{
		if (!State.Plates.IsValidIndex(PlateId))
		{
			return 0.0;
		}
		const CarrierLab::FCarrierPlate& Plate = State.Plates[PlateId];
		if (!Plate.LocalTriangles.IsValidIndex(LocalTriangleId))
		{
			return 0.0;
		}
		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
		double Sum = 0.0;
		int32 Count = 0;
		for (const int32 VertexId : {Triangle.A, Triangle.B, Triangle.C})
		{
			if (Plate.Vertices.IsValidIndex(VertexId))
			{
				Sum += Plate.Vertices[VertexId].AreaWeight;
				++Count;
			}
		}
		return Count > 0 ? Sum / static_cast<double>(Count) : 0.0;
	};

	auto DistanceFromContactKm = [this](const int32 PlateId, const int32 LocalTriangleId, const FVector3d& Evidence) -> double
	{
		if (!State.Plates.IsValidIndex(PlateId))
		{
			return 0.0;
		}
		const CarrierLab::FCarrierPlate& Plate = State.Plates[PlateId];
		if (!Plate.LocalTriangles.IsValidIndex(LocalTriangleId))
		{
			return 0.0;
		}
		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
		if (!Plate.Vertices.IsValidIndex(Triangle.A) ||
			!Plate.Vertices.IsValidIndex(Triangle.B) ||
			!Plate.Vertices.IsValidIndex(Triangle.C))
		{
			return 0.0;
		}
		const FVector3d Center = NormalizeOrFallback(
			Plate.Vertices[Triangle.A].UnitPosition +
			Plate.Vertices[Triangle.B].UnitPosition +
			Plate.Vertices[Triangle.C].UnitPosition,
			Evidence);
		const double Dot = FMath::Clamp(FVector3d::DotProduct(Center, Evidence), -1.0, 1.0);
		return FMath::Acos(Dot) * SphereRadius;
	};

	auto AddLabel = [&](const FCarrierLabPhaseIIContactRecord& Contact,
		const int32 PlateId,
		const int32 OtherPlateId,
		const int32 LocalTriangleId,
		const ECarrierLabPhaseIITriangleLabel Label,
		const ECarrierLabPhaseIIPolaritySource PolaritySource,
		const TCHAR* Reason)
	{
		if (!State.Plates.IsValidIndex(PlateId))
		{
			++OutMetrics.NaNOrInfCount;
			return;
		}
		const CarrierLab::FCarrierPlate& Plate = State.Plates[PlateId];
		if (!Plate.LocalTriangles.IsValidIndex(LocalTriangleId))
		{
			++OutMetrics.NaNOrInfCount;
			return;
		}
		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];

		FCarrierLabPhaseIITriangleLabelRecord& Record = OutLabels.AddDefaulted_GetRef();
		Record.LabelId = OutLabels.Num() - 1;
		Record.ContactId = Contact.ContactId;
		Record.Step = Contact.Step;
		Record.SampleId = Contact.SampleId;
		Record.PlateId = PlateId;
		Record.OtherPlateId = OtherPlateId;
		Record.LocalTriangleId = LocalTriangleId;
		Record.SourceGlobalTriangleId = Triangle.SourceTriangleId;
		Record.EvidenceUnitPosition = Contact.EvidenceUnitPosition;
		Record.SignedConvergenceVelocity = Contact.SignedConvergenceVelocity;
		Record.VelocityMargin = Contact.VelocityMargin;
		Record.DistanceFromContactKm = DistanceFromContactKm(PlateId, LocalTriangleId, Contact.EvidenceUnitPosition);
		Record.Label = Label;
		Record.PolaritySource = PolaritySource;
		Record.bFromThirdPlateContact = Contact.bThirdPlate;
		Record.LabelReason = Reason;

		int32& ContactLabelCount = LabelsPerContact.FindOrAdd(Contact.ContactId);
		++ContactLabelCount;
		OutMetrics.MaxLabelsPerContact = FMath::Max(OutMetrics.MaxLabelsPerContact, ContactLabelCount);
		if (Contact.bThirdPlate)
		{
			++OutMetrics.LabelsFromThirdPlateContactCount;
		}
		switch (Label)
		{
		case ECarrierLabPhaseIITriangleLabel::Subducting:
			++OutMetrics.SubductingLabelCount;
			break;
		case ECarrierLabPhaseIITriangleLabel::Overriding:
			++OutMetrics.OverridingLabelCount;
			break;
		case ECarrierLabPhaseIITriangleLabel::CollisionCandidate:
			++OutMetrics.CollisionCandidateLabelCount;
			break;
		case ECarrierLabPhaseIITriangleLabel::Ambiguous:
			++OutMetrics.AmbiguousLabelCount;
			break;
		case ECarrierLabPhaseIITriangleLabel::None:
		default:
			break;
		}

		const uint64 UniqueKey =
			(static_cast<uint64>(static_cast<uint32>(PlateId)) << 32) |
			static_cast<uint32>(LocalTriangleId);
		if (!UniqueTriangles.Contains(UniqueKey))
		{
			UniqueTriangles.Add(UniqueKey);
			AreaProxy += TriangleAreaProxy(PlateId, LocalTriangleId);
		}
	};

	for (const FCarrierLabPhaseIIContactRecord& Contact : Contacts)
	{
		if (Contact.bThirdPlate || Contact.ContactClass == ECarrierLabPhaseIIContactClass::ThirdPlate)
		{
			++OutMetrics.ThirdPlateOutOfScopeContactCount;
			continue;
		}
		if (Contact.ContactClass != ECarrierLabPhaseIIContactClass::Convergent ||
			Contact.SignedConvergenceVelocity <= Contact.VelocityMargin)
		{
			++OutMetrics.NonConvergentSkippedContactCount;
			continue;
		}

		++OutMetrics.LabelableContactCount;
		const bool bFixturePolarity = ContactUsesFixturePolarity(Contact);
		const bool bAContinental = IsPlateContinental(Contact.PlateA);
		const bool bBContinental = IsPlateContinental(Contact.PlateB);

		if (bFixturePolarity)
		{
			++OutMetrics.FixtureSpecifiedPolarityContactCount;
			const int32 UnderPlate = Config.FixtureUnderPlate;
			const int32 OverPlate = Config.FixtureOverPlate;
			const int32 UnderTriangle = Contact.PlateA == UnderPlate ? Contact.PlateALocalTriangleId : Contact.PlateBLocalTriangleId;
			const int32 OverTriangle = Contact.PlateA == OverPlate ? Contact.PlateALocalTriangleId : Contact.PlateBLocalTriangleId;
			AddLabel(Contact, UnderPlate, OverPlate, UnderTriangle, ECarrierLabPhaseIITriangleLabel::Subducting, ECarrierLabPhaseIIPolaritySource::FixtureSpecified, TEXT("fixture_specified_under_plate"));
			AddLabel(Contact, OverPlate, UnderPlate, OverTriangle, ECarrierLabPhaseIITriangleLabel::Overriding, ECarrierLabPhaseIIPolaritySource::FixtureSpecified, TEXT("fixture_specified_over_plate"));
		}
		else if (bAContinental != bBContinental)
		{
			++OutMetrics.MixedMaterialPolarityContactCount;
			const int32 UnderPlate = bAContinental ? Contact.PlateB : Contact.PlateA;
			const int32 OverPlate = bAContinental ? Contact.PlateA : Contact.PlateB;
			const int32 UnderTriangle = bAContinental ? Contact.PlateBLocalTriangleId : Contact.PlateALocalTriangleId;
			const int32 OverTriangle = bAContinental ? Contact.PlateALocalTriangleId : Contact.PlateBLocalTriangleId;
			AddLabel(Contact, UnderPlate, OverPlate, UnderTriangle, ECarrierLabPhaseIITriangleLabel::Subducting, ECarrierLabPhaseIIPolaritySource::MixedMaterial, TEXT("oceanic_under_continental"));
			AddLabel(Contact, OverPlate, UnderPlate, OverTriangle, ECarrierLabPhaseIITriangleLabel::Overriding, ECarrierLabPhaseIIPolaritySource::MixedMaterial, TEXT("continental_over_oceanic"));
		}
		else
		{
			++OutMetrics.SameMaterialAmbiguousContactCount;
			AddLabel(Contact, Contact.PlateA, Contact.PlateB, Contact.PlateALocalTriangleId, ECarrierLabPhaseIITriangleLabel::Ambiguous, ECarrierLabPhaseIIPolaritySource::SameMaterialAmbiguous, TEXT("same_material_ambiguous"));
			AddLabel(Contact, Contact.PlateB, Contact.PlateA, Contact.PlateBLocalTriangleId, ECarrierLabPhaseIITriangleLabel::Ambiguous, ECarrierLabPhaseIIPolaritySource::SameMaterialAmbiguous, TEXT("same_material_ambiguous"));
		}
	}

	OutMetrics.LabelRecordCount = OutLabels.Num();
	OutMetrics.UniqueLabeledTriangleCount = UniqueTriangles.Num();
	OutMetrics.UniqueLabeledTriangleAreaProxy = AreaProxy;

	uint64 LabelHash = 1469598103934665603ull;
	HashMix(LabelHash, static_cast<uint64>(OutMetrics.Step + 1));
	HashMix(LabelHash, static_cast<uint64>(OutMetrics.ContactRecordCount + 1));
	for (const FCarrierLabPhaseIITriangleLabelRecord& Record : OutLabels)
	{
		HashMix(LabelHash, static_cast<uint64>(Record.LabelId + 1));
		HashMix(LabelHash, static_cast<uint64>(Record.ContactId + 1));
		HashMix(LabelHash, static_cast<uint64>(Record.SampleId + 1));
		HashMix(LabelHash, static_cast<uint64>(Record.PlateId + 1));
		HashMix(LabelHash, static_cast<uint64>(Record.OtherPlateId + 1));
		HashMix(LabelHash, static_cast<uint64>(Record.LocalTriangleId + 1));
		HashMix(LabelHash, static_cast<uint64>(Record.SourceGlobalTriangleId + 1));
		HashMixDouble(LabelHash, Record.SignedConvergenceVelocity);
		HashMixDouble(LabelHash, Record.DistanceFromContactKm);
		HashMix(LabelHash, static_cast<uint64>(Record.Label) + 1);
		HashMix(LabelHash, static_cast<uint64>(Record.PolaritySource) + 1);
		HashMix(LabelHash, Record.bFromThirdPlateContact ? 1ull : 0ull);
	}
	OutMetrics.TriangleLabelHash = HashToString(LabelHash);
	OutMetrics.TriangleLabelSeconds = FPlatformTime::Seconds() - StartSeconds;
	return true;
}

bool ACarrierLabVisualizationActor::ApplyPhaseIIResamplingFilterEvent(
	const TArray<FCarrierLabPhaseIITriangleLabelRecord>& Labels,
	TArray<FCarrierLabPhaseIIFilterDecisionRecord>& OutDecisions,
	FCarrierLabPhaseIIResamplingFilterMetrics& OutMetrics,
	TArray<FCarrierLabPhaseIIMaterialRecord>* OutMaterialRecords,
	FCarrierLabPhaseIIMaterialLedgerMetrics* OutMaterialMetrics)
{
	OutDecisions.Reset();
	if (OutMaterialRecords != nullptr)
	{
		OutMaterialRecords->Reset();
	}
	if (OutMaterialMetrics != nullptr)
	{
		*OutMaterialMetrics = FCarrierLabPhaseIIMaterialLedgerMetrics();
	}
	OutMetrics = FCarrierLabPhaseIIResamplingFilterMetrics();
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}

	const double EventStartSeconds = FPlatformTime::Seconds();
	ProjectCurrentCarrier();
	OutMetrics.EventId = CurrentMetrics.EventCount + 1;
	OutMetrics.Step = CurrentMetrics.Step;
	OutMetrics.SampleCount = State.Samples.Num();
	OutMetrics.AuthoritativeCAFBefore = CurrentMetrics.AuthoritativeCAF;
	OutMetrics.ProjectedCAFBefore = CurrentMetrics.ProjectedCAF;
	OutMetrics.ProjectionHashBefore = CurrentMetrics.LastHash;
	OutMetrics.StateHashBefore = CurrentMetrics.StateHash;

	auto TriangleKey = [](const int32 PlateId, const int32 LocalTriangleId) -> uint64
	{
		return (static_cast<uint64>(static_cast<uint32>(PlateId)) << 32) | static_cast<uint32>(LocalTriangleId);
	};

	TMap<uint64, const FCarrierLabPhaseIITriangleLabelRecord*> SubductingLabelsByTriangle;
	for (const FCarrierLabPhaseIITriangleLabelRecord& Label : Labels)
	{
		if (Label.bFromThirdPlateContact)
		{
			++OutMetrics.ThirdPlateLabelInputCount;
			continue;
		}
		if (Label.Label == ECarrierLabPhaseIITriangleLabel::Ambiguous ||
			Label.Label == ECarrierLabPhaseIITriangleLabel::CollisionCandidate)
		{
			++OutMetrics.AmbiguousLabelInputCount;
			continue;
		}
		if (Label.Label != ECarrierLabPhaseIITriangleLabel::Subducting ||
			Label.PlateId == INDEX_NONE ||
			Label.LocalTriangleId == INDEX_NONE)
		{
			continue;
		}

		++OutMetrics.SubductingLabelInputCount;
		const uint64 Key = TriangleKey(Label.PlateId, Label.LocalTriangleId);
		const FCarrierLabPhaseIITriangleLabelRecord** Existing = SubductingLabelsByTriangle.Find(Key);
		if (Existing == nullptr || (*Existing != nullptr && Label.LabelId < (*Existing)->LabelId))
		{
			SubductingLabelsByTriangle.Add(Key, &Label);
		}
	}

	FString MeshError;
	if (!RefreshProjectionRayMesh(MeshError))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab Phase II filtered resample failed: %s"), *MeshError);
		return false;
	}

	const double FilterStartSeconds = FPlatformTime::Seconds();
	const FCarrierLabVizBoundaryIndex BoundaryIndex = BuildBoundaryIndex(State);
	TArray<double> AreaBefore;
	TArray<double> AreaAfter;
	AreaBefore.Init(0.0, State.Plates.Num());
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (AreaBefore.IsValidIndex(Sample.PlateId))
		{
			AreaBefore[Sample.PlateId] += Sample.AreaWeight;
		}
	}

	TArray<int32> NewPlateIds;
	TArray<double> NewFractions;
	NewPlateIds.SetNum(State.Samples.Num());
	NewFractions.SetNum(State.Samples.Num());
	TArray<ECarrierLabPhaseIIMaterialEventClass> MaterialClassBySample;
	TArray<ECarrierLabPhaseIIFilterDecisionClass> MaterialDecisionClassBySample;
	TArray<int32> MaterialRawPlateCountBySample;
	TArray<int32> MaterialPostPlateCountBySample;
	TArray<int32> MaterialSourceContactIdBySample;
	TArray<int32> MaterialSourceLabelIdBySample;
	TArray<int32> MaterialHitPlateIdBySample;
	TArray<int32> MaterialHitLocalTriangleIdBySample;
	TArray<int32> MaterialHitContinentalVertexCountBySample;
	TArray<ECarrierLabPhaseIISourceTriangleUniformity> MaterialHitUniformityBySample;
	TArray<bool> MaterialThirdPlateBySample;
	TArray<bool> MaterialNonSeparatingGapBySample;
	MaterialClassBySample.Init(ECarrierLabPhaseIIMaterialEventClass::Preserved, State.Samples.Num());
	MaterialDecisionClassBySample.Init(ECarrierLabPhaseIIFilterDecisionClass::ResolvedSingle, State.Samples.Num());
	MaterialRawPlateCountBySample.Init(0, State.Samples.Num());
	MaterialPostPlateCountBySample.Init(0, State.Samples.Num());
	MaterialSourceContactIdBySample.Init(INDEX_NONE, State.Samples.Num());
	MaterialSourceLabelIdBySample.Init(INDEX_NONE, State.Samples.Num());
	MaterialHitPlateIdBySample.Init(INDEX_NONE, State.Samples.Num());
	MaterialHitLocalTriangleIdBySample.Init(INDEX_NONE, State.Samples.Num());
	MaterialHitContinentalVertexCountBySample.Init(INDEX_NONE, State.Samples.Num());
	MaterialHitUniformityBySample.Init(ECarrierLabPhaseIISourceTriangleUniformity::Unknown, State.Samples.Num());
	MaterialThirdPlateBySample.Init(false, State.Samples.Num());
	MaterialNonSeparatingGapBySample.Init(false, State.Samples.Num());
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		NewPlateIds[Sample.Id] = Sample.PlateId;
		NewFractions[Sample.Id] = Sample.ContinentalFraction;
	}

	TSet<int32> SamplesWithFilteredCandidates;
	TArray<FCarrierLabVizCandidate> Candidates;
	TArray<FCarrierLabVizCandidate> RemainingCandidates;
	auto SetHitTriangleSource = [this, &MaterialHitPlateIdBySample, &MaterialHitLocalTriangleIdBySample, &MaterialHitContinentalVertexCountBySample, &MaterialHitUniformityBySample](
		const int32 SampleId,
		const int32 PlateId,
		const int32 LocalTriangleId)
	{
		if (!MaterialHitPlateIdBySample.IsValidIndex(SampleId))
		{
			return;
		}

		int32 ContinentalVertexCount = INDEX_NONE;
		const ECarrierLabPhaseIISourceTriangleUniformity Uniformity = ClassifyHitTriangleUniformity(State, PlateId, LocalTriangleId, ContinentalVertexCount);
		MaterialHitPlateIdBySample[SampleId] = PlateId;
		MaterialHitLocalTriangleIdBySample[SampleId] = LocalTriangleId;
		MaterialHitContinentalVertexCountBySample[SampleId] = ContinentalVertexCount;
		MaterialHitUniformityBySample[SampleId] = Uniformity;
	};
	auto SetMaterialClass = [&MaterialClassBySample, &MaterialDecisionClassBySample, &MaterialRawPlateCountBySample, &MaterialPostPlateCountBySample, &MaterialSourceContactIdBySample, &MaterialSourceLabelIdBySample, &MaterialThirdPlateBySample, &MaterialNonSeparatingGapBySample](
		const int32 SampleId,
		const ECarrierLabPhaseIIMaterialEventClass EventClass,
		const ECarrierLabPhaseIIFilterDecisionClass DecisionClass,
		const int32 RawPlateCount,
		const int32 PostPlateCount,
		const bool bThirdPlate,
		const bool bNonSeparatingGap,
		const int32 SourceContactId,
		const int32 SourceLabelId)
	{
		if (!MaterialClassBySample.IsValidIndex(SampleId))
		{
			return;
		}
		MaterialClassBySample[SampleId] = EventClass;
		MaterialDecisionClassBySample[SampleId] = DecisionClass;
		MaterialRawPlateCountBySample[SampleId] = RawPlateCount;
		MaterialPostPlateCountBySample[SampleId] = PostPlateCount;
		MaterialThirdPlateBySample[SampleId] = bThirdPlate;
		MaterialNonSeparatingGapBySample[SampleId] = bNonSeparatingGap;
		if (SourceContactId != INDEX_NONE)
		{
			MaterialSourceContactIdBySample[SampleId] = SourceContactId;
		}
		if (SourceLabelId != INDEX_NONE)
		{
			MaterialSourceLabelIdBySample[SampleId] = SourceLabelId;
		}
	};
	auto ClassifyUnresolvedMaterial = [this](const int32 RawPlateCount, const TArray<FCarrierLabVizCandidate>& Remaining)
	{
		if (RawPlateCount >= 3)
		{
			return ECarrierLabPhaseIIMaterialEventClass::UnresolvedTripleJunctionMultiHit;
		}

		bool bHasContinental = false;
		bool bHasOceanic = false;
		for (const FCarrierLabVizCandidate& Candidate : Remaining)
		{
			if (!State.Plates.IsValidIndex(Candidate.PlateId))
			{
				continue;
			}
			const double Fraction = InterpolateContinentalFraction(State.Plates[Candidate.PlateId], Candidate);
			if (Fraction >= 0.5)
			{
				bHasContinental = true;
			}
			else
			{
				bHasOceanic = true;
			}
		}
		return (bHasContinental && bHasOceanic)
			? ECarrierLabPhaseIIMaterialEventClass::UnresolvedMixedMaterialMultiHit
			: ECarrierLabPhaseIIMaterialEventClass::UnresolvedSameMaterialMultiHit;
	};
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		uint64 RawPlateMask = 0;
		bool bAnyBoundary = false;
		QuerySampleCandidates(State, ProjectionRayMesh, Sample, Candidates, RawPlateMask, bAnyBoundary);
		const int32 RawPlateCount = CountBits64(RawPlateMask);
		if (RawPlateCount == 0)
		{
			++OutMetrics.RawMissSampleCount;
			FCarrierLabVizBoundaryPoint Q1;
			FCarrierLabVizBoundaryPoint Q2;
			if (FindNearestBoundaryPair(BoundaryIndex, Sample.UnitPosition, Q1, Q2))
			{
				const FVector3d RidgePoint = NormalizeOrFallback(Q1.UnitPosition + Q2.UnitPosition, Sample.UnitPosition);
				const double SignedVelocity = SignedPairSeparationVelocityForPlatePair(RidgePoint, Motions, Q1.PlateId, Q2.PlateId);
				++OutMetrics.GapFillCount;
				const bool bNonSeparatingGap = SignedVelocity <= 0.0;
				if (SignedVelocity <= 0.0)
				{
					++OutMetrics.NonSeparatingGapFillCount;
				}
				NewPlateIds[Sample.Id] = Q1.PlateId;
				NewFractions[Sample.Id] = FMath::Clamp(0.5 * (Q1.ContinentalFraction + Q2.ContinentalFraction), 0.0, 1.0);

				FCarrierLabPhaseIIFilterDecisionRecord& Decision = OutDecisions.AddDefaulted_GetRef();
				Decision.DecisionId = OutDecisions.Num() - 1;
				Decision.EventId = OutMetrics.EventId;
				Decision.Step = OutMetrics.Step;
				Decision.SampleId = Sample.Id;
				Decision.ResolvedPlateId = Q1.PlateId;
				Decision.DecisionClass = ECarrierLabPhaseIIFilterDecisionClass::GapFill;
				SetMaterialClass(
					Sample.Id,
					ECarrierLabPhaseIIMaterialEventClass::OverwrittenByGapFill,
					ECarrierLabPhaseIIFilterDecisionClass::GapFill,
					RawPlateCount,
					0,
					false,
					bNonSeparatingGap,
					INDEX_NONE,
					INDEX_NONE);
			}
			continue;
		}

		if (RawPlateCount > 1)
		{
			++OutMetrics.RawMultiHitSampleCount;
			if (RawPlateCount >= 3)
			{
				++OutMetrics.RawThirdPlateSampleCount;
			}
		}

		uint64 PostPlateMask = 0;
		int32 FilteredCandidateCount = 0;
		RemainingCandidates.Reset(Candidates.Num());
		for (const FCarrierLabVizCandidate& Candidate : Candidates)
		{
			const FCarrierLabPhaseIITriangleLabelRecord* const* LabelPtr = SubductingLabelsByTriangle.Find(TriangleKey(Candidate.PlateId, Candidate.LocalTriangleId));
			if (LabelPtr != nullptr && *LabelPtr != nullptr)
			{
				++FilteredCandidateCount;
				++OutMetrics.FilteredCandidateCount;
				SamplesWithFilteredCandidates.Add(Sample.Id);
				const FCarrierLabPhaseIITriangleLabelRecord& Label = **LabelPtr;
				if (MaterialSourceContactIdBySample.IsValidIndex(Sample.Id) &&
					MaterialSourceContactIdBySample[Sample.Id] == INDEX_NONE)
				{
					MaterialSourceContactIdBySample[Sample.Id] = Label.ContactId;
					MaterialSourceLabelIdBySample[Sample.Id] = Label.LabelId;
				}
				if (Label.bFromThirdPlateContact)
				{
					++OutMetrics.DecisionsFromThirdPlateLabelCount;
				}

				FCarrierLabPhaseIIFilterDecisionRecord& Decision = OutDecisions.AddDefaulted_GetRef();
				Decision.DecisionId = OutDecisions.Num() - 1;
				Decision.EventId = OutMetrics.EventId;
				Decision.Step = OutMetrics.Step;
				Decision.SampleId = Sample.Id;
				Decision.RawCandidateCount = Candidates.Num();
				Decision.RawPlateCount = RawPlateCount;
				Decision.FilteredCandidateCount = FilteredCandidateCount;
				Decision.FilteredPlateId = Candidate.PlateId;
				Decision.FilteredLocalTriangleId = Candidate.LocalTriangleId;
				Decision.SourceContactId = Label.ContactId;
				Decision.SourceLabelId = Label.LabelId;
				Decision.bBoundaryEvidence = bAnyBoundary || Candidate.bBoundary;
				Decision.bThirdPlateEvidence = RawPlateCount >= 3;
				Decision.DecisionClass = ECarrierLabPhaseIIFilterDecisionClass::CandidateFiltered;
				continue;
			}

			RemainingCandidates.Add(Candidate);
			PostPlateMask |= (1ull << static_cast<uint64>(Candidate.PlateId));
		}

		const int32 PostPlateCount = CountBits64(PostPlateMask);
		if (PostPlateCount == 0)
		{
			++OutMetrics.FilterExhaustedSampleCount;
			FCarrierLabPhaseIIFilterDecisionRecord& Decision = OutDecisions.AddDefaulted_GetRef();
			Decision.DecisionId = OutDecisions.Num() - 1;
			Decision.EventId = OutMetrics.EventId;
			Decision.Step = OutMetrics.Step;
			Decision.SampleId = Sample.Id;
			Decision.RawCandidateCount = Candidates.Num();
			Decision.RawPlateCount = RawPlateCount;
			Decision.FilteredCandidateCount = FilteredCandidateCount;
			Decision.PostFilterCandidateCount = RemainingCandidates.Num();
			Decision.PostFilterPlateCount = PostPlateCount;
			Decision.ResolvedPlateId = Sample.PlateId;
			Decision.bBoundaryEvidence = bAnyBoundary;
			Decision.bThirdPlateEvidence = RawPlateCount >= 3;
			Decision.DecisionClass = ECarrierLabPhaseIIFilterDecisionClass::FilterExhausted;
			SetMaterialClass(
				Sample.Id,
				ECarrierLabPhaseIIMaterialEventClass::FilterExhaustedUnknown,
				ECarrierLabPhaseIIFilterDecisionClass::FilterExhausted,
				RawPlateCount,
				PostPlateCount,
				RawPlateCount >= 3,
				false,
				MaterialSourceContactIdBySample.IsValidIndex(Sample.Id) ? MaterialSourceContactIdBySample[Sample.Id] : INDEX_NONE,
				MaterialSourceLabelIdBySample.IsValidIndex(Sample.Id) ? MaterialSourceLabelIdBySample[Sample.Id] : INDEX_NONE);
			continue;
		}

		if (PostPlateCount > 1)
		{
			++OutMetrics.PostFilterMultiHitSampleCount;
			++OutMetrics.UnresolvedMultiHitSampleCount;
			if (!bAnyBoundary)
			{
				++OutMetrics.PostFilterNonBoundaryMultiHitSampleCount;
			}

			FCarrierLabPhaseIIFilterDecisionRecord& Decision = OutDecisions.AddDefaulted_GetRef();
			Decision.DecisionId = OutDecisions.Num() - 1;
			Decision.EventId = OutMetrics.EventId;
			Decision.Step = OutMetrics.Step;
			Decision.SampleId = Sample.Id;
			Decision.RawCandidateCount = Candidates.Num();
			Decision.RawPlateCount = RawPlateCount;
			Decision.FilteredCandidateCount = FilteredCandidateCount;
			Decision.PostFilterCandidateCount = RemainingCandidates.Num();
			Decision.PostFilterPlateCount = PostPlateCount;
			Decision.ResolvedPlateId = Sample.PlateId;
			Decision.bBoundaryEvidence = bAnyBoundary;
			Decision.bThirdPlateEvidence = RawPlateCount >= 3;
			Decision.DecisionClass = ECarrierLabPhaseIIFilterDecisionClass::UnresolvedMultiHit;
			SetMaterialClass(
				Sample.Id,
				ClassifyUnresolvedMaterial(RawPlateCount, RemainingCandidates),
				ECarrierLabPhaseIIFilterDecisionClass::UnresolvedMultiHit,
				RawPlateCount,
				PostPlateCount,
				RawPlateCount >= 3,
				false,
				MaterialSourceContactIdBySample.IsValidIndex(Sample.Id) ? MaterialSourceContactIdBySample[Sample.Id] : INDEX_NONE,
				MaterialSourceLabelIdBySample.IsValidIndex(Sample.Id) ? MaterialSourceLabelIdBySample[Sample.Id] : INDEX_NONE);
			continue;
		}

		++OutMetrics.PostFilterSingleHitSampleCount;
		const int32 ResolvedPlateId = LowestSetBitIndex(PostPlateMask);
		const FCarrierLabVizCandidate* Chosen = RemainingCandidates.FindByPredicate([ResolvedPlateId](const FCarrierLabVizCandidate& Candidate)
		{
			return Candidate.PlateId == ResolvedPlateId;
		});
		NewPlateIds[Sample.Id] = ResolvedPlateId;
		NewFractions[Sample.Id] = (Chosen != nullptr && State.Plates.IsValidIndex(ResolvedPlateId))
			? InterpolateContinentalFraction(State.Plates[ResolvedPlateId], *Chosen)
			: Sample.ContinentalFraction;
		if (Chosen != nullptr)
		{
			SetHitTriangleSource(Sample.Id, ResolvedPlateId, Chosen->LocalTriangleId);
		}
		if (MaterialRawPlateCountBySample.IsValidIndex(Sample.Id))
		{
			MaterialRawPlateCountBySample[Sample.Id] = RawPlateCount;
			MaterialPostPlateCountBySample[Sample.Id] = PostPlateCount;
		}

		if (FilteredCandidateCount > 0)
		{
			FCarrierLabPhaseIIFilterDecisionRecord& Decision = OutDecisions.AddDefaulted_GetRef();
			Decision.DecisionId = OutDecisions.Num() - 1;
			Decision.EventId = OutMetrics.EventId;
			Decision.Step = OutMetrics.Step;
			Decision.SampleId = Sample.Id;
			Decision.RawCandidateCount = Candidates.Num();
			Decision.RawPlateCount = RawPlateCount;
			Decision.FilteredCandidateCount = FilteredCandidateCount;
			Decision.PostFilterCandidateCount = RemainingCandidates.Num();
			Decision.PostFilterPlateCount = PostPlateCount;
			Decision.ResolvedPlateId = ResolvedPlateId;
			Decision.bBoundaryEvidence = bAnyBoundary;
			Decision.bThirdPlateEvidence = RawPlateCount >= 3;
			Decision.DecisionClass = ECarrierLabPhaseIIFilterDecisionClass::ResolvedSingle;
			SetMaterialClass(
				Sample.Id,
				ECarrierLabPhaseIIMaterialEventClass::ConsumedBySubduction,
				ECarrierLabPhaseIIFilterDecisionClass::ResolvedSingle,
				RawPlateCount,
				PostPlateCount,
				RawPlateCount >= 3,
				false,
				MaterialSourceContactIdBySample.IsValidIndex(Sample.Id) ? MaterialSourceContactIdBySample[Sample.Id] : INDEX_NONE,
				MaterialSourceLabelIdBySample.IsValidIndex(Sample.Id) ? MaterialSourceLabelIdBySample[Sample.Id] : INDEX_NONE);
		}
	}
	OutMetrics.FilteredSampleCount = SamplesWithFilteredCandidates.Num();
	OutMetrics.FilterSeconds = FPlatformTime::Seconds() - FilterStartSeconds;

	if (OutMaterialRecords != nullptr || OutMaterialMetrics != nullptr)
	{
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerMetrics;
		LedgerMetrics.EventId = OutMetrics.EventId;
		LedgerMetrics.Step = OutMetrics.Step;
		LedgerMetrics.SampleCount = State.Samples.Num();
		TArray<double> PlateBefore;
		TArray<double> PlateAfter;
		TArray<double> PlateRecordDelta;
		PlateBefore.Init(0.0, State.Plates.Num());
		PlateAfter.Init(0.0, State.Plates.Num());
		PlateRecordDelta.Init(0.0, State.Plates.Num());

		auto AddLossGain = [](const double Delta, double& Loss, double& Gain)
		{
			if (Delta < 0.0)
			{
				Loss += -Delta;
			}
			else
			{
				Gain += Delta;
			}
		};

		uint64 LedgerHash = 1469598103934665603ull;
		HashMix(LedgerHash, static_cast<uint64>(OutMetrics.EventId + 1));
		HashMix(LedgerHash, static_cast<uint64>(OutMetrics.Step + 1));
		HashMix(LedgerHash, static_cast<uint64>(State.Samples.Num() + 1));
		for (const CarrierLab::FSphereSample& Sample : State.Samples)
		{
			const int32 SampleId = Sample.Id;
			const int32 TargetPlateId = NewPlateIds.IsValidIndex(SampleId) ? NewPlateIds[SampleId] : Sample.PlateId;
			const double AfterFraction = FMath::Clamp(NewFractions.IsValidIndex(SampleId) ? NewFractions[SampleId] : Sample.ContinentalFraction, 0.0, 1.0);
			const double ContinentalBefore = Sample.AreaWeight * Sample.ContinentalFraction;
			const double ContinentalAfter = Sample.AreaWeight * AfterFraction;
			const double OceanicBefore = Sample.AreaWeight - ContinentalBefore;
			const double OceanicAfter = Sample.AreaWeight - ContinentalAfter;
			const double ContinentalDelta = ContinentalAfter - ContinentalBefore;
			const double OceanicDelta = OceanicAfter - OceanicBefore;
			ECarrierLabPhaseIIMaterialEventClass EventClass = MaterialClassBySample.IsValidIndex(SampleId)
				? MaterialClassBySample[SampleId]
				: ECarrierLabPhaseIIMaterialEventClass::Preserved;
			const bool bMaterialChanged = FMath::Abs(ContinentalDelta) > 1.0e-15 || FMath::Abs(OceanicDelta) > 1.0e-15;
			const bool bPlateChanged = TargetPlateId != Sample.PlateId;
			if (EventClass == ECarrierLabPhaseIIMaterialEventClass::Preserved && (bMaterialChanged || bPlateChanged))
			{
				EventClass = ECarrierLabPhaseIIMaterialEventClass::SingleHitTransfer;
			}

			LedgerMetrics.TotalArea += Sample.AreaWeight;
			LedgerMetrics.ContinentalMassBefore += ContinentalBefore;
			LedgerMetrics.ContinentalMassAfter += ContinentalAfter;
			LedgerMetrics.OceanicMassBefore += OceanicBefore;
			LedgerMetrics.OceanicMassAfter += OceanicAfter;
			if (PlateBefore.IsValidIndex(Sample.PlateId))
			{
				PlateBefore[Sample.PlateId] += ContinentalBefore;
			}
			if (PlateAfter.IsValidIndex(TargetPlateId))
			{
				PlateAfter[TargetPlateId] += ContinentalAfter;
			}

			const bool bNeedsRecord =
				EventClass != ECarrierLabPhaseIIMaterialEventClass::Preserved ||
				bMaterialChanged ||
				bPlateChanged;
			if (!bNeedsRecord)
			{
				++LedgerMetrics.PreservedRecordCount;
				continue;
			}

			FCarrierLabPhaseIIMaterialRecord Record;
			Record.RecordId = LedgerMetrics.RecordCount;
			Record.EventId = OutMetrics.EventId;
			Record.Step = OutMetrics.Step;
			Record.SampleId = SampleId;
			Record.SourcePlateId = Sample.PlateId;
			Record.TargetPlateId = TargetPlateId;
			Record.SourceContactId = MaterialSourceContactIdBySample.IsValidIndex(SampleId) ? MaterialSourceContactIdBySample[SampleId] : INDEX_NONE;
			Record.SourceLabelId = MaterialSourceLabelIdBySample.IsValidIndex(SampleId) ? MaterialSourceLabelIdBySample[SampleId] : INDEX_NONE;
			Record.HitPlateId = MaterialHitPlateIdBySample.IsValidIndex(SampleId) ? MaterialHitPlateIdBySample[SampleId] : INDEX_NONE;
			Record.HitLocalTriangleId = MaterialHitLocalTriangleIdBySample.IsValidIndex(SampleId) ? MaterialHitLocalTriangleIdBySample[SampleId] : INDEX_NONE;
			Record.HitTriangleContinentalVertexCount = MaterialHitContinentalVertexCountBySample.IsValidIndex(SampleId) ? MaterialHitContinentalVertexCountBySample[SampleId] : INDEX_NONE;
			Record.RawPlateCount = MaterialRawPlateCountBySample.IsValidIndex(SampleId) ? MaterialRawPlateCountBySample[SampleId] : 0;
			Record.PostFilterPlateCount = MaterialPostPlateCountBySample.IsValidIndex(SampleId) ? MaterialPostPlateCountBySample[SampleId] : 0;
			Record.AreaWeight = Sample.AreaWeight;
			Record.ContinentalBefore = ContinentalBefore;
			Record.ContinentalAfter = ContinentalAfter;
			Record.ContinentalDelta = ContinentalDelta;
			Record.OceanicBefore = OceanicBefore;
			Record.OceanicAfter = OceanicAfter;
			Record.OceanicDelta = OceanicDelta;
			Record.bMaterialChanged = bMaterialChanged;
			Record.bPlateChanged = bPlateChanged;
			Record.bThirdPlateEvidence = MaterialThirdPlateBySample.IsValidIndex(SampleId) && MaterialThirdPlateBySample[SampleId];
			Record.bNonSeparatingGap = MaterialNonSeparatingGapBySample.IsValidIndex(SampleId) && MaterialNonSeparatingGapBySample[SampleId];
			Record.EventClass = EventClass;
			Record.DecisionClass = MaterialDecisionClassBySample.IsValidIndex(SampleId)
				? MaterialDecisionClassBySample[SampleId]
				: ECarrierLabPhaseIIFilterDecisionClass::ResolvedSingle;
			Record.HitTriangleUniformity = MaterialHitUniformityBySample.IsValidIndex(SampleId)
				? MaterialHitUniformityBySample[SampleId]
				: ECarrierLabPhaseIISourceTriangleUniformity::Unknown;

			++LedgerMetrics.RecordCount;
			if (bMaterialChanged)
			{
				++LedgerMetrics.ChangedRecordCount;
			}
			if (bPlateChanged)
			{
				++LedgerMetrics.PlateChangedRecordCount;
			}
			LedgerMetrics.LedgerContinentalDelta += ContinentalDelta;
			LedgerMetrics.LedgerOceanicDelta += OceanicDelta;
			if (PlateRecordDelta.IsValidIndex(Sample.PlateId))
			{
				PlateRecordDelta[Sample.PlateId] -= ContinentalBefore;
			}
			if (PlateRecordDelta.IsValidIndex(TargetPlateId))
			{
				PlateRecordDelta[TargetPlateId] += ContinentalAfter;
			}

			switch (EventClass)
			{
			case ECarrierLabPhaseIIMaterialEventClass::SingleHitTransfer:
				++LedgerMetrics.SingleHitTransferRecordCount;
				AddLossGain(ContinentalDelta, LedgerMetrics.SingleHitTransferContinentalLoss, LedgerMetrics.SingleHitTransferContinentalGain);
				switch (Record.HitTriangleUniformity)
				{
				case ECarrierLabPhaseIISourceTriangleUniformity::UniformContinental:
					++LedgerMetrics.SingleHitUniformContinentalRecordCount;
					AddLossGain(ContinentalDelta, LedgerMetrics.SingleHitUniformContinentalLoss, LedgerMetrics.SingleHitUniformContinentalGain);
					break;
				case ECarrierLabPhaseIISourceTriangleUniformity::UniformOceanic:
					++LedgerMetrics.SingleHitUniformOceanicRecordCount;
					AddLossGain(ContinentalDelta, LedgerMetrics.SingleHitUniformOceanicLoss, LedgerMetrics.SingleHitUniformOceanicGain);
					break;
				case ECarrierLabPhaseIISourceTriangleUniformity::Mixed:
					++LedgerMetrics.SingleHitMixedTriangleRecordCount;
					AddLossGain(ContinentalDelta, LedgerMetrics.SingleHitMixedTriangleLoss, LedgerMetrics.SingleHitMixedTriangleGain);
					break;
				case ECarrierLabPhaseIISourceTriangleUniformity::Unknown:
				default:
					++LedgerMetrics.SingleHitUnknownTriangleRecordCount;
					AddLossGain(ContinentalDelta, LedgerMetrics.SingleHitUnknownTriangleLoss, LedgerMetrics.SingleHitUnknownTriangleGain);
					break;
				}
				break;
			case ECarrierLabPhaseIIMaterialEventClass::ConsumedBySubduction:
				++LedgerMetrics.SubductionRecordCount;
				AddLossGain(ContinentalDelta, LedgerMetrics.SubductionContinentalLoss, LedgerMetrics.SubductionContinentalGain);
				break;
			case ECarrierLabPhaseIIMaterialEventClass::OverwrittenByGapFill:
				++LedgerMetrics.GapFillRecordCount;
				if (Record.bNonSeparatingGap)
				{
					++LedgerMetrics.NonSeparatingGapFillRecordCount;
				}
				AddLossGain(ContinentalDelta, LedgerMetrics.GapFillContinentalLoss, LedgerMetrics.GapFillContinentalGain);
				break;
			case ECarrierLabPhaseIIMaterialEventClass::UnresolvedSameMaterialMultiHit:
				++LedgerMetrics.UnresolvedSameMaterialRecordCount;
				AddLossGain(ContinentalDelta, LedgerMetrics.UnresolvedSameMaterialContinentalLoss, LedgerMetrics.UnresolvedSameMaterialContinentalGain);
				LedgerMetrics.UnresolvedSameMaterialContinentalDelta += ContinentalDelta;
				break;
			case ECarrierLabPhaseIIMaterialEventClass::UnresolvedTripleJunctionMultiHit:
				++LedgerMetrics.UnresolvedTripleJunctionRecordCount;
				AddLossGain(ContinentalDelta, LedgerMetrics.UnresolvedTripleJunctionContinentalLoss, LedgerMetrics.UnresolvedTripleJunctionContinentalGain);
				LedgerMetrics.UnresolvedTripleJunctionContinentalDelta += ContinentalDelta;
				break;
			case ECarrierLabPhaseIIMaterialEventClass::UnresolvedMixedMaterialMultiHit:
				++LedgerMetrics.UnresolvedMixedMaterialRecordCount;
				AddLossGain(ContinentalDelta, LedgerMetrics.UnresolvedMixedMaterialContinentalLoss, LedgerMetrics.UnresolvedMixedMaterialContinentalGain);
				LedgerMetrics.UnresolvedMixedMaterialContinentalDelta += ContinentalDelta;
				break;
			case ECarrierLabPhaseIIMaterialEventClass::FilterExhaustedUnknown:
				++LedgerMetrics.FilterExhaustedRecordCount;
				AddLossGain(ContinentalDelta, LedgerMetrics.FilterExhaustedContinentalLoss, LedgerMetrics.FilterExhaustedContinentalGain);
				LedgerMetrics.FilterExhaustedContinentalDelta += ContinentalDelta;
				break;
			case ECarrierLabPhaseIIMaterialEventClass::Preserved:
				++LedgerMetrics.PreservedRecordCount;
				break;
			case ECarrierLabPhaseIIMaterialEventClass::NumericResidual:
			default:
				break;
			}

			HashMix(LedgerHash, static_cast<uint64>(Record.RecordId + 1));
			HashMix(LedgerHash, static_cast<uint64>(Record.SampleId + 1));
			HashMix(LedgerHash, static_cast<uint64>(Record.SourcePlateId + 1));
			HashMix(LedgerHash, static_cast<uint64>(Record.TargetPlateId + 1));
			HashMix(LedgerHash, static_cast<uint64>(Record.SourceContactId + 1));
			HashMix(LedgerHash, static_cast<uint64>(Record.SourceLabelId + 1));
			HashMix(LedgerHash, static_cast<uint64>(Record.HitPlateId + 1));
			HashMix(LedgerHash, static_cast<uint64>(Record.HitLocalTriangleId + 1));
			HashMix(LedgerHash, static_cast<uint64>(Record.HitTriangleContinentalVertexCount + 1));
			HashMix(LedgerHash, static_cast<uint64>(Record.RawPlateCount + 1));
			HashMix(LedgerHash, static_cast<uint64>(Record.PostFilterPlateCount + 1));
			HashMixDouble(LedgerHash, Record.AreaWeight);
			HashMixDouble(LedgerHash, Record.ContinentalBefore);
			HashMixDouble(LedgerHash, Record.ContinentalAfter);
			HashMixDouble(LedgerHash, Record.OceanicBefore);
			HashMixDouble(LedgerHash, Record.OceanicAfter);
			HashMix(LedgerHash, Record.bMaterialChanged ? 1ull : 0ull);
			HashMix(LedgerHash, Record.bPlateChanged ? 1ull : 0ull);
			HashMix(LedgerHash, Record.bThirdPlateEvidence ? 1ull : 0ull);
			HashMix(LedgerHash, Record.bNonSeparatingGap ? 1ull : 0ull);
			HashMix(LedgerHash, static_cast<uint64>(Record.EventClass) + 1);
			HashMix(LedgerHash, static_cast<uint64>(Record.DecisionClass) + 1);
			HashMix(LedgerHash, static_cast<uint64>(Record.HitTriangleUniformity) + 1);

			if (OutMaterialRecords != nullptr)
			{
				OutMaterialRecords->Add(Record);
			}
		}

		const double ActiveContinentalDelta = LedgerMetrics.ContinentalMassAfter - LedgerMetrics.ContinentalMassBefore;
		const double ActiveOceanicDelta = LedgerMetrics.OceanicMassAfter - LedgerMetrics.OceanicMassBefore;
		LedgerMetrics.ContinentalDeltaResidual = ActiveContinentalDelta - LedgerMetrics.LedgerContinentalDelta;
		LedgerMetrics.OceanicDeltaResidual = ActiveOceanicDelta - LedgerMetrics.LedgerOceanicDelta;
		for (int32 PlateId = 0; PlateId < PlateBefore.Num() && PlateId < PlateAfter.Num(); ++PlateId)
		{
			const double PlateResidual = (PlateAfter[PlateId] - PlateBefore[PlateId]) - (PlateRecordDelta.IsValidIndex(PlateId) ? PlateRecordDelta[PlateId] : 0.0);
			if (FMath::Abs(PlateResidual) > FMath::Abs(LedgerMetrics.MaxPerPlateContinentalResidual))
			{
				LedgerMetrics.MaxPerPlateContinentalResidual = PlateResidual;
				LedgerMetrics.MaxPerPlateContinentalResidualPlateId = PlateId;
			}
		}
		HashMixDouble(LedgerHash, LedgerMetrics.ContinentalMassBefore);
		HashMixDouble(LedgerHash, LedgerMetrics.ContinentalMassAfter);
		HashMixDouble(LedgerHash, LedgerMetrics.LedgerContinentalDelta);
		HashMixDouble(LedgerHash, LedgerMetrics.ContinentalDeltaResidual);
		LedgerMetrics.MaterialLedgerHash = HashToString(LedgerHash);
		OutMetrics.MaterialLedgerHash = LedgerMetrics.MaterialLedgerHash;
		if (OutMaterialMetrics != nullptr)
		{
			*OutMaterialMetrics = LedgerMetrics;
		}
	}

	for (CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (NewPlateIds.IsValidIndex(Sample.Id) && NewPlateIds[Sample.Id] != INDEX_NONE)
		{
			Sample.PlateId = NewPlateIds[Sample.Id];
			Sample.ContinentalFraction = FMath::Clamp(NewFractions[Sample.Id], 0.0, 1.0);
			Sample.bContinental = Sample.ContinentalFraction >= 0.5;
		}
	}

	CarrierLab::FCarrierLabStage0::RebuildPlateLocalStateFromSamples(State);
	bPlateRayMeshTopologyDirty = true;
	bProjectionRayMeshTopologyDirty = true;
	for (FCarrierLabVisualizationMotion& Motion : Motions)
	{
		Motion.CurrentCenter = FVector3d::ZeroVector;
	}
	TArray<int32> Counts;
	Counts.Init(0, Motions.Num());
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (Motions.IsValidIndex(Sample.PlateId))
		{
			Motions[Sample.PlateId].CurrentCenter += Sample.UnitPosition;
			++Counts[Sample.PlateId];
		}
	}
	for (int32 PlateId = 0; PlateId < Motions.Num(); ++PlateId)
	{
		Motions[PlateId].CurrentCenter = NormalizeOrFallback(
			Motions[PlateId].CurrentCenter,
			State.Plates.IsValidIndex(PlateId) ? State.Plates[PlateId].InitialCenter : FVector3d::UnitZ());
	}

	AreaAfter.Init(0.0, State.Plates.Num());
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (AreaAfter.IsValidIndex(Sample.PlateId))
		{
			AreaAfter[Sample.PlateId] += Sample.AreaWeight;
		}
	}
	for (int32 PlateId = 0; PlateId < AreaBefore.Num() && PlateId < AreaAfter.Num(); ++PlateId)
	{
		const double DeltaPercent = AreaBefore[PlateId] > UE_DOUBLE_SMALL_NUMBER
			? 100.0 * FMath::Abs(AreaAfter[PlateId] - AreaBefore[PlateId]) / AreaBefore[PlateId]
			: 0.0;
		if (DeltaPercent > OutMetrics.MaxPlateAreaDeltaPercent)
		{
			OutMetrics.MaxPlateAreaDeltaPercent = DeltaPercent;
			OutMetrics.MaxPlateAreaDeltaPlateId = PlateId;
		}
	}

	++CurrentMetrics.EventCount;
	const int32 EventCount = CurrentMetrics.EventCount;
	CaptureDriftReference();
	ProjectCurrentCarrier();
	CurrentMetrics.EventCount = EventCount;
	CurrentMetrics.LastGapFillCount = OutMetrics.GapFillCount;
	CurrentMetrics.LastNonSeparatingGapFillCount = OutMetrics.NonSeparatingGapFillCount;
	CurrentMetrics.ResampleEventSeconds = FPlatformTime::Seconds() - EventStartSeconds;
	CurrentMetrics.ProjectionSeconds += CurrentMetrics.ResampleEventSeconds;

	OutMetrics.AuthoritativeCAFAfter = CurrentMetrics.AuthoritativeCAF;
	OutMetrics.ProjectedCAFAfter = CurrentMetrics.ProjectedCAF;
	OutMetrics.ProjectionHashAfter = CurrentMetrics.LastHash;
	OutMetrics.StateHashAfter = CurrentMetrics.StateHash;
	OutMetrics.ResampleEventSeconds = CurrentMetrics.ResampleEventSeconds;

	uint64 DecisionHash = 1469598103934665603ull;
	HashMix(DecisionHash, static_cast<uint64>(OutMetrics.EventId + 1));
	HashMix(DecisionHash, static_cast<uint64>(OutMetrics.Step + 1));
	HashMix(DecisionHash, static_cast<uint64>(OutMetrics.SampleCount + 1));
	HashMix(DecisionHash, static_cast<uint64>(OutMetrics.FilteredCandidateCount + 1));
	HashMix(DecisionHash, static_cast<uint64>(OutMetrics.UnresolvedMultiHitSampleCount + 1));
	HashMix(DecisionHash, static_cast<uint64>(OutMetrics.FilterExhaustedSampleCount + 1));
	HashMixDouble(DecisionHash, OutMetrics.AuthoritativeCAFBefore);
	HashMixDouble(DecisionHash, OutMetrics.AuthoritativeCAFAfter);
	HashMixDouble(DecisionHash, OutMetrics.ProjectedCAFBefore);
	HashMixDouble(DecisionHash, OutMetrics.ProjectedCAFAfter);
	for (const FCarrierLabPhaseIIFilterDecisionRecord& Decision : OutDecisions)
	{
		HashMix(DecisionHash, static_cast<uint64>(Decision.DecisionId + 1));
		HashMix(DecisionHash, static_cast<uint64>(Decision.EventId + 1));
		HashMix(DecisionHash, static_cast<uint64>(Decision.Step + 1));
		HashMix(DecisionHash, static_cast<uint64>(Decision.SampleId + 1));
		HashMix(DecisionHash, static_cast<uint64>(Decision.RawCandidateCount + 1));
		HashMix(DecisionHash, static_cast<uint64>(Decision.RawPlateCount + 1));
		HashMix(DecisionHash, static_cast<uint64>(Decision.FilteredCandidateCount + 1));
		HashMix(DecisionHash, static_cast<uint64>(Decision.PostFilterCandidateCount + 1));
		HashMix(DecisionHash, static_cast<uint64>(Decision.PostFilterPlateCount + 1));
		HashMix(DecisionHash, static_cast<uint64>(Decision.ResolvedPlateId + 1));
		HashMix(DecisionHash, static_cast<uint64>(Decision.FilteredPlateId + 1));
		HashMix(DecisionHash, static_cast<uint64>(Decision.FilteredLocalTriangleId + 1));
		HashMix(DecisionHash, static_cast<uint64>(Decision.SourceContactId + 1));
		HashMix(DecisionHash, static_cast<uint64>(Decision.SourceLabelId + 1));
		HashMix(DecisionHash, Decision.bBoundaryEvidence ? 1ull : 0ull);
		HashMix(DecisionHash, Decision.bThirdPlateEvidence ? 1ull : 0ull);
		HashMix(DecisionHash, static_cast<uint64>(Decision.DecisionClass) + 1);
	}
	OutMetrics.FilterDecisionHash = HashToString(DecisionHash);
	return true;
}

bool ACarrierLabVisualizationActor::GetPhaseIIMotion(const int32 PlateId, FCarrierLabVisualizationMotion& OutMotion) const
{
	if (!Motions.IsValidIndex(PlateId))
	{
		return false;
	}
	OutMotion = Motions[PlateId];
	return true;
}

double ACarrierLabVisualizationActor::ComputePhaseIIPairSignedConvergenceVelocity(const int32 PlateA, const int32 PlateB) const
{
	if (!Motions.IsValidIndex(PlateA) || !Motions.IsValidIndex(PlateB))
	{
		return 0.0;
	}
	const FVector3d Evidence = NormalizeOrFallback(Motions[PlateA].CurrentCenter + Motions[PlateB].CurrentCenter, Motions[PlateA].CurrentCenter);
	return -SignedPairSeparationVelocityForPlatePair(Evidence, Motions, PlateA, PlateB);
}

bool ACarrierLabVisualizationActor::GetPhaseIIIA1ElevationAudit(
	int32& OutSampleCount,
	int32& OutPlateVertexCount,
	double& OutMaxAbsSampleElevation,
	double& OutMaxAbsPlateVertexElevation) const
{
	OutSampleCount = State.Samples.Num();
	OutPlateVertexCount = 0;
	OutMaxAbsSampleElevation = 0.0;
	OutMaxAbsPlateVertexElevation = 0.0;
	if (!bInitialized)
	{
		return false;
	}

	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		OutMaxAbsSampleElevation = FMath::Max(OutMaxAbsSampleElevation, FMath::Abs(Sample.Elevation));
	}
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		OutPlateVertexCount += Plate.Vertices.Num();
		for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			OutMaxAbsPlateVertexElevation = FMath::Max(OutMaxAbsPlateVertexElevation, FMath::Abs(Vertex.Elevation));
		}
	}
	return true;
}

bool ACarrierLabVisualizationActor::RefreshPlateRayMeshes(FString& OutError)
{
	if (bPlateRayMeshTopologyDirty)
	{
		if (!BuildPlateRayMeshTopology(State, PlateRayMeshes, OutError))
		{
			return false;
		}
		bPlateRayMeshTopologyDirty = false;
	}

	if (RefreshPlateRayMeshVerticesAndTrees(State, PlateRayMeshes, OutError))
	{
		return true;
	}

	bPlateRayMeshTopologyDirty = true;
	if (!BuildPlateRayMeshTopology(State, PlateRayMeshes, OutError))
	{
		return false;
	}
	bPlateRayMeshTopologyDirty = false;
	return RefreshPlateRayMeshVerticesAndTrees(State, PlateRayMeshes, OutError);
}

bool ACarrierLabVisualizationActor::RefreshProjectionRayMesh(FString& OutError)
{
	if (bProjectionRayMeshTopologyDirty)
	{
		if (!BuildProjectionRayMeshTopology(State, ProjectionRayMesh, OutError))
		{
			return false;
		}
		bProjectionRayMeshTopologyDirty = false;
	}

	if (RefreshProjectionRayMeshVerticesAndTree(State, ProjectionRayMesh, OutError))
	{
		return true;
	}

	bProjectionRayMeshTopologyDirty = true;
	if (!BuildProjectionRayMeshTopology(State, ProjectionRayMesh, OutError))
	{
		return false;
	}
	bProjectionRayMeshTopologyDirty = false;
	return RefreshProjectionRayMeshVerticesAndTree(State, ProjectionRayMesh, OutError);
}

void ACarrierLabVisualizationActor::TogglePlay()
{
	bPlaying = !bPlaying;
}

void ACarrierLabVisualizationActor::SetPlaying(const bool bNewPlaying)
{
	bPlaying = bNewPlaying;
}

int32 ACarrierLabVisualizationActor::GetNaturalCadenceSteps() const
{
	constexpr double ResamplingMMaxMa = 128.0;
	constexpr double ResamplingMMinMa = 32.0;
	constexpr double V0MmPerYear = 100.0;
	const double Alpha = FMath::Min(1.0, VelocityMmPerYear / V0MmPerYear);
	const double CadenceMa = (1.0 - Alpha) * ResamplingMMaxMa + Alpha * ResamplingMMinMa;
	return FMath::Max(1, FMath::RoundToInt(CadenceMa / DeltaTimeMa));
}

void ACarrierLabVisualizationActor::StepOnce()
{
	if (!bInitialized && !InitializeCarrier())
	{
		return;
	}
	AdvanceOneStep();
	ProjectCurrentCarrier();
}

void ACarrierLabVisualizationActor::ApplyResampleEvent()
{
	if (!bInitialized && !InitializeCarrier())
	{
		return;
	}

	const double StartSeconds = FPlatformTime::Seconds();
	FString MeshError;
	if (!RefreshProjectionRayMesh(MeshError))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab visualization resample failed: %s"), *MeshError);
		return;
	}

	const FCarrierLabVizBoundaryIndex BoundaryIndex = BuildBoundaryIndex(State);
	TArray<int32> NewPlateIds;
	TArray<double> NewFractions;
	NewPlateIds.Init(INDEX_NONE, State.Samples.Num());
	NewFractions.Init(0.0, State.Samples.Num());

	int32 GapFillCount = 0;
	int32 NonSeparatingGapFillCount = 0;
	TArray<FCarrierLabVizCandidate> Candidates;
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		uint64 PlateMask = 0;
		bool bAnyBoundary = false;
		QuerySampleCandidates(State, ProjectionRayMesh, Sample, Candidates, PlateMask, bAnyBoundary);
		const int32 PlateHitCount = CountBits64(PlateMask);
		if (PlateHitCount == 0)
		{
			FCarrierLabVizBoundaryPoint Q1;
			FCarrierLabVizBoundaryPoint Q2;
			if (FindNearestBoundaryPair(BoundaryIndex, Sample.UnitPosition, Q1, Q2))
			{
				const FVector3d RidgePoint = NormalizeOrFallback(Q1.UnitPosition + Q2.UnitPosition, Sample.UnitPosition);
				const double SignedVelocity = SignedPairSeparationVelocityForPlatePair(RidgePoint, Motions, Q1.PlateId, Q2.PlateId);
				++GapFillCount;
				if (SignedVelocity <= 0.0)
				{
					++NonSeparatingGapFillCount;
				}
				NewPlateIds[Sample.Id] = Q1.PlateId;
				NewFractions[Sample.Id] = FMath::Clamp(0.5 * (Q1.ContinentalFraction + Q2.ContinentalFraction), 0.0, 1.0);
			}
			else
			{
				NewPlateIds[Sample.Id] = Sample.PlateId;
				NewFractions[Sample.Id] = Sample.ContinentalFraction;
			}
			continue;
		}

		const int32 ResolvedPlateId = PlateHitCount == 1
			? LowestSetBitIndex(PlateMask)
			: ChooseCandidatePlateByPolicy(Sample, Candidates, Motions, MultiHitPolicy, RandomTieBreakSeed, CurrentMetrics.Step, CurrentMetrics.EventCount + 1);
		const FCarrierLabVizCandidate* Chosen = Candidates.FindByPredicate([ResolvedPlateId](const FCarrierLabVizCandidate& Candidate)
		{
			return Candidate.PlateId == ResolvedPlateId;
		});
		NewPlateIds[Sample.Id] = ResolvedPlateId;
		NewFractions[Sample.Id] = (Chosen != nullptr && State.Plates.IsValidIndex(ResolvedPlateId))
			? InterpolateContinentalFraction(State.Plates[ResolvedPlateId], *Chosen)
			: 0.0;
	}

	for (CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (NewPlateIds.IsValidIndex(Sample.Id) && NewPlateIds[Sample.Id] != INDEX_NONE)
		{
			Sample.PlateId = NewPlateIds[Sample.Id];
			Sample.ContinentalFraction = FMath::Clamp(NewFractions[Sample.Id], 0.0, 1.0);
			Sample.bContinental = Sample.ContinentalFraction >= 0.5;
		}
	}

	CarrierLab::FCarrierLabStage0::RebuildPlateLocalStateFromSamples(State);
	bPlateRayMeshTopologyDirty = true;
	bProjectionRayMeshTopologyDirty = true;
	for (FCarrierLabVisualizationMotion& Motion : Motions)
	{
		Motion.CurrentCenter = FVector3d::ZeroVector;
	}
	TArray<int32> Counts;
	Counts.Init(0, Motions.Num());
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (Motions.IsValidIndex(Sample.PlateId))
		{
			Motions[Sample.PlateId].CurrentCenter += Sample.UnitPosition;
			++Counts[Sample.PlateId];
		}
	}
	for (int32 PlateId = 0; PlateId < Motions.Num(); ++PlateId)
	{
		Motions[PlateId].CurrentCenter = NormalizeOrFallback(
			Motions[PlateId].CurrentCenter,
			State.Plates.IsValidIndex(PlateId) ? State.Plates[PlateId].InitialCenter : FVector3d::UnitZ());
	}

	++CurrentMetrics.EventCount;
	const int32 EventCount = CurrentMetrics.EventCount;
	CaptureDriftReference();
	ProjectCurrentCarrier();
	CurrentMetrics.EventCount = EventCount;
	CurrentMetrics.LastGapFillCount = GapFillCount;
	CurrentMetrics.LastNonSeparatingGapFillCount = NonSeparatingGapFillCount;
	CurrentMetrics.ResampleEventSeconds = FPlatformTime::Seconds() - StartSeconds;
	CurrentMetrics.ProjectionSeconds += CurrentMetrics.ResampleEventSeconds;
}

void ACarrierLabVisualizationActor::SetVisualizationLayer(const ECarrierLabVisualizationLayer NewLayer)
{
	VisualizationLayer = NewLayer;
	if (bInitialized)
	{
		RebuildRenderMesh();
	}
}

void ACarrierLabVisualizationActor::SetMultiHitPolicy(const ECarrierLabMultiHitPolicy NewPolicy)
{
	MultiHitPolicy = NewPolicy;
	if (bInitialized)
	{
		ProjectCurrentCarrier();
	}
}

void ACarrierLabVisualizationActor::ShowPlateIdLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::PlateId);
}

void ACarrierLabVisualizationActor::ShowContinentalFractionLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::ContinentalFraction);
}

void ACarrierLabVisualizationActor::ShowMissMaskLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::MissMask);
}

void ACarrierLabVisualizationActor::ShowOverlapMaskLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::OverlapMask);
}

void ACarrierLabVisualizationActor::ShowBoundaryMaskLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::BoundaryMask);
}

void ACarrierLabVisualizationActor::BindInputControls()
{
	APlayerController* PlayerController = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PlayerController == nullptr)
	{
		return;
	}
	EnableInput(PlayerController);
	if (InputComponent == nullptr)
	{
		return;
	}

	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ACarrierLabVisualizationActor::TogglePlay);
	InputComponent->BindKey(EKeys::Period, IE_Pressed, this, &ACarrierLabVisualizationActor::StepOnce);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ACarrierLabVisualizationActor::ApplyResampleEvent);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowPlateIdLayer);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowContinentalFractionLayer);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowMissMaskLayer);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowOverlapMaskLayer);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowBoundaryMaskLayer);
}

void ACarrierLabVisualizationActor::AdvanceOneStep()
{
	for (CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		if (!Motions.IsValidIndex(Plate.PlateId))
		{
			continue;
		}
		const FCarrierLabVisualizationMotion& Motion = Motions[Plate.PlateId];
		for (CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			Vertex.UnitPosition = NormalizeOrFallback(RotateVector(Vertex.UnitPosition, Motion.Axis, Motion.AngularSpeedRadiansPerStep), Vertex.UnitPosition);
		}
	}
	for (FCarrierLabVisualizationMotion& Motion : Motions)
	{
		Motion.CurrentCenter = NormalizeOrFallback(RotateVector(Motion.CurrentCenter, Motion.Axis, Motion.AngularSpeedRadiansPerStep), Motion.CurrentCenter);
	}
	++CurrentMetrics.Step;
	const int32 Cadence = GetNaturalCadenceSteps();
	CurrentMetrics.NextResampleStep = ((CurrentMetrics.Step / Cadence) + 1) * Cadence;
}

void ACarrierLabVisualizationActor::CaptureDriftReference()
{
	DriftReferencePositions.Reset(State.Plates.Num());
	DriftReferencePositions.SetNum(State.Plates.Num());
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		if (!DriftReferencePositions.IsValidIndex(Plate.PlateId))
		{
			continue;
		}
		TArray<FVector3d>& Positions = DriftReferencePositions[Plate.PlateId];
		Positions.Reset(Plate.Vertices.Num());
		for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			Positions.Add(Vertex.UnitPosition);
		}
	}
	DriftReferenceStep = CurrentMetrics.Step;
}

void ACarrierLabVisualizationActor::ComputeDriftMetrics()
{
	DriftErrorKmBySample.Init(-1.0, State.Samples.Num());
	CurrentMetrics.DriftErrorMeanKm = 0.0;
	CurrentMetrics.DriftErrorP95Km = 0.0;

	TArray<double> ErrorsKm;
	double ErrorSumKm = 0.0;
	const int32 RelativeStep = FMath::Max(0, CurrentMetrics.Step - DriftReferenceStep);
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		if (!DriftReferencePositions.IsValidIndex(Plate.PlateId) || !Motions.IsValidIndex(Plate.PlateId))
		{
			continue;
		}
		const TArray<FVector3d>& ReferencePositions = DriftReferencePositions[Plate.PlateId];
		const FCarrierLabVisualizationMotion& Motion = Motions[Plate.PlateId];
		for (int32 LocalVertexId = 0; LocalVertexId < Plate.Vertices.Num(); ++LocalVertexId)
		{
			if (!ReferencePositions.IsValidIndex(LocalVertexId))
			{
				continue;
			}
			const CarrierLab::FCarrierVertex& Vertex = Plate.Vertices[LocalVertexId];
			const FVector3d Expected = NormalizeOrFallback(
				RotateVector(ReferencePositions[LocalVertexId], Motion.Axis, Motion.AngularSpeedRadiansPerStep * static_cast<double>(RelativeStep)),
				ReferencePositions[LocalVertexId]);
			const double ErrorKm = FMath::Acos(FMath::Clamp(FVector3d::DotProduct(Expected, Vertex.UnitPosition), -1.0, 1.0)) * EarthRadiusKm;
			if (!FMath::IsFinite(ErrorKm))
			{
				++CurrentMetrics.NaNOrInfCount;
				continue;
			}
			ErrorSumKm += ErrorKm;
			ErrorsKm.Add(ErrorKm);
			if (DriftErrorKmBySample.IsValidIndex(Vertex.GlobalSampleId))
			{
				DriftErrorKmBySample[Vertex.GlobalSampleId] = FMath::Max(DriftErrorKmBySample[Vertex.GlobalSampleId], ErrorKm);
			}
		}
	}

	if (!ErrorsKm.IsEmpty())
	{
		ErrorsKm.Sort();
		CurrentMetrics.DriftErrorMeanKm = ErrorSumKm / static_cast<double>(ErrorsKm.Num());
		CurrentMetrics.DriftErrorP95Km = ErrorsKm[FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(0.95 * (ErrorsKm.Num() - 1))), 0, ErrorsKm.Num() - 1)];
	}
}

void ACarrierLabVisualizationActor::ComputePlateBoundaryMask()
{
	PlateBoundaryMask.Init(0, State.Samples.Num());
	CurrentMetrics.BoundaryVertexCount = 0;

	for (const CarrierLab::FSphereTriangle& Triangle : State.Triangles)
	{
		if (!RenderPlateIds.IsValidIndex(Triangle.A) ||
			!RenderPlateIds.IsValidIndex(Triangle.B) ||
			!RenderPlateIds.IsValidIndex(Triangle.C))
		{
			continue;
		}

		const int32 PlateA = RenderPlateIds[Triangle.A];
		const int32 PlateB = RenderPlateIds[Triangle.B];
		const int32 PlateC = RenderPlateIds[Triangle.C];
		if (PlateA == PlateB && PlateB == PlateC)
		{
			continue;
		}

		if (PlateBoundaryMask.IsValidIndex(Triangle.A) && PlateBoundaryMask[Triangle.A] == 0)
		{
			PlateBoundaryMask[Triangle.A] = 1;
			++CurrentMetrics.BoundaryVertexCount;
		}
		if (PlateBoundaryMask.IsValidIndex(Triangle.B) && PlateBoundaryMask[Triangle.B] == 0)
		{
			PlateBoundaryMask[Triangle.B] = 1;
			++CurrentMetrics.BoundaryVertexCount;
		}
		if (PlateBoundaryMask.IsValidIndex(Triangle.C) && PlateBoundaryMask[Triangle.C] == 0)
		{
			PlateBoundaryMask[Triangle.C] = 1;
			++CurrentMetrics.BoundaryVertexCount;
		}
	}
}

void ACarrierLabVisualizationActor::ProjectCurrentCarrier()
{
	if (!bInitialized)
	{
		return;
	}

	const double StartSeconds = FPlatformTime::Seconds();
	CurrentMetrics.SampleCount = State.Samples.Num();
	CurrentMetrics.PlateCount = State.Plates.Num();
	CurrentMetrics.RawMissCount = 0;
	CurrentMetrics.RawMultiHitCount = 0;
	CurrentMetrics.BoundaryHitCount = 0;
	CurrentMetrics.BoundaryVertexCount = 0;
	CurrentMetrics.NaNOrInfCount = 0;
	CurrentMetrics.BvhBuildSeconds = 0.0;
	CurrentMetrics.ProjectionQuerySeconds = 0.0;
	CurrentMetrics.DriftMetricsSeconds = 0.0;
	CurrentMetrics.BoundaryMaskSeconds = 0.0;
	CurrentMetrics.HashSeconds = 0.0;
	CurrentMetrics.ResampleEventSeconds = 0.0;
	CurrentMetrics.MeshUpdateSeconds = 0.0;
	const int32 Cadence = GetNaturalCadenceSteps();
	CurrentMetrics.NextResampleStep = ((CurrentMetrics.Step / Cadence) + 1) * Cadence;

	RenderPlateIds.Init(INDEX_NONE, State.Samples.Num());
	RenderContinentalFractions.Init(0.0, State.Samples.Num());
	DriftErrorKmBySample.Init(-1.0, State.Samples.Num());
	MissMask.Init(0, State.Samples.Num());
	OverlapMask.Init(0, State.Samples.Num());
	BoundaryMask.Init(0, State.Samples.Num());
	PlateBoundaryMask.Init(0, State.Samples.Num());

	FString MeshError;
	const double BvhStartSeconds = FPlatformTime::Seconds();
	if (!RefreshProjectionRayMesh(MeshError))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab visualization projection failed: %s"), *MeshError);
		return;
	}
	CurrentMetrics.BvhBuildSeconds = FPlatformTime::Seconds() - BvhStartSeconds;

	const double QueryStartSeconds = FPlatformTime::Seconds();
	double TotalArea = 0.0;
	double AuthoritativeContinentalArea = 0.0;
	double ProjectedContinentalArea = 0.0;
	TArray<double> AuthoritativeAreaBySample;
	TArray<double> ProjectedAreaBySample;
	TArray<uint8> NaNOrInfMask;
	AuthoritativeAreaBySample.Init(0.0, State.Samples.Num());
	ProjectedAreaBySample.Init(0.0, State.Samples.Num());
	NaNOrInfMask.Init(0, State.Samples.Num());
	const ECarrierLabMultiHitPolicy ProjectionPolicy = MultiHitPolicy;
	const int32 ProjectionTieBreakSeed = RandomTieBreakSeed;
	const int32 ProjectionStep = CurrentMetrics.Step;
	const int32 ProjectionEventCount = CurrentMetrics.EventCount;
	ParallelFor(State.Samples.Num(), [this, &AuthoritativeAreaBySample, &ProjectedAreaBySample, &NaNOrInfMask, ProjectionPolicy, ProjectionTieBreakSeed, ProjectionStep, ProjectionEventCount](int32 SampleIndex)
	{
		const CarrierLab::FSphereSample& Sample = State.Samples[SampleIndex];
		const int32 OutputIndex = Sample.Id;
		if (!RenderPlateIds.IsValidIndex(OutputIndex))
		{
			return;
		}

		AuthoritativeAreaBySample[OutputIndex] = Sample.AreaWeight * Sample.ContinentalFraction;
		if (!FMath::IsFinite(Sample.UnitPosition.X) || !FMath::IsFinite(Sample.UnitPosition.Y) || !FMath::IsFinite(Sample.UnitPosition.Z))
		{
			NaNOrInfMask[OutputIndex] = 1;
		}

		TArray<FCarrierLabVizCandidate> Candidates;
		uint64 PlateMask = 0;
		bool bAnyBoundary = false;
		QuerySampleCandidates(State, ProjectionRayMesh, Sample, Candidates, PlateMask, bAnyBoundary);
		const int32 PlateHitCount = CountBits64(PlateMask);
		BoundaryMask[OutputIndex] = bAnyBoundary ? 1 : 0;

		if (PlateHitCount == 0)
		{
			MissMask[OutputIndex] = 1;
			return;
		}
		if (PlateHitCount > 1)
		{
			OverlapMask[OutputIndex] = 1;
		}

		const int32 ResolvedPlateId = PlateHitCount == 1
			? LowestSetBitIndex(PlateMask)
			: ChooseCandidatePlateByPolicy(Sample, Candidates, Motions, ProjectionPolicy, ProjectionTieBreakSeed, ProjectionStep, ProjectionEventCount);
		const FCarrierLabVizCandidate* Chosen = Candidates.FindByPredicate([ResolvedPlateId](const FCarrierLabVizCandidate& Candidate)
		{
			return Candidate.PlateId == ResolvedPlateId;
		});
		const double Fraction = (Chosen != nullptr && State.Plates.IsValidIndex(ResolvedPlateId))
			? InterpolateContinentalFraction(State.Plates[ResolvedPlateId], *Chosen)
			: 0.0;
		RenderPlateIds[OutputIndex] = ResolvedPlateId;
		RenderContinentalFractions[OutputIndex] = Fraction;
		ProjectedAreaBySample[OutputIndex] = Sample.AreaWeight * Fraction;
	});

	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		const int32 OutputIndex = Sample.Id;
		TotalArea += Sample.AreaWeight;
		if (!AuthoritativeAreaBySample.IsValidIndex(OutputIndex))
		{
			continue;
		}
		AuthoritativeContinentalArea += AuthoritativeAreaBySample[OutputIndex];
		ProjectedContinentalArea += ProjectedAreaBySample[OutputIndex];
		CurrentMetrics.NaNOrInfCount += NaNOrInfMask[OutputIndex] != 0 ? 1 : 0;
		CurrentMetrics.BoundaryHitCount += BoundaryMask[OutputIndex] != 0 ? 1 : 0;
		CurrentMetrics.RawMissCount += MissMask[OutputIndex] != 0 ? 1 : 0;
		CurrentMetrics.RawMultiHitCount += OverlapMask[OutputIndex] != 0 ? 1 : 0;
	}
	CurrentMetrics.ProjectionQuerySeconds = FPlatformTime::Seconds() - QueryStartSeconds;

	CurrentMetrics.AuthoritativeCAF = TotalArea > UE_DOUBLE_SMALL_NUMBER ? AuthoritativeContinentalArea / TotalArea : 0.0;
	CurrentMetrics.ProjectedCAF = TotalArea > UE_DOUBLE_SMALL_NUMBER ? ProjectedContinentalArea / TotalArea : 0.0;
	const double DriftStartSeconds = FPlatformTime::Seconds();
	ComputeDriftMetrics();
	CurrentMetrics.DriftMetricsSeconds = FPlatformTime::Seconds() - DriftStartSeconds;
	const double BoundaryStartSeconds = FPlatformTime::Seconds();
	ComputePlateBoundaryMask();
	CurrentMetrics.BoundaryMaskSeconds = FPlatformTime::Seconds() - BoundaryStartSeconds;
	CurrentMetrics.ProjectionSeconds = FPlatformTime::Seconds() - StartSeconds;
	const double HashStartSeconds = FPlatformTime::Seconds();
	UpdateLastHash();
	CurrentMetrics.HashSeconds = FPlatformTime::Seconds() - HashStartSeconds;
	RebuildRenderMesh();
}

void ACarrierLabVisualizationActor::UpdateLastHash()
{
	uint64 ProjectionHash = 1469598103934665603ull;
	HashMix(ProjectionHash, static_cast<uint64>(CurrentMetrics.Step + 1));
	HashMix(ProjectionHash, static_cast<uint64>(CurrentMetrics.EventCount + 1));
	HashMix(ProjectionHash, static_cast<uint64>(CurrentMetrics.RawMissCount + 1));
	HashMix(ProjectionHash, static_cast<uint64>(CurrentMetrics.RawMultiHitCount + 1));
	HashMixDouble(ProjectionHash, CurrentMetrics.AuthoritativeCAF);
	HashMixDouble(ProjectionHash, CurrentMetrics.ProjectedCAF);
	HashMixDouble(ProjectionHash, CurrentMetrics.DriftErrorMeanKm);
	HashMixDouble(ProjectionHash, CurrentMetrics.DriftErrorP95Km);
	for (int32 Index = 0; Index < RenderPlateIds.Num(); ++Index)
	{
		HashMix(ProjectionHash, static_cast<uint64>(RenderPlateIds[Index] + 1));
		HashMixDouble(ProjectionHash, RenderContinentalFractions.IsValidIndex(Index) ? RenderContinentalFractions[Index] : 0.0);
		HashMix(ProjectionHash, MissMask.IsValidIndex(Index) ? MissMask[Index] : 0);
		HashMix(ProjectionHash, OverlapMask.IsValidIndex(Index) ? OverlapMask[Index] : 0);
		HashMix(ProjectionHash, BoundaryMask.IsValidIndex(Index) ? BoundaryMask[Index] : 0);
		HashMix(ProjectionHash, PlateBoundaryMask.IsValidIndex(Index) ? PlateBoundaryMask[Index] : 0);
	}
	CurrentMetrics.LastHash = HashToString(ProjectionHash);

	uint64 StateHash = 1469598103934665603ull;
	HashMix(StateHash, static_cast<uint64>(CurrentMetrics.Step + 1));
	HashMix(StateHash, static_cast<uint64>(CurrentMetrics.EventCount + 1));
	HashMix(StateHash, static_cast<uint64>(State.Samples.Num() + 1));
	HashMix(StateHash, static_cast<uint64>(State.Plates.Num() + 1));
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		HashMix(StateHash, static_cast<uint64>(Sample.Id + 1));
		HashMix(StateHash, static_cast<uint64>(Sample.PlateId + 1));
		HashMixDouble(StateHash, Sample.UnitPosition.X);
		HashMixDouble(StateHash, Sample.UnitPosition.Y);
		HashMixDouble(StateHash, Sample.UnitPosition.Z);
		HashMixDouble(StateHash, Sample.AreaWeight);
		HashMixDouble(StateHash, Sample.ContinentalFraction);
	}
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		HashMix(StateHash, static_cast<uint64>(Plate.PlateId + 1));
		HashMixDouble(StateHash, Plate.InitialCenter.X);
		HashMixDouble(StateHash, Plate.InitialCenter.Y);
		HashMixDouble(StateHash, Plate.InitialCenter.Z);
		HashMix(StateHash, Plate.bContinental ? 1 : 0);
		HashMix(StateHash, static_cast<uint64>(Plate.Vertices.Num() + 1));
		HashMix(StateHash, static_cast<uint64>(Plate.LocalTriangles.Num() + 1));
		for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			HashMix(StateHash, static_cast<uint64>(Vertex.GlobalSampleId + 1));
			HashMixDouble(StateHash, Vertex.UnitPosition.X);
			HashMixDouble(StateHash, Vertex.UnitPosition.Y);
			HashMixDouble(StateHash, Vertex.UnitPosition.Z);
			HashMixDouble(StateHash, Vertex.AreaWeight);
			HashMixDouble(StateHash, Vertex.ContinentalFraction);
		}
		for (const CarrierLab::FCarrierPlateTriangle& Triangle : Plate.LocalTriangles)
		{
			HashMix(StateHash, static_cast<uint64>(Triangle.SourceTriangleId + 1));
			HashMix(StateHash, static_cast<uint64>(Triangle.A + 1));
			HashMix(StateHash, static_cast<uint64>(Triangle.B + 1));
			HashMix(StateHash, static_cast<uint64>(Triangle.C + 1));
		}
	}
	CurrentMetrics.StateHash = HashToString(StateHash);

	uint64 CrustHash = 1469598103934665603ull;
	HashMix(CrustHash, static_cast<uint64>(CurrentMetrics.Step + 1));
	HashMix(CrustHash, static_cast<uint64>(CurrentMetrics.EventCount + 1));
	HashMix(CrustHash, static_cast<uint64>(State.Samples.Num() + 1));
	HashMix(CrustHash, static_cast<uint64>(State.Plates.Num() + 1));
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		HashMix(CrustHash, static_cast<uint64>(Sample.Id + 1));
		HashMix(CrustHash, static_cast<uint64>(Sample.PlateId + 1));
		HashMixDouble(CrustHash, Sample.Elevation);
	}
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		HashMix(CrustHash, static_cast<uint64>(Plate.PlateId + 1));
		HashMix(CrustHash, static_cast<uint64>(Plate.Vertices.Num() + 1));
		for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			HashMix(CrustHash, static_cast<uint64>(Vertex.GlobalSampleId + 1));
			HashMixDouble(CrustHash, Vertex.Elevation);
		}
	}
	CurrentMetrics.CrustStateHash = HashToString(CrustHash);
}

FLinearColor ACarrierLabVisualizationActor::ColorForSample(const int32 SampleId) const
{
	switch (VisualizationLayer)
	{
	case ECarrierLabVisualizationLayer::ContinentalFraction:
		return ContinentalColor(RenderContinentalFractions.IsValidIndex(SampleId) ? RenderContinentalFractions[SampleId] : 0.0);
	case ECarrierLabVisualizationLayer::MissMask:
		return MissMask.IsValidIndex(SampleId) && MissMask[SampleId] != 0
			? FLinearColor(0.95f, 0.05f, 0.04f, 1.0f)
			: FLinearColor(0.02f, 0.05f, 0.09f, 1.0f);
	case ECarrierLabVisualizationLayer::OverlapMask:
		return OverlapMask.IsValidIndex(SampleId) && OverlapMask[SampleId] != 0
			? FLinearColor(1.0f, 0.46f, 0.05f, 1.0f)
			: FLinearColor(0.02f, 0.05f, 0.09f, 1.0f);
	case ECarrierLabVisualizationLayer::BoundaryMask:
		return PlateBoundaryMask.IsValidIndex(SampleId) && PlateBoundaryMask[SampleId] != 0
			? FLinearColor(0.95f, 0.95f, 0.90f, 1.0f)
			: FLinearColor(0.02f, 0.05f, 0.09f, 1.0f);
	case ECarrierLabVisualizationLayer::DriftError:
		return DriftColor(DriftErrorKmBySample.IsValidIndex(SampleId) ? DriftErrorKmBySample[SampleId] : -1.0);
	case ECarrierLabVisualizationLayer::PlateId:
	default:
		return PlateColor(RenderPlateIds.IsValidIndex(SampleId) ? RenderPlateIds[SampleId] : INDEX_NONE);
	}
}

bool ACarrierLabVisualizationActor::BuildRenderMeshTopology()
{
	if (!MeshComponent || State.Samples.IsEmpty() || State.Triangles.IsEmpty())
	{
		return false;
	}

	FDynamicMesh3 Mesh;
	Mesh.EnableTriangleGroups();
	Mesh.EnableAttributes();
	Mesh.Attributes()->EnablePrimaryColors();
	FDynamicMeshColorOverlay* Colors = Mesh.Attributes()->PrimaryColors();

	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		Mesh.AppendVertex(Sample.UnitPosition * SphereRadius);
	}

	for (const CarrierLab::FSphereTriangle& Triangle : State.Triangles)
	{
		const int32 TriangleId = Mesh.AppendTriangle(Triangle.A, Triangle.B, Triangle.C, 0);
		if (TriangleId < 0)
		{
			continue;
		}

		const int32 ColorA = Colors->AppendElement(ToVector4f(ColorForSample(Triangle.A)));
		const int32 ColorB = Colors->AppendElement(ToVector4f(ColorForSample(Triangle.B)));
		const int32 ColorC = Colors->AppendElement(ToVector4f(ColorForSample(Triangle.C)));
		Colors->SetParentVertex(ColorA, Triangle.A);
		Colors->SetParentVertex(ColorB, Triangle.B);
		Colors->SetParentVertex(ColorC, Triangle.C);
		Colors->SetTriangle(TriangleId, FIndex3i(ColorA, ColorB, ColorC));
	}

	MeshComponent->SetMesh(MoveTemp(Mesh));
	CachedRenderMeshSampleCount = State.Samples.Num();
	CachedRenderMeshTriangleCount = State.Triangles.Num();
	bRenderMeshTopologyDirty = false;
	return true;
}

void ACarrierLabVisualizationActor::RebuildRenderMesh()
{
	if (!MeshComponent || State.Samples.IsEmpty() || State.Triangles.IsEmpty())
	{
		return;
	}

	const double StartSeconds = FPlatformTime::Seconds();
	bool bColorsUpdated = false;
	const bool bNeedsTopologyBuild =
		bRenderMeshTopologyDirty ||
		CachedRenderMeshSampleCount != State.Samples.Num() ||
		CachedRenderMeshTriangleCount != State.Triangles.Num();

	if (bNeedsTopologyBuild)
	{
		bColorsUpdated = BuildRenderMeshTopology();
	}
	else
	{
		MeshComponent->EditMesh([this, &bColorsUpdated](FDynamicMesh3& Mesh)
		{
			if (Mesh.VertexCount() != State.Samples.Num() || Mesh.TriangleCount() != State.Triangles.Num() || Mesh.Attributes() == nullptr)
			{
				bRenderMeshTopologyDirty = true;
				return;
			}

			FDynamicMeshColorOverlay* Colors = Mesh.Attributes()->PrimaryColors();
			if (Colors == nullptr)
			{
				bRenderMeshTopologyDirty = true;
				return;
			}

			for (const int32 ElementId : Colors->ElementIndicesItr())
			{
				const int32 ParentVertex = Colors->GetParentVertex(ElementId);
				Colors->SetElement(ElementId, ToVector4f(ColorForSample(ParentVertex)));
			}
			bColorsUpdated = true;
		}, EDynamicMeshComponentRenderUpdateMode::NoUpdate);

		if (bColorsUpdated)
		{
			MeshComponent->FastNotifyColorsUpdated();
		}
		else if (bRenderMeshTopologyDirty)
		{
			bColorsUpdated = BuildRenderMeshTopology();
		}
	}

	MeshComponent->SetColorOverrideMode(EDynamicMeshComponentColorOverrideMode::VertexColors);
	MeshComponent->SetVertexColorSpaceTransformMode(EDynamicMeshVertexColorTransformMode::NoTransform);
	MeshComponent->SetTwoSided(true);
	MeshComponent->SetEnableFlatShading(true);
	MeshComponent->SetEnableWireframeRenderPass(bShowWireframeOverlay);
	CurrentMetrics.MeshUpdateSeconds = FPlatformTime::Seconds() - StartSeconds;
}

FString ACarrierLabVisualizationActor::BuildHudText() const
{
	const TCHAR* LayerName = TEXT("plate id");
	switch (VisualizationLayer)
	{
	case ECarrierLabVisualizationLayer::ContinentalFraction:
		LayerName = TEXT("continental fraction");
		break;
	case ECarrierLabVisualizationLayer::MissMask:
		LayerName = TEXT("miss mask");
		break;
	case ECarrierLabVisualizationLayer::OverlapMask:
		LayerName = TEXT("overlap mask");
		break;
	case ECarrierLabVisualizationLayer::BoundaryMask:
		LayerName = TEXT("boundary mask");
		break;
	case ECarrierLabVisualizationLayer::DriftError:
		LayerName = TEXT("drift error");
		break;
	case ECarrierLabVisualizationLayer::PlateId:
	default:
		break;
	}

	return FString::Printf(
		TEXT("CarrierLab Phase I Viewer | %s | layer=%s\nstep=%d next_resample=%d events=%d samples=%d plates=%d\nmiss=%d multi=%d boundary_vertices=%d boundary_degenerate=%d gap_fill=%d nonsep_gap=%d nan=%d\nAuthCAF=%.6f ProjCAF=%.6f drift_mean=%.9fkm drift_p95=%.9fkm hash=%s\nprojection=%.3fs bvh=%.3fs query=%.3fs drift=%.3fs boundary=%.3fs hash_time=%.3fs render=%.3fs resample=%.3fs\nSpace play/pause | . step | R resample | 1-5 layers"),
		bPlaying ? TEXT("PLAY") : TEXT("PAUSED"),
		LayerName,
		CurrentMetrics.Step,
		CurrentMetrics.NextResampleStep,
		CurrentMetrics.EventCount,
		CurrentMetrics.SampleCount,
		CurrentMetrics.PlateCount,
		CurrentMetrics.RawMissCount,
		CurrentMetrics.RawMultiHitCount,
		CurrentMetrics.BoundaryVertexCount,
		CurrentMetrics.BoundaryHitCount,
		CurrentMetrics.LastGapFillCount,
		CurrentMetrics.LastNonSeparatingGapFillCount,
		CurrentMetrics.NaNOrInfCount,
		CurrentMetrics.AuthoritativeCAF,
		CurrentMetrics.ProjectedCAF,
		CurrentMetrics.DriftErrorMeanKm,
		CurrentMetrics.DriftErrorP95Km,
		*CurrentMetrics.LastHash,
		CurrentMetrics.ProjectionSeconds,
		CurrentMetrics.BvhBuildSeconds,
		CurrentMetrics.ProjectionQuerySeconds,
		CurrentMetrics.DriftMetricsSeconds,
		CurrentMetrics.BoundaryMaskSeconds,
		CurrentMetrics.HashSeconds,
		CurrentMetrics.MeshUpdateSeconds,
		CurrentMetrics.ResampleEventSeconds);
}

void ACarrierLabVisualizationActor::ShowHud() const
{
	if (GEngine == nullptr)
	{
		return;
	}
	GEngine->AddOnScreenDebugMessage(
		static_cast<uint64>(GetUniqueID()),
		0.0f,
		FColor::Cyan,
		BuildHudText(),
		false,
		FVector2D(0.85f, 0.85f));
}
