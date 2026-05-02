// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabCarrier.h"

#include "CompGeom/ConvexHull3.h"
#include "HAL/PlatformMemory.h"

namespace CarrierLab
{
	namespace
	{
		constexpr double Pi = UE_DOUBLE_PI;
		constexpr double GoldenAngle = 2.3999632297286533222;
		constexpr double GeometryEpsilon = 1.0e-12;

		struct FWorkingTriangle
		{
			int32 A = INDEX_NONE;
			int32 B = INDEX_NONE;
			int32 C = INDEX_NONE;
		};

		struct FBoundaryEdge
		{
			int32 A = INDEX_NONE;
			int32 B = INDEX_NONE;
		};

		struct FProjectionCandidate
		{
			int32 PlateId = INDEX_NONE;
			int32 LocalTriangleId = INDEX_NONE;
			bool bBoundaryDegenerate = false;
		};

		static int32 CountBits64(uint64 Value)
		{
			int32 Count = 0;
			while (Value != 0)
			{
				Value &= (Value - 1);
				++Count;
			}
			return Count;
		}

		static int32 LowestSetBitIndex(const uint64 Value)
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

		static uint64 MakeEdgeKey(const int32 A, const int32 B)
		{
			const uint32 MinIndex = static_cast<uint32>(FMath::Min(A, B));
			const uint32 MaxIndex = static_cast<uint32>(FMath::Max(A, B));
			return (static_cast<uint64>(MinIndex) << 32) | static_cast<uint64>(MaxIndex);
		}

		static void HashMix(uint64& Hash, const uint64 Value)
		{
			Hash ^= Value;
			Hash *= 1099511628211ull;
		}

		static void HashMixDouble(uint64& Hash, const double Value)
		{
			HashMix(Hash, static_cast<uint64>(FMath::RoundToInt64(Value * 1000000000.0)));
		}

		static bool IsFiniteUnit(const FVector3d& P)
		{
			return FMath::IsFinite(P.X) && FMath::IsFinite(P.Y) && FMath::IsFinite(P.Z) &&
				FMath::IsNearlyEqual(P.SizeSquared(), 1.0, 1.0e-8);
		}

		static int32 FindNearestPlateCenter(const TArray<FVector3d>& Centers, const FVector3d& UnitPosition)
		{
			int32 BestPlateId = INDEX_NONE;
			double BestDot = -TNumericLimits<double>::Max();
			for (int32 PlateId = 0; PlateId < Centers.Num(); ++PlateId)
			{
				const double Dot = FVector3d::DotProduct(UnitPosition, Centers[PlateId]);
				if (Dot > BestDot || (FMath::IsNearlyEqual(Dot, BestDot) && (BestPlateId == INDEX_NONE || PlateId < BestPlateId)))
				{
					BestDot = Dot;
					BestPlateId = PlateId;
				}
			}
			return BestPlateId;
		}

		static int32 FindOrAddLocalVertex(FCarrierPlate& Plate, const FSphereSample& Sample)
		{
			if (const int32* Existing = Plate.GlobalSampleIdToLocalVertexId.Find(Sample.Id))
			{
				return *Existing;
			}

			FCarrierVertex Vertex;
			Vertex.GlobalSampleId = Sample.Id;
			Vertex.UnitPosition = Sample.UnitPosition;
			Vertex.AreaWeight = Sample.AreaWeight;
			Vertex.ContinentalFraction = Sample.ContinentalFraction;
			Vertex.Elevation = Sample.Elevation;
			Vertex.OceanicAge = Sample.OceanicAge;
			Vertex.RidgeDirection = Sample.RidgeDirection;
			Vertex.FoldDirection = Sample.FoldDirection;
			Vertex.bContinental = Sample.bContinental;
			const int32 LocalVertexId = Plate.Vertices.Add(Vertex);
			Plate.GlobalSampleIdToLocalVertexId.Add(Sample.Id, LocalVertexId);
			return LocalVertexId;
		}

		static double OrientedVolume(const TArray<FSphereSample>& Samples, const int32 A, const int32 B, const int32 C)
		{
			return FVector3d::DotProduct(
				Samples[A].UnitPosition,
				FVector3d::CrossProduct(Samples[B].UnitPosition, Samples[C].UnitPosition));
		}

		static bool OrientTriangle(const TArray<FSphereSample>& Samples, FWorkingTriangle& Triangle)
		{
			if (Triangle.A == Triangle.B || Triangle.B == Triangle.C || Triangle.C == Triangle.A)
			{
				return false;
			}

			const double Volume = OrientedVolume(Samples, Triangle.A, Triangle.B, Triangle.C);
			if (FMath::Abs(Volume) <= GeometryEpsilon)
			{
				return false;
			}

			if (Volume < 0.0)
			{
				Swap(Triangle.B, Triangle.C);
			}
			return true;
		}

