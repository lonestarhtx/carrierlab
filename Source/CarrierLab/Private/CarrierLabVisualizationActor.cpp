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
	constexpr double PhaseIIIBDistanceToFrontLimitKm = 1800.0;
	constexpr int32 PhaseIIIObservabilityMapWidth = 2048;
	constexpr int32 PhaseIIIObservabilityMapHeight = 1024;
	constexpr int32 PhaseIIIObservabilityLookupLonBins = 360;
	constexpr int32 PhaseIIIObservabilityLookupLatBins = 180;
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

	struct FCarrierLabCrustFields
	{
		double Elevation = 0.0;
		double HistoricalElevation = 0.0;
		double OceanicAge = 0.0;
		FVector3d RidgeDirection = FVector3d::ZeroVector;
		FVector3d FoldDirection = FVector3d::ZeroVector;
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

	FVector3d RotateCarrierVectorField(const FVector3d& Vector, const FVector3d& Axis, const double AngleRadians)
	{
		return Vector.SquaredLength() > UE_DOUBLE_SMALL_NUMBER
			? RotateVector(Vector, Axis, AngleRadians)
			: FVector3d::ZeroVector;
	}

	double AngularSpeedRadiansPerStep(const double VelocityMmPerYear)
	{
		const double DistanceKmPerStep = VelocityMmPerYear * 1.0e-6 * DeltaTimeMa * 1000000.0;
		return DistanceKmPerStep / EarthRadiusKm;
	}

	double VelocityMmPerYearFromAngularSpeed(const double AngularSpeedRadiansPerStep)
	{
		return FMath::Abs(AngularSpeedRadiansPerStep) * EarthRadiusKm / DeltaTimeMa;
	}

	int32 ObservabilityLookupBin(const int32 LonBin, const int32 LatBin)
	{
		const int32 WrappedLon = (LonBin % PhaseIIIObservabilityLookupLonBins + PhaseIIIObservabilityLookupLonBins) % PhaseIIIObservabilityLookupLonBins;
		const int32 ClampedLat = FMath::Clamp(LatBin, 0, PhaseIIIObservabilityLookupLatBins - 1);
		return ClampedLat * PhaseIIIObservabilityLookupLonBins + WrappedLon;
	}

	FIntPoint ObservabilityLookupPixelBin(const FVector3d& UnitPosition)
	{
		double Lon = FMath::Atan2(UnitPosition.Y, UnitPosition.X);
		if (Lon < 0.0)
		{
			Lon += UE_DOUBLE_PI * 2.0;
		}
		const double Lat = FMath::Asin(FMath::Clamp(UnitPosition.Z, -1.0, 1.0));
		return FIntPoint(
			FMath::Clamp(
				static_cast<int32>(FMath::FloorToDouble((Lon / (UE_DOUBLE_PI * 2.0)) * PhaseIIIObservabilityLookupLonBins)),
				0,
				PhaseIIIObservabilityLookupLonBins - 1),
			FMath::Clamp(
				static_cast<int32>(FMath::FloorToDouble(((Lat + UE_DOUBLE_PI * 0.5) / UE_DOUBLE_PI) * PhaseIIIObservabilityLookupLatBins)),
				0,
				PhaseIIIObservabilityLookupLatBins - 1));
	}

	bool InverseMollweidePixel(const int32 X, const int32 Y, FVector3d& OutUnitPosition)
	{
		const double NormalizedX = 2.0 * (static_cast<double>(X) + 0.5) / static_cast<double>(PhaseIIIObservabilityMapWidth) - 1.0;
		const double NormalizedY = 1.0 - 2.0 * (static_cast<double>(Y) + 0.5) / static_cast<double>(PhaseIIIObservabilityMapHeight);
		if (NormalizedX * NormalizedX + NormalizedY * NormalizedY > 1.0)
		{
			return false;
		}

		const double Theta = FMath::Asin(FMath::Clamp(NormalizedY, -1.0, 1.0));
		const double CosTheta = FMath::Cos(Theta);
		const double Longitude = FMath::Abs(CosTheta) > 1.0e-10
			? FMath::Clamp(UE_DOUBLE_PI * NormalizedX / CosTheta, -UE_DOUBLE_PI, UE_DOUBLE_PI)
			: 0.0;
		const double LatitudeArgument = FMath::Clamp((2.0 * Theta + FMath::Sin(2.0 * Theta)) / UE_DOUBLE_PI, -1.0, 1.0);
		const double Latitude = FMath::Asin(LatitudeArgument);
		const double CosLatitude = FMath::Cos(Latitude);
		OutUnitPosition = FVector3d(
			CosLatitude * FMath::Cos(Longitude),
			CosLatitude * FMath::Sin(Longitude),
			FMath::Sin(Latitude));
		return true;
	}

	int32 FindNearestObservabilitySample(
		const FVector3d& UnitPosition,
		const TArray<CarrierLab::FSphereSample>& Samples,
		const TArray<TArray<int32>>& SampleBins)
	{
		const FIntPoint Bin = ObservabilityLookupPixelBin(UnitPosition);
		double BestDot = -TNumericLimits<double>::Max();
		int32 BestSampleId = INDEX_NONE;

		for (int32 Radius = 0; Radius <= 4 && BestSampleId == INDEX_NONE; ++Radius)
		{
			for (int32 LatOffset = -Radius; LatOffset <= Radius; ++LatOffset)
			{
				const int32 LatBin = Bin.Y + LatOffset;
				if (LatBin < 0 || LatBin >= PhaseIIIObservabilityLookupLatBins)
				{
					continue;
				}
				for (int32 LonOffset = -Radius; LonOffset <= Radius; ++LonOffset)
				{
					if (FMath::Max(FMath::Abs(LonOffset), FMath::Abs(LatOffset)) != Radius)
					{
						continue;
					}
					const TArray<int32>& Candidates = SampleBins[ObservabilityLookupBin(Bin.X + LonOffset, LatBin)];
					for (const int32 SampleId : Candidates)
					{
						if (!Samples.IsValidIndex(SampleId))
						{
							continue;
						}
						const double Dot = FVector3d::DotProduct(UnitPosition, Samples[SampleId].UnitPosition);
						if (Dot > BestDot)
						{
							BestDot = Dot;
							BestSampleId = SampleId;
						}
					}
				}
			}
		}

		if (BestSampleId != INDEX_NONE)
		{
			return BestSampleId;
		}

		for (const CarrierLab::FSphereSample& Sample : Samples)
		{
			const double Dot = FVector3d::DotProduct(UnitPosition, Sample.UnitPosition);
			if (Dot > BestDot)
			{
				BestDot = Dot;
				BestSampleId = Sample.Id;
			}
		}
		return BestSampleId;
	}

	FLinearColor DimMapBase(const FLinearColor& Color)
	{
		return FLinearColor(
			FMath::Clamp(Color.R * 0.62f, 0.0f, 1.0f),
			FMath::Clamp(Color.G * 0.62f, 0.0f, 1.0f),
			FMath::Clamp(Color.B * 0.62f, 0.0f, 1.0f),
			1.0f);
	}

	FLinearColor BlendMapOverlay(const FLinearColor& Base, const FLinearColor& Overlay, const float Alpha = 0.82f)
	{
		return FLinearColor(
			FMath::Lerp(Base.R, Overlay.R, Alpha),
			FMath::Lerp(Base.G, Overlay.G, Alpha),
			FMath::Lerp(Base.B, Overlay.B, Alpha),
			1.0f);
	}

	FIntPoint ObservabilitySamplePixel(const FVector3d& UnitPosition)
	{
		const double Lon = FMath::Atan2(UnitPosition.Y, UnitPosition.X);
		const double Lat = FMath::Asin(FMath::Clamp(UnitPosition.Z, -1.0, 1.0));
		const int32 X = FMath::Clamp(
			FMath::RoundToInt(((Lon + UE_PI) / (2.0 * UE_PI)) * static_cast<double>(PhaseIIIObservabilityMapWidth - 1)),
			0,
			PhaseIIIObservabilityMapWidth - 1);
		const int32 Y = FMath::Clamp(
			FMath::RoundToInt((0.5 - Lat / UE_PI) * static_cast<double>(PhaseIIIObservabilityMapHeight - 1)),
			0,
			PhaseIIIObservabilityMapHeight - 1);
		return FIntPoint(X, Y);
	}

	void PaintObservabilitySample(TArray<FColor>& Pixels, const FIntPoint Pixel, const FColor Color, const int32 Radius)
	{
		for (int32 Dy = -Radius; Dy <= Radius; ++Dy)
		{
			for (int32 Dx = -Radius; Dx <= Radius; ++Dx)
			{
				const int32 X = FMath::Clamp(Pixel.X + Dx, 0, PhaseIIIObservabilityMapWidth - 1);
				const int32 Y = FMath::Clamp(Pixel.Y + Dy, 0, PhaseIIIObservabilityMapHeight - 1);
				Pixels[Y * PhaseIIIObservabilityMapWidth + X] = Color;
			}
		}
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

	uint64 MakePlatePairKey(const int32 A, const int32 B)
	{
		const uint32 MinPlateId = static_cast<uint32>(FMath::Min(A, B));
		const uint32 MaxPlateId = static_cast<uint32>(FMath::Max(A, B));
		return (static_cast<uint64>(MinPlateId) << 32) | static_cast<uint64>(MaxPlateId);
	}

	uint64 MakePlateTriangleKey(const int32 PlateId, const int32 LocalTriangleId)
	{
		return (static_cast<uint64>(static_cast<uint32>(PlateId)) << 32) | static_cast<uint32>(LocalTriangleId);
	}

	uint64 MakePlateVertexKey(const int32 PlateId, const int32 LocalVertexId)
	{
		return (static_cast<uint64>(static_cast<uint32>(PlateId)) << 32) | static_cast<uint32>(LocalVertexId);
	}

	double PhaseIIIC3DistanceTransfer(const double DistanceKm, const double EffectRadiusKm)
	{
		if (!FMath::IsFinite(DistanceKm) ||
			!FMath::IsFinite(EffectRadiusKm) ||
			EffectRadiusKm <= UE_DOUBLE_SMALL_NUMBER ||
			DistanceKm < 0.0 ||
			DistanceKm > EffectRadiusKm)
		{
			return 0.0;
		}
		const double NormalizedDistance = DistanceKm / EffectRadiusKm;
		return FMath::Exp(3.0 * NormalizedDistance) *
			FMath::Exp(-9.0 * NormalizedDistance * NormalizedDistance);
	}

	double PhaseIIIC3SpeedTransfer(
		const double SignedConvergenceVelocity,
		const double ReferenceVelocityMmPerYear)
	{
		if (!FMath::IsFinite(SignedConvergenceVelocity) ||
			!FMath::IsFinite(ReferenceVelocityMmPerYear) ||
			ReferenceVelocityMmPerYear <= UE_DOUBLE_SMALL_NUMBER ||
			SignedConvergenceVelocity <= 0.0)
		{
			return 0.0;
		}
		const double VelocityMmPerYear = SignedConvergenceVelocity * EarthRadiusKm / DeltaTimeMa;
		return FMath::Clamp(VelocityMmPerYear / ReferenceVelocityMmPerYear, 0.0, 1.0);
	}

	double PhaseIIIC3ReliefTransfer(
		const double HistoricalElevationKm,
		const double TrenchDepthKm,
		const double ContinentalMaxElevationKm)
	{
		const double Denominator = ContinentalMaxElevationKm - TrenchDepthKm;
		if (!FMath::IsFinite(HistoricalElevationKm) ||
			!FMath::IsFinite(TrenchDepthKm) ||
			!FMath::IsFinite(ContinentalMaxElevationKm) ||
			Denominator <= UE_DOUBLE_SMALL_NUMBER)
		{
			return 0.0;
		}
		const double NormalizedElevation = FMath::Clamp((HistoricalElevationKm - TrenchDepthKm) / Denominator, 0.0, 1.0);
		return NormalizedElevation * NormalizedElevation;
	}

	double PhaseIIIC3UpliftDeltaKm(
		const double UpliftRateMmPerYear,
		const double DistanceTransfer,
		const double SpeedTransfer,
		const double ReliefTransfer)
	{
		if (!FMath::IsFinite(UpliftRateMmPerYear) ||
			UpliftRateMmPerYear <= 0.0)
		{
			return 0.0;
		}
		return UpliftRateMmPerYear * DeltaTimeMa * DistanceTransfer * SpeedTransfer * ReliefTransfer;
	}

	void DecodePlatePairKey(const uint64 Key, int32& OutA, int32& OutB)
	{
		OutA = static_cast<int32>(static_cast<uint32>(Key >> 32));
		OutB = static_cast<int32>(static_cast<uint32>(Key & 0xffffffffull));
	}

	FString HashToString(const uint64 Hash)
	{
		return FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Hash));
	}

	FString ComputeMotionStateHash(const TArray<FCarrierLabVisualizationMotion>& Motions)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMix(Hash, static_cast<uint64>(Motions.Num() + 1));
		for (const FCarrierLabVisualizationMotion& Motion : Motions)
		{
			HashMixDouble(Hash, Motion.Axis.X);
			HashMixDouble(Hash, Motion.Axis.Y);
			HashMixDouble(Hash, Motion.Axis.Z);
			HashMixDouble(Hash, Motion.CurrentCenter.X);
			HashMixDouble(Hash, Motion.CurrentCenter.Y);
			HashMixDouble(Hash, Motion.CurrentCenter.Z);
			HashMixDouble(Hash, Motion.AngularSpeedRadiansPerStep);
		}
		return HashToString(Hash);
	}

	uint64 ComputeConvergenceTrackingHash(const CarrierLab::FCarrierState& State)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMix(Hash, static_cast<uint64>(State.ConvergenceTrackingResetSerial + 1));
		HashMix(Hash, static_cast<uint64>(State.ConvergenceTrackingDistanceCullCount + 1));
		HashMix(Hash, static_cast<uint64>(State.Plates.Num() + 1));
		TArray<uint64> MatrixPairKeys = State.ConvergenceSubductionMatrixPairKeys.Array();
		MatrixPairKeys.Sort();
		HashMix(Hash, static_cast<uint64>(MatrixPairKeys.Num() + 1));
		for (const uint64 PairKey : MatrixPairKeys)
		{
			HashMix(Hash, PairKey + 1ull);
		}
		TArray<CarrierLab::FConvergenceSubductionPolarityDecision> PolarityDecisions = State.ConvergenceSubductionPolarityDecisions;
		PolarityDecisions.Sort([](
			const CarrierLab::FConvergenceSubductionPolarityDecision& A,
			const CarrierLab::FConvergenceSubductionPolarityDecision& B)
		{
			return A.PairKey < B.PairKey;
		});
		HashMix(Hash, static_cast<uint64>(PolarityDecisions.Num() + 1));
		for (const CarrierLab::FConvergenceSubductionPolarityDecision& Decision : PolarityDecisions)
		{
			HashMix(Hash, Decision.PairKey + 1ull);
			HashMix(Hash, static_cast<uint64>(Decision.PlateA + 1));
			HashMix(Hash, static_cast<uint64>(Decision.PlateB + 1));
			HashMix(Hash, static_cast<uint64>(Decision.UnderPlate + 1));
			HashMix(Hash, static_cast<uint64>(Decision.OverPlate + 1));
			HashMixDouble(Hash, Decision.PlateAContinentalFraction);
			HashMixDouble(Hash, Decision.PlateBContinentalFraction);
			HashMixDouble(Hash, Decision.PlateAOceanicAge);
			HashMixDouble(Hash, Decision.PlateBOceanicAge);
			HashMix(Hash, static_cast<uint64>(Decision.DecisionClass) + 1ull);
		}
		TArray<CarrierLab::FConvergenceSubductionTriangleHit> TriangleHits = State.ConvergenceSubductionTriangleHits;
		TriangleHits.Sort([](
			const CarrierLab::FConvergenceSubductionTriangleHit& A,
			const CarrierLab::FConvergenceSubductionTriangleHit& B)
		{
			if (A.PairKey != B.PairKey)
			{
				return A.PairKey < B.PairKey;
			}
			if (A.PlateId != B.PlateId)
			{
				return A.PlateId < B.PlateId;
			}
			if (A.OtherPlateId != B.OtherPlateId)
			{
				return A.OtherPlateId < B.OtherPlateId;
			}
			return A.LocalTriangleId < B.LocalTriangleId;
		});
		HashMix(Hash, static_cast<uint64>(TriangleHits.Num() + 1));
		for (const CarrierLab::FConvergenceSubductionTriangleHit& Hit : TriangleHits)
		{
			HashMix(Hash, Hit.PairKey + 1ull);
			HashMix(Hash, static_cast<uint64>(Hit.PlateId + 1));
			HashMix(Hash, static_cast<uint64>(Hit.OtherPlateId + 1));
			HashMix(Hash, static_cast<uint64>(Hit.LocalTriangleId + 1));
		}
		HashMix(Hash, static_cast<uint64>(State.ConvergenceNeighborPropagationSeedCount + 1));
		HashMix(Hash, static_cast<uint64>(State.ConvergenceNeighborPropagationAddedCount + 1));
		HashMix(Hash, static_cast<uint64>(State.ConvergenceNeighborPropagationDuplicateCount + 1));
		HashMix(Hash, static_cast<uint64>(State.ConvergenceNeighborPropagationDistanceRejectedCount + 1));
		HashMix(Hash, static_cast<uint64>(State.ConvergenceNeighborPropagationInvalidCount + 1));
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			HashMix(Hash, static_cast<uint64>(Plate.PlateId + 1));
			HashMix(Hash, static_cast<uint64>(Plate.ActiveBoundaryTriangles.Num() + 1));
			HashMix(Hash, static_cast<uint64>(Plate.ActiveBoundaryTriangleDistancesKm.Num() + 1));
			for (int32 ActiveIndex = 0; ActiveIndex < Plate.ActiveBoundaryTriangles.Num(); ++ActiveIndex)
			{
				const int32 LocalTriangleId = Plate.ActiveBoundaryTriangles[ActiveIndex];
				HashMix(Hash, static_cast<uint64>(LocalTriangleId + 1));
				const CarrierLab::FCarrierPlateTriangle* Triangle = Plate.LocalTriangles.IsValidIndex(LocalTriangleId)
					? &Plate.LocalTriangles[LocalTriangleId]
					: nullptr;
				HashMix(Hash, static_cast<uint64>((Triangle != nullptr ? Triangle->SourceTriangleId : INDEX_NONE) + 1));
				HashMix(Hash, Triangle != nullptr && Triangle->bBoundary ? 1ull : 0ull);
				HashMixDouble(Hash, Plate.ActiveBoundaryTriangleDistancesKm.IsValidIndex(ActiveIndex)
					? Plate.ActiveBoundaryTriangleDistancesKm[ActiveIndex]
					: 0.0);
			}
		}
		return Hash;
	}

	double ComputeActiveTriangleDistanceStepKm(
		const CarrierLab::FCarrierPlate& Plate,
		const int32 LocalTriangleId,
		const FCarrierLabVisualizationMotion& Motion)
	{
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

		const FVector3d Barycenter = NormalizeOrFallback(
			Plate.Vertices[Triangle.A].UnitPosition +
			Plate.Vertices[Triangle.B].UnitPosition +
			Plate.Vertices[Triangle.C].UnitPosition,
			Plate.Vertices[Triangle.A].UnitPosition);
		const double SinToAxis = FVector3d::CrossProduct(Motion.Axis, Barycenter).Length();
		const double StepRadians = SinToAxis * FMath::Abs(Motion.AngularSpeedRadiansPerStep);
		return StepRadians * EarthRadiusKm;
	}

	bool ComputePlateLocalTriangleBarycenter(
		const CarrierLab::FCarrierPlate& Plate,
		const int32 LocalTriangleId,
		FVector3d& OutBarycenter)
	{
		if (!Plate.LocalTriangles.IsValidIndex(LocalTriangleId))
		{
			return false;
		}

		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
		if (!Plate.Vertices.IsValidIndex(Triangle.A) ||
			!Plate.Vertices.IsValidIndex(Triangle.B) ||
			!Plate.Vertices.IsValidIndex(Triangle.C))
		{
			return false;
		}

		OutBarycenter = NormalizeOrFallback(
			Plate.Vertices[Triangle.A].UnitPosition +
			Plate.Vertices[Triangle.B].UnitPosition +
			Plate.Vertices[Triangle.C].UnitPosition,
			Plate.Vertices[Triangle.A].UnitPosition);
		return true;
	}

	double ComputePlateLocalTriangleDistanceKm(
		const CarrierLab::FCarrierPlate& Plate,
		const int32 A,
		const int32 B)
	{
		FVector3d BaryA;
		FVector3d BaryB;
		if (!ComputePlateLocalTriangleBarycenter(Plate, A, BaryA) ||
			!ComputePlateLocalTriangleBarycenter(Plate, B, BaryB))
		{
			return TNumericLimits<double>::Max();
		}

		const double Dot = FMath::Clamp(FVector3d::DotProduct(BaryA, BaryB), -1.0, 1.0);
		return FMath::Acos(Dot) * EarthRadiusKm;
	}

	void GetPlateLocalTriangleNeighbors(
		const CarrierLab::FCarrierPlate& Plate,
		const int32 LocalTriangleId,
		TArray<int32>& OutNeighbors)
	{
		OutNeighbors.Reset();
		if (!Plate.LocalTriangles.IsValidIndex(LocalTriangleId))
		{
			return;
		}

		const CarrierLab::FCarrierPlateTriangle& Source = Plate.LocalTriangles[LocalTriangleId];
		const int32 SourceVertices[3] = {Source.A, Source.B, Source.C};
		for (int32 CandidateId = 0; CandidateId < Plate.LocalTriangles.Num(); ++CandidateId)
		{
			if (CandidateId == LocalTriangleId)
			{
				continue;
			}

			const CarrierLab::FCarrierPlateTriangle& Candidate = Plate.LocalTriangles[CandidateId];
			const int32 CandidateVertices[3] = {Candidate.A, Candidate.B, Candidate.C};
			int32 SharedVertexCount = 0;
			for (const int32 SourceVertex : SourceVertices)
			{
				for (const int32 CandidateVertex : CandidateVertices)
				{
					if (SourceVertex == CandidateVertex)
					{
						++SharedVertexCount;
						break;
					}
				}
			}

			if (SharedVertexCount >= 2)
			{
				OutNeighbors.Add(CandidateId);
			}
		}
		OutNeighbors.Sort();
	}

	bool IsSubductionPolarityDecision(const CarrierLab::FConvergenceSubductionPolarityDecision& Decision)
	{
		return Decision.UnderPlate != INDEX_NONE &&
			Decision.OverPlate != INDEX_NONE &&
			(Decision.DecisionClass == CarrierLab::EConvergenceSubductionPolarityClass::OceanicUnderContinental ||
				Decision.DecisionClass == CarrierLab::EConvergenceSubductionPolarityClass::OlderOceanicUnderYoungerOceanic);
	}

	double ComputePlateContinentalFraction(
		const CarrierLab::FCarrierState& State,
		const int32 PlateId)
	{
		if (!State.Plates.IsValidIndex(PlateId))
		{
			return 0.0;
		}

		const CarrierLab::FCarrierPlate& Plate = State.Plates[PlateId];
		double WeightedFraction = 0.0;
		double TotalWeight = 0.0;
		for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			const double Weight = FMath::Max(Vertex.AreaWeight, 0.0);
			WeightedFraction += Weight * FMath::Clamp(Vertex.ContinentalFraction, 0.0, 1.0);
			TotalWeight += Weight;
		}

		if (TotalWeight > UE_DOUBLE_SMALL_NUMBER)
		{
			return WeightedFraction / TotalWeight;
		}
		return Plate.bContinental ? 1.0 : 0.0;
	}

	double ComputePlateOceanicAge(
		const CarrierLab::FCarrierState& State,
		const int32 PlateId)
	{
		if (!State.Plates.IsValidIndex(PlateId))
		{
			return 0.0;
		}

		const CarrierLab::FCarrierPlate& Plate = State.Plates[PlateId];
		double WeightedAge = 0.0;
		double TotalWeight = 0.0;
		for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			const double Weight = FMath::Max(Vertex.AreaWeight, 0.0);
			WeightedAge += Weight * FMath::Max(Vertex.OceanicAge, 0.0);
			TotalWeight += Weight;
		}

		return TotalWeight > UE_DOUBLE_SMALL_NUMBER
			? WeightedAge / TotalWeight
			: 0.0;
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

	FVector3d RetangentAndNormalizeVectorField(const FVector3d& Vector, const FVector3d& UnitPosition)
	{
		const FVector3d Tangent = Vector - UnitPosition * FVector3d::DotProduct(Vector, UnitPosition);
		const double TangentSize = Tangent.Size();
		return TangentSize > UE_DOUBLE_SMALL_NUMBER ? Tangent / TangentSize : FVector3d::ZeroVector;
	}

	FVector3d MakeDeterministicTangent(const FVector3d& UnitPosition)
	{
		const FVector3d Reference = FMath::Abs(UnitPosition.Z) < 0.9 ? FVector3d::UnitZ() : FVector3d::UnitX();
		return RetangentAndNormalizeVectorField(FVector3d::CrossProduct(Reference, UnitPosition), UnitPosition);
	}

	bool InterpolateCrustFields(
		const CarrierLab::FCarrierPlate& Plate,
		const FCarrierLabVizCandidate& Candidate,
		const FVector3d& TargetUnitPosition,
		FCarrierLabCrustFields& OutFields)
	{
		if (!Plate.LocalTriangles.IsValidIndex(Candidate.LocalTriangleId))
		{
			return false;
		}

		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[Candidate.LocalTriangleId];
		if (!Plate.Vertices.IsValidIndex(Triangle.A) ||
			!Plate.Vertices.IsValidIndex(Triangle.B) ||
			!Plate.Vertices.IsValidIndex(Triangle.C))
		{
			return false;
		}

		const CarrierLab::FCarrierVertex& A = Plate.Vertices[Triangle.A];
		const CarrierLab::FCarrierVertex& B = Plate.Vertices[Triangle.B];
		const CarrierLab::FCarrierVertex& C = Plate.Vertices[Triangle.C];
		OutFields.Elevation =
			Candidate.Bary.X * A.Elevation +
			Candidate.Bary.Y * B.Elevation +
			Candidate.Bary.Z * C.Elevation;
		OutFields.HistoricalElevation =
			Candidate.Bary.X * A.HistoricalElevation +
			Candidate.Bary.Y * B.HistoricalElevation +
			Candidate.Bary.Z * C.HistoricalElevation;
		OutFields.OceanicAge =
			Candidate.Bary.X * A.OceanicAge +
			Candidate.Bary.Y * B.OceanicAge +
			Candidate.Bary.Z * C.OceanicAge;
		OutFields.RidgeDirection = RetangentAndNormalizeVectorField(
			Candidate.Bary.X * A.RidgeDirection +
			Candidate.Bary.Y * B.RidgeDirection +
			Candidate.Bary.Z * C.RidgeDirection,
			TargetUnitPosition);
		OutFields.FoldDirection = RetangentAndNormalizeVectorField(
			Candidate.Bary.X * A.FoldDirection +
			Candidate.Bary.Y * B.FoldDirection +
			Candidate.Bary.Z * C.FoldDirection,
			TargetUnitPosition);
		return true;
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
		const FLinearColor Ocean(0.03f, 0.26f, 0.48f, 1.0f);
		const FLinearColor Coast(0.28f, 0.56f, 0.44f, 1.0f);
		const FLinearColor Land(0.54f, 0.68f, 0.39f, 1.0f);
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

	FLinearColor ElevationColor(const double ElevationKm)
	{
		if (!FMath::IsFinite(ElevationKm))
		{
			return FLinearColor(1.0f, 0.0f, 1.0f, 1.0f);
		}
		if (FMath::Abs(ElevationKm) <= 1.0e-9)
		{
			return FLinearColor(0.08f, 0.34f, 0.16f, 1.0f);
		}

		const double Alpha = FMath::Clamp(FMath::Abs(ElevationKm) / 10.0, 0.0, 1.0);
		return ElevationKm < 0.0
			? FMath::Lerp(
				FLinearColor(0.05f, 0.36f, 0.45f, 1.0f),
				FLinearColor(0.03f, 0.08f, 0.75f, 1.0f),
				static_cast<float>(Alpha))
			: FMath::Lerp(
				FLinearColor(0.48f, 0.45f, 0.10f, 1.0f),
				FLinearColor(0.95f, 0.06f, 0.03f, 1.0f),
				static_cast<float>(Alpha));
	}

	FLinearColor SubductionRoleColor(const uint8 Role)
	{
		switch (Role)
		{
		case 1:
			return FLinearColor(0.12f, 0.25f, 0.95f, 1.0f);
		case 2:
			return FLinearColor(1.0f, 0.82f, 0.10f, 1.0f);
		case 3:
			return FLinearColor(0.85f, 0.34f, 0.05f, 1.0f);
		case 4:
			return FLinearColor(0.42f, 0.42f, 0.46f, 1.0f);
		default:
			return FLinearColor(0.02f, 0.05f, 0.09f, 1.0f);
		}
	}

	FLinearColor DistanceToFrontColor(const double DistanceKm)
	{
		if (DistanceKm < 0.0 || !FMath::IsFinite(DistanceKm))
		{
			return FLinearColor(0.02f, 0.05f, 0.09f, 1.0f);
		}
		const double Alpha = FMath::Clamp(DistanceKm / PhaseIIIBDistanceToFrontLimitKm, 0.0, 1.0);
		return FMath::Lerp(
			FLinearColor(0.06f, 0.74f, 0.92f, 1.0f),
			FLinearColor(0.96f, 0.18f, 0.04f, 1.0f),
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
	LastPhaseIIIC3UpliftAudit = FCarrierLabPhaseIIIC3UpliftAudit();
	LastPhaseIIIC4SlabPullAudit = FCarrierLabPhaseIIIC4SlabPullAudit();
	LastPhaseIIIC5ElevationLedgerAudit = FCarrierLabPhaseIIIC5ElevationLedgerAudit();
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

void ACarrierLabVisualizationActor::ConfigurePhaseIIICProcessLayer(const bool bEnabled, const bool bInEnableSlabPull)
{
	bEnablePhaseIIICSubductingMarks = bEnabled;
	bEnablePhaseIIICVisibleHistoricalElevation = bEnabled;
	bEnablePhaseIIICOverridingPlateUplift = bEnabled;
	bEnablePhaseIIICSlabPull = bEnabled && bInEnableSlabPull;
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
		const uint64 Key = MakePlateTriangleKey(Label.PlateId, Label.LocalTriangleId);
		const FCarrierLabPhaseIITriangleLabelRecord** Existing = SubductingLabelsByTriangle.Find(Key);
		if (Existing == nullptr || (*Existing != nullptr && Label.LabelId < (*Existing)->LabelId))
		{
			SubductingLabelsByTriangle.Add(Key, &Label);
		}
	}

	TMap<uint64, const CarrierLab::FConvergenceSubductingTriangleMark*> PersistentMarksByTriangle;
	if (bEnablePhaseIIICSubductingMarks)
	{
		for (const CarrierLab::FConvergenceSubductingTriangleMark& Mark : State.ConvergenceSubductingTriangleMarks)
		{
			if (!State.Plates.IsValidIndex(Mark.PlateId) ||
				!State.Plates[Mark.PlateId].LocalTriangles.IsValidIndex(Mark.LocalTriangleId))
			{
				continue;
			}
			++OutMetrics.PersistentSubductingMarkInputCount;
			const uint64 Key = MakePlateTriangleKey(Mark.PlateId, Mark.LocalTriangleId);
			const CarrierLab::FConvergenceSubductingTriangleMark** Existing = PersistentMarksByTriangle.Find(Key);
			if (Existing == nullptr || (*Existing != nullptr && Mark.MarkId < (*Existing)->MarkId))
			{
				PersistentMarksByTriangle.Add(Key, &Mark);
			}
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
	TArray<double> NewElevations;
	TArray<double> NewHistoricalElevations;
	TArray<double> NewOceanicAges;
	TArray<FVector3d> NewRidgeDirections;
	TArray<FVector3d> NewFoldDirections;
	NewPlateIds.SetNum(State.Samples.Num());
	NewFractions.SetNum(State.Samples.Num());
	NewElevations.SetNum(State.Samples.Num());
	NewHistoricalElevations.SetNum(State.Samples.Num());
	NewOceanicAges.SetNum(State.Samples.Num());
	NewRidgeDirections.SetNum(State.Samples.Num());
	NewFoldDirections.SetNum(State.Samples.Num());
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
		NewElevations[Sample.Id] = Sample.Elevation;
		NewHistoricalElevations[Sample.Id] = Sample.HistoricalElevation;
		NewOceanicAges[Sample.Id] = Sample.OceanicAge;
		NewRidgeDirections[Sample.Id] = Sample.RidgeDirection;
		NewFoldDirections[Sample.Id] = Sample.FoldDirection;
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
				NewElevations[Sample.Id] = 0.0;
				NewHistoricalElevations[Sample.Id] = 0.0;
				NewOceanicAges[Sample.Id] = 0.0;
				NewRidgeDirections[Sample.Id] = FVector3d::ZeroVector;
				NewFoldDirections[Sample.Id] = FVector3d::ZeroVector;

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
			const uint64 CandidateTriangleKey = MakePlateTriangleKey(Candidate.PlateId, Candidate.LocalTriangleId);
			const FCarrierLabPhaseIITriangleLabelRecord* const* LabelPtr = SubductingLabelsByTriangle.Find(CandidateTriangleKey);
			const CarrierLab::FConvergenceSubductingTriangleMark* const* PersistentMarkPtr = PersistentMarksByTriangle.Find(CandidateTriangleKey);
			if ((LabelPtr != nullptr && *LabelPtr != nullptr) ||
				(PersistentMarkPtr != nullptr && *PersistentMarkPtr != nullptr))
			{
				++FilteredCandidateCount;
				++OutMetrics.FilteredCandidateCount;
				SamplesWithFilteredCandidates.Add(Sample.Id);
				const FCarrierLabPhaseIITriangleLabelRecord* Label = LabelPtr != nullptr ? *LabelPtr : nullptr;
				const CarrierLab::FConvergenceSubductingTriangleMark* Mark = PersistentMarkPtr != nullptr ? *PersistentMarkPtr : nullptr;
				const int32 SourceContactId = Label != nullptr ? Label->ContactId : (Mark != nullptr ? Mark->EvidenceId : INDEX_NONE);
				const int32 SourceLabelId = Label != nullptr ? Label->LabelId : INDEX_NONE;
				if (MaterialSourceContactIdBySample.IsValidIndex(Sample.Id) &&
					MaterialSourceContactIdBySample[Sample.Id] == INDEX_NONE)
				{
					MaterialSourceContactIdBySample[Sample.Id] = SourceContactId;
					MaterialSourceLabelIdBySample[Sample.Id] = SourceLabelId;
				}
				if (Label != nullptr && Label->bFromThirdPlateContact)
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
				Decision.SourceContactId = SourceContactId;
				Decision.SourceLabelId = SourceLabelId;
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
			FCarrierLabCrustFields InterpolatedFields;
			if (State.Plates.IsValidIndex(ResolvedPlateId) &&
				InterpolateCrustFields(State.Plates[ResolvedPlateId], *Chosen, Sample.UnitPosition, InterpolatedFields))
			{
				NewElevations[Sample.Id] = InterpolatedFields.Elevation;
				NewHistoricalElevations[Sample.Id] = InterpolatedFields.HistoricalElevation;
				NewOceanicAges[Sample.Id] = InterpolatedFields.OceanicAge;
				NewRidgeDirections[Sample.Id] = InterpolatedFields.RidgeDirection;
				NewFoldDirections[Sample.Id] = InterpolatedFields.FoldDirection;
			}
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
			Sample.Elevation = NewElevations[Sample.Id];
			Sample.HistoricalElevation = NewHistoricalElevations[Sample.Id];
			Sample.OceanicAge = NewOceanicAges[Sample.Id];
			Sample.RidgeDirection = NewRidgeDirections[Sample.Id];
			Sample.FoldDirection = NewFoldDirections[Sample.Id];
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

bool ACarrierLabVisualizationActor::GetPhaseIIIA2CrustFieldAudit(
	int32& OutSampleCount,
	int32& OutPlateVertexCount,
	double& OutMaxAbsSampleElevation,
	double& OutMaxAbsPlateVertexElevation,
	double& OutMaxAbsSampleOceanicAge,
	double& OutMaxAbsPlateVertexOceanicAge) const
{
	OutSampleCount = State.Samples.Num();
	OutPlateVertexCount = 0;
	OutMaxAbsSampleElevation = 0.0;
	OutMaxAbsPlateVertexElevation = 0.0;
	OutMaxAbsSampleOceanicAge = 0.0;
	OutMaxAbsPlateVertexOceanicAge = 0.0;
	if (!bInitialized)
	{
		return false;
	}

	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		OutMaxAbsSampleElevation = FMath::Max(OutMaxAbsSampleElevation, FMath::Abs(Sample.Elevation));
		OutMaxAbsSampleOceanicAge = FMath::Max(OutMaxAbsSampleOceanicAge, FMath::Abs(Sample.OceanicAge));
	}
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		OutPlateVertexCount += Plate.Vertices.Num();
		for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			OutMaxAbsPlateVertexElevation = FMath::Max(OutMaxAbsPlateVertexElevation, FMath::Abs(Vertex.Elevation));
			OutMaxAbsPlateVertexOceanicAge = FMath::Max(OutMaxAbsPlateVertexOceanicAge, FMath::Abs(Vertex.OceanicAge));
		}
	}
	return true;
}

bool ACarrierLabVisualizationActor::GetPhaseIIIA3VectorFieldAudit(
	int32& OutSampleCount,
	int32& OutPlateVertexCount,
	double& OutMaxSampleVectorMagnitude,
	double& OutMaxPlateVertexVectorMagnitude,
	double& OutMaxPlateVertexRadialDot) const
{
	OutSampleCount = State.Samples.Num();
	OutPlateVertexCount = 0;
	OutMaxSampleVectorMagnitude = 0.0;
	OutMaxPlateVertexVectorMagnitude = 0.0;
	OutMaxPlateVertexRadialDot = 0.0;
	if (!bInitialized)
	{
		return false;
	}

	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		OutMaxSampleVectorMagnitude = FMath::Max(OutMaxSampleVectorMagnitude, Sample.RidgeDirection.Size());
		OutMaxSampleVectorMagnitude = FMath::Max(OutMaxSampleVectorMagnitude, Sample.FoldDirection.Size());
	}
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		OutPlateVertexCount += Plate.Vertices.Num();
		for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			OutMaxPlateVertexVectorMagnitude = FMath::Max(OutMaxPlateVertexVectorMagnitude, Vertex.RidgeDirection.Size());
			OutMaxPlateVertexVectorMagnitude = FMath::Max(OutMaxPlateVertexVectorMagnitude, Vertex.FoldDirection.Size());
			OutMaxPlateVertexRadialDot = FMath::Max(OutMaxPlateVertexRadialDot, FMath::Abs(FVector3d::DotProduct(Vertex.UnitPosition, Vertex.RidgeDirection)));
			OutMaxPlateVertexRadialDot = FMath::Max(OutMaxPlateVertexRadialDot, FMath::Abs(FVector3d::DotProduct(Vertex.UnitPosition, Vertex.FoldDirection)));
		}
	}
	return true;
}

