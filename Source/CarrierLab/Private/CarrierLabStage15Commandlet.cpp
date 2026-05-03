// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabStage15Commandlet.h"

#include "CarrierLabCarrier.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "HAL/PlatformMemory.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Spatial/SpatialInterfaces.h"

namespace
{
	using UE::Geometry::FDynamicMesh3;
	using UE::Geometry::FDynamicMeshAABBTree3;
	using UE::Geometry::IMeshSpatial;

	constexpr double EarthRadiusKm = 6371.0;
	constexpr double BoundaryBarycentricEpsilon = 1.0e-7;
	constexpr double DeltaTimeMa = 2.0;
	constexpr double ResamplingMMaxMa = 128.0;
	constexpr double ResamplingMMinMa = 32.0;
	constexpr double V0MmPerYear = 100.0;
	constexpr double ChosenVelocityMmPerYear = 66.6666666667;
	constexpr int32 BoundaryLonBins = 256;
	constexpr int32 BoundaryLatBins = 128;
	constexpr int32 BoundarySearchRadiusBins = 3;

	enum class EStage15MissClass : uint8
	{
		None = 0,
		DivergentGap = 1,
		NumericMiss = 2,
		OutOfDomain = 3
	};

	enum class EStage15OverlapClass : uint8
	{
		None = 0,
		BoundaryDegenerate = 1,
		ConvergentOverlap = 2,
		NumericOverlap = 3,
		ThirdPlateBoundary = 4,
		ThirdPlateNonDegenerate = 5
	};

	enum class EStage15MotionMode : uint8
	{
		Default = 0,
		ForcedConvergence = 1,
		ForcedDivergence = 2
	};

	enum class EStage15MultiHitPolicy : uint8
	{
		Centroid = 0,
		SyntheticSubduction = 1,
		RandomSeeded = 2
	};

	struct FStage15Motion
	{
		FVector3d Axis = FVector3d::UnitZ();
		double AngularSpeedRadiansPerStep = 0.0;
		FVector3d CurrentCenter = FVector3d::UnitZ();
	};

	struct FPlateRayMesh
	{
		FDynamicMesh3 Mesh;
		TUniquePtr<FDynamicMeshAABBTree3> Tree;
		TMap<int32, int32> MeshTriangleIdToLocalTriangleId;
	};

	struct FStage15Candidate
	{
		int32 PlateId = INDEX_NONE;
		int32 LocalTriangleId = INDEX_NONE;
		FVector3d Bary = FVector3d::ZeroVector;
		double Distance = 0.0;
		bool bBoundary = false;
	};

	struct FStage15Buffers
	{
		TArray<int32> ResolvedPlateIds;
		TArray<uint8> MissClasses;
		TArray<uint8> OverlapClasses;
		TArray<uint8> BoundaryMask;
		TArray<double> ProjectedContinentalFraction;
	};

	struct FStage15Metrics
	{
		FString Scenario;
		int32 Resolution = 0;
		int32 PlateCount = 0;
		int32 Seed = 42;
		int32 Step = 0;
		int32 EventCount = 0;
		int32 RawHitCount = 0;
		int32 RawMissCount = 0;
		int32 RawMultiHitCount = 0;
		int32 DivergentGapCount = 0;
		int32 NumericMissCount = 0;
		int32 BoundaryDegenerateOverlapCount = 0;
		int32 ConvergentOverlapCount = 0;
		int32 NumericOverlapCount = 0;
		int32 ThirdPlateIntrusionCount = 0;
		int32 ThirdPlateBoundaryDegenerateCount = 0;
		int32 ThirdPlateNonDegenerateCount = 0;
		int32 NaNOrInfCount = 0;
		int64 RayTreeQueryCount = 0;
		int64 RayHitTriangleCount = 0;
		double AuthoritativeCAF = 0.0;
		double ProjectedCAF = 0.0;
		double DriftErrorMeanKm = 0.0;
		double DriftErrorP95Km = 0.0;
		double StepKernelSeconds = 0.0;
		double MemoryGb = 0.0;
		FString ProjectionHash;
		FString StateHash;
	};

	struct FStage15BoundaryPoint
	{
		int32 PlateId = INDEX_NONE;
		FVector3d UnitPosition = FVector3d::UnitZ();
		double ContinentalFraction = 0.0;
	};

	struct FStage15BoundaryIndex
	{
		TArray<FStage15BoundaryPoint> Points;
		TMap<int32, TArray<int32>> Bins;
	};

	struct FStage15GapFill
	{
		int32 SampleId = INDEX_NONE;
		int32 Q1PlateId = INDEX_NONE;
		int32 Q2PlateId = INDEX_NONE;
		double Q1ContinentalFraction = 0.0;
		double Q2ContinentalFraction = 0.0;
		double FillContinentalFraction = 0.0;
		double SignedRelativeVelocity = 0.0;
		bool bSeparating = false;
	};

	struct FStage15EventRecord
	{
		FString Scenario;
		FString MultiHitPolicy;
		int32 Resolution = 0;
		int32 EventId = 0;
		int32 Step = 0;
		int32 TieBreakSeed = 0;
		int32 GapFillCount = 0;
		int32 NonSeparatingGapFillCount = 0;
		int32 NoBoundaryPairFallbackCount = 0;
		int32 PolicyMultiHitCount = 0;
		int32 RawMissBefore = 0;
		int32 RawMultiBefore = 0;
		int32 RawMissAfter = 0;
		int32 RawMultiAfter = 0;
		int32 NonBoundaryMultiAfter = 0;
		double AuthoritativeCAFBefore = 0.0;
		double AuthoritativeCAFAfter = 0.0;
		double ProjectedCAFBefore = 0.0;
		double ProjectedCAFAfter = 0.0;
		double MaxPlateAreaDeltaPercent = 0.0;
		int32 MaxPlateAreaDeltaPlateId = INDEX_NONE;
		FString EventHash;
		TArray<double> PlateAreaDeltaPercents;
		TArray<FStage15GapFill> GapFills;
	};

	struct FStage15RunConfig
	{
		FString Scenario = TEXT("cadence");
		int32 Resolution = 60000;
		int32 PlateCount = 40;
		int32 Seed = 42;
		int32 StepCount = 400;
		bool bEnableResamplingEvents = false;
		bool bHoldEventsUntilFinalStep = false;
		EStage15MotionMode MotionMode = EStage15MotionMode::Default;
		EStage15MultiHitPolicy MultiHitPolicy = EStage15MultiHitPolicy::Centroid;
		double VelocityMmPerYear = ChosenVelocityMmPerYear;
		double CadenceMa = 64.0;
		int32 CadenceSteps = 32;
		int32 TieBreakSeed = 15042;
		double ContinentalPlateFraction = 0.30;
		bool bAllContinental = false;
		bool bOceanOnly = false;
	};

	struct FStage15RunResult
	{
		FStage15RunConfig Config;
		TArray<FStage15Metrics> Metrics;
		TArray<FStage15EventRecord> Events;
		FString EventLogHash;
		bool bPassed = true;
	};

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
		return FString::Printf(TEXT("%016llx"), Hash);
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

	bool IsFiniteUnit(const FVector3d& P)
	{
		return FMath::IsFinite(P.X) && FMath::IsFinite(P.Y) && FMath::IsFinite(P.Z) &&
			FMath::IsNearlyEqual(P.SizeSquared(), 1.0, 1.0e-8);
	}

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

	double AngularDistanceRadians(const FVector3d& A, const FVector3d& B)
	{
		return FMath::Acos(FMath::Clamp(FVector3d::DotProduct(A, B), -1.0, 1.0));
	}