		static bool ComputeSphericalCircumcircle(
			const TArray<FSphereSample>& Samples,
			const FWorkingTriangle& Triangle,
			FVector3d& OutCenter,
			double& OutCosRadius)
		{
			const FVector3d& A = Samples[Triangle.A].UnitPosition;
			const FVector3d& B = Samples[Triangle.B].UnitPosition;
			const FVector3d& C = Samples[Triangle.C].UnitPosition;

			FVector3d Center = FVector3d::CrossProduct(B - A, C - A);
			const double Size = Center.Size();
			if (Size <= GeometryEpsilon)
			{
				return false;
			}

			Center /= Size;
			if (FVector3d::DotProduct(Center, A + B + C) < 0.0)
			{
				Center *= -1.0;
			}

			OutCenter = Center;
			OutCosRadius = FVector3d::DotProduct(Center, A);
			return FMath::IsFinite(OutCosRadius);
		}

		static bool CircumcircleContains(
			const TArray<FSphereSample>& Samples,
			const FWorkingTriangle& Triangle,
			const FVector3d& Point)
		{
			FVector3d Center;
			double CosRadius = 0.0;
			if (!ComputeSphericalCircumcircle(Samples, Triangle, Center, CosRadius))
			{
				return false;
			}

			return FVector3d::DotProduct(Center, Point) > CosRadius + 1.0e-11;
		}

		static bool PointInSphericalTriangle(
			const TArray<FSphereSample>& Samples,
			const FWorkingTriangle& Triangle,
			const FVector3d& Point)
		{
			const FVector3d& A = Samples[Triangle.A].UnitPosition;
			const FVector3d& B = Samples[Triangle.B].UnitPosition;
			const FVector3d& C = Samples[Triangle.C].UnitPosition;

			const double SAB = FVector3d::DotProduct(FVector3d::CrossProduct(A, B), C);
			const double SBC = FVector3d::DotProduct(FVector3d::CrossProduct(B, C), A);
			const double SCA = FVector3d::DotProduct(FVector3d::CrossProduct(C, A), B);
			if (FMath::Abs(SAB) <= GeometryEpsilon ||
				FMath::Abs(SBC) <= GeometryEpsilon ||
				FMath::Abs(SCA) <= GeometryEpsilon)
			{
				return false;
			}

			const double E0 = FVector3d::DotProduct(FVector3d::CrossProduct(A, B), Point) * FMath::Sign(SAB);
			const double E1 = FVector3d::DotProduct(FVector3d::CrossProduct(B, C), Point) * FMath::Sign(SBC);
			const double E2 = FVector3d::DotProduct(FVector3d::CrossProduct(C, A), Point) * FMath::Sign(SCA);
			return E0 >= -1.0e-10 && E1 >= -1.0e-10 && E2 >= -1.0e-10;
		}

		static int32 FindExtremeIndex(const TArray<FSphereSample>& Samples, const int32 Axis, const bool bMax)
		{
			int32 BestIndex = INDEX_NONE;
			double BestValue = bMax ? -TNumericLimits<double>::Max() : TNumericLimits<double>::Max();
			for (const FSphereSample& Sample : Samples)
			{
				const double Value = Axis == 0 ? Sample.UnitPosition.X : (Axis == 1 ? Sample.UnitPosition.Y : Sample.UnitPosition.Z);
				if ((bMax && Value > BestValue) || (!bMax && Value < BestValue))
				{
					BestValue = Value;
					BestIndex = Sample.Id;
				}
			}
			return BestIndex;
		}

		static void AddUniqueSeed(TArray<int32>& Seeds, const int32 Index)
		{
			if (Index != INDEX_NONE)
			{
				Seeds.AddUnique(Index);
			}
		}

		static bool AddTriangleChecked(
			const TArray<FSphereSample>& Samples,
			TArray<FWorkingTriangle>& Triangles,
			int32 A,
			int32 B,
			int32 C)
		{
			FWorkingTriangle Triangle{A, B, C};
			if (!OrientTriangle(Samples, Triangle))
			{
				return false;
			}
			Triangles.Add(Triangle);
			return true;
		}
	}

	void FCarrierLabStage0::GenerateFibonacciSamples(const int32 SampleCount, TArray<FSphereSample>& OutSamples)
	{
		OutSamples.Reset(SampleCount);
		const double AreaWeight = 4.0 * Pi / static_cast<double>(SampleCount);
		for (int32 Index = 0; Index < SampleCount; ++Index)
		{
			const double Z = 1.0 - (2.0 * (static_cast<double>(Index) + 0.5) / static_cast<double>(SampleCount));
			const double Radius = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
			const double Theta = GoldenAngle * static_cast<double>(Index);

			FSphereSample Sample;
			Sample.Id = Index;
			Sample.UnitPosition = FVector3d(FMath::Cos(Theta) * Radius, FMath::Sin(Theta) * Radius, Z).GetSafeNormal();
			Sample.AreaWeight = AreaWeight;
			OutSamples.Add(Sample);
		}
	}