bool ACarrierLabVisualizationActor::SeedPhaseIIIA3VectorAuditProbe(
	int32& OutPlateId,
	int32& OutLocalVertexId,
	FVector3d& OutInitialPosition,
	FVector3d& OutInitialRidgeDirection,
	FVector3d& OutInitialFoldDirection,
	FVector3d& OutRotationAxis,
	double& OutAngularSpeedRadiansPerStep)
{
	OutPlateId = INDEX_NONE;
	OutLocalVertexId = INDEX_NONE;
	OutInitialPosition = FVector3d::ZeroVector;
	OutInitialRidgeDirection = FVector3d::ZeroVector;
	OutInitialFoldDirection = FVector3d::ZeroVector;
	OutRotationAxis = FVector3d::UnitZ();
	OutAngularSpeedRadiansPerStep = 0.0;
	if (!bInitialized)
	{
		return false;
	}

	for (CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		if (!Motions.IsValidIndex(Plate.PlateId) || Plate.Vertices.IsEmpty())
		{
			continue;
		}

		FCarrierLabVisualizationMotion& Motion = Motions[Plate.PlateId];
		for (int32 LocalVertexId = 0; LocalVertexId < Plate.Vertices.Num(); ++LocalVertexId)
		{
			CarrierLab::FCarrierVertex& Vertex = Plate.Vertices[LocalVertexId];
			FVector3d Ridge = FVector3d::CrossProduct(Motion.Axis, Vertex.UnitPosition);
			if (Ridge.SquaredLength() <= UE_DOUBLE_SMALL_NUMBER)
			{
				Ridge = FVector3d::CrossProduct(FVector3d::UnitX(), Vertex.UnitPosition);
			}
			if (Ridge.SquaredLength() <= UE_DOUBLE_SMALL_NUMBER)
			{
				Ridge = FVector3d::CrossProduct(FVector3d::UnitY(), Vertex.UnitPosition);
			}
			if (Ridge.SquaredLength() <= UE_DOUBLE_SMALL_NUMBER)
			{
				continue;
			}

			Ridge = Ridge.GetSafeNormal();
			FVector3d Fold = FVector3d::CrossProduct(Vertex.UnitPosition, Ridge).GetSafeNormal();
			Vertex.RidgeDirection = Ridge;
			Vertex.FoldDirection = Fold;

			OutPlateId = Plate.PlateId;
			OutLocalVertexId = LocalVertexId;
			OutInitialPosition = Vertex.UnitPosition;
			OutInitialRidgeDirection = Vertex.RidgeDirection;
			OutInitialFoldDirection = Vertex.FoldDirection;
			OutRotationAxis = Motion.Axis;
			OutAngularSpeedRadiansPerStep = Motion.AngularSpeedRadiansPerStep;
			UpdateLastHash();
			return true;
		}
	}
	return false;
}

bool ACarrierLabVisualizationActor::GetPhaseIIIA3VectorAuditProbe(
	const int32 PlateId,
	const int32 LocalVertexId,
	FVector3d& OutPosition,
	FVector3d& OutRidgeDirection,
	FVector3d& OutFoldDirection) const
{
	OutPosition = FVector3d::ZeroVector;
	OutRidgeDirection = FVector3d::ZeroVector;
	OutFoldDirection = FVector3d::ZeroVector;
	if (!State.Plates.IsValidIndex(PlateId) || !State.Plates[PlateId].Vertices.IsValidIndex(LocalVertexId))
	{
		return false;
	}

	const CarrierLab::FCarrierVertex& Vertex = State.Plates[PlateId].Vertices[LocalVertexId];
	OutPosition = Vertex.UnitPosition;
	OutRidgeDirection = Vertex.RidgeDirection;
	OutFoldDirection = Vertex.FoldDirection;
	return true;
}

bool ACarrierLabVisualizationActor::SeedPhaseIIIA4BoundarySmearProbe(
	const int32 PreferredPlateId,
	FCarrierLabPhaseIIIA4SeedMetrics& OutSeedMetrics)
{
	OutSeedMetrics = FCarrierLabPhaseIIIA4SeedMetrics();
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}

	int32 ChosenPlateId = INDEX_NONE;
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		if (PreferredPlateId != INDEX_NONE && Plate.PlateId != PreferredPlateId)
		{
			continue;
		}

		int32 BoundaryTriangleCount = 0;
		int32 SeedableVertexCount = 0;
		int32 ZeroedVertexCount = 0;
		for (const CarrierLab::FCarrierPlateTriangle& Triangle : Plate.LocalTriangles)
		{
			if (Triangle.bBoundary)
			{
				++BoundaryTriangleCount;
			}
		}
		for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			const bool bSourceOwnedByPlate =
				State.Samples.IsValidIndex(Vertex.GlobalSampleId) &&
				State.Samples[Vertex.GlobalSampleId].PlateId == Plate.PlateId;
			if (bSourceOwnedByPlate)
			{
				++SeedableVertexCount;
			}
			else
			{
				++ZeroedVertexCount;
			}
		}

		if (BoundaryTriangleCount > 0 && SeedableVertexCount > 0 && ZeroedVertexCount > 0)
		{
			ChosenPlateId = Plate.PlateId;
			OutSeedMetrics.BoundaryTriangleCount = BoundaryTriangleCount;
			break;
		}
	}

	if (ChosenPlateId == INDEX_NONE || !State.Plates.IsValidIndex(ChosenPlateId))
	{
		return false;
	}

	CarrierLab::FCarrierPlate& Plate = State.Plates[ChosenPlateId];
	OutSeedMetrics.PlateId = ChosenPlateId;
	for (CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
	{
		const bool bSourceOwnedByPlate =
			State.Samples.IsValidIndex(Vertex.GlobalSampleId) &&
			State.Samples[Vertex.GlobalSampleId].PlateId == Plate.PlateId;
		if (!bSourceOwnedByPlate)
		{
			Vertex.Elevation = 0.0;
			Vertex.OceanicAge = 0.0;
			Vertex.RidgeDirection = FVector3d::ZeroVector;
			Vertex.FoldDirection = FVector3d::ZeroVector;
			++OutSeedMetrics.ZeroedVertexCount;
			continue;
		}

		const FVector3d UnitPosition = Vertex.UnitPosition.GetSafeNormal();
		const FVector3d RidgeDirection = MakeDeterministicTangent(UnitPosition);
		const FVector3d FoldDirection = RetangentAndNormalizeVectorField(
			FVector3d::CrossProduct(UnitPosition, RidgeDirection),
			UnitPosition);
		Vertex.Elevation = OutSeedMetrics.SeedElevation;
		Vertex.OceanicAge = OutSeedMetrics.SeedOceanicAge;
		Vertex.RidgeDirection = RidgeDirection;
		Vertex.FoldDirection = FoldDirection;
		++OutSeedMetrics.SeededVertexCount;
	}

	UpdateLastHash();
	return true;
}