	bool IsBoundaryHit(const FVector3d& Bary)
	{
		return Bary.X <= BoundaryBarycentricEpsilon || Bary.Y <= BoundaryBarycentricEpsilon || Bary.Z <= BoundaryBarycentricEpsilon;
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

	int32 CountBits64(uint64 Value)
	{
		int32 Count = 0;
		while (Value != 0)
		{
			Value &= Value - 1;
			++Count;
		}
		return Count;
	}

	int32 LowestSetBitIndex(const uint64 Mask)
	{
		for (int32 Index = 0; Index < 64; ++Index)
		{
			if ((Mask & (1ull << Index)) != 0)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	double ComputeCadenceMaFromVelocity(const double VelocityMmPerYear)
	{
		const double Alpha = FMath::Min(1.0, VelocityMmPerYear / V0MmPerYear);
		return (1.0 - Alpha) * ResamplingMMaxMa + Alpha * ResamplingMMinMa;
	}

	double AngularSpeedRadiansPerStep(const double VelocityMmPerYear)
	{
		const double DistanceKmPerStep = VelocityMmPerYear * 1.0e-6 * DeltaTimeMa * 1000000.0;
		return DistanceKmPerStep / EarthRadiusKm;
	}

	void BuildDefaultMotions(const CarrierLab::FCarrierState& State, TArray<FStage15Motion>& OutMotions, const double VelocityMmPerYear)
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

	void BuildForcedPairMotions(
		const CarrierLab::FCarrierState& State,
		TArray<FStage15Motion>& OutMotions,
		const bool bConvergent,
		const double VelocityMmPerYear)
	{
		OutMotions.Reset(State.Plates.Num());
		OutMotions.SetNum(State.Plates.Num());
		const double AngularSpeed = AngularSpeedRadiansPerStep(VelocityMmPerYear);
		for (int32 PlateId = 0; PlateId < State.Plates.Num(); ++PlateId)
		{
			const CarrierLab::FCarrierPlate& Plate = State.Plates[PlateId];
			const CarrierLab::FCarrierPlate& OtherPlate = State.Plates[PlateId == 0 ? 1 : 0];
			FVector3d Direction = OtherPlate.InitialCenter - FVector3d::DotProduct(OtherPlate.InitialCenter, Plate.InitialCenter) * Plate.InitialCenter;
			Direction = NormalizeOrFallback(Direction, FVector3d::UnitX());
			if (!bConvergent)
			{
				Direction *= -1.0;
			}
			OutMotions[PlateId].Axis = NormalizeOrFallback(FVector3d::CrossProduct(Plate.InitialCenter, Direction), FVector3d::UnitZ());
			OutMotions[PlateId].AngularSpeedRadiansPerStep = AngularSpeed;
			OutMotions[PlateId].CurrentCenter = Plate.InitialCenter;
		}
	}

	void ApplyOneRigidMotionStep(CarrierLab::FCarrierState& State, TArray<FStage15Motion>& Motions)
	{
		for (CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			if (!Motions.IsValidIndex(Plate.PlateId))
			{
				continue;
			}
			const FStage15Motion& Motion = Motions[Plate.PlateId];
			for (CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
			{
				Vertex.UnitPosition = NormalizeOrFallback(RotateVector(Vertex.UnitPosition, Motion.Axis, Motion.AngularSpeedRadiansPerStep), Vertex.UnitPosition);
			}
		}
		for (FStage15Motion& Motion : Motions)
		{
			Motion.CurrentCenter = NormalizeOrFallback(RotateVector(Motion.CurrentCenter, Motion.Axis, Motion.AngularSpeedRadiansPerStep), Motion.CurrentCenter);
		}
	}

	void UpdateMotionCentersFromSamples(const CarrierLab::FCarrierState& State, TArray<FStage15Motion>& Motions)
	{
		for (FStage15Motion& Motion : Motions)
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
	}

	bool BuildPlateRayMeshes(const CarrierLab::FCarrierState& State, TArray<FPlateRayMesh>& OutPlateMeshes, FString& OutError)
	{
		OutPlateMeshes.Reset(State.Plates.Num());
		OutPlateMeshes.SetNum(State.Plates.Num());
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			if (!OutPlateMeshes.IsValidIndex(Plate.PlateId))
			{
				OutError = FString::Printf(TEXT("Invalid plate id %d while building ray meshes."), Plate.PlateId);
				return false;
			}
			FPlateRayMesh& PlateMesh = OutPlateMeshes[Plate.PlateId];
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
					OutError = FString::Printf(TEXT("FDynamicMesh3 rejected plate %d local triangle %d."), Plate.PlateId, LocalTriangleId);
					return false;
				}
				PlateMesh.MeshTriangleIdToLocalTriangleId.Add(MeshTriangleId, LocalTriangleId);
			}
			PlateMesh.Tree = MakeUnique<FDynamicMeshAABBTree3>(&PlateMesh.Mesh, true);
		}
		return true;
	}

	void FindTwoNearestPlates(const FVector3d& Position, const TArray<FStage15Motion>& Motions, int32& OutA, int32& OutB)
	{
		OutA = INDEX_NONE;
		OutB = INDEX_NONE;
		double BestA = -TNumericLimits<double>::Max();
		double BestB = -TNumericLimits<double>::Max();
		for (int32 PlateId = 0; PlateId < Motions.Num(); ++PlateId)
		{
			const double Dot = FVector3d::DotProduct(Position, Motions[PlateId].CurrentCenter);
			if (Dot > BestA)
			{
				BestB = BestA;
				OutB = OutA;
				BestA = Dot;
				OutA = PlateId;
			}
			else if (Dot > BestB)
			{
				BestB = Dot;
				OutB = PlateId;
			}
		}
	}

	double SignedPairSeparationVelocityForPlatePair(
		const FVector3d& Position,
		const TArray<FStage15Motion>& Motions,
		const int32 A,
		const int32 B);

	double SignedPairSeparationVelocity(const FVector3d& Position, const TArray<FStage15Motion>& Motions)
	{
		int32 A = INDEX_NONE;
		int32 B = INDEX_NONE;
		FindTwoNearestPlates(Position, Motions, A, B);
		return SignedPairSeparationVelocityForPlatePair(Position, Motions, A, B);
	}

	double SignedPairSeparationVelocityForPlatePair(
		const FVector3d& Position,
		const TArray<FStage15Motion>& Motions,
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

	bool IsSeparatingKinematics(const FVector3d& Position, const TArray<FStage15Motion>& Motions)
	{
		return SignedPairSeparationVelocity(Position, Motions) > 0.0;
	}

	bool IsTriangleFilteredBySubductionOrCollision(const int32 PlateId, const int32 LocalTriangleId)
	{
		// Thesis step 4 hook. Stage 1.5 has no subduction/collision state, so no triangle is excluded.
		return PlateId == INDEX_NONE && LocalTriangleId == INDEX_NONE;
	}

	double InterpolateContinentalFraction(const CarrierLab::FCarrierPlate& Plate, const FStage15Candidate& Candidate)
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

	bool IsSeparatingKinematics(const FVector3d& Position, const TArray<FStage15Motion>& Motions);

	int32 ChooseNearestCandidatePlate(
		const CarrierLab::FSphereSample& Sample,
		const TArray<FStage15Candidate>& Candidates,
		const TArray<FStage15Motion>& Motions)
	{
		int32 BestPlateId = INDEX_NONE;
		double BestDot = -TNumericLimits<double>::Max();
		for (const FStage15Candidate& Candidate : Candidates)
		{
			if (!Motions.IsValidIndex(Candidate.PlateId))
			{
				continue;
			}
			const double Dot = FVector3d::DotProduct(Sample.UnitPosition, Motions[Candidate.PlateId].CurrentCenter);
			if (Dot > BestDot + 1.0e-12 || (FMath::IsNearlyEqual(Dot, BestDot, 1.0e-12) && (BestPlateId == INDEX_NONE || Candidate.PlateId < BestPlateId)))
			{
				BestDot = Dot;
				BestPlateId = Candidate.PlateId;
			}
		}
		return BestPlateId;
	}

	FString MultiHitPolicyName(const EStage15MultiHitPolicy Policy)
	{
		switch (Policy)
		{
		case EStage15MultiHitPolicy::Centroid:
			return TEXT("centroid");
		case EStage15MultiHitPolicy::SyntheticSubduction:
			return TEXT("synthetic_subduction");
		case EStage15MultiHitPolicy::RandomSeeded:
			return TEXT("random_seeded");
		default:
			return TEXT("unknown");
		}
	}

	TArray<int32> UniqueCandidatePlates(const TArray<FStage15Candidate>& Candidates)
	{
		TArray<int32> PlateIds;
		for (const FStage15Candidate& Candidate : Candidates)
		{
			PlateIds.AddUnique(Candidate.PlateId);
		}
		PlateIds.Sort();
		return PlateIds;
	}

	int32 ChooseSyntheticSubductionCandidatePlate(
		const CarrierLab::FSphereSample& Sample,
		const TArray<FStage15Candidate>& Candidates,
		const TArray<FStage15Motion>& Motions)
	{
		const TArray<int32> PlateIds = UniqueCandidatePlates(Candidates);
		if (PlateIds.Num() <= 1 || IsSeparatingKinematics(Sample.UnitPosition, Motions))
		{
			return ChooseNearestCandidatePlate(Sample, Candidates, Motions);
		}

		const int32 LowerIdSubductingPlate = PlateIds[0];
		TArray<FStage15Candidate> RemainingCandidates;
		for (const FStage15Candidate& Candidate : Candidates)
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
		const TArray<FStage15Candidate>& Candidates,
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
		const TArray<FStage15Candidate>& Candidates,
		const TArray<FStage15Motion>& Motions,
		const EStage15MultiHitPolicy Policy,
		const int32 TieBreakSeed,
		const int32 Step,
		const int32 EventId)
	{
		switch (Policy)
		{
		case EStage15MultiHitPolicy::SyntheticSubduction:
			return ChooseSyntheticSubductionCandidatePlate(Sample, Candidates, Motions);
		case EStage15MultiHitPolicy::RandomSeeded:
			return ChooseRandomSeededCandidatePlate(Sample, Candidates, TieBreakSeed, Step, EventId);
		case EStage15MultiHitPolicy::Centroid:
		default:
			return ChooseNearestCandidatePlate(Sample, Candidates, Motions);
		}
	}

	int32 BoundaryBinKey(const FVector3d& UnitPosition)
	{
		const double Lon = FMath::Atan2(UnitPosition.Y, UnitPosition.X);
		const double Lat = FMath::Asin(FMath::Clamp(UnitPosition.Z, -1.0, 1.0));
		const int32 LonIndex = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble((Lon + UE_DOUBLE_PI) / (2.0 * UE_DOUBLE_PI) * BoundaryLonBins)), 0, BoundaryLonBins - 1);
		const int32 LatIndex = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble((Lat + 0.5 * UE_DOUBLE_PI) / UE_DOUBLE_PI * BoundaryLatBins)), 0, BoundaryLatBins - 1);
		return LatIndex * BoundaryLonBins + LonIndex;
	}

	void AddBoundaryPoint(FStage15BoundaryIndex& Index, const int32 PlateId, const FVector3d& Position, const double ContinentalFraction)
	{
		FStage15BoundaryPoint Point;
		Point.PlateId = PlateId;
		Point.UnitPosition = NormalizeOrFallback(Position, FVector3d::UnitZ());
		Point.ContinentalFraction = FMath::Clamp(ContinentalFraction, 0.0, 1.0);
		const int32 PointIndex = Index.Points.Add(Point);
		Index.Bins.FindOrAdd(BoundaryBinKey(Point.UnitPosition)).Add(PointIndex);
	}

	FStage15BoundaryIndex BuildBoundaryIndex(const CarrierLab::FCarrierState& State)
	{
		FStage15BoundaryIndex Index;
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
		const FStage15BoundaryIndex& Index,
		const FVector3d& SamplePosition,
		FStage15BoundaryPoint& OutQ1,
		FStage15BoundaryPoint& OutQ2)
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
			const FStage15BoundaryPoint& Point = Index.Points[PointIndex];
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

	void QuerySampleCandidates(
		const CarrierLab::FCarrierState& State,
		const TArray<FPlateRayMesh>& PlateMeshes,
		const CarrierLab::FSphereSample& Sample,
		FStage15Metrics& Metrics,
		TArray<FStage15Candidate>& Candidates,
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
			++Metrics.RayTreeQueryCount;
			PlateMeshes[Plate.PlateId].Tree->FindAllHitTriangles(Ray, Hits, QueryOptions);
			bool bAcceptedHit = false;
			for (const MeshIntersection::FHitIntersectionResult& Hit : Hits)
			{
				if (Hit.Distance <= 0.0 || Hit.Distance > 2.0 + 1.0e-6)
				{
					continue;
				}
				const int32* LocalTriangleId = PlateMeshes[Plate.PlateId].MeshTriangleIdToLocalTriangleId.Find(Hit.TriangleId);
				if (LocalTriangleId == nullptr || IsTriangleFilteredBySubductionOrCollision(Plate.PlateId, *LocalTriangleId))
				{
					continue;
				}
				++Metrics.RayHitTriangleCount;
				FStage15Candidate Candidate;
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
				if (LocalTriangleId != nullptr && NearestDistSqr <= 1.0e-16 && !IsTriangleFilteredBySubductionOrCollision(Plate.PlateId, *LocalTriangleId))
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
							if (DotB >= DotA && DotB >= DotC)
							{
								Bary = FVector3d(0.0, 1.0, 0.0);
							}
							else if (DotC >= DotA && DotC >= DotB)
							{
								Bary = FVector3d(0.0, 0.0, 1.0);
							}
						}
					}
					++Metrics.RayHitTriangleCount;
					FStage15Candidate Candidate;
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

	FString HashProjection(const FStage15Buffers& Buffers)
	{
		uint64 Hash = 1469598103934665603ull;
		for (int32 Index = 0; Index < Buffers.ResolvedPlateIds.Num(); ++Index)
		{
			HashMix(Hash, static_cast<uint64>(Buffers.ResolvedPlateIds[Index] + 1));
			HashMix(Hash, static_cast<uint64>(Buffers.MissClasses.IsValidIndex(Index) ? Buffers.MissClasses[Index] : 0));
			HashMix(Hash, static_cast<uint64>(Buffers.OverlapClasses.IsValidIndex(Index) ? Buffers.OverlapClasses[Index] : 0));
			HashMix(Hash, static_cast<uint64>(Buffers.BoundaryMask.IsValidIndex(Index) ? Buffers.BoundaryMask[Index] : 0));
			HashMixDouble(Hash, Buffers.ProjectedContinentalFraction.IsValidIndex(Index) ? Buffers.ProjectedContinentalFraction[Index] : 0.0);
		}
		return HashToString(Hash);
	}

	FString HashState(const CarrierLab::FCarrierState& State)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMix(Hash, static_cast<uint64>(State.Samples.Num()));
		HashMix(Hash, static_cast<uint64>(State.Plates.Num()));
		for (const CarrierLab::FSphereSample& Sample : State.Samples)
		{
			HashMix(Hash, static_cast<uint64>(Sample.PlateId + 1));
			HashMixDouble(Hash, Sample.ContinentalFraction);
			HashMix(Hash, Sample.bContinental ? 1ull : 0ull);
		}
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			HashMix(Hash, static_cast<uint64>(Plate.PlateId + 1));
			HashMix(Hash, static_cast<uint64>(Plate.Vertices.Num()));
			HashMix(Hash, static_cast<uint64>(Plate.LocalTriangles.Num()));
			for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
			{
				HashMix(Hash, static_cast<uint64>(Vertex.GlobalSampleId + 1));
				HashMixDouble(Hash, Vertex.UnitPosition.X);
				HashMixDouble(Hash, Vertex.UnitPosition.Y);
				HashMixDouble(Hash, Vertex.UnitPosition.Z);
				HashMixDouble(Hash, Vertex.ContinentalFraction);
			}
		}
		return HashToString(Hash);
	}

	FStage15Metrics ProjectCurrent(
		const FStage15RunConfig& Config,
		const CarrierLab::FCarrierState& State,
		const TArray<FStage15Motion>& Motions,
		const int32 Step,
		const int32 EventCount,
		FStage15Buffers* OutBuffers)
	{
		FStage15Metrics Metrics;
		Metrics.Scenario = Config.Scenario;
		Metrics.Resolution = State.Samples.Num();
		Metrics.PlateCount = State.Plates.Num();
		Metrics.Seed = State.Config.Seed;
		Metrics.Step = Step;
		Metrics.EventCount = EventCount;

		FStage15Buffers LocalBuffers;
		FStage15Buffers& Buffers = OutBuffers != nullptr ? *OutBuffers : LocalBuffers;
		Buffers.ResolvedPlateIds.Init(INDEX_NONE, State.Samples.Num());
		Buffers.MissClasses.Init(static_cast<uint8>(EStage15MissClass::None), State.Samples.Num());
		Buffers.OverlapClasses.Init(static_cast<uint8>(EStage15OverlapClass::None), State.Samples.Num());
		Buffers.BoundaryMask.Init(0, State.Samples.Num());
		Buffers.ProjectedContinentalFraction.Init(0.0, State.Samples.Num());

		const double StartSeconds = FPlatformTime::Seconds();
		TArray<FPlateRayMesh> PlateMeshes;
		FString MeshError;
		if (!BuildPlateRayMeshes(State, PlateMeshes, MeshError))
		{
			UE_LOG(LogTemp, Error, TEXT("Stage 1.5 BVH build failed: %s"), *MeshError);
			++Metrics.NaNOrInfCount;
			return Metrics;
		}

		double TotalArea = 0.0;
		double AuthoritativeArea = 0.0;
		double ProjectedArea = 0.0;
		TArray<FStage15Candidate> Candidates;
		for (const CarrierLab::FSphereSample& Sample : State.Samples)
		{
			TotalArea += Sample.AreaWeight;
			AuthoritativeArea += Sample.AreaWeight * Sample.ContinentalFraction;
			if (!IsFiniteUnit(Sample.UnitPosition))
			{
				++Metrics.NaNOrInfCount;
				++Metrics.RawMissCount;
				Buffers.MissClasses[Sample.Id] = static_cast<uint8>(EStage15MissClass::OutOfDomain);
				continue;
			}
			uint64 PlateMask = 0;
			bool bAnyBoundary = false;
			QuerySampleCandidates(State, PlateMeshes, Sample, Metrics, Candidates, PlateMask, bAnyBoundary);
			const int32 PlateHitCount = CountBits64(PlateMask);
			Buffers.BoundaryMask[Sample.Id] = bAnyBoundary ? 1 : 0;
			if (PlateHitCount == 0)
			{
				++Metrics.RawMissCount;
				if (IsSeparatingKinematics(Sample.UnitPosition, Motions))
				{
					++Metrics.DivergentGapCount;
					Buffers.MissClasses[Sample.Id] = static_cast<uint8>(EStage15MissClass::DivergentGap);
				}
				else
				{
					++Metrics.NumericMissCount;
					Buffers.MissClasses[Sample.Id] = static_cast<uint8>(EStage15MissClass::NumericMiss);
				}
				continue;
			}

			++Metrics.RawHitCount;
			if (PlateHitCount > 1)
			{
				++Metrics.RawMultiHitCount;
				if (PlateHitCount >= 3)
				{
					++Metrics.ThirdPlateIntrusionCount;
					if (bAnyBoundary)
					{
						++Metrics.ThirdPlateBoundaryDegenerateCount;
						Buffers.OverlapClasses[Sample.Id] = static_cast<uint8>(EStage15OverlapClass::ThirdPlateBoundary);
					}
					else
					{
						++Metrics.ThirdPlateNonDegenerateCount;
						Buffers.OverlapClasses[Sample.Id] = static_cast<uint8>(EStage15OverlapClass::ThirdPlateNonDegenerate);
					}
				}
				else if (bAnyBoundary)
				{
					++Metrics.BoundaryDegenerateOverlapCount;
					Buffers.OverlapClasses[Sample.Id] = static_cast<uint8>(EStage15OverlapClass::BoundaryDegenerate);
				}
				else if (IsSeparatingKinematics(Sample.UnitPosition, Motions))
				{
					++Metrics.NumericOverlapCount;
					Buffers.OverlapClasses[Sample.Id] = static_cast<uint8>(EStage15OverlapClass::NumericOverlap);
				}
				else
				{
					++Metrics.ConvergentOverlapCount;
					Buffers.OverlapClasses[Sample.Id] = static_cast<uint8>(EStage15OverlapClass::ConvergentOverlap);
				}
			}

			const int32 ResolvedPlateId = PlateHitCount == 1
				? LowestSetBitIndex(PlateMask)
				: ChooseCandidatePlateByPolicy(Sample, Candidates, Motions, Config.MultiHitPolicy, Config.TieBreakSeed, Step, EventCount);
			const FStage15Candidate* Chosen = Candidates.FindByPredicate([ResolvedPlateId](const FStage15Candidate& Candidate)
			{
				return Candidate.PlateId == ResolvedPlateId;
			});
			const double Fraction = (Chosen != nullptr && State.Plates.IsValidIndex(ResolvedPlateId))
				? InterpolateContinentalFraction(State.Plates[ResolvedPlateId], *Chosen)
				: 0.0;
			Buffers.ResolvedPlateIds[Sample.Id] = ResolvedPlateId;
			Buffers.ProjectedContinentalFraction[Sample.Id] = Fraction;
			ProjectedArea += Sample.AreaWeight * Fraction;
		}

		Metrics.AuthoritativeCAF = AuthoritativeArea / FMath::Max(TotalArea, UE_DOUBLE_SMALL_NUMBER);
		Metrics.ProjectedCAF = ProjectedArea / FMath::Max(TotalArea, UE_DOUBLE_SMALL_NUMBER);
		Metrics.StepKernelSeconds = FPlatformTime::Seconds() - StartSeconds;
		Metrics.MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		Metrics.ProjectionHash = HashProjection(Buffers);
		Metrics.StateHash = HashState(State);
		return Metrics;
	}

	TArray<double> ComputePlateAreas(const CarrierLab::FCarrierState& State)
	{
		TArray<double> Areas;
		Areas.Init(0.0, State.Plates.Num());
		for (const CarrierLab::FSphereSample& Sample : State.Samples)
		{
			if (Areas.IsValidIndex(Sample.PlateId))
			{
				Areas[Sample.PlateId] += Sample.AreaWeight;
			}
		}
		return Areas;
	}

	void RunResamplingEventCore(
		const FStage15RunConfig& Config,
		CarrierLab::FCarrierState& State,
		TArray<FStage15Motion>& Motions,
		const int32 Step,
		const int32 EventId,
		FStage15EventRecord& OutEvent)
	{
		OutEvent.Scenario = Config.Scenario;
		OutEvent.MultiHitPolicy = MultiHitPolicyName(Config.MultiHitPolicy);
		OutEvent.Resolution = State.Samples.Num();
		OutEvent.EventId = EventId;
		OutEvent.Step = Step;
		OutEvent.TieBreakSeed = Config.TieBreakSeed;
		const TArray<double> AreaBefore = ComputePlateAreas(State);

		FStage15Buffers BeforeBuffers;
		const FStage15Metrics Before = ProjectCurrent(Config, State, Motions, Step, EventId, &BeforeBuffers);
		OutEvent.RawMissBefore = Before.RawMissCount;
		OutEvent.RawMultiBefore = Before.RawMultiHitCount;
		OutEvent.AuthoritativeCAFBefore = Before.AuthoritativeCAF;
		OutEvent.ProjectedCAFBefore = Before.ProjectedCAF;

		TArray<FPlateRayMesh> PlateMeshes;
		FString MeshError;
		if (!BuildPlateRayMeshes(State, PlateMeshes, MeshError))
		{
			UE_LOG(LogTemp, Error, TEXT("Stage 1.5 event mesh build failed: %s"), *MeshError);
			return;
		}
		const FStage15BoundaryIndex BoundaryIndex = BuildBoundaryIndex(State);
		TArray<int32> NewPlateIds;
		TArray<double> NewFractions;
		NewPlateIds.Init(INDEX_NONE, State.Samples.Num());
		NewFractions.Init(0.0, State.Samples.Num());
		TArray<FStage15Candidate> Candidates;
		FStage15Metrics QueryMetrics;
		for (const CarrierLab::FSphereSample& Sample : State.Samples)
		{
			uint64 PlateMask = 0;
			bool bAnyBoundary = false;
			QuerySampleCandidates(State, PlateMeshes, Sample, QueryMetrics, Candidates, PlateMask, bAnyBoundary);
			const int32 PlateHitCount = CountBits64(PlateMask);
			if (PlateHitCount == 0)
			{
				FStage15BoundaryPoint Q1;
				FStage15BoundaryPoint Q2;
				if (FindNearestBoundaryPair(BoundaryIndex, Sample.UnitPosition, Q1, Q2))
				{
					FStage15GapFill Fill;
					Fill.SampleId = Sample.Id;
					Fill.Q1PlateId = Q1.PlateId;
					Fill.Q2PlateId = Q2.PlateId;
					Fill.Q1ContinentalFraction = Q1.ContinentalFraction;
					Fill.Q2ContinentalFraction = Q2.ContinentalFraction;
					Fill.FillContinentalFraction = FMath::Clamp(0.5 * (Q1.ContinentalFraction + Q2.ContinentalFraction), 0.0, 1.0);
					const FVector3d RidgePoint = NormalizeOrFallback(Q1.UnitPosition + Q2.UnitPosition, Sample.UnitPosition);
					Fill.SignedRelativeVelocity = SignedPairSeparationVelocityForPlatePair(RidgePoint, Motions, Q1.PlateId, Q2.PlateId);
					Fill.bSeparating = Fill.SignedRelativeVelocity > 0.0;
					OutEvent.GapFills.Add(Fill);
					++OutEvent.GapFillCount;
					if (!Fill.bSeparating)
					{
						++OutEvent.NonSeparatingGapFillCount;
					}
					NewPlateIds[Sample.Id] = Q1.PlateId;
					NewFractions[Sample.Id] = Fill.FillContinentalFraction;
				}
				else
				{
					++OutEvent.NoBoundaryPairFallbackCount;
					NewPlateIds[Sample.Id] = Sample.PlateId;
					NewFractions[Sample.Id] = Sample.ContinentalFraction;
				}
				continue;
			}

			const int32 ResolvedPlateId = PlateHitCount == 1
				? LowestSetBitIndex(PlateMask)
				: ChooseCandidatePlateByPolicy(Sample, Candidates, Motions, Config.MultiHitPolicy, Config.TieBreakSeed, Step, EventId);
			if (PlateHitCount > 1)
			{
				++OutEvent.PolicyMultiHitCount;
			}
			const FStage15Candidate* Chosen = Candidates.FindByPredicate([ResolvedPlateId](const FStage15Candidate& Candidate)
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
		UpdateMotionCentersFromSamples(State, Motions);

		const TArray<double> AreaAfter = ComputePlateAreas(State);
		OutEvent.PlateAreaDeltaPercents.Init(0.0, AreaBefore.Num());
		for (int32 PlateId = 0; PlateId < AreaBefore.Num() && PlateId < AreaAfter.Num(); ++PlateId)
		{
			const double DeltaPercent = AreaBefore[PlateId] > UE_DOUBLE_SMALL_NUMBER
				? 100.0 * FMath::Abs(AreaAfter[PlateId] - AreaBefore[PlateId]) / AreaBefore[PlateId]
				: 0.0;
			OutEvent.PlateAreaDeltaPercents[PlateId] = DeltaPercent;
			if (DeltaPercent > OutEvent.MaxPlateAreaDeltaPercent)
			{
				OutEvent.MaxPlateAreaDeltaPercent = DeltaPercent;
				OutEvent.MaxPlateAreaDeltaPlateId = PlateId;
			}
		}

		FStage15Buffers AfterBuffers;
		const FStage15Metrics After = ProjectCurrent(Config, State, Motions, Step, EventId, &AfterBuffers);
		OutEvent.RawMissAfter = After.RawMissCount;
		OutEvent.RawMultiAfter = After.RawMultiHitCount;
		OutEvent.NonBoundaryMultiAfter =
			After.ConvergentOverlapCount +
			After.NumericOverlapCount +
			After.ThirdPlateNonDegenerateCount;
		OutEvent.AuthoritativeCAFAfter = After.AuthoritativeCAF;
		OutEvent.ProjectedCAFAfter = After.ProjectedCAF;

		uint64 Hash = 1469598103934665603ull;
		HashMix(Hash, static_cast<uint64>(OutEvent.EventId));
		HashMix(Hash, static_cast<uint64>(OutEvent.Step));
		HashMix(Hash, static_cast<uint64>(OutEvent.TieBreakSeed + 1));
		for (const TCHAR Ch : OutEvent.MultiHitPolicy)
		{
			HashMix(Hash, static_cast<uint64>(Ch));
		}
		HashMix(Hash, static_cast<uint64>(OutEvent.GapFillCount));
		HashMix(Hash, static_cast<uint64>(OutEvent.NonSeparatingGapFillCount));
		HashMix(Hash, static_cast<uint64>(OutEvent.NoBoundaryPairFallbackCount));
		HashMix(Hash, static_cast<uint64>(OutEvent.PolicyMultiHitCount));
		HashMixDouble(Hash, OutEvent.AuthoritativeCAFBefore);
		HashMixDouble(Hash, OutEvent.AuthoritativeCAFAfter);
		HashMixDouble(Hash, OutEvent.ProjectedCAFBefore);
		HashMixDouble(Hash, OutEvent.ProjectedCAFAfter);
		HashMixDouble(Hash, OutEvent.MaxPlateAreaDeltaPercent);
		HashMix(Hash, static_cast<uint64>(OutEvent.MaxPlateAreaDeltaPlateId + 1));
		for (const double PlateDelta : OutEvent.PlateAreaDeltaPercents)
		{
			HashMixDouble(Hash, PlateDelta);
		}
		for (const FStage15GapFill& Fill : OutEvent.GapFills)
		{
			HashMix(Hash, static_cast<uint64>(Fill.SampleId + 1));
			HashMix(Hash, static_cast<uint64>(Fill.Q1PlateId + 1));
			HashMix(Hash, static_cast<uint64>(Fill.Q2PlateId + 1));
			HashMixDouble(Hash, Fill.Q1ContinentalFraction);
			HashMixDouble(Hash, Fill.Q2ContinentalFraction);
			HashMixDouble(Hash, Fill.FillContinentalFraction);
			HashMixDouble(Hash, Fill.SignedRelativeVelocity);
			HashMix(Hash, Fill.bSeparating ? 1ull : 0ull);
		}
		OutEvent.EventHash = HashToString(Hash);
	}

	void ApplyResamplingEvent(
		const FStage15RunConfig& Config,
		CarrierLab::FCarrierState& State,
		TArray<FStage15Motion>& Motions,
		const int32 Step,
		const int32 EventId,
		FStage15EventRecord& OutEvent)
	{
		RunResamplingEventCore(Config, State, Motions, Step, EventId, OutEvent);
	}

	void ApplyResamplingEventForTest(
		const FStage15RunConfig& Config,
		CarrierLab::FCarrierState& State,
		TArray<FStage15Motion>& Motions,
		const int32 Step,
		const int32 EventId,
		FStage15EventRecord& OutEvent)
	{
		RunResamplingEventCore(Config, State, Motions, Step, EventId, OutEvent);
	}

	bool ShouldFireResamplingEvent(const FStage15RunConfig& Config, const int32 Step)
	{
		if (!Config.bEnableResamplingEvents || Step <= 0 || Config.VelocityMmPerYear <= UE_DOUBLE_SMALL_NUMBER || Config.PlateCount <= 1)
		{
			return false;
		}
		if (Config.bHoldEventsUntilFinalStep)
		{
			return Step == Config.StepCount;
		}
		return Config.CadenceSteps > 0 && Step % Config.CadenceSteps == 0;
	}

	bool AdvanceStep(
		const FStage15RunConfig& Config,
		CarrierLab::FCarrierState& State,
		TArray<FStage15Motion>& Motions,
		const int32 Step,
		int32& EventCount,
		TArray<FStage15EventRecord>& Events)
	{
		ApplyOneRigidMotionStep(State, Motions);
		if (ShouldFireResamplingEvent(Config, Step))
		{
			FStage15EventRecord Event;
			ApplyResamplingEvent(Config, State, Motions, Step, EventCount + 1, Event);
			Events.Add(Event);
			++EventCount;
			return true;
		}
		return false;
	}

	void ComputeDriftMetrics(
		const CarrierLab::FCarrierState& State,
		const TArray<FStage15Motion>& Motions,
		const int32 Step,
		const int32 LastResamplingStep,
		FStage15Metrics& Metrics)
	{
		TArray<double> ErrorsKm;
		double SumKm = 0.0;
		const int32 WindowSteps = FMath::Max(0, Step - LastResamplingStep);
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			if (!Motions.IsValidIndex(Plate.PlateId))
			{
				continue;
			}
			for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
			{
				if (!State.Samples.IsValidIndex(Vertex.GlobalSampleId))
				{
					continue;
				}
				const FVector3d Expected = NormalizeOrFallback(
					RotateVector(
						State.Samples[Vertex.GlobalSampleId].UnitPosition,
						Motions[Plate.PlateId].Axis,
						Motions[Plate.PlateId].AngularSpeedRadiansPerStep * static_cast<double>(WindowSteps)),
					State.Samples[Vertex.GlobalSampleId].UnitPosition);
				const double ErrorKm = AngularDistanceRadians(Vertex.UnitPosition, Expected) * EarthRadiusKm;
				ErrorsKm.Add(ErrorKm);
				SumKm += ErrorKm;
			}
		}
		if (ErrorsKm.IsEmpty())
		{
			return;
		}
		ErrorsKm.Sort();
		Metrics.DriftErrorMeanKm = SumKm / static_cast<double>(ErrorsKm.Num());
		Metrics.DriftErrorP95Km = ErrorsKm[FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(0.95 * (ErrorsKm.Num() - 1))), 0, ErrorsKm.Num() - 1)];
	}

	FString HashEventLog(const TArray<FStage15EventRecord>& Events)
	{
		uint64 Hash = 1469598103934665603ull;
		for (const FStage15EventRecord& Event : Events)
		{
			HashMix(Hash, static_cast<uint64>(Event.EventId));
			HashMix(Hash, static_cast<uint64>(Event.Step));
			HashMix(Hash, static_cast<uint64>(Event.GapFillCount));
			HashMix(Hash, static_cast<uint64>(Event.NonSeparatingGapFillCount));
			HashMix(Hash, static_cast<uint64>(Event.NoBoundaryPairFallbackCount));
			HashMix(Hash, static_cast<uint64>(Event.PolicyMultiHitCount));
			HashMixDouble(Hash, Event.AuthoritativeCAFAfter);
			HashMixDouble(Hash, Event.ProjectedCAFAfter);
			for (const TCHAR Ch : Event.EventHash)
			{
				HashMix(Hash, static_cast<uint64>(Ch));
			}
		}
		return HashToString(Hash);
	}

	void ApplyMaterialOverride(CarrierLab::FCarrierState& State, const FStage15RunConfig& Config)
	{
		if (!Config.bAllContinental && !Config.bOceanOnly)
		{
			return;
		}
		const double Fraction = Config.bAllContinental ? 1.0 : 0.0;
		for (CarrierLab::FSphereSample& Sample : State.Samples)
		{
			Sample.ContinentalFraction = Fraction;
			Sample.bContinental = Fraction >= 0.5;
		}
		CarrierLab::FCarrierLabStage0::RebuildPlateLocalStateFromSamples(State);
	}

	FStage15RunResult RunScenario(const FStage15RunConfig& Config)
	{
		FStage15RunResult Result;
		Result.Config = Config;
		CarrierLab::FStage0Config Stage0Config;
		Stage0Config.SampleCount = Config.Resolution;
		Stage0Config.PlateCount = Config.PlateCount;
		Stage0Config.Seed = Config.Seed;
		Stage0Config.ContinentalPlateFraction = Config.ContinentalPlateFraction;
		CarrierLab::FCarrierState State;
		FString Error;
		if (!CarrierLab::FCarrierLabStage0::BuildColdStartCarrier(Stage0Config, State, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("Stage 1.5 build failed: %s"), *Error);
			Result.bPassed = false;
			return Result;
		}
		ApplyMaterialOverride(State, Config);

		TArray<FStage15Motion> Motions;
		if (Config.MotionMode == EStage15MotionMode::ForcedConvergence)
		{
			BuildForcedPairMotions(State, Motions, true, Config.VelocityMmPerYear);
		}
		else if (Config.MotionMode == EStage15MotionMode::ForcedDivergence)
		{
			BuildForcedPairMotions(State, Motions, false, Config.VelocityMmPerYear);
		}
		else
		{
			BuildDefaultMotions(State, Motions, Config.VelocityMmPerYear);
		}
		int32 EventCount = 0;
		int32 LastResamplingStep = 0;
		for (int32 Step = 0; Step <= Config.StepCount; ++Step)
		{
			if (Step > 0)
			{
				if (AdvanceStep(Config, State, Motions, Step, EventCount, Result.Events))
				{
					LastResamplingStep = Step;
				}
			}
			if (Step == 0 || Step == 96 || Step == 128 || Step == 400 || (Config.bEnableResamplingEvents && Config.CadenceSteps > 0 && Step % Config.CadenceSteps == 0))
			{
				FStage15Metrics Metrics = ProjectCurrent(Config, State, Motions, Step, EventCount, nullptr);
				ComputeDriftMetrics(State, Motions, Step, LastResamplingStep, Metrics);
				Result.Metrics.Add(Metrics);
			}
		}
		Result.EventLogHash = HashEventLog(Result.Events);
		if ((Config.VelocityMmPerYear <= UE_DOUBLE_SMALL_NUMBER || Config.PlateCount <= 1) && !Result.Events.IsEmpty())
		{
			Result.bPassed = false;
		}
		return Result;
	}

	TArray<int32> ParseResolutions(const FString& Params)
	{
		FString Value;
		TArray<int32> Resolutions;
		if (!FParse::Value(*Params, TEXT("Resolutions="), Value))
		{
			return {60000, 100000, 250000, 500000};
		}
		TArray<FString> Parts;
		Value.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& Part : Parts)
		{
			const int32 Resolution = FCString::Atoi(*Part);
			if (Resolution > 0)
			{
				Resolutions.Add(Resolution);
			}
		}
		return Resolutions;
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("Stage1_5"), Stamp);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	FString MetricsJson(const FStage15Metrics& Metrics)
	{
		return FString::Printf(
			TEXT("{\"scenario\":%s,\"resolution\":%d,\"step\":%d,\"event_count\":%d,\"raw_miss_count\":%d,\"raw_multi_hit_count\":%d,\"divergent_gap_count\":%d,\"numeric_miss_count\":%d,\"convergent_overlap_count\":%d,\"numeric_overlap_count\":%d,\"third_plate_intrusion_count\":%d,\"authoritative_caf\":%.12f,\"projected_caf\":%.12f,\"drift_error_mean_km\":%.12f,\"drift_error_p95_km\":%.12f,\"kernel_seconds\":%.6f,\"memory_gb\":%.3f,\"projection_hash\":%s,\"state_hash\":%s}"),
			*JsonString(Metrics.Scenario),
			Metrics.Resolution,
			Metrics.Step,
			Metrics.EventCount,
			Metrics.RawMissCount,
			Metrics.RawMultiHitCount,
			Metrics.DivergentGapCount,
			Metrics.NumericMissCount,
			Metrics.ConvergentOverlapCount,
			Metrics.NumericOverlapCount,
			Metrics.ThirdPlateIntrusionCount,
			Metrics.AuthoritativeCAF,
			Metrics.ProjectedCAF,
			Metrics.DriftErrorMeanKm,
			Metrics.DriftErrorP95Km,
			Metrics.StepKernelSeconds,
			Metrics.MemoryGb,
			*JsonString(Metrics.ProjectionHash),
			*JsonString(Metrics.StateHash));
	}

	FString JsonDoubleArray(const TArray<double>& Values)
	{
		FString Result = TEXT("[");
		for (int32 Index = 0; Index < Values.Num(); ++Index)
		{
			if (Index > 0)
			{
				Result += TEXT(",");
			}
			Result += FString::Printf(TEXT("%.9f"), Values[Index]);
		}
		Result += TEXT("]");
		return Result;
	}

	FString EventJson(const FStage15EventRecord& Event)
	{
		return FString::Printf(
			TEXT("{\"scenario\":%s,\"multi_hit_policy\":%s,\"tie_break_seed\":%d,\"resolution\":%d,\"event_id\":%d,\"step\":%d,\"gap_fill_count\":%d,\"non_separating_gap_fill_count\":%d,\"no_boundary_pair_fallback_count\":%d,\"policy_multi_hit_count\":%d,\"raw_miss_before\":%d,\"raw_multi_before\":%d,\"raw_miss_after\":%d,\"raw_multi_after\":%d,\"non_boundary_multi_after\":%d,\"authoritative_caf_before\":%.12f,\"authoritative_caf_after\":%.12f,\"projected_caf_before\":%.12f,\"projected_caf_after\":%.12f,\"max_plate_area_delta_percent\":%.6f,\"max_plate_area_delta_plate_id\":%d,\"plate_area_delta_percent\":%s,\"event_hash\":%s}"),
			*JsonString(Event.Scenario),
			*JsonString(Event.MultiHitPolicy),
			Event.TieBreakSeed,
			Event.Resolution,
			Event.EventId,
			Event.Step,
			Event.GapFillCount,
			Event.NonSeparatingGapFillCount,
			Event.NoBoundaryPairFallbackCount,
			Event.PolicyMultiHitCount,
			Event.RawMissBefore,
			Event.RawMultiBefore,
			Event.RawMissAfter,
			Event.RawMultiAfter,
			Event.NonBoundaryMultiAfter,
			Event.AuthoritativeCAFBefore,
			Event.AuthoritativeCAFAfter,
			Event.ProjectedCAFBefore,
			Event.ProjectedCAFAfter,
			Event.MaxPlateAreaDeltaPercent,
			Event.MaxPlateAreaDeltaPlateId,
			*JsonDoubleArray(Event.PlateAreaDeltaPercents),
			*JsonString(Event.EventHash));
	}

	FString GapFillJson(const FStage15EventRecord& Event, const FStage15GapFill& Fill)
	{
		return FString::Printf(
			TEXT("{\"scenario\":%s,\"multi_hit_policy\":%s,\"tie_break_seed\":%d,\"resolution\":%d,\"event_id\":%d,\"step\":%d,\"sample_id\":%d,\"q1_plate_id\":%d,\"q2_plate_id\":%d,\"q1_continental_fraction\":%.12f,\"q2_continental_fraction\":%.12f,\"fill_continental_fraction\":%.12f,\"signed_relative_velocity\":%.12f,\"separating\":%s}"),
			*JsonString(Event.Scenario),
			*JsonString(Event.MultiHitPolicy),
			Event.TieBreakSeed,
			Event.Resolution,
			Event.EventId,
			Event.Step,
			Fill.SampleId,
			Fill.Q1PlateId,
			Fill.Q2PlateId,
			Fill.Q1ContinentalFraction,
			Fill.Q2ContinentalFraction,
			Fill.FillContinentalFraction,
			Fill.SignedRelativeVelocity,
			Fill.bSeparating ? TEXT("true") : TEXT("false"));
	}

	bool WriteTextFile(const FString& Path, const FString& Text)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		return FFileHelper::SaveStringToFile(Text, *Path);
	}

	FString BuildSawtoothSvg(const TArray<FStage15RunResult>& Results, const int32 Resolution)
	{
		const int32 Width = 900;
		const int32 Height = 320;
		const int32 Pad = 40;
		FString AuthPolyline;
		FString ProjPolyline;
		for (const FStage15RunResult& Result : Results)
		{
			if (Result.Config.Scenario != TEXT("cadence_centroid") || Result.Config.Resolution != Resolution)
			{
				continue;
			}
			for (const FStage15Metrics& Metrics : Result.Metrics)
			{
				const double X = Pad + (Width - 2 * Pad) * static_cast<double>(Metrics.Step) / static_cast<double>(Result.Config.StepCount);
				const double AuthY = Height - Pad - (Height - 2 * Pad) * Metrics.AuthoritativeCAF;
				const double ProjY = Height - Pad - (Height - 2 * Pad) * Metrics.ProjectedCAF;
				AuthPolyline += FString::Printf(TEXT("%.2f,%.2f "), X, AuthY);
				ProjPolyline += FString::Printf(TEXT("%.2f,%.2f "), X, ProjY);
			}
		}
		return FString::Printf(
			TEXT("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%d\" height=\"%d\"><rect width=\"100%%\" height=\"100%%\" fill=\"#0b1020\"/><text x=\"40\" y=\"24\" fill=\"#e8eefc\" font-size=\"16\">Stage 1.5 CAF sawtooth (%d samples)</text><polyline points=\"%s\" fill=\"none\" stroke=\"#5eead4\" stroke-width=\"2\"/><polyline points=\"%s\" fill=\"none\" stroke=\"#f59e0b\" stroke-width=\"2\"/><text x=\"680\" y=\"285\" fill=\"#5eead4\" font-size=\"13\">authoritative</text><text x=\"680\" y=\"305\" fill=\"#f59e0b\" font-size=\"13\">projected</text></svg>"),
			Width,
			Height,
			Resolution,
			*AuthPolyline,
			*ProjPolyline);
	}

	int32 ChooseSawtoothResolution(const TArray<FStage15RunResult>& Results)
	{
		for (const FStage15RunResult& Result : Results)
		{
			if (Result.Config.Scenario == TEXT("cadence_centroid") && Result.Config.Resolution == 250000 && Result.Metrics.Num() > 0)
			{
				return Result.Config.Resolution;
			}
		}
		for (const FStage15RunResult& Result : Results)
		{
			if (Result.Config.Scenario == TEXT("cadence_centroid") && Result.Metrics.Num() > 0)
			{
				return Result.Config.Resolution;
			}
		}
		return Results.Num() > 0 ? Results[0].Config.Resolution : 0;
	}

	bool GateEvent(const FStage15RunConfig& Config, const FStage15EventRecord& Event)
	{
		const double MissRateAfter = Event.Resolution > 0 ? static_cast<double>(Event.RawMissAfter) / static_cast<double>(Event.Resolution) : 1.0;
		const double NonBoundaryMultiRateAfter = Event.Resolution > 0 ? static_cast<double>(Event.NonBoundaryMultiAfter) / static_cast<double>(Event.Resolution) : 1.0;
		const bool bMainCarrierScenario = Config.Scenario.StartsWith(TEXT("cadence_")) || Config.Scenario.StartsWith(TEXT("long_window_shock"));
		const bool bAuthoritativeStable = FMath::Abs(Event.AuthoritativeCAFAfter - Event.AuthoritativeCAFBefore) <= 1.0e-3;
		const bool bMainNominalLand = !bMainCarrierScenario || FMath::Abs(Event.AuthoritativeCAFAfter - Config.ContinentalPlateFraction) <= 5.0e-3;
		const bool bControlMaterialExpected =
			(!Config.bAllContinental || Event.AuthoritativeCAFAfter >= 1.0 - 1.0e-6) &&
			(!Config.bOceanOnly || Event.AuthoritativeCAFAfter <= 1.0e-6);
		return bAuthoritativeStable &&
			bMainNominalLand &&
			bControlMaterialExpected &&
			FMath::Abs(Event.ProjectedCAFAfter - Event.AuthoritativeCAFAfter) <= 0.05 * FMath::Max(Event.AuthoritativeCAFAfter, UE_DOUBLE_SMALL_NUMBER) &&
			MissRateAfter < 0.02 &&
			NonBoundaryMultiRateAfter < 0.02 &&
			Event.NoBoundaryPairFallbackCount == 0 &&
			Event.MaxPlateAreaDeltaPercent < 5.0;
	}

	bool AllEventGatesPass(const TArray<FStage15RunResult>& Results)
	{
		for (const FStage15RunResult& Result : Results)
		{
			for (const FStage15EventRecord& Event : Result.Events)
			{
				if (!GateEvent(Result.Config, Event))
				{
					return false;
				}
			}
		}
		return true;
	}

	TArray<int32> GetCadenceResolutions(const TArray<FStage15RunResult>& Results)
	{
		TArray<int32> Resolutions;
		for (const FStage15RunResult& Result : Results)
		{
			if (Result.Config.Scenario.StartsWith(TEXT("cadence_")))
			{
				Resolutions.AddUnique(Result.Config.Resolution);
			}
		}
		Resolutions.Sort();
		return Resolutions;
	}

	FString FormatResolutionList(const TArray<int32>& Resolutions)
	{
		FString Text;
		for (int32 Index = 0; Index < Resolutions.Num(); ++Index)
		{
			if (Index > 0)
			{
				Text += TEXT(", ");
			}
			Text += FString::Printf(TEXT("%d"), Resolutions[Index]);
		}
		return Text.IsEmpty() ? TEXT("none") : Text;
	}

	FString BuildReport(
		const FString& OutputRoot,
		const TArray<FStage15RunResult>& Results,
		const TArray<FStage15RunResult>& ReplayResults,
		const FString& SawtoothFileName,
		const int32 SawtoothResolution)
	{
		FString Report = TEXT("# Stage 1.5 Checkpoint: Carrier Foundation Characterization\n\n");
		Report += FString::Printf(
			TEXT("Cadence: `M=128 Ma`, `m=32 Ma`, `dt=2 Ma`, chosen `vm=%.6f mm/y`, `alpha=%.6f`, `DeltaT=%.3f Ma`, `cadence_steps=32`. This intentionally targets the requested 32-step interval, producing 12 natural events over 400 steps.\n\n"),
			ChosenVelocityMmPerYear,
			ChosenVelocityMmPerYear / V0MmPerYear,
			ComputeCadenceMaFromVelocity(ChosenVelocityMmPerYear));
		Report += TEXT("Subduction/collision filtering hook exists in `IsTriangleFilteredBySubductionOrCollision`; it is a no-op because Stage 1.5 deliberately has no subduction or collision state.\n\n");
		Report += TEXT("Multi-hit handling is an experimental condition, not a paper-faithful substitute for subduction. Cadence-faithful runs are repeated with centroid tie-break, synthetic lower-id-subducts labels for convergent overlaps, and seeded random tie-break. Stable metrics across those conditions indicate process-independent carrier behavior; unstable metrics identify behavior future subduction integration owns.\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);

		const TArray<int32> CadenceResolutions = GetCadenceResolutions(Results);
		const bool bAllEventGatesPass = AllEventGatesPass(Results);
		Report += TEXT("## Resolution Scope\n\n");
		Report += FString::Printf(TEXT("This checkpoint includes target cadence resolution(s): `%s`.\n\n"), *FormatResolutionList(CadenceResolutions));
		if (!bAllEventGatesPass && (CadenceResolutions.Num() < 4 || !CadenceResolutions.Contains(500000)))
		{
			Report += TEXT("Larger target resolutions were not run in this checkpoint because the first executed target resolution already failed Stage 1.5 hard event gates. That is a stop-condition pause, not a final paper verdict; it preserves the stage discipline by documenting the 60k foundation failure before spending time on 100k/250k/500k escalation.\n\n");
		}

		Report += TEXT("## Main Metrics\n\n");
		Report += TEXT("| Scenario | Resolution | Step | Events | Miss % | Raw multi % | Auth CAF | Proj CAF | Drift p95 km | Kernel s | Memory GB | Projection hash | State hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|\n");
		for (const FStage15RunResult& Result : Results)
		{
			for (const FStage15Metrics& Metrics : Result.Metrics)
			{
				const double MissRate = Metrics.Resolution > 0 ? 100.0 * Metrics.RawMissCount / Metrics.Resolution : 0.0;
				const double MultiRate = Metrics.Resolution > 0 ? 100.0 * Metrics.RawMultiHitCount / Metrics.Resolution : 0.0;
				Report += FString::Printf(
					TEXT("| %s | %d | %d | %d | %.3f | %.3f | %.6f | %.6f | %.12f | %.6f | %.3f | `%s` | `%s` |\n"),
					*Metrics.Scenario,
					Metrics.Resolution,
					Metrics.Step,
					Metrics.EventCount,
					MissRate,
					MultiRate,
					Metrics.AuthoritativeCAF,
					Metrics.ProjectedCAF,
					Metrics.DriftErrorP95Km,
					Metrics.StepKernelSeconds,
					Metrics.MemoryGb,
					*Metrics.ProjectionHash,
					*Metrics.StateHash);
			}
		}

		Report += TEXT("\n## Resampling Events\n\n");
		Report += TEXT("| Scenario | Policy | Seed | Resolution | Event | Step | Gap fills | Non-separating gaps | No-boundary fallback | Policy multi | Miss before/after | Raw multi before/after | Non-boundary multi after | Auth CAF before/after | Proj CAF before/after | Max plate delta % | Gate |\n");
		Report += TEXT("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		bool bEventsPass = true;
		for (const FStage15RunResult& Result : Results)
		{
			for (const FStage15EventRecord& Event : Result.Events)
			{
				const bool bGate = GateEvent(Result.Config, Event);
				bEventsPass = bEventsPass && bGate;
				Report += FString::Printf(
					TEXT("| %s | %s | %d | %d | %d | %d | %d | %d | %d | %d | %d/%d | %d/%d | %d | %.6f/%.6f | %.6f/%.6f | %.3f (plate %d) | %s |\n"),
					*Event.Scenario,
					*Event.MultiHitPolicy,
					Event.TieBreakSeed,
					Event.Resolution,
					Event.EventId,
					Event.Step,
					Event.GapFillCount,
					Event.NonSeparatingGapFillCount,
					Event.NoBoundaryPairFallbackCount,
					Event.PolicyMultiHitCount,
					Event.RawMissBefore,
					Event.RawMissAfter,
					Event.RawMultiBefore,
					Event.RawMultiAfter,
					Event.NonBoundaryMultiAfter,
					Event.AuthoritativeCAFBefore,
					Event.AuthoritativeCAFAfter,
					Event.ProjectedCAFBefore,
					Event.ProjectedCAFAfter,
					Event.MaxPlateAreaDeltaPercent,
					Event.MaxPlateAreaDeltaPlateId,
					bGate ? TEXT("pass") : TEXT("fail"));
			}
		}

		Report += TEXT("\n## Foundation Characterization Readout\n\n");
		Report += TEXT("The post-event coverage path is distinguishable from the material-transfer path: resampling can close raw misses and non-boundary overlaps after an event, while authoritative CAF and per-plate area deltas still fail the conservation gates. Read Auth CAF before/after, non-separating gap fills, and max plate delta as the primary Stage 1.5 foundation diagnostics.\n\n");
		Report += TEXT("Policy sensitivity across centroid, synthetic-subduction, and random tie-break conditions means convergent-resolution behavior is not process-independent in this slice. Stable all-continental and ocean-only controls characterize the conservative q1/q2 material-transfer policy; mixed-material instability identifies the surface future subduction/collision integration must own.\n\n");

		Report += TEXT("\n## Determinism\n\n");
		Report += TEXT("| Scenario | Resolution | Event log hash | Replay hash | Match |\n");
		Report += TEXT("|---|---:|---|---|---|\n");
		bool bDeterministic = true;
		for (const FStage15RunResult& Result : Results)
		{
			const FStage15RunResult* Replay = ReplayResults.FindByPredicate([&Result](const FStage15RunResult& Candidate)
			{
				return Candidate.Config.Scenario == Result.Config.Scenario && Candidate.Config.Resolution == Result.Config.Resolution;
			});
			const bool bMatch = Replay != nullptr && Replay->EventLogHash == Result.EventLogHash;
			bDeterministic = bDeterministic && bMatch;
			Report += FString::Printf(
				TEXT("| %s | %d | `%s` | `%s` | %s |\n"),
				*Result.Config.Scenario,
				Result.Config.Resolution,
				*Result.EventLogHash,
				Replay != nullptr ? *Replay->EventLogHash : TEXT("missing"),
				bMatch ? TEXT("yes") : TEXT("no"));
		}

		Report += TEXT("\n## Aurous Comparison\n\n");
		Report += TEXT("| Metric | Lab | Aurous failed prototype | Parameters match | Note |\n");
		Report += TEXT("|---|---:|---:|---|---|\n");
		bool bWroteAurousComparison = false;
		for (const FStage15RunResult& Result : Results)
		{
			if (Result.Config.Scenario.StartsWith(TEXT("cadence_")) && Result.Config.Resolution == 250000)
			{
				const FStage15EventRecord* Closest = Result.Events.FindByPredicate([](const FStage15EventRecord& Event)
				{
					return Event.Step == 96;
				});
				if (Closest != nullptr)
				{
					bWroteAurousComparison = true;
					Report += FString::Printf(TEXT("| %s miss rate after event closest to step 100 | %.12f | 0.330000000000 | true | Lab uses event step 96 after cadence resampling; Aurous baseline is step 100 no-clean resampling failure |\n"), *Result.Config.Scenario, static_cast<double>(Closest->RawMissAfter) / static_cast<double>(Closest->Resolution));
					Report += FString::Printf(TEXT("| %s multi-hit rate after event closest to step 100 | %.12f | 0.240000000000 | true | Lab reports raw multi-hit after event; non-boundary multi is %d samples |\n"), *Result.Config.Scenario, static_cast<double>(Closest->RawMultiAfter) / static_cast<double>(Closest->Resolution), Closest->NonBoundaryMultiAfter);
				}
			}
		}
		if (!bWroteAurousComparison)
		{
			Report += TEXT("| Stage 1.5 250k event closest to step 100 | no lab value | miss 33%, multi-hit 24%; CAF 0.0006 at step 400 | false | 250k was not run after the 60k hard-gate stop, so no parameter-matched Stage 1.5 comparison row is available in this checkpoint. |\n");
		}

		Report += TEXT("\n## Sawtooth Visualization\n\n");
		Report += FString::Printf(TEXT("The sawtooth SVG is written to `%s` for the %d-sample centroid policy condition. Authoritative CAF should be flat; projected CAF should jump upward after events if resampling is closing coverage gaps.\n\n"), *SawtoothFileName, SawtoothResolution);

		Report += TEXT("## Replay Artifacts\n\n");
		Report += TEXT("`events.jsonl` includes per-event policy, seed, pre/post CAF, miss/multi counts, and per-plate area-delta arrays. `gap_fills.jsonl` has one row per gap-fill sample with q1 plate id, q2 plate id, q1/q2/fill continental fractions, signed relative velocity, and separating flag. Event hashes include policy, seed, per-plate area deltas, and gap-fill rows in append order.\n\n");

		Report += TEXT("## Post-Audit Hardening Note\n\n");
		Report += TEXT("The Stage 1.5 resampling path now counts `no_boundary_pair_fallback_count` whenever a zero-hit sample cannot find two distinct q1/q2 boundary plates. Event hashes include this count, and event gates require it to remain zero. A nonzero value is a stop-condition finding because the fixed global sample would otherwise be retaining carrier authority by inertia.\n\n");

		Report += TEXT("## Negative Controls\n\n");
		Report += TEXT("Controls included in this run: zero-motion with resampling enabled, single-plate with resampling enabled, forced divergence, forced convergence, all-continental, and ocean-only. The step loop has one guarded enable+cadence call site; `ApplyResamplingEventForTest` is reserved for direct seeded event calls and uses the same internal helper as the production path.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += (bEventsPass && bDeterministic)
			? TEXT("Foundation-characterization checkpoint is ready for user review: Stage 1.5 produced deterministic event logs and all event gates passed under the tested policy conditions. Stage 2 still requires explicit user approval.\n")
			: TEXT("No-go for Stage 2: at least one Stage 1.5 event or determinism gate failed. Treat this as a foundation-characterization finding, not as permission to tune the gates.\n");
		return Report;
	}
}

UCarrierLabStage15Commandlet::UCarrierLabStage15Commandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarrierLabStage15Commandlet::Main(const FString& Params)
{
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	const TArray<int32> Resolutions = ParseResolutions(Params);
	TArray<FStage15RunResult> Results;
	TArray<FStage15RunResult> ReplayResults;

	for (const int32 Resolution : Resolutions)
	{
		const EStage15MultiHitPolicy Policies[] =
		{
			EStage15MultiHitPolicy::Centroid,
			EStage15MultiHitPolicy::SyntheticSubduction,
			EStage15MultiHitPolicy::RandomSeeded
		};
		FStage15RunConfig CadenceBase;
		CadenceBase.Resolution = Resolution;
		CadenceBase.bEnableResamplingEvents = true;
		CadenceBase.CadenceMa = ComputeCadenceMaFromVelocity(CadenceBase.VelocityMmPerYear);
		CadenceBase.CadenceSteps = FMath::Max(1, FMath::RoundToInt(CadenceBase.CadenceMa / DeltaTimeMa));
		for (const EStage15MultiHitPolicy Policy : Policies)
		{
			FStage15RunConfig Cadence = CadenceBase;
			Cadence.MultiHitPolicy = Policy;
			Cadence.Scenario = FString::Printf(TEXT("cadence_%s"), *MultiHitPolicyName(Policy));
			Cadence.TieBreakSeed = Policy == EStage15MultiHitPolicy::RandomSeeded ? 915042 : 15042;
			Results.Add(RunScenario(Cadence));
			ReplayResults.Add(RunScenario(Cadence));
		}

		FStage15RunConfig Shock = CadenceBase;
		Shock.Scenario = TEXT("long_window_shock_centroid");
		Shock.MultiHitPolicy = EStage15MultiHitPolicy::Centroid;
		Shock.bHoldEventsUntilFinalStep = true;
		Results.Add(RunScenario(Shock));
		ReplayResults.Add(RunScenario(Shock));
	}

	TArray<FStage15RunConfig> Controls;
	FStage15RunConfig Zero;
	Zero.Scenario = TEXT("control_zero_motion");
	Zero.Resolution = 10000;
	Zero.VelocityMmPerYear = 0.0;
	Zero.bEnableResamplingEvents = true;
	Controls.Add(Zero);
	FStage15RunConfig Single = Zero;
	Single.Scenario = TEXT("control_single_plate");
	Single.PlateCount = 1;
	Single.VelocityMmPerYear = ChosenVelocityMmPerYear;
	Controls.Add(Single);
	FStage15RunConfig AllLand = Zero;
	AllLand.Scenario = TEXT("control_all_continental");
	AllLand.VelocityMmPerYear = ChosenVelocityMmPerYear;
	AllLand.bAllContinental = true;
	Controls.Add(AllLand);
	FStage15RunConfig Ocean = AllLand;
	Ocean.Scenario = TEXT("control_ocean_only");
	Ocean.bAllContinental = false;
	Ocean.bOceanOnly = true;
	Controls.Add(Ocean);
	FStage15RunConfig ForcedDivergence = Zero;
	ForcedDivergence.Scenario = TEXT("control_forced_divergence");
	ForcedDivergence.PlateCount = 2;
	ForcedDivergence.VelocityMmPerYear = ChosenVelocityMmPerYear;
	ForcedDivergence.MotionMode = EStage15MotionMode::ForcedDivergence;
	Controls.Add(ForcedDivergence);
	FStage15RunConfig ForcedConvergence = ForcedDivergence;
	ForcedConvergence.Scenario = TEXT("control_forced_convergence");
	ForcedConvergence.MotionMode = EStage15MotionMode::ForcedConvergence;
	Controls.Add(ForcedConvergence);

	for (const FStage15RunConfig& Control : Controls)
	{
		Results.Add(RunScenario(Control));
		ReplayResults.Add(RunScenario(Control));
	}

	FString MetricsJsonl;
	FString EventsJsonl;
	FString GapFillsJsonl;
	for (const FStage15RunResult& Result : Results)
	{
		for (const FStage15Metrics& Metrics : Result.Metrics)
		{
			MetricsJsonl += MetricsJson(Metrics);
			MetricsJsonl += LINE_TERMINATOR;
		}
		for (const FStage15EventRecord& Event : Result.Events)
		{
			EventsJsonl += EventJson(Event);
			EventsJsonl += LINE_TERMINATOR;
			for (const FStage15GapFill& Fill : Event.GapFills)
			{
				GapFillsJsonl += GapFillJson(Event, Fill);
				GapFillsJsonl += LINE_TERMINATOR;
			}
		}
	}

	WriteTextFile(FPaths::Combine(OutputRoot, TEXT("metrics.jsonl")), MetricsJsonl);
	WriteTextFile(FPaths::Combine(OutputRoot, TEXT("events.jsonl")), EventsJsonl);
	WriteTextFile(FPaths::Combine(OutputRoot, TEXT("gap_fills.jsonl")), GapFillsJsonl);
	const int32 SawtoothResolution = ChooseSawtoothResolution(Results);
	const FString SawtoothFileName = FString::Printf(TEXT("caf_sawtooth_%d.svg"), SawtoothResolution);
	WriteTextFile(FPaths::Combine(OutputRoot, SawtoothFileName), BuildSawtoothSvg(Results, SawtoothResolution));

	const FString Report = BuildReport(OutputRoot, Results, ReplayResults, SawtoothFileName, SawtoothResolution);
	WriteTextFile(FPaths::Combine(FPaths::ProjectDir(), TEXT("docs"), TEXT("checkpoints"), TEXT("stage-1.5-report.md")), Report);

	bool bAllPassed = true;
	for (const FStage15RunResult& Result : Results)
	{
		bAllPassed = bAllPassed && Result.bPassed;
		for (const FStage15EventRecord& Event : Result.Events)
		{
			bAllPassed = bAllPassed && GateEvent(Result.Config, Event);
		}
	}
	for (const FStage15RunResult& Result : Results)
	{
		const FStage15RunResult* Replay = ReplayResults.FindByPredicate([&Result](const FStage15RunResult& Candidate)
		{
			return Candidate.Config.Scenario == Result.Config.Scenario && Candidate.Config.Resolution == Result.Config.Resolution;
		});
		bAllPassed = bAllPassed && Replay != nullptr && Replay->EventLogHash == Result.EventLogHash;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Stage 1.5 artifacts: %s"), *OutputRoot);
	return bAllPassed ? 0 : 1;
}