	void FCarrierLabStage0::GeneratePlateCenters(const FStage0Config& Config, TArray<FVector3d>& OutCenters)
	{
		OutCenters.Reset(Config.PlateCount);
		const double SeedPhase = FMath::Frac(static_cast<double>(Config.Seed) * 0.6180339887498948482);
		for (int32 Index = 0; Index < Config.PlateCount; ++Index)
		{
			const double Z = 1.0 - (2.0 * (static_cast<double>(Index) + 0.5) / static_cast<double>(Config.PlateCount));
			const double Radius = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
			const double Theta = GoldenAngle * (static_cast<double>(Index) + SeedPhase);
			OutCenters.Add(FVector3d(FMath::Cos(Theta) * Radius, FMath::Sin(Theta) * Radius, Z).GetSafeNormal());
		}
	}

	bool FCarrierLabStage0::BuildSphericalDelaunayConvexHull(
		const TArray<FSphereSample>& Samples,
		TArray<FSphereTriangle>& OutTriangles,
		FString& OutError)
	{
		OutTriangles.Reset();
		if (Samples.Num() < 4)
		{
			OutError = TEXT("Convex-hull spherical Delaunay needs at least four samples.");
			return false;
		}

		TArray<FVector3d> Points;
		Points.Reserve(Samples.Num());
		for (const FSphereSample& Sample : Samples)
		{
			Points.Add(Sample.UnitPosition);
		}

		UE::Geometry::FConvexHull3d Hull;
		Hull.bSaveTriangleNeighbors = false;
		if (!Hull.Solve(TArrayView<const FVector3d>(Points.GetData(), Points.Num())) || !Hull.IsSolutionAvailable())
		{
			OutError = FString::Printf(
				TEXT("FConvexHull3d failed to produce a 3D hull; reported dimension %d."),
				Hull.GetDimension());
			return false;
		}

		if (Hull.GetNumHullPoints() != Samples.Num())
		{
			OutError = FString::Printf(
				TEXT("FConvexHull3d used %d hull vertices for %d sphere samples."),
				Hull.GetNumHullPoints(),
				Samples.Num());
			return false;
		}

		TSet<uint64> TriangleKeys;
		const TArray<UE::Geometry::FIndex3i>& HullTriangles = Hull.GetTriangles();
		OutTriangles.Reserve(HullTriangles.Num());
		for (const UE::Geometry::FIndex3i& HullTriangle : HullTriangles)
		{
			FWorkingTriangle Triangle{HullTriangle.A, HullTriangle.B, HullTriangle.C};
			if (!OrientTriangle(Samples, Triangle))
			{
				continue;
			}
			TArray<int32> Sorted = {Triangle.A, Triangle.B, Triangle.C};
			Sorted.Sort();
			uint64 Key = 1469598103934665603ull;
			HashMix(Key, static_cast<uint64>(Sorted[0]));
			HashMix(Key, static_cast<uint64>(Sorted[1]));
			HashMix(Key, static_cast<uint64>(Sorted[2]));
			if (TriangleKeys.Contains(Key))
			{
				continue;
			}
			TriangleKeys.Add(Key);

			FSphereTriangle FinalTriangle;
			FinalTriangle.A = Triangle.A;
			FinalTriangle.B = Triangle.B;
			FinalTriangle.C = Triangle.C;
			OutTriangles.Add(FinalTriangle);
		}

		return !OutTriangles.IsEmpty();
	}

	void FCarrierLabStage0::AssignPlatesAndTriangles(FCarrierState& State)
	{
		TArray<FVector3d> Centers;
		GeneratePlateCenters(State.Config, Centers);

		State.Plates.Reset(State.Config.PlateCount);
		const int32 ContinentalPlateCount = FMath::Clamp(
			FMath::RoundToInt(static_cast<double>(State.Config.PlateCount) * State.Config.ContinentalPlateFraction),
			0,
			State.Config.PlateCount);
		for (int32 PlateId = 0; PlateId < State.Config.PlateCount; ++PlateId)
		{
			FCarrierPlate Plate;
			Plate.PlateId = PlateId;
			Plate.InitialCenter = Centers[PlateId];
			Plate.bContinental = PlateId < ContinentalPlateCount;
			State.Plates.Add(Plate);
		}

		for (FSphereSample& Sample : State.Samples)
		{
			const int32 BestPlateId = FindNearestPlateCenter(Centers, Sample.UnitPosition);
			Sample.PlateId = BestPlateId;
			Sample.ContinentalFraction = State.Plates[BestPlateId].bContinental ? 1.0 : 0.0;
			Sample.bContinental = Sample.ContinentalFraction >= 0.5;
			State.Plates[BestPlateId].SampleIds.Add(Sample.Id);
		}

		for (int32 TriangleIndex = 0; TriangleIndex < State.Triangles.Num(); ++TriangleIndex)
		{
			FSphereTriangle& Triangle = State.Triangles[TriangleIndex];
			const int32 PlateA = State.Samples[Triangle.A].PlateId;
			const int32 PlateB = State.Samples[Triangle.B].PlateId;
			const int32 PlateC = State.Samples[Triangle.C].PlateId;
			Triangle.bBoundary = PlateA != PlateB || PlateB != PlateC;

			if (PlateA == PlateB || PlateA == PlateC)
			{
				Triangle.PlateId = PlateA;
			}
			else if (PlateB == PlateC)
			{
				Triangle.PlateId = PlateB;
			}
			else
			{
				const FVector3d TriangleCenter = (
					State.Samples[Triangle.A].UnitPosition +
					State.Samples[Triangle.B].UnitPosition +
					State.Samples[Triangle.C].UnitPosition).GetSafeNormal();
				Triangle.PlateId = FindNearestPlateCenter(Centers, TriangleCenter);
			}

			State.Plates[Triangle.PlateId].TriangleIds.Add(TriangleIndex);
		}
	}