bool ACarrierLabVisualizationActor::GetPhaseIIIA4FieldAudit(
	const double SeedElevation,
	FCarrierLabPhaseIIIA4FieldAudit& OutAudit) const
{
	OutAudit = FCarrierLabPhaseIIIA4FieldAudit();
	if (!bInitialized)
	{
		return false;
	}

	constexpr double NonZeroEpsilon = 1.0e-12;
	double PositiveElevationSum = 0.0;
	int32 PositiveElevationCount = 0;
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		++OutAudit.SampleCount;
		const double RidgeMagnitude = Sample.RidgeDirection.Size();
		const double FoldMagnitude = Sample.FoldDirection.Size();
		const double MaxVectorMagnitude = FMath::Max(RidgeMagnitude, FoldMagnitude);
		const double MaxRadialDot = FMath::Max(
			FMath::Abs(FVector3d::DotProduct(Sample.UnitPosition, Sample.RidgeDirection)),
			FMath::Abs(FVector3d::DotProduct(Sample.UnitPosition, Sample.FoldDirection)));
		OutAudit.MaxAbsSampleElevation = FMath::Max(OutAudit.MaxAbsSampleElevation, FMath::Abs(Sample.Elevation));
		OutAudit.MaxAbsSampleOceanicAge = FMath::Max(OutAudit.MaxAbsSampleOceanicAge, FMath::Abs(Sample.OceanicAge));
		OutAudit.MaxSampleVectorMagnitude = FMath::Max(OutAudit.MaxSampleVectorMagnitude, MaxVectorMagnitude);
		OutAudit.MaxSampleVectorRadialDot = FMath::Max(OutAudit.MaxSampleVectorRadialDot, MaxRadialDot);
		if (FMath::Abs(Sample.Elevation) > NonZeroEpsilon ||
			FMath::Abs(Sample.OceanicAge) > NonZeroEpsilon ||
			MaxVectorMagnitude > NonZeroEpsilon)
		{
			++OutAudit.NonZeroSampleFieldCount;
		}
		if (Sample.Elevation > NonZeroEpsilon)
		{
			OutAudit.MinPositiveSampleElevation = OutAudit.MinPositiveSampleElevation <= 0.0
				? Sample.Elevation
				: FMath::Min(OutAudit.MinPositiveSampleElevation, Sample.Elevation);
			OutAudit.MaxPositiveSampleElevation = FMath::Max(OutAudit.MaxPositiveSampleElevation, Sample.Elevation);
			PositiveElevationSum += Sample.Elevation;
			++PositiveElevationCount;
			if (Sample.Elevation < SeedElevation - NonZeroEpsilon)
			{
				++OutAudit.SmearedSampleCount;
			}
		}
	}
	OutAudit.MeanPositiveSampleElevation = PositiveElevationCount > 0
		? PositiveElevationSum / static_cast<double>(PositiveElevationCount)
		: 0.0;

	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			++OutAudit.PlateVertexCount;
			const double RidgeMagnitude = Vertex.RidgeDirection.Size();
			const double FoldMagnitude = Vertex.FoldDirection.Size();
			const double MaxVectorMagnitude = FMath::Max(RidgeMagnitude, FoldMagnitude);
			const double MaxRadialDot = FMath::Max(
				FMath::Abs(FVector3d::DotProduct(Vertex.UnitPosition, Vertex.RidgeDirection)),
				FMath::Abs(FVector3d::DotProduct(Vertex.UnitPosition, Vertex.FoldDirection)));
			OutAudit.MaxAbsPlateVertexElevation = FMath::Max(OutAudit.MaxAbsPlateVertexElevation, FMath::Abs(Vertex.Elevation));
			OutAudit.MaxAbsPlateVertexOceanicAge = FMath::Max(OutAudit.MaxAbsPlateVertexOceanicAge, FMath::Abs(Vertex.OceanicAge));
			OutAudit.MaxPlateVertexVectorMagnitude = FMath::Max(OutAudit.MaxPlateVertexVectorMagnitude, MaxVectorMagnitude);
			OutAudit.MaxPlateVertexVectorRadialDot = FMath::Max(OutAudit.MaxPlateVertexVectorRadialDot, MaxRadialDot);
			if (FMath::Abs(Vertex.Elevation) > NonZeroEpsilon ||
				FMath::Abs(Vertex.OceanicAge) > NonZeroEpsilon ||
				MaxVectorMagnitude > NonZeroEpsilon)
			{
				++OutAudit.NonZeroPlateVertexFieldCount;
			}
		}
	}
	return true;
}

bool ACarrierLabVisualizationActor::GetPhaseIIIA4FieldAuditForSamples(
	const TArray<int32>& SampleIds,
	const double SeedElevation,
	FCarrierLabPhaseIIIA4FieldAudit& OutAudit) const
{
	OutAudit = FCarrierLabPhaseIIIA4FieldAudit();
	if (!bInitialized)
	{
		return false;
	}

	constexpr double NonZeroEpsilon = 1.0e-12;
	double PositiveElevationSum = 0.0;
	int32 PositiveElevationCount = 0;
	for (const int32 SampleId : SampleIds)
	{
		if (!State.Samples.IsValidIndex(SampleId))
		{
			continue;
		}

		const CarrierLab::FSphereSample& Sample = State.Samples[SampleId];
		++OutAudit.SampleCount;
		const double RidgeMagnitude = Sample.RidgeDirection.Size();
		const double FoldMagnitude = Sample.FoldDirection.Size();
		const double MaxVectorMagnitude = FMath::Max(RidgeMagnitude, FoldMagnitude);
		const double MaxRadialDot = FMath::Max(
			FMath::Abs(FVector3d::DotProduct(Sample.UnitPosition, Sample.RidgeDirection)),
			FMath::Abs(FVector3d::DotProduct(Sample.UnitPosition, Sample.FoldDirection)));
		OutAudit.MaxAbsSampleElevation = FMath::Max(OutAudit.MaxAbsSampleElevation, FMath::Abs(Sample.Elevation));
		OutAudit.MaxAbsSampleOceanicAge = FMath::Max(OutAudit.MaxAbsSampleOceanicAge, FMath::Abs(Sample.OceanicAge));
		OutAudit.MaxSampleVectorMagnitude = FMath::Max(OutAudit.MaxSampleVectorMagnitude, MaxVectorMagnitude);
		OutAudit.MaxSampleVectorRadialDot = FMath::Max(OutAudit.MaxSampleVectorRadialDot, MaxRadialDot);
		if (FMath::Abs(Sample.Elevation) > NonZeroEpsilon ||
			FMath::Abs(Sample.OceanicAge) > NonZeroEpsilon ||
			MaxVectorMagnitude > NonZeroEpsilon)
		{
			++OutAudit.NonZeroSampleFieldCount;
		}
		if (Sample.Elevation > NonZeroEpsilon)
		{
			OutAudit.MinPositiveSampleElevation = OutAudit.MinPositiveSampleElevation <= 0.0
				? Sample.Elevation
				: FMath::Min(OutAudit.MinPositiveSampleElevation, Sample.Elevation);
			OutAudit.MaxPositiveSampleElevation = FMath::Max(OutAudit.MaxPositiveSampleElevation, Sample.Elevation);
			PositiveElevationSum += Sample.Elevation;
			++PositiveElevationCount;
			if (Sample.Elevation < SeedElevation - NonZeroEpsilon)
			{
				++OutAudit.SmearedSampleCount;
			}
		}
	}
	OutAudit.MeanPositiveSampleElevation = PositiveElevationCount > 0
		? PositiveElevationSum / static_cast<double>(PositiveElevationCount)
		: 0.0;
	return true;
}

bool ACarrierLabVisualizationActor::GetPhaseIIIB1TrackingAudit(FCarrierLabPhaseIIIB1TrackingAudit& OutAudit) const
{
	OutAudit = FCarrierLabPhaseIIIB1TrackingAudit();
	if (!bInitialized)
	{
		return false;
	}

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCount = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerial = State.ConvergenceTrackingResetSerial;

	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		TSet<int32> SeenActiveTriangles;
		for (int32 LocalTriangleId = 0; LocalTriangleId < Plate.LocalTriangles.Num(); ++LocalTriangleId)
		{
			const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
			if (!Triangle.bBoundary)
			{
				continue;
			}

			++OutAudit.SourceBoundaryTriangleCount;
			if (!Plate.ActiveBoundaryTriangles.Contains(LocalTriangleId))
			{
				++OutAudit.MissingBoundaryTriangleCount;
			}
		}

		if (Plate.ActiveBoundaryTriangles.IsEmpty())
		{
			++OutAudit.EmptyActivePlateCount;
		}

		for (const int32 LocalTriangleId : Plate.ActiveBoundaryTriangles)
		{
			++OutAudit.ActiveBoundaryTriangleCount;
			if (SeenActiveTriangles.Contains(LocalTriangleId))
			{
				++OutAudit.DuplicateActiveTriangleCount;
			}
			SeenActiveTriangles.Add(LocalTriangleId);

			if (!Plate.LocalTriangles.IsValidIndex(LocalTriangleId))
			{
				++OutAudit.InvalidActiveTriangleCount;
				continue;
			}

			if (!Plate.LocalTriangles[LocalTriangleId].bBoundary)
			{
				++OutAudit.NonBoundaryActiveTriangleCount;
			}
		}
	}

	OutAudit.ConvergenceTrackingHash = HashToString(ComputeConvergenceTrackingHash(State));
	return true;
}

bool ACarrierLabVisualizationActor::GetPhaseIIIB2DistanceAudit(FCarrierLabPhaseIIIB2DistanceAudit& OutAudit) const
{
	OutAudit = FCarrierLabPhaseIIIB2DistanceAudit();
	if (!bInitialized)
	{
		return false;
	}

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCount = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	OutAudit.DistanceThresholdKm = PhaseIIIBDistanceToFrontLimitKm;
	OutAudit.DistanceCulledTriangleCount = State.ConvergenceTrackingDistanceCullCount;

	double DistanceSumKm = 0.0;
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		for (const CarrierLab::FCarrierPlateTriangle& Triangle : Plate.LocalTriangles)
		{
			if (Triangle.bBoundary)
			{
				++OutAudit.SourceBoundaryTriangleCount;
			}
		}

		if (Plate.ActiveBoundaryTriangles.IsEmpty())
		{
			++OutAudit.EmptyActivePlateCount;
		}

		for (int32 ActiveIndex = 0; ActiveIndex < Plate.ActiveBoundaryTriangles.Num(); ++ActiveIndex)
		{
			const int32 LocalTriangleId = Plate.ActiveBoundaryTriangles[ActiveIndex];
			++OutAudit.ActiveBoundaryTriangleCount;
			if (!Plate.ActiveBoundaryTriangleDistancesKm.IsValidIndex(ActiveIndex))
			{
				++OutAudit.MissingDistanceRecordCount;
				continue;
			}

			const double DistanceKm = Plate.ActiveBoundaryTriangleDistancesKm[ActiveIndex];
			++OutAudit.DistanceRecordCount;
			if (!FMath::IsFinite(DistanceKm))
			{
				++OutAudit.NonFiniteDistanceCount;
				continue;
			}
			if (DistanceKm < 0.0)
			{
				++OutAudit.NegativeDistanceCount;
			}
			if (DistanceKm > PhaseIIIBDistanceToFrontLimitKm)
			{
				++OutAudit.OverThresholdActiveTriangleCount;
			}

			if (OutAudit.DistanceRecordCount == 1)
			{
				OutAudit.MinDistanceKm = DistanceKm;
				OutAudit.MaxDistanceKm = DistanceKm;
			}
			else
			{
				OutAudit.MinDistanceKm = FMath::Min(OutAudit.MinDistanceKm, DistanceKm);
				OutAudit.MaxDistanceKm = FMath::Max(OutAudit.MaxDistanceKm, DistanceKm);
			}
			DistanceSumKm += DistanceKm;

			if (OutAudit.ProbePlateId == INDEX_NONE &&
				Plate.LocalTriangles.IsValidIndex(LocalTriangleId) &&
				Motions.IsValidIndex(Plate.PlateId))
			{
				OutAudit.ProbePlateId = Plate.PlateId;
				OutAudit.ProbeLocalTriangleId = LocalTriangleId;
				OutAudit.ProbeDistanceKm = DistanceKm;
				OutAudit.ProbeStepDistanceKm = ComputeActiveTriangleDistanceStepKm(Plate, LocalTriangleId, Motions[Plate.PlateId]);
			}
		}
	}

	OutAudit.MeanDistanceKm = OutAudit.DistanceRecordCount > 0
		? DistanceSumKm / static_cast<double>(OutAudit.DistanceRecordCount)
		: 0.0;
	OutAudit.ConvergenceTrackingHash = HashToString(ComputeConvergenceTrackingHash(State));
	return true;
}

bool ACarrierLabVisualizationActor::GetPhaseIIIB3SubductionMatrixAudit(FCarrierLabPhaseIIIB3SubductionMatrixAudit& OutAudit) const
{
	OutAudit = FCarrierLabPhaseIIIB3SubductionMatrixAudit();
	if (!bInitialized)
	{
		return false;
	}

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCount = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	OutAudit.RayTestCount = State.ConvergenceSubductionMatrixRayTestCount;
	OutAudit.HitCount = State.ConvergenceSubductionMatrixHitCount;
	OutAudit.BoundaryHitCount = State.ConvergenceSubductionMatrixBoundaryHitCount;
	OutAudit.NonConvergentHitCount = State.ConvergenceSubductionMatrixNonConvergentHitCount;

	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		OutAudit.ActiveBoundaryTriangleCount += Plate.ActiveBoundaryTriangles.Num();
		OutAudit.DistanceRecordCount += Plate.ActiveBoundaryTriangleDistancesKm.Num();
	}

	TArray<uint64> PairKeys = State.ConvergenceSubductionMatrixPairKeys.Array();
	PairKeys.Sort();
	OutAudit.MatrixPairCount = PairKeys.Num();
	for (const uint64 PairKey : PairKeys)
	{
		int32 PlateA = INDEX_NONE;
		int32 PlateB = INDEX_NONE;
		DecodePlatePairKey(PairKey, PlateA, PlateB);
		if (PlateA == PlateB)
		{
			++OutAudit.SelfMatrixPairCount;
		}
		if (!State.Plates.IsValidIndex(PlateA) || !State.Plates.IsValidIndex(PlateB))
		{
			++OutAudit.InvalidMatrixPairCount;
			continue;
		}
		if (OutAudit.ProbePlateA == INDEX_NONE)
		{
			OutAudit.ProbePlateA = PlateA;
			OutAudit.ProbePlateB = PlateB;
		}
	}
	for (const CarrierLab::FConvergenceSubductionTriangleHit& TriangleHit : State.ConvergenceSubductionTriangleHits)
	{
		if (!State.Plates.IsValidIndex(TriangleHit.PlateId) ||
			!State.Plates[TriangleHit.PlateId].LocalTriangles.IsValidIndex(TriangleHit.LocalTriangleId))
		{
			continue;
		}

		const CarrierLab::FCarrierPlate& Plate = State.Plates[TriangleHit.PlateId];
		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[TriangleHit.LocalTriangleId];
		if (!Plate.Vertices.IsValidIndex(Triangle.A) ||
			!Plate.Vertices.IsValidIndex(Triangle.B) ||
			!Plate.Vertices.IsValidIndex(Triangle.C))
		{
			continue;
		}

		const FVector3d Barycenter = NormalizeOrFallback(
			Plate.Vertices[Triangle.A].UnitPosition +
			Plate.Vertices[Triangle.B].UnitPosition +
			Plate.Vertices[Triangle.C].UnitPosition,
			Plate.Vertices[Triangle.A].UnitPosition);
		OutAudit.ProbeSignedConvergenceVelocity =
			-SignedPairSeparationVelocityForPlatePair(Barycenter, Motions, TriangleHit.PlateId, TriangleHit.OtherPlateId);
		break;
	}

	uint64 MatrixEvidenceHash = 1469598103934665603ull;
	HashMix(MatrixEvidenceHash, static_cast<uint64>(State.ConvergenceSubductionMatrixEvidence.Num() + 1));
	for (const CarrierLab::FConvergenceSubductionMatrixEvidence& Evidence : State.ConvergenceSubductionMatrixEvidence)
	{
		HashMix(MatrixEvidenceHash, static_cast<uint64>(Evidence.EvidenceId + 1));
		HashMix(MatrixEvidenceHash, static_cast<uint64>(Evidence.ContactId + 1));
		HashMix(MatrixEvidenceHash, Evidence.PairKey + 1ull);
		HashMix(MatrixEvidenceHash, static_cast<uint64>(Evidence.PlateId + 1));
		HashMix(MatrixEvidenceHash, static_cast<uint64>(Evidence.OtherPlateId + 1));
		HashMix(MatrixEvidenceHash, static_cast<uint64>(Evidence.LocalTriangleId + 1));
		HashMix(MatrixEvidenceHash, static_cast<uint64>(Evidence.OtherLocalTriangleId + 1));
		HashMixDouble(MatrixEvidenceHash, Evidence.SignedConvergenceVelocity);
		HashMix(MatrixEvidenceHash, Evidence.bAccepted ? 1ull : 0ull);

		if (Evidence.bAccepted && Evidence.SignedConvergenceVelocity > PhaseIIContactVelocityMargin)
		{
			++OutAudit.AcceptedLocalPositiveHitCount;
			if (OutAudit.AcceptedEvidence.Num() < 8)
			{
				OutAudit.AcceptedEvidence.Add(Evidence);
			}
		}
		else if (!Evidence.bAccepted && Evidence.SignedConvergenceVelocity <= PhaseIIContactVelocityMargin)
		{
			++OutAudit.RejectedLocalNonPositiveHitCount;
			if (OutAudit.RejectedEvidence.Num() < 8)
			{
				OutAudit.RejectedEvidence.Add(Evidence);
			}
		}
	}

	OutAudit.MatrixEvidenceHash = HashToString(MatrixEvidenceHash);
	OutAudit.ConvergenceTrackingHash = HashToString(ComputeConvergenceTrackingHash(State));
	return true;
}

bool ACarrierLabVisualizationActor::GetPhaseIIIB4PolarityAudit(FCarrierLabPhaseIIIB4PolarityAudit& OutAudit) const
{
	OutAudit = FCarrierLabPhaseIIIB4PolarityAudit();
	if (!bInitialized)
	{
		return false;
	}

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCount = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	OutAudit.MatrixPairCount = State.ConvergenceSubductionMatrixPairKeys.Num();

	TSet<uint64> DecisionKeys;
	for (const CarrierLab::FConvergenceSubductionPolarityDecision& Decision : State.ConvergenceSubductionPolarityDecisions)
	{
		DecisionKeys.Add(Decision.PairKey);
		FCarrierLabPhaseIIIB4PolarityDecisionAudit& AuditDecision = OutAudit.Decisions.AddDefaulted_GetRef();
		AuditDecision.PairKey = Decision.PairKey;
		AuditDecision.PlateA = Decision.PlateA;
		AuditDecision.PlateB = Decision.PlateB;
		AuditDecision.UnderPlate = Decision.UnderPlate;
		AuditDecision.OverPlate = Decision.OverPlate;
		AuditDecision.PlateAContinentalFraction = Decision.PlateAContinentalFraction;
		AuditDecision.PlateBContinentalFraction = Decision.PlateBContinentalFraction;
		AuditDecision.PlateAOceanicAge = Decision.PlateAOceanicAge;
		AuditDecision.PlateBOceanicAge = Decision.PlateBOceanicAge;
		AuditDecision.DecisionClass = Decision.DecisionClass;

		switch (Decision.DecisionClass)
		{
		case CarrierLab::EConvergenceSubductionPolarityClass::OceanicUnderContinental:
			++OutAudit.OceanicUnderContinentalCount;
			++OutAudit.SubductionPolarityCount;
			break;
		case CarrierLab::EConvergenceSubductionPolarityClass::CollisionCandidate:
			++OutAudit.CollisionCandidateCount;
			break;
		case CarrierLab::EConvergenceSubductionPolarityClass::OceanOceanDeferred:
			++OutAudit.OceanOceanDeferredCount;
			break;
		case CarrierLab::EConvergenceSubductionPolarityClass::OlderOceanicUnderYoungerOceanic:
			++OutAudit.OlderOceanicUnderYoungerOceanicCount;
			++OutAudit.SubductionPolarityCount;
			break;
		case CarrierLab::EConvergenceSubductionPolarityClass::Invalid:
			++OutAudit.InvalidDecisionCount;
			break;
		case CarrierLab::EConvergenceSubductionPolarityClass::None:
		default:
			break;
		}

		if (OutAudit.ProbePlateA == INDEX_NONE)
		{
			OutAudit.ProbePlateA = Decision.PlateA;
			OutAudit.ProbePlateB = Decision.PlateB;
			OutAudit.ProbeUnderPlate = Decision.UnderPlate;
			OutAudit.ProbeOverPlate = Decision.OverPlate;
			OutAudit.ProbePlateAContinentalFraction = Decision.PlateAContinentalFraction;
			OutAudit.ProbePlateBContinentalFraction = Decision.PlateBContinentalFraction;
			OutAudit.ProbePlateAOceanicAge = Decision.PlateAOceanicAge;
			OutAudit.ProbePlateBOceanicAge = Decision.PlateBOceanicAge;
			OutAudit.ProbeDecisionClass = Decision.DecisionClass;
		}
	}
	OutAudit.DecisionCount = OutAudit.Decisions.Num();

	for (const uint64 PairKey : State.ConvergenceSubductionMatrixPairKeys)
	{
		if (!DecisionKeys.Contains(PairKey))
		{
			++OutAudit.MissingDecisionCount;
		}
	}

	uint64 PolarityHash = 1469598103934665603ull;
	TArray<FCarrierLabPhaseIIIB4PolarityDecisionAudit> SortedDecisions = OutAudit.Decisions;
	SortedDecisions.Sort([](
		const FCarrierLabPhaseIIIB4PolarityDecisionAudit& A,
		const FCarrierLabPhaseIIIB4PolarityDecisionAudit& B)
	{
		return A.PairKey < B.PairKey;
	});
	HashMix(PolarityHash, static_cast<uint64>(SortedDecisions.Num() + 1));
	for (const FCarrierLabPhaseIIIB4PolarityDecisionAudit& Decision : SortedDecisions)
	{
		HashMix(PolarityHash, Decision.PairKey + 1ull);
		HashMix(PolarityHash, static_cast<uint64>(Decision.PlateA + 1));
		HashMix(PolarityHash, static_cast<uint64>(Decision.PlateB + 1));
		HashMix(PolarityHash, static_cast<uint64>(Decision.UnderPlate + 1));
		HashMix(PolarityHash, static_cast<uint64>(Decision.OverPlate + 1));
		HashMixDouble(PolarityHash, Decision.PlateAContinentalFraction);
		HashMixDouble(PolarityHash, Decision.PlateBContinentalFraction);
		HashMixDouble(PolarityHash, Decision.PlateAOceanicAge);
		HashMixDouble(PolarityHash, Decision.PlateBOceanicAge);
		HashMix(PolarityHash, static_cast<uint64>(Decision.DecisionClass) + 1ull);
	}
	OutAudit.PolarityHash = HashToString(PolarityHash);
	OutAudit.ConvergenceTrackingHash = HashToString(ComputeConvergenceTrackingHash(State));
	return true;
}