	void FCarrierLabStage0::BuildAdjacency(FCarrierState& State)
	{
		State.SampleIncidentTriangleIds.SetNum(State.Samples.Num());
		State.SampleNeighborIds.SetNum(State.Samples.Num());

		for (int32 TriangleIndex = 0; TriangleIndex < State.Triangles.Num(); ++TriangleIndex)
		{
			const FSphereTriangle& Triangle = State.Triangles[TriangleIndex];
			const int32 Verts[3] = {Triangle.A, Triangle.B, Triangle.C};
			for (int32 LocalIndex = 0; LocalIndex < 3; ++LocalIndex)
			{
				const int32 A = Verts[LocalIndex];
				const int32 B = Verts[(LocalIndex + 1) % 3];
				const int32 C = Verts[(LocalIndex + 2) % 3];
				State.SampleIncidentTriangleIds[A].Add(TriangleIndex);
				State.SampleNeighborIds[A].AddUnique(B);
				State.SampleNeighborIds[A].AddUnique(C);
			}
		}

		for (int32 SampleIndex = 0; SampleIndex < State.SampleNeighborIds.Num(); ++SampleIndex)
		{
			State.SampleNeighborIds[SampleIndex].Sort();
		}
	}

	void FCarrierLabStage0::BuildPlateLocalTriangulations(FCarrierState& State)
	{
		State.SampleRayCandidateTriangles.Reset();
		State.SampleRayCandidateTriangles.SetNum(State.Samples.Num());
		State.ConvergenceSubductionMatrixPairKeys.Reset();
		State.ConvergenceSubductionPolarityDecisions.Reset();
		State.ConvergenceSubductionTriangleHits.Reset();
		State.ConvergenceSubductionMatrixEvidence.Reset();
		State.ConvergenceSubductingTriangleMarks.Reset();
		State.ConvergenceTrackingDistanceCullCount = 0;
		State.ConvergenceSubductionMatrixRayTestCount = 0;
		State.ConvergenceSubductionMatrixHitCount = 0;
		State.ConvergenceSubductionMatrixBoundaryHitCount = 0;
		State.ConvergenceSubductionMatrixNonConvergentHitCount = 0;
		State.ConvergenceNeighborPropagationSeedCount = 0;
		State.ConvergenceNeighborPropagationAddedCount = 0;
		State.ConvergenceNeighborPropagationDuplicateCount = 0;
		State.ConvergenceNeighborPropagationDistanceRejectedCount = 0;
		State.ConvergenceNeighborPropagationInvalidCount = 0;
		State.ConvergenceSubductingTriangleMarkDuplicateCount = 0;
		State.ConvergenceSubductingTriangleMarkInvalidCount = 0;

		for (FCarrierPlate& Plate : State.Plates)
		{
			Plate.Vertices.Reset();
			Plate.LocalTriangles.Reset();
			Plate.ActiveBoundaryTriangles.Reset();
			Plate.ActiveBoundaryTriangleDistancesKm.Reset();
			Plate.GlobalSampleIdToLocalVertexId.Reset();
		}

		for (int32 TriangleIndex = 0; TriangleIndex < State.Triangles.Num(); ++TriangleIndex)
		{
			const FSphereTriangle& Triangle = State.Triangles[TriangleIndex];
			if (!State.Plates.IsValidIndex(Triangle.PlateId))
			{
				continue;
			}

			FCarrierPlate& Plate = State.Plates[Triangle.PlateId];
			FCarrierPlateTriangle LocalTriangle;
			LocalTriangle.A = FindOrAddLocalVertex(Plate, State.Samples[Triangle.A]);
			LocalTriangle.B = FindOrAddLocalVertex(Plate, State.Samples[Triangle.B]);
			LocalTriangle.C = FindOrAddLocalVertex(Plate, State.Samples[Triangle.C]);
			LocalTriangle.SourceTriangleId = TriangleIndex;
			LocalTriangle.bBoundary = Triangle.bBoundary;
			const int32 LocalTriangleId = Plate.LocalTriangles.Add(LocalTriangle);
			if (LocalTriangle.bBoundary)
			{
				Plate.ActiveBoundaryTriangles.Add(LocalTriangleId);
				Plate.ActiveBoundaryTriangleDistancesKm.Add(0.0);
			}

			const int32 SourceVerts[3] = {Triangle.A, Triangle.B, Triangle.C};
			for (const int32 SourceVertexId : SourceVerts)
			{
				if (State.SampleRayCandidateTriangles.IsValidIndex(SourceVertexId))
				{
					State.SampleRayCandidateTriangles[SourceVertexId].Add(FCarrierRayTriangleRef{Plate.PlateId, LocalTriangleId});
				}
			}
		}
	}