bool ACarrierLabVisualizationActor::GetPhaseIIIB6NeighborPropagationAudit(FCarrierLabPhaseIIIB6NeighborPropagationAudit& OutAudit) const
{
	OutAudit = FCarrierLabPhaseIIIB6NeighborPropagationAudit();
	if (!bInitialized)
	{
		return false;
	}

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCount = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	OutAudit.DistanceThresholdKm = PhaseIIIBDistanceToFrontLimitKm;
	OutAudit.PropagationSeedHitCount = State.ConvergenceNeighborPropagationSeedCount;
	OutAudit.PropagationAddedCount = State.ConvergenceNeighborPropagationAddedCount;
	OutAudit.PropagationDuplicateCount = State.ConvergenceNeighborPropagationDuplicateCount;
	OutAudit.PropagationDistanceRejectedCount = State.ConvergenceNeighborPropagationDistanceRejectedCount;
	OutAudit.PropagationInvalidCount = State.ConvergenceNeighborPropagationInvalidCount;

	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		OutAudit.TotalPlateLocalTriangleCount += Plate.LocalTriangles.Num();
		if (!Plate.ActiveBoundaryTriangles.IsEmpty())
		{
			++OutAudit.ActivePlateCount;
			OutAudit.MaxActiveTrianglesOnPlate = FMath::Max(OutAudit.MaxActiveTrianglesOnPlate, Plate.ActiveBoundaryTriangles.Num());
		}

		for (int32 ActiveIndex = 0; ActiveIndex < Plate.ActiveBoundaryTriangles.Num(); ++ActiveIndex)
		{
			const int32 LocalTriangleId = Plate.ActiveBoundaryTriangles[ActiveIndex];
			++OutAudit.ActiveTriangleCount;
			if (Plate.ActiveBoundaryTriangleDistancesKm.IsValidIndex(ActiveIndex))
			{
				++OutAudit.DistanceRecordCount;
				const double DistanceKm = Plate.ActiveBoundaryTriangleDistancesKm[ActiveIndex];
				OutAudit.MaxDistanceKm = FMath::Max(OutAudit.MaxDistanceKm, DistanceKm);
				if (DistanceKm > PhaseIIIBDistanceToFrontLimitKm)
				{
					++OutAudit.OverThresholdActiveTriangleCount;
				}
				if (OutAudit.ProbePlateId == INDEX_NONE)
				{
					OutAudit.ProbePlateId = Plate.PlateId;
					OutAudit.ProbeLocalTriangleId = LocalTriangleId;
					OutAudit.ProbeDistanceKm = DistanceKm;
				}
			}

			if (Plate.LocalTriangles.IsValidIndex(LocalTriangleId) &&
				!Plate.LocalTriangles[LocalTriangleId].bBoundary)
			{
				++OutAudit.NonBoundaryActiveTriangleCount;
			}
		}
	}

	TMap<uint64, CarrierLab::FConvergenceSubductionPolarityDecision> DecisionsByPair;
	for (const CarrierLab::FConvergenceSubductionPolarityDecision& Decision : State.ConvergenceSubductionPolarityDecisions)
	{
		DecisionsByPair.Add(Decision.PairKey, Decision);
	}
	for (const CarrierLab::FConvergenceSubductionTriangleHit& Hit : State.ConvergenceSubductionTriangleHits)
	{
		const CarrierLab::FConvergenceSubductionPolarityDecision* Decision = DecisionsByPair.Find(Hit.PairKey);
		if (Decision == nullptr ||
			!IsSubductionPolarityDecision(*Decision) ||
			Decision->UnderPlate != Hit.PlateId ||
			!State.Plates.IsValidIndex(Hit.PlateId) ||
			!State.Plates[Hit.PlateId].LocalTriangles.IsValidIndex(Hit.LocalTriangleId))
		{
			continue;
		}

		OutAudit.SeedEvidenceId = Hit.EvidenceId;
		OutAudit.SeedPlateId = Hit.PlateId;
		OutAudit.SeedOtherPlateId = Hit.OtherPlateId;
		OutAudit.SeedLocalTriangleId = Hit.LocalTriangleId;
		OutAudit.SeedSignedConvergenceVelocity = Hit.SignedConvergenceVelocity;
		break;
	}

	OutAudit.ConvergenceTrackingHash = HashToString(ComputeConvergenceTrackingHash(State));
	return true;
}

bool ACarrierLabVisualizationActor::GetPhaseIIIB7HashClosureAudit(FCarrierLabPhaseIIIB7HashClosureAudit& OutAudit) const
{
	OutAudit = FCarrierLabPhaseIIIB7HashClosureAudit();
	if (!bInitialized)
	{
		return false;
	}

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCount = CurrentMetrics.EventCount;
	OutAudit.SampleCount = State.Samples.Num();
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	OutAudit.MatrixPairCount = State.ConvergenceSubductionMatrixPairKeys.Num();
	OutAudit.MatrixRayTestCount = State.ConvergenceSubductionMatrixRayTestCount;
	OutAudit.MatrixHitCount = State.ConvergenceSubductionMatrixHitCount;
	OutAudit.MatrixBoundaryHitCount = State.ConvergenceSubductionMatrixBoundaryHitCount;
	OutAudit.MatrixNonConvergentHitCount = State.ConvergenceSubductionMatrixNonConvergentHitCount;
	OutAudit.PolarityDecisionCount = State.ConvergenceSubductionPolarityDecisions.Num();
	OutAudit.TriangleHitCount = State.ConvergenceSubductionTriangleHits.Num();
	OutAudit.PropagationSeedHitCount = State.ConvergenceNeighborPropagationSeedCount;
	OutAudit.PropagationAddedCount = State.ConvergenceNeighborPropagationAddedCount;
	OutAudit.PropagationDuplicateCount = State.ConvergenceNeighborPropagationDuplicateCount;
	OutAudit.PropagationDistanceRejectedCount = State.ConvergenceNeighborPropagationDistanceRejectedCount;
	OutAudit.PropagationInvalidCount = State.ConvergenceNeighborPropagationInvalidCount;
	OutAudit.ProjectionHash = CurrentMetrics.LastHash;
	OutAudit.StateHash = CurrentMetrics.StateHash;
	OutAudit.CrustStateHash = CurrentMetrics.CrustStateHash;
	OutAudit.MetricsConvergenceTrackingHash = CurrentMetrics.ConvergenceTrackingHash;
	OutAudit.ComputedConvergenceTrackingHash = HashToString(ComputeConvergenceTrackingHash(State));
	OutAudit.bMetricsHashMatchesComputed =
		OutAudit.MetricsConvergenceTrackingHash == OutAudit.ComputedConvergenceTrackingHash;

	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		OutAudit.ActiveTriangleCount += Plate.ActiveBoundaryTriangles.Num();
		OutAudit.DistanceRecordCount += Plate.ActiveBoundaryTriangleDistancesKm.Num();
	}

	return true;
}

bool ACarrierLabVisualizationActor::GetPhaseIIIC1SubductingMarkAudit(FCarrierLabPhaseIIIC1SubductingMarkAudit& OutAudit) const
{
	OutAudit = FCarrierLabPhaseIIIC1SubductingMarkAudit();
	if (!bInitialized)
	{
		return false;
	}

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCount = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	OutAudit.bEnabled = bEnablePhaseIIICSubductingMarks;
	OutAudit.MarkCount = State.ConvergenceSubductingTriangleMarks.Num();
	OutAudit.DuplicateMarkCount = State.ConvergenceSubductingTriangleMarkDuplicateCount;
	OutAudit.InvalidMarkCount = State.ConvergenceSubductingTriangleMarkInvalidCount;

	TMap<uint64, CarrierLab::FConvergenceSubductionPolarityDecision> DecisionsByPair;
	for (const CarrierLab::FConvergenceSubductionPolarityDecision& Decision : State.ConvergenceSubductionPolarityDecisions)
	{
		DecisionsByPair.Add(Decision.PairKey, Decision);
	}

	TSet<uint64> SeenTriangleKeys;
	TArray<CarrierLab::FConvergenceSubductingTriangleMark> SortedMarks = State.ConvergenceSubductingTriangleMarks;
	SortedMarks.Sort([](
		const CarrierLab::FConvergenceSubductingTriangleMark& A,
		const CarrierLab::FConvergenceSubductingTriangleMark& B)
	{
		if (A.PlateId != B.PlateId)
		{
			return A.PlateId < B.PlateId;
		}
		if (A.LocalTriangleId != B.LocalTriangleId)
		{
			return A.LocalTriangleId < B.LocalTriangleId;
		}
		return A.MarkId < B.MarkId;
	});

	uint64 MarkHash = 1469598103934665603ull;
	HashMix(MarkHash, static_cast<uint64>(OutAudit.Step + 1));
	HashMix(MarkHash, static_cast<uint64>(OutAudit.EventCount + 1));
	HashMix(MarkHash, static_cast<uint64>(OutAudit.ResetSerial + 1));
	HashMix(MarkHash, OutAudit.bEnabled ? 1ull : 0ull);
	HashMix(MarkHash, static_cast<uint64>(SortedMarks.Num() + 1));
	for (const CarrierLab::FConvergenceSubductingTriangleMark& Mark : SortedMarks)
	{
		const uint64 TriangleKey = MakePlateTriangleKey(Mark.PlateId, Mark.LocalTriangleId);
		if (SeenTriangleKeys.Contains(TriangleKey))
		{
			++OutAudit.DuplicateMarkCount;
		}
		SeenTriangleKeys.Add(TriangleKey);
		if (!State.Plates.IsValidIndex(Mark.PlateId) ||
			!State.Plates[Mark.PlateId].LocalTriangles.IsValidIndex(Mark.LocalTriangleId))
		{
			++OutAudit.InvalidMarkCount;
		}

		const CarrierLab::FConvergenceSubductionPolarityDecision* Decision = DecisionsByPair.Find(Mark.PairKey);
		if (Decision == nullptr || !IsSubductionPolarityDecision(*Decision))
		{
			++OutAudit.NonSubductionDecisionCount;
		}
		else if (Decision->UnderPlate != Mark.PlateId)
		{
			++OutAudit.UnderPlateMismatchCount;
		}

		HashMix(MarkHash, static_cast<uint64>(Mark.MarkId + 1));
		HashMix(MarkHash, Mark.PairKey + 1ull);
		HashMix(MarkHash, static_cast<uint64>(Mark.PlateId + 1));
		HashMix(MarkHash, static_cast<uint64>(Mark.OtherPlateId + 1));
		HashMix(MarkHash, static_cast<uint64>(Mark.LocalTriangleId + 1));
		HashMix(MarkHash, static_cast<uint64>(Mark.EvidenceId + 1));
		HashMixDouble(MarkHash, Mark.SignedConvergenceVelocity);
		HashMix(MarkHash, static_cast<uint64>(Mark.DecisionClass) + 1ull);

		if (OutAudit.Records.Num() < 16)
		{
			FCarrierLabPhaseIIIC1SubductingMarkAuditRecord& Record = OutAudit.Records.AddDefaulted_GetRef();
			Record.MarkId = Mark.MarkId;
			Record.PairKey = Mark.PairKey;
			Record.PlateId = Mark.PlateId;
			Record.OtherPlateId = Mark.OtherPlateId;
			Record.LocalTriangleId = Mark.LocalTriangleId;
			Record.EvidenceId = Mark.EvidenceId;
			Record.SignedConvergenceVelocity = Mark.SignedConvergenceVelocity;
			Record.DecisionClass = Mark.DecisionClass;
			Record.bHistoricalElevationSnapshotTaken = Mark.bHistoricalElevationSnapshotTaken;
			Record.HistoricalElevationSnapshotVertexCount = Mark.HistoricalElevationSnapshotVertexCount;
			Record.HistoricalElevationSnapshotMin = Mark.HistoricalElevationSnapshotMin;
			Record.HistoricalElevationSnapshotMax = Mark.HistoricalElevationSnapshotMax;
			Record.VisibleElevationAppliedKm = Mark.VisibleElevationAppliedKm;
		}
	}

	OutAudit.SubductingMarkHash = HashToString(MarkHash);
	return true;
}

bool ACarrierLabVisualizationActor::GetPhaseIIIC2ElevationAudit(FCarrierLabPhaseIIIC2ElevationAudit& OutAudit) const
{
	OutAudit = FCarrierLabPhaseIIIC2ElevationAudit();
	if (!bInitialized)
	{
		return false;
	}

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCount = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	OutAudit.bMarksEnabled = bEnablePhaseIIICSubductingMarks;
	OutAudit.bElevationSplitEnabled = bEnablePhaseIIICVisibleHistoricalElevation;
	OutAudit.MarkCount = State.ConvergenceSubductingTriangleMarks.Num();
	OutAudit.DuplicateSnapshotCount = State.ConvergenceHistoricalElevationDuplicateSnapshotCount;
	OutAudit.StateSnapshotCount = State.ConvergenceHistoricalElevationSnapshotCount;
	OutAudit.StateSnapshotVertexCount = State.ConvergenceHistoricalElevationSnapshotVertexCount;
	OutAudit.StateInvalidSnapshotCount = State.ConvergenceHistoricalElevationInvalidSnapshotCount;
	OutAudit.TrenchDepthKm = PhaseIIICTrenchDepthKm;
	OutAudit.VisibleElevationHash = CurrentMetrics.VisibleElevationHash;
	OutAudit.HistoricalElevationHash = CurrentMetrics.HistoricalElevationHash;
	OutAudit.CrustStateHash = CurrentMetrics.CrustStateHash;

	bool bInitializedVisible = false;
	bool bInitializedHistorical = false;
	for (const CarrierLab::FConvergenceSubductingTriangleMark& Mark : State.ConvergenceSubductingTriangleMarks)
	{
		if (!Mark.bHistoricalElevationSnapshotTaken)
		{
			++OutAudit.MissingSnapshotCount;
			continue;
		}
		++OutAudit.SnapshotMarkCount;
		OutAudit.SnapshotVertexCount += Mark.HistoricalElevationSnapshotVertexCount;
		if (!State.Plates.IsValidIndex(Mark.PlateId) ||
			!State.Plates[Mark.PlateId].LocalTriangles.IsValidIndex(Mark.LocalTriangleId))
		{
			++OutAudit.InvalidSnapshotCount;
			continue;
		}

		const CarrierLab::FCarrierPlate& Plate = State.Plates[Mark.PlateId];
		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[Mark.LocalTriangleId];
		const int32 VertexIds[3] = { Triangle.A, Triangle.B, Triangle.C };
		double VisibleMin = TNumericLimits<double>::Max();
		double VisibleMax = -TNumericLimits<double>::Max();
		double HistoricalMin = TNumericLimits<double>::Max();
		double HistoricalMax = -TNumericLimits<double>::Max();
		bool bAllVerticesValid = true;
		for (const int32 VertexId : VertexIds)
		{
			if (!Plate.Vertices.IsValidIndex(VertexId))
			{
				bAllVerticesValid = false;
				break;
			}
			const CarrierLab::FCarrierVertex& Vertex = Plate.Vertices[VertexId];
			VisibleMin = FMath::Min(VisibleMin, Vertex.Elevation);
			VisibleMax = FMath::Max(VisibleMax, Vertex.Elevation);
			HistoricalMin = FMath::Min(HistoricalMin, Vertex.HistoricalElevation);
			HistoricalMax = FMath::Max(HistoricalMax, Vertex.HistoricalElevation);
		}
		if (!bAllVerticesValid)
		{
			++OutAudit.InvalidSnapshotCount;
			continue;
		}

		if (!bInitializedVisible)
		{
			OutAudit.VisibleElevationMin = VisibleMin;
			OutAudit.VisibleElevationMax = VisibleMax;
			bInitializedVisible = true;
		}
		else
		{
			OutAudit.VisibleElevationMin = FMath::Min(OutAudit.VisibleElevationMin, VisibleMin);
			OutAudit.VisibleElevationMax = FMath::Max(OutAudit.VisibleElevationMax, VisibleMax);
		}

		if (!bInitializedHistorical)
		{
			OutAudit.HistoricalElevationMin = HistoricalMin;
			OutAudit.HistoricalElevationMax = HistoricalMax;
			bInitializedHistorical = true;
		}
		else
		{
			OutAudit.HistoricalElevationMin = FMath::Min(OutAudit.HistoricalElevationMin, HistoricalMin);
			OutAudit.HistoricalElevationMax = FMath::Max(OutAudit.HistoricalElevationMax, HistoricalMax);
		}

		if (OutAudit.Records.Num() < 16)
		{
			FCarrierLabPhaseIIIC2ElevationAuditRecord& Record = OutAudit.Records.AddDefaulted_GetRef();
			Record.MarkId = Mark.MarkId;
			Record.PlateId = Mark.PlateId;
			Record.OtherPlateId = Mark.OtherPlateId;
			Record.LocalTriangleId = Mark.LocalTriangleId;
			Record.SnapshotVertexCount = Mark.HistoricalElevationSnapshotVertexCount;
			Record.HistoricalElevationMin = HistoricalMin;
			Record.HistoricalElevationMax = HistoricalMax;
			Record.VisibleElevationMin = VisibleMin;
			Record.VisibleElevationMax = VisibleMax;
			Record.AppliedTrenchDepthKm = Mark.VisibleElevationAppliedKm;
		}
	}

	return true;
}

bool ACarrierLabVisualizationActor::GetPhaseIIIC3UpliftAudit(FCarrierLabPhaseIIIC3UpliftAudit& OutAudit) const
{
	OutAudit = LastPhaseIIIC3UpliftAudit;
	if (!bInitialized)
	{
		return false;
	}

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCount = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	OutAudit.bMarksEnabled = bEnablePhaseIIICSubductingMarks;
	OutAudit.bElevationSplitEnabled = bEnablePhaseIIICVisibleHistoricalElevation;
	OutAudit.bUpliftEnabled = bEnablePhaseIIICOverridingPlateUplift;
	OutAudit.MarkCount = State.ConvergenceSubductingTriangleMarks.Num();
	OutAudit.EffectRadiusKm = PhaseIIICSubductionEffectRadiusKm;
	OutAudit.UpliftRateMmPerYear = PhaseIIICSubductionUpliftMmPerYear;
	OutAudit.ReferenceVelocityMmPerYear = PhaseIIICReferenceVelocityMmPerYear;
	OutAudit.TrenchDepthKm = PhaseIIICTrenchDepthKm;
	OutAudit.ContinentalMaxElevationKm = PhaseIIICMaxContinentalElevationKm;
	OutAudit.VisibleElevationHash = CurrentMetrics.VisibleElevationHash;
	OutAudit.HistoricalElevationHash = CurrentMetrics.HistoricalElevationHash;
	OutAudit.CrustStateHash = CurrentMetrics.CrustStateHash;
	return true;
}

bool ACarrierLabVisualizationActor::GetPhaseIIIC4SlabPullAudit(FCarrierLabPhaseIIIC4SlabPullAudit& OutAudit) const
{
	OutAudit = LastPhaseIIIC4SlabPullAudit;
	if (!bInitialized)
	{
		return false;
	}

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCount = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	OutAudit.bMarksEnabled = bEnablePhaseIIICSubductingMarks;
	OutAudit.bSlabPullEnabled = bEnablePhaseIIICSlabPull;
	OutAudit.MarkCount = State.ConvergenceSubductingTriangleMarks.Num();
	return true;
}

bool ACarrierLabVisualizationActor::GetPhaseIIIC5ElevationLedgerAudit(FCarrierLabPhaseIIIC5ElevationLedgerAudit& OutAudit) const
{
	OutAudit = LastPhaseIIIC5ElevationLedgerAudit;
	if (!bInitialized)
	{
		return false;
	}

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCount = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	OutAudit.bMarksEnabled = bEnablePhaseIIICSubductingMarks;
	OutAudit.bElevationSplitEnabled = bEnablePhaseIIICVisibleHistoricalElevation;
	OutAudit.bUpliftEnabled = bEnablePhaseIIICOverridingPlateUplift;
	OutAudit.bSlabPullEnabled = bEnablePhaseIIICSlabPull;
	OutAudit.VisibleElevationHash = CurrentMetrics.VisibleElevationHash;
	OutAudit.HistoricalElevationHash = CurrentMetrics.HistoricalElevationHash;
	OutAudit.CrustStateHash = CurrentMetrics.CrustStateHash;
	return true;
}

bool ACarrierLabVisualizationActor::SetPlateContinentalForTest(const int32 PlateId, const bool bContinental)
{
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}
	if (!State.Plates.IsValidIndex(PlateId))
	{
		return false;
	}

	const double Fraction = bContinental ? 1.0 : 0.0;
	CarrierLab::FCarrierPlate& Plate = State.Plates[PlateId];
	Plate.bContinental = bContinental;
	for (CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
	{
		Vertex.ContinentalFraction = Fraction;
		Vertex.bContinental = bContinental;
	}
	for (CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (Sample.PlateId == PlateId)
		{
			Sample.ContinentalFraction = Fraction;
			Sample.bContinental = bContinental;
		}
	}
	ProjectCurrentCarrier();
	return true;
}

bool ACarrierLabVisualizationActor::SetPlateElevationForTest(const int32 PlateId, const double ElevationKm)
{
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}
	if (!State.Plates.IsValidIndex(PlateId) || !FMath::IsFinite(ElevationKm))
	{
		return false;
	}

	CarrierLab::FCarrierPlate& Plate = State.Plates[PlateId];
	for (CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
	{
		Vertex.Elevation = ElevationKm;
		Vertex.HistoricalElevation = 0.0;
		Vertex.bHasHistoricalElevationSnapshot = false;
	}
	for (CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (Sample.PlateId == PlateId)
		{
			Sample.Elevation = ElevationKm;
			Sample.HistoricalElevation = 0.0;
		}
	}
	ProjectCurrentCarrier();
	return true;
}

bool ACarrierLabVisualizationActor::SetPlateOceanicAgeForTest(const int32 PlateId, const double OceanicAgeMa)
{
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}
	if (!State.Plates.IsValidIndex(PlateId))
	{
		return false;
	}

	const double ClampedAge = FMath::Max(OceanicAgeMa, 0.0);
	CarrierLab::FCarrierPlate& Plate = State.Plates[PlateId];
	for (CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
	{
		Vertex.OceanicAge = ClampedAge;
	}
	for (CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (Sample.PlateId == PlateId)
		{
			Sample.OceanicAge = ClampedAge;
		}
	}
	ProjectCurrentCarrier();
	return true;
}