	bool FCarrierLabStage0::IntersectRayWithPlateTriangle(
		const FVector3d& RayDirection,
		const FVector3d& A,
		const FVector3d& B,
		const FVector3d& C,
		FVector3d& OutBarycentric,
		bool& bOutBoundaryDegenerate)
	{
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
		const double BarycentricDenominator = D00 * D11 - D01 * D01;
		if (FMath::Abs(BarycentricDenominator) <= GeometryEpsilon)
		{
			return false;
		}

		const double V = (D11 * D20 - D01 * D21) / BarycentricDenominator;
		const double W = (D00 * D21 - D01 * D20) / BarycentricDenominator;
		const double U = 1.0 - V - W;
		constexpr double BarycentricEpsilon = 1.0e-10;
		if (U < -BarycentricEpsilon || V < -BarycentricEpsilon || W < -BarycentricEpsilon)
		{
			return false;
		}

		OutBarycentric = FVector3d(U, V, W);
		bOutBoundaryDegenerate =
			U <= 1.0e-8 || V <= 1.0e-8 || W <= 1.0e-8 ||
			U >= 1.0 - 1.0e-8 || V >= 1.0 - 1.0e-8 || W >= 1.0 - 1.0e-8;
		return true;
	}

	bool FCarrierLabStage0::BuildColdStartCarrier(const FStage0Config& Config, FCarrierState& OutState, FString& OutError)
	{
		if (Config.SampleCount < 16)
		{
			OutError = TEXT("SampleCount must be at least 16.");
			return false;
		}
		if (Config.PlateCount <= 0 || Config.PlateCount > Config.SampleCount)
		{
			OutError = TEXT("PlateCount must be positive and no greater than SampleCount.");
			return false;
		}

		FCarrierState State;
		State.Config = Config;
		GenerateFibonacciSamples(Config.SampleCount, State.Samples);

		if (!BuildSphericalDelaunayConvexHull(State.Samples, State.Triangles, OutError))
		{
			return false;
		}

		AssignPlatesAndTriangles(State);
		BuildAdjacency(State);
		BuildPlateLocalTriangulations(State);
		OutState = MoveTemp(State);
		return true;
	}

	void FCarrierLabStage0::RebuildPlateLocalStateFromSamples(FCarrierState& State)
	{
		for (FCarrierPlate& Plate : State.Plates)
		{
			Plate.SampleIds.Reset();
			Plate.TriangleIds.Reset();
			Plate.Vertices.Reset();
			Plate.LocalTriangles.Reset();
			Plate.ActiveBoundaryTriangles.Reset();
			Plate.ActiveBoundaryTriangleDistancesKm.Reset();
			Plate.GlobalSampleIdToLocalVertexId.Reset();
		}

		for (FSphereSample& Sample : State.Samples)
		{
			Sample.bContinental = Sample.ContinentalFraction >= 0.5;
			if (!State.Plates.IsValidIndex(Sample.PlateId))
			{
				Sample.PlateId = 0;
			}
			State.Plates[Sample.PlateId].SampleIds.Add(Sample.Id);
		}

		for (int32 TriangleIndex = 0; TriangleIndex < State.Triangles.Num(); ++TriangleIndex)
		{
			FSphereTriangle& Triangle = State.Triangles[TriangleIndex];
			const int32 PlateA = State.Samples.IsValidIndex(Triangle.A) ? State.Samples[Triangle.A].PlateId : INDEX_NONE;
			const int32 PlateB = State.Samples.IsValidIndex(Triangle.B) ? State.Samples[Triangle.B].PlateId : INDEX_NONE;
			const int32 PlateC = State.Samples.IsValidIndex(Triangle.C) ? State.Samples[Triangle.C].PlateId : INDEX_NONE;
			Triangle.bBoundary = PlateA != PlateB || PlateB != PlateC;

			if (PlateA == PlateB || PlateA == PlateC)
			{
				Triangle.PlateId = PlateA;
			}
			else if (PlateB == PlateC)
			{
				Triangle.PlateId = PlateB;
			}
			else
			{
				const FVector3d TriangleCenter = (
					State.Samples[Triangle.A].UnitPosition +
					State.Samples[Triangle.B].UnitPosition +
					State.Samples[Triangle.C].UnitPosition).GetSafeNormal();
				int32 BestPlateId = INDEX_NONE;
				double BestDot = -TNumericLimits<double>::Max();
				for (const FCarrierPlate& Plate : State.Plates)
				{
					const double Dot = FVector3d::DotProduct(TriangleCenter, Plate.InitialCenter);
					if (Dot > BestDot || (FMath::IsNearlyEqual(Dot, BestDot) && (BestPlateId == INDEX_NONE || Plate.PlateId < BestPlateId)))
					{
						BestDot = Dot;
						BestPlateId = Plate.PlateId;
					}
				}
				Triangle.PlateId = BestPlateId;
			}

			if (State.Plates.IsValidIndex(Triangle.PlateId))
			{
				State.Plates[Triangle.PlateId].TriangleIds.Add(TriangleIndex);
			}
		}

		BuildPlateLocalTriangulations(State);
		++State.ConvergenceTrackingResetSerial;
	}

	FStage0Metrics FCarrierLabStage0::ProjectColdStart(const FCarrierState& State, FStage0ProjectionBuffers* OutProjectionBuffers)
	{
		const double StartTime = FPlatformTime::Seconds();
		FStage0Metrics Metrics;
		Metrics.SampleCount = State.Samples.Num();
		Metrics.PlateCount = State.Plates.Num();
		Metrics.TriangleCount = State.Triangles.Num();
		for (const FCarrierPlate& Plate : State.Plates)
		{
			Metrics.PlateLocalVertexCount += Plate.Vertices.Num();
			Metrics.PlateLocalTriangleCount += Plate.LocalTriangles.Num();
		}

		TSet<uint64> Edges;
		for (const FSphereTriangle& Triangle : State.Triangles)
		{
			Edges.Add(MakeEdgeKey(Triangle.A, Triangle.B));
			Edges.Add(MakeEdgeKey(Triangle.B, Triangle.C));
			Edges.Add(MakeEdgeKey(Triangle.C, Triangle.A));
			if (Triangle.bBoundary)
			{
				++Metrics.BoundaryTriangleCount;
			}
		}
		Metrics.EdgeCount = Edges.Num();
		Metrics.EulerCharacteristic = Metrics.SampleCount - Metrics.EdgeCount + Metrics.TriangleCount;

		if (OutProjectionBuffers != nullptr)
		{
			OutProjectionBuffers->ResolvedPlateIds.Init(INDEX_NONE, State.Samples.Num());
			OutProjectionBuffers->MissClasses.Init(static_cast<uint8>(EStage0MissClass::None), State.Samples.Num());
			OutProjectionBuffers->OverlapClasses.Init(static_cast<uint8>(EStage0OverlapClass::None), State.Samples.Num());
			OutProjectionBuffers->BoundaryMask.Init(0, State.Samples.Num());
			OutProjectionBuffers->ContinentalMask.Init(0, State.Samples.Num());
		}

		double AuthoritativeContinentalArea = 0.0;
		double ProjectedContinentalArea = 0.0;
		double TotalArea = 0.0;
		for (const FSphereSample& Sample : State.Samples)
		{
			if (!IsFiniteUnit(Sample.UnitPosition))
			{
				++Metrics.NaNOrInfCount;
			}

			TotalArea += Sample.AreaWeight;
			if (Sample.ContinentalFraction > 0.0)
			{
				AuthoritativeContinentalArea += Sample.AreaWeight * Sample.ContinentalFraction;
			}

			TArray<FProjectionCandidate> Candidates;
			if (State.SampleRayCandidateTriangles.IsValidIndex(Sample.Id))
			{
				const TArray<FCarrierRayTriangleRef>& CandidateRefs = State.SampleRayCandidateTriangles[Sample.Id];
				Metrics.RayCandidateCount += CandidateRefs.Num();
				for (const FCarrierRayTriangleRef& CandidateRef : CandidateRefs)
				{
					if (!State.Plates.IsValidIndex(CandidateRef.PlateId))
					{
						continue;
					}

					const FCarrierPlate& Plate = State.Plates[CandidateRef.PlateId];
					if (!Plate.LocalTriangles.IsValidIndex(CandidateRef.LocalTriangleId))
					{
						continue;
					}

					const FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[CandidateRef.LocalTriangleId];
					if (!Plate.Vertices.IsValidIndex(Triangle.A) ||
						!Plate.Vertices.IsValidIndex(Triangle.B) ||
						!Plate.Vertices.IsValidIndex(Triangle.C))
					{
						continue;
					}

					FVector3d Barycentric;
					bool bBoundaryDegenerate = false;
					++Metrics.RayTriangleTestCount;
					if (IntersectRayWithPlateTriangle(
						Sample.UnitPosition,
						Plate.Vertices[Triangle.A].UnitPosition,
						Plate.Vertices[Triangle.B].UnitPosition,
						Plate.Vertices[Triangle.C].UnitPosition,
						Barycentric,
						bBoundaryDegenerate))
					{
						Candidates.Add(FProjectionCandidate{Plate.PlateId, CandidateRef.LocalTriangleId, bBoundaryDegenerate});
					}
				}
			}

			if (Candidates.IsEmpty())
			{
				++Metrics.RawMissCount;
				++Metrics.NonDegenerateMissCount;
				if (OutProjectionBuffers != nullptr && OutProjectionBuffers->MissClasses.IsValidIndex(Sample.Id))
				{
					const bool bHadCandidateRefs =
						State.SampleRayCandidateTriangles.IsValidIndex(Sample.Id) &&
						!State.SampleRayCandidateTriangles[Sample.Id].IsEmpty();
					OutProjectionBuffers->MissClasses[Sample.Id] = static_cast<uint8>(
						bHadCandidateRefs ? EStage0MissClass::NumericMiss : EStage0MissClass::TopologyHole);
				}
				continue;
			}

			++Metrics.RawHitCount;
			uint64 CandidatePlateMask = 0;
			bool bAllCandidatesBoundaryDegenerate = true;
			bool bAnyCandidateBoundaryDegenerate = false;
			for (const FProjectionCandidate& Candidate : Candidates)
			{
				if (Candidate.PlateId >= 0 && Candidate.PlateId < 64)
				{
					CandidatePlateMask |= (1ull << Candidate.PlateId);
				}
				bAllCandidatesBoundaryDegenerate = bAllCandidatesBoundaryDegenerate && Candidate.bBoundaryDegenerate;
				bAnyCandidateBoundaryDegenerate = bAnyCandidateBoundaryDegenerate || Candidate.bBoundaryDegenerate;
			}

			const int32 CandidatePlateCount = CountBits64(CandidatePlateMask);
			if (CandidatePlateCount > 1)
			{
				++Metrics.RawMultiHitCount;
				if (CandidatePlateCount > 2)
				{
					++Metrics.ThirdPlateIntrusionCount;
					if (bAllCandidatesBoundaryDegenerate)
					{
						++Metrics.ThirdPlateBoundaryDegenerateCount;
						if (OutProjectionBuffers != nullptr && OutProjectionBuffers->OverlapClasses.IsValidIndex(Sample.Id))
						{
							OutProjectionBuffers->OverlapClasses[Sample.Id] = static_cast<uint8>(EStage0OverlapClass::ThirdPlateBoundary);
						}
					}
					else
					{
						++Metrics.ThirdPlateNonDegenerateCount;
						if (OutProjectionBuffers != nullptr && OutProjectionBuffers->OverlapClasses.IsValidIndex(Sample.Id))
						{
							OutProjectionBuffers->OverlapClasses[Sample.Id] = static_cast<uint8>(EStage0OverlapClass::ThirdPlateNonDegenerate);
						}
					}
				}
				else if (bAllCandidatesBoundaryDegenerate)
				{
					++Metrics.BoundaryDegenerateOverlapCount;
					if (OutProjectionBuffers != nullptr && OutProjectionBuffers->OverlapClasses.IsValidIndex(Sample.Id))
					{
						OutProjectionBuffers->OverlapClasses[Sample.Id] = static_cast<uint8>(EStage0OverlapClass::BoundaryDegenerate);
					}
				}
				else
				{
					++Metrics.NonDegenerateOverlapCount;
					if (OutProjectionBuffers != nullptr && OutProjectionBuffers->OverlapClasses.IsValidIndex(Sample.Id))
					{
						OutProjectionBuffers->OverlapClasses[Sample.Id] = static_cast<uint8>(EStage0OverlapClass::NonDegenerate);
					}
				}
			}

			if (OutProjectionBuffers != nullptr && OutProjectionBuffers->BoundaryMask.IsValidIndex(Sample.Id))
			{
				OutProjectionBuffers->BoundaryMask[Sample.Id] = bAnyCandidateBoundaryDegenerate ? 1 : 0;
			}

			int32 ResolvedPlateId = LowestSetBitIndex(CandidatePlateMask);
			if (Sample.PlateId >= 0 && Sample.PlateId < 64 && (CandidatePlateMask & (1ull << Sample.PlateId)) != 0)
			{
				ResolvedPlateId = Sample.PlateId;
			}

			if (OutProjectionBuffers != nullptr && OutProjectionBuffers->ResolvedPlateIds.IsValidIndex(Sample.Id))
			{
				OutProjectionBuffers->ResolvedPlateIds[Sample.Id] = ResolvedPlateId;
			}

			if (State.Plates.IsValidIndex(ResolvedPlateId) && State.Plates[ResolvedPlateId].bContinental)
			{
				ProjectedContinentalArea += Sample.AreaWeight;
				if (OutProjectionBuffers != nullptr && OutProjectionBuffers->ContinentalMask.IsValidIndex(Sample.Id))
				{
					OutProjectionBuffers->ContinentalMask[Sample.Id] = 1;
				}
			}
		}

		Metrics.TotalMaterialArea = TotalArea;
		Metrics.AuthoritativeContinentalAreaFraction = AuthoritativeContinentalArea / FMath::Max(TotalArea, UE_DOUBLE_SMALL_NUMBER);
		Metrics.ProjectedContinentalAreaFraction = ProjectedContinentalArea / FMath::Max(TotalArea, UE_DOUBLE_SMALL_NUMBER);
		Metrics.WallClockSeconds = FPlatformTime::Seconds() - StartTime;
		Metrics.MemoryUsedBytes = FPlatformMemory::GetStats().UsedPhysical;
		Metrics.DeterminismHash = HashStateAndMetrics(State, Metrics);
		return Metrics;
	}