bool ACarrierLabVisualizationActor::SeedPhaseIIIB3NonConvergentEvidenceForTest(FCarrierLabPhaseIIIB3SubductionMatrixAudit& OutAudit)
{
	OutAudit = FCarrierLabPhaseIIIB3SubductionMatrixAudit();
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}
	if (!State.Plates.IsValidIndex(0) || !State.Plates.IsValidIndex(1))
	{
		return false;
	}

	const CarrierLab::FCarrierPlate& Plate = State.Plates[0];
	const CarrierLab::FCarrierPlate& OtherPlate = State.Plates[1];
	const int32 LocalTriangleId = !Plate.ActiveBoundaryTriangles.IsEmpty()
		? Plate.ActiveBoundaryTriangles[0]
		: (Plate.LocalTriangles.IsValidIndex(0) ? 0 : INDEX_NONE);
	const int32 OtherLocalTriangleId = OtherPlate.LocalTriangles.IsValidIndex(0) ? 0 : INDEX_NONE;
	if (!Plate.LocalTriangles.IsValidIndex(LocalTriangleId) ||
		!OtherPlate.LocalTriangles.IsValidIndex(OtherLocalTriangleId))
	{
		return false;
	}

	State.ConvergenceSubductionMatrixPairKeys.Reset();
	State.ConvergenceSubductionPolarityDecisions.Reset();
	State.ConvergenceSubductionTriangleHits.Reset();
	State.ConvergenceSubductionMatrixEvidence.Reset();
	State.ConvergenceSubductingTriangleMarks.Reset();
	State.ConvergenceSubductionMatrixRayTestCount = 1;
	State.ConvergenceSubductionMatrixHitCount = 0;
	State.ConvergenceSubductionMatrixBoundaryHitCount = 0;
	State.ConvergenceSubductionMatrixNonConvergentHitCount = 1;
	State.ConvergenceNeighborPropagationSeedCount = 0;
	State.ConvergenceNeighborPropagationAddedCount = 0;
	State.ConvergenceNeighborPropagationDuplicateCount = 0;
	State.ConvergenceNeighborPropagationDistanceRejectedCount = 0;
	State.ConvergenceNeighborPropagationInvalidCount = 0;
	State.ConvergenceSubductingTriangleMarkDuplicateCount = 0;
	State.ConvergenceSubductingTriangleMarkInvalidCount = 0;
	State.ConvergenceHistoricalElevationSnapshotCount = 0;
	State.ConvergenceHistoricalElevationSnapshotVertexCount = 0;
	State.ConvergenceHistoricalElevationDuplicateSnapshotCount = 0;
	State.ConvergenceHistoricalElevationInvalidSnapshotCount = 0;

	CarrierLab::FConvergenceSubductionMatrixEvidence& Evidence =
		State.ConvergenceSubductionMatrixEvidence.AddDefaulted_GetRef();
	Evidence.EvidenceId = 0;
	Evidence.ContactId = 0;
	Evidence.PairKey = MakePlatePairKey(0, 1);
	Evidence.PlateId = 0;
	Evidence.OtherPlateId = 1;
	Evidence.LocalTriangleId = LocalTriangleId;
	Evidence.OtherLocalTriangleId = OtherLocalTriangleId;
	Evidence.SignedConvergenceVelocity = -0.01;
	Evidence.bAccepted = false;

	UpdateConvergenceSubductionPolarityDecisions();
	UpdateConvergenceNeighborPropagation();
	CurrentMetrics.ConvergenceTrackingHash = HashToString(ComputeConvergenceTrackingHash(State));
	return GetPhaseIIIB3SubductionMatrixAudit(OutAudit);
}

bool ACarrierLabVisualizationActor::SeedPhaseIIIB6SingleConvergentTriangleForTest(
	const int32 PreferredUnderPlateId,
	FCarrierLabPhaseIIIB6SeedMetrics& OutSeedMetrics)
{
	OutSeedMetrics = FCarrierLabPhaseIIIB6SeedMetrics();
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}
	if (!State.Plates.IsValidIndex(PreferredUnderPlateId) ||
		!Motions.IsValidIndex(PreferredUnderPlateId))
	{
		return false;
	}

	FString MeshError;
	if (!RefreshPlateRayMeshes(MeshError))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab IIIB.6 seed failed: %s"), *MeshError);
		return false;
	}

	const CarrierLab::FCarrierPlate& Plate = State.Plates[PreferredUnderPlateId];
	TArray<int32> NeighborCandidates;
	for (int32 LocalTriangleId = 0; LocalTriangleId < Plate.LocalTriangles.Num(); ++LocalTriangleId)
	{
		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
		if (!Triangle.bBoundary)
		{
			continue;
		}

		GetPlateLocalTriangleNeighbors(Plate, LocalTriangleId, NeighborCandidates);
		if (NeighborCandidates.IsEmpty())
		{
			continue;
		}

		const double StepDistanceKm = ComputeActiveTriangleDistanceStepKm(Plate, LocalTriangleId, Motions[PreferredUnderPlateId]);
		if (!FMath::IsFinite(StepDistanceKm) || StepDistanceKm > PhaseIIIBDistanceToFrontLimitKm)
		{
			continue;
		}

		double MaxNeighborDistanceKm = 0.0;
		int32 InRangeNeighborCount = 0;
		for (const int32 NeighborLocalTriangleId : NeighborCandidates)
		{
			const double NeighborDistanceKm = ComputePlateLocalTriangleDistanceKm(Plate, LocalTriangleId, NeighborLocalTriangleId);
			if (!FMath::IsFinite(NeighborDistanceKm) || NeighborDistanceKm == TNumericLimits<double>::Max())
			{
				continue;
			}
			const double PropagatedDistanceKm = StepDistanceKm + NeighborDistanceKm;
			if (PropagatedDistanceKm <= PhaseIIIBDistanceToFrontLimitKm)
			{
				++InRangeNeighborCount;
				MaxNeighborDistanceKm = FMath::Max(MaxNeighborDistanceKm, NeighborDistanceKm);
			}
		}
		if (InRangeNeighborCount <= 0)
		{
			continue;
		}

		for (const CarrierLab::FCarrierPlate& OtherPlate : State.Plates)
		{
			if (OtherPlate.PlateId == Plate.PlateId)
			{
				continue;
			}

			const double PairSignedConvergenceVelocity = ComputePhaseIIPairSignedConvergenceVelocity(Plate.PlateId, OtherPlate.PlateId);
			if (PairSignedConvergenceVelocity <= PhaseIIContactVelocityMargin)
			{
				continue;
			}

			for (CarrierLab::FCarrierPlate& MutablePlate : State.Plates)
			{
				MutablePlate.ActiveBoundaryTriangles.Reset();
				MutablePlate.ActiveBoundaryTriangleDistancesKm.Reset();
			}
			CarrierLab::FCarrierPlate& SeedPlate = State.Plates[PreferredUnderPlateId];
			SeedPlate.ActiveBoundaryTriangles.Add(LocalTriangleId);
			SeedPlate.ActiveBoundaryTriangleDistancesKm.Add(0.0);

			const uint64 PairKey = MakePlatePairKey(PreferredUnderPlateId, OtherPlate.PlateId);
			State.ConvergenceSubductionMatrixPairKeys.Reset();
			State.ConvergenceSubductionMatrixPairKeys.Add(PairKey);
			State.ConvergenceSubductionPolarityDecisions.Reset();
			State.ConvergenceSubductionTriangleHits.Reset();
			State.ConvergenceSubductionMatrixEvidence.Reset();
			State.ConvergenceSubductingTriangleMarks.Reset();
			State.ConvergenceSubductionMatrixRayTestCount = 0;
			State.ConvergenceSubductionMatrixHitCount = 1;
			State.ConvergenceSubductionMatrixBoundaryHitCount = 0;
			State.ConvergenceSubductionMatrixNonConvergentHitCount = 0;
			State.ConvergenceNeighborPropagationSeedCount = 0;
			State.ConvergenceNeighborPropagationAddedCount = 0;
			State.ConvergenceNeighborPropagationDuplicateCount = 0;
			State.ConvergenceNeighborPropagationDistanceRejectedCount = 0;
			State.ConvergenceNeighborPropagationInvalidCount = 0;
			State.ConvergenceSubductingTriangleMarkDuplicateCount = 0;
			State.ConvergenceSubductingTriangleMarkInvalidCount = 0;
			State.ConvergenceHistoricalElevationSnapshotCount = 0;
			State.ConvergenceHistoricalElevationSnapshotVertexCount = 0;
			State.ConvergenceHistoricalElevationDuplicateSnapshotCount = 0;
			State.ConvergenceHistoricalElevationInvalidSnapshotCount = 0;

			CarrierLab::FConvergenceSubductionTriangleHit& TriangleHit = State.ConvergenceSubductionTriangleHits.AddDefaulted_GetRef();
			TriangleHit.PairKey = PairKey;
			TriangleHit.PlateId = PreferredUnderPlateId;
			TriangleHit.OtherPlateId = OtherPlate.PlateId;
			TriangleHit.LocalTriangleId = LocalTriangleId;
			TriangleHit.EvidenceId = 0;
			TriangleHit.SignedConvergenceVelocity = PairSignedConvergenceVelocity;
			CarrierLab::FConvergenceSubductionMatrixEvidence& Evidence = State.ConvergenceSubductionMatrixEvidence.AddDefaulted_GetRef();
			Evidence.EvidenceId = 0;
			Evidence.ContactId = 0;
			Evidence.PairKey = PairKey;
			Evidence.PlateId = PreferredUnderPlateId;
			Evidence.OtherPlateId = OtherPlate.PlateId;
			Evidence.LocalTriangleId = LocalTriangleId;
			Evidence.OtherLocalTriangleId = OtherPlate.LocalTriangles.IsValidIndex(0) ? 0 : INDEX_NONE;
			Evidence.SignedConvergenceVelocity = PairSignedConvergenceVelocity;
			Evidence.bAccepted = true;
			UpdateConvergenceSubductionPolarityDecisions();
			UpdateConvergenceNeighborPropagation();

			OutSeedMetrics.SeedPlateId = PreferredUnderPlateId;
			OutSeedMetrics.SeedOtherPlateId = OtherPlate.PlateId;
			OutSeedMetrics.SeedLocalTriangleId = LocalTriangleId;
			OutSeedMetrics.SeedNeighborCandidateCount = InRangeNeighborCount;
			OutSeedMetrics.SeedStepDistanceKm = StepDistanceKm;
			OutSeedMetrics.MaxSeedNeighborDistanceKm = MaxNeighborDistanceKm;
			OutSeedMetrics.SeedPairKey = PairKey;
			return true;
		}
	}

	return false;
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
	TArray<double> NewElevations;
	TArray<double> NewHistoricalElevations;
	TArray<double> NewOceanicAges;
	TArray<FVector3d> NewRidgeDirections;
	TArray<FVector3d> NewFoldDirections;
	NewPlateIds.Init(INDEX_NONE, State.Samples.Num());
	NewFractions.Init(0.0, State.Samples.Num());
	NewElevations.Init(0.0, State.Samples.Num());
	NewHistoricalElevations.Init(0.0, State.Samples.Num());
	NewOceanicAges.Init(0.0, State.Samples.Num());
	NewRidgeDirections.Init(FVector3d::ZeroVector, State.Samples.Num());
	NewFoldDirections.Init(FVector3d::ZeroVector, State.Samples.Num());

	int32 GapFillCount = 0;
	int32 NonSeparatingGapFillCount = 0;
	TArray<FCarrierLabVizCandidate> Candidates;
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		NewPlateIds[Sample.Id] = Sample.PlateId;
		NewFractions[Sample.Id] = Sample.ContinentalFraction;
		NewElevations[Sample.Id] = Sample.Elevation;
		NewHistoricalElevations[Sample.Id] = Sample.HistoricalElevation;
		NewOceanicAges[Sample.Id] = Sample.OceanicAge;
		NewRidgeDirections[Sample.Id] = Sample.RidgeDirection;
		NewFoldDirections[Sample.Id] = Sample.FoldDirection;

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
				NewElevations[Sample.Id] = 0.0;
				NewHistoricalElevations[Sample.Id] = 0.0;
				NewOceanicAges[Sample.Id] = 0.0;
				NewRidgeDirections[Sample.Id] = FVector3d::ZeroVector;
				NewFoldDirections[Sample.Id] = FVector3d::ZeroVector;
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
			: Sample.ContinentalFraction;
		if (Chosen != nullptr && State.Plates.IsValidIndex(ResolvedPlateId))
		{
			FCarrierLabCrustFields InterpolatedFields;
			if (InterpolateCrustFields(State.Plates[ResolvedPlateId], *Chosen, Sample.UnitPosition, InterpolatedFields))
			{
				NewElevations[Sample.Id] = InterpolatedFields.Elevation;
				NewHistoricalElevations[Sample.Id] = InterpolatedFields.HistoricalElevation;
				NewOceanicAges[Sample.Id] = InterpolatedFields.OceanicAge;
				NewRidgeDirections[Sample.Id] = InterpolatedFields.RidgeDirection;
				NewFoldDirections[Sample.Id] = InterpolatedFields.FoldDirection;
			}
		}
	}

	for (CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (NewPlateIds.IsValidIndex(Sample.Id) && NewPlateIds[Sample.Id] != INDEX_NONE)
		{
			Sample.PlateId = NewPlateIds[Sample.Id];
			Sample.ContinentalFraction = FMath::Clamp(NewFractions[Sample.Id], 0.0, 1.0);
			Sample.Elevation = NewElevations[Sample.Id];
			Sample.HistoricalElevation = NewHistoricalElevations[Sample.Id];
			Sample.OceanicAge = NewOceanicAges[Sample.Id];
			Sample.RidgeDirection = NewRidgeDirections[Sample.Id];
			Sample.FoldDirection = NewFoldDirections[Sample.Id];
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

void ACarrierLabVisualizationActor::ShowPhaseIIISummaryLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::PhaseIIISummary);
}

void ACarrierLabVisualizationActor::ShowElevationHeatmapLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::ElevationHeatmap);
}

void ACarrierLabVisualizationActor::ShowSubductionMaskLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::SubductionMask);
}

void ACarrierLabVisualizationActor::ShowDistanceToFrontHeatmapLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::DistanceToFrontHeatmap);
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
	InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowPhaseIIISummaryLayer);
	InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowElevationHeatmapLayer);
	InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowSubductionMaskLayer);
	InputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowDistanceToFrontHeatmapLayer);
}

void ACarrierLabVisualizationActor::UpdateConvergenceTrackingDistances()
{
	for (CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		TArray<int32> KeptActiveTriangles;
		TArray<double> KeptDistancesKm;
		KeptActiveTriangles.Reserve(Plate.ActiveBoundaryTriangles.Num());
		KeptDistancesKm.Reserve(Plate.ActiveBoundaryTriangles.Num());

		const FCarrierLabVisualizationMotion* Motion = Motions.IsValidIndex(Plate.PlateId)
			? &Motions[Plate.PlateId]
			: nullptr;
		for (int32 ActiveIndex = 0; ActiveIndex < Plate.ActiveBoundaryTriangles.Num(); ++ActiveIndex)
		{
			const int32 LocalTriangleId = Plate.ActiveBoundaryTriangles[ActiveIndex];
			const double PreviousDistanceKm = Plate.ActiveBoundaryTriangleDistancesKm.IsValidIndex(ActiveIndex)
				? Plate.ActiveBoundaryTriangleDistancesKm[ActiveIndex]
				: 0.0;
			const double StepDistanceKm = Motion != nullptr
				? ComputeActiveTriangleDistanceStepKm(Plate, LocalTriangleId, *Motion)
				: 0.0;
			const double NewDistanceKm = PreviousDistanceKm + StepDistanceKm;
			if (!FMath::IsFinite(NewDistanceKm) || NewDistanceKm > PhaseIIIBDistanceToFrontLimitKm)
			{
				++State.ConvergenceTrackingDistanceCullCount;
				continue;
			}

			KeptActiveTriangles.Add(LocalTriangleId);
			KeptDistancesKm.Add(NewDistanceKm);
		}

		Plate.ActiveBoundaryTriangles = MoveTemp(KeptActiveTriangles);
		Plate.ActiveBoundaryTriangleDistancesKm = MoveTemp(KeptDistancesKm);
	}
}

void ACarrierLabVisualizationActor::UpdateConvergenceSubductionMatrix()
{
	State.ConvergenceSubductionTriangleHits.Reset();
	State.ConvergenceSubductionMatrixEvidence.Reset();
	FString MeshError;
	if (!RefreshPlateRayMeshes(MeshError))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab convergence subduction matrix update failed: %s"), *MeshError);
		return;
	}

	IMeshSpatial::FQueryOptions QueryOptions(2.0 + 1.0e-6);
	TArray<MeshIntersection::FHitIntersectionResult> Hits;
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		if (!Motions.IsValidIndex(Plate.PlateId))
		{
			continue;
		}

		for (const int32 LocalTriangleId : Plate.ActiveBoundaryTriangles)
		{
			if (!Plate.LocalTriangles.IsValidIndex(LocalTriangleId))
			{
				continue;
			}

			const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
			if (!Plate.Vertices.IsValidIndex(Triangle.A) ||
				!Plate.Vertices.IsValidIndex(Triangle.B) ||
				!Plate.Vertices.IsValidIndex(Triangle.C))
			{
				continue;
			}

			const FVector3d Barycenter = NormalizeOrFallback(
				Plate.Vertices[Triangle.A].UnitPosition +
				Plate.Vertices[Triangle.B].UnitPosition +
				Plate.Vertices[Triangle.C].UnitPosition,
				Plate.Vertices[Triangle.A].UnitPosition);
			const FRay3d Ray(FVector3d::Zero(), Barycenter);

			for (const CarrierLab::FCarrierPlate& OtherPlate : State.Plates)
			{
				if (OtherPlate.PlateId == Plate.PlateId ||
					!PlateRayMeshes.IsValidIndex(OtherPlate.PlateId) ||
					!PlateRayMeshes[OtherPlate.PlateId].Tree.IsValid())
				{
					continue;
				}

				++State.ConvergenceSubductionMatrixRayTestCount;
				Hits.Reset();
				PlateRayMeshes[OtherPlate.PlateId].Tree->FindAllHitTriangles(Ray, Hits, QueryOptions);
				bool bAcceptedPair = false;
				for (const MeshIntersection::FHitIntersectionResult& Hit : Hits)
				{
					if (Hit.Distance <= 0.0 || Hit.Distance > 2.0 + 1.0e-6)
					{
						continue;
					}
					const int32* OtherLocalTriangleId = PlateRayMeshes[OtherPlate.PlateId].MeshTriangleIdToLocalTriangleId.Find(Hit.TriangleId);
					if (OtherLocalTriangleId == nullptr)
					{
						continue;
					}
					if (IsBoundaryHit(Hit.BaryCoords))
					{
						++State.ConvergenceSubductionMatrixBoundaryHitCount;
						continue;
					}

					const double LocalSignedConvergenceVelocity =
						-SignedPairSeparationVelocityForPlatePair(Barycenter, Motions, Plate.PlateId, OtherPlate.PlateId);
					const int32 EvidenceId = State.ConvergenceSubductionMatrixEvidence.Num();
					if (LocalSignedConvergenceVelocity <= PhaseIIContactVelocityMargin)
					{
						State.ConvergenceSubductionMatrixEvidence.Add(CarrierLab::FConvergenceSubductionMatrixEvidence{
							EvidenceId,
							EvidenceId,
							MakePlatePairKey(Plate.PlateId, OtherPlate.PlateId),
							Plate.PlateId,
							OtherPlate.PlateId,
							LocalTriangleId,
							*OtherLocalTriangleId,
							LocalSignedConvergenceVelocity,
							false});
						++State.ConvergenceSubductionMatrixNonConvergentHitCount;
						continue;
					}

					State.ConvergenceSubductionMatrixPairKeys.Add(MakePlatePairKey(Plate.PlateId, OtherPlate.PlateId));
					State.ConvergenceSubductionMatrixEvidence.Add(CarrierLab::FConvergenceSubductionMatrixEvidence{
						EvidenceId,
						EvidenceId,
						MakePlatePairKey(Plate.PlateId, OtherPlate.PlateId),
						Plate.PlateId,
						OtherPlate.PlateId,
						LocalTriangleId,
						*OtherLocalTriangleId,
						LocalSignedConvergenceVelocity,
						true});
					State.ConvergenceSubductionTriangleHits.Add(CarrierLab::FConvergenceSubductionTriangleHit{
						MakePlatePairKey(Plate.PlateId, OtherPlate.PlateId),
						Plate.PlateId,
						OtherPlate.PlateId,
						LocalTriangleId,
						EvidenceId,
						LocalSignedConvergenceVelocity});
					++State.ConvergenceSubductionMatrixHitCount;
					bAcceptedPair = true;
					break;
				}
				if (bAcceptedPair)
				{
					break;
				}
			}
		}
	}
}

void ACarrierLabVisualizationActor::UpdateConvergenceSubductionPolarityDecisions()
{
	State.ConvergenceSubductionPolarityDecisions.Reset();
	TArray<uint64> PairKeys = State.ConvergenceSubductionMatrixPairKeys.Array();
	PairKeys.Sort();
	State.ConvergenceSubductionPolarityDecisions.Reserve(PairKeys.Num());

	for (const uint64 PairKey : PairKeys)
	{
		CarrierLab::FConvergenceSubductionPolarityDecision& Decision = State.ConvergenceSubductionPolarityDecisions.AddDefaulted_GetRef();
		Decision.PairKey = PairKey;
		DecodePlatePairKey(PairKey, Decision.PlateA, Decision.PlateB);

		if (!State.Plates.IsValidIndex(Decision.PlateA) ||
			!State.Plates.IsValidIndex(Decision.PlateB) ||
			Decision.PlateA == Decision.PlateB)
		{
			Decision.DecisionClass = CarrierLab::EConvergenceSubductionPolarityClass::Invalid;
			continue;
		}

		Decision.PlateAContinentalFraction = ComputePlateContinentalFraction(State, Decision.PlateA);
		Decision.PlateBContinentalFraction = ComputePlateContinentalFraction(State, Decision.PlateB);
		Decision.PlateAOceanicAge = ComputePlateOceanicAge(State, Decision.PlateA);
		Decision.PlateBOceanicAge = ComputePlateOceanicAge(State, Decision.PlateB);
		const bool bAContinental = Decision.PlateAContinentalFraction >= 0.5;
		const bool bBContinental = Decision.PlateBContinentalFraction >= 0.5;

		if (bAContinental != bBContinental)
		{
			Decision.DecisionClass = CarrierLab::EConvergenceSubductionPolarityClass::OceanicUnderContinental;
			Decision.UnderPlate = bAContinental ? Decision.PlateB : Decision.PlateA;
			Decision.OverPlate = bAContinental ? Decision.PlateA : Decision.PlateB;
		}
		else if (bAContinental && bBContinental)
		{
			Decision.DecisionClass = CarrierLab::EConvergenceSubductionPolarityClass::CollisionCandidate;
		}
		else
		{
			constexpr double AgeEpsilonMa = 1.0e-9;
			const double AgeDelta = Decision.PlateAOceanicAge - Decision.PlateBOceanicAge;
			if (FMath::Abs(AgeDelta) > AgeEpsilonMa)
			{
				Decision.DecisionClass = CarrierLab::EConvergenceSubductionPolarityClass::OlderOceanicUnderYoungerOceanic;
				Decision.UnderPlate = AgeDelta > 0.0 ? Decision.PlateA : Decision.PlateB;
				Decision.OverPlate = AgeDelta > 0.0 ? Decision.PlateB : Decision.PlateA;
			}
			else
			{
				Decision.DecisionClass = CarrierLab::EConvergenceSubductionPolarityClass::OceanOceanDeferred;
			}
		}
	}
}