	uint64 FCarrierLabStage0::ComputeTopologyHash(const FCarrierState& State, const FStage0Metrics& Metrics)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMix(Hash, static_cast<uint64>(State.Config.SampleCount));
		HashMix(Hash, static_cast<uint64>(State.Config.PlateCount));
		HashMix(Hash, static_cast<uint64>(State.Config.Seed));
		HashMix(Hash, static_cast<uint64>(Metrics.TriangleCount));
		HashMix(Hash, static_cast<uint64>(Metrics.PlateLocalVertexCount));
		HashMix(Hash, static_cast<uint64>(Metrics.PlateLocalTriangleCount));
		HashMix(Hash, static_cast<uint64>(Metrics.EdgeCount));
		HashMix(Hash, static_cast<uint64>(Metrics.EulerCharacteristic));
		HashMix(Hash, static_cast<uint64>(Metrics.RawMissCount));
		HashMix(Hash, static_cast<uint64>(Metrics.RawMultiHitCount));
		HashMix(Hash, static_cast<uint64>(Metrics.BoundaryDegenerateOverlapCount));
		HashMix(Hash, static_cast<uint64>(Metrics.NonDegenerateOverlapCount));
		HashMix(Hash, static_cast<uint64>(Metrics.ThirdPlateIntrusionCount));
		HashMix(Hash, static_cast<uint64>(Metrics.ThirdPlateBoundaryDegenerateCount));
		HashMix(Hash, static_cast<uint64>(Metrics.ThirdPlateNonDegenerateCount));
		HashMix(Hash, static_cast<uint64>(Metrics.RayCandidateCount));
		HashMix(Hash, static_cast<uint64>(Metrics.RayTriangleTestCount));
		HashMixDouble(Hash, Metrics.AuthoritativeContinentalAreaFraction);
		HashMixDouble(Hash, Metrics.ProjectedContinentalAreaFraction);