void ACarrierLabVisualizationActor::UpdateConvergenceNeighborPropagation()
{
	State.ConvergenceNeighborPropagationSeedCount = 0;
	State.ConvergenceNeighborPropagationAddedCount = 0;
	State.ConvergenceNeighborPropagationDuplicateCount = 0;
	State.ConvergenceNeighborPropagationDistanceRejectedCount = 0;
	State.ConvergenceNeighborPropagationInvalidCount = 0;

	TMap<uint64, CarrierLab::FConvergenceSubductionPolarityDecision> DecisionsByPair;
	for (const CarrierLab::FConvergenceSubductionPolarityDecision& Decision : State.ConvergenceSubductionPolarityDecisions)
	{
		DecisionsByPair.Add(Decision.PairKey, Decision);
	}
	if (DecisionsByPair.IsEmpty() || State.ConvergenceSubductionTriangleHits.IsEmpty())
	{
		return;
	}

	for (const CarrierLab::FConvergenceSubductionTriangleHit& Hit : State.ConvergenceSubductionTriangleHits)
	{
		const CarrierLab::FConvergenceSubductionPolarityDecision* Decision = DecisionsByPair.Find(Hit.PairKey);
		if (Decision != nullptr && !IsSubductionPolarityDecision(*Decision))
		{
			continue;
		}
		if (Decision != nullptr && Decision->UnderPlate != Hit.PlateId)
		{
			continue;
		}
		if (Decision == nullptr || !State.Plates.IsValidIndex(Hit.PlateId))
		{
			++State.ConvergenceNeighborPropagationInvalidCount;
			continue;
		}

		CarrierLab::FCarrierPlate& Plate = State.Plates[Hit.PlateId];
		if (!Plate.LocalTriangles.IsValidIndex(Hit.LocalTriangleId))
		{
			++State.ConvergenceNeighborPropagationInvalidCount;
			continue;
		}

		int32 ParentActiveIndex = INDEX_NONE;
		for (int32 ActiveIndex = 0; ActiveIndex < Plate.ActiveBoundaryTriangles.Num(); ++ActiveIndex)
		{
			if (Plate.ActiveBoundaryTriangles[ActiveIndex] == Hit.LocalTriangleId)
			{
				ParentActiveIndex = ActiveIndex;
				break;
			}
		}
		if (ParentActiveIndex == INDEX_NONE ||
			!Plate.ActiveBoundaryTriangleDistancesKm.IsValidIndex(ParentActiveIndex))
		{
			++State.ConvergenceNeighborPropagationInvalidCount;
			continue;
		}

		++State.ConvergenceNeighborPropagationSeedCount;
		const double ParentDistanceKm = Plate.ActiveBoundaryTriangleDistancesKm[ParentActiveIndex];
		TArray<int32> Neighbors;
		GetPlateLocalTriangleNeighbors(Plate, Hit.LocalTriangleId, Neighbors);

		TMap<int32, int32> ActiveIndexByTriangle;
		for (int32 ActiveIndex = 0; ActiveIndex < Plate.ActiveBoundaryTriangles.Num(); ++ActiveIndex)
		{
			ActiveIndexByTriangle.Add(Plate.ActiveBoundaryTriangles[ActiveIndex], ActiveIndex);
		}

		for (const int32 NeighborLocalTriangleId : Neighbors)
		{
			if (!Plate.LocalTriangles.IsValidIndex(NeighborLocalTriangleId))
			{
				++State.ConvergenceNeighborPropagationInvalidCount;
				continue;
			}

			const double NeighborDistanceKm = ComputePlateLocalTriangleDistanceKm(Plate, Hit.LocalTriangleId, NeighborLocalTriangleId);
			const double PropagatedDistanceKm = ParentDistanceKm + NeighborDistanceKm;
			if (!FMath::IsFinite(PropagatedDistanceKm) || PropagatedDistanceKm > PhaseIIIBDistanceToFrontLimitKm)
			{
				++State.ConvergenceNeighborPropagationDistanceRejectedCount;
				continue;
			}

			if (const int32* ExistingActiveIndex = ActiveIndexByTriangle.Find(NeighborLocalTriangleId))
			{
				++State.ConvergenceNeighborPropagationDuplicateCount;
				if (Plate.ActiveBoundaryTriangleDistancesKm.IsValidIndex(*ExistingActiveIndex) &&
					PropagatedDistanceKm < Plate.ActiveBoundaryTriangleDistancesKm[*ExistingActiveIndex])
				{
					Plate.ActiveBoundaryTriangleDistancesKm[*ExistingActiveIndex] = PropagatedDistanceKm;
				}
				continue;
			}

			const int32 NewActiveIndex = Plate.ActiveBoundaryTriangles.Add(NeighborLocalTriangleId);
			Plate.ActiveBoundaryTriangleDistancesKm.Add(PropagatedDistanceKm);
			ActiveIndexByTriangle.Add(NeighborLocalTriangleId, NewActiveIndex);
			++State.ConvergenceNeighborPropagationAddedCount;
		}
	}

	for (CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		TArray<TPair<int32, double>> ActivePairs;
		ActivePairs.Reserve(Plate.ActiveBoundaryTriangles.Num());
		for (int32 ActiveIndex = 0; ActiveIndex < Plate.ActiveBoundaryTriangles.Num(); ++ActiveIndex)
		{
			ActivePairs.Add(TPair<int32, double>(
				Plate.ActiveBoundaryTriangles[ActiveIndex],
				Plate.ActiveBoundaryTriangleDistancesKm.IsValidIndex(ActiveIndex)
					? Plate.ActiveBoundaryTriangleDistancesKm[ActiveIndex]
					: 0.0));
		}
		ActivePairs.Sort([](const TPair<int32, double>& A, const TPair<int32, double>& B)
		{
			return A.Key < B.Key;
		});

		Plate.ActiveBoundaryTriangles.Reset(ActivePairs.Num());
		Plate.ActiveBoundaryTriangleDistancesKm.Reset(ActivePairs.Num());
		for (const TPair<int32, double>& Pair : ActivePairs)
		{
			Plate.ActiveBoundaryTriangles.Add(Pair.Key);
			Plate.ActiveBoundaryTriangleDistancesKm.Add(Pair.Value);
		}
	}
}

void ACarrierLabVisualizationActor::UpdatePhaseIIICSubductingTriangleMarks()
{
	TMap<uint64, CarrierLab::FConvergenceSubductionPolarityDecision> DecisionsByPair;
	for (const CarrierLab::FConvergenceSubductionPolarityDecision& Decision : State.ConvergenceSubductionPolarityDecisions)
	{
		DecisionsByPair.Add(Decision.PairKey, Decision);
	}

	TSet<uint64> ExistingTriangleKeys;
	for (const CarrierLab::FConvergenceSubductingTriangleMark& Mark : State.ConvergenceSubductingTriangleMarks)
	{
		ExistingTriangleKeys.Add(MakePlateTriangleKey(Mark.PlateId, Mark.LocalTriangleId));
	}

	for (const CarrierLab::FConvergenceSubductionTriangleHit& Hit : State.ConvergenceSubductionTriangleHits)
	{
		const CarrierLab::FConvergenceSubductionPolarityDecision* Decision = DecisionsByPair.Find(Hit.PairKey);
		if (Decision == nullptr || !IsSubductionPolarityDecision(*Decision))
		{
			continue;
		}
		if (Decision->UnderPlate != Hit.PlateId)
		{
			continue;
		}
		if (!State.Plates.IsValidIndex(Hit.PlateId) ||
			!State.Plates[Hit.PlateId].LocalTriangles.IsValidIndex(Hit.LocalTriangleId))
		{
			++State.ConvergenceSubductingTriangleMarkInvalidCount;
			continue;
		}

		const uint64 TriangleKey = MakePlateTriangleKey(Hit.PlateId, Hit.LocalTriangleId);
		if (ExistingTriangleKeys.Contains(TriangleKey))
		{
			++State.ConvergenceSubductingTriangleMarkDuplicateCount;
			continue;
		}

		CarrierLab::FConvergenceSubductingTriangleMark& Mark = State.ConvergenceSubductingTriangleMarks.AddDefaulted_GetRef();
		Mark.MarkId = State.ConvergenceSubductingTriangleMarks.Num() - 1;
		Mark.PairKey = Hit.PairKey;
		Mark.PlateId = Hit.PlateId;
		Mark.OtherPlateId = Hit.OtherPlateId;
		Mark.LocalTriangleId = Hit.LocalTriangleId;
		Mark.EvidenceId = Hit.EvidenceId;
		Mark.SignedConvergenceVelocity = Hit.SignedConvergenceVelocity;
		Mark.DecisionClass = Decision->DecisionClass;
		if (bEnablePhaseIIICVisibleHistoricalElevation)
		{
			ApplyPhaseIIIC2ElevationSplitToMark(Mark);
		}
		ExistingTriangleKeys.Add(TriangleKey);
	}
}

double ACarrierLabVisualizationActor::SumPlateVisibleElevationKm() const
{
	double Sum = 0.0;
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			Sum += Vertex.Elevation;
		}
	}
	return Sum;
}

void ACarrierLabVisualizationActor::BeginPhaseIIIC5ElevationLedger()
{
	LastPhaseIIIC5ElevationLedgerAudit = FCarrierLabPhaseIIIC5ElevationLedgerAudit();
	LastPhaseIIIC5ElevationLedgerAudit.Step = CurrentMetrics.Step + 1;
	LastPhaseIIIC5ElevationLedgerAudit.EventCount = CurrentMetrics.EventCount;
	LastPhaseIIIC5ElevationLedgerAudit.PlateCount = State.Plates.Num();
	LastPhaseIIIC5ElevationLedgerAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	LastPhaseIIIC5ElevationLedgerAudit.bMarksEnabled = bEnablePhaseIIICSubductingMarks;
	LastPhaseIIIC5ElevationLedgerAudit.bElevationSplitEnabled = bEnablePhaseIIICVisibleHistoricalElevation;
	LastPhaseIIIC5ElevationLedgerAudit.bUpliftEnabled = bEnablePhaseIIICOverridingPlateUplift;
	LastPhaseIIIC5ElevationLedgerAudit.bSlabPullEnabled = bEnablePhaseIIICSlabPull;
	LastPhaseIIIC5ElevationLedgerAudit.ActualVisibleElevationBeforeKm = SumPlateVisibleElevationKm();
}

void ACarrierLabVisualizationActor::AddPhaseIIIC5ElevationLedgerRecord(
	const ECarrierLabPhaseIIIC5ElevationLedgerClass LedgerClass,
	const int32 MarkId,
	const int32 PlateId,
	const int32 OtherPlateId,
	const int32 LocalTriangleId,
	const int32 LocalVertexId,
	const int32 GlobalSampleId,
	const double PreviousElevationKm,
	const double NewElevationKm,
	const double SignedConvergenceVelocity)
{
	const double DeltaKm = NewElevationKm - PreviousElevationKm;
	if (!FMath::IsFinite(DeltaKm) || FMath::Abs(DeltaKm) <= 1.0e-15)
	{
		return;
	}

	FCarrierLabPhaseIIIC5ElevationLedgerRecord& Record =
		LastPhaseIIIC5ElevationLedgerAudit.Records.AddDefaulted_GetRef();
	Record.RecordId = LastPhaseIIIC5ElevationLedgerAudit.Records.Num() - 1;
	Record.Step = LastPhaseIIIC5ElevationLedgerAudit.Step;
	Record.MarkId = MarkId;
	Record.PlateId = PlateId;
	Record.OtherPlateId = OtherPlateId;
	Record.LocalTriangleId = LocalTriangleId;
	Record.LocalVertexId = LocalVertexId;
	Record.GlobalSampleId = GlobalSampleId;
	Record.PreviousElevationKm = PreviousElevationKm;
	Record.NewElevationKm = NewElevationKm;
	Record.DeltaKm = DeltaKm;
	Record.SignedConvergenceVelocity = SignedConvergenceVelocity;
	Record.LedgerClass = LedgerClass;

	++LastPhaseIIIC5ElevationLedgerAudit.RecordCount;
	LastPhaseIIIC5ElevationLedgerAudit.LedgerVisibleElevationDeltaKm += DeltaKm;
	switch (LedgerClass)
	{
	case ECarrierLabPhaseIIIC5ElevationLedgerClass::TrenchVisibleElevation:
		++LastPhaseIIIC5ElevationLedgerAudit.TrenchRecordCount;
		LastPhaseIIIC5ElevationLedgerAudit.TrenchVisibleElevationDeltaKm += DeltaKm;
		break;
	case ECarrierLabPhaseIIIC5ElevationLedgerClass::OverridingUplift:
		++LastPhaseIIIC5ElevationLedgerAudit.UpliftRecordCount;
		LastPhaseIIIC5ElevationLedgerAudit.UpliftVisibleElevationDeltaKm += DeltaKm;
		break;
	default:
		break;
	}
}

void ACarrierLabVisualizationActor::FinalizePhaseIIIC5ElevationLedger()
{
	LastPhaseIIIC5ElevationLedgerAudit.ActualVisibleElevationAfterKm = SumPlateVisibleElevationKm();
	LastPhaseIIIC5ElevationLedgerAudit.ActualVisibleElevationDeltaKm =
		LastPhaseIIIC5ElevationLedgerAudit.ActualVisibleElevationAfterKm -
		LastPhaseIIIC5ElevationLedgerAudit.ActualVisibleElevationBeforeKm;
	LastPhaseIIIC5ElevationLedgerAudit.VisibleElevationResidualKm =
		LastPhaseIIIC5ElevationLedgerAudit.ActualVisibleElevationDeltaKm -
		LastPhaseIIIC5ElevationLedgerAudit.LedgerVisibleElevationDeltaKm;

	TSet<uint64> UniqueVertices;
	uint64 LedgerHash = 1469598103934665603ull;
	HashMix(LedgerHash, static_cast<uint64>(LastPhaseIIIC5ElevationLedgerAudit.Step + 1));
	HashMix(LedgerHash, static_cast<uint64>(LastPhaseIIIC5ElevationLedgerAudit.EventCount + 1));
	HashMix(LedgerHash, static_cast<uint64>(LastPhaseIIIC5ElevationLedgerAudit.ResetSerial + 1));
	HashMix(LedgerHash, LastPhaseIIIC5ElevationLedgerAudit.bMarksEnabled ? 1ull : 0ull);
	HashMix(LedgerHash, LastPhaseIIIC5ElevationLedgerAudit.bElevationSplitEnabled ? 1ull : 0ull);
	HashMix(LedgerHash, LastPhaseIIIC5ElevationLedgerAudit.bUpliftEnabled ? 1ull : 0ull);
	HashMix(LedgerHash, LastPhaseIIIC5ElevationLedgerAudit.bSlabPullEnabled ? 1ull : 0ull);
	for (const FCarrierLabPhaseIIIC5ElevationLedgerRecord& Record : LastPhaseIIIC5ElevationLedgerAudit.Records)
	{
		UniqueVertices.Add(MakePlateVertexKey(Record.PlateId, Record.LocalVertexId));
		HashMix(LedgerHash, static_cast<uint64>(Record.RecordId + 1));
		HashMix(LedgerHash, static_cast<uint64>(Record.MarkId + 1));
		HashMix(LedgerHash, static_cast<uint64>(Record.PlateId + 1));
		HashMix(LedgerHash, static_cast<uint64>(Record.OtherPlateId + 1));
		HashMix(LedgerHash, static_cast<uint64>(Record.LocalTriangleId + 1));
		HashMix(LedgerHash, static_cast<uint64>(Record.LocalVertexId + 1));
		HashMix(LedgerHash, static_cast<uint64>(Record.GlobalSampleId + 1));
		HashMixDouble(LedgerHash, Record.PreviousElevationKm);
		HashMixDouble(LedgerHash, Record.NewElevationKm);
		HashMixDouble(LedgerHash, Record.DeltaKm);
		HashMixDouble(LedgerHash, Record.SignedConvergenceVelocity);
		HashMix(LedgerHash, static_cast<uint64>(Record.LedgerClass) + 1ull);
	}
	LastPhaseIIIC5ElevationLedgerAudit.UniqueVertexCount = UniqueVertices.Num();
	HashMixDouble(LedgerHash, LastPhaseIIIC5ElevationLedgerAudit.ActualVisibleElevationBeforeKm);
	HashMixDouble(LedgerHash, LastPhaseIIIC5ElevationLedgerAudit.ActualVisibleElevationAfterKm);
	HashMixDouble(LedgerHash, LastPhaseIIIC5ElevationLedgerAudit.ActualVisibleElevationDeltaKm);
	HashMixDouble(LedgerHash, LastPhaseIIIC5ElevationLedgerAudit.LedgerVisibleElevationDeltaKm);
	HashMixDouble(LedgerHash, LastPhaseIIIC5ElevationLedgerAudit.VisibleElevationResidualKm);
	LastPhaseIIIC5ElevationLedgerAudit.ElevationLedgerHash = HashToString(LedgerHash);
}

bool ACarrierLabVisualizationActor::ApplyPhaseIIIC2ElevationSplitToMark(CarrierLab::FConvergenceSubductingTriangleMark& Mark)
{
	if (Mark.bHistoricalElevationSnapshotTaken)
	{
		++State.ConvergenceHistoricalElevationDuplicateSnapshotCount;
		return true;
	}
	if (!State.Plates.IsValidIndex(Mark.PlateId))
	{
		++State.ConvergenceHistoricalElevationInvalidSnapshotCount;
		return false;
	}

	CarrierLab::FCarrierPlate& Plate = State.Plates[Mark.PlateId];
	if (!Plate.LocalTriangles.IsValidIndex(Mark.LocalTriangleId))
	{
		++State.ConvergenceHistoricalElevationInvalidSnapshotCount;
		return false;
	}

	const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[Mark.LocalTriangleId];
	const int32 VertexIds[3] = { Triangle.A, Triangle.B, Triangle.C };
	for (const int32 VertexId : VertexIds)
	{
		if (!Plate.Vertices.IsValidIndex(VertexId))
		{
			++State.ConvergenceHistoricalElevationInvalidSnapshotCount;
			return false;
		}
	}

	double MinHistoricalElevation = TNumericLimits<double>::Max();
	double MaxHistoricalElevation = -TNumericLimits<double>::Max();
	for (const int32 VertexId : VertexIds)
	{
		CarrierLab::FCarrierVertex& Vertex = Plate.Vertices[VertexId];
		if (!Vertex.bHasHistoricalElevationSnapshot)
		{
			Vertex.HistoricalElevation = Vertex.Elevation;
			Vertex.bHasHistoricalElevationSnapshot = true;
		}
		MinHistoricalElevation = FMath::Min(MinHistoricalElevation, Vertex.HistoricalElevation);
		MaxHistoricalElevation = FMath::Max(MaxHistoricalElevation, Vertex.HistoricalElevation);
	}
	for (const int32 VertexId : VertexIds)
	{
		CarrierLab::FCarrierVertex& Vertex = Plate.Vertices[VertexId];
		const double PreviousElevationKm = Vertex.Elevation;
		Vertex.Elevation = PhaseIIICTrenchDepthKm;
		AddPhaseIIIC5ElevationLedgerRecord(
			ECarrierLabPhaseIIIC5ElevationLedgerClass::TrenchVisibleElevation,
			Mark.MarkId,
			Mark.PlateId,
			Mark.OtherPlateId,
			Mark.LocalTriangleId,
			VertexId,
			Vertex.GlobalSampleId,
			PreviousElevationKm,
			Vertex.Elevation,
			Mark.SignedConvergenceVelocity);
	}

	Mark.bHistoricalElevationSnapshotTaken = true;
	Mark.HistoricalElevationSnapshotVertexCount = 3;
	Mark.HistoricalElevationSnapshotMin = MinHistoricalElevation;
	Mark.HistoricalElevationSnapshotMax = MaxHistoricalElevation;
	Mark.VisibleElevationAppliedKm = PhaseIIICTrenchDepthKm;
	++State.ConvergenceHistoricalElevationSnapshotCount;
	State.ConvergenceHistoricalElevationSnapshotVertexCount += 3;
	return true;
}

void ACarrierLabVisualizationActor::ApplyPhaseIIIC3OverridingPlateUplift()
{
	LastPhaseIIIC3UpliftAudit = FCarrierLabPhaseIIIC3UpliftAudit();
	LastPhaseIIIC3UpliftAudit.Step = CurrentMetrics.Step + 1;
	LastPhaseIIIC3UpliftAudit.EventCount = CurrentMetrics.EventCount;
	LastPhaseIIIC3UpliftAudit.PlateCount = State.Plates.Num();
	LastPhaseIIIC3UpliftAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	LastPhaseIIIC3UpliftAudit.bMarksEnabled = bEnablePhaseIIICSubductingMarks;
	LastPhaseIIIC3UpliftAudit.bElevationSplitEnabled = bEnablePhaseIIICVisibleHistoricalElevation;
	LastPhaseIIIC3UpliftAudit.bUpliftEnabled = bEnablePhaseIIICOverridingPlateUplift;
	LastPhaseIIIC3UpliftAudit.MarkCount = State.ConvergenceSubductingTriangleMarks.Num();
	LastPhaseIIIC3UpliftAudit.EffectRadiusKm = PhaseIIICSubductionEffectRadiusKm;
	LastPhaseIIIC3UpliftAudit.UpliftRateMmPerYear = PhaseIIICSubductionUpliftMmPerYear;
	LastPhaseIIIC3UpliftAudit.ReferenceVelocityMmPerYear = PhaseIIICReferenceVelocityMmPerYear;
	LastPhaseIIIC3UpliftAudit.TrenchDepthKm = PhaseIIICTrenchDepthKm;
	LastPhaseIIIC3UpliftAudit.ContinentalMaxElevationKm = PhaseIIICMaxContinentalElevationKm;

	uint64 UpliftHash = 1469598103934665603ull;
	HashMix(UpliftHash, static_cast<uint64>(LastPhaseIIIC3UpliftAudit.Step + 1));
	HashMix(UpliftHash, static_cast<uint64>(State.ConvergenceSubductingTriangleMarks.Num() + 1));
	HashMixDouble(UpliftHash, PhaseIIICSubductionUpliftMmPerYear);
	HashMixDouble(UpliftHash, PhaseIIICReferenceVelocityMmPerYear);
	HashMixDouble(UpliftHash, PhaseIIICTrenchDepthKm);
	HashMixDouble(UpliftHash, PhaseIIICMaxContinentalElevationKm);
	HashMixDouble(UpliftHash, PhaseIIICSubductionEffectRadiusKm);

	if (!bEnablePhaseIIICOverridingPlateUplift ||
		!bEnablePhaseIIICSubductingMarks ||
		!bEnablePhaseIIICVisibleHistoricalElevation)
	{
		LastPhaseIIIC3UpliftAudit.UpliftHash = HashToString(UpliftHash);
		return;
	}

	TSet<uint64> UpliftedVertexKeys;
	for (const CarrierLab::FConvergenceSubductingTriangleMark& Mark : State.ConvergenceSubductingTriangleMarks)
	{
		if (!Mark.bHistoricalElevationSnapshotTaken ||
			!State.Plates.IsValidIndex(Mark.PlateId) ||
			!State.Plates.IsValidIndex(Mark.OtherPlateId))
		{
			++LastPhaseIIIC3UpliftAudit.InvalidInputCount;
			continue;
		}

		const CarrierLab::FCarrierPlate& UnderPlate = State.Plates[Mark.PlateId];
		CarrierLab::FCarrierPlate& OverPlate = State.Plates[Mark.OtherPlateId];
		if (!UnderPlate.LocalTriangles.IsValidIndex(Mark.LocalTriangleId))
		{
			++LastPhaseIIIC3UpliftAudit.InvalidInputCount;
			continue;
		}

		const CarrierLab::FCarrierPlateTriangle& UnderTriangle = UnderPlate.LocalTriangles[Mark.LocalTriangleId];
		const int32 UnderVertexIds[3] = { UnderTriangle.A, UnderTriangle.B, UnderTriangle.C };
		bool bUnderVerticesValid = true;
		FVector3d UnderBarycenter = FVector3d::ZeroVector;
		double HistoricalElevationKm = 0.0;
		for (const int32 UnderVertexId : UnderVertexIds)
		{
			if (!UnderPlate.Vertices.IsValidIndex(UnderVertexId))
			{
				bUnderVerticesValid = false;
				break;
			}
			const CarrierLab::FCarrierVertex& UnderVertex = UnderPlate.Vertices[UnderVertexId];
			UnderBarycenter += UnderVertex.UnitPosition;
			HistoricalElevationKm += UnderVertex.HistoricalElevation;
		}
		if (!bUnderVerticesValid)
		{
			++LastPhaseIIIC3UpliftAudit.InvalidInputCount;
			continue;
		}
		UnderBarycenter = NormalizeOrFallback(UnderBarycenter, UnderPlate.Vertices[UnderVertexIds[0]].UnitPosition);
		HistoricalElevationKm /= 3.0;

		const double SpeedTransfer = PhaseIIIC3SpeedTransfer(
			Mark.SignedConvergenceVelocity,
			PhaseIIICReferenceVelocityMmPerYear);
		const double ReliefTransfer = PhaseIIIC3ReliefTransfer(
			HistoricalElevationKm,
			PhaseIIICTrenchDepthKm,
			PhaseIIICMaxContinentalElevationKm);
		if (SpeedTransfer <= 0.0 || ReliefTransfer <= 0.0)
		{
			++LastPhaseIIIC3UpliftAudit.InvalidInputCount;
			continue;
		}

		for (int32 OverVertexId = 0; OverVertexId < OverPlate.Vertices.Num(); ++OverVertexId)
		{
			CarrierLab::FCarrierVertex& OverVertex = OverPlate.Vertices[OverVertexId];
			if (OverVertex.ContinentalFraction <= 0.5)
			{
				++LastPhaseIIIC3UpliftAudit.SkippedNonContinentalVertexCount;
				continue;
			}

			const double Dot = FMath::Clamp(FVector3d::DotProduct(UnderBarycenter, OverVertex.UnitPosition), -1.0, 1.0);
			const double DistanceKm = FMath::Acos(Dot) * EarthRadiusKm;
			if (!FMath::IsFinite(DistanceKm) || DistanceKm > PhaseIIICSubductionEffectRadiusKm)
			{
				++LastPhaseIIIC3UpliftAudit.SkippedOutsideRadiusCount;
				continue;
			}

			const double DistanceTransfer = PhaseIIIC3DistanceTransfer(DistanceKm, PhaseIIICSubductionEffectRadiusKm);
			const double DeltaKm = PhaseIIIC3UpliftDeltaKm(
				PhaseIIICSubductionUpliftMmPerYear,
				DistanceTransfer,
				SpeedTransfer,
				ReliefTransfer);
			if (!FMath::IsFinite(DeltaKm) || DeltaKm <= 0.0)
			{
				++LastPhaseIIIC3UpliftAudit.InvalidInputCount;
				continue;
			}

			const double PreviousElevationKm = OverVertex.Elevation;
			OverVertex.Elevation += DeltaKm;
			AddPhaseIIIC5ElevationLedgerRecord(
				ECarrierLabPhaseIIIC5ElevationLedgerClass::OverridingUplift,
				Mark.MarkId,
				Mark.OtherPlateId,
				Mark.PlateId,
				Mark.LocalTriangleId,
				OverVertexId,
				OverVertex.GlobalSampleId,
				PreviousElevationKm,
				OverVertex.Elevation,
				Mark.SignedConvergenceVelocity);

			if (Motions.IsValidIndex(Mark.PlateId) && Motions.IsValidIndex(Mark.OtherPlateId))
			{
				const FVector3d UnderVelocity = FVector3d::CrossProduct(Motions[Mark.PlateId].Axis, OverVertex.UnitPosition) *
					Motions[Mark.PlateId].AngularSpeedRadiansPerStep;
				const FVector3d OverVelocity = FVector3d::CrossProduct(Motions[Mark.OtherPlateId].Axis, OverVertex.UnitPosition) *
					Motions[Mark.OtherPlateId].AngularSpeedRadiansPerStep;
				const FVector3d RelativeConvergence = RetangentAndNormalizeVectorField(UnderVelocity - OverVelocity, OverVertex.UnitPosition);
				if (RelativeConvergence.SquaredLength() > UE_DOUBLE_SMALL_NUMBER)
				{
					OverVertex.FoldDirection = RetangentAndNormalizeVectorField(
						OverVertex.FoldDirection + RelativeConvergence * DeltaKm,
						OverVertex.UnitPosition);
				}
			}

			const uint64 VertexKey = MakePlateVertexKey(Mark.OtherPlateId, OverVertexId);
			UpliftedVertexKeys.Add(VertexKey);
			++LastPhaseIIIC3UpliftAudit.UpliftRecordCount;
			LastPhaseIIIC3UpliftAudit.TotalAppliedDeltaKm += DeltaKm;
			LastPhaseIIIC3UpliftAudit.MaxAppliedDeltaKm = FMath::Max(
				LastPhaseIIIC3UpliftAudit.MaxAppliedDeltaKm,
				DeltaKm);

			HashMix(UpliftHash, static_cast<uint64>(Mark.MarkId + 1));
			HashMix(UpliftHash, static_cast<uint64>(Mark.PlateId + 1));
			HashMix(UpliftHash, static_cast<uint64>(Mark.OtherPlateId + 1));
			HashMix(UpliftHash, static_cast<uint64>(Mark.LocalTriangleId + 1));
			HashMix(UpliftHash, static_cast<uint64>(OverVertexId + 1));
			HashMix(UpliftHash, static_cast<uint64>(OverVertex.GlobalSampleId + 1));
			HashMixDouble(UpliftHash, DistanceKm);
			HashMixDouble(UpliftHash, Mark.SignedConvergenceVelocity);
			HashMixDouble(UpliftHash, HistoricalElevationKm);
			HashMixDouble(UpliftHash, PreviousElevationKm);
			HashMixDouble(UpliftHash, DeltaKm);
			HashMixDouble(UpliftHash, OverVertex.Elevation);

			FCarrierLabPhaseIIIC3UpliftAuditRecord& Record = LastPhaseIIIC3UpliftAudit.Records.AddDefaulted_GetRef();
			Record.MarkId = Mark.MarkId;
			Record.UnderPlateId = Mark.PlateId;
			Record.OverPlateId = Mark.OtherPlateId;
			Record.UnderLocalTriangleId = Mark.LocalTriangleId;
			Record.OverLocalVertexId = OverVertexId;
			Record.OverGlobalSampleId = OverVertex.GlobalSampleId;
			Record.DistanceKm = DistanceKm;
			Record.SignedConvergenceVelocity = Mark.SignedConvergenceVelocity;
			Record.HistoricalElevationKm = HistoricalElevationKm;
			Record.PreviousElevationKm = PreviousElevationKm;
			Record.AppliedDeltaKm = DeltaKm;
			Record.NewElevationKm = OverVertex.Elevation;
			Record.DistanceTransfer = DistanceTransfer;
			Record.SpeedTransfer = SpeedTransfer;
			Record.ReliefTransfer = ReliefTransfer;
			Record.FoldDirectionMagnitude = OverVertex.FoldDirection.Size();
		}
	}

	LastPhaseIIIC3UpliftAudit.UniqueUpliftedVertexCount = UpliftedVertexKeys.Num();
	LastPhaseIIIC3UpliftAudit.UpliftHash = HashToString(UpliftHash);
}

void ACarrierLabVisualizationActor::ApplyPhaseIIIC4SlabPull()
{
	LastPhaseIIIC4SlabPullAudit = FCarrierLabPhaseIIIC4SlabPullAudit();
	LastPhaseIIIC4SlabPullAudit.Step = CurrentMetrics.Step + 1;
	LastPhaseIIIC4SlabPullAudit.EventCount = CurrentMetrics.EventCount;
	LastPhaseIIIC4SlabPullAudit.PlateCount = State.Plates.Num();
	LastPhaseIIIC4SlabPullAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	LastPhaseIIIC4SlabPullAudit.bMarksEnabled = bEnablePhaseIIICSubductingMarks;
	LastPhaseIIIC4SlabPullAudit.bSlabPullEnabled = bEnablePhaseIIICSlabPull;
	LastPhaseIIIC4SlabPullAudit.MarkCount = State.ConvergenceSubductingTriangleMarks.Num();
	LastPhaseIIIC4SlabPullAudit.SlabPullSpeedMmPerYear = PhaseIIICSlabPullSpeedMmPerYear;
	LastPhaseIIIC4SlabPullAudit.ReferenceVelocityMmPerYear = PhaseIIICReferenceVelocityMmPerYear;
	LastPhaseIIIC4SlabPullAudit.SlabPullAngularStep = AngularSpeedRadiansPerStep(PhaseIIICSlabPullSpeedMmPerYear);
	LastPhaseIIIC4SlabPullAudit.MaxAllowedAngularStep = AngularSpeedRadiansPerStep(PhaseIIICReferenceVelocityMmPerYear);
	LastPhaseIIIC4SlabPullAudit.MotionHashBefore = ComputeMotionStateHash(Motions);

	uint64 SlabPullHash = 1469598103934665603ull;
	HashMix(SlabPullHash, static_cast<uint64>(LastPhaseIIIC4SlabPullAudit.Step + 1));
	HashMix(SlabPullHash, static_cast<uint64>(State.ConvergenceSubductingTriangleMarks.Num() + 1));
	HashMix(SlabPullHash, bEnablePhaseIIICSlabPull ? 1ull : 0ull);
	HashMixDouble(SlabPullHash, PhaseIIICSlabPullSpeedMmPerYear);
	HashMixDouble(SlabPullHash, PhaseIIICReferenceVelocityMmPerYear);
	HashMixDouble(SlabPullHash, LastPhaseIIIC4SlabPullAudit.SlabPullAngularStep);
	HashMixDouble(SlabPullHash, LastPhaseIIIC4SlabPullAudit.MaxAllowedAngularStep);

	if (!bEnablePhaseIIICSlabPull ||
		!bEnablePhaseIIICSubductingMarks ||
		State.ConvergenceSubductingTriangleMarks.IsEmpty())
	{
		LastPhaseIIIC4SlabPullAudit.MotionHashAfter = LastPhaseIIIC4SlabPullAudit.MotionHashBefore;
		LastPhaseIIIC4SlabPullAudit.SlabPullHash = HashToString(SlabPullHash);
		return;
	}

	TMap<int32, FVector3d> ContributionSumsByPlate;
	TMap<int32, int32> ContributionCountsByPlate;
	for (const CarrierLab::FConvergenceSubductingTriangleMark& Mark : State.ConvergenceSubductingTriangleMarks)
	{
		if (!State.Plates.IsValidIndex(Mark.PlateId) ||
			!State.Plates[Mark.PlateId].LocalTriangles.IsValidIndex(Mark.LocalTriangleId) ||
			!Motions.IsValidIndex(Mark.PlateId))
		{
			++LastPhaseIIIC4SlabPullAudit.InvalidInputCount;
			continue;
		}

		const CarrierLab::FCarrierPlate& Plate = State.Plates[Mark.PlateId];
		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[Mark.LocalTriangleId];
		const int32 VertexIds[3] = { Triangle.A, Triangle.B, Triangle.C };
		FVector3d FrontBarycenter = FVector3d::ZeroVector;
		bool bAllVerticesValid = true;
		for (const int32 VertexId : VertexIds)
		{
			if (!Plate.Vertices.IsValidIndex(VertexId))
			{
				bAllVerticesValid = false;
				break;
			}
			FrontBarycenter += Plate.Vertices[VertexId].UnitPosition;
		}
		if (!bAllVerticesValid)
		{
			++LastPhaseIIIC4SlabPullAudit.InvalidInputCount;
			continue;
		}
		FrontBarycenter = NormalizeOrFallback(FrontBarycenter, Plate.Vertices[VertexIds[0]].UnitPosition);

		const FVector3d PlateCenter = Motions[Mark.PlateId].CurrentCenter;
		const FVector3d ContributionRaw = FVector3d::CrossProduct(PlateCenter, FrontBarycenter);
		if (ContributionRaw.SquaredLength() <= UE_DOUBLE_SMALL_NUMBER)
		{
			++LastPhaseIIIC4SlabPullAudit.InvalidInputCount;
			continue;
		}
		const FVector3d ContributionUnit = ContributionRaw.GetSafeNormal();
		ContributionSumsByPlate.FindOrAdd(Mark.PlateId) += ContributionUnit;
		ContributionCountsByPlate.FindOrAdd(Mark.PlateId) += 1;
		++LastPhaseIIIC4SlabPullAudit.ContributionCount;

		FCarrierLabPhaseIIIC4SlabPullContributionRecord& Record =
			LastPhaseIIIC4SlabPullAudit.Contributions.AddDefaulted_GetRef();
		Record.MarkId = Mark.MarkId;
		Record.PlateId = Mark.PlateId;
		Record.OtherPlateId = Mark.OtherPlateId;
		Record.LocalTriangleId = Mark.LocalTriangleId;
		Record.PlateCenter = PlateCenter;
		Record.FrontBarycenter = FrontBarycenter;
		Record.ContributionUnit = ContributionUnit;
		Record.SignedConvergenceVelocity = Mark.SignedConvergenceVelocity;

		HashMix(SlabPullHash, static_cast<uint64>(Mark.MarkId + 1));
		HashMix(SlabPullHash, static_cast<uint64>(Mark.PlateId + 1));
		HashMix(SlabPullHash, static_cast<uint64>(Mark.OtherPlateId + 1));
		HashMix(SlabPullHash, static_cast<uint64>(Mark.LocalTriangleId + 1));
		HashMixDouble(SlabPullHash, PlateCenter.X);
		HashMixDouble(SlabPullHash, PlateCenter.Y);
		HashMixDouble(SlabPullHash, PlateCenter.Z);
		HashMixDouble(SlabPullHash, FrontBarycenter.X);
		HashMixDouble(SlabPullHash, FrontBarycenter.Y);
		HashMixDouble(SlabPullHash, FrontBarycenter.Z);
		HashMixDouble(SlabPullHash, ContributionUnit.X);
		HashMixDouble(SlabPullHash, ContributionUnit.Y);
		HashMixDouble(SlabPullHash, ContributionUnit.Z);
		HashMixDouble(SlabPullHash, Mark.SignedConvergenceVelocity);
	}

	TArray<int32> AffectedPlateIds;
	ContributionSumsByPlate.GetKeys(AffectedPlateIds);
	AffectedPlateIds.Sort();
	LastPhaseIIIC4SlabPullAudit.AffectedPlateCount = AffectedPlateIds.Num();
	for (const int32 PlateId : AffectedPlateIds)
	{
		if (!Motions.IsValidIndex(PlateId))
		{
			++LastPhaseIIIC4SlabPullAudit.InvalidInputCount;
			continue;
		}

		FCarrierLabVisualizationMotion& Motion = Motions[PlateId];
		const FVector3d OldAxis = Motion.Axis;
		const double OldAngularSpeed = Motion.AngularSpeedRadiansPerStep;
		const FVector3d OldOmega = OldAxis * OldAngularSpeed;
		const FVector3d ContributionSum = ContributionSumsByPlate[PlateId];
		const FVector3d RawOmega = OldOmega + ContributionSum * LastPhaseIIIC4SlabPullAudit.SlabPullAngularStep;
		const double RawAngularSpeed = RawOmega.Size();

		FVector3d NewOmega = RawOmega;
		bool bClamped = false;
		if (RawAngularSpeed > LastPhaseIIIC4SlabPullAudit.MaxAllowedAngularStep &&
			RawAngularSpeed > UE_DOUBLE_SMALL_NUMBER)
		{
			NewOmega = RawOmega / RawAngularSpeed * LastPhaseIIIC4SlabPullAudit.MaxAllowedAngularStep;
			bClamped = true;
		}

		const double NewAngularSpeed = NewOmega.Size();
		if (NewAngularSpeed > UE_DOUBLE_SMALL_NUMBER)
		{
			Motion.Axis = NewOmega / NewAngularSpeed;
			Motion.AngularSpeedRadiansPerStep = NewAngularSpeed;
		}
		else
		{
			Motion.Axis = OldAxis;
			Motion.AngularSpeedRadiansPerStep = 0.0;
		}

		const double NewVelocity = VelocityMmPerYearFromAngularSpeed(Motion.AngularSpeedRadiansPerStep);
		LastPhaseIIIC4SlabPullAudit.MaxVelocityMmPerYear = FMath::Max(
			LastPhaseIIIC4SlabPullAudit.MaxVelocityMmPerYear,
			NewVelocity);

		FCarrierLabPhaseIIIC4SlabPullPlateRecord& Record =
			LastPhaseIIIC4SlabPullAudit.PlateRecords.AddDefaulted_GetRef();
		Record.PlateId = PlateId;
		Record.ContributionCount = ContributionCountsByPlate.Contains(PlateId) ? ContributionCountsByPlate[PlateId] : 0;
		Record.OldAxis = OldAxis;
		Record.NewAxis = Motion.Axis;
		Record.ContributionSum = ContributionSum;
		Record.OldAngularSpeedRadiansPerStep = OldAngularSpeed;
		Record.RawAngularSpeedRadiansPerStep = RawAngularSpeed;
		Record.NewAngularSpeedRadiansPerStep = Motion.AngularSpeedRadiansPerStep;
		Record.MaxAllowedAngularSpeedRadiansPerStep = LastPhaseIIIC4SlabPullAudit.MaxAllowedAngularStep;
		Record.NewVelocityMmPerYear = NewVelocity;
		Record.bClampedToReferenceSpeed = bClamped;

		HashMix(SlabPullHash, static_cast<uint64>(PlateId + 1));
		HashMix(SlabPullHash, static_cast<uint64>(Record.ContributionCount + 1));
		HashMixDouble(SlabPullHash, OldAxis.X);
		HashMixDouble(SlabPullHash, OldAxis.Y);
		HashMixDouble(SlabPullHash, OldAxis.Z);
		HashMixDouble(SlabPullHash, OldAngularSpeed);
		HashMixDouble(SlabPullHash, ContributionSum.X);
		HashMixDouble(SlabPullHash, ContributionSum.Y);
		HashMixDouble(SlabPullHash, ContributionSum.Z);
		HashMixDouble(SlabPullHash, RawAngularSpeed);
		HashMixDouble(SlabPullHash, Motion.Axis.X);
		HashMixDouble(SlabPullHash, Motion.Axis.Y);
		HashMixDouble(SlabPullHash, Motion.Axis.Z);
		HashMixDouble(SlabPullHash, Motion.AngularSpeedRadiansPerStep);
		HashMixDouble(SlabPullHash, NewVelocity);
		HashMix(SlabPullHash, bClamped ? 1ull : 0ull);
	}

	LastPhaseIIIC4SlabPullAudit.MotionHashAfter = ComputeMotionStateHash(Motions);
	LastPhaseIIIC4SlabPullAudit.SlabPullHash = HashToString(SlabPullHash);
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
			Vertex.RidgeDirection = RotateCarrierVectorField(Vertex.RidgeDirection, Motion.Axis, Motion.AngularSpeedRadiansPerStep);
			Vertex.FoldDirection = RotateCarrierVectorField(Vertex.FoldDirection, Motion.Axis, Motion.AngularSpeedRadiansPerStep);
		}
	}
	for (FCarrierLabVisualizationMotion& Motion : Motions)
	{
		Motion.CurrentCenter = NormalizeOrFallback(RotateVector(Motion.CurrentCenter, Motion.Axis, Motion.AngularSpeedRadiansPerStep), Motion.CurrentCenter);
	}
	UpdateConvergenceTrackingDistances();
	UpdateConvergenceSubductionMatrix();
	UpdateConvergenceSubductionPolarityDecisions();
	UpdateConvergenceNeighborPropagation();
	BeginPhaseIIIC5ElevationLedger();
	if (bEnablePhaseIIICSubductingMarks)
	{
		UpdatePhaseIIICSubductingTriangleMarks();
	}
	if (bEnablePhaseIIICOverridingPlateUplift)
	{
		ApplyPhaseIIIC3OverridingPlateUplift();
	}
	else
	{
		LastPhaseIIIC3UpliftAudit = FCarrierLabPhaseIIIC3UpliftAudit();
		LastPhaseIIIC3UpliftAudit.Step = CurrentMetrics.Step + 1;
		LastPhaseIIIC3UpliftAudit.EventCount = CurrentMetrics.EventCount;
		LastPhaseIIIC3UpliftAudit.PlateCount = State.Plates.Num();
		LastPhaseIIIC3UpliftAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
		LastPhaseIIIC3UpliftAudit.bMarksEnabled = bEnablePhaseIIICSubductingMarks;
		LastPhaseIIIC3UpliftAudit.bElevationSplitEnabled = bEnablePhaseIIICVisibleHistoricalElevation;
		LastPhaseIIIC3UpliftAudit.bUpliftEnabled = false;
		LastPhaseIIIC3UpliftAudit.MarkCount = State.ConvergenceSubductingTriangleMarks.Num();
	}
	ApplyPhaseIIIC4SlabPull();
	FinalizePhaseIIIC5ElevationLedger();
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