		for (const FSphereSample& Sample : State.Samples)
		{
			HashMix(Hash, static_cast<uint64>(Sample.PlateId + 1));
			HashMixDouble(Hash, Sample.ContinentalFraction);
			HashMix(Hash, Sample.bContinental ? 1ull : 0ull);
		}
		for (const FSphereTriangle& Triangle : State.Triangles)
		{
			HashMix(Hash, static_cast<uint64>(Triangle.A + 1));
			HashMix(Hash, static_cast<uint64>(Triangle.B + 1));
			HashMix(Hash, static_cast<uint64>(Triangle.C + 1));
			HashMix(Hash, static_cast<uint64>(Triangle.PlateId + 1));
		}
		for (const FCarrierPlate& Plate : State.Plates)
		{
			HashMix(Hash, static_cast<uint64>(Plate.PlateId + 1));
			HashMix(Hash, Plate.bContinental ? 1ull : 0ull);
			HashMixDouble(Hash, Plate.InitialCenter.X);
			HashMixDouble(Hash, Plate.InitialCenter.Y);
			HashMixDouble(Hash, Plate.InitialCenter.Z);
			HashMix(Hash, static_cast<uint64>(Plate.Vertices.Num()));
			HashMix(Hash, static_cast<uint64>(Plate.LocalTriangles.Num()));
			for (const FCarrierVertex& Vertex : Plate.Vertices)
			{
				HashMix(Hash, static_cast<uint64>(Vertex.GlobalSampleId + 1));
				HashMixDouble(Hash, Vertex.UnitPosition.X);
				HashMixDouble(Hash, Vertex.UnitPosition.Y);
				HashMixDouble(Hash, Vertex.UnitPosition.Z);
				HashMixDouble(Hash, Vertex.ContinentalFraction);
				HashMix(Hash, Vertex.bContinental ? 1ull : 0ull);
			}
			for (const FCarrierPlateTriangle& Triangle : Plate.LocalTriangles)
			{
				HashMix(Hash, static_cast<uint64>(Triangle.A + 1));
				HashMix(Hash, static_cast<uint64>(Triangle.B + 1));
				HashMix(Hash, static_cast<uint64>(Triangle.C + 1));
				HashMix(Hash, static_cast<uint64>(Triangle.SourceTriangleId + 1));
				HashMix(Hash, Triangle.bBoundary ? 1ull : 0ull);
			}
		}
		return Hash;
	}

	FString FCarrierLabStage0::HashStateAndMetrics(const FCarrierState& State, const FStage0Metrics& Metrics)
	{
		return FString::Printf(TEXT("%016llx"), ComputeTopologyHash(State, Metrics));
	}
}