void ACarrierLabVisualizationActor::ComputePhaseIIIObservabilityMasks()
{
	SubductionRoleMask.Init(0, State.Samples.Num());
	DistanceToFrontKmBySample.Init(-1.0, State.Samples.Num());

	auto MarkTriangleVertices = [this](const CarrierLab::FCarrierPlate& Plate, const int32 LocalTriangleId, const uint8 MaskValue)
	{
		if (!Plate.LocalTriangles.IsValidIndex(LocalTriangleId))
		{
			return;
		}
		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
		const int32 VertexIds[3] = { Triangle.A, Triangle.B, Triangle.C };
		for (const int32 LocalVertexId : VertexIds)
		{
			if (!Plate.Vertices.IsValidIndex(LocalVertexId))
			{
				continue;
			}
			const int32 SampleId = Plate.Vertices[LocalVertexId].GlobalSampleId;
			if (!SubductionRoleMask.IsValidIndex(SampleId))
			{
				continue;
			}
			SubductionRoleMask[SampleId] = FMath::Max(SubductionRoleMask[SampleId], MaskValue);
		}
	};

	auto MarkTriangleDistance = [this](const CarrierLab::FCarrierPlate& Plate, const int32 LocalTriangleId, const double DistanceKm)
	{
		if (!Plate.LocalTriangles.IsValidIndex(LocalTriangleId) || !FMath::IsFinite(DistanceKm))
		{
			return;
		}
		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
		const int32 VertexIds[3] = { Triangle.A, Triangle.B, Triangle.C };
		for (const int32 LocalVertexId : VertexIds)
		{
			if (!Plate.Vertices.IsValidIndex(LocalVertexId))
			{
				continue;
			}
			const int32 SampleId = Plate.Vertices[LocalVertexId].GlobalSampleId;
			if (!DistanceToFrontKmBySample.IsValidIndex(SampleId))
			{
				continue;
			}
			const double Previous = DistanceToFrontKmBySample[SampleId];
			DistanceToFrontKmBySample[SampleId] = Previous < 0.0 ? DistanceKm : FMath::Min(Previous, DistanceKm);
		}
	};

	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		for (int32 Index = 0; Index < Plate.ActiveBoundaryTriangles.Num(); ++Index)
		{
			const double DistanceKm = Plate.ActiveBoundaryTriangleDistancesKm.IsValidIndex(Index)
				? Plate.ActiveBoundaryTriangleDistancesKm[Index]
				: 0.0;
			MarkTriangleDistance(Plate, Plate.ActiveBoundaryTriangles[Index], DistanceKm);
		}
	}

	TMap<uint64, CarrierLab::FConvergenceSubductionPolarityDecision> DecisionsByPair;
	for (const CarrierLab::FConvergenceSubductionPolarityDecision& Decision : State.ConvergenceSubductionPolarityDecisions)
	{
		DecisionsByPair.Add(Decision.PairKey, Decision);
	}

	for (const CarrierLab::FConvergenceSubductionTriangleHit& Hit : State.ConvergenceSubductionTriangleHits)
	{
		if (!State.Plates.IsValidIndex(Hit.PlateId))
		{
			continue;
		}

		uint8 MaskValue = 0;
		const CarrierLab::FConvergenceSubductionPolarityDecision* Decision = DecisionsByPair.Find(Hit.PairKey);
		if (Decision != nullptr)
		{
			if (IsSubductionPolarityDecision(*Decision))
			{
				MaskValue = Hit.PlateId == Decision->UnderPlate ? 1 : (Hit.PlateId == Decision->OverPlate ? 2 : 0);
			}
			else if (Decision->DecisionClass == CarrierLab::EConvergenceSubductionPolarityClass::CollisionCandidate)
			{
				MaskValue = 3;
			}
			else if (Decision->DecisionClass == CarrierLab::EConvergenceSubductionPolarityClass::OceanOceanDeferred)
			{
				MaskValue = 4;
			}
		}
		if (MaskValue != 0)
		{
			MarkTriangleVertices(State.Plates[Hit.PlateId], Hit.LocalTriangleId, MaskValue);
		}
	}

	for (const CarrierLab::FConvergenceSubductingTriangleMark& Mark : State.ConvergenceSubductingTriangleMarks)
	{
		if (!State.Plates.IsValidIndex(Mark.PlateId))
		{
			continue;
		}
		MarkTriangleVertices(State.Plates[Mark.PlateId], Mark.LocalTriangleId, 1);
	}
}

bool ACarrierLabVisualizationActor::GetPhaseIIIProcessOverlayTriangles(
	TArray<FCarrierLabPhaseIIIProcessOverlayTriangle>& OutRoleTriangles,
	TArray<FCarrierLabPhaseIIIProcessOverlayTriangle>& OutDistanceTriangles,
	TArray<FCarrierLabPhaseIIIProcessOverlayTriangle>& OutPlateBoundaryTriangles) const
{
	OutRoleTriangles.Reset();
	OutDistanceTriangles.Reset();
	OutPlateBoundaryTriangles.Reset();

	auto AddTriangle = [this](
		const int32 PlateId,
		const int32 LocalTriangleId,
		const uint8 OverlayRole,
		const double DistanceKm,
		TArray<FCarrierLabPhaseIIIProcessOverlayTriangle>& OutTriangles)
	{
		if (!State.Plates.IsValidIndex(PlateId))
		{
			return;
		}
		const CarrierLab::FCarrierPlate& Plate = State.Plates[PlateId];
		if (!Plate.LocalTriangles.IsValidIndex(LocalTriangleId))
		{
			return;
		}

		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
		if (!Plate.Vertices.IsValidIndex(Triangle.A) ||
			!Plate.Vertices.IsValidIndex(Triangle.B) ||
			!Plate.Vertices.IsValidIndex(Triangle.C))
		{
			return;
		}

		FCarrierLabPhaseIIIProcessOverlayTriangle& Overlay = OutTriangles.AddDefaulted_GetRef();
		Overlay.A = Plate.Vertices[Triangle.A].UnitPosition;
		Overlay.B = Plate.Vertices[Triangle.B].UnitPosition;
		Overlay.C = Plate.Vertices[Triangle.C].UnitPosition;
		Overlay.PlateId = PlateId;
		Overlay.Role = OverlayRole;
		const auto SourcePlateId = [this, &Plate](const int32 LocalVertexId) -> int32
		{
			if (!Plate.Vertices.IsValidIndex(LocalVertexId))
			{
				return INDEX_NONE;
			}
			const int32 GlobalSampleId = Plate.Vertices[LocalVertexId].GlobalSampleId;
			return State.Samples.IsValidIndex(GlobalSampleId)
				? State.Samples[GlobalSampleId].PlateId
				: INDEX_NONE;
		};
		const int32 SourcePlateA = SourcePlateId(Triangle.A);
		const int32 SourcePlateB = SourcePlateId(Triangle.B);
		const int32 SourcePlateC = SourcePlateId(Triangle.C);
		Overlay.BoundaryEdgeMask = 0;
		if (SourcePlateA != INDEX_NONE && SourcePlateB != INDEX_NONE && SourcePlateA != SourcePlateB)
		{
			Overlay.BoundaryEdgeMask |= 1u;
		}
		if (SourcePlateB != INDEX_NONE && SourcePlateC != INDEX_NONE && SourcePlateB != SourcePlateC)
		{
			Overlay.BoundaryEdgeMask |= 2u;
		}
		if (SourcePlateC != INDEX_NONE && SourcePlateA != INDEX_NONE && SourcePlateC != SourcePlateA)
		{
			Overlay.BoundaryEdgeMask |= 4u;
		}
		Overlay.DistanceKm = DistanceKm;
	};

	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		for (int32 LocalTriangleId = 0; LocalTriangleId < Plate.LocalTriangles.Num(); ++LocalTriangleId)
		{
			if (Plate.LocalTriangles[LocalTriangleId].bBoundary)
			{
				AddTriangle(Plate.PlateId, LocalTriangleId, 0, -1.0, OutPlateBoundaryTriangles);
			}
		}

		for (int32 Index = 0; Index < Plate.ActiveBoundaryTriangles.Num(); ++Index)
		{
			const double DistanceKm = Plate.ActiveBoundaryTriangleDistancesKm.IsValidIndex(Index)
				? Plate.ActiveBoundaryTriangleDistancesKm[Index]
				: 0.0;
			AddTriangle(Plate.PlateId, Plate.ActiveBoundaryTriangles[Index], 0, DistanceKm, OutDistanceTriangles);
		}
	}

	TMap<uint64, CarrierLab::FConvergenceSubductionPolarityDecision> DecisionsByPair;
	for (const CarrierLab::FConvergenceSubductionPolarityDecision& Decision : State.ConvergenceSubductionPolarityDecisions)
	{
		DecisionsByPair.Add(Decision.PairKey, Decision);
	}

	for (const CarrierLab::FConvergenceSubductionTriangleHit& Hit : State.ConvergenceSubductionTriangleHits)
	{
		uint8 OverlayRole = 0;
		const CarrierLab::FConvergenceSubductionPolarityDecision* Decision = DecisionsByPair.Find(Hit.PairKey);
		if (Decision != nullptr)
		{
			if (IsSubductionPolarityDecision(*Decision))
			{
				OverlayRole = Hit.PlateId == Decision->UnderPlate ? 1 : (Hit.PlateId == Decision->OverPlate ? 2 : 0);
			}
			else if (Decision->DecisionClass == CarrierLab::EConvergenceSubductionPolarityClass::CollisionCandidate)
			{
				OverlayRole = 3;
			}
			else if (Decision->DecisionClass == CarrierLab::EConvergenceSubductionPolarityClass::OceanOceanDeferred)
			{
				OverlayRole = 4;
			}
		}

		if (OverlayRole != 0)
		{
			AddTriangle(Hit.PlateId, Hit.LocalTriangleId, OverlayRole, -1.0, OutRoleTriangles);
		}
	}

	for (const CarrierLab::FConvergenceSubductingTriangleMark& Mark : State.ConvergenceSubductingTriangleMarks)
	{
		AddTriangle(Mark.PlateId, Mark.LocalTriangleId, 1, -1.0, OutRoleTriangles);
	}

	OutRoleTriangles.Sort([](
		const FCarrierLabPhaseIIIProcessOverlayTriangle& A,
		const FCarrierLabPhaseIIIProcessOverlayTriangle& B)
	{
		return A.Role < B.Role;
	});
	return true;
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
	RenderElevations.Init(0.0, State.Samples.Num());
	DriftErrorKmBySample.Init(-1.0, State.Samples.Num());
	MissMask.Init(0, State.Samples.Num());
	OverlapMask.Init(0, State.Samples.Num());
	BoundaryMask.Init(0, State.Samples.Num());
	PlateBoundaryMask.Init(0, State.Samples.Num());
	SubductionRoleMask.Init(0, State.Samples.Num());
	DistanceToFrontKmBySample.Init(-1.0, State.Samples.Num());

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
		if (Chosen != nullptr && State.Plates.IsValidIndex(ResolvedPlateId) && RenderElevations.IsValidIndex(OutputIndex))
		{
			FCarrierLabCrustFields ProjectedFields;
			if (InterpolateCrustFields(State.Plates[ResolvedPlateId], *Chosen, Sample.UnitPosition, ProjectedFields))
			{
				RenderElevations[OutputIndex] = ProjectedFields.Elevation;
			}
		}
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
	ComputePhaseIIIObservabilityMasks();
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
		if (Sample.HistoricalElevation != 0.0)
		{
			HashMixDouble(CrustHash, Sample.HistoricalElevation);
		}
		HashMixDouble(CrustHash, Sample.OceanicAge);
		HashMixDouble(CrustHash, Sample.RidgeDirection.X);
		HashMixDouble(CrustHash, Sample.RidgeDirection.Y);
		HashMixDouble(CrustHash, Sample.RidgeDirection.Z);
		HashMixDouble(CrustHash, Sample.FoldDirection.X);
		HashMixDouble(CrustHash, Sample.FoldDirection.Y);
		HashMixDouble(CrustHash, Sample.FoldDirection.Z);
	}
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		HashMix(CrustHash, static_cast<uint64>(Plate.PlateId + 1));
		HashMix(CrustHash, static_cast<uint64>(Plate.Vertices.Num() + 1));
		for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			HashMix(CrustHash, static_cast<uint64>(Vertex.GlobalSampleId + 1));
			HashMixDouble(CrustHash, Vertex.Elevation);
			if (Vertex.HistoricalElevation != 0.0 || Vertex.bHasHistoricalElevationSnapshot)
			{
				HashMixDouble(CrustHash, Vertex.HistoricalElevation);
			}
			HashMixDouble(CrustHash, Vertex.OceanicAge);
			HashMixDouble(CrustHash, Vertex.RidgeDirection.X);
			HashMixDouble(CrustHash, Vertex.RidgeDirection.Y);
			HashMixDouble(CrustHash, Vertex.RidgeDirection.Z);
			HashMixDouble(CrustHash, Vertex.FoldDirection.X);
			HashMixDouble(CrustHash, Vertex.FoldDirection.Y);
			HashMixDouble(CrustHash, Vertex.FoldDirection.Z);
			if (Vertex.bHasHistoricalElevationSnapshot)
			{
				HashMix(CrustHash, 1ull);
			}
		}
	}
	CurrentMetrics.CrustStateHash = HashToString(CrustHash);

	uint64 VisibleElevationHash = 1469598103934665603ull;
	uint64 HistoricalElevationHash = 1469598103934665603ull;
	HashMix(VisibleElevationHash, static_cast<uint64>(CurrentMetrics.Step + 1));
	HashMix(VisibleElevationHash, static_cast<uint64>(CurrentMetrics.EventCount + 1));
	HashMix(HistoricalElevationHash, static_cast<uint64>(CurrentMetrics.Step + 1));
	HashMix(HistoricalElevationHash, static_cast<uint64>(CurrentMetrics.EventCount + 1));
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		HashMix(VisibleElevationHash, static_cast<uint64>(Sample.Id + 1));
		HashMixDouble(VisibleElevationHash, Sample.Elevation);
		HashMix(HistoricalElevationHash, static_cast<uint64>(Sample.Id + 1));
		HashMixDouble(HistoricalElevationHash, Sample.HistoricalElevation);
	}
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		HashMix(VisibleElevationHash, static_cast<uint64>(Plate.PlateId + 1));
		HashMix(HistoricalElevationHash, static_cast<uint64>(Plate.PlateId + 1));
		for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			HashMix(VisibleElevationHash, static_cast<uint64>(Vertex.GlobalSampleId + 1));
			HashMixDouble(VisibleElevationHash, Vertex.Elevation);
			HashMix(HistoricalElevationHash, static_cast<uint64>(Vertex.GlobalSampleId + 1));
			HashMixDouble(HistoricalElevationHash, Vertex.HistoricalElevation);
			HashMix(HistoricalElevationHash, Vertex.bHasHistoricalElevationSnapshot ? 1ull : 0ull);
		}
	}
	CurrentMetrics.VisibleElevationHash = HashToString(VisibleElevationHash);
	CurrentMetrics.HistoricalElevationHash = HashToString(HistoricalElevationHash);
	CurrentMetrics.ConvergenceTrackingHash = HashToString(ComputeConvergenceTrackingHash(State));
}

bool ACarrierLabVisualizationActor::BuildVisualizationLayerMap(
	const ECarrierLabVisualizationLayer Layer,
	TArray<FColor>& OutPixels,
	int32& OutWidth,
	int32& OutHeight) const
{
	if (!bInitialized)
	{
		return false;
	}

	OutWidth = PhaseIIIObservabilityMapWidth;
	OutHeight = PhaseIIIObservabilityMapHeight;
	OutPixels.Init(FColor(0, 0, 0, 255), OutWidth * OutHeight);

	TArray<TArray<int32>> SampleBins;
	SampleBins.SetNum(PhaseIIIObservabilityLookupLonBins * PhaseIIIObservabilityLookupLatBins);
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (!State.Samples.IsValidIndex(Sample.Id))
		{
			continue;
		}
		const FIntPoint Bin = ObservabilityLookupPixelBin(Sample.UnitPosition);
		SampleBins[ObservabilityLookupBin(Bin.X, Bin.Y)].Add(Sample.Id);
	}

	auto MapColorForSample = [this, Layer](const int32 SampleId)
	{
		const FLinearColor Base = ContinentalColor(RenderContinentalFractions.IsValidIndex(SampleId) ? RenderContinentalFractions[SampleId] : 0.0);
		switch (Layer)
		{
		case ECarrierLabVisualizationLayer::ElevationHeatmap:
		{
			const double ElevationKm = RenderElevations.IsValidIndex(SampleId)
				? RenderElevations[SampleId]
				: (State.Samples.IsValidIndex(SampleId) ? State.Samples[SampleId].Elevation : 0.0);
			return FMath::Abs(ElevationKm) <= 1.0e-9
				? Base
				: BlendMapOverlay(DimMapBase(Base), ElevationColor(ElevationKm));
		}
		case ECarrierLabVisualizationLayer::SubductionMask:
		{
			const uint8 Role = SubductionRoleMask.IsValidIndex(SampleId) ? SubductionRoleMask[SampleId] : 0;
			return Role == 0 ? DimMapBase(Base) : BlendMapOverlay(DimMapBase(Base), SubductionRoleColor(Role));
		}
		case ECarrierLabVisualizationLayer::DistanceToFrontHeatmap:
		{
			const double DistanceKm = DistanceToFrontKmBySample.IsValidIndex(SampleId) ? DistanceToFrontKmBySample[SampleId] : -1.0;
			return (DistanceKm < 0.0 || !FMath::IsFinite(DistanceKm))
				? DimMapBase(Base)
				: BlendMapOverlay(DimMapBase(Base), DistanceToFrontColor(DistanceKm));
		}
		default:
			return ColorForSampleLayer(SampleId, Layer);
		}
	};

	for (int32 Y = 0; Y < OutHeight; ++Y)
	{
		for (int32 X = 0; X < OutWidth; ++X)
		{
			FVector3d UnitPosition;
			if (!InverseMollweidePixel(X, Y, UnitPosition))
			{
				continue;
			}

			const int32 SampleId = FindNearestObservabilitySample(UnitPosition, State.Samples, SampleBins);
			if (SampleId == INDEX_NONE)
			{
				continue;
			}

			FColor Color = MapColorForSample(SampleId).ToFColor(true);
			Color.A = 255;
			OutPixels[Y * OutWidth + X] = Color;
		}
	}
	return true;
}

FLinearColor ACarrierLabVisualizationActor::ColorForSampleLayer(
	const int32 SampleId,
	const ECarrierLabVisualizationLayer Layer) const
{
	switch (Layer)
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
	case ECarrierLabVisualizationLayer::PhaseIIISummary:
	{
		const double ContinentalFraction = RenderContinentalFractions.IsValidIndex(SampleId) ? RenderContinentalFractions[SampleId] : 0.0;
		const double ElevationKm = RenderElevations.IsValidIndex(SampleId)
			? RenderElevations[SampleId]
			: (State.Samples.IsValidIndex(SampleId) ? State.Samples[SampleId].Elevation : 0.0);
		const double DistanceKm = DistanceToFrontKmBySample.IsValidIndex(SampleId) ? DistanceToFrontKmBySample[SampleId] : -1.0;
		const uint8 RoleMask = SubductionRoleMask.IsValidIndex(SampleId) ? SubductionRoleMask[SampleId] : 0;
		FLinearColor Color = ContinentalColor(ContinentalFraction);
		if (PlateBoundaryMask.IsValidIndex(SampleId) && PlateBoundaryMask[SampleId] != 0)
		{
			Color = BlendMapOverlay(DimMapBase(Color), FLinearColor(0.72f, 0.82f, 0.96f, 1.0f), 0.55f);
		}
		if (DistanceKm >= 0.0 && FMath::IsFinite(DistanceKm))
		{
			Color = BlendMapOverlay(DimMapBase(Color), DistanceToFrontColor(DistanceKm), 0.34f);
		}
		if (RoleMask != 0)
		{
			Color = BlendMapOverlay(DimMapBase(Color), SubductionRoleColor(RoleMask), 0.72f);
		}
		if (FMath::Abs(ElevationKm) > 1.0e-9)
		{
			Color = BlendMapOverlay(Color, ElevationColor(ElevationKm), 0.58f);
		}
		if (MissMask.IsValidIndex(SampleId) && MissMask[SampleId] != 0)
		{
			Color = BlendMapOverlay(Color, FLinearColor(0.95f, 0.05f, 0.04f, 1.0f), 0.82f);
		}
		else if (OverlapMask.IsValidIndex(SampleId) && OverlapMask[SampleId] != 0)
		{
			Color = BlendMapOverlay(Color, FLinearColor(1.0f, 0.46f, 0.05f, 1.0f), 0.70f);
		}
		return Color;
	}
	case ECarrierLabVisualizationLayer::ElevationHeatmap:
		return ElevationColor(RenderElevations.IsValidIndex(SampleId)
			? RenderElevations[SampleId]
			: (State.Samples.IsValidIndex(SampleId) ? State.Samples[SampleId].Elevation : 0.0));
	case ECarrierLabVisualizationLayer::SubductionMask:
		return SubductionRoleColor(SubductionRoleMask.IsValidIndex(SampleId) ? SubductionRoleMask[SampleId] : 0);
	case ECarrierLabVisualizationLayer::DistanceToFrontHeatmap:
		return DistanceToFrontColor(DistanceToFrontKmBySample.IsValidIndex(SampleId) ? DistanceToFrontKmBySample[SampleId] : -1.0);
	case ECarrierLabVisualizationLayer::PlateId:
	default:
		return PlateColor(RenderPlateIds.IsValidIndex(SampleId) ? RenderPlateIds[SampleId] : INDEX_NONE);
	}
}

FLinearColor ACarrierLabVisualizationActor::ColorForSample(const int32 SampleId) const
{
	return ColorForSampleLayer(SampleId, VisualizationLayer);
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
	case ECarrierLabVisualizationLayer::PhaseIIISummary:
		LayerName = TEXT("phase iii summary");
		break;
	case ECarrierLabVisualizationLayer::ElevationHeatmap:
		LayerName = TEXT("elevation heatmap");
		break;
	case ECarrierLabVisualizationLayer::SubductionMask:
		LayerName = TEXT("subduction mask");
		break;
	case ECarrierLabVisualizationLayer::DistanceToFrontHeatmap:
		LayerName = TEXT("distance to front");
		break;
	case ECarrierLabVisualizationLayer::PlateId:
	default:
		break;
	}

	return FString::Printf(
		TEXT("CarrierLab Phase I Viewer | %s | layer=%s\nstep=%d next_resample=%d events=%d samples=%d plates=%d\nmiss=%d multi=%d boundary_vertices=%d boundary_degenerate=%d gap_fill=%d nonsep_gap=%d nan=%d\nAuthCAF=%.6f ProjCAF=%.6f drift_mean=%.9fkm drift_p95=%.9fkm hash=%s\nprojection=%.3fs bvh=%.3fs query=%.3fs drift=%.3fs boundary=%.3fs hash_time=%.3fs render=%.3fs resample=%.3fs\nSpace play/pause | . step | R resample | 1-8 layers"),
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
