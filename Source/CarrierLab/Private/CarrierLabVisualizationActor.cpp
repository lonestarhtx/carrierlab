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
#include "HAL/PlatformTime.h"
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
	constexpr double ResamplingMMaxMa = 128.0;
	constexpr double ResamplingMMinMa = 32.0;
	constexpr double ResamplingReferenceVelocityMmPerYear = 100.0;
	constexpr double PhaseIIContactVelocityMargin = 1.0e-6;
	constexpr double PhaseIIIBDistanceToFrontLimitKm = 1800.0;
	constexpr int32 PhaseIIIObservabilityMapWidth = 2048;
	constexpr int32 PhaseIIIObservabilityMapHeight = 1024;
	constexpr int32 PhaseIIIObservabilityLookupLonBins = 360;
	constexpr int32 PhaseIIIObservabilityLookupLatBins = 180;
	constexpr int32 BoundaryLonBins = 72;
	constexpr int32 BoundaryLatBins = 36;
	constexpr int32 BoundarySearchRadiusBins = 2;
	constexpr double PhaseIIIE4RidgePeakElevationKm = -1.0;
	constexpr double PhaseIIIE4AbyssalElevationKm = -6.0;
	constexpr double PhaseIIIE4ZGammaGeophysicsReferenceDistanceKm = 3000.0;
	constexpr double PhaseIIIEContinentalOverwriteThreshold = 1.0e-4;
	// Same tolerance family as the IIIE.2 edge-t comparison and IIIE.4
	// independent-oracle residual gates: geometry duplicates only, not a
	// tectonic remesh winner policy.
	constexpr double PhaseIIIE3RayDistanceCoincidenceToleranceKm = 1.0e-9;
	constexpr double PhaseIIIE3ScalarFieldTolerance = 1.0e-9;
	constexpr double PhaseIIIE3ElevationToleranceKm = 1.0e-9;
	constexpr double PhaseIIIE3UnitVectorTolerance = 1.0e-8;
	constexpr double PhaseIIIE63OceanicAgeTieToleranceMa = 1.0e-6;
	constexpr uint8 PhaseIIIELiveRemeshMaskUnresolvedHold = 1u << 0;
	constexpr uint8 PhaseIIIELiveRemeshMaskCoalescedDuplicate = 1u << 1;
	constexpr uint8 PhaseIIIELiveRemeshMaskAppliedGenerated = 1u << 2;
	constexpr uint8 PhaseIIIELiveRemeshMaskRiftingPending = 1u << 3;
	constexpr uint8 PhaseIIIELiveRemeshMaskSharedBoundaryTieBreak = 1u << 4;
	constexpr uint8 PhaseIIIELiveRemeshMaskDistanceTieFallback = 1u << 5;

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

	struct FCarrierLabContinuousBoundaryEdge
	{
		int32 EdgeId = INDEX_NONE;
		int32 PlateId = INDEX_NONE;
		FVector3d StartUnitPosition = FVector3d::UnitX();
		FVector3d EndUnitPosition = FVector3d::UnitY();
		double StartContinentalFraction = 0.0;
		double EndContinentalFraction = 0.0;
		double StartElevation = 0.0;
		double EndElevation = 0.0;
	};

	struct FCarrierLabContinuousBoundaryCandidate
	{
		int32 EdgeId = INDEX_NONE;
		int32 PlateId = INDEX_NONE;
		FVector3d UnitPosition = FVector3d::UnitZ();
		double DistanceKm = 0.0;
		double EdgeT = 0.0;
		double ContinentalFraction = 0.0;
		double Elevation = 0.0;
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

	void HashMixString(uint64& Hash, const FString& Value)
	{
		for (const TCHAR Ch : Value)
		{
			HashMix(Hash, static_cast<uint64>(Ch));
		}
		HashMix(Hash, 0xffull);
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

	double PhaseIIID7CollisionDistanceTransfer(const double DistanceKm, const double InfluenceRadiusKm)
	{
		if (!FMath::IsFinite(DistanceKm) ||
			!FMath::IsFinite(InfluenceRadiusKm) ||
			InfluenceRadiusKm <= UE_DOUBLE_SMALL_NUMBER ||
			DistanceKm < 0.0 ||
			DistanceKm > InfluenceRadiusKm)
		{
			return 0.0;
		}
		const double NormalizedDistance = DistanceKm / InfluenceRadiusKm;
		const double OneMinusSquared = 1.0 - NormalizedDistance * NormalizedDistance;
		return OneMinusSquared * OneMinusSquared;
	}

	double PhaseIIID7CollisionDeltaKm(
		const double CollisionCoefficientPerKm,
		const double TerraneAreaKm2,
		const double DistanceTransfer)
	{
		if (!FMath::IsFinite(CollisionCoefficientPerKm) ||
			!FMath::IsFinite(TerraneAreaKm2) ||
			!FMath::IsFinite(DistanceTransfer) ||
			CollisionCoefficientPerKm <= 0.0 ||
			TerraneAreaKm2 <= 0.0 ||
			DistanceTransfer <= 0.0)
		{
			return 0.0;
		}
		return CollisionCoefficientPerKm * TerraneAreaKm2 * DistanceTransfer;
	}

	double PhaseIIID7InfluenceRadiusKm(
		const double CollisionRadiusConstantKm,
		const double RelativeVelocityMmPerYear,
		const double ReferenceVelocityMmPerYear,
		const double TerraneAreaKm2,
		const double ReferencePlateAreaKm2)
	{
		if (!FMath::IsFinite(CollisionRadiusConstantKm) ||
			!FMath::IsFinite(RelativeVelocityMmPerYear) ||
			!FMath::IsFinite(ReferenceVelocityMmPerYear) ||
			!FMath::IsFinite(TerraneAreaKm2) ||
			!FMath::IsFinite(ReferencePlateAreaKm2) ||
			CollisionRadiusConstantKm <= 0.0 ||
			ReferenceVelocityMmPerYear <= UE_DOUBLE_SMALL_NUMBER ||
			ReferencePlateAreaKm2 <= UE_DOUBLE_SMALL_NUMBER ||
			TerraneAreaKm2 <= 0.0)
		{
			return 0.0;
		}
		const double SpeedScale = FMath::Max(0.0, RelativeVelocityMmPerYear / ReferenceVelocityMmPerYear);
		const double AreaScale = TerraneAreaKm2 / ReferencePlateAreaKm2;
		return (SpeedScale > 0.0 && AreaScale > 0.0)
			? CollisionRadiusConstantKm * FMath::Sqrt(SpeedScale) * AreaScale
			: 0.0;
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

	double ComputePlateLocalTriangleContinentalFraction(
		const CarrierLab::FCarrierPlate& Plate,
		const int32 LocalTriangleId)
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

		return (FMath::Clamp(Plate.Vertices[Triangle.A].ContinentalFraction, 0.0, 1.0) +
			FMath::Clamp(Plate.Vertices[Triangle.B].ContinentalFraction, 0.0, 1.0) +
			FMath::Clamp(Plate.Vertices[Triangle.C].ContinentalFraction, 0.0, 1.0)) / 3.0;
	}

	bool IsPlateLocalTriangleContinental(
		const CarrierLab::FCarrierPlate& Plate,
		const int32 LocalTriangleId)
	{
		return ComputePlateLocalTriangleContinentalFraction(Plate, LocalTriangleId) > 0.5;
	}

	double ComputePlateLocalTriangleAreaWeight(
		const CarrierLab::FCarrierPlate& Plate,
		const int32 LocalTriangleId)
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

		return (FMath::Max(Plate.Vertices[Triangle.A].AreaWeight, 0.0) +
			FMath::Max(Plate.Vertices[Triangle.B].AreaWeight, 0.0) +
			FMath::Max(Plate.Vertices[Triangle.C].AreaWeight, 0.0)) / 3.0;
	}

	struct FPhaseIIID1TerraneComponent
	{
		TArray<int32> LocalTriangleIds;
		TSet<int32> ContinentalTriangleSet;
		int32 ContinentalTriangleCount = 0;
		int32 InnerSeaTriangleCount = 0;
	};

	bool BuildPhaseIIID1TerraneComponent(
		const CarrierLab::FCarrierPlate& Plate,
		const int32 SeedLocalTriangleId,
		FPhaseIIID1TerraneComponent& OutComponent,
		FCarrierLabPhaseIIIDiagnosticCallCounts* Diagnostics = nullptr)
	{
		OutComponent = FPhaseIIID1TerraneComponent();
		if (!Plate.LocalTriangles.IsValidIndex(SeedLocalTriangleId) ||
			!IsPlateLocalTriangleContinental(Plate, SeedLocalTriangleId))
		{
			return false;
		}

		TArray<int32> Queue;
		TSet<int32> ContinentalComponent;
		Queue.Add(SeedLocalTriangleId);
		ContinentalComponent.Add(SeedLocalTriangleId);

		const double ExpansionStartSeconds = FPlatformTime::Seconds();
		for (int32 QueueIndex = 0; QueueIndex < Queue.Num(); ++QueueIndex)
		{
			TArray<int32> Neighbors;
			GetPlateLocalTriangleNeighbors(Plate, Queue[QueueIndex], Neighbors);
			for (const int32 NeighborId : Neighbors)
			{
				if (!Plate.LocalTriangles.IsValidIndex(NeighborId) ||
					ContinentalComponent.Contains(NeighborId) ||
					!IsPlateLocalTriangleContinental(Plate, NeighborId))
				{
					continue;
				}
				ContinentalComponent.Add(NeighborId);
				Queue.Add(NeighborId);
			}
		}
		if (Diagnostics != nullptr)
		{
			Diagnostics->D1ComponentExpansionSeconds += FPlatformTime::Seconds() - ExpansionStartSeconds;
			Diagnostics->D1ExpandedContinentalTriangleCount += ContinentalComponent.Num();
		}

		TSet<int32> TerraneTriangles = ContinentalComponent;
		int32 InnerSeaTriangleCount = 0;
		int32 ScannedOceanicTriangleCount = 0;
		TSet<int32> VisitedOceanicTriangles;
		const double InnerSeaStartSeconds = FPlatformTime::Seconds();
		for (int32 LocalTriangleId = 0; LocalTriangleId < Plate.LocalTriangles.Num(); ++LocalTriangleId)
		{
			if (ContinentalComponent.Contains(LocalTriangleId) ||
				VisitedOceanicTriangles.Contains(LocalTriangleId) ||
				IsPlateLocalTriangleContinental(Plate, LocalTriangleId))
			{
				continue;
			}

			TArray<int32> OceanicQueue;
			TArray<int32> OceanicRegion;
			bool bTouchesPlateBoundary = false;
			bool bAdjacentToComponent = false;
			bool bAdjacentOnlyToComponentContinents = true;

			OceanicQueue.Add(LocalTriangleId);
			VisitedOceanicTriangles.Add(LocalTriangleId);
			for (int32 QueueIndex = 0; QueueIndex < OceanicQueue.Num(); ++QueueIndex)
			{
				const int32 OceanicTriangleId = OceanicQueue[QueueIndex];
				OceanicRegion.Add(OceanicTriangleId);
				++ScannedOceanicTriangleCount;

				const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[OceanicTriangleId];
				if (Triangle.bBoundary)
				{
					bTouchesPlateBoundary = true;
				}

				TArray<int32> Neighbors;
				GetPlateLocalTriangleNeighbors(Plate, OceanicTriangleId, Neighbors);
				if (Neighbors.Num() < 3)
				{
					bTouchesPlateBoundary = true;
				}

				for (const int32 NeighborId : Neighbors)
				{
					if (!Plate.LocalTriangles.IsValidIndex(NeighborId))
					{
						bTouchesPlateBoundary = true;
						continue;
					}

					if (IsPlateLocalTriangleContinental(Plate, NeighborId))
					{
						if (ContinentalComponent.Contains(NeighborId))
						{
							bAdjacentToComponent = true;
						}
						else
						{
							bAdjacentOnlyToComponentContinents = false;
						}
						continue;
					}

					if (!VisitedOceanicTriangles.Contains(NeighborId))
					{
						VisitedOceanicTriangles.Add(NeighborId);
						OceanicQueue.Add(NeighborId);
					}
				}
			}

			if (!bTouchesPlateBoundary &&
				bAdjacentToComponent &&
				bAdjacentOnlyToComponentContinents)
			{
				for (const int32 OceanicTriangleId : OceanicRegion)
				{
					TerraneTriangles.Add(OceanicTriangleId);
					++InnerSeaTriangleCount;
				}
			}
		}
		if (Diagnostics != nullptr)
		{
			Diagnostics->D1InnerSeaScanSeconds += FPlatformTime::Seconds() - InnerSeaStartSeconds;
			Diagnostics->D1ScannedOceanicTriangleCount += ScannedOceanicTriangleCount;
			Diagnostics->D1InnerSeaTriangleCount += InnerSeaTriangleCount;
		}

		OutComponent.LocalTriangleIds = TerraneTriangles.Array();
		OutComponent.LocalTriangleIds.Sort();
		OutComponent.ContinentalTriangleSet = MoveTemp(ContinentalComponent);
		OutComponent.ContinentalTriangleCount = OutComponent.ContinentalTriangleSet.Num();
		OutComponent.InnerSeaTriangleCount = InnerSeaTriangleCount;
		return OutComponent.LocalTriangleIds.Num() > 0;
	}

	double ComputePhaseIIIDComponentAreaWeight(
		const CarrierLab::FCarrierPlate& Plate,
		const FPhaseIIID1TerraneComponent& Component)
	{
		double AreaWeight = 0.0;
		for (const int32 LocalTriangleId : Component.LocalTriangleIds)
		{
			AreaWeight += ComputePlateLocalTriangleAreaWeight(Plate, LocalTriangleId);
		}
		return AreaWeight;
	}

	uint64 MakeLocalEdgeKey(const int32 A, const int32 B)
	{
		const uint32 MinVertexId = static_cast<uint32>(FMath::Min(A, B));
		const uint32 MaxVertexId = static_cast<uint32>(FMath::Max(A, B));
		return (static_cast<uint64>(MinVertexId) << 32) | static_cast<uint64>(MaxVertexId);
	}

	void AddPlateTriangleEdgeCounts(
		const CarrierLab::FCarrierPlateTriangle& Triangle,
		TMap<uint64, int32>& EdgeCounts)
	{
		++EdgeCounts.FindOrAdd(MakeLocalEdgeKey(Triangle.A, Triangle.B));
		++EdgeCounts.FindOrAdd(MakeLocalEdgeKey(Triangle.B, Triangle.C));
		++EdgeCounts.FindOrAdd(MakeLocalEdgeKey(Triangle.C, Triangle.A));
	}

	FString ComputePhaseIIID4TriangleSetHash(
		const int32 PlateId,
		const TArray<int32>& LocalTriangleIds)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIID4-triangle-set-v1"));
		HashMix(Hash, static_cast<uint64>(PlateId + 1));
		HashMix(Hash, static_cast<uint64>(LocalTriangleIds.Num() + 1));
		for (const int32 LocalTriangleId : LocalTriangleIds)
		{
			HashMix(Hash, static_cast<uint64>(LocalTriangleId + 1));
		}
		return HashToString(Hash);
	}

	FString ComputePhaseIIID4IndexMapHash(
		const FString& Domain,
		const int32 PlateId,
		const TArray<int32>& OldToNew)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, Domain);
		HashMix(Hash, static_cast<uint64>(PlateId + 1));
		HashMix(Hash, static_cast<uint64>(OldToNew.Num() + 1));
		for (const int32 NewIndex : OldToNew)
		{
			HashMix(Hash, static_cast<uint64>(NewIndex + 2));
		}
		return HashToString(Hash);
	}

	FString ComputePhaseIIID5TriangleSetHash(
		const int32 SourcePlateId,
		const int32 DestinationPlateId,
		const TArray<int32>& LocalTriangleIds)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIID5-triangle-set-v1"));
		HashMix(Hash, static_cast<uint64>(SourcePlateId + 1));
		HashMix(Hash, static_cast<uint64>(DestinationPlateId + 1));
		HashMix(Hash, static_cast<uint64>(LocalTriangleIds.Num() + 1));
		for (const int32 LocalTriangleId : LocalTriangleIds)
		{
			HashMix(Hash, static_cast<uint64>(LocalTriangleId + 1));
		}
		return HashToString(Hash);
	}

	FString ComputePhaseIIID5IndexMapHash(
		const FString& Domain,
		const int32 SourcePlateId,
		const int32 DestinationPlateId,
		const TArray<int32>& OldToNew)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, Domain);
		HashMix(Hash, static_cast<uint64>(SourcePlateId + 1));
		HashMix(Hash, static_cast<uint64>(DestinationPlateId + 1));
		HashMix(Hash, static_cast<uint64>(OldToNew.Num() + 1));
		for (const int32 NewIndex : OldToNew)
		{
			HashMix(Hash, static_cast<uint64>(NewIndex + 2));
		}
		return HashToString(Hash);
	}

	double ComputePlateLocalContinentalAreaWeight(const CarrierLab::FCarrierPlate& Plate)
	{
		double AreaWeight = 0.0;
		for (int32 LocalTriangleId = 0; LocalTriangleId < Plate.LocalTriangles.Num(); ++LocalTriangleId)
		{
			AreaWeight += ComputePlateLocalTriangleAreaWeight(Plate, LocalTriangleId) *
				ComputePlateLocalTriangleContinentalFraction(Plate, LocalTriangleId);
		}
		return AreaWeight;
	}

	double ComputePlateLocalContinentalAreaWeightForTriangles(
		const CarrierLab::FCarrierPlate& Plate,
		const TArray<int32>& LocalTriangleIds)
	{
		double AreaWeight = 0.0;
		for (const int32 LocalTriangleId : LocalTriangleIds)
		{
			AreaWeight += ComputePlateLocalTriangleAreaWeight(Plate, LocalTriangleId) *
				ComputePlateLocalTriangleContinentalFraction(Plate, LocalTriangleId);
		}
		return AreaWeight;
	}

	void RebuildPlateBoundaryTrackingFromLocalTopology(
		CarrierLab::FCarrierPlate& Plate,
		int32& OutBoundaryTriangleCount,
		int32& OutNonManifoldEdgeCount)
	{
		OutBoundaryTriangleCount = 0;
		OutNonManifoldEdgeCount = 0;
		Plate.ActiveBoundaryTriangles.Reset();
		Plate.ActiveBoundaryTriangleDistancesKm.Reset();

		TMap<uint64, int32> EdgeCounts;
		for (const CarrierLab::FCarrierPlateTriangle& Triangle : Plate.LocalTriangles)
		{
			if (Plate.Vertices.IsValidIndex(Triangle.A) &&
				Plate.Vertices.IsValidIndex(Triangle.B) &&
				Plate.Vertices.IsValidIndex(Triangle.C) &&
				Triangle.A != Triangle.B &&
				Triangle.B != Triangle.C &&
				Triangle.C != Triangle.A)
			{
				AddPlateTriangleEdgeCounts(Triangle, EdgeCounts);
			}
		}

		for (const TPair<uint64, int32>& EdgePair : EdgeCounts)
		{
			if (EdgePair.Value > 2)
			{
				++OutNonManifoldEdgeCount;
			}
		}

		for (int32 LocalTriangleId = 0; LocalTriangleId < Plate.LocalTriangles.Num(); ++LocalTriangleId)
		{
			CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
			const bool bBoundary =
				EdgeCounts.FindRef(MakeLocalEdgeKey(Triangle.A, Triangle.B)) == 1 ||
				EdgeCounts.FindRef(MakeLocalEdgeKey(Triangle.B, Triangle.C)) == 1 ||
				EdgeCounts.FindRef(MakeLocalEdgeKey(Triangle.C, Triangle.A)) == 1;
			Triangle.bBoundary = bBoundary;
			if (bBoundary)
			{
				Plate.ActiveBoundaryTriangles.Add(LocalTriangleId);
				Plate.ActiveBoundaryTriangleDistancesKm.Add(0.0);
				++OutBoundaryTriangleCount;
			}
		}
	}

	void RebuildPlateLookupLists(CarrierLab::FCarrierPlate& Plate)
	{
		Plate.SampleIds.Reset();
		Plate.TriangleIds.Reset();
		Plate.GlobalSampleIdToLocalVertexId.Reset();
		for (int32 LocalVertexId = 0; LocalVertexId < Plate.Vertices.Num(); ++LocalVertexId)
		{
			const CarrierLab::FCarrierVertex& Vertex = Plate.Vertices[LocalVertexId];
			if (Vertex.GlobalSampleId != INDEX_NONE)
			{
				Plate.SampleIds.AddUnique(Vertex.GlobalSampleId);
				if (!Plate.GlobalSampleIdToLocalVertexId.Contains(Vertex.GlobalSampleId))
				{
					Plate.GlobalSampleIdToLocalVertexId.Add(Vertex.GlobalSampleId, LocalVertexId);
				}
			}
		}
		for (const CarrierLab::FCarrierPlateTriangle& Triangle : Plate.LocalTriangles)
		{
			if (Triangle.SourceTriangleId != INDEX_NONE)
			{
				Plate.TriangleIds.AddUnique(Triangle.SourceTriangleId);
			}
		}
		Plate.SampleIds.Sort();
		Plate.TriangleIds.Sort();
	}

	FString ComputePhaseIIID6PlateTopologyHash(
		const FString& Domain,
		const CarrierLab::FCarrierPlate& Plate,
		const int32 BoundaryTriangleCount,
		const int32 NonManifoldEdgeCount)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, Domain);
		HashMix(Hash, static_cast<uint64>(Plate.PlateId + 1));
		HashMix(Hash, static_cast<uint64>(Plate.Vertices.Num() + 1));
		HashMix(Hash, static_cast<uint64>(Plate.LocalTriangles.Num() + 1));
		HashMix(Hash, static_cast<uint64>(BoundaryTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(NonManifoldEdgeCount + 1));
		for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			HashMix(Hash, static_cast<uint64>(Vertex.GlobalSampleId + 1));
			HashMixDouble(Hash, Vertex.UnitPosition.X);
			HashMixDouble(Hash, Vertex.UnitPosition.Y);
			HashMixDouble(Hash, Vertex.UnitPosition.Z);
			HashMixDouble(Hash, Vertex.AreaWeight);
			HashMixDouble(Hash, Vertex.ContinentalFraction);
			HashMixDouble(Hash, Vertex.Elevation);
			HashMixDouble(Hash, Vertex.HistoricalElevation);
			HashMix(Hash, Vertex.bHasHistoricalElevationSnapshot ? 1ull : 0ull);
		}
		for (const CarrierLab::FCarrierPlateTriangle& Triangle : Plate.LocalTriangles)
		{
			HashMix(Hash, static_cast<uint64>(Triangle.SourceTriangleId + 1));
			HashMix(Hash, static_cast<uint64>(Triangle.A + 1));
			HashMix(Hash, static_cast<uint64>(Triangle.B + 1));
			HashMix(Hash, static_cast<uint64>(Triangle.C + 1));
			HashMix(Hash, Triangle.bBoundary ? 1ull : 0ull);
		}
		return HashToString(Hash);
	}

	bool IsTrackingRecordInvolvingPlate(const int32 PlateA, const int32 PlateB, const int32 SourcePlateId, const int32 DestinationPlateId)
	{
		return PlateA == SourcePlateId ||
			PlateA == DestinationPlateId ||
			PlateB == SourcePlateId ||
			PlateB == DestinationPlateId;
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

	struct FCarrierLabIIIE3SelectionCandidate
	{
		int32 PlateId = INDEX_NONE;
		int32 LocalTriangleId = INDEX_NONE;
		int32 SourceTriangleId = INDEX_NONE;
		int32 GlobalVertexIds[3] = { INDEX_NONE, INDEX_NONE, INDEX_NONE };
		FVector3d Bary = FVector3d(1.0, 0.0, 0.0);
		double Distance = 0.0;
		double ContinentalFraction = 0.0;
		double PlateContinentalFraction = 0.0;
		double PlateOceanicAge = 0.0;
		FCarrierLabCrustFields Fields;
		ECarrierLabPhaseIIIE3FilterReason FilterReason = ECarrierLabPhaseIIIE3FilterReason::None;
		bool bBoundary = false;
		bool bHasSourceTopologySnapshot = false;
		bool bHasPlateAggregateOverride = false;
	};

	int32 CountDistinctIIIE3Plates(const TArray<FCarrierLabIIIE3SelectionCandidate>& Candidates)
	{
		TSet<int32> PlateIds;
		for (const FCarrierLabIIIE3SelectionCandidate& Candidate : Candidates)
		{
			if (Candidate.PlateId != INDEX_NONE)
			{
				PlateIds.Add(Candidate.PlateId);
			}
		}
		return PlateIds.Num();
	}

	bool HasMixedIIIE3Material(const TArray<FCarrierLabIIIE3SelectionCandidate>& Candidates)
	{
		bool bHasContinental = false;
		bool bHasOceanic = false;
		for (const FCarrierLabIIIE3SelectionCandidate& Candidate : Candidates)
		{
			if (Candidate.ContinentalFraction >= 0.5)
			{
				bHasContinental = true;
			}
			else
			{
				bHasOceanic = true;
			}
		}
		return bHasContinental && bHasOceanic;
	}

	void MeasureIIIE3MultiHitResiduals(
		const TArray<FCarrierLabIIIE3SelectionCandidate>& Candidates,
		double& OutMaxRayDistanceResidualKm,
		double& OutMaxScalarResidual,
		double& OutMaxElevationResidualKm,
		double& OutMaxUnitVectorResidual)
	{
		OutMaxRayDistanceResidualKm = 0.0;
		OutMaxScalarResidual = 0.0;
		OutMaxElevationResidualKm = 0.0;
		OutMaxUnitVectorResidual = 0.0;
		if (Candidates.Num() <= 1)
		{
			return;
		}

		const FCarrierLabIIIE3SelectionCandidate& Reference = Candidates[0];
		for (int32 Index = 1; Index < Candidates.Num(); ++Index)
		{
			const FCarrierLabIIIE3SelectionCandidate& Candidate = Candidates[Index];
			OutMaxRayDistanceResidualKm = FMath::Max(
				OutMaxRayDistanceResidualKm,
				FMath::Abs(Candidate.Distance - Reference.Distance) * EarthRadiusKm);
			OutMaxScalarResidual = FMath::Max(
				OutMaxScalarResidual,
				FMath::Abs(Candidate.ContinentalFraction - Reference.ContinentalFraction));
			OutMaxScalarResidual = FMath::Max(
				OutMaxScalarResidual,
				FMath::Abs(Candidate.Fields.OceanicAge - Reference.Fields.OceanicAge));
			OutMaxElevationResidualKm = FMath::Max(
				OutMaxElevationResidualKm,
				FMath::Abs(Candidate.Fields.Elevation - Reference.Fields.Elevation));
			OutMaxElevationResidualKm = FMath::Max(
				OutMaxElevationResidualKm,
				FMath::Abs(Candidate.Fields.HistoricalElevation - Reference.Fields.HistoricalElevation));
			OutMaxUnitVectorResidual = FMath::Max(
				OutMaxUnitVectorResidual,
				(Candidate.Fields.RidgeDirection - Reference.Fields.RidgeDirection).Size());
			OutMaxUnitVectorResidual = FMath::Max(
				OutMaxUnitVectorResidual,
				(Candidate.Fields.FoldDirection - Reference.Fields.FoldDirection).Size());
		}
	}

	bool AreAllIIIE3BoundaryCandidates(const TArray<FCarrierLabIIIE3SelectionCandidate>& Candidates)
	{
		for (const FCarrierLabIIIE3SelectionCandidate& Candidate : Candidates)
		{
			if (!Candidate.bBoundary)
			{
				return false;
			}
		}
		return !Candidates.IsEmpty();
	}

	ECarrierLabPhaseIIIE3MultiHitBucket ClassifyIIIE3MultiHitBucket(
		const TArray<FCarrierLabIIIE3SelectionCandidate>& Candidates,
		double& OutMaxRayDistanceResidualKm,
		double& OutMaxScalarResidual,
		double& OutMaxElevationResidualKm,
		double& OutMaxUnitVectorResidual)
	{
		MeasureIIIE3MultiHitResiduals(
			Candidates,
			OutMaxRayDistanceResidualKm,
			OutMaxScalarResidual,
			OutMaxElevationResidualKm,
			OutMaxUnitVectorResidual);

		if (Candidates.Num() <= 1)
		{
			return ECarrierLabPhaseIIIE3MultiHitBucket::None;
		}

		const int32 PlateCount = CountDistinctIIIE3Plates(Candidates);
		if (PlateCount >= 3)
		{
			return ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate;
		}
		if (HasMixedIIIE3Material(Candidates))
		{
			return ECarrierLabPhaseIIIE3MultiHitBucket::MixedMaterial;
		}
		if (PlateCount == 1)
		{
			return AreAllIIIE3BoundaryCandidates(Candidates) &&
				OutMaxRayDistanceResidualKm <= PhaseIIIE3RayDistanceCoincidenceToleranceKm
				? ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateCoincident
				: ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateDistanceSeparated;
		}
		return OutMaxRayDistanceResidualKm <= PhaseIIIE3RayDistanceCoincidenceToleranceKm
			? ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual
			: ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent;
	}

	bool IsIIIE3CoalescingFieldMismatch(
		const ECarrierLabPhaseIIIE3MultiHitBucket Bucket,
		const double MaxScalarResidual,
		const double MaxElevationResidualKm,
		const double MaxUnitVectorResidual)
	{
		return Bucket == ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateCoincident &&
			(MaxScalarResidual > PhaseIIIE3ScalarFieldTolerance ||
				MaxElevationResidualKm > PhaseIIIE3ElevationToleranceKm ||
				MaxUnitVectorResidual > PhaseIIIE3UnitVectorTolerance);
	}

	bool TryCoalesceIIIE3WithinPlateCoincidentCandidates(
		const TArray<FCarrierLabIIIE3SelectionCandidate>& VisibleCandidates,
		FCarrierLabIIIE3SelectionCandidate& OutCandidate)
	{
		double MaxRayResidualKm = 0.0;
		double MaxScalarResidual = 0.0;
		double MaxElevationResidualKm = 0.0;
		double MaxUnitVectorResidual = 0.0;
		const ECarrierLabPhaseIIIE3MultiHitBucket Bucket = ClassifyIIIE3MultiHitBucket(
			VisibleCandidates,
			MaxRayResidualKm,
			MaxScalarResidual,
			MaxElevationResidualKm,
			MaxUnitVectorResidual);
		if (Bucket != ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateCoincident ||
			IsIIIE3CoalescingFieldMismatch(Bucket, MaxScalarResidual, MaxElevationResidualKm, MaxUnitVectorResidual))
		{
			return false;
		}

		int32 BestIndex = INDEX_NONE;
		for (int32 Index = 0; Index < VisibleCandidates.Num(); ++Index)
		{
			if (BestIndex == INDEX_NONE ||
				VisibleCandidates[Index].LocalTriangleId < VisibleCandidates[BestIndex].LocalTriangleId ||
				(VisibleCandidates[Index].LocalTriangleId == VisibleCandidates[BestIndex].LocalTriangleId &&
					VisibleCandidates[Index].Distance < VisibleCandidates[BestIndex].Distance))
			{
				BestIndex = Index;
			}
		}
		if (!VisibleCandidates.IsValidIndex(BestIndex))
		{
			return false;
		}

		OutCandidate = VisibleCandidates[BestIndex];
		return true;
	}

	ECarrierLabPhaseIIIE62BarycentricShape ClassifyPhaseIIIE62BarycentricShape(const FVector3d& Bary)
	{
		const int32 NearZeroCount =
			(Bary.X <= PhaseIIIE3ScalarFieldTolerance ? 1 : 0) +
			(Bary.Y <= PhaseIIIE3ScalarFieldTolerance ? 1 : 0) +
			(Bary.Z <= PhaseIIIE3ScalarFieldTolerance ? 1 : 0);
		const int32 NearOneCount =
			(Bary.X >= 1.0 - PhaseIIIE3ScalarFieldTolerance ? 1 : 0) +
			(Bary.Y >= 1.0 - PhaseIIIE3ScalarFieldTolerance ? 1 : 0) +
			(Bary.Z >= 1.0 - PhaseIIIE3ScalarFieldTolerance ? 1 : 0);
		if (NearOneCount == 1 && NearZeroCount >= 2)
		{
			return ECarrierLabPhaseIIIE62BarycentricShape::Vertex;
		}
		if (NearZeroCount >= 1)
		{
			return ECarrierLabPhaseIIIE62BarycentricShape::Edge;
		}
		if (Bary.X >= -PhaseIIIE3ScalarFieldTolerance &&
			Bary.Y >= -PhaseIIIE3ScalarFieldTolerance &&
			Bary.Z >= -PhaseIIIE3ScalarFieldTolerance)
		{
			return ECarrierLabPhaseIIIE62BarycentricShape::Interior;
		}
		return ECarrierLabPhaseIIIE62BarycentricShape::Unknown;
	}

	bool BuildPhaseIIIE62CandidateSnapshot(
		const CarrierLab::FCarrierState* State,
		const FCarrierLabIIIE3SelectionCandidate& Candidate,
		const FCarrierLabIIIE3SelectionCandidate& Reference,
		const int32 CandidateIndex,
		FCarrierLabPhaseIIIE62CandidateSnapshot& OutSnapshot)
	{
		OutSnapshot = FCarrierLabPhaseIIIE62CandidateSnapshot();
		OutSnapshot.CandidateIndex = CandidateIndex;
		OutSnapshot.PlateId = Candidate.PlateId;
		OutSnapshot.LocalTriangleId = Candidate.LocalTriangleId;
		OutSnapshot.Bary = Candidate.Bary;
		OutSnapshot.BarycentricShape = ClassifyPhaseIIIE62BarycentricShape(Candidate.Bary);
		OutSnapshot.bBoundary = Candidate.bBoundary;
		OutSnapshot.Distance = Candidate.Distance;
		OutSnapshot.RayDistanceResidualKm = FMath::Abs(Candidate.Distance - Reference.Distance) * EarthRadiusKm;
		OutSnapshot.ScalarResidual = FMath::Max(
			FMath::Abs(Candidate.ContinentalFraction - Reference.ContinentalFraction),
			FMath::Abs(Candidate.Fields.OceanicAge - Reference.Fields.OceanicAge));
		OutSnapshot.ElevationResidualKm = FMath::Max(
			FMath::Abs(Candidate.Fields.Elevation - Reference.Fields.Elevation),
			FMath::Abs(Candidate.Fields.HistoricalElevation - Reference.Fields.HistoricalElevation));
		OutSnapshot.UnitVectorResidual = FMath::Max(
			(Candidate.Fields.RidgeDirection - Reference.Fields.RidgeDirection).Size(),
			(Candidate.Fields.FoldDirection - Reference.Fields.FoldDirection).Size());
		OutSnapshot.FilterReason = Candidate.FilterReason;

		if (Candidate.bHasSourceTopologySnapshot)
		{
			OutSnapshot.SourceTriangleId = Candidate.SourceTriangleId;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				OutSnapshot.GlobalVertexIds[Corner] = Candidate.GlobalVertexIds[Corner];
			}
			return OutSnapshot.SourceTriangleId != INDEX_NONE &&
				OutSnapshot.BarycentricShape != ECarrierLabPhaseIIIE62BarycentricShape::Unknown &&
				OutSnapshot.GlobalVertexIds[0] != INDEX_NONE &&
				OutSnapshot.GlobalVertexIds[1] != INDEX_NONE &&
				OutSnapshot.GlobalVertexIds[2] != INDEX_NONE;
		}

		if (State == nullptr || !State->Plates.IsValidIndex(Candidate.PlateId))
		{
			return false;
		}
		const CarrierLab::FCarrierPlate& Plate = State->Plates[Candidate.PlateId];
		if (!Plate.LocalTriangles.IsValidIndex(Candidate.LocalTriangleId))
		{
			return false;
		}
		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[Candidate.LocalTriangleId];
		OutSnapshot.SourceTriangleId = Triangle.SourceTriangleId;
		const int32 LocalVertexIds[3] = { Triangle.A, Triangle.B, Triangle.C };
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			if (!Plate.Vertices.IsValidIndex(LocalVertexIds[Corner]))
			{
				return false;
			}
			OutSnapshot.GlobalVertexIds[Corner] = Plate.Vertices[LocalVertexIds[Corner]].GlobalSampleId;
		}
		return true;
	}

	bool BuildPhaseIIIE62CandidateSnapshot(
		const CarrierLab::FCarrierState& State,
		const FCarrierLabIIIE3SelectionCandidate& Candidate,
		const FCarrierLabIIIE3SelectionCandidate& Reference,
		const int32 CandidateIndex,
		FCarrierLabPhaseIIIE62CandidateSnapshot& OutSnapshot)
	{
		return BuildPhaseIIIE62CandidateSnapshot(&State, Candidate, Reference, CandidateIndex, OutSnapshot);
	}

	int32 CountDistinctIIIE62SnapshotPlates(const TArray<FCarrierLabPhaseIIIE62CandidateSnapshot>& Snapshots)
	{
		TSet<int32> PlateIds;
		for (const FCarrierLabPhaseIIIE62CandidateSnapshot& Snapshot : Snapshots)
		{
			if (Snapshot.PlateId != INDEX_NONE)
			{
				PlateIds.Add(Snapshot.PlateId);
			}
		}
		return PlateIds.Num();
	}

	int32 CountDistinctIIIE62SourceTriangles(const TArray<FCarrierLabPhaseIIIE62CandidateSnapshot>& Snapshots)
	{
		TSet<int32> SourceTriangleIds;
		for (const FCarrierLabPhaseIIIE62CandidateSnapshot& Snapshot : Snapshots)
		{
			if (Snapshot.SourceTriangleId != INDEX_NONE)
			{
				SourceTriangleIds.Add(Snapshot.SourceTriangleId);
			}
		}
		return SourceTriangleIds.Num();
	}

	TSet<int32> MakeIIIE62GlobalVertexSet(const FCarrierLabPhaseIIIE62CandidateSnapshot& Snapshot)
	{
		TSet<int32> Vertices;
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			if (Snapshot.GlobalVertexIds[Corner] != INDEX_NONE)
			{
				Vertices.Add(Snapshot.GlobalVertexIds[Corner]);
			}
		}
		return Vertices;
	}

	int32 CountCommonIIIE62GlobalVertices(const TArray<FCarrierLabPhaseIIIE62CandidateSnapshot>& Snapshots)
	{
		if (Snapshots.IsEmpty())
		{
			return 0;
		}
		TSet<int32> Common = MakeIIIE62GlobalVertexSet(Snapshots[0]);
		for (int32 Index = 1; Index < Snapshots.Num(); ++Index)
		{
			Common = Common.Intersect(MakeIIIE62GlobalVertexSet(Snapshots[Index]));
		}
		return Common.Num();
	}

	int32 MaxPairSharedIIIE62GlobalVertices(const TArray<FCarrierLabPhaseIIIE62CandidateSnapshot>& Snapshots)
	{
		int32 MaxShared = 0;
		for (int32 A = 0; A < Snapshots.Num(); ++A)
		{
			const TSet<int32> ASet = MakeIIIE62GlobalVertexSet(Snapshots[A]);
			for (int32 B = A + 1; B < Snapshots.Num(); ++B)
			{
				const TSet<int32> Shared = ASet.Intersect(MakeIIIE62GlobalVertexSet(Snapshots[B]));
				MaxShared = FMath::Max(MaxShared, Shared.Num());
			}
		}
		return MaxShared;
	}

	bool HasInvalidIIIE62Snapshot(const TArray<FCarrierLabPhaseIIIE62CandidateSnapshot>& Snapshots)
	{
		for (const FCarrierLabPhaseIIIE62CandidateSnapshot& Snapshot : Snapshots)
		{
			if (Snapshot.PlateId == INDEX_NONE ||
				Snapshot.LocalTriangleId == INDEX_NONE ||
				Snapshot.SourceTriangleId == INDEX_NONE ||
				Snapshot.BarycentricShape == ECarrierLabPhaseIIIE62BarycentricShape::Unknown)
			{
				return true;
			}
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				if (Snapshot.GlobalVertexIds[Corner] == INDEX_NONE)
				{
					return true;
				}
			}
		}
		return false;
	}

	void DiagnosePhaseIIIE62HoldFromSnapshots(
		const int32 SampleId,
		const ECarrierLabPhaseIIIE3SelectionClass SelectionClass,
		const ECarrierLabPhaseIIIE3MultiHitBucket MultiHitBucket,
		const TArray<FCarrierLabPhaseIIIE62CandidateSnapshot>& CandidateSnapshots,
		FCarrierLabPhaseIIIE62HoldRecord& OutRecord)
	{
		OutRecord = FCarrierLabPhaseIIIE62HoldRecord();
		OutRecord.SampleId = SampleId;
		OutRecord.SelectionClass = SelectionClass;
		OutRecord.MultiHitBucket = MultiHitBucket;
		OutRecord.Candidates = CandidateSnapshots;
		OutRecord.CandidateCount = CandidateSnapshots.Num();
		OutRecord.DistinctPlateCount = CountDistinctIIIE62SnapshotPlates(CandidateSnapshots);
		OutRecord.DistinctSourceTriangleCount = CountDistinctIIIE62SourceTriangles(CandidateSnapshots);
		OutRecord.SharedGlobalVertexCount = CountCommonIIIE62GlobalVertices(CandidateSnapshots);
		OutRecord.bAllBoundary = !CandidateSnapshots.IsEmpty();

		for (const FCarrierLabPhaseIIIE62CandidateSnapshot& Snapshot : CandidateSnapshots)
		{
			OutRecord.bAllBoundary = OutRecord.bAllBoundary && Snapshot.bBoundary;
			OutRecord.bHasInteriorCandidate =
				OutRecord.bHasInteriorCandidate ||
				Snapshot.BarycentricShape == ECarrierLabPhaseIIIE62BarycentricShape::Interior ||
				Snapshot.BarycentricShape == ECarrierLabPhaseIIIE62BarycentricShape::Unknown;
			OutRecord.MaxRayDistanceResidualKm = FMath::Max(OutRecord.MaxRayDistanceResidualKm, Snapshot.RayDistanceResidualKm);
			OutRecord.MaxScalarResidual = FMath::Max(OutRecord.MaxScalarResidual, Snapshot.ScalarResidual);
			OutRecord.MaxElevationResidualKm = FMath::Max(OutRecord.MaxElevationResidualKm, Snapshot.ElevationResidualKm);
			OutRecord.MaxUnitVectorResidual = FMath::Max(OutRecord.MaxUnitVectorResidual, Snapshot.UnitVectorResidual);
		}
		OutRecord.bFieldMismatch =
			OutRecord.MaxScalarResidual > PhaseIIIE3ScalarFieldTolerance ||
			OutRecord.MaxElevationResidualKm > PhaseIIIE3ElevationToleranceKm ||
			OutRecord.MaxUnitVectorResidual > PhaseIIIE3UnitVectorTolerance;

		if (CandidateSnapshots.Num() <= 1 || HasInvalidIIIE62Snapshot(CandidateSnapshots))
		{
			OutRecord.HoldShape = ECarrierLabPhaseIIIE62HoldShape::InvalidOrUnclassified;
			return;
		}
		if (!OutRecord.bAllBoundary || OutRecord.bHasInteriorCandidate)
		{
			OutRecord.HoldShape = ECarrierLabPhaseIIIE62HoldShape::NonBoundaryOrInteriorOverlap;
			return;
		}

		if (OutRecord.DistinctPlateCount == 2)
		{
			if (OutRecord.bFieldMismatch)
			{
				OutRecord.HoldShape = ECarrierLabPhaseIIIE62HoldShape::TwoPlateFieldMismatch;
				return;
			}
			if (OutRecord.DistinctSourceTriangleCount == 1)
			{
				OutRecord.HoldShape = ECarrierLabPhaseIIIE62HoldShape::TwoPlateSameSourceTriangle;
				return;
			}
			const int32 MaxPairShared = MaxPairSharedIIIE62GlobalVertices(CandidateSnapshots);
			if (MaxPairShared >= 2)
			{
				OutRecord.HoldShape = ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalEdge;
			}
			else if (MaxPairShared == 1)
			{
				OutRecord.HoldShape = ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalVertexOnly;
			}
			else
			{
				OutRecord.HoldShape = ECarrierLabPhaseIIIE62HoldShape::TwoPlateNoSharedGlobalVertices;
			}
			return;
		}

		if (OutRecord.DistinctPlateCount >= 3)
		{
			if (OutRecord.SharedGlobalVertexCount > 0)
			{
				OutRecord.HoldShape = ECarrierLabPhaseIIIE62HoldShape::ThreePlateCommonGlobalVertex;
			}
			else if (MaxPairSharedIIIE62GlobalVertices(CandidateSnapshots) >= 2)
			{
				OutRecord.HoldShape = ECarrierLabPhaseIIIE62HoldShape::ThreePlateEdgePlusIntruder;
			}
			else
			{
				OutRecord.HoldShape = ECarrierLabPhaseIIIE62HoldShape::ThreePlateNoCommonSourceVertex;
			}
			return;
		}

		OutRecord.HoldShape = ECarrierLabPhaseIIIE62HoldShape::InvalidOrUnclassified;
	}

	void AccumulatePhaseIIIE62HoldRecord(
		const FCarrierLabPhaseIIIE62HoldRecord& Record,
		FCarrierLabPhaseIIIE62CrossPlateMultiHitAudit& Audit)
	{
		++Audit.DiagnosedHoldCount;
		Audit.CandidateSnapshotCount += Record.Candidates.Num();
		switch (Record.HoldShape)
		{
		case ECarrierLabPhaseIIIE62HoldShape::TwoPlateSameSourceTriangle:
			++Audit.TwoPlateSameSourceTriangleCount;
			break;
		case ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalEdge:
			++Audit.TwoPlateSharedGlobalEdgeCount;
			break;
		case ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalVertexOnly:
			++Audit.TwoPlateSharedGlobalVertexOnlyCount;
			break;
		case ECarrierLabPhaseIIIE62HoldShape::TwoPlateNoSharedGlobalVertices:
			++Audit.TwoPlateNoSharedGlobalVerticesCount;
			break;
		case ECarrierLabPhaseIIIE62HoldShape::TwoPlateFieldMismatch:
			++Audit.TwoPlateFieldMismatchCount;
			break;
		case ECarrierLabPhaseIIIE62HoldShape::ThreePlateCommonGlobalVertex:
			++Audit.ThreePlateCommonGlobalVertexCount;
			break;
		case ECarrierLabPhaseIIIE62HoldShape::ThreePlateEdgePlusIntruder:
			++Audit.ThreePlateEdgePlusIntruderCount;
			break;
		case ECarrierLabPhaseIIIE62HoldShape::ThreePlateNoCommonSourceVertex:
			++Audit.ThreePlateNoCommonSourceVertexCount;
			break;
		case ECarrierLabPhaseIIIE62HoldShape::NonBoundaryOrInteriorOverlap:
			++Audit.NonBoundaryOrInteriorOverlapCount;
			break;
		case ECarrierLabPhaseIIIE62HoldShape::InvalidOrUnclassified:
		default:
			++Audit.InvalidOrUnclassifiedCount;
			break;
		}
		Audit.Records.Add(Record);
	}

	FString ComputePhaseIIIE62DiagnosisHash(const TArray<FCarrierLabPhaseIIIE62HoldRecord>& Records)
	{
		uint64 Hash = 1469598103934665603ull;
		TArray<FCarrierLabPhaseIIIE62HoldRecord> SortedRecords = Records;
		SortedRecords.Sort([](
			const FCarrierLabPhaseIIIE62HoldRecord& A,
			const FCarrierLabPhaseIIIE62HoldRecord& B)
		{
			return A.SampleId < B.SampleId;
		});

		for (const FCarrierLabPhaseIIIE62HoldRecord& Record : SortedRecords)
		{
			HashMix(Hash, static_cast<uint64>(Record.SampleId + 1));
			HashMix(Hash, static_cast<uint64>(Record.SelectionClass) + 1ull);
			HashMix(Hash, static_cast<uint64>(Record.MultiHitBucket) + 1ull);
			HashMix(Hash, static_cast<uint64>(Record.HoldShape) + 1ull);
			HashMix(Hash, static_cast<uint64>(Record.CandidateCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.DistinctPlateCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.DistinctSourceTriangleCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.SharedGlobalVertexCount + 1));
			HashMix(Hash, Record.bAllBoundary ? 2ull : 1ull);
			HashMix(Hash, Record.bHasInteriorCandidate ? 2ull : 1ull);
			HashMix(Hash, Record.bFieldMismatch ? 2ull : 1ull);
			HashMixDouble(Hash, Record.MaxRayDistanceResidualKm);
			HashMixDouble(Hash, Record.MaxScalarResidual);
			HashMixDouble(Hash, Record.MaxElevationResidualKm);
			HashMixDouble(Hash, Record.MaxUnitVectorResidual);

			TArray<FCarrierLabPhaseIIIE62CandidateSnapshot> SortedSnapshots = Record.Candidates;
			SortedSnapshots.Sort([](
				const FCarrierLabPhaseIIIE62CandidateSnapshot& A,
				const FCarrierLabPhaseIIIE62CandidateSnapshot& B)
			{
				if (A.PlateId != B.PlateId)
				{
					return A.PlateId < B.PlateId;
				}
				if (A.LocalTriangleId != B.LocalTriangleId)
				{
					return A.LocalTriangleId < B.LocalTriangleId;
				}
				return A.CandidateIndex < B.CandidateIndex;
			});
			for (const FCarrierLabPhaseIIIE62CandidateSnapshot& Snapshot : SortedSnapshots)
			{
				HashMix(Hash, static_cast<uint64>(Snapshot.CandidateIndex + 1));
				HashMix(Hash, static_cast<uint64>(Snapshot.PlateId + 1));
				HashMix(Hash, static_cast<uint64>(Snapshot.LocalTriangleId + 1));
				HashMix(Hash, static_cast<uint64>(Snapshot.SourceTriangleId + 1));
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					HashMix(Hash, static_cast<uint64>(Snapshot.GlobalVertexIds[Corner] + 1));
				}
				HashMixDouble(Hash, Snapshot.Bary.X);
				HashMixDouble(Hash, Snapshot.Bary.Y);
				HashMixDouble(Hash, Snapshot.Bary.Z);
				HashMix(Hash, static_cast<uint64>(Snapshot.BarycentricShape) + 1ull);
				HashMix(Hash, Snapshot.bBoundary ? 2ull : 1ull);
				HashMixDouble(Hash, Snapshot.Distance);
				HashMixDouble(Hash, Snapshot.RayDistanceResidualKm);
				HashMixDouble(Hash, Snapshot.ScalarResidual);
				HashMixDouble(Hash, Snapshot.ElevationResidualKm);
				HashMixDouble(Hash, Snapshot.UnitVectorResidual);
				HashMix(Hash, static_cast<uint64>(Snapshot.FilterReason) + 1ull);
			}
		}
		return HashToString(Hash);
	}

	void AccumulatePhaseIIIE64HoldRecord(
		const FCarrierLabPhaseIIIE64HoldRecord& Record,
		FCarrierLabPhaseIIIE64PostMotionMultiHitAudit& Audit)
	{
		++Audit.DiagnosedHoldCount;
		if (Record.MultiHitBucket == ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent)
		{
			++Audit.CrossPlateDifferentHoldCount;
			if (Record.bHasUniqueNearest)
			{
				++Audit.UniqueNearestCrossPlateDifferentCount;
			}
			if (Record.bNearestDistanceTie)
			{
				++Audit.DistanceTieCrossPlateDifferentCount;
			}
		}
		else if (Record.MultiHitBucket == ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate)
		{
			++Audit.ThirdPlateHoldCount;
			if (Record.bHasUniqueNearest)
			{
				++Audit.UniqueNearestThirdPlateCount;
			}
			if (Record.bNearestDistanceTie)
			{
				++Audit.DistanceTieThirdPlateCount;
			}
		}

		Audit.ProcessMarkedHoldCount += Record.bAnyCandidateProcessMarked ? 1 : 0;
		Audit.SubductingMarkedHoldCount += Record.bAnySubductingMarked ? 1 : 0;
		Audit.ObductionPendingMarkedHoldCount += Record.bAnyObductionPendingMarked ? 1 : 0;
		Audit.CollisionPendingMarkedHoldCount += Record.bAnyCollisionPendingMarked ? 1 : 0;
		Audit.NearestMostContinentalCount += Record.bNearestIsMostContinentalPlate ? 1 : 0;
		Audit.NearestOlderOceanicCount += Record.bNearestIsOlderOceanicPlate ? 1 : 0;
		Audit.NearestLowerPlateIdCount += Record.bNearestIsLowerPlateId ? 1 : 0;
		Audit.MaxNearestDistanceGapKm = FMath::Max(Audit.MaxNearestDistanceGapKm, Record.NearestDistanceGapKm);
		Audit.Records.Add(Record);
	}

	void FinalizePhaseIIIE64DistanceStats(FCarrierLabPhaseIIIE64PostMotionMultiHitAudit& Audit)
	{
		TArray<double> GapsKm;
		GapsKm.Reserve(Audit.Records.Num());
		for (const FCarrierLabPhaseIIIE64HoldRecord& Record : Audit.Records)
		{
			if (Record.bHasUniqueNearest || Record.bNearestDistanceTie)
			{
				GapsKm.Add(FMath::Max(0.0, Record.NearestDistanceGapKm));
			}
		}
		GapsKm.Sort();
		if (GapsKm.IsEmpty())
		{
			Audit.MedianNearestDistanceGapKm = 0.0;
			Audit.P95NearestDistanceGapKm = 0.0;
			return;
		}
		Audit.MedianNearestDistanceGapKm = GapsKm[GapsKm.Num() / 2];
		const int32 P95Index = FMath::Clamp(
			FMath::CeilToInt(static_cast<double>(GapsKm.Num()) * 0.95) - 1,
			0,
			GapsKm.Num() - 1);
		Audit.P95NearestDistanceGapKm = GapsKm[P95Index];
	}

	FString ComputePhaseIIIE64DiagnosisHash(const TArray<FCarrierLabPhaseIIIE64HoldRecord>& Records)
	{
		uint64 Hash = 1469598103934665603ull;
		TArray<FCarrierLabPhaseIIIE64HoldRecord> SortedRecords = Records;
		SortedRecords.Sort([](
			const FCarrierLabPhaseIIIE64HoldRecord& A,
			const FCarrierLabPhaseIIIE64HoldRecord& B)
		{
			return A.SampleId < B.SampleId;
		});
		for (const FCarrierLabPhaseIIIE64HoldRecord& Record : SortedRecords)
		{
			HashMix(Hash, static_cast<uint64>(Record.SampleId + 1));
			HashMix(Hash, static_cast<uint64>(Record.Step + 1));
			HashMix(Hash, static_cast<uint64>(Record.SelectionClass) + 1ull);
			HashMix(Hash, static_cast<uint64>(Record.MultiHitBucket) + 1ull);
			HashMix(Hash, static_cast<uint64>(Record.CandidateCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.DistinctPlateCount + 1));
			HashMix(Hash, Record.bHasUniqueNearest ? 2ull : 1ull);
			HashMix(Hash, Record.bNearestDistanceTie ? 2ull : 1ull);
			HashMixDouble(Hash, Record.NearestDistanceGapKm);
			HashMix(Hash, Record.bAnySubductingMarked ? 2ull : 1ull);
			HashMix(Hash, Record.bAnyObductionPendingMarked ? 2ull : 1ull);
			HashMix(Hash, Record.bAnyCollisionPendingMarked ? 2ull : 1ull);
			HashMix(Hash, Record.bNearestIsMostContinentalPlate ? 2ull : 1ull);
			HashMix(Hash, Record.bNearestIsOlderOceanicPlate ? 2ull : 1ull);
			HashMix(Hash, Record.bNearestIsLowerPlateId ? 2ull : 1ull);
			for (const FCarrierLabPhaseIIIE64CandidateDiagnostic& Candidate : Record.Candidates)
			{
				HashMix(Hash, static_cast<uint64>(Candidate.Snapshot.CandidateIndex + 1));
				HashMix(Hash, static_cast<uint64>(Candidate.Snapshot.PlateId + 1));
				HashMix(Hash, static_cast<uint64>(Candidate.Snapshot.LocalTriangleId + 1));
				HashMix(Hash, static_cast<uint64>(Candidate.Snapshot.SourceTriangleId + 1));
				HashMixDouble(Hash, Candidate.Snapshot.Distance);
				HashMixDouble(Hash, Candidate.Snapshot.RayDistanceResidualKm);
				HashMix(Hash, static_cast<uint64>(Candidate.Snapshot.BarycentricShape) + 1ull);
				HashMix(Hash, Candidate.bSubductingMarked ? 2ull : 1ull);
				HashMix(Hash, Candidate.bObductionPendingMarked ? 2ull : 1ull);
				HashMix(Hash, Candidate.bCollisionPendingMarked ? 2ull : 1ull);
				HashMix(Hash, Candidate.bNearestCandidate ? 2ull : 1ull);
				HashMixDouble(Hash, Candidate.PlateContinentalFraction);
				HashMixDouble(Hash, Candidate.PlateOceanicAge);
			}
		}
		return HashToString(Hash);
	}

	ECarrierLabPhaseIIIE3SelectionClass ClassifyIIIE3UnresolvedMultiHit(
		const TArray<FCarrierLabIIIE3SelectionCandidate>& RawCandidates,
		const TArray<FCarrierLabIIIE3SelectionCandidate>& VisibleCandidates)
	{
		if (CountDistinctIIIE3Plates(RawCandidates) >= 3 || CountDistinctIIIE3Plates(VisibleCandidates) >= 3)
		{
			return ECarrierLabPhaseIIIE3SelectionClass::UnresolvedThirdPlateMultiHit;
		}

		bool bHasContinental = false;
		bool bHasOceanic = false;
		for (const FCarrierLabIIIE3SelectionCandidate& Candidate : VisibleCandidates)
		{
			if (Candidate.ContinentalFraction >= 0.5)
			{
				bHasContinental = true;
			}
			else
			{
				bHasOceanic = true;
			}
		}

		return bHasContinental && bHasOceanic
			? ECarrierLabPhaseIIIE3SelectionClass::UnresolvedMixedMaterialMultiHit
			: ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit;
	}

	struct FCarrierLabIIIE63PlateTieBreakStats
	{
		int32 PlateId = INDEX_NONE;
		double ContinentalFraction = 0.0;
		double OceanicAge = 0.0;
	};

	bool IsIIIE63ResolvableSharedBoundaryShape(const ECarrierLabPhaseIIIE62HoldShape Shape)
	{
		return Shape == ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalEdge ||
			Shape == ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalVertexOnly ||
			Shape == ECarrierLabPhaseIIIE62HoldShape::ThreePlateCommonGlobalVertex;
	}

	bool GetIIIE63PlateTieBreakStats(
		const CarrierLab::FCarrierState* State,
		const FCarrierLabIIIE3SelectionCandidate& Candidate,
		FCarrierLabIIIE63PlateTieBreakStats& OutStats)
	{
		if (Candidate.PlateId == INDEX_NONE)
		{
			return false;
		}

		OutStats.PlateId = Candidate.PlateId;
		if (Candidate.bHasPlateAggregateOverride)
		{
			OutStats.ContinentalFraction = FMath::Clamp(Candidate.PlateContinentalFraction, 0.0, 1.0);
			OutStats.OceanicAge = FMath::Max(Candidate.PlateOceanicAge, 0.0);
			return true;
		}

		if (State == nullptr || !State->Plates.IsValidIndex(Candidate.PlateId))
		{
			return false;
		}

		OutStats.ContinentalFraction = ComputePlateContinentalFraction(*State, Candidate.PlateId);
		OutStats.OceanicAge = ComputePlateOceanicAge(*State, Candidate.PlateId);
		return true;
	}

	bool BuildIIIE63PlateTieBreakStats(
		const CarrierLab::FCarrierState* State,
		const TArray<FCarrierLabIIIE3SelectionCandidate>& VisibleCandidates,
		TArray<FCarrierLabIIIE63PlateTieBreakStats>& OutStats)
	{
		OutStats.Reset();
		for (const FCarrierLabIIIE3SelectionCandidate& Candidate : VisibleCandidates)
		{
			if (Candidate.PlateId == INDEX_NONE)
			{
				return false;
			}
			if (OutStats.ContainsByPredicate([&Candidate](const FCarrierLabIIIE63PlateTieBreakStats& Existing)
			{
				return Existing.PlateId == Candidate.PlateId;
			}))
			{
				continue;
			}

			FCarrierLabIIIE63PlateTieBreakStats Stats;
			if (!GetIIIE63PlateTieBreakStats(State, Candidate, Stats))
			{
				return false;
			}
			OutStats.Add(Stats);
		}

		OutStats.Sort([](
			const FCarrierLabIIIE63PlateTieBreakStats& A,
			const FCarrierLabIIIE63PlateTieBreakStats& B)
		{
			return A.PlateId < B.PlateId;
		});
		return OutStats.Num() >= 2;
	}

	bool SelectIIIE63WinningPlate(
		const TArray<FCarrierLabIIIE63PlateTieBreakStats>& Stats,
		int32& OutPlateId,
		ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule& OutRule,
		double& OutContinentalMargin,
		double& OutOceanicAgeMargin)
	{
		OutPlateId = INDEX_NONE;
		OutRule = ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::None;
		OutContinentalMargin = 0.0;
		OutOceanicAgeMargin = 0.0;
		if (Stats.Num() < 2)
		{
			return false;
		}

		double BestContinental = -TNumericLimits<double>::Max();
		double SecondContinental = -TNumericLimits<double>::Max();
		for (const FCarrierLabIIIE63PlateTieBreakStats& Stat : Stats)
		{
			if (Stat.ContinentalFraction > BestContinental)
			{
				SecondContinental = BestContinental;
				BestContinental = Stat.ContinentalFraction;
			}
			else if (Stat.ContinentalFraction > SecondContinental)
			{
				SecondContinental = Stat.ContinentalFraction;
			}
		}
		OutContinentalMargin = BestContinental - SecondContinental;

		TArray<FCarrierLabIIIE63PlateTieBreakStats> ContinentalTied;
		for (const FCarrierLabIIIE63PlateTieBreakStats& Stat : Stats)
		{
			if (BestContinental - Stat.ContinentalFraction <= PhaseIIIE3ScalarFieldTolerance)
			{
				ContinentalTied.Add(Stat);
			}
		}
		if (ContinentalTied.Num() == 1 && OutContinentalMargin > PhaseIIIE3ScalarFieldTolerance)
		{
			OutPlateId = ContinentalTied[0].PlateId;
			OutRule = ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::ContinentalPriority;
			return true;
		}

		double BestAge = -TNumericLimits<double>::Max();
		double SecondAge = -TNumericLimits<double>::Max();
		for (const FCarrierLabIIIE63PlateTieBreakStats& Stat : ContinentalTied)
		{
			if (Stat.OceanicAge > BestAge)
			{
				SecondAge = BestAge;
				BestAge = Stat.OceanicAge;
			}
			else if (Stat.OceanicAge > SecondAge)
			{
				SecondAge = Stat.OceanicAge;
			}
		}
		OutOceanicAgeMargin = BestAge - SecondAge;

		TArray<FCarrierLabIIIE63PlateTieBreakStats> AgeTied;
		for (const FCarrierLabIIIE63PlateTieBreakStats& Stat : ContinentalTied)
		{
			if (BestAge - Stat.OceanicAge <= PhaseIIIE63OceanicAgeTieToleranceMa)
			{
				AgeTied.Add(Stat);
			}
		}
		if (AgeTied.Num() == 1 && OutOceanicAgeMargin > PhaseIIIE63OceanicAgeTieToleranceMa)
		{
			OutPlateId = AgeTied[0].PlateId;
			OutRule = ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::OlderOceanicAge;
			return true;
		}

		AgeTied.Sort([](
			const FCarrierLabIIIE63PlateTieBreakStats& A,
			const FCarrierLabIIIE63PlateTieBreakStats& B)
		{
			return A.PlateId < B.PlateId;
		});
		OutPlateId = AgeTied[0].PlateId;
		OutRule = ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::LowerPlateId;
		return true;
	}

	ECarrierLabPhaseIIIE66DistanceTieFallbackLayer ConvertIIIE63RuleToIIIE66Layer(
		const ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule Rule)
	{
		switch (Rule)
		{
		case ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::ContinentalPriority:
			return ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::ContinentalPriority;
		case ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::OlderOceanicAge:
			return ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::OlderOceanicAge;
		case ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::LowerPlateId:
			return ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::LowerPlateId;
		case ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::None:
		default:
			return ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::Unresolved;
		}
	}

	bool FindIIIE63CandidateForWinningPlate(
		const TArray<FCarrierLabIIIE3SelectionCandidate>& VisibleCandidates,
		const int32 WinningPlateId,
		FCarrierLabIIIE3SelectionCandidate& OutWinner)
	{
		int32 BestCandidateIndex = INDEX_NONE;
		for (int32 Index = 0; Index < VisibleCandidates.Num(); ++Index)
		{
			if (VisibleCandidates[Index].PlateId != WinningPlateId)
			{
				continue;
			}
			if (BestCandidateIndex == INDEX_NONE ||
				VisibleCandidates[Index].LocalTriangleId < VisibleCandidates[BestCandidateIndex].LocalTriangleId ||
				(VisibleCandidates[Index].LocalTriangleId == VisibleCandidates[BestCandidateIndex].LocalTriangleId &&
					VisibleCandidates[Index].Distance < VisibleCandidates[BestCandidateIndex].Distance))
			{
				BestCandidateIndex = Index;
			}
		}
		if (!VisibleCandidates.IsValidIndex(BestCandidateIndex))
		{
			return false;
		}

		OutWinner = VisibleCandidates[BestCandidateIndex];
		return true;
	}

	bool TryResolveIIIE63SharedBoundaryTieBreak(
		const CarrierLab::FCarrierState* State,
		const TArray<FCarrierLabIIIE3SelectionCandidate>& VisibleCandidates,
		FCarrierLabPhaseIIIE3SelectionRecord& OutRecord,
		FCarrierLabIIIE3SelectionCandidate& OutWinner)
	{
		TArray<FCarrierLabPhaseIIIE62CandidateSnapshot> Snapshots;
		Snapshots.Reserve(VisibleCandidates.Num());
		if (VisibleCandidates.IsEmpty())
		{
			return false;
		}

		const FCarrierLabIIIE3SelectionCandidate& Reference = VisibleCandidates[0];
		for (int32 CandidateIndex = 0; CandidateIndex < VisibleCandidates.Num(); ++CandidateIndex)
		{
			FCarrierLabPhaseIIIE62CandidateSnapshot Snapshot;
			if (!BuildPhaseIIIE62CandidateSnapshot(
				State,
				VisibleCandidates[CandidateIndex],
				Reference,
				CandidateIndex,
				Snapshot))
			{
				return false;
			}
			Snapshots.Add(Snapshot);
		}

		FCarrierLabPhaseIIIE62HoldRecord HoldRecord;
		DiagnosePhaseIIIE62HoldFromSnapshots(
			OutRecord.SampleId,
			OutRecord.SelectionClass,
			OutRecord.MultiHitBucket,
			Snapshots,
			HoldRecord);
		OutRecord.SharedBoundaryShapeClass = HoldRecord.HoldShape;
		if (!IsIIIE63ResolvableSharedBoundaryShape(HoldRecord.HoldShape))
		{
			return false;
		}

		TArray<FCarrierLabIIIE63PlateTieBreakStats> PlateStats;
		if (!BuildIIIE63PlateTieBreakStats(State, VisibleCandidates, PlateStats))
		{
			return false;
		}

		int32 WinningPlateId = INDEX_NONE;
		ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule Rule = ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::None;
		double ContinentalMargin = 0.0;
		double OceanicAgeMargin = 0.0;
		if (!SelectIIIE63WinningPlate(PlateStats, WinningPlateId, Rule, ContinentalMargin, OceanicAgeMargin))
		{
			return false;
		}

		if (!FindIIIE63CandidateForWinningPlate(VisibleCandidates, WinningPlateId, OutWinner))
		{
			return false;
		}

		OutRecord.bUsedSharedBoundaryTieBreak = true;
		OutRecord.SharedBoundaryTieBreakRule = Rule;
		OutRecord.SharedBoundaryTieBreakCandidateCount = VisibleCandidates.Num();
		OutRecord.SharedBoundaryTieBreakPlateCount = PlateStats.Num();
		OutRecord.SharedBoundaryTieBreakContinentalMargin = ContinentalMargin;
		OutRecord.SharedBoundaryTieBreakOceanicAgeMargin = OceanicAgeMargin;
		OutRecord.SharedBoundaryCandidatePlateIds.Reset();
		OutRecord.SharedBoundaryCandidateContinentalFractions.Reset();
		OutRecord.SharedBoundaryCandidateOceanicAges.Reset();
		for (const FCarrierLabIIIE63PlateTieBreakStats& Stat : PlateStats)
		{
			OutRecord.SharedBoundaryCandidatePlateIds.Add(Stat.PlateId);
			OutRecord.SharedBoundaryCandidateContinentalFractions.Add(Stat.ContinentalFraction);
			OutRecord.SharedBoundaryCandidateOceanicAges.Add(Stat.OceanicAge);
		}
		return true;
	}

	bool TryResolveIIIE65NearestHitTieBreak(
		const TArray<FCarrierLabIIIE3SelectionCandidate>& VisibleCandidates,
		FCarrierLabPhaseIIIE3SelectionRecord& OutRecord,
		FCarrierLabIIIE3SelectionCandidate& OutWinner)
	{
		OutRecord.NearestHitToleranceKm = PhaseIIIE3RayDistanceCoincidenceToleranceKm;
		OutRecord.NearestHitCandidateCount = VisibleCandidates.Num();

		TSet<int32> DistinctPlateIds;
		int32 ProcessMarkedRefCount = 0;
		for (const FCarrierLabIIIE3SelectionCandidate& Candidate : VisibleCandidates)
		{
			DistinctPlateIds.Add(Candidate.PlateId);
			if (Candidate.FilterReason != ECarrierLabPhaseIIIE3FilterReason::None)
			{
				++ProcessMarkedRefCount;
			}
		}
		OutRecord.NearestHitDistinctPlateCount = DistinctPlateIds.Num();
		OutRecord.NearestHitProcessMarkedRefCount = ProcessMarkedRefCount;

		const bool bSupportedBucket =
			OutRecord.MultiHitBucket == ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent ||
			OutRecord.MultiHitBucket == ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate;
		if (!bSupportedBucket)
		{
			OutRecord.NearestHitResultClass = ECarrierLabPhaseIIIE65NearestHitResult::UnsupportedHeld;
			return false;
		}
		if (VisibleCandidates.Num() < 2)
		{
			OutRecord.NearestHitResultClass = ECarrierLabPhaseIIIE65NearestHitResult::UnsupportedHeld;
			return false;
		}

		int32 NearestIndex = 0;
		for (int32 CandidateIndex = 1; CandidateIndex < VisibleCandidates.Num(); ++CandidateIndex)
		{
			if (VisibleCandidates[CandidateIndex].Distance < VisibleCandidates[NearestIndex].Distance)
			{
				NearestIndex = CandidateIndex;
			}
		}
		const double NearestDistance = VisibleCandidates[NearestIndex].Distance;
		double SecondDistance = TNumericLimits<double>::Max();
		for (int32 CandidateIndex = 0; CandidateIndex < VisibleCandidates.Num(); ++CandidateIndex)
		{
			if (CandidateIndex == NearestIndex)
			{
				continue;
			}
			SecondDistance = FMath::Min(SecondDistance, VisibleCandidates[CandidateIndex].Distance);
		}
		const double Gap = FMath::Max(0.0, SecondDistance - NearestDistance);
		OutRecord.NearestHitGapKm = Gap;

		if (Gap <= PhaseIIIE3RayDistanceCoincidenceToleranceKm)
		{
			OutRecord.NearestHitResultClass = ECarrierLabPhaseIIIE65NearestHitResult::DistanceTieHeld;
			return false;
		}

		OutRecord.bUsedNearestHitTieBreak = true;
		OutRecord.NearestHitResultClass =
			OutRecord.MultiHitBucket == ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate
				? ECarrierLabPhaseIIIE65NearestHitResult::UniqueNearestThirdPlate
				: ECarrierLabPhaseIIIE65NearestHitResult::UniqueNearestCrossPlateDifferent;
		OutWinner = VisibleCandidates[NearestIndex];
		return true;
	}

	bool TryResolveIIIE66DistanceTieFallback(
		const CarrierLab::FCarrierState* State,
		const TArray<FCarrierLabIIIE3SelectionCandidate>& VisibleCandidates,
		FCarrierLabPhaseIIIE3SelectionRecord& OutRecord,
		FCarrierLabIIIE3SelectionCandidate& OutWinner)
	{
		// IIIE.6.6 is deliberately gated on IIIE.6.5's micron-scale
		// distance-tie classification. Disabling IIIE.6.5 therefore preserves
		// the historical hold instead of silently handing resolution to this
		// last-resort lab policy.
		if (OutRecord.NearestHitResultClass != ECarrierLabPhaseIIIE65NearestHitResult::DistanceTieHeld)
		{
			return false;
		}

		const bool bSupportedBucket =
			OutRecord.MultiHitBucket == ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent ||
			OutRecord.MultiHitBucket == ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate;
		if (!bSupportedBucket)
		{
			OutRecord.DistanceTieFallbackLayer = ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::Unresolved;
			return false;
		}

		TArray<FCarrierLabIIIE63PlateTieBreakStats> PlateStats;
		if (!BuildIIIE63PlateTieBreakStats(State, VisibleCandidates, PlateStats))
		{
			OutRecord.DistanceTieFallbackLayer = ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::Unresolved;
			return false;
		}

		int32 WinningPlateId = INDEX_NONE;
		ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule Rule = ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::None;
		double ContinentalMargin = 0.0;
		double OceanicAgeMargin = 0.0;
		if (!SelectIIIE63WinningPlate(PlateStats, WinningPlateId, Rule, ContinentalMargin, OceanicAgeMargin))
		{
			OutRecord.DistanceTieFallbackLayer = ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::Unresolved;
			return false;
		}
		if (!FindIIIE63CandidateForWinningPlate(VisibleCandidates, WinningPlateId, OutWinner))
		{
			OutRecord.DistanceTieFallbackLayer = ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::Unresolved;
			return false;
		}

		const ECarrierLabPhaseIIIE66DistanceTieFallbackLayer Layer = ConvertIIIE63RuleToIIIE66Layer(Rule);
		if (Layer == ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::Unresolved)
		{
			OutRecord.DistanceTieFallbackLayer = Layer;
			return false;
		}

		OutRecord.bUsedDistanceTieFallback = true;
		OutRecord.DistanceTieFallbackLayer = Layer;
		OutRecord.DistanceTieFallbackCandidateCount = VisibleCandidates.Num();
		OutRecord.DistanceTieFallbackPlateCount = PlateStats.Num();
		OutRecord.DistanceTieFallbackContinentalMargin = ContinentalMargin;
		OutRecord.DistanceTieFallbackOceanicAgeMargin = OceanicAgeMargin;
		return true;
	}

	bool SelectPhaseIIIE3FilteredRemeshSource(
		const int32 SampleId,
		const FVector3d& SampleUnitPosition,
		const TArray<FCarrierLabIIIE3SelectionCandidate>& Candidates,
		const bool bEnableDuplicateHitCoalescing,
		const bool bEnableSharedBoundaryTieBreak,
		const bool bEnableNearestHitTieBreak,
		const bool bEnableDistanceTieFallback,
		const CarrierLab::FCarrierState* SharedBoundaryTieBreakState,
		FCarrierLabPhaseIIIE3SelectionRecord& OutRecord)
	{
		OutRecord = FCarrierLabPhaseIIIE3SelectionRecord();
		OutRecord.SampleId = SampleId;
		OutRecord.SampleUnitPosition = NormalizeOrFallback(SampleUnitPosition, FVector3d::UnitZ());
		OutRecord.RawCandidateCount = Candidates.Num();
		OutRecord.RawPlateCount = CountDistinctIIIE3Plates(Candidates);

		TArray<FCarrierLabIIIE3SelectionCandidate> VisibleCandidates;
		VisibleCandidates.Reserve(Candidates.Num());
		for (const FCarrierLabIIIE3SelectionCandidate& Candidate : Candidates)
		{
			switch (Candidate.FilterReason)
			{
			case ECarrierLabPhaseIIIE3FilterReason::Subducting:
				++OutRecord.FilteredCandidateCount;
				++OutRecord.FilteredSubductingCount;
				break;
			case ECarrierLabPhaseIIIE3FilterReason::ObductionPending:
				++OutRecord.FilteredCandidateCount;
				++OutRecord.FilteredObductionPendingCount;
				break;
			case ECarrierLabPhaseIIIE3FilterReason::CollisionPending:
				++OutRecord.FilteredCandidateCount;
				++OutRecord.FilteredCollisionPendingCount;
				break;
			case ECarrierLabPhaseIIIE3FilterReason::None:
			default:
				VisibleCandidates.Add(Candidate);
				break;
			}
		}

		OutRecord.PostFilterCandidateCount = VisibleCandidates.Num();
		OutRecord.PostFilterPlateCount = CountDistinctIIIE3Plates(VisibleCandidates);
		OutRecord.EffectiveCandidateCount = VisibleCandidates.Num();

		if (Candidates.IsEmpty())
		{
			OutRecord.SelectionClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
			OutRecord.bDivergentGapRoute = true;
			return true;
		}

		if (VisibleCandidates.IsEmpty())
		{
			OutRecord.SelectionClass = ECarrierLabPhaseIIIE3SelectionClass::DivergentGapAfterFiltering;
			OutRecord.bDivergentGapRoute = true;
			return true;
		}

		if (VisibleCandidates.Num() == 1)
		{
			const FCarrierLabIIIE3SelectionCandidate& Winner = VisibleCandidates[0];
			OutRecord.SelectionClass = ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit;
			OutRecord.bResolvedSingleHit = true;
			OutRecord.ResolvedPlateId = Winner.PlateId;
			OutRecord.ResolvedLocalTriangleId = Winner.LocalTriangleId;
			OutRecord.ResolvedBary = Winner.Bary;
			OutRecord.ContinentalFraction = Winner.ContinentalFraction;
			OutRecord.Elevation = Winner.Fields.Elevation;
			OutRecord.HistoricalElevation = Winner.Fields.HistoricalElevation;
			OutRecord.OceanicAge = Winner.Fields.OceanicAge;
			OutRecord.RidgeDirection = Winner.Fields.RidgeDirection;
			OutRecord.FoldDirection = Winner.Fields.FoldDirection;
			return true;
		}

		OutRecord.MultiHitBucket = ClassifyIIIE3MultiHitBucket(
			VisibleCandidates,
			OutRecord.MultiHitMaxRayDistanceResidualKm,
			OutRecord.MultiHitMaxScalarResidual,
			OutRecord.MultiHitMaxElevationResidualKm,
			OutRecord.MultiHitMaxUnitVectorResidual);
		OutRecord.bCoalescingRejectedByFieldMismatch = IsIIIE3CoalescingFieldMismatch(
			OutRecord.MultiHitBucket,
			OutRecord.MultiHitMaxScalarResidual,
			OutRecord.MultiHitMaxElevationResidualKm,
			OutRecord.MultiHitMaxUnitVectorResidual);
		if (bEnableDuplicateHitCoalescing)
		{
			FCarrierLabIIIE3SelectionCandidate CoalescedWinner;
			if (TryCoalesceIIIE3WithinPlateCoincidentCandidates(VisibleCandidates, CoalescedWinner))
			{
				OutRecord.EffectiveCandidateCount = 1;
				OutRecord.CoalescedCandidateCount = VisibleCandidates.Num() - 1;
				OutRecord.CoalescedDuplicateHitCount = 1;
				OutRecord.bCoalescedDuplicateHit = true;
				OutRecord.SelectionClass = ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit;
				OutRecord.bResolvedSingleHit = true;
				OutRecord.ResolvedPlateId = CoalescedWinner.PlateId;
				OutRecord.ResolvedLocalTriangleId = CoalescedWinner.LocalTriangleId;
				OutRecord.ResolvedBary = CoalescedWinner.Bary;
				OutRecord.ContinentalFraction = CoalescedWinner.ContinentalFraction;
				OutRecord.Elevation = CoalescedWinner.Fields.Elevation;
				OutRecord.HistoricalElevation = CoalescedWinner.Fields.HistoricalElevation;
				OutRecord.OceanicAge = CoalescedWinner.Fields.OceanicAge;
				OutRecord.RidgeDirection = CoalescedWinner.Fields.RidgeDirection;
				OutRecord.FoldDirection = CoalescedWinner.Fields.FoldDirection;
				return true;
			}
		}
		if (bEnableSharedBoundaryTieBreak)
		{
			FCarrierLabIIIE3SelectionCandidate SharedBoundaryWinner;
			if (TryResolveIIIE63SharedBoundaryTieBreak(
				SharedBoundaryTieBreakState,
				VisibleCandidates,
				OutRecord,
				SharedBoundaryWinner))
			{
				OutRecord.EffectiveCandidateCount = 1;
				OutRecord.SelectionClass = ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit;
				OutRecord.bResolvedSingleHit = true;
				OutRecord.ResolvedPlateId = SharedBoundaryWinner.PlateId;
				OutRecord.ResolvedLocalTriangleId = SharedBoundaryWinner.LocalTriangleId;
				OutRecord.ResolvedBary = SharedBoundaryWinner.Bary;
				OutRecord.ContinentalFraction = SharedBoundaryWinner.ContinentalFraction;
				OutRecord.Elevation = SharedBoundaryWinner.Fields.Elevation;
				OutRecord.HistoricalElevation = SharedBoundaryWinner.Fields.HistoricalElevation;
				OutRecord.OceanicAge = SharedBoundaryWinner.Fields.OceanicAge;
				OutRecord.RidgeDirection = SharedBoundaryWinner.Fields.RidgeDirection;
				OutRecord.FoldDirection = SharedBoundaryWinner.Fields.FoldDirection;
				return true;
			}
		}
		if (bEnableNearestHitTieBreak)
		{
			FCarrierLabIIIE3SelectionCandidate NearestHitWinner;
			if (TryResolveIIIE65NearestHitTieBreak(VisibleCandidates, OutRecord, NearestHitWinner))
			{
				OutRecord.EffectiveCandidateCount = 1;
				OutRecord.SelectionClass = ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit;
				OutRecord.bResolvedSingleHit = true;
				OutRecord.ResolvedPlateId = NearestHitWinner.PlateId;
				OutRecord.ResolvedLocalTriangleId = NearestHitWinner.LocalTriangleId;
				OutRecord.ResolvedBary = NearestHitWinner.Bary;
				OutRecord.ContinentalFraction = NearestHitWinner.ContinentalFraction;
				OutRecord.Elevation = NearestHitWinner.Fields.Elevation;
				OutRecord.HistoricalElevation = NearestHitWinner.Fields.HistoricalElevation;
				OutRecord.OceanicAge = NearestHitWinner.Fields.OceanicAge;
				OutRecord.RidgeDirection = NearestHitWinner.Fields.RidgeDirection;
				OutRecord.FoldDirection = NearestHitWinner.Fields.FoldDirection;
				return true;
			}
		}
		if (bEnableDistanceTieFallback)
		{
			FCarrierLabIIIE3SelectionCandidate DistanceTieFallbackWinner;
			if (TryResolveIIIE66DistanceTieFallback(
				SharedBoundaryTieBreakState,
				VisibleCandidates,
				OutRecord,
				DistanceTieFallbackWinner))
			{
				OutRecord.EffectiveCandidateCount = 1;
				OutRecord.SelectionClass = ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit;
				OutRecord.bResolvedSingleHit = true;
				OutRecord.ResolvedPlateId = DistanceTieFallbackWinner.PlateId;
				OutRecord.ResolvedLocalTriangleId = DistanceTieFallbackWinner.LocalTriangleId;
				OutRecord.ResolvedBary = DistanceTieFallbackWinner.Bary;
				OutRecord.ContinentalFraction = DistanceTieFallbackWinner.ContinentalFraction;
				OutRecord.Elevation = DistanceTieFallbackWinner.Fields.Elevation;
				OutRecord.HistoricalElevation = DistanceTieFallbackWinner.Fields.HistoricalElevation;
				OutRecord.OceanicAge = DistanceTieFallbackWinner.Fields.OceanicAge;
				OutRecord.RidgeDirection = DistanceTieFallbackWinner.Fields.RidgeDirection;
				OutRecord.FoldDirection = DistanceTieFallbackWinner.Fields.FoldDirection;
				return true;
			}
		}
		OutRecord.SelectionClass = ClassifyIIIE3UnresolvedMultiHit(Candidates, VisibleCandidates);
		if (OutRecord.SharedBoundaryShapeClass == ECarrierLabPhaseIIIE62HoldShape::InvalidOrUnclassified)
		{
			FCarrierLabIIIE3SelectionCandidate IgnoredWinner;
			TryResolveIIIE63SharedBoundaryTieBreak(
				SharedBoundaryTieBreakState,
				VisibleCandidates,
				OutRecord,
				IgnoredWinner);
			OutRecord.bUsedSharedBoundaryTieBreak = false;
			OutRecord.SharedBoundaryTieBreakRule = ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::None;
			OutRecord.SharedBoundaryTieBreakCandidateCount = 0;
			OutRecord.SharedBoundaryTieBreakPlateCount = 0;
			OutRecord.SharedBoundaryTieBreakContinentalMargin = 0.0;
			OutRecord.SharedBoundaryTieBreakOceanicAgeMargin = 0.0;
			OutRecord.SharedBoundaryCandidatePlateIds.Reset();
			OutRecord.SharedBoundaryCandidateContinentalFractions.Reset();
			OutRecord.SharedBoundaryCandidateOceanicAges.Reset();
		}
		OutRecord.bUnresolvedMultiHit = true;
		return true;
	}

	void AccumulatePhaseIIIE3Record(
		const FCarrierLabPhaseIIIE3SelectionRecord& Record,
		FCarrierLabPhaseIIIE3RemeshSelectionAudit& Audit)
	{
		++Audit.SampleCount;
		if (Record.RawCandidateCount > 0)
		{
			++Audit.RawHitSampleCount;
		}
		else
		{
			++Audit.RawMissSampleCount;
		}
		if (Record.RawCandidateCount > 1)
		{
			++Audit.RawMultiHitSampleCount;
		}
		if (Record.RawPlateCount >= 3)
		{
			++Audit.RawThirdPlateHitSampleCount;
		}

		Audit.FilteredCandidateCount += Record.FilteredCandidateCount;
		Audit.FilteredSubductingCount += Record.FilteredSubductingCount;
		Audit.FilteredObductionPendingCount += Record.FilteredObductionPendingCount;
		Audit.FilteredCollisionPendingCount += Record.FilteredCollisionPendingCount;
		Audit.PostFilterSingleHitCount += Record.bResolvedSingleHit ? 1 : 0;
		Audit.DivergentGapRouteCount += Record.bDivergentGapRoute ? 1 : 0;
		Audit.UnresolvedMultiHitCount += Record.bUnresolvedMultiHit ? 1 : 0;
		Audit.CoalescedMultiHitCount += Record.bCoalescedDuplicateHit ? 1 : 0;
		Audit.CoalescedCandidateCount += Record.CoalescedCandidateCount;
		Audit.CoalescingFieldMismatchHoldCount += Record.bCoalescingRejectedByFieldMismatch ? 1 : 0;
		Audit.SharedBoundaryTieBreakCount += Record.bUsedSharedBoundaryTieBreak ? 1 : 0;
		Audit.MaxMultiHitRayDistanceResidualKm = FMath::Max(Audit.MaxMultiHitRayDistanceResidualKm, Record.MultiHitMaxRayDistanceResidualKm);
		Audit.MaxMultiHitScalarResidual = FMath::Max(Audit.MaxMultiHitScalarResidual, Record.MultiHitMaxScalarResidual);
		Audit.MaxMultiHitElevationResidualKm = FMath::Max(Audit.MaxMultiHitElevationResidualKm, Record.MultiHitMaxElevationResidualKm);
		Audit.MaxMultiHitUnitVectorResidual = FMath::Max(Audit.MaxMultiHitUnitVectorResidual, Record.MultiHitMaxUnitVectorResidual);
		Audit.PriorOwnerFallbackCount += Record.bUsedPriorOwnerFallback ? 1 : 0;
		Audit.PolicyWinnerCount += Record.bUsedPolicyWinner ? 1 : 0;

		const int32 PositiveResolverCount =
			(Record.bUsedSharedBoundaryTieBreak ? 1 : 0) +
			(Record.bUsedNearestHitTieBreak ? 1 : 0) +
			(Record.bUsedDistanceTieFallback ? 1 : 0);
		if (PositiveResolverCount > 1)
		{
			UE_LOG(
				LogTemp,
				Fatal,
				TEXT("CarrierLab IIIE.3 selection record sample %d co-fired multiple positive remesh resolvers; selector-hook ordering bug."),
				Record.SampleId);
		}
		if (Record.bUsedDistanceTieFallback &&
			Record.NearestHitResultClass != ECarrierLabPhaseIIIE65NearestHitResult::DistanceTieHeld)
		{
			UE_LOG(
				LogTemp,
				Fatal,
				TEXT("CarrierLab IIIE.6.6 distance-tie fallback fired for sample %d without IIIE.6.5 DistanceTieHeld classification."),
				Record.SampleId);
		}

		if (Record.bUsedNearestHitTieBreak)
		{
			if (Record.NearestHitResultClass == ECarrierLabPhaseIIIE65NearestHitResult::UniqueNearestThirdPlate)
			{
				++Audit.NearestHitThirdPlateResolvedCount;
			}
			else
			{
				++Audit.NearestHitCrossPlateDifferentResolvedCount;
			}
		}
		else if (Record.NearestHitResultClass == ECarrierLabPhaseIIIE65NearestHitResult::DistanceTieHeld &&
			!Record.bUsedDistanceTieFallback)
		{
			++Audit.NearestHitDistanceTieHeldCount;
		}
		else if (Record.NearestHitResultClass == ECarrierLabPhaseIIIE65NearestHitResult::UnsupportedHeld)
		{
			++Audit.NearestHitUnsupportedHeldCount;
		}

		if (Record.bUsedSharedBoundaryTieBreak)
		{
			switch (Record.SharedBoundaryShapeClass)
			{
			case ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalEdge:
				++Audit.SharedBoundaryTwoPlateEdgeCount;
				break;
			case ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalVertexOnly:
				++Audit.SharedBoundaryTwoPlateVertexOnlyCount;
				break;
			case ECarrierLabPhaseIIIE62HoldShape::ThreePlateCommonGlobalVertex:
				++Audit.SharedBoundaryThreePlateCommonVertexCount;
				break;
			default:
				break;
			}

			switch (Record.SharedBoundaryTieBreakRule)
			{
			case ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::ContinentalPriority:
				++Audit.SharedBoundaryContinentalPriorityCount;
				break;
			case ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::OlderOceanicAge:
				++Audit.SharedBoundaryOlderOceanicAgeCount;
				break;
			case ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::LowerPlateId:
				++Audit.SharedBoundaryLowerPlateIdCount;
				break;
			case ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::None:
			default:
				break;
			}
		}

		if (Record.bUsedDistanceTieFallback)
		{
			++Audit.DistanceTieFallbackCount;
			const bool bCrossPlateDifferent = Record.MultiHitBucket == ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent;
			const bool bThirdPlate = Record.MultiHitBucket == ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate;
			if (bCrossPlateDifferent)
			{
				++Audit.DistanceTieFallbackCrossPlateDifferentCount;
			}
			else if (bThirdPlate)
			{
				++Audit.DistanceTieFallbackThirdPlateCount;
			}

			switch (Record.DistanceTieFallbackLayer)
			{
			case ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::ContinentalPriority:
				++Audit.DistanceTieFallbackLayer1WinsCount;
				if (bCrossPlateDifferent)
				{
					++Audit.DistanceTieFallbackCrossPlateDifferentLayer1Count;
				}
				else if (bThirdPlate)
				{
					++Audit.DistanceTieFallbackThirdPlateLayer1Count;
				}
				break;
			case ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::OlderOceanicAge:
				++Audit.DistanceTieFallbackLayer2WinsCount;
				if (bCrossPlateDifferent)
				{
					++Audit.DistanceTieFallbackCrossPlateDifferentLayer2Count;
				}
				else if (bThirdPlate)
				{
					++Audit.DistanceTieFallbackThirdPlateLayer2Count;
				}
				break;
			case ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::LowerPlateId:
				++Audit.DistanceTieFallbackLayer3WinsCount;
				if (bCrossPlateDifferent)
				{
					++Audit.DistanceTieFallbackCrossPlateDifferentLayer3Count;
				}
				else if (bThirdPlate)
				{
					++Audit.DistanceTieFallbackThirdPlateLayer3Count;
				}
				break;
			case ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::Unresolved:
			case ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::None:
			default:
				break;
			}
		}

		switch (Record.MultiHitBucket)
		{
		case ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateCoincident:
			++Audit.WithinPlateCoincidentMultiHitCount;
			break;
		case ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateDistanceSeparated:
			++Audit.WithinPlateDistanceSeparatedMultiHitCount;
			break;
		case ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual:
			++Audit.CrossPlateEqualMultiHitCount;
			break;
		case ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent:
			++Audit.CrossPlateDifferentMultiHitCount;
			break;
		case ECarrierLabPhaseIIIE3MultiHitBucket::MixedMaterial:
			++Audit.MixedMaterialMultiHitCount;
			break;
		case ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate:
			++Audit.ThirdPlateMultiHitCount;
			break;
		default:
			break;
		}

		switch (Record.SelectionClass)
		{
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit:
			++Audit.UnresolvedSameMaterialMultiHitCount;
			break;
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedMixedMaterialMultiHit:
			++Audit.UnresolvedMixedMaterialMultiHitCount;
			break;
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedThirdPlateMultiHit:
			++Audit.UnresolvedThirdPlateMultiHitCount;
			break;
		default:
			break;
		}
		Audit.Records.Add(Record);
	}

	FString ComputePhaseIIIE3SelectionHash(const TArray<FCarrierLabPhaseIIIE3SelectionRecord>& Records)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIIE3-filtered-selection-v4-distance-tie-fallback"));
		HashMix(Hash, static_cast<uint64>(Records.Num() + 1));
		for (const FCarrierLabPhaseIIIE3SelectionRecord& Record : Records)
		{
			HashMix(Hash, static_cast<uint64>(Record.SampleId + 2));
			HashMixDouble(Hash, Record.SampleUnitPosition.X);
			HashMixDouble(Hash, Record.SampleUnitPosition.Y);
			HashMixDouble(Hash, Record.SampleUnitPosition.Z);
			HashMix(Hash, static_cast<uint64>(Record.RawCandidateCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.RawPlateCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.FilteredCandidateCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.FilteredSubductingCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.FilteredObductionPendingCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.FilteredCollisionPendingCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.PostFilterCandidateCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.PostFilterPlateCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.EffectiveCandidateCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.CoalescedCandidateCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.CoalescedDuplicateHitCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.SelectionClass) + 1ull);
			HashMix(Hash, static_cast<uint64>(Record.MultiHitBucket) + 1ull);
			HashMix(Hash, static_cast<uint64>(Record.bResolvedSingleHit ? 2 : 1));
			HashMix(Hash, static_cast<uint64>(Record.bDivergentGapRoute ? 2 : 1));
			HashMix(Hash, static_cast<uint64>(Record.bUnresolvedMultiHit ? 2 : 1));
			HashMix(Hash, static_cast<uint64>(Record.bCoalescedDuplicateHit ? 2 : 1));
			HashMix(Hash, static_cast<uint64>(Record.bCoalescingRejectedByFieldMismatch ? 2 : 1));
			HashMix(Hash, static_cast<uint64>(Record.bUsedSharedBoundaryTieBreak ? 2 : 1));
			HashMix(Hash, static_cast<uint64>(Record.bUsedNearestHitTieBreak ? 2 : 1));
			HashMix(Hash, static_cast<uint64>(Record.bUsedDistanceTieFallback ? 2 : 1));
			HashMix(Hash, static_cast<uint64>(Record.bUsedPolicyWinner ? 2 : 1));
			HashMix(Hash, static_cast<uint64>(Record.bUsedPriorOwnerFallback ? 2 : 1));
			HashMix(Hash, static_cast<uint64>(Record.SharedBoundaryShapeClass) + 1ull);
			HashMix(Hash, static_cast<uint64>(Record.SharedBoundaryTieBreakRule) + 1ull);
			HashMix(Hash, static_cast<uint64>(Record.SharedBoundaryTieBreakCandidateCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.SharedBoundaryTieBreakPlateCount + 1));
			HashMixDouble(Hash, Record.SharedBoundaryTieBreakContinentalMargin);
			HashMixDouble(Hash, Record.SharedBoundaryTieBreakOceanicAgeMargin);
			HashMix(Hash, static_cast<uint64>(Record.SharedBoundaryCandidatePlateIds.Num() + 1));
			for (const int32 PlateId : Record.SharedBoundaryCandidatePlateIds)
			{
				HashMix(Hash, static_cast<uint64>(PlateId + 2));
			}
			HashMix(Hash, static_cast<uint64>(Record.SharedBoundaryCandidateContinentalFractions.Num() + 1));
			for (const double Fraction : Record.SharedBoundaryCandidateContinentalFractions)
			{
				HashMixDouble(Hash, Fraction);
			}
			HashMix(Hash, static_cast<uint64>(Record.SharedBoundaryCandidateOceanicAges.Num() + 1));
			for (const double Age : Record.SharedBoundaryCandidateOceanicAges)
			{
				HashMixDouble(Hash, Age);
			}
			HashMix(Hash, static_cast<uint64>(Record.NearestHitResultClass) + 1ull);
			HashMixDouble(Hash, Record.NearestHitGapKm);
			HashMixDouble(Hash, Record.NearestHitToleranceKm);
			HashMix(Hash, static_cast<uint64>(Record.NearestHitCandidateCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.NearestHitDistinctPlateCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.NearestHitProcessMarkedRefCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.DistanceTieFallbackLayer) + 1ull);
			HashMix(Hash, static_cast<uint64>(Record.DistanceTieFallbackCandidateCount + 1));
			HashMix(Hash, static_cast<uint64>(Record.DistanceTieFallbackPlateCount + 1));
			HashMixDouble(Hash, Record.DistanceTieFallbackContinentalMargin);
			HashMixDouble(Hash, Record.DistanceTieFallbackOceanicAgeMargin);
			HashMix(Hash, static_cast<uint64>(Record.ResolvedPlateId + 2));
			HashMix(Hash, static_cast<uint64>(Record.ResolvedLocalTriangleId + 2));
			HashMixDouble(Hash, Record.ResolvedBary.X);
			HashMixDouble(Hash, Record.ResolvedBary.Y);
			HashMixDouble(Hash, Record.ResolvedBary.Z);
			HashMixDouble(Hash, Record.MultiHitMaxRayDistanceResidualKm);
			HashMixDouble(Hash, Record.MultiHitMaxScalarResidual);
			HashMixDouble(Hash, Record.MultiHitMaxElevationResidualKm);
			HashMixDouble(Hash, Record.MultiHitMaxUnitVectorResidual);
			HashMixDouble(Hash, Record.ContinentalFraction);
			HashMixDouble(Hash, Record.Elevation);
			HashMixDouble(Hash, Record.HistoricalElevation);
			HashMixDouble(Hash, Record.OceanicAge);
			HashMixDouble(Hash, Record.RidgeDirection.X);
			HashMixDouble(Hash, Record.RidgeDirection.Y);
			HashMixDouble(Hash, Record.RidgeDirection.Z);
			HashMixDouble(Hash, Record.FoldDirection.X);
			HashMixDouble(Hash, Record.FoldDirection.Y);
			HashMixDouble(Hash, Record.FoldDirection.Z);
		}
		return HashToString(Hash);
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

	double UnitAngularDistanceRadians(const FVector3d& A, const FVector3d& B)
	{
		return FMath::Acos(FMath::Clamp(FVector3d::DotProduct(A, B), -1.0, 1.0));
	}

	bool IsOnMinorArc(const FVector3d& A, const FVector3d& B, const FVector3d& Candidate, const double EdgeAngle)
	{
		if (EdgeAngle <= 1.0e-12)
		{
			return false;
		}
		const double CandidateAngle =
			UnitAngularDistanceRadians(A, Candidate) +
			UnitAngularDistanceRadians(Candidate, B);
		return FMath::Abs(CandidateAngle - EdgeAngle) <= 1.0e-7;
	}

	bool ComputeClosestContinuousBoundaryCandidate(
		const FCarrierLabContinuousBoundaryEdge& Edge,
		const FVector3d& SamplePosition,
		FCarrierLabContinuousBoundaryCandidate& OutCandidate)
	{
		if (Edge.PlateId == INDEX_NONE)
		{
			return false;
		}

		const FVector3d A = NormalizeOrFallback(Edge.StartUnitPosition, FVector3d::UnitX());
		const FVector3d B = NormalizeOrFallback(Edge.EndUnitPosition, FVector3d::UnitY());
		const FVector3d P = NormalizeOrFallback(SamplePosition, FVector3d::UnitZ());
		const double EdgeAngle = UnitAngularDistanceRadians(A, B);
		if (EdgeAngle <= 1.0e-12)
		{
			OutCandidate.EdgeId = Edge.EdgeId;
			OutCandidate.PlateId = Edge.PlateId;
			OutCandidate.UnitPosition = A;
			OutCandidate.DistanceKm = EarthRadiusKm * UnitAngularDistanceRadians(P, A);
			OutCandidate.EdgeT = 0.0;
			OutCandidate.ContinentalFraction = FMath::Clamp(Edge.StartContinentalFraction, 0.0, 1.0);
			OutCandidate.Elevation = Edge.StartElevation;
			return true;
		}

		const FVector3d EdgeNormal = FVector3d::CrossProduct(A, B);
		const double EdgeNormalSize = EdgeNormal.Size();
		FVector3d Closest = A;
		double EdgeT = 0.0;
		if (EdgeNormalSize > UE_DOUBLE_SMALL_NUMBER)
		{
			const FVector3d UnitNormal = EdgeNormal / EdgeNormalSize;
			const FVector3d Projected = P - FVector3d::DotProduct(P, UnitNormal) * UnitNormal;
			FVector3d ProjectedUnit = NormalizeOrFallback(Projected, A);
			if (FVector3d::DotProduct(ProjectedUnit, P) < FVector3d::DotProduct(-ProjectedUnit, P))
			{
				ProjectedUnit = -ProjectedUnit;
			}

			if (IsOnMinorArc(A, B, ProjectedUnit, EdgeAngle))
			{
				Closest = ProjectedUnit;
				EdgeT = FMath::Clamp(UnitAngularDistanceRadians(A, Closest) / EdgeAngle, 0.0, 1.0);
			}
			else
			{
				const double DotA = FVector3d::DotProduct(P, A);
				const double DotB = FVector3d::DotProduct(P, B);
				if (DotB > DotA)
				{
					Closest = B;
					EdgeT = 1.0;
				}
			}
		}

		OutCandidate.EdgeId = Edge.EdgeId;
		OutCandidate.PlateId = Edge.PlateId;
		OutCandidate.UnitPosition = NormalizeOrFallback(Closest, A);
		OutCandidate.DistanceKm = EarthRadiusKm * UnitAngularDistanceRadians(P, OutCandidate.UnitPosition);
		OutCandidate.EdgeT = EdgeT;
		OutCandidate.ContinentalFraction = FMath::Clamp(
			FMath::Lerp(Edge.StartContinentalFraction, Edge.EndContinentalFraction, EdgeT),
			0.0,
			1.0);
		OutCandidate.Elevation = FMath::Lerp(Edge.StartElevation, Edge.EndElevation, EdgeT);
		return true;
	}

	bool IsBetterContinuousBoundaryCandidate(
		const FCarrierLabContinuousBoundaryCandidate& Candidate,
		const FCarrierLabContinuousBoundaryCandidate& Best)
	{
		if (Best.PlateId == INDEX_NONE)
		{
			return true;
		}
		if (Candidate.DistanceKm < Best.DistanceKm - 1.0e-9)
		{
			return true;
		}
		if (!FMath::IsNearlyEqual(Candidate.DistanceKm, Best.DistanceKm, 1.0e-9))
		{
			return false;
		}
		if (Candidate.PlateId != Best.PlateId)
		{
			return Candidate.PlateId < Best.PlateId;
		}
		return Candidate.EdgeId < Best.EdgeId;
	}

	bool QueryContinuousBoundaryPair(
		const TArray<FCarrierLabContinuousBoundaryEdge>& Edges,
		const FVector3d& SamplePosition,
		FCarrierLabPhaseIIIE2BoundaryQueryAudit& OutAudit)
	{
		OutAudit = FCarrierLabPhaseIIIE2BoundaryQueryAudit();
		OutAudit.SampleUnitPosition = NormalizeOrFallback(SamplePosition, FVector3d::UnitZ());
		OutAudit.BoundaryEdgeCount = Edges.Num();

		TSet<int32> DistinctPlates;
		TArray<FCarrierLabContinuousBoundaryCandidate> Candidates;
		Candidates.Reserve(Edges.Num());
		for (const FCarrierLabContinuousBoundaryEdge& Edge : Edges)
		{
			if (Edge.PlateId == INDEX_NONE)
			{
				continue;
			}
			DistinctPlates.Add(Edge.PlateId);
			FCarrierLabContinuousBoundaryCandidate Candidate;
			if (ComputeClosestContinuousBoundaryCandidate(Edge, OutAudit.SampleUnitPosition, Candidate))
			{
				Candidates.Add(Candidate);
			}
		}
		OutAudit.DistinctPlateCount = DistinctPlates.Num();

		FCarrierLabContinuousBoundaryCandidate Q1;
		for (const FCarrierLabContinuousBoundaryCandidate& Candidate : Candidates)
		{
			if (IsBetterContinuousBoundaryCandidate(Candidate, Q1))
			{
				Q1 = Candidate;
			}
		}
		if (Q1.PlateId == INDEX_NONE)
		{
			return false;
		}

		FCarrierLabContinuousBoundaryCandidate Q2;
		for (const FCarrierLabContinuousBoundaryCandidate& Candidate : Candidates)
		{
			if (Candidate.PlateId == Q1.PlateId)
			{
				continue;
			}
			if (IsBetterContinuousBoundaryCandidate(Candidate, Q2))
			{
				Q2 = Candidate;
			}
		}
		if (Q2.PlateId == INDEX_NONE)
		{
			return false;
		}

		const FVector3d QGammaInput = Q1.UnitPosition + Q2.UnitPosition;
		OutAudit.bFound = true;
		OutAudit.Q1PlateId = Q1.PlateId;
		OutAudit.Q2PlateId = Q2.PlateId;
		OutAudit.Q1EdgeId = Q1.EdgeId;
		OutAudit.Q2EdgeId = Q2.EdgeId;
		OutAudit.Q1DistanceKm = Q1.DistanceKm;
		OutAudit.Q2DistanceKm = Q2.DistanceKm;
		OutAudit.Q1EdgeT = Q1.EdgeT;
		OutAudit.Q2EdgeT = Q2.EdgeT;
		OutAudit.Q1ContinentalFraction = Q1.ContinentalFraction;
		OutAudit.Q2ContinentalFraction = Q2.ContinentalFraction;
		OutAudit.Q1Elevation = Q1.Elevation;
		OutAudit.Q2Elevation = Q2.Elevation;
		OutAudit.Q1UnitPosition = Q1.UnitPosition;
		OutAudit.Q2UnitPosition = Q2.UnitPosition;
		OutAudit.QGammaInputNorm = QGammaInput.Size();
		OutAudit.QGammaUnitPosition = NormalizeOrFallback(QGammaInput, OutAudit.SampleUnitPosition);
		OutAudit.QGammaUnitResidual = FMath::Abs(OutAudit.QGammaUnitPosition.Size() - 1.0);
		return true;
	}

	TArray<FCarrierLabContinuousBoundaryEdge> BuildContinuousBoundaryEdges(const CarrierLab::FCarrierState& State)
	{
		TArray<FCarrierLabContinuousBoundaryEdge> Edges;
		int32 EdgeId = 0;
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
						FCarrierLabContinuousBoundaryEdge Edge;
						Edge.EdgeId = EdgeId++;
						Edge.PlateId = Plate.PlateId;
						Edge.StartUnitPosition = A.UnitPosition;
						Edge.EndUnitPosition = B.UnitPosition;
						Edge.StartContinentalFraction = A.ContinentalFraction;
						Edge.EndContinentalFraction = B.ContinentalFraction;
						Edge.StartElevation = A.Elevation;
						Edge.EndElevation = B.Elevation;
						Edges.Add(Edge);
					}
				}
			}
		}
		return Edges;
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

	bool IsPhaseIIIE4DivergentGapRoute(const ECarrierLabPhaseIIIE3SelectionClass SelectionClass)
	{
		return SelectionClass == ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap ||
			SelectionClass == ECarrierLabPhaseIIIE3SelectionClass::DivergentGapAfterFiltering;
	}

	bool IsPhaseIIIE4UnresolvedMultiHitRoute(const ECarrierLabPhaseIIIE3SelectionClass SelectionClass)
	{
		return SelectionClass == ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit ||
			SelectionClass == ECarrierLabPhaseIIIE3SelectionClass::UnresolvedMixedMaterialMultiHit ||
			SelectionClass == ECarrierLabPhaseIIIE3SelectionClass::UnresolvedThirdPlateMultiHit;
	}

	void PopulatePhaseIIIE4OceanicRecord(
		const FVector3d& SamplePosition,
		const ECarrierLabPhaseIIIE3SelectionClass SourceSelectionClass,
		const FCarrierLabPhaseIIIE2BoundaryQueryAudit& BoundaryAudit,
		const double SignedSeparationVelocity,
		const bool bRestoreNonSeparatingAnomalyVeto,
		FCarrierLabPhaseIIIE4OceanicGenerationRecord& OutRecord)
	{
		OutRecord = FCarrierLabPhaseIIIE4OceanicGenerationRecord();
		OutRecord.SampleUnitPosition = NormalizeOrFallback(SamplePosition, FVector3d::UnitZ());
		OutRecord.SourceSelectionClass = SourceSelectionClass;
		OutRecord.bBoundaryPairFound = BoundaryAudit.bFound;
		OutRecord.SignedSeparationVelocity = SignedSeparationVelocity;
		OutRecord.Q1PlateId = BoundaryAudit.Q1PlateId;
		OutRecord.Q2PlateId = BoundaryAudit.Q2PlateId;
		OutRecord.Q1EdgeId = BoundaryAudit.Q1EdgeId;
		OutRecord.Q2EdgeId = BoundaryAudit.Q2EdgeId;
		OutRecord.Q1DistanceKm = BoundaryAudit.Q1DistanceKm;
		OutRecord.Q2DistanceKm = BoundaryAudit.Q2DistanceKm;
		OutRecord.Q1Elevation = BoundaryAudit.Q1Elevation;
		OutRecord.Q2Elevation = BoundaryAudit.Q2Elevation;
		OutRecord.QGammaInputNorm = BoundaryAudit.QGammaInputNorm;
		OutRecord.QGammaUnitResidual = BoundaryAudit.QGammaUnitResidual;
		OutRecord.Q1UnitPosition = BoundaryAudit.Q1UnitPosition;
		OutRecord.Q2UnitPosition = BoundaryAudit.Q2UnitPosition;
		OutRecord.QGammaUnitPosition = BoundaryAudit.QGammaUnitPosition;

		if (!IsPhaseIIIE4DivergentGapRoute(SourceSelectionClass))
		{
			OutRecord.bRejectedNonDivergentRoute = true;
			OutRecord.bRejectedUnresolvedMultiHit = IsPhaseIIIE4UnresolvedMultiHitRoute(SourceSelectionClass);
			return;
		}

		if (!BoundaryAudit.bFound)
		{
			return;
		}

		if (!(SignedSeparationVelocity > 0.0))
		{
			if (bRestoreNonSeparatingAnomalyVeto)
			{
				OutRecord.bNonSeparatingAnomaly = true;
				return;
			}
			OutRecord.bGeneratedWithNonPositiveSeparation = true;
		}

		const double QDistanceDenominator = BoundaryAudit.Q1DistanceKm + BoundaryAudit.Q2DistanceKm;
		const double QElevationT = QDistanceDenominator > UE_DOUBLE_SMALL_NUMBER
			? BoundaryAudit.Q1DistanceKm / QDistanceDenominator
			: 0.5;
		OutRecord.ZBarElevation = FMath::Lerp(BoundaryAudit.Q1Elevation, BoundaryAudit.Q2Elevation, QElevationT);

		OutRecord.RidgeDistanceKm = EarthRadiusKm * UnitAngularDistanceRadians(
			OutRecord.SampleUnitPosition,
			BoundaryAudit.QGammaUnitPosition);
		OutRecord.NearestBoundaryDistanceKm = FMath::Min(BoundaryAudit.Q1DistanceKm, BoundaryAudit.Q2DistanceKm);
		const double ElevationDenominator = OutRecord.RidgeDistanceKm + OutRecord.NearestBoundaryDistanceKm;
		OutRecord.Alpha = ElevationDenominator > UE_DOUBLE_SMALL_NUMBER
			? FMath::Clamp(OutRecord.RidgeDistanceKm / ElevationDenominator, 0.0, 1.0)
			: 0.0;
		// Named lab extension: alpha remains only the thesis zBar/zGamma blend
		// coefficient. zGamma uses a geophysics-derived sqrt distance profile so
		// gap width cannot alter the ridge profile, but this is not paper-faithful.
		OutRecord.ZGammaProfileDistanceKm = OutRecord.RidgeDistanceKm;
		OutRecord.ZGammaProfileReferenceDistanceKm = PhaseIIIE4ZGammaGeophysicsReferenceDistanceKm;
		OutRecord.ZGammaProfileT = PhaseIIIE4ZGammaGeophysicsReferenceDistanceKm > UE_DOUBLE_SMALL_NUMBER
			? FMath::Sqrt(FMath::Clamp(OutRecord.ZGammaProfileDistanceKm / PhaseIIIE4ZGammaGeophysicsReferenceDistanceKm, 0.0, 1.0))
			: 0.0;
		OutRecord.bUsedZGammaDistanceProfilePlaceholder = false;
		OutRecord.bUsedZGammaGeophysicsDerivedProfile = true;
		OutRecord.bPaperFaithfulZGammaProfile = false;
		OutRecord.ZGammaElevation = FMath::Lerp(
			PhaseIIIE4RidgePeakElevationKm,
			PhaseIIIE4AbyssalElevationKm,
			OutRecord.ZGammaProfileT);
		OutRecord.Elevation =
			OutRecord.Alpha * OutRecord.ZBarElevation +
			(1.0 - OutRecord.Alpha) * OutRecord.ZGammaElevation;
		OutRecord.OceanicAge = 0.0;
		OutRecord.RidgeDirection = RetangentAndNormalizeVectorField(
			FVector3d::CrossProduct(OutRecord.SampleUnitPosition - BoundaryAudit.QGammaUnitPosition, OutRecord.SampleUnitPosition),
			OutRecord.SampleUnitPosition);
		OutRecord.RidgeDirectionMagnitude = OutRecord.RidgeDirection.Size();
		OutRecord.RidgeDirectionRadialDot = FMath::Abs(FVector3d::DotProduct(OutRecord.RidgeDirection, OutRecord.SampleUnitPosition));
		OutRecord.AssignedPlateId = BoundaryAudit.Q1DistanceKm <= BoundaryAudit.Q2DistanceKm
			? BoundaryAudit.Q1PlateId
			: BoundaryAudit.Q2PlateId;
		OutRecord.bGeneratedOceanicCrust = true;
	}

	int32 CountPhaseIIIE5ActiveTriangles(const CarrierLab::FCarrierState& State)
	{
		int32 Count = 0;
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			Count += Plate.ActiveBoundaryTriangles.Num();
		}
		return Count;
	}

	int32 CountPhaseIIIE5ActiveDistanceRecords(const CarrierLab::FCarrierState& State)
	{
		int32 Count = 0;
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			Count += Plate.ActiveBoundaryTriangleDistancesKm.Num();
		}
		return Count;
	}

	int32 CountPhaseIIIE5DistanceToFrontRecords(const TArray<double>& DistancesKm)
	{
		int32 Count = 0;
		for (const double DistanceKm : DistancesKm)
		{
			if (FMath::IsFinite(DistanceKm) && DistanceKm >= 0.0)
			{
				++Count;
			}
		}
		return Count;
	}

	CarrierLab::FCarrierVertex MakePhaseIIIE5VertexFromSample(const CarrierLab::FSphereSample& Sample)
	{
		CarrierLab::FCarrierVertex Vertex;
		Vertex.GlobalSampleId = Sample.Id;
		Vertex.UnitPosition = Sample.UnitPosition;
		Vertex.AreaWeight = Sample.AreaWeight;
		Vertex.ContinentalFraction = Sample.ContinentalFraction;
		Vertex.Elevation = Sample.Elevation;
		Vertex.HistoricalElevation = Sample.HistoricalElevation;
		Vertex.OceanicAge = Sample.OceanicAge;
		Vertex.RidgeDirection = Sample.RidgeDirection;
		Vertex.FoldDirection = Sample.FoldDirection;
		Vertex.bHasHistoricalElevationSnapshot = false;
		Vertex.bContinental = Sample.bContinental;
		return Vertex;
	}

	int32 FindOrAddPhaseIIIE5LocalVertex(
		CarrierLab::FCarrierPlate& Plate,
		const CarrierLab::FSphereSample& Sample)
	{
		if (int32* Existing = Plate.GlobalSampleIdToLocalVertexId.Find(Sample.Id))
		{
			return *Existing;
		}

		const int32 LocalVertexId = Plate.Vertices.Add(MakePhaseIIIE5VertexFromSample(Sample));
		Plate.GlobalSampleIdToLocalVertexId.Add(Sample.Id, LocalVertexId);
		return LocalVertexId;
	}

	int32 AddPhaseIIIE5SyntheticLocalVertex(
		CarrierLab::FCarrierPlate& Plate,
		const FVector3d& UnitPosition,
		const TArray<int32>& SourceSampleIds,
		const TArray<FCarrierLabPhaseIIIE5RemeshVertexRecord>& VertexRecords,
		const CarrierLab::FCarrierState& State)
	{
		CarrierLab::FCarrierVertex Vertex;
		Vertex.GlobalSampleId = INDEX_NONE;
		Vertex.UnitPosition = NormalizeOrFallback(UnitPosition, FVector3d::UnitZ());
		if (SourceSampleIds.IsEmpty())
		{
			return Plate.Vertices.Add(Vertex);
		}

		double AreaWeight = 0.0;
		double ContinentalFraction = 0.0;
		double Elevation = 0.0;
		double HistoricalElevation = 0.0;
		double OceanicAge = 0.0;
		FVector3d RidgeDirection = FVector3d::ZeroVector;
		FVector3d FoldDirection = FVector3d::ZeroVector;
		int32 SourceCount = 0;
		for (const int32 SampleId : SourceSampleIds)
		{
			if (!State.Samples.IsValidIndex(SampleId) || !VertexRecords.IsValidIndex(SampleId))
			{
				continue;
			}
			const CarrierLab::FSphereSample& Sample = State.Samples[SampleId];
			const FCarrierLabPhaseIIIE5RemeshVertexRecord& Record = VertexRecords[SampleId];
			AreaWeight += Sample.AreaWeight;
			ContinentalFraction += Record.ContinentalFraction;
			Elevation += Record.Elevation;
			HistoricalElevation += Record.HistoricalElevation;
			OceanicAge += Record.OceanicAge;
			RidgeDirection += Record.RidgeDirection;
			FoldDirection += Record.FoldDirection;
			++SourceCount;
		}

		if (SourceCount > 0)
		{
			const double InvCount = 1.0 / static_cast<double>(SourceCount);
			Vertex.AreaWeight = AreaWeight * InvCount;
			Vertex.ContinentalFraction = FMath::Clamp(ContinentalFraction * InvCount, 0.0, 1.0);
			Vertex.Elevation = Elevation * InvCount;
			Vertex.HistoricalElevation = HistoricalElevation * InvCount;
			Vertex.OceanicAge = OceanicAge * InvCount;
			Vertex.RidgeDirection = RetangentAndNormalizeVectorField(RidgeDirection * InvCount, Vertex.UnitPosition);
			Vertex.FoldDirection = RetangentAndNormalizeVectorField(FoldDirection * InvCount, Vertex.UnitPosition);
			Vertex.bContinental = Vertex.ContinentalFraction >= 0.5;
		}
		return Plate.Vertices.Add(Vertex);
	}

	int32 AddPhaseIIIE5LocalTriangle(
		CarrierLab::FCarrierPlate& Plate,
		const int32 SourceTriangleId,
		const int32 A,
		const int32 B,
		const int32 C,
		const bool bBoundary)
	{
		CarrierLab::FCarrierPlateTriangle LocalTriangle;
		LocalTriangle.A = A;
		LocalTriangle.B = B;
		LocalTriangle.C = C;
		LocalTriangle.SourceTriangleId = SourceTriangleId;
		LocalTriangle.bBoundary = bBoundary;
		return Plate.LocalTriangles.Add(LocalTriangle);
	}

	ECarrierLabPhaseIIIE5TriangleAssignmentClass ClassifyPhaseIIIE5TriangleAssignment(
		const int32 PlateA,
		const int32 PlateB,
		const int32 PlateC,
		int32& OutAssignedPlateId)
	{
		OutAssignedPlateId = INDEX_NONE;
		if (PlateA == INDEX_NONE || PlateB == INDEX_NONE || PlateC == INDEX_NONE)
		{
			return ECarrierLabPhaseIIIE5TriangleAssignmentClass::Invalid;
		}
		if (PlateA == PlateB && PlateB == PlateC)
		{
			OutAssignedPlateId = PlateA;
			return ECarrierLabPhaseIIIE5TriangleAssignmentClass::AllVerticesSamePlate;
		}
		if (PlateA == PlateB || PlateA == PlateC)
		{
			OutAssignedPlateId = PlateA;
			return ECarrierLabPhaseIIIE5TriangleAssignmentClass::MajorityTwoOfThree;
		}
		if (PlateB == PlateC)
		{
			OutAssignedPlateId = PlateB;
			return ECarrierLabPhaseIIIE5TriangleAssignmentClass::MajorityTwoOfThree;
		}
		return ECarrierLabPhaseIIIE5TriangleAssignmentClass::TripleJunctionCentroidSplit;
	}

	FString ComputePhaseIIIE5PlateTopologyHash(const CarrierLab::FCarrierPlate& Plate)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIIE5-plate-topology-v1"));
		HashMix(Hash, static_cast<uint64>(Plate.PlateId + 1));
		HashMix(Hash, static_cast<uint64>(Plate.Vertices.Num() + 1));
		for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			HashMix(Hash, static_cast<uint64>(Vertex.GlobalSampleId + 2));
			HashMixDouble(Hash, Vertex.ContinentalFraction);
			HashMixDouble(Hash, Vertex.Elevation);
			HashMixDouble(Hash, Vertex.HistoricalElevation);
			HashMixDouble(Hash, Vertex.OceanicAge);
			HashMixDouble(Hash, Vertex.RidgeDirection.X);
			HashMixDouble(Hash, Vertex.RidgeDirection.Y);
			HashMixDouble(Hash, Vertex.RidgeDirection.Z);
		}
		HashMix(Hash, static_cast<uint64>(Plate.LocalTriangles.Num() + 1));
		for (const CarrierLab::FCarrierPlateTriangle& Triangle : Plate.LocalTriangles)
		{
			HashMix(Hash, static_cast<uint64>(Triangle.SourceTriangleId + 2));
			HashMix(Hash, static_cast<uint64>(Triangle.A + 2));
			HashMix(Hash, static_cast<uint64>(Triangle.B + 2));
			HashMix(Hash, static_cast<uint64>(Triangle.C + 2));
			HashMix(Hash, Triangle.bBoundary ? 1ull : 0ull);
		}
		return HashToString(Hash);
	}

	void ResetPhaseIIIE5ProcessState(
		CarrierLab::FCarrierState& State,
		TArray<double>& DistanceToFrontKmBySample,
		FCarrierLabPhaseIIIE5ProcessResetAudit& OutResetAudit)
	{
		OutResetAudit.ResetSerialBefore = State.ConvergenceTrackingResetSerial;
		OutResetAudit.ActiveTriangleCountBefore = CountPhaseIIIE5ActiveTriangles(State);
		OutResetAudit.DistanceRecordCountBefore = CountPhaseIIIE5ActiveDistanceRecords(State);
		OutResetAudit.MatrixPairCountBefore = State.ConvergenceSubductionMatrixPairKeys.Num();
		OutResetAudit.MatrixEvidenceCountBefore = State.ConvergenceSubductionMatrixEvidence.Num();
		OutResetAudit.SubductingMarkCountBefore = State.ConvergenceSubductingTriangleMarks.Num();
		OutResetAudit.ObductionMarkCountBefore = State.ConvergenceObductionTriangleMarks.Num();
		OutResetAudit.CollisionPendingTriangleCountBefore = State.ConvergenceCollisionPendingTriangleKeys.Num();
		OutResetAudit.DistanceToFrontRecordCountBefore = CountPhaseIIIE5DistanceToFrontRecords(DistanceToFrontKmBySample);

		for (CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			Plate.ActiveBoundaryTriangles.Reset();
			Plate.ActiveBoundaryTriangleDistancesKm.Reset();
		}
		State.ConvergenceSubductionMatrixPairKeys.Reset();
		State.ConvergenceSubductionPolarityDecisions.Reset();
		State.ConvergenceSubductionTriangleHits.Reset();
		State.ConvergenceSubductionMatrixEvidence.Reset();
		State.ConvergenceSubductingTriangleMarks.Reset();
		State.ConvergenceObductionTriangleMarks.Reset();
		State.ConvergenceCollisionPendingTriangleKeys.Reset();
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
		State.ConvergenceObductionTriangleMarkDuplicateCount = 0;
		State.ConvergenceObductionTriangleMarkInvalidCount = 0;
		State.ConvergenceHistoricalElevationSnapshotCount = 0;
		State.ConvergenceHistoricalElevationSnapshotVertexCount = 0;
		State.ConvergenceHistoricalElevationDuplicateSnapshotCount = 0;
		State.ConvergenceHistoricalElevationInvalidSnapshotCount = 0;
		DistanceToFrontKmBySample.Init(-1.0, State.Samples.Num());
		++State.ConvergenceTrackingResetSerial;

		OutResetAudit.ResetSerialAfter = State.ConvergenceTrackingResetSerial;
		OutResetAudit.ActiveTriangleCountAfter = CountPhaseIIIE5ActiveTriangles(State);
		OutResetAudit.DistanceRecordCountAfter = CountPhaseIIIE5ActiveDistanceRecords(State);
		OutResetAudit.MatrixPairCountAfter = State.ConvergenceSubductionMatrixPairKeys.Num();
		OutResetAudit.MatrixEvidenceCountAfter = State.ConvergenceSubductionMatrixEvidence.Num();
		OutResetAudit.SubductingMarkCountAfter = State.ConvergenceSubductingTriangleMarks.Num();
		OutResetAudit.ObductionMarkCountAfter = State.ConvergenceObductionTriangleMarks.Num();
		OutResetAudit.CollisionPendingTriangleCountAfter = State.ConvergenceCollisionPendingTriangleKeys.Num();
		OutResetAudit.DistanceToFrontRecordCountAfter = CountPhaseIIIE5DistanceToFrontRecords(DistanceToFrontKmBySample);
		OutResetAudit.bResetSerialAdvanced = OutResetAudit.ResetSerialAfter == OutResetAudit.ResetSerialBefore + 1;
		OutResetAudit.bProcessStateEmptyAfter =
			OutResetAudit.ActiveTriangleCountAfter == 0 &&
			OutResetAudit.DistanceRecordCountAfter == 0 &&
			OutResetAudit.MatrixPairCountAfter == 0 &&
			OutResetAudit.MatrixEvidenceCountAfter == 0 &&
			OutResetAudit.SubductingMarkCountAfter == 0 &&
			OutResetAudit.ObductionMarkCountAfter == 0 &&
			OutResetAudit.CollisionPendingTriangleCountAfter == 0 &&
			OutResetAudit.DistanceToFrontRecordCountAfter == 0;
	}

	void BuildPhaseIIIE5AuditHashes(FCarrierLabPhaseIIIE5TopologyRebuildAudit& Audit)
	{
		uint64 AssignmentHash = 1469598103934665603ull;
		HashMixString(AssignmentHash, TEXT("CarrierLab-IIIE5-remesh-assignment-v1"));
		HashMix(AssignmentHash, static_cast<uint64>(Audit.VertexRecords.Num() + 1));
		for (const FCarrierLabPhaseIIIE5RemeshVertexRecord& Record : Audit.VertexRecords)
		{
			HashMix(AssignmentHash, static_cast<uint64>(Record.SampleId + 2));
			HashMix(AssignmentHash, static_cast<uint64>(Record.AssignedPlateId + 2));
			HashMix(AssignmentHash, Record.bGeneratedOceanicCrust ? 1ull : 0ull);
			HashMix(AssignmentHash, Record.bUsedPolicyWinner ? 1ull : 0ull);
			HashMix(AssignmentHash, Record.bUsedPriorOwnerFallback ? 1ull : 0ull);
			HashMix(AssignmentHash, Record.bUsedProjectionOwnerFallback ? 1ull : 0ull);
			HashMix(AssignmentHash, Record.bUsedZGammaDistanceProfilePlaceholder ? 1ull : 0ull);
			HashMix(AssignmentHash, Record.bUsedZGammaGeophysicsDerivedProfile ? 1ull : 0ull);
			HashMix(AssignmentHash, Record.bPaperFaithfulZGammaProfile ? 1ull : 0ull);
			HashMixDouble(AssignmentHash, Record.ContinentalFraction);
			HashMixDouble(AssignmentHash, Record.Elevation);
			HashMixDouble(AssignmentHash, Record.OceanicAge);
		}
		Audit.AssignmentHash = HashToString(AssignmentHash);

		uint64 TopologyHash = 1469598103934665603ull;
		HashMixString(TopologyHash, TEXT("CarrierLab-IIIE5-topology-rebuild-v1"));
		HashMixString(TopologyHash, Audit.AssignmentHash);
		HashMix(TopologyHash, static_cast<uint64>(Audit.SampleCount + 1));
		HashMix(TopologyHash, static_cast<uint64>(Audit.GlobalTriangleCount + 1));
		HashMix(TopologyHash, static_cast<uint64>(Audit.AssignedTriangleCount + 1));
		HashMix(TopologyHash, static_cast<uint64>(Audit.MajorityTriangleCount + 1));
		HashMix(TopologyHash, static_cast<uint64>(Audit.TripleJunctionCentroidSplitCount + 1));
		HashMix(TopologyHash, static_cast<uint64>(Audit.TripleJunctionCentroidSplitLocalTriangleCount + 1));
		HashMix(TopologyHash, static_cast<uint64>(Audit.TripleJunctionCentroidSplitSyntheticVertexCount + 1));
		HashMix(TopologyHash, static_cast<uint64>(Audit.UnresolvedTripleJunctionCount + 1));
		for (const FCarrierLabPhaseIIIE5PlateRebuildRecord& PlateRecord : Audit.PlateRecords)
		{
			HashMix(TopologyHash, static_cast<uint64>(PlateRecord.PlateId + 1));
			HashMix(TopologyHash, static_cast<uint64>(PlateRecord.VertexCount + 1));
			HashMix(TopologyHash, static_cast<uint64>(PlateRecord.TriangleCount + 1));
			HashMixString(TopologyHash, PlateRecord.TopologyHash);
		}
		HashMix(TopologyHash, Audit.ResetAudit.bProcessStateEmptyAfter ? 1ull : 0ull);
		HashMix(TopologyHash, Audit.bMotionPreserved ? 1ull : 0ull);
		HashMix(TopologyHash, Audit.bQProvenancePreserved ? 1ull : 0ull);
		HashMix(TopologyHash, Audit.bZGammaHoldPreserved ? 1ull : 0ull);
		Audit.TopologyHash = HashToString(TopologyHash);
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
		case 5:
			return FLinearColor(0.74f, 0.24f, 0.86f, 1.0f);
		case 6:
			return FLinearColor(0.96f, 0.10f, 0.20f, 1.0f);
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

	FLinearColor OceanicAgeColor(const double AgeMa)
	{
		if (!FMath::IsFinite(AgeMa) || AgeMa < 0.0)
		{
			return FLinearColor(0.02f, 0.05f, 0.09f, 1.0f);
		}
		const double Alpha = FMath::Clamp(AgeMa / 150.0, 0.0, 1.0);
		return FMath::Lerp(
			FLinearColor(0.05f, 0.80f, 0.88f, 1.0f),
			FLinearColor(0.02f, 0.05f, 0.40f, 1.0f),
			static_cast<float>(Alpha));
	}

	FLinearColor RidgeDirectionColor(const FVector3d& Direction, const FVector3d& UnitPosition)
	{
		if (!FMath::IsFinite(Direction.X) ||
			!FMath::IsFinite(Direction.Y) ||
			!FMath::IsFinite(Direction.Z) ||
			Direction.SquaredLength() <= UE_DOUBLE_SMALL_NUMBER)
		{
			return FLinearColor(0.02f, 0.05f, 0.09f, 1.0f);
		}

		const FVector3d Normal = NormalizeOrFallback(UnitPosition, FVector3d::UnitZ());
		FVector3d East = FVector3d::CrossProduct(FVector3d::UnitZ(), Normal);
		if (East.SquaredLength() <= UE_DOUBLE_SMALL_NUMBER)
		{
			East = FVector3d::CrossProduct(FVector3d::UnitX(), Normal);
		}
		East.Normalize();
		const FVector3d North = NormalizeOrFallback(FVector3d::CrossProduct(Normal, East), FVector3d::UnitY());
		const FVector3d TangentDirection = RetangentAndNormalizeVectorField(Direction, Normal);
		const double Angle = FMath::Atan2(
			FVector3d::DotProduct(TangentDirection, North),
			FVector3d::DotProduct(TangentDirection, East));
		const uint8 Hue = static_cast<uint8>(FMath::RoundToInt(((Angle + UE_PI) / (2.0 * UE_PI)) * 255.0) & 255);
		return FLinearColor::MakeFromHSV8(Hue, 190, 245);
	}

	FLinearColor RemeshSummaryColor(
		const FLinearColor& Base,
		const uint8 RoleMask,
		const uint8 EventMask,
		const double ContinentalFraction,
		const double OceanicAgeMa,
		const double ElevationKm,
		const FVector3d& RidgeDirection,
		const FVector3d& UnitPosition)
	{
		FLinearColor Color = DimMapBase(Base);
		if (ContinentalFraction <= 1.0e-6)
		{
			Color = BlendMapOverlay(Color, OceanicAgeColor(OceanicAgeMa), 0.46f);
		}
		if (FMath::Abs(ElevationKm) > 1.0e-9)
		{
			Color = BlendMapOverlay(Color, ElevationColor(ElevationKm), 0.42f);
		}
		if (RidgeDirection.SquaredLength() > UE_DOUBLE_SMALL_NUMBER)
		{
			Color = BlendMapOverlay(Color, RidgeDirectionColor(RidgeDirection, UnitPosition), 0.56f);
		}
		if (RoleMask != 0)
		{
			Color = BlendMapOverlay(Color, SubductionRoleColor(RoleMask), 0.76f);
		}
		if ((EventMask & PhaseIIIELiveRemeshMaskCoalescedDuplicate) != 0)
		{
			Color = BlendMapOverlay(Color, FLinearColor(0.52f, 0.30f, 1.0f, 1.0f), 0.80f);
		}
		if ((EventMask & PhaseIIIELiveRemeshMaskSharedBoundaryTieBreak) != 0)
		{
			Color = BlendMapOverlay(Color, FLinearColor(0.20f, 1.0f, 0.45f, 1.0f), 0.80f);
		}
		if ((EventMask & PhaseIIIELiveRemeshMaskDistanceTieFallback) != 0)
		{
			Color = BlendMapOverlay(Color, FLinearColor(1.0f, 0.28f, 0.08f, 1.0f), 0.80f);
		}
		if ((EventMask & PhaseIIIELiveRemeshMaskAppliedGenerated) != 0)
		{
			Color = BlendMapOverlay(Color, FLinearColor(0.0f, 0.95f, 1.0f, 1.0f), 0.86f);
		}
		if ((EventMask & PhaseIIIELiveRemeshMaskRiftingPending) != 0)
		{
			Color = BlendMapOverlay(Color, FLinearColor(1.0f, 0.72f, 0.05f, 1.0f), 0.86f);
		}
		if ((EventMask & PhaseIIIELiveRemeshMaskUnresolvedHold) != 0)
		{
			Color = BlendMapOverlay(Color, FLinearColor(1.0f, 0.05f, 0.72f, 1.0f), 0.92f);
		}
		return Color;
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
		bool bNeedsProjection = false;
		while (StepAccumulator >= 1.0 && StepsThisFrame < 8)
		{
			const bool bProjectedThisStep = AdvanceOneStepWithNaturalResampling();
			bNeedsProjection = !bProjectedThisStep;
			StepAccumulator -= 1.0;
			++StepsThisFrame;
		}
		if (StepsThisFrame > 0 && bNeedsProjection)
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
	LastPhaseIIICObductionUpliftAudit = FCarrierLabPhaseIIICObductionUpliftAudit();
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
	ResetObservedSpeedWindowForRemesh();
	UpdateNaturalCadenceMetrics(false);
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
	ResetObservedSpeedWindowForRemesh();
	CurrentMetrics.NextResampleStep = 0;
	UpdateNaturalCadenceMetrics(false);
}

void ACarrierLabVisualizationActor::ConfigurePhaseIIICProcessLayer(const bool bEnabled, const bool bInEnableSlabPull)
{
	bEnablePhaseIIICSubductingMarks = bEnabled;
	bEnablePhaseIIICVisibleHistoricalElevation = bEnabled;
	bEnablePhaseIIICOverridingPlateUplift = bEnabled;
	bEnablePhaseIIICObductionUpliftBridge = bEnabled;
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
			else
			{
				++OutMetrics.NoBoundaryPairMissCount;
				FCarrierLabPhaseIIFilterDecisionRecord& Decision = OutDecisions.AddDefaulted_GetRef();
				Decision.DecisionId = OutDecisions.Num() - 1;
				Decision.EventId = OutMetrics.EventId;
				Decision.Step = OutMetrics.Step;
				Decision.SampleId = Sample.Id;
				Decision.RawCandidateCount = Candidates.Num();
				Decision.RawPlateCount = RawPlateCount;
				Decision.DecisionClass = ECarrierLabPhaseIIFilterDecisionClass::FilterExhausted;
				SetMaterialClass(
					Sample.Id,
					ECarrierLabPhaseIIMaterialEventClass::FilterExhaustedUnknown,
					ECarrierLabPhaseIIFilterDecisionClass::FilterExhausted,
					RawPlateCount,
					0,
					false,
					false,
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

	ensureMsgf(
		OutMetrics.NoBoundaryPairMissCount == 0,
		TEXT("CarrierLab filtered resampling hit %d no-boundary-pair misses; this path is a retention hazard and must remain gated."),
		OutMetrics.NoBoundaryPairMissCount);

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
	CurrentMetrics.LastNoBoundaryPairMissCount = OutMetrics.NoBoundaryPairMissCount;
	CurrentMetrics.PolicyResolvedMultiHitCount = 0;
	CurrentMetrics.LastRemeshMode = TEXT("phase_ii_filtered_process");
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
	HashMix(DecisionHash, static_cast<uint64>(OutMetrics.NoBoundaryPairMissCount + 1));
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

bool ACarrierLabVisualizationActor::QueryPhaseIIIE2ContinuousBoundaryPairForTest(
	const FVector3d& SamplePosition,
	const TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe>& BoundaryEdges,
	FCarrierLabPhaseIIIE2BoundaryQueryAudit& OutAudit) const
{
	TArray<FCarrierLabContinuousBoundaryEdge> Edges;
	Edges.Reserve(BoundaryEdges.Num());
	for (int32 EdgeIndex = 0; EdgeIndex < BoundaryEdges.Num(); ++EdgeIndex)
	{
		const FCarrierLabPhaseIIIE2BoundaryEdgeProbe& Probe = BoundaryEdges[EdgeIndex];
		FCarrierLabContinuousBoundaryEdge Edge;
		Edge.EdgeId = EdgeIndex;
		Edge.PlateId = Probe.PlateId;
		Edge.StartUnitPosition = Probe.StartUnitPosition;
		Edge.EndUnitPosition = Probe.EndUnitPosition;
		Edge.StartContinentalFraction = Probe.StartContinentalFraction;
		Edge.EndContinentalFraction = Probe.EndContinentalFraction;
		Edge.StartElevation = Probe.StartElevation;
		Edge.EndElevation = Probe.EndElevation;
		Edges.Add(Edge);
	}
	return QueryContinuousBoundaryPair(Edges, SamplePosition, OutAudit);
}

bool ACarrierLabVisualizationActor::BuildPhaseIIIE2BoundaryEdgesFromCurrentStateForTest(
	TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe>& OutBoundaryEdges) const
{
	OutBoundaryEdges.Reset();
	if (!bInitialized)
	{
		return false;
	}

	const TArray<FCarrierLabContinuousBoundaryEdge> Edges = BuildContinuousBoundaryEdges(State);
	OutBoundaryEdges.Reserve(Edges.Num());
	for (const FCarrierLabContinuousBoundaryEdge& Edge : Edges)
	{
		FCarrierLabPhaseIIIE2BoundaryEdgeProbe& Probe = OutBoundaryEdges.AddDefaulted_GetRef();
		Probe.PlateId = Edge.PlateId;
		Probe.StartUnitPosition = Edge.StartUnitPosition;
		Probe.EndUnitPosition = Edge.EndUnitPosition;
		Probe.StartContinentalFraction = Edge.StartContinentalFraction;
		Probe.EndContinentalFraction = Edge.EndContinentalFraction;
		Probe.StartElevation = Edge.StartElevation;
		Probe.EndElevation = Edge.EndElevation;
	}
	return !OutBoundaryEdges.IsEmpty();
}

bool ACarrierLabVisualizationActor::QueryPhaseIIIE2ContinuousBoundaryPairFromCurrentStateForTest(
	const FVector3d& SamplePosition,
	FCarrierLabPhaseIIIE2BoundaryQueryAudit& OutAudit) const
{
	if (!bInitialized)
	{
		OutAudit = FCarrierLabPhaseIIIE2BoundaryQueryAudit();
		return false;
	}
	const TArray<FCarrierLabContinuousBoundaryEdge> Edges = BuildContinuousBoundaryEdges(State);
	return QueryContinuousBoundaryPair(Edges, SamplePosition, OutAudit);
}

bool ACarrierLabVisualizationActor::QueryPhaseIIIE3FilteredRemeshSelectionForTest(
	const FVector3d& SamplePosition,
	const TArray<FCarrierLabPhaseIIIE3CandidateProbe>& CandidateProbes,
	FCarrierLabPhaseIIIE3SelectionRecord& OutRecord) const
{
	TArray<FCarrierLabIIIE3SelectionCandidate> Candidates;
	Candidates.Reserve(CandidateProbes.Num());
	for (const FCarrierLabPhaseIIIE3CandidateProbe& Probe : CandidateProbes)
	{
		FCarrierLabIIIE3SelectionCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.PlateId = Probe.PlateId;
		Candidate.LocalTriangleId = Probe.LocalTriangleId;
		Candidate.SourceTriangleId = Probe.SourceTriangleId;
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			Candidate.GlobalVertexIds[Corner] = Probe.GlobalVertexIds[Corner];
		}
		Candidate.Bary = Probe.Bary;
		Candidate.Distance = Probe.Distance;
		Candidate.ContinentalFraction = FMath::Clamp(Probe.ContinentalFraction, 0.0, 1.0);
		Candidate.PlateContinentalFraction = FMath::Clamp(Probe.PlateContinentalFraction, 0.0, 1.0);
		Candidate.PlateOceanicAge = FMath::Max(Probe.PlateOceanicAge, 0.0);
		Candidate.Fields.Elevation = Probe.Elevation;
		Candidate.Fields.HistoricalElevation = Probe.HistoricalElevation;
		Candidate.Fields.OceanicAge = Probe.OceanicAge;
		Candidate.Fields.RidgeDirection = RetangentAndNormalizeVectorField(Probe.RidgeDirection, NormalizeOrFallback(SamplePosition, FVector3d::UnitZ()));
		Candidate.Fields.FoldDirection = RetangentAndNormalizeVectorField(Probe.FoldDirection, NormalizeOrFallback(SamplePosition, FVector3d::UnitZ()));
		Candidate.FilterReason = Probe.FilterReason;
		Candidate.bBoundary = Probe.bBoundary;
		Candidate.bHasSourceTopologySnapshot = Probe.bHasSourceTopologySnapshot;
		Candidate.bHasPlateAggregateOverride = Probe.bHasPlateAggregateOverride;
	}

	return SelectPhaseIIIE3FilteredRemeshSource(
		INDEX_NONE,
		SamplePosition,
		Candidates,
		bEnablePhaseIIIE3DuplicateHitCoalescing,
		bEnablePhaseIIIE3SharedBoundaryTieBreak,
		bEnablePhaseIIIE3NearestHitTieBreak,
		bEnablePhaseIIIE3DistanceTieFallback,
		nullptr,
		OutRecord);
}

bool ACarrierLabVisualizationActor::QueryPhaseIIIE4DivergentOceanicFieldsForTest(
	const FVector3d& SamplePosition,
	const ECarrierLabPhaseIIIE3SelectionClass SourceSelectionClass,
	const TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe>& BoundaryEdges,
	const double SignedSeparationVelocity,
	FCarrierLabPhaseIIIE4OceanicGenerationRecord& OutRecord) const
{
	FCarrierLabPhaseIIIE2BoundaryQueryAudit BoundaryAudit;
	if (IsPhaseIIIE4DivergentGapRoute(SourceSelectionClass))
	{
		QueryPhaseIIIE2ContinuousBoundaryPairForTest(SamplePosition, BoundaryEdges, BoundaryAudit);
	}
	PopulatePhaseIIIE4OceanicRecord(
		SamplePosition,
		SourceSelectionClass,
		BoundaryAudit,
		SignedSeparationVelocity,
		bRestoreNonSeparatingAnomalyVeto,
		OutRecord);
	return true;
}

bool ACarrierLabVisualizationActor::QueryPhaseIIIE4DivergentOceanicFieldsFromCurrentStateForTest(
	const FVector3d& SamplePosition,
	const ECarrierLabPhaseIIIE3SelectionClass SourceSelectionClass,
	FCarrierLabPhaseIIIE4OceanicGenerationRecord& OutRecord) const
{
	if (!bInitialized)
	{
		OutRecord = FCarrierLabPhaseIIIE4OceanicGenerationRecord();
		return false;
	}

	FCarrierLabPhaseIIIE2BoundaryQueryAudit BoundaryAudit;
	if (IsPhaseIIIE4DivergentGapRoute(SourceSelectionClass))
	{
		QueryPhaseIIIE2ContinuousBoundaryPairFromCurrentStateForTest(SamplePosition, BoundaryAudit);
	}
	const double SignedSeparationVelocity = BoundaryAudit.bFound
		? SignedPairSeparationVelocityForPlatePair(
			BoundaryAudit.QGammaUnitPosition,
			Motions,
			BoundaryAudit.Q1PlateId,
			BoundaryAudit.Q2PlateId)
		: 0.0;
	PopulatePhaseIIIE4OceanicRecord(
		SamplePosition,
		SourceSelectionClass,
		BoundaryAudit,
		SignedSeparationVelocity,
		bRestoreNonSeparatingAnomalyVeto,
		OutRecord);
	return true;
}

bool ACarrierLabVisualizationActor::RunPhaseIIIE5TopologyRebuildFixtureForTest(
	const FCarrierLabPhaseIIIE5RemeshInputFixture& Fixture,
	FCarrierLabPhaseIIIE5TopologyRebuildAudit& OutAudit)
{
	OutAudit = FCarrierLabPhaseIIIE5TopologyRebuildAudit();
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}

	OutAudit.bRan = true;
	OutAudit.SampleCount = State.Samples.Num();
	OutAudit.GlobalTriangleCount = State.Triangles.Num();
	OutAudit.MotionHashBefore = ComputeMotionStateHash(Motions);
	OutAudit.bFixtureOwnedVertexAssignmentRecords = !Fixture.bUseExplicitVertexRecords;

	TArray<FCarrierLabPhaseIIIE5RemeshVertexRecord> VertexRecords;
	if (Fixture.bUseExplicitVertexRecords)
	{
		VertexRecords = Fixture.ExplicitVertexRecords;
		if (VertexRecords.Num() != State.Samples.Num())
		{
			++OutAudit.MissingVertexAssignmentCount;
			VertexRecords.SetNum(State.Samples.Num());
		}
	}
	else
	{
		VertexRecords.SetNum(State.Samples.Num());
		for (const CarrierLab::FSphereSample& Sample : State.Samples)
		{
			if (!VertexRecords.IsValidIndex(Sample.Id))
			{
				++OutAudit.MissingVertexAssignmentCount;
				continue;
			}
			FCarrierLabPhaseIIIE5RemeshVertexRecord& Record = VertexRecords[Sample.Id];
			Record.SampleId = Sample.Id;
			Record.AssignedPlateId = Fixture.bForceAllSamplesToPlateZero ? 0 : Sample.PlateId;
			Record.bResolvedSingleHit = true;
			Record.PreRemeshContinentalFraction = Sample.ContinentalFraction;
			Record.PreRemeshElevation = Sample.Elevation;
			Record.ContinentalFraction = Sample.ContinentalFraction;
			Record.Elevation = Sample.Elevation;
			Record.HistoricalElevation = Sample.HistoricalElevation;
			Record.OceanicAge = Sample.OceanicAge;
			Record.RidgeDirection = Sample.RidgeDirection;
			Record.FoldDirection = Sample.FoldDirection;
		}

		int32 OverrideVerts[3] = { INDEX_NONE, INDEX_NONE, INDEX_NONE };
		if (State.Triangles.IsValidIndex(Fixture.OverrideTriangleId))
		{
			const CarrierLab::FSphereTriangle& Triangle = State.Triangles[Fixture.OverrideTriangleId];
			OverrideVerts[0] = Triangle.A;
			OverrideVerts[1] = Triangle.B;
			OverrideVerts[2] = Triangle.C;
			const int32 OverridePlates[3] = { Fixture.OverridePlateA, Fixture.OverridePlateB, Fixture.OverridePlateC };
			for (int32 Index = 0; Index < 3; ++Index)
			{
				if (VertexRecords.IsValidIndex(OverrideVerts[Index]) && State.Plates.IsValidIndex(OverridePlates[Index]))
				{
					VertexRecords[OverrideVerts[Index]].AssignedPlateId = OverridePlates[Index];
				}
			}
		}

		if (Fixture.bInjectGeneratedOceanicRecord)
		{
			const int32 TargetSampleId = OverrideVerts[0] != INDEX_NONE ? OverrideVerts[0] : 0;
			if (VertexRecords.IsValidIndex(TargetSampleId))
			{
				FCarrierLabPhaseIIIE5RemeshVertexRecord& Record = VertexRecords[TargetSampleId];
				Record.bResolvedSingleHit = false;
				Record.bDivergentGapRoute = true;
				Record.bGeneratedOceanicCrust = true;
				Record.AssignedPlateId = State.Plates.IsValidIndex(Fixture.GeneratedOceanicRecord.AssignedPlateId)
					? Fixture.GeneratedOceanicRecord.AssignedPlateId
					: Record.AssignedPlateId;
				Record.ContinentalFraction = 0.0;
				Record.Elevation = Fixture.GeneratedOceanicRecord.Elevation;
				Record.HistoricalElevation = 0.0;
				Record.OceanicAge = Fixture.GeneratedOceanicRecord.OceanicAge;
				Record.RidgeDirection = Fixture.GeneratedOceanicRecord.RidgeDirection;
				Record.FoldDirection = FVector3d::ZeroVector;
				Record.bUsedZGammaDistanceProfilePlaceholder = Fixture.GeneratedOceanicRecord.bUsedZGammaDistanceProfilePlaceholder;
				Record.bUsedZGammaGeophysicsDerivedProfile = Fixture.GeneratedOceanicRecord.bUsedZGammaGeophysicsDerivedProfile;
				Record.bPaperFaithfulZGammaProfile = Fixture.GeneratedOceanicRecord.bPaperFaithfulZGammaProfile;
				Record.OceanicRecord = Fixture.GeneratedOceanicRecord;
				Record.OceanicRecord.SampleId = TargetSampleId;
			}
		}

		if (Fixture.bSeedProcessStateBeforeRemesh)
		{
			for (CarrierLab::FCarrierPlate& Plate : State.Plates)
			{
				if (!Plate.LocalTriangles.IsEmpty())
				{
					Plate.ActiveBoundaryTriangles.AddUnique(0);
					if (Plate.ActiveBoundaryTriangleDistancesKm.Num() < Plate.ActiveBoundaryTriangles.Num())
					{
						Plate.ActiveBoundaryTriangleDistancesKm.Add(123.0);
					}
					break;
				}
			}
			if (State.Plates.Num() >= 2)
			{
				State.ConvergenceSubductionMatrixPairKeys.Add(MakePlatePairKey(0, 1));
				CarrierLab::FConvergenceSubductionMatrixEvidence& Evidence =
					State.ConvergenceSubductionMatrixEvidence.AddDefaulted_GetRef();
				Evidence.EvidenceId = 0;
				Evidence.PairKey = MakePlatePairKey(0, 1);
				Evidence.PlateId = 0;
				Evidence.OtherPlateId = 1;
				Evidence.LocalTriangleId = 0;
				Evidence.OtherLocalTriangleId = 0;
				Evidence.bAccepted = true;

				CarrierLab::FConvergenceSubductingTriangleMark& SubductingMark =
					State.ConvergenceSubductingTriangleMarks.AddDefaulted_GetRef();
				SubductingMark.MarkId = 0;
				SubductingMark.PairKey = Evidence.PairKey;
				SubductingMark.PlateId = 0;
				SubductingMark.OtherPlateId = 1;
				SubductingMark.LocalTriangleId = 0;

				CarrierLab::FConvergenceObductionTriangleMark& ObductionMark =
					State.ConvergenceObductionTriangleMarks.AddDefaulted_GetRef();
				ObductionMark.MarkId = 0;
				ObductionMark.PairKey = Evidence.PairKey;
				ObductionMark.PlateId = 1;
				ObductionMark.OtherPlateId = 0;
				ObductionMark.LocalTriangleId = 0;
				State.ConvergenceCollisionPendingTriangleKeys.Add(MakePlateTriangleKey(0, 0));
			}
			DistanceToFrontKmBySample.Init(321.0, State.Samples.Num());
		}
	}

	FCarrierLabPhaseIIIE5ProcessResetAudit ResetBeforeRebuild;
	ResetBeforeRebuild.ResetSerialBefore = State.ConvergenceTrackingResetSerial;
	ResetBeforeRebuild.ActiveTriangleCountBefore = CountPhaseIIIE5ActiveTriangles(State);
	ResetBeforeRebuild.DistanceRecordCountBefore = CountPhaseIIIE5ActiveDistanceRecords(State);
	ResetBeforeRebuild.MatrixPairCountBefore = State.ConvergenceSubductionMatrixPairKeys.Num();
	ResetBeforeRebuild.MatrixEvidenceCountBefore = State.ConvergenceSubductionMatrixEvidence.Num();
	ResetBeforeRebuild.SubductingMarkCountBefore = State.ConvergenceSubductingTriangleMarks.Num();
	ResetBeforeRebuild.ObductionMarkCountBefore = State.ConvergenceObductionTriangleMarks.Num();
	ResetBeforeRebuild.CollisionPendingTriangleCountBefore = State.ConvergenceCollisionPendingTriangleKeys.Num();
	ResetBeforeRebuild.DistanceToFrontRecordCountBefore = CountPhaseIIIE5DistanceToFrontRecords(DistanceToFrontKmBySample);

	for (const FCarrierLabPhaseIIIE5RemeshVertexRecord& Record : VertexRecords)
	{
		OutAudit.PriorOwnerFallbackCount += Record.bUsedPriorOwnerFallback ? 1 : 0;
		OutAudit.ProjectionOwnerFallbackCount += Record.bUsedProjectionOwnerFallback ? 1 : 0;
		OutAudit.PolicyWinnerCount += Record.bUsedPolicyWinner ? 1 : 0;
		OutAudit.UnresolvedMultiHitRoutedCount += Record.bUnresolvedMultiHit ? 1 : 0;
		if (Record.bGeneratedOceanicCrust)
		{
			++OutAudit.GeneratedOceanicVertexCount;
		}
		if (!State.Samples.IsValidIndex(Record.SampleId))
		{
			++OutAudit.MissingVertexAssignmentCount;
			continue;
		}
		if (!State.Plates.IsValidIndex(Record.AssignedPlateId))
		{
			++OutAudit.InvalidAssignedPlateCount;
			continue;
		}

		CarrierLab::FSphereSample& Sample = State.Samples[Record.SampleId];
		Sample.PlateId = Record.AssignedPlateId;
		Sample.ContinentalFraction = FMath::Clamp(Record.ContinentalFraction, 0.0, 1.0);
		Sample.Elevation = Record.Elevation;
		Sample.HistoricalElevation = Record.HistoricalElevation;
		Sample.OceanicAge = Record.OceanicAge;
		Sample.RidgeDirection = RetangentAndNormalizeVectorField(Record.RidgeDirection, Sample.UnitPosition);
		Sample.FoldDirection = RetangentAndNormalizeVectorField(Record.FoldDirection, Sample.UnitPosition);
		Sample.bContinental = Sample.ContinentalFraction >= 0.5;
	}

	for (CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		Plate.SampleIds.Reset();
		Plate.TriangleIds.Reset();
		Plate.Vertices.Reset();
		Plate.LocalTriangles.Reset();
		Plate.ActiveBoundaryTriangles.Reset();
		Plate.ActiveBoundaryTriangleDistancesKm.Reset();
		Plate.GlobalSampleIdToLocalVertexId.Reset();
	}
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (State.Plates.IsValidIndex(Sample.PlateId))
		{
			State.Plates[Sample.PlateId].SampleIds.Add(Sample.Id);
		}
	}

	State.SampleRayCandidateTriangles.Reset();
	State.SampleRayCandidateTriangles.SetNum(State.Samples.Num());
	TMap<int32, int32> AssignedTriangleAuthorityCounts;
	for (int32 TriangleId = 0; TriangleId < State.Triangles.Num(); ++TriangleId)
	{
		CarrierLab::FSphereTriangle& Triangle = State.Triangles[TriangleId];
		FCarrierLabPhaseIIIE5TriangleRebuildRecord& TriangleRecord = OutAudit.TriangleRecords.AddDefaulted_GetRef();
		TriangleRecord.GlobalTriangleId = TriangleId;
		TriangleRecord.VertexA = Triangle.A;
		TriangleRecord.VertexB = Triangle.B;
		TriangleRecord.VertexC = Triangle.C;
		TriangleRecord.PlateA = State.Samples.IsValidIndex(Triangle.A) ? State.Samples[Triangle.A].PlateId : INDEX_NONE;
		TriangleRecord.PlateB = State.Samples.IsValidIndex(Triangle.B) ? State.Samples[Triangle.B].PlateId : INDEX_NONE;
		TriangleRecord.PlateC = State.Samples.IsValidIndex(Triangle.C) ? State.Samples[Triangle.C].PlateId : INDEX_NONE;
		TriangleRecord.bBoundary =
			TriangleRecord.PlateA != TriangleRecord.PlateB ||
			TriangleRecord.PlateB != TriangleRecord.PlateC;
		Triangle.bBoundary = TriangleRecord.bBoundary;
		TriangleRecord.AssignmentClass = ClassifyPhaseIIIE5TriangleAssignment(
			TriangleRecord.PlateA,
			TriangleRecord.PlateB,
			TriangleRecord.PlateC,
			TriangleRecord.AssignedPlateId);

		switch (TriangleRecord.AssignmentClass)
		{
		case ECarrierLabPhaseIIIE5TriangleAssignmentClass::AllVerticesSamePlate:
			++OutAudit.AllSameTriangleCount;
			break;
		case ECarrierLabPhaseIIIE5TriangleAssignmentClass::MajorityTwoOfThree:
			++OutAudit.MajorityTriangleCount;
			break;
		case ECarrierLabPhaseIIIE5TriangleAssignmentClass::TripleJunctionCentroidSplit:
			++OutAudit.TripleJunctionCentroidSplitCount;
			break;
		case ECarrierLabPhaseIIIE5TriangleAssignmentClass::UnresolvedTripleJunction:
			++OutAudit.UnresolvedTripleJunctionCount;
			break;
		default:
			++OutAudit.InvalidTriangleCount;
			break;
		}

		if (TriangleRecord.AssignmentClass == ECarrierLabPhaseIIIE5TriangleAssignmentClass::TripleJunctionCentroidSplit)
		{
			const int32 SourceVerts[3] = { Triangle.A, Triangle.B, Triangle.C };
			const int32 SourcePlates[3] = { TriangleRecord.PlateA, TriangleRecord.PlateB, TriangleRecord.PlateC };
			const FVector3d SourcePositions[3] =
			{
				State.Samples.IsValidIndex(Triangle.A) ? State.Samples[Triangle.A].UnitPosition : FVector3d::UnitX(),
				State.Samples.IsValidIndex(Triangle.B) ? State.Samples[Triangle.B].UnitPosition : FVector3d::UnitY(),
				State.Samples.IsValidIndex(Triangle.C) ? State.Samples[Triangle.C].UnitPosition : FVector3d::UnitZ()
			};
			const FVector3d EdgeMidpoints[3] =
			{
				NormalizeOrFallback(SourcePositions[0] + SourcePositions[1], SourcePositions[0]),
				NormalizeOrFallback(SourcePositions[1] + SourcePositions[2], SourcePositions[1]),
				NormalizeOrFallback(SourcePositions[2] + SourcePositions[0], SourcePositions[2])
			};
			const FVector3d Centroid = NormalizeOrFallback(
				SourcePositions[0] + SourcePositions[1] + SourcePositions[2],
				SourcePositions[0]);

			bool bSplitOk = true;
			int32 FirstLocalTriangleId = INDEX_NONE;
			int32 LocalTriangleCount = 0;
			int32 SyntheticVertexCount = 0;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 PlateId = SourcePlates[Corner];
				if (!State.Plates.IsValidIndex(PlateId) || !State.Samples.IsValidIndex(SourceVerts[Corner]))
				{
					bSplitOk = false;
					break;
				}

				CarrierLab::FCarrierPlate& Plate = State.Plates[PlateId];
				const int32 Next = (Corner + 1) % 3;
				const int32 Prev = (Corner + 2) % 3;
				const int32 CornerVertexId = FindOrAddPhaseIIIE5LocalVertex(Plate, State.Samples[SourceVerts[Corner]]);
				const int32 NextMidpointVertexId = AddPhaseIIIE5SyntheticLocalVertex(
					Plate,
					EdgeMidpoints[Corner],
					TArray<int32>{ SourceVerts[Corner], SourceVerts[Next] },
					VertexRecords,
					State);
				const int32 CentroidVertexId = AddPhaseIIIE5SyntheticLocalVertex(
					Plate,
					Centroid,
					TArray<int32>{ SourceVerts[0], SourceVerts[1], SourceVerts[2] },
					VertexRecords,
					State);
				const int32 PrevMidpointVertexId = AddPhaseIIIE5SyntheticLocalVertex(
					Plate,
					EdgeMidpoints[Prev],
					TArray<int32>{ SourceVerts[Prev], SourceVerts[Corner] },
					VertexRecords,
					State);
				SyntheticVertexCount += 3;

				const int32 LocalA = AddPhaseIIIE5LocalTriangle(
					Plate,
					TriangleId,
					CornerVertexId,
					NextMidpointVertexId,
					CentroidVertexId,
					true);
				const int32 LocalB = AddPhaseIIIE5LocalTriangle(
					Plate,
					TriangleId,
					CornerVertexId,
					CentroidVertexId,
					PrevMidpointVertexId,
					true);
				if (FirstLocalTriangleId == INDEX_NONE)
				{
					FirstLocalTriangleId = LocalA;
					TriangleRecord.AssignedPlateId = PlateId;
				}
				LocalTriangleCount += 2;
				Plate.TriangleIds.AddUnique(TriangleId);
				if (State.SampleRayCandidateTriangles.IsValidIndex(SourceVerts[Corner]))
				{
					State.SampleRayCandidateTriangles[SourceVerts[Corner]].Add(
						CarrierLab::FCarrierRayTriangleRef{ Plate.PlateId, LocalA });
					State.SampleRayCandidateTriangles[SourceVerts[Corner]].Add(
						CarrierLab::FCarrierRayTriangleRef{ Plate.PlateId, LocalB });
				}
			}

			if (!bSplitOk)
			{
				++OutAudit.InvalidTriangleCount;
				Triangle.PlateId = INDEX_NONE;
				continue;
			}

			TriangleRecord.LocalTriangleId = FirstLocalTriangleId;
			TriangleRecord.LocalTriangleCount = LocalTriangleCount;
			Triangle.PlateId = INDEX_NONE;
			++OutAudit.AssignedTriangleCount;
			OutAudit.TripleJunctionCentroidSplitLocalTriangleCount += LocalTriangleCount;
			OutAudit.TripleJunctionCentroidSplitSyntheticVertexCount += SyntheticVertexCount;
			continue;
		}

		if (!State.Plates.IsValidIndex(TriangleRecord.AssignedPlateId))
		{
			Triangle.PlateId = INDEX_NONE;
			continue;
		}

		CarrierLab::FCarrierPlate& Plate = State.Plates[TriangleRecord.AssignedPlateId];
		TriangleRecord.LocalTriangleId = AddPhaseIIIE5LocalTriangle(
			Plate,
			TriangleId,
			FindOrAddPhaseIIIE5LocalVertex(Plate, State.Samples[Triangle.A]),
			FindOrAddPhaseIIIE5LocalVertex(Plate, State.Samples[Triangle.B]),
			FindOrAddPhaseIIIE5LocalVertex(Plate, State.Samples[Triangle.C]),
			TriangleRecord.bBoundary);
		TriangleRecord.LocalTriangleCount = 1;
		Triangle.PlateId = TriangleRecord.AssignedPlateId;
		Plate.TriangleIds.Add(TriangleId);
		AssignedTriangleAuthorityCounts.FindOrAdd(TriangleId)++;
		++OutAudit.AssignedTriangleCount;

		const int32 SourceVerts[3] = { Triangle.A, Triangle.B, Triangle.C };
		for (const int32 SourceVertexId : SourceVerts)
		{
			if (State.SampleRayCandidateTriangles.IsValidIndex(SourceVertexId))
			{
				State.SampleRayCandidateTriangles[SourceVertexId].Add(
					CarrierLab::FCarrierRayTriangleRef{ Plate.PlateId, TriangleRecord.LocalTriangleId });
			}
		}
	}

	OutAudit.bNoDuplicateTriangleAuthority = true;
	for (const TPair<int32, int32>& Pair : AssignedTriangleAuthorityCounts)
	{
		if (Pair.Value != 1)
		{
			OutAudit.bNoDuplicateTriangleAuthority = false;
			break;
		}
	}

	ResetPhaseIIIE5ProcessState(State, DistanceToFrontKmBySample, OutAudit.ResetAudit);
	OutAudit.ResetAudit.ResetSerialBefore = ResetBeforeRebuild.ResetSerialBefore;
	OutAudit.ResetAudit.ActiveTriangleCountBefore = ResetBeforeRebuild.ActiveTriangleCountBefore;
	OutAudit.ResetAudit.DistanceRecordCountBefore = ResetBeforeRebuild.DistanceRecordCountBefore;
	OutAudit.ResetAudit.MatrixPairCountBefore = ResetBeforeRebuild.MatrixPairCountBefore;
	OutAudit.ResetAudit.MatrixEvidenceCountBefore = ResetBeforeRebuild.MatrixEvidenceCountBefore;
	OutAudit.ResetAudit.SubductingMarkCountBefore = ResetBeforeRebuild.SubductingMarkCountBefore;
	OutAudit.ResetAudit.ObductionMarkCountBefore = ResetBeforeRebuild.ObductionMarkCountBefore;
	OutAudit.ResetAudit.CollisionPendingTriangleCountBefore = ResetBeforeRebuild.CollisionPendingTriangleCountBefore;
	OutAudit.ResetAudit.DistanceToFrontRecordCountBefore = ResetBeforeRebuild.DistanceToFrontRecordCountBefore;

	OutAudit.bPlateLocalTopologyCompact = true;
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		FCarrierLabPhaseIIIE5PlateRebuildRecord& PlateRecord = OutAudit.PlateRecords.AddDefaulted_GetRef();
		PlateRecord.PlateId = Plate.PlateId;
		PlateRecord.SampleCount = Plate.SampleIds.Num();
		PlateRecord.TriangleCount = Plate.LocalTriangles.Num();
		PlateRecord.VertexCount = Plate.Vertices.Num();
		PlateRecord.bLocalTriangleIndicesCompact = true;
		PlateRecord.bLocalVertexIndicesCompact = true;
		for (const CarrierLab::FCarrierPlateTriangle& Triangle : Plate.LocalTriangles)
		{
			if (!Plate.Vertices.IsValidIndex(Triangle.A) ||
				!Plate.Vertices.IsValidIndex(Triangle.B) ||
				!Plate.Vertices.IsValidIndex(Triangle.C) ||
				Triangle.A == Triangle.B ||
				Triangle.B == Triangle.C ||
				Triangle.C == Triangle.A)
			{
				PlateRecord.bLocalTriangleIndicesCompact = false;
				PlateRecord.bLocalVertexIndicesCompact = false;
			}
		}
		PlateRecord.TopologyHash = ComputePhaseIIIE5PlateTopologyHash(Plate);
		PlateRecord.bMotionPreserved = true;
		OutAudit.bPlateLocalTopologyCompact =
			OutAudit.bPlateLocalTopologyCompact &&
			PlateRecord.bLocalTriangleIndicesCompact &&
			PlateRecord.bLocalVertexIndicesCompact;
	}

	OutAudit.bQProvenancePreserved = OutAudit.GeneratedOceanicVertexCount == 0;
	OutAudit.bZGammaHoldPreserved = OutAudit.GeneratedOceanicVertexCount == 0;
	for (const FCarrierLabPhaseIIIE5RemeshVertexRecord& Record : VertexRecords)
	{
		if (!Record.bGeneratedOceanicCrust)
		{
			continue;
		}
		if (!State.Plates.IsValidIndex(Record.AssignedPlateId))
		{
			continue;
		}
		const CarrierLab::FCarrierPlate& Plate = State.Plates[Record.AssignedPlateId];
		const int32* LocalVertexId = Plate.GlobalSampleIdToLocalVertexId.Find(Record.SampleId);
		if (LocalVertexId == nullptr || !Plate.Vertices.IsValidIndex(*LocalVertexId))
		{
			continue;
		}
		const CarrierLab::FCarrierVertex& Vertex = Plate.Vertices[*LocalVertexId];
		const bool bFieldsPreserved =
			FMath::IsNearlyEqual(Vertex.Elevation, Record.Elevation, 1.0e-12) &&
			FMath::IsNearlyEqual(Vertex.OceanicAge, Record.OceanicAge, 1.0e-12) &&
			(Vertex.RidgeDirection - RetangentAndNormalizeVectorField(Record.RidgeDirection, Vertex.UnitPosition)).Size() <= 1.0e-9;
		const bool bQProvenancePresent =
			Record.OceanicRecord.bGeneratedOceanicCrust &&
			Record.OceanicRecord.Q1PlateId != INDEX_NONE &&
			Record.OceanicRecord.Q2PlateId != INDEX_NONE &&
			Record.OceanicRecord.QGammaUnitResidual <= 1.0e-8;
		const bool bRecordZGammaExtension =
			Record.bUsedZGammaDistanceProfilePlaceholder ||
			Record.bUsedZGammaGeophysicsDerivedProfile;
		const bool bOceanicZGammaExtension =
			Record.OceanicRecord.bUsedZGammaDistanceProfilePlaceholder ||
			Record.OceanicRecord.bUsedZGammaGeophysicsDerivedProfile;
		const bool bZGammaHoldPresent =
			bRecordZGammaExtension &&
			!Record.bPaperFaithfulZGammaProfile &&
			bOceanicZGammaExtension &&
			!Record.OceanicRecord.bPaperFaithfulZGammaProfile;
		if (bFieldsPreserved && bQProvenancePresent)
		{
			++OutAudit.PreservedGeneratedOceanicVertexCount;
		}
		OutAudit.bQProvenancePreserved = OutAudit.bQProvenancePreserved || (bFieldsPreserved && bQProvenancePresent);
		OutAudit.bZGammaHoldPreserved = OutAudit.bZGammaHoldPreserved || bZGammaHoldPresent;
	}

	OutAudit.MotionHashAfter = ComputeMotionStateHash(Motions);
	OutAudit.bMotionPreserved = OutAudit.MotionHashBefore == OutAudit.MotionHashAfter;
	for (FCarrierLabPhaseIIIE5PlateRebuildRecord& PlateRecord : OutAudit.PlateRecords)
	{
		PlateRecord.bMotionPreserved = OutAudit.bMotionPreserved;
	}
	OutAudit.bNoPriorOwnerFallback = OutAudit.PriorOwnerFallbackCount == 0;
	OutAudit.bNoProjectionOwnerFallback = OutAudit.ProjectionOwnerFallbackCount == 0;
	OutAudit.bNoPolicyWinner = OutAudit.PolicyWinnerCount == 0;
	OutAudit.bNoUnresolvedMultiHitRouted = OutAudit.UnresolvedMultiHitRoutedCount == 0;
	OutAudit.bTripleJunctionCentroidSplitApplied =
		OutAudit.TripleJunctionCentroidSplitCount == 0 ||
		(OutAudit.UnresolvedTripleJunctionCount == 0 &&
			OutAudit.TripleJunctionCentroidSplitLocalTriangleCount == OutAudit.TripleJunctionCentroidSplitCount * 6 &&
			OutAudit.TripleJunctionCentroidSplitSyntheticVertexCount == OutAudit.TripleJunctionCentroidSplitCount * 9);
	OutAudit.VertexRecords = MoveTemp(VertexRecords);
	OutAudit.bApplied =
		OutAudit.MissingVertexAssignmentCount == 0 &&
		OutAudit.InvalidAssignedPlateCount == 0 &&
		OutAudit.InvalidTriangleCount == 0 &&
		OutAudit.UnresolvedTripleJunctionCount == 0 &&
		OutAudit.bTripleJunctionCentroidSplitApplied &&
		OutAudit.bNoDuplicateTriangleAuthority &&
		OutAudit.bPlateLocalTopologyCompact &&
		OutAudit.ResetAudit.bResetSerialAdvanced &&
		OutAudit.ResetAudit.bProcessStateEmptyAfter &&
		OutAudit.bMotionPreserved &&
		OutAudit.bNoPriorOwnerFallback &&
		OutAudit.bNoProjectionOwnerFallback &&
		OutAudit.bNoPolicyWinner &&
		OutAudit.bNoUnresolvedMultiHitRouted;

	BuildPhaseIIIE5AuditHashes(OutAudit);
	bPlateRayMeshTopologyDirty = true;
	bProjectionRayMeshTopologyDirty = true;
	bRenderMeshTopologyDirty = true;
	++CurrentMetrics.EventCount;
	CurrentMetrics.LastRemeshMode = TEXT("phase_iii_e5_topology_rebuild_audit");
	CaptureDriftReference();
	return true;
}

bool ACarrierLabVisualizationActor::RunPhaseIIIE5CollisionPendingWireFixtureForTest(
	FCarrierLabPhaseIIIE5CollisionPendingWireAudit& OutAudit,
	FCarrierLabPhaseIIIE3RemeshSelectionAudit& OutSelectionAudit,
	const double InterpenetrationThresholdKm)
{
	OutAudit = FCarrierLabPhaseIIIE5CollisionPendingWireAudit();
	OutSelectionAudit = FCarrierLabPhaseIIIE3RemeshSelectionAudit();
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}

	FCarrierLabPhaseIIID1TerraneAudit TerraneAudit;
	if (!DetectPhaseIIID1ConnectedTerranes(TerraneAudit))
	{
		return false;
	}
	FCarrierLabPhaseIIID2CollisionGroupingAudit GroupingAudit;
	if (!BuildPhaseIIID2CollisionGroupsFromTerranes(TerraneAudit, GroupingAudit, InterpenetrationThresholdKm))
	{
		return false;
	}

	State.ConvergenceCollisionPendingTriangleKeys.Reset();
	TArray<int32> SampleIds;
	TArray<FCarrierLabPhaseIIID2CollisionGroupRecord> Groups = GroupingAudit.Groups;
	const bool bHasAcceptedDetectedGroup = Groups.ContainsByPredicate([this](const FCarrierLabPhaseIIID2CollisionGroupRecord& Group)
	{
		return Group.bAccepted &&
			State.Plates.IsValidIndex(Group.MaxDistanceSourcePlateId) &&
			State.Plates[Group.MaxDistanceSourcePlateId].LocalTriangles.IsValidIndex(Group.MaxDistanceLocalTriangleId);
	});
	if (!bHasAcceptedDetectedGroup)
	{
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			if (!Plate.LocalTriangles.IsEmpty())
			{
				FCarrierLabPhaseIIID2CollisionGroupRecord& FixtureGroup = Groups.AddDefaulted_GetRef();
				FixtureGroup.GroupId = Groups.Num() - 1;
				FixtureGroup.PairKey = State.Plates.Num() >= 2 ? MakePlatePairKey(0, 1) : 0;
				FixtureGroup.PlateA = 0;
				FixtureGroup.PlateB = State.Plates.Num() >= 2 ? 1 : 0;
				FixtureGroup.CandidateRecordCount = 1;
				FixtureGroup.ValidDistanceCount = 1;
				FixtureGroup.MaxDistanceSourcePlateId = Plate.PlateId;
				FixtureGroup.MaxDistanceOtherPlateId = FixtureGroup.PlateB;
				FixtureGroup.MaxDistanceLocalTriangleId = 0;
				FixtureGroup.MaxInterpenetrationKm = InterpenetrationThresholdKm;
				FixtureGroup.ThresholdKm = InterpenetrationThresholdKm;
				FixtureGroup.bAccepted = true;
				FixtureGroup.GroupHash = TEXT("fixture-owned-accepted-iiid2-group");
				OutAudit.bUsedFixtureOwnedAcceptedGroup = true;
				break;
			}
		}
	}

	for (const FCarrierLabPhaseIIID2CollisionGroupRecord& Group : Groups)
	{
		if (!Group.bAccepted ||
			!State.Plates.IsValidIndex(Group.MaxDistanceSourcePlateId) ||
			!State.Plates[Group.MaxDistanceSourcePlateId].LocalTriangles.IsValidIndex(Group.MaxDistanceLocalTriangleId))
		{
			continue;
		}
		++OutAudit.AcceptedGroupCount;
		State.ConvergenceCollisionPendingTriangleKeys.Add(
			MakePlateTriangleKey(Group.MaxDistanceSourcePlateId, Group.MaxDistanceLocalTriangleId));
		const CarrierLab::FCarrierPlate& Plate = State.Plates[Group.MaxDistanceSourcePlateId];
		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[Group.MaxDistanceLocalTriangleId];
		const int32 LocalVerts[3] = { Triangle.A, Triangle.B, Triangle.C };
		for (const int32 LocalVertexId : LocalVerts)
		{
			if (Plate.Vertices.IsValidIndex(LocalVertexId))
			{
				SampleIds.AddUnique(Plate.Vertices[LocalVertexId].GlobalSampleId);
			}
		}
	}
	SampleIds.Sort();

	OutAudit.bRan = true;
	OutAudit.bSeededFromAcceptedCollisionGroups = !State.ConvergenceCollisionPendingTriangleKeys.IsEmpty();
	OutAudit.PendingTriangleKeyCount = State.ConvergenceCollisionPendingTriangleKeys.Num();
	OutAudit.GroupingHash = GroupingAudit.GroupingHash;
	if (!SampleIds.IsEmpty())
	{
		RunPhaseIIIE3FilteredRemeshSelectionAuditForSamples(SampleIds, OutSelectionAudit);
		OutAudit.FilteredCollisionPendingCount = OutSelectionAudit.FilteredCollisionPendingCount;
		OutAudit.FilteredSubductingCount = OutSelectionAudit.FilteredSubductingCount;
		OutAudit.FilteredObductionPendingCount = OutSelectionAudit.FilteredObductionPendingCount;
		OutAudit.SelectionHash = OutSelectionAudit.SelectionHash;
	}
	return true;
}

bool ACarrierLabVisualizationActor::RefreshPhaseIIIMetricsForTest()
{
	if (!bInitialized)
	{
		return false;
	}
	ProjectCurrentCarrier();
	return true;
}

bool ACarrierLabVisualizationActor::RunPhaseIIIE3FilteredRemeshSelectionAudit(FCarrierLabPhaseIIIE3RemeshSelectionAudit& OutAudit)
{
	TArray<int32> SampleIds;
	SampleIds.Reserve(State.Samples.Num());
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		SampleIds.Add(Sample.Id);
	}
	return RunPhaseIIIE3FilteredRemeshSelectionAuditForSamples(SampleIds, OutAudit);
}

bool ACarrierLabVisualizationActor::RunPhaseIIIE3FilteredRemeshSelectionAuditForSamples(
	const TArray<int32>& SampleIds,
	FCarrierLabPhaseIIIE3RemeshSelectionAudit& OutAudit)
{
	OutAudit = FCarrierLabPhaseIIIE3RemeshSelectionAudit();
	if (!bInitialized)
	{
		return false;
	}

	FString MeshError;
	if (!RefreshPlateRayMeshes(MeshError))
	{
		return false;
	}

	TSet<uint64> SubductingTriangleKeys;
	for (const CarrierLab::FConvergenceSubductingTriangleMark& Mark : State.ConvergenceSubductingTriangleMarks)
	{
		SubductingTriangleKeys.Add(MakePlateTriangleKey(Mark.PlateId, Mark.LocalTriangleId));
	}

	TSet<uint64> ObductionTriangleKeys;
	for (const CarrierLab::FConvergenceObductionTriangleMark& Mark : State.ConvergenceObductionTriangleMarks)
	{
		ObductionTriangleKeys.Add(MakePlateTriangleKey(Mark.PlateId, Mark.LocalTriangleId));
	}

	for (const int32 SampleId : SampleIds)
	{
		if (!State.Samples.IsValidIndex(SampleId))
		{
			continue;
		}

		const CarrierLab::FSphereSample& Sample = State.Samples[SampleId];
		TArray<FCarrierLabVizCandidate> RawCandidates;
		uint64 RawPlateMask = 0;
		bool bAnyBoundary = false;
		QuerySampleCandidates(State, PlateRayMeshes, Sample, RawCandidates, RawPlateMask, bAnyBoundary);

		TArray<FCarrierLabIIIE3SelectionCandidate> SelectionCandidates;
		SelectionCandidates.Reserve(RawCandidates.Num());
		for (const FCarrierLabVizCandidate& RawCandidate : RawCandidates)
		{
			if (!State.Plates.IsValidIndex(RawCandidate.PlateId))
			{
				continue;
			}
			const CarrierLab::FCarrierPlate& Plate = State.Plates[RawCandidate.PlateId];
			if (!Plate.LocalTriangles.IsValidIndex(RawCandidate.LocalTriangleId))
			{
				continue;
			}

			FCarrierLabIIIE3SelectionCandidate& Candidate = SelectionCandidates.AddDefaulted_GetRef();
			Candidate.PlateId = RawCandidate.PlateId;
			Candidate.LocalTriangleId = RawCandidate.LocalTriangleId;
			Candidate.Bary = RawCandidate.Bary;
			Candidate.Distance = RawCandidate.Distance;
			Candidate.ContinentalFraction = InterpolateContinentalFraction(Plate, RawCandidate);
			InterpolateCrustFields(Plate, RawCandidate, Sample.UnitPosition, Candidate.Fields);
			Candidate.bBoundary = RawCandidate.bBoundary;

			const uint64 TriangleKey = MakePlateTriangleKey(Candidate.PlateId, Candidate.LocalTriangleId);
			if (SubductingTriangleKeys.Contains(TriangleKey))
			{
				Candidate.FilterReason = ECarrierLabPhaseIIIE3FilterReason::Subducting;
			}
			else if (ObductionTriangleKeys.Contains(TriangleKey))
			{
				Candidate.FilterReason = ECarrierLabPhaseIIIE3FilterReason::ObductionPending;
			}
			else if (State.ConvergenceCollisionPendingTriangleKeys.Contains(TriangleKey))
			{
				Candidate.FilterReason = ECarrierLabPhaseIIIE3FilterReason::CollisionPending;
			}
		}

		FCarrierLabPhaseIIIE3SelectionRecord Record;
		SelectPhaseIIIE3FilteredRemeshSource(
			Sample.Id,
			Sample.UnitPosition,
			SelectionCandidates,
			bEnablePhaseIIIE3DuplicateHitCoalescing,
			bEnablePhaseIIIE3SharedBoundaryTieBreak,
			bEnablePhaseIIIE3NearestHitTieBreak,
			bEnablePhaseIIIE3DistanceTieFallback,
			&State,
			Record);
		AccumulatePhaseIIIE3Record(Record, OutAudit);
	}

	OutAudit.bNearestHitTieBreakDisabled = !bEnablePhaseIIIE3NearestHitTieBreak;
	OutAudit.bDistanceTieFallbackDisabled = !bEnablePhaseIIIE3DistanceTieFallback;
	OutAudit.SelectionHash = ComputePhaseIIIE3SelectionHash(OutAudit.Records);
	OutAudit.bRan = true;
	return true;
}

bool ACarrierLabVisualizationActor::DiagnosePhaseIIIE62HoldSnapshotsForTest(
	const int32 SampleId,
	const ECarrierLabPhaseIIIE3SelectionClass SelectionClass,
	const ECarrierLabPhaseIIIE3MultiHitBucket MultiHitBucket,
	const TArray<FCarrierLabPhaseIIIE62CandidateSnapshot>& CandidateSnapshots,
	FCarrierLabPhaseIIIE62HoldRecord& OutRecord) const
{
	DiagnosePhaseIIIE62HoldFromSnapshots(SampleId, SelectionClass, MultiHitBucket, CandidateSnapshots, OutRecord);
	return true;
}

bool ACarrierLabVisualizationActor::RunPhaseIIIE62CrossPlateMultiHitDiagnosisAudit(
	FCarrierLabPhaseIIIE62CrossPlateMultiHitAudit& OutAudit)
{
	OutAudit = FCarrierLabPhaseIIIE62CrossPlateMultiHitAudit();
	if (!bInitialized)
	{
		return false;
	}

	FString MeshError;
	if (!RefreshPlateRayMeshes(MeshError))
	{
		return false;
	}

	TSet<uint64> SubductingTriangleKeys;
	for (const CarrierLab::FConvergenceSubductingTriangleMark& Mark : State.ConvergenceSubductingTriangleMarks)
	{
		SubductingTriangleKeys.Add(MakePlateTriangleKey(Mark.PlateId, Mark.LocalTriangleId));
	}

	TSet<uint64> ObductionTriangleKeys;
	for (const CarrierLab::FConvergenceObductionTriangleMark& Mark : State.ConvergenceObductionTriangleMarks)
	{
		ObductionTriangleKeys.Add(MakePlateTriangleKey(Mark.PlateId, Mark.LocalTriangleId));
	}

	FCarrierLabPhaseIIIE3RemeshSelectionAudit SelectionAudit;
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		TArray<FCarrierLabVizCandidate> RawCandidates;
		uint64 RawPlateMask = 0;
		bool bAnyBoundary = false;
		QuerySampleCandidates(State, PlateRayMeshes, Sample, RawCandidates, RawPlateMask, bAnyBoundary);

		TArray<FCarrierLabIIIE3SelectionCandidate> SelectionCandidates;
		SelectionCandidates.Reserve(RawCandidates.Num());
		for (const FCarrierLabVizCandidate& RawCandidate : RawCandidates)
		{
			if (!State.Plates.IsValidIndex(RawCandidate.PlateId))
			{
				continue;
			}
			const CarrierLab::FCarrierPlate& Plate = State.Plates[RawCandidate.PlateId];
			if (!Plate.LocalTriangles.IsValidIndex(RawCandidate.LocalTriangleId))
			{
				continue;
			}

			FCarrierLabIIIE3SelectionCandidate& Candidate = SelectionCandidates.AddDefaulted_GetRef();
			Candidate.PlateId = RawCandidate.PlateId;
			Candidate.LocalTriangleId = RawCandidate.LocalTriangleId;
			Candidate.Bary = RawCandidate.Bary;
			Candidate.Distance = RawCandidate.Distance;
			Candidate.ContinentalFraction = InterpolateContinentalFraction(Plate, RawCandidate);
			InterpolateCrustFields(Plate, RawCandidate, Sample.UnitPosition, Candidate.Fields);
			Candidate.bBoundary = RawCandidate.bBoundary;

			const uint64 TriangleKey = MakePlateTriangleKey(Candidate.PlateId, Candidate.LocalTriangleId);
			if (SubductingTriangleKeys.Contains(TriangleKey))
			{
				Candidate.FilterReason = ECarrierLabPhaseIIIE3FilterReason::Subducting;
			}
			else if (ObductionTriangleKeys.Contains(TriangleKey))
			{
				Candidate.FilterReason = ECarrierLabPhaseIIIE3FilterReason::ObductionPending;
			}
			else if (State.ConvergenceCollisionPendingTriangleKeys.Contains(TriangleKey))
			{
				Candidate.FilterReason = ECarrierLabPhaseIIIE3FilterReason::CollisionPending;
			}
		}

		FCarrierLabPhaseIIIE3SelectionRecord Record;
		SelectPhaseIIIE3FilteredRemeshSource(
			Sample.Id,
			Sample.UnitPosition,
			SelectionCandidates,
			bEnablePhaseIIIE3DuplicateHitCoalescing,
			false,
			false,
			false,
			&State,
			Record);
		AccumulatePhaseIIIE3Record(Record, SelectionAudit);

		if (!Record.bUnresolvedMultiHit ||
			(Record.MultiHitBucket != ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual &&
				Record.MultiHitBucket != ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate))
		{
			continue;
		}

		TArray<FCarrierLabIIIE3SelectionCandidate> VisibleCandidates;
		for (const FCarrierLabIIIE3SelectionCandidate& Candidate : SelectionCandidates)
		{
			if (Candidate.FilterReason == ECarrierLabPhaseIIIE3FilterReason::None)
			{
				VisibleCandidates.Add(Candidate);
			}
		}
		VisibleCandidates.Sort([](
			const FCarrierLabIIIE3SelectionCandidate& A,
			const FCarrierLabIIIE3SelectionCandidate& B)
		{
			if (A.PlateId != B.PlateId)
			{
				return A.PlateId < B.PlateId;
			}
			if (A.LocalTriangleId != B.LocalTriangleId)
			{
				return A.LocalTriangleId < B.LocalTriangleId;
			}
			return A.Distance < B.Distance;
		});

		TArray<FCarrierLabPhaseIIIE62CandidateSnapshot> CandidateSnapshots;
		CandidateSnapshots.Reserve(VisibleCandidates.Num());
		if (!VisibleCandidates.IsEmpty())
		{
			const FCarrierLabIIIE3SelectionCandidate& Reference = VisibleCandidates[0];
			for (int32 CandidateIndex = 0; CandidateIndex < VisibleCandidates.Num(); ++CandidateIndex)
			{
				FCarrierLabPhaseIIIE62CandidateSnapshot Snapshot;
				if (BuildPhaseIIIE62CandidateSnapshot(
					State,
					VisibleCandidates[CandidateIndex],
					Reference,
					CandidateIndex,
					Snapshot))
				{
					CandidateSnapshots.Add(Snapshot);
				}
			}
		}

		FCarrierLabPhaseIIIE62HoldRecord HoldRecord;
		DiagnosePhaseIIIE62HoldFromSnapshots(
			Record.SampleId,
			Record.SelectionClass,
			Record.MultiHitBucket,
			CandidateSnapshots,
			HoldRecord);
		AccumulatePhaseIIIE62HoldRecord(HoldRecord, OutAudit);
	}

	SelectionAudit.SelectionHash = ComputePhaseIIIE3SelectionHash(SelectionAudit.Records);
	OutAudit.SampleCount = SelectionAudit.SampleCount;
	OutAudit.SelectionUnresolvedMultiHitCount = SelectionAudit.UnresolvedMultiHitCount;
	OutAudit.SelectionCrossPlateEqualMultiHitCount = SelectionAudit.CrossPlateEqualMultiHitCount;
	OutAudit.SelectionThirdPlateMultiHitCount = SelectionAudit.ThirdPlateMultiHitCount;
	OutAudit.CoalescedMultiHitCount = SelectionAudit.CoalescedMultiHitCount;
	OutAudit.PriorOwnerFallbackCount = SelectionAudit.PriorOwnerFallbackCount;
	OutAudit.PolicyWinnerCount = SelectionAudit.PolicyWinnerCount;
	OutAudit.ProjectionAuthorityCount = 0;
	OutAudit.SelectionHash = SelectionAudit.SelectionHash;
	OutAudit.DiagnosisHash = ComputePhaseIIIE62DiagnosisHash(OutAudit.Records);
	OutAudit.bRan = true;
	return true;
}

bool ACarrierLabVisualizationActor::RunPhaseIIIE64PostMotionMultiHitDiagnosisAudit(
	FCarrierLabPhaseIIIE64PostMotionMultiHitAudit& OutAudit)
{
	OutAudit = FCarrierLabPhaseIIIE64PostMotionMultiHitAudit();
	if (!bInitialized)
	{
		return false;
	}

	FString MeshError;
	if (!RefreshPlateRayMeshes(MeshError))
	{
		return false;
	}

	TSet<uint64> SubductingTriangleKeys;
	for (const CarrierLab::FConvergenceSubductingTriangleMark& Mark : State.ConvergenceSubductingTriangleMarks)
	{
		SubductingTriangleKeys.Add(MakePlateTriangleKey(Mark.PlateId, Mark.LocalTriangleId));
	}

	TSet<uint64> ObductionTriangleKeys;
	for (const CarrierLab::FConvergenceObductionTriangleMark& Mark : State.ConvergenceObductionTriangleMarks)
	{
		ObductionTriangleKeys.Add(MakePlateTriangleKey(Mark.PlateId, Mark.LocalTriangleId));
	}

	FCarrierLabPhaseIIIE3RemeshSelectionAudit SelectionAudit;
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		TArray<FCarrierLabVizCandidate> RawCandidates;
		uint64 RawPlateMask = 0;
		bool bAnyBoundary = false;
		QuerySampleCandidates(State, PlateRayMeshes, Sample, RawCandidates, RawPlateMask, bAnyBoundary);

		TArray<FCarrierLabIIIE3SelectionCandidate> SelectionCandidates;
		SelectionCandidates.Reserve(RawCandidates.Num());
		for (const FCarrierLabVizCandidate& RawCandidate : RawCandidates)
		{
			if (!State.Plates.IsValidIndex(RawCandidate.PlateId))
			{
				continue;
			}
			const CarrierLab::FCarrierPlate& Plate = State.Plates[RawCandidate.PlateId];
			if (!Plate.LocalTriangles.IsValidIndex(RawCandidate.LocalTriangleId))
			{
				continue;
			}

			FCarrierLabIIIE3SelectionCandidate& Candidate = SelectionCandidates.AddDefaulted_GetRef();
			Candidate.PlateId = RawCandidate.PlateId;
			Candidate.LocalTriangleId = RawCandidate.LocalTriangleId;
			Candidate.Bary = RawCandidate.Bary;
			Candidate.Distance = RawCandidate.Distance;
			Candidate.ContinentalFraction = InterpolateContinentalFraction(Plate, RawCandidate);
			InterpolateCrustFields(Plate, RawCandidate, Sample.UnitPosition, Candidate.Fields);
			Candidate.bBoundary = RawCandidate.bBoundary;

			const uint64 TriangleKey = MakePlateTriangleKey(Candidate.PlateId, Candidate.LocalTriangleId);
			if (SubductingTriangleKeys.Contains(TriangleKey))
			{
				Candidate.FilterReason = ECarrierLabPhaseIIIE3FilterReason::Subducting;
			}
			else if (ObductionTriangleKeys.Contains(TriangleKey))
			{
				Candidate.FilterReason = ECarrierLabPhaseIIIE3FilterReason::ObductionPending;
			}
			else if (State.ConvergenceCollisionPendingTriangleKeys.Contains(TriangleKey))
			{
				Candidate.FilterReason = ECarrierLabPhaseIIIE3FilterReason::CollisionPending;
			}
		}

		FCarrierLabPhaseIIIE3SelectionRecord Record;
		SelectPhaseIIIE3FilteredRemeshSource(
			Sample.Id,
			Sample.UnitPosition,
			SelectionCandidates,
			bEnablePhaseIIIE3DuplicateHitCoalescing,
			bEnablePhaseIIIE3SharedBoundaryTieBreak,
			bEnablePhaseIIIE3NearestHitTieBreak,
			bEnablePhaseIIIE3DistanceTieFallback,
			&State,
			Record);
		AccumulatePhaseIIIE3Record(Record, SelectionAudit);
		if (!Record.bUnresolvedMultiHit)
		{
			continue;
		}

		TArray<FCarrierLabIIIE3SelectionCandidate> VisibleCandidates;
		for (const FCarrierLabIIIE3SelectionCandidate& Candidate : SelectionCandidates)
		{
			if (Candidate.FilterReason == ECarrierLabPhaseIIIE3FilterReason::None)
			{
				VisibleCandidates.Add(Candidate);
			}
		}
		VisibleCandidates.Sort([](
			const FCarrierLabIIIE3SelectionCandidate& A,
			const FCarrierLabIIIE3SelectionCandidate& B)
		{
			if (!FMath::IsNearlyEqual(A.Distance, B.Distance, PhaseIIIE3RayDistanceCoincidenceToleranceKm))
			{
				return A.Distance < B.Distance;
			}
			if (A.PlateId != B.PlateId)
			{
				return A.PlateId < B.PlateId;
			}
			return A.LocalTriangleId < B.LocalTriangleId;
		});
		if (VisibleCandidates.IsEmpty())
		{
			continue;
		}

		FCarrierLabPhaseIIIE64HoldRecord HoldRecord;
		HoldRecord.SampleId = Record.SampleId;
		HoldRecord.Step = CurrentMetrics.Step;
		HoldRecord.SampleUnitPosition = Record.SampleUnitPosition;
		HoldRecord.SelectionClass = Record.SelectionClass;
		HoldRecord.MultiHitBucket = Record.MultiHitBucket;
		HoldRecord.CandidateCount = VisibleCandidates.Num();

		TSet<int32> PlateIds;
		for (const FCarrierLabIIIE3SelectionCandidate& Candidate : VisibleCandidates)
		{
			PlateIds.Add(Candidate.PlateId);
		}
		HoldRecord.DistinctPlateCount = PlateIds.Num();

		const int32 NearestCandidateIndex = 0;
		double NearestDistance = VisibleCandidates[0].Distance;
		double SecondDistance = TNumericLimits<double>::Max();
		for (int32 CandidateIndex = 1; CandidateIndex < VisibleCandidates.Num(); ++CandidateIndex)
		{
			SecondDistance = FMath::Min(SecondDistance, VisibleCandidates[CandidateIndex].Distance);
		}
		HoldRecord.NearestDistanceGapKm = SecondDistance < TNumericLimits<double>::Max()
			? FMath::Max(0.0, SecondDistance - NearestDistance)
			: 0.0;
		HoldRecord.bHasUniqueNearest = VisibleCandidates.Num() == 1 ||
			HoldRecord.NearestDistanceGapKm > PhaseIIIE3RayDistanceCoincidenceToleranceKm;
		HoldRecord.bNearestDistanceTie = !HoldRecord.bHasUniqueNearest;

		double BestContinental = -TNumericLimits<double>::Max();
		double SecondContinental = -TNumericLimits<double>::Max();
		double BestOceanicAge = -TNumericLimits<double>::Max();
		double SecondOceanicAge = -TNumericLimits<double>::Max();
		int32 LowestPlateId = INDEX_NONE;
		TMap<int32, double> ContinentalByPlate;
		TMap<int32, double> OceanicAgeByPlate;
		for (const int32 PlateId : PlateIds)
		{
			const double Continental = ComputePlateContinentalFraction(State, PlateId);
			const double OceanicAge = ComputePlateOceanicAge(State, PlateId);
			ContinentalByPlate.Add(PlateId, Continental);
			OceanicAgeByPlate.Add(PlateId, OceanicAge);
			if (Continental > BestContinental)
			{
				SecondContinental = BestContinental;
				BestContinental = Continental;
			}
			else if (Continental > SecondContinental)
			{
				SecondContinental = Continental;
			}
			if (OceanicAge > BestOceanicAge)
			{
				SecondOceanicAge = BestOceanicAge;
				BestOceanicAge = OceanicAge;
			}
			else if (OceanicAge > SecondOceanicAge)
			{
				SecondOceanicAge = OceanicAge;
			}
			if (LowestPlateId == INDEX_NONE || PlateId < LowestPlateId)
			{
				LowestPlateId = PlateId;
			}
		}
		HoldRecord.bContinentalPlateTie = BestContinental - SecondContinental <= PhaseIIIE3ScalarFieldTolerance;
		HoldRecord.bOceanicAgePlateTie = BestOceanicAge - SecondOceanicAge <= PhaseIIIE63OceanicAgeTieToleranceMa;

		const int32 NearestPlateId = VisibleCandidates[NearestCandidateIndex].PlateId;
		const double* NearestContinental = ContinentalByPlate.Find(NearestPlateId);
		const double* NearestOceanicAge = OceanicAgeByPlate.Find(NearestPlateId);
		HoldRecord.bNearestIsMostContinentalPlate =
			HoldRecord.bHasUniqueNearest &&
			NearestContinental != nullptr &&
			BestContinental - *NearestContinental <= PhaseIIIE3ScalarFieldTolerance &&
			!HoldRecord.bContinentalPlateTie;
		HoldRecord.bNearestIsOlderOceanicPlate =
			HoldRecord.bHasUniqueNearest &&
			NearestOceanicAge != nullptr &&
			BestOceanicAge - *NearestOceanicAge <= PhaseIIIE63OceanicAgeTieToleranceMa &&
			!HoldRecord.bOceanicAgePlateTie;
		HoldRecord.bNearestIsLowerPlateId =
			HoldRecord.bHasUniqueNearest &&
			NearestPlateId == LowestPlateId;

		const FCarrierLabIIIE3SelectionCandidate& Reference = VisibleCandidates[0];
		HoldRecord.Candidates.Reserve(VisibleCandidates.Num());
		for (int32 CandidateIndex = 0; CandidateIndex < VisibleCandidates.Num(); ++CandidateIndex)
		{
			const FCarrierLabIIIE3SelectionCandidate& Candidate = VisibleCandidates[CandidateIndex];
			FCarrierLabPhaseIIIE62CandidateSnapshot Snapshot;
			if (!BuildPhaseIIIE62CandidateSnapshot(State, Candidate, Reference, CandidateIndex, Snapshot))
			{
				continue;
			}

			FCarrierLabPhaseIIIE64CandidateDiagnostic& Diagnostic = HoldRecord.Candidates.AddDefaulted_GetRef();
			Diagnostic.Snapshot = Snapshot;
			Diagnostic.bNearestCandidate = CandidateIndex == NearestCandidateIndex;
			const uint64 TriangleKey = MakePlateTriangleKey(Candidate.PlateId, Candidate.LocalTriangleId);
			Diagnostic.bSubductingMarked = SubductingTriangleKeys.Contains(TriangleKey);
			Diagnostic.bObductionPendingMarked = ObductionTriangleKeys.Contains(TriangleKey);
			Diagnostic.bCollisionPendingMarked = State.ConvergenceCollisionPendingTriangleKeys.Contains(TriangleKey);
			Diagnostic.PlateContinentalFraction = ContinentalByPlate.Contains(Candidate.PlateId)
				? ContinentalByPlate[Candidate.PlateId]
				: 0.0;
			Diagnostic.PlateOceanicAge = OceanicAgeByPlate.Contains(Candidate.PlateId)
				? OceanicAgeByPlate[Candidate.PlateId]
				: 0.0;

			HoldRecord.bAnySubductingMarked = HoldRecord.bAnySubductingMarked || Diagnostic.bSubductingMarked;
			HoldRecord.bAnyObductionPendingMarked = HoldRecord.bAnyObductionPendingMarked || Diagnostic.bObductionPendingMarked;
			HoldRecord.bAnyCollisionPendingMarked = HoldRecord.bAnyCollisionPendingMarked || Diagnostic.bCollisionPendingMarked;
		}
		HoldRecord.bAnyCandidateProcessMarked =
			HoldRecord.bAnySubductingMarked ||
			HoldRecord.bAnyObductionPendingMarked ||
			HoldRecord.bAnyCollisionPendingMarked;
		AccumulatePhaseIIIE64HoldRecord(HoldRecord, OutAudit);
	}

	SelectionAudit.SelectionHash = ComputePhaseIIIE3SelectionHash(SelectionAudit.Records);
	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.SampleCount = SelectionAudit.SampleCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.SelectionUnresolvedMultiHitCount = SelectionAudit.UnresolvedMultiHitCount;
	OutAudit.SelectionCrossPlateDifferentMultiHitCount = SelectionAudit.CrossPlateDifferentMultiHitCount;
	OutAudit.SelectionThirdPlateMultiHitCount = SelectionAudit.ThirdPlateMultiHitCount;
	OutAudit.SelectionCrossPlateEqualMultiHitCount = SelectionAudit.CrossPlateEqualMultiHitCount;
	OutAudit.CoalescedMultiHitCount = SelectionAudit.CoalescedMultiHitCount;
	OutAudit.SharedBoundaryTieBreakCount = SelectionAudit.SharedBoundaryTieBreakCount;
	OutAudit.PriorOwnerFallbackCount = SelectionAudit.PriorOwnerFallbackCount;
	OutAudit.PolicyWinnerCount = SelectionAudit.PolicyWinnerCount;
	OutAudit.ProjectionAuthorityCount = 0;
	OutAudit.SelectionHash = SelectionAudit.SelectionHash;
	FinalizePhaseIIIE64DistanceStats(OutAudit);
	OutAudit.DiagnosisHash = ComputePhaseIIIE64DiagnosisHash(OutAudit.Records);
	OutAudit.bRan = true;
	return true;
}

bool ACarrierLabVisualizationActor::RunPhaseIIIE67ApplyPathInvalidRecordsDiagnosisAudit(
	FCarrierLabPhaseIIIE67ApplyPathInvalidRecordsAudit& OutAudit)
{
	OutAudit = FCarrierLabPhaseIIIE67ApplyPathInvalidRecordsAudit();
	if (!bInitialized)
	{
		return false;
	}

	const double TotalStartSeconds = FPlatformTime::Seconds();
	const double SelectionStartSeconds = FPlatformTime::Seconds();
	FCarrierLabPhaseIIIE3RemeshSelectionAudit SelectionAudit;
	if (!RunPhaseIIIE3FilteredRemeshSelectionAudit(SelectionAudit))
	{
		return false;
	}
	OutAudit.SelectionSeconds = FPlatformTime::Seconds() - SelectionStartSeconds;

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.SampleCount = State.Samples.Num();
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.SelectionResolvedSingleHitCount = SelectionAudit.PostFilterSingleHitCount;
	OutAudit.SelectionDivergentGapRouteCount = SelectionAudit.DivergentGapRouteCount;
	OutAudit.SelectionUnresolvedMultiHitCount = SelectionAudit.UnresolvedMultiHitCount;
	OutAudit.SelectionHash = SelectionAudit.SelectionHash;
	OutAudit.SpatialInvalidCounts.Init(0, 12 * 6);
	TArray<int32> SpatialGeneratedNonPositiveCounts;
	SpatialGeneratedNonPositiveCounts.Init(0, 12 * 6);
	TArray<double> NonPositiveSeparationMagnitudes;

	TArray<FCarrierLabPhaseIIIE5RemeshVertexRecord> VertexRecords;
	VertexRecords.SetNum(State.Samples.Num());

	auto IsFiniteVector = [](const FVector3d& Value)
	{
		return FMath::IsFinite(Value.X) && FMath::IsFinite(Value.Y) && FMath::IsFinite(Value.Z);
	};

	auto IsSampleUnitValid = [&IsFiniteVector](const FVector3d& UnitPosition)
	{
		if (!IsFiniteVector(UnitPosition))
		{
			return false;
		}
		return FMath::Abs(UnitPosition.SizeSquared() - 1.0) <= 1.0e-6;
	};

	auto AreSampleFieldsValid = [&IsFiniteVector](const CarrierLab::FSphereSample& Sample)
	{
		return
			FMath::IsFinite(Sample.ContinentalFraction) &&
			Sample.ContinentalFraction >= -1.0e-9 &&
			Sample.ContinentalFraction <= 1.0 + 1.0e-9 &&
			FMath::IsFinite(Sample.Elevation) &&
			FMath::IsFinite(Sample.HistoricalElevation) &&
			FMath::IsFinite(Sample.OceanicAge) &&
			Sample.OceanicAge >= -1.0e-9 &&
			IsFiniteVector(Sample.RidgeDirection) &&
			IsFiniteVector(Sample.FoldDirection);
	};

	auto AccumulateReason = [&OutAudit](const ECarrierLabPhaseIIIE67InvalidRecordReason Reason)
	{
		switch (Reason)
		{
		case ECarrierLabPhaseIIIE67InvalidRecordReason::InvalidSampleIndex:
			++OutAudit.InvalidSampleIndexCount;
			break;
		case ECarrierLabPhaseIIIE67InvalidRecordReason::InvalidSampleUnitPosition:
			++OutAudit.InvalidSampleUnitPositionCount;
			break;
		case ECarrierLabPhaseIIIE67InvalidRecordReason::SampleFieldOutOfRange:
			++OutAudit.SampleFieldOutOfRangeCount;
			break;
		case ECarrierLabPhaseIIIE67InvalidRecordReason::ResolvedHitInvalidPlate:
			++OutAudit.ResolvedHitInvalidPlateCount;
			break;
		case ECarrierLabPhaseIIIE67InvalidRecordReason::DivergentGapNoBoundaryPair:
			++OutAudit.DivergentGapNoBoundaryPairCount;
			break;
		case ECarrierLabPhaseIIIE67InvalidRecordReason::DivergentGapNonSeparating:
			++OutAudit.DivergentGapNonSeparatingCount;
			break;
		case ECarrierLabPhaseIIIE67InvalidRecordReason::DivergentGapGenerationOtherFailure:
			++OutAudit.DivergentGapGenerationOtherFailureCount;
			break;
		case ECarrierLabPhaseIIIE67InvalidRecordReason::GeneratedGapInvalidAssignedPlate:
			++OutAudit.GeneratedGapInvalidAssignedPlateCount;
			break;
		case ECarrierLabPhaseIIIE67InvalidRecordReason::UnhandledSelectionClass:
			++OutAudit.UnhandledSelectionClassCount;
			break;
		case ECarrierLabPhaseIIIE67InvalidRecordReason::None:
		default:
			break;
		}
	};

	auto AccumulateInvalid = [this, &OutAudit, &IsSampleUnitValid, &AreSampleFieldsValid, &AccumulateReason](
		const FCarrierLabPhaseIIIE3SelectionRecord& SelectionRecord,
		const ECarrierLabPhaseIIIE67InvalidRecordReason Reason,
		const FCarrierLabPhaseIIIE4OceanicGenerationRecord* OceanicRecord)
	{
		++OutAudit.InvalidRecordCount;
		AccumulateReason(Reason);

		switch (SelectionRecord.SelectionClass)
		{
		case ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap:
			++OutAudit.InvalidNoHitDivergentGapCount;
			break;
		case ECarrierLabPhaseIIIE3SelectionClass::DivergentGapAfterFiltering:
			++OutAudit.InvalidFilterExhaustedDivergentGapCount;
			break;
		case ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit:
			++OutAudit.InvalidResolvedSingleHitCount;
			break;
		default:
			++OutAudit.InvalidUnhandledSelectionCount;
			break;
		}

		if (SelectionRecord.FilteredSubductingCount > 0 ||
			SelectionRecord.FilteredObductionPendingCount > 0 ||
			SelectionRecord.FilteredCollisionPendingCount > 0)
		{
			++OutAudit.InvalidWithAnyProcessFilterCount;
		}
		if (SelectionRecord.FilteredSubductingCount > 0)
		{
			++OutAudit.InvalidWithSubductingFilterCount;
		}
		if (SelectionRecord.FilteredObductionPendingCount > 0)
		{
			++OutAudit.InvalidWithObductionFilterCount;
		}
		if (SelectionRecord.FilteredCollisionPendingCount > 0)
		{
			++OutAudit.InvalidWithCollisionFilterCount;
		}

		FCarrierLabPhaseIIIE67InvalidRecord& Record = OutAudit.Records.AddDefaulted_GetRef();
		Record.SampleId = SelectionRecord.SampleId;
		Record.Step = CurrentMetrics.Step;
		Record.Reason = Reason;
		Record.SelectionClass = SelectionRecord.SelectionClass;
		Record.MultiHitBucket = SelectionRecord.MultiHitBucket;
		Record.ResolvedPlateId = SelectionRecord.ResolvedPlateId;
		Record.OceanicAssignedPlateId = OceanicRecord != nullptr ? OceanicRecord->AssignedPlateId : INDEX_NONE;
		Record.FilteredSubductingCount = SelectionRecord.FilteredSubductingCount;
		Record.FilteredObductionPendingCount = SelectionRecord.FilteredObductionPendingCount;
		Record.FilteredCollisionPendingCount = SelectionRecord.FilteredCollisionPendingCount;
		Record.RawCandidateCount = SelectionRecord.RawCandidateCount;
		Record.PostFilterCandidateCount = SelectionRecord.PostFilterCandidateCount;
		if (OceanicRecord != nullptr)
		{
			Record.bBoundaryPairFound = OceanicRecord->bBoundaryPairFound;
			Record.bNonSeparatingAnomaly = OceanicRecord->bNonSeparatingAnomaly;
			Record.bGeneratedWithNonPositiveSeparation = OceanicRecord->bGeneratedWithNonPositiveSeparation;
			Record.bGeneratedOceanicCrust = OceanicRecord->bGeneratedOceanicCrust;
			Record.SignedSeparationVelocity = OceanicRecord->SignedSeparationVelocity;
		}

		if (!State.Samples.IsValidIndex(SelectionRecord.SampleId))
		{
			Record.bSampleUnitValid = false;
			Record.bSampleFieldsValid = false;
			return;
		}

		const CarrierLab::FSphereSample& Sample = State.Samples[SelectionRecord.SampleId];
		Record.bSampleUnitValid = IsSampleUnitValid(Sample.UnitPosition);
		Record.bSampleFieldsValid = AreSampleFieldsValid(Sample);
		if (!Record.bSampleUnitValid)
		{
			++OutAudit.InvalidSampleUnitPositionCount;
		}
		if (!Record.bSampleFieldsValid)
		{
			++OutAudit.SampleFieldOutOfRangeCount;
		}
		Record.ContinentalFraction = Sample.ContinentalFraction;
		Record.Elevation = Sample.Elevation;
		Record.HistoricalElevation = Sample.HistoricalElevation;
		Record.OceanicAge = Sample.OceanicAge;
		const FVector3d Unit = Sample.UnitPosition.GetSafeNormal();
		Record.LatitudeDegrees = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Unit.Z, -1.0, 1.0)));
		Record.LongitudeDegrees = FMath::RadiansToDegrees(FMath::Atan2(Unit.Y, Unit.X));
		Record.SpatialLonBin = FMath::Clamp(FMath::FloorToInt((Record.LongitudeDegrees + 180.0) / 30.0), 0, 11);
		Record.SpatialLatBin = FMath::Clamp(FMath::FloorToInt((Record.LatitudeDegrees + 90.0) / 30.0), 0, 5);
		const int32 SpatialIndex = Record.SpatialLatBin * 12 + Record.SpatialLonBin;
		if (OutAudit.SpatialInvalidCounts.IsValidIndex(SpatialIndex))
		{
			++OutAudit.SpatialInvalidCounts[SpatialIndex];
		}
	};

	const double RecordBuildStartSeconds = FPlatformTime::Seconds();
	int32 ProcessedRecordCount = 0;
	int32 DivergentRecordCount = 0;
	double LastProgressLogSeconds = RecordBuildStartSeconds;
	for (const FCarrierLabPhaseIIIE3SelectionRecord& SelectionRecord : SelectionAudit.Records)
	{
		++ProcessedRecordCount;
		const double ValidationStartSeconds = FPlatformTime::Seconds();
		if (!State.Samples.IsValidIndex(SelectionRecord.SampleId) || !VertexRecords.IsValidIndex(SelectionRecord.SampleId))
		{
			OutAudit.ValidationSeconds += FPlatformTime::Seconds() - ValidationStartSeconds;
			AccumulateInvalid(SelectionRecord, ECarrierLabPhaseIIIE67InvalidRecordReason::InvalidSampleIndex, nullptr);
			continue;
		}

		const CarrierLab::FSphereSample& Sample = State.Samples[SelectionRecord.SampleId];
		FCarrierLabPhaseIIIE5RemeshVertexRecord& VertexRecord = VertexRecords[SelectionRecord.SampleId];
		VertexRecord.SampleId = Sample.Id;
		VertexRecord.AssignedPlateId = Sample.PlateId;
		VertexRecord.PreRemeshContinentalFraction = Sample.ContinentalFraction;
		VertexRecord.PreRemeshElevation = Sample.Elevation;
		VertexRecord.ContinentalFraction = Sample.ContinentalFraction;
		VertexRecord.Elevation = Sample.Elevation;
		VertexRecord.HistoricalElevation = Sample.HistoricalElevation;
		VertexRecord.OceanicAge = Sample.OceanicAge;
		VertexRecord.RidgeDirection = Sample.RidgeDirection;
		VertexRecord.FoldDirection = Sample.FoldDirection;
		OutAudit.ValidationSeconds += FPlatformTime::Seconds() - ValidationStartSeconds;

		if (SelectionRecord.SelectionClass == ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit)
		{
			const double ResolvedStartSeconds = FPlatformTime::Seconds();
			if (!State.Plates.IsValidIndex(SelectionRecord.ResolvedPlateId))
			{
				OutAudit.ResolvedRecordSeconds += FPlatformTime::Seconds() - ResolvedStartSeconds;
				AccumulateInvalid(SelectionRecord, ECarrierLabPhaseIIIE67InvalidRecordReason::ResolvedHitInvalidPlate, nullptr);
				continue;
			}
			VertexRecord.AssignedPlateId = SelectionRecord.ResolvedPlateId;
			VertexRecord.bResolvedSingleHit = true;
			VertexRecord.ContinentalFraction = SelectionRecord.ContinentalFraction;
			VertexRecord.Elevation = SelectionRecord.Elevation;
			VertexRecord.HistoricalElevation = SelectionRecord.HistoricalElevation;
			VertexRecord.OceanicAge = SelectionRecord.OceanicAge;
			VertexRecord.RidgeDirection = SelectionRecord.RidgeDirection;
			VertexRecord.FoldDirection = SelectionRecord.FoldDirection;
			OutAudit.ResolvedRecordSeconds += FPlatformTime::Seconds() - ResolvedStartSeconds;
			continue;
		}

		if (IsPhaseIIIE4DivergentGapRoute(SelectionRecord.SelectionClass))
		{
			++DivergentRecordCount;
			FCarrierLabPhaseIIIE4OceanicGenerationRecord OceanicRecord;
			const double QueryStartSeconds = FPlatformTime::Seconds();
			QueryPhaseIIIE4DivergentOceanicFieldsFromCurrentStateForTest(
				Sample.UnitPosition,
				SelectionRecord.SelectionClass,
				OceanicRecord);
			OutAudit.DivergentQuerySeconds += FPlatformTime::Seconds() - QueryStartSeconds;
			const double NowSeconds = FPlatformTime::Seconds();
			if (NowSeconds - LastProgressLogSeconds >= 30.0)
			{
				UE_LOG(
					LogTemp,
					Display,
					TEXT("CarrierLab IIIE.6.7 record-builder progress: step=%d processed=%d/%d divergent=%d invalid=%d query_s=%.3f"),
					CurrentMetrics.Step,
					ProcessedRecordCount,
					SelectionAudit.Records.Num(),
					DivergentRecordCount,
					OutAudit.InvalidRecordCount,
					OutAudit.DivergentQuerySeconds);
				LastProgressLogSeconds = NowSeconds;
			}

			const double DivergentValidationStartSeconds = FPlatformTime::Seconds();
			if (!OceanicRecord.bGeneratedOceanicCrust)
			{
				OutAudit.NoBoundaryPairMissCount += OceanicRecord.bBoundaryPairFound ? 0 : 1;
				OutAudit.NonSeparatingGapFillCount += OceanicRecord.bNonSeparatingAnomaly ? 1 : 0;
				ECarrierLabPhaseIIIE67InvalidRecordReason Reason = ECarrierLabPhaseIIIE67InvalidRecordReason::DivergentGapGenerationOtherFailure;
				if (!OceanicRecord.bBoundaryPairFound)
				{
					Reason = ECarrierLabPhaseIIIE67InvalidRecordReason::DivergentGapNoBoundaryPair;
				}
				else if (OceanicRecord.bNonSeparatingAnomaly)
				{
					Reason = ECarrierLabPhaseIIIE67InvalidRecordReason::DivergentGapNonSeparating;
				}
				OutAudit.ValidationSeconds += FPlatformTime::Seconds() - DivergentValidationStartSeconds;
				AccumulateInvalid(SelectionRecord, Reason, &OceanicRecord);
				continue;
			}

			++OutAudit.GeneratedCandidateCount;
			if (OceanicRecord.bGeneratedWithNonPositiveSeparation)
			{
				++OutAudit.GeneratedWithNonPositiveSeparationCount;
				NonPositiveSeparationMagnitudes.Add(FMath::Abs(OceanicRecord.SignedSeparationVelocity));
				const FVector3d Unit = Sample.UnitPosition.GetSafeNormal();
				const double LatitudeDegrees = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Unit.Z, -1.0, 1.0)));
				const double LongitudeDegrees = FMath::RadiansToDegrees(FMath::Atan2(Unit.Y, Unit.X));
				const int32 SpatialLonBin = FMath::Clamp(FMath::FloorToInt((LongitudeDegrees + 180.0) / 30.0), 0, 11);
				const int32 SpatialLatBin = FMath::Clamp(FMath::FloorToInt((LatitudeDegrees + 90.0) / 30.0), 0, 5);
				const int32 SpatialIndex = SpatialLatBin * 12 + SpatialLonBin;
				if (SpatialGeneratedNonPositiveCounts.IsValidIndex(SpatialIndex))
				{
					++SpatialGeneratedNonPositiveCounts[SpatialIndex];
				}
			}
			VertexRecord.bResolvedSingleHit = false;
			VertexRecord.bDivergentGapRoute = true;
			VertexRecord.OceanicRecord = OceanicRecord;
			VertexRecord.OceanicRecord.SampleId = Sample.Id;
			VertexRecord.bUsedZGammaDistanceProfilePlaceholder = OceanicRecord.bUsedZGammaDistanceProfilePlaceholder;
			VertexRecord.bUsedZGammaGeophysicsDerivedProfile = OceanicRecord.bUsedZGammaGeophysicsDerivedProfile;
			VertexRecord.bPaperFaithfulZGammaProfile = OceanicRecord.bPaperFaithfulZGammaProfile;

			if (Sample.ContinentalFraction > PhaseIIIEContinentalOverwriteThreshold)
			{
				++OutAudit.RiftingPendingCount;
				if (SelectionRecord.FilteredSubductingCount > 0 ||
					SelectionRecord.FilteredObductionPendingCount > 0 ||
					SelectionRecord.FilteredCollisionPendingCount > 0)
				{
					++OutAudit.RiftingPendingWithAnyProcessFilterCount;
				}
				OutAudit.ValidationSeconds += FPlatformTime::Seconds() - DivergentValidationStartSeconds;
				continue;
			}

			if (!State.Plates.IsValidIndex(OceanicRecord.AssignedPlateId))
			{
				OutAudit.ValidationSeconds += FPlatformTime::Seconds() - DivergentValidationStartSeconds;
				AccumulateInvalid(SelectionRecord, ECarrierLabPhaseIIIE67InvalidRecordReason::GeneratedGapInvalidAssignedPlate, &OceanicRecord);
				continue;
			}
			++OutAudit.AppliedGeneratedCount;
			VertexRecord.bGeneratedOceanicCrust = true;
			VertexRecord.AssignedPlateId = OceanicRecord.AssignedPlateId;
			VertexRecord.ContinentalFraction = 0.0;
			VertexRecord.Elevation = OceanicRecord.Elevation;
			VertexRecord.HistoricalElevation = 0.0;
			VertexRecord.OceanicAge = OceanicRecord.OceanicAge;
			VertexRecord.RidgeDirection = OceanicRecord.RidgeDirection;
			VertexRecord.FoldDirection = FVector3d::ZeroVector;
			OutAudit.ValidationSeconds += FPlatformTime::Seconds() - DivergentValidationStartSeconds;
			continue;
		}

		VertexRecord.bUnresolvedMultiHit = SelectionRecord.bUnresolvedMultiHit;
		AccumulateInvalid(SelectionRecord, ECarrierLabPhaseIIIE67InvalidRecordReason::UnhandledSelectionClass, nullptr);
	}
	OutAudit.RecordBuildSeconds = FPlatformTime::Seconds() - RecordBuildStartSeconds;
	OutAudit.TotalSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
	if (NonPositiveSeparationMagnitudes.Num() > 0)
	{
		NonPositiveSeparationMagnitudes.Sort();
		OutAudit.NonPositiveSeparationMinMagnitude = NonPositiveSeparationMagnitudes[0];
		OutAudit.NonPositiveSeparationMaxMagnitude = NonPositiveSeparationMagnitudes.Last();
		const int32 MedianIndex = NonPositiveSeparationMagnitudes.Num() / 2;
		OutAudit.NonPositiveSeparationMedianMagnitude = (NonPositiveSeparationMagnitudes.Num() % 2) == 0
			? 0.5 * (NonPositiveSeparationMagnitudes[MedianIndex - 1] + NonPositiveSeparationMagnitudes[MedianIndex])
			: NonPositiveSeparationMagnitudes[MedianIndex];
	}

	uint64 Hash = 1469598103934665603ull;
	HashMixString(Hash, TEXT("CarrierLab-IIIE67-apply-invalid-records-v1"));
	HashMix(Hash, static_cast<uint64>(OutAudit.Step + 1));
	HashMix(Hash, static_cast<uint64>(OutAudit.SampleCount + 1));
	HashMix(Hash, static_cast<uint64>(OutAudit.InvalidRecordCount + 1));
	HashMix(Hash, static_cast<uint64>(OutAudit.DivergentGapNoBoundaryPairCount + 1));
	HashMix(Hash, static_cast<uint64>(OutAudit.DivergentGapNonSeparatingCount + 1));
	HashMix(Hash, static_cast<uint64>(OutAudit.GeneratedCandidateCount + 1));
	HashMix(Hash, static_cast<uint64>(OutAudit.AppliedGeneratedCount + 1));
	HashMix(Hash, static_cast<uint64>(OutAudit.RiftingPendingCount + 1));
	HashMix(Hash, static_cast<uint64>(OutAudit.InvalidWithAnyProcessFilterCount + 1));
	HashMix(Hash, static_cast<uint64>(OutAudit.GeneratedWithNonPositiveSeparationCount + 1));
	HashMixDouble(Hash, OutAudit.NonPositiveSeparationMinMagnitude);
	HashMixDouble(Hash, OutAudit.NonPositiveSeparationMedianMagnitude);
	HashMixDouble(Hash, OutAudit.NonPositiveSeparationMaxMagnitude);
	uint64 NonPositiveSpatialHash = 1469598103934665603ull;
	HashMixString(NonPositiveSpatialHash, TEXT("CarrierLab-IIIE67-nonpositive-spatial-v1"));
	for (const int32 Count : SpatialGeneratedNonPositiveCounts)
	{
		HashMix(NonPositiveSpatialHash, static_cast<uint64>(Count + 1));
	}
	OutAudit.NonPositiveSeparationSpatialHash = HashToString(NonPositiveSpatialHash);
	HashMixString(Hash, OutAudit.NonPositiveSeparationSpatialHash);
	for (const FCarrierLabPhaseIIIE67InvalidRecord& Record : OutAudit.Records)
	{
		HashMix(Hash, static_cast<uint64>(Record.SampleId + 2));
		HashMix(Hash, static_cast<uint64>(Record.Reason) + 1ull);
		HashMix(Hash, static_cast<uint64>(Record.SelectionClass) + 1ull);
		HashMix(Hash, static_cast<uint64>(Record.MultiHitBucket) + 1ull);
		HashMix(Hash, static_cast<uint64>(Record.FilteredSubductingCount + 1));
		HashMix(Hash, static_cast<uint64>(Record.FilteredObductionPendingCount + 1));
		HashMix(Hash, static_cast<uint64>(Record.FilteredCollisionPendingCount + 1));
		HashMix(Hash, static_cast<uint64>(Record.OceanicAssignedPlateId + 2));
		HashMix(Hash, static_cast<uint64>(Record.SpatialLonBin + 1));
		HashMix(Hash, static_cast<uint64>(Record.SpatialLatBin + 1));
	}
	OutAudit.DiagnosisHash = HashToString(Hash);
	OutAudit.bRan = true;
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

bool ACarrierLabVisualizationActor::GetPhaseIIICObductionUpliftAudit(FCarrierLabPhaseIIICObductionUpliftAudit& OutAudit) const
{
	OutAudit = LastPhaseIIICObductionUpliftAudit;
	if (!bInitialized)
	{
		return false;
	}

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCount = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	OutAudit.bEnabled = bEnablePhaseIIICObductionUpliftBridge;
	OutAudit.bUpliftEnabled = bEnablePhaseIIICOverridingPlateUplift;
	OutAudit.MarkCount = State.ConvergenceObductionTriangleMarks.Num();
	OutAudit.DuplicateMarkCount = State.ConvergenceObductionTriangleMarkDuplicateCount;
	OutAudit.InvalidMarkCount = State.ConvergenceObductionTriangleMarkInvalidCount;
	OutAudit.EffectRadiusKm = PhaseIIICSubductionEffectRadiusKm;
	OutAudit.UpliftRateMmPerYear = PhaseIIICSubductionUpliftMmPerYear;
	OutAudit.ReferenceVelocityMmPerYear = PhaseIIICReferenceVelocityMmPerYear;
	OutAudit.FoldInfluenceBeta = PhaseIIICFoldDirectionBeta;
	OutAudit.TrenchDepthKm = PhaseIIICTrenchDepthKm;
	OutAudit.ContinentalMaxElevationKm = PhaseIIICMaxContinentalElevationKm;
	OutAudit.VisibleElevationHash = CurrentMetrics.VisibleElevationHash;
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

bool ACarrierLabVisualizationActor::DetectPhaseIIID1ConnectedTerranes(FCarrierLabPhaseIIID1TerraneAudit& OutAudit) const
{
	++PhaseIIIDiagnosticCallCounts.DetectTerranes;
	OutAudit = FCarrierLabPhaseIIID1TerraneAudit();
	if (!bInitialized)
	{
		return false;
	}

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCount = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerial = State.ConvergenceTrackingResetSerial;

	double TimingStartSeconds = FPlatformTime::Seconds();
	TMap<uint64, CarrierLab::FConvergenceSubductionPolarityDecision> DecisionsByPair;
	for (const CarrierLab::FConvergenceSubductionPolarityDecision& Decision : State.ConvergenceSubductionPolarityDecisions)
	{
		DecisionsByPair.Add(Decision.PairKey, Decision);
	}
	PhaseIIIDiagnosticCallCounts.D1DecisionIndexSeconds += FPlatformTime::Seconds() - TimingStartSeconds;

	TArray<FPhaseIIID1TerraneComponent> ComponentCache;
	TMap<uint64, int32> ComponentIndexByTriangle;

	auto FindOrBuildComponent = [this, &ComponentCache, &ComponentIndexByTriangle](
		const CarrierLab::FCarrierPlate& Plate,
		const int32 SeedLocalTriangleId,
		FPhaseIIID1TerraneComponent const*& OutComponent) -> bool
	{
		OutComponent = nullptr;
		const uint64 SeedKey = MakePlateTriangleKey(Plate.PlateId, SeedLocalTriangleId);
		if (const int32* ExistingIndex = ComponentIndexByTriangle.Find(SeedKey))
		{
			if (ComponentCache.IsValidIndex(*ExistingIndex))
			{
				OutComponent = &ComponentCache[*ExistingIndex];
				++PhaseIIIDiagnosticCallCounts.D1ComponentCacheHitCount;
				return true;
			}
		}

		FPhaseIIID1TerraneComponent NewComponent;
		if (!BuildPhaseIIID1TerraneComponent(Plate, SeedLocalTriangleId, NewComponent, &PhaseIIIDiagnosticCallCounts))
		{
			return false;
		}
		++PhaseIIIDiagnosticCallCounts.D1ComponentBuildCount;

		const int32 NewIndex = ComponentCache.Add(MoveTemp(NewComponent));
		for (const int32 LocalTriangleId : ComponentCache[NewIndex].LocalTriangleIds)
		{
			if (ComponentCache[NewIndex].ContinentalTriangleSet.Contains(LocalTriangleId))
			{
				ComponentIndexByTriangle.Add(MakePlateTriangleKey(Plate.PlateId, LocalTriangleId), NewIndex);
			}
		}
		OutComponent = &ComponentCache[NewIndex];
		return true;
	};

	TimingStartSeconds = FPlatformTime::Seconds();
	TArray<CarrierLab::FConvergenceSubductionTriangleHit> SortedHits = State.ConvergenceSubductionTriangleHits;
	SortedHits.Sort([](
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
		if (A.LocalTriangleId != B.LocalTriangleId)
		{
			return A.LocalTriangleId < B.LocalTriangleId;
		}
		return A.EvidenceId < B.EvidenceId;
	});
	PhaseIIIDiagnosticCallCounts.D1HitSortSeconds += FPlatformTime::Seconds() - TimingStartSeconds;
	PhaseIIIDiagnosticCallCounts.D1SortedHitCount += SortedHits.Num();

	TimingStartSeconds = FPlatformTime::Seconds();
	uint64 AuditHash = 1469598103934665603ull;
	HashMix(AuditHash, static_cast<uint64>(OutAudit.Step + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.EventCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.PlateCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.ResetSerial + 1));
	HashMix(AuditHash, static_cast<uint64>(SortedHits.Num() + 1));
	PhaseIIIDiagnosticCallCounts.D1AuditHashSeconds += FPlatformTime::Seconds() - TimingStartSeconds;

	for (const CarrierLab::FConvergenceSubductionTriangleHit& Hit : SortedHits)
	{
		TimingStartSeconds = FPlatformTime::Seconds();
		HashMix(AuditHash, Hit.PairKey + 1ull);
		HashMix(AuditHash, static_cast<uint64>(Hit.PlateId + 1));
		HashMix(AuditHash, static_cast<uint64>(Hit.OtherPlateId + 1));
		HashMix(AuditHash, static_cast<uint64>(Hit.LocalTriangleId + 1));
		HashMix(AuditHash, static_cast<uint64>(Hit.EvidenceId + 1));
		HashMixDouble(AuditHash, Hit.SignedConvergenceVelocity);

		const CarrierLab::FConvergenceSubductionPolarityDecision* Decision = DecisionsByPair.Find(Hit.PairKey);
		if (Decision == nullptr ||
			Decision->DecisionClass != CarrierLab::EConvergenceSubductionPolarityClass::CollisionCandidate)
		{
			++OutAudit.NonCollisionDecisionHitCount;
			PhaseIIIDiagnosticCallCounts.D1HitClassificationSeconds += FPlatformTime::Seconds() - TimingStartSeconds;
			continue;
		}

		++OutAudit.CollisionCandidateHitCount;
		if (!State.Plates.IsValidIndex(Hit.PlateId))
		{
			++OutAudit.InvalidSeedCount;
			PhaseIIIDiagnosticCallCounts.D1HitClassificationSeconds += FPlatformTime::Seconds() - TimingStartSeconds;
			continue;
		}

		const CarrierLab::FCarrierPlate& Plate = State.Plates[Hit.PlateId];
		if (!Plate.LocalTriangles.IsValidIndex(Hit.LocalTriangleId))
		{
			++OutAudit.InvalidSeedCount;
			PhaseIIIDiagnosticCallCounts.D1HitClassificationSeconds += FPlatformTime::Seconds() - TimingStartSeconds;
			continue;
		}
		if (!IsPlateLocalTriangleContinental(Plate, Hit.LocalTriangleId))
		{
			++OutAudit.NonContinentalSeedCount;
			PhaseIIIDiagnosticCallCounts.D1HitClassificationSeconds += FPlatformTime::Seconds() - TimingStartSeconds;
			continue;
		}
		PhaseIIIDiagnosticCallCounts.D1HitClassificationSeconds += FPlatformTime::Seconds() - TimingStartSeconds;

		const FPhaseIIID1TerraneComponent* Component = nullptr;
		if (!FindOrBuildComponent(Plate, Hit.LocalTriangleId, Component) || Component == nullptr)
		{
			++OutAudit.EmptyTerraneCount;
			continue;
		}

		TimingStartSeconds = FPlatformTime::Seconds();
		FCarrierLabPhaseIIID1TerraneRecord& Record = OutAudit.Records.AddDefaulted_GetRef();
		Record.RecordId = OutAudit.Records.Num() - 1;
		Record.PairKey = Hit.PairKey;
		Record.SourcePlateId = Hit.PlateId;
		Record.OtherPlateId = Hit.OtherPlateId;
		Record.SeedLocalTriangleId = Hit.LocalTriangleId;
		Record.EvidenceId = Hit.EvidenceId;
		Record.SignedConvergenceVelocity = Hit.SignedConvergenceVelocity;
		Record.LocalTriangleIds = Component->LocalTriangleIds;
		Record.TriangleCount = Record.LocalTriangleIds.Num();
		Record.ContinentalTriangleCount = Component->ContinentalTriangleCount;
		Record.InnerSeaTriangleCount = Component->InnerSeaTriangleCount;

		TSet<int32> VertexIds;
		double WeightedContinentalFraction = 0.0;
		double TotalAreaWeight = 0.0;
		uint64 TerraneHash = 1469598103934665603ull;
		HashMix(TerraneHash, Hit.PairKey + 1ull);
		HashMix(TerraneHash, static_cast<uint64>(Hit.PlateId + 1));
		HashMix(TerraneHash, static_cast<uint64>(Hit.OtherPlateId + 1));
		HashMix(TerraneHash, static_cast<uint64>(Hit.LocalTriangleId + 1));
		HashMix(TerraneHash, static_cast<uint64>(Hit.EvidenceId + 1));
		HashMixDouble(TerraneHash, Hit.SignedConvergenceVelocity);
		HashMix(TerraneHash, static_cast<uint64>(Record.LocalTriangleIds.Num() + 1));

		for (const int32 LocalTriangleId : Record.LocalTriangleIds)
		{
			HashMix(TerraneHash, static_cast<uint64>(LocalTriangleId + 1));
			if (!Plate.LocalTriangles.IsValidIndex(LocalTriangleId))
			{
				continue;
			}

			const double TriangleAreaWeight = ComputePlateLocalTriangleAreaWeight(Plate, LocalTriangleId);
			const double TriangleContinentalFraction = ComputePlateLocalTriangleContinentalFraction(Plate, LocalTriangleId);
			WeightedContinentalFraction += TriangleAreaWeight * TriangleContinentalFraction;
			TotalAreaWeight += TriangleAreaWeight;

			const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
			VertexIds.Add(Triangle.A);
			VertexIds.Add(Triangle.B);
			VertexIds.Add(Triangle.C);
		}

		Record.VertexCount = VertexIds.Num();
		Record.AreaWeight = TotalAreaWeight;
		Record.MeanContinentalFraction = TotalAreaWeight > UE_DOUBLE_SMALL_NUMBER
			? WeightedContinentalFraction / TotalAreaWeight
			: 0.0;
		Record.TerraneHash = HashToString(TerraneHash);

		++OutAudit.TerraneRecordCount;
		OutAudit.TotalTerraneTriangleCount += Record.TriangleCount;
		OutAudit.TotalContinentalTriangleCount += Record.ContinentalTriangleCount;
		OutAudit.TotalInnerSeaTriangleCount += Record.InnerSeaTriangleCount;
		OutAudit.MaxTerraneTriangleCount = FMath::Max(OutAudit.MaxTerraneTriangleCount, Record.TriangleCount);

		HashMix(AuditHash, static_cast<uint64>(Record.RecordId + 1));
		HashMixString(AuditHash, Record.TerraneHash);
		HashMix(AuditHash, static_cast<uint64>(Record.TriangleCount + 1));
		HashMix(AuditHash, static_cast<uint64>(Record.ContinentalTriangleCount + 1));
		HashMix(AuditHash, static_cast<uint64>(Record.InnerSeaTriangleCount + 1));
		HashMix(AuditHash, static_cast<uint64>(Record.VertexCount + 1));
		HashMixDouble(AuditHash, Record.MeanContinentalFraction);
		HashMixDouble(AuditHash, Record.AreaWeight);
		PhaseIIIDiagnosticCallCounts.D1RecordConstructionSeconds += FPlatformTime::Seconds() - TimingStartSeconds;
	}

	TimingStartSeconds = FPlatformTime::Seconds();
	HashMix(AuditHash, static_cast<uint64>(OutAudit.CollisionCandidateHitCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.TerraneRecordCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.TotalTerraneTriangleCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.TotalContinentalTriangleCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.TotalInnerSeaTriangleCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.MaxTerraneTriangleCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.InvalidSeedCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.NonCollisionDecisionHitCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.NonContinentalSeedCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.EmptyTerraneCount + 1));
	OutAudit.TerraneDetectionHash = HashToString(AuditHash);
	PhaseIIIDiagnosticCallCounts.D1AuditHashSeconds += FPlatformTime::Seconds() - TimingStartSeconds;
	PhaseIIIDiagnosticCallCounts.D1CollisionCandidateHitCount += OutAudit.CollisionCandidateHitCount;
	PhaseIIIDiagnosticCallCounts.D1RecordCount += OutAudit.TerraneRecordCount;
	return true;
}

bool ACarrierLabVisualizationActor::DetectPhaseIIID2CollisionGroups(
	FCarrierLabPhaseIIID2CollisionGroupingAudit& OutAudit,
	const double InterpenetrationThresholdKm) const
{
	++PhaseIIIDiagnosticCallCounts.GroupCollisions;
	FCarrierLabPhaseIIID1TerraneAudit TerraneAudit;
	if (!DetectPhaseIIID1ConnectedTerranes(TerraneAudit))
	{
		return false;
	}
	return BuildPhaseIIID2CollisionGroupsFromTerranes(TerraneAudit, OutAudit, InterpenetrationThresholdKm);
}

bool ACarrierLabVisualizationActor::BuildPhaseIIID2CollisionGroupsFromTerranes(
	const FCarrierLabPhaseIIID1TerraneAudit& TerraneAudit,
	FCarrierLabPhaseIIID2CollisionGroupingAudit& OutAudit,
	const double InterpenetrationThresholdKm) const
{
	OutAudit = FCarrierLabPhaseIIID2CollisionGroupingAudit();
	OutAudit.Step = TerraneAudit.Step;
	OutAudit.EventCount = TerraneAudit.EventCount;
	OutAudit.PlateCount = TerraneAudit.PlateCount;
	OutAudit.ResetSerial = TerraneAudit.ResetSerial;
	OutAudit.ThresholdKm = InterpenetrationThresholdKm;
	OutAudit.TerraneRecordCount = TerraneAudit.TerraneRecordCount;
	OutAudit.SourceTerraneDetectionHash = TerraneAudit.TerraneDetectionHash;

	TMap<uint64, TArray<int32>> RecordIndicesByPair;
	for (int32 RecordIndex = 0; RecordIndex < TerraneAudit.Records.Num(); ++RecordIndex)
	{
		const FCarrierLabPhaseIIID1TerraneRecord& Record = TerraneAudit.Records[RecordIndex];
		if (Record.PairKey == 0)
		{
			continue;
		}
		RecordIndicesByPair.FindOrAdd(Record.PairKey).Add(RecordIndex);
	}

	TMap<uint64, double> ActiveDistanceByPlateTriangle;
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		for (int32 ActiveIndex = 0; ActiveIndex < Plate.ActiveBoundaryTriangles.Num(); ++ActiveIndex)
		{
			if (!Plate.ActiveBoundaryTriangleDistancesKm.IsValidIndex(ActiveIndex))
			{
				continue;
			}
			const int32 LocalTriangleId = Plate.ActiveBoundaryTriangles[ActiveIndex];
			const double DistanceKm = Plate.ActiveBoundaryTriangleDistancesKm[ActiveIndex];
			ActiveDistanceByPlateTriangle.Add(MakePlateTriangleKey(Plate.PlateId, LocalTriangleId), DistanceKm);
		}
	}

	TArray<uint64> PairKeys;
	RecordIndicesByPair.GetKeys(PairKeys);
	PairKeys.Sort();

	uint64 AuditHash = 1469598103934665603ull;
	HashMixString(AuditHash, TEXT("CarrierLab-IIID2-collision-groups-v1"));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.Step + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.EventCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.PlateCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.ResetSerial + 1));
	HashMixDouble(AuditHash, OutAudit.ThresholdKm);
	HashMixString(AuditHash, OutAudit.SourceTerraneDetectionHash);

	for (const uint64 PairKey : PairKeys)
	{
		TArray<int32>& RecordIndices = RecordIndicesByPair.FindChecked(PairKey);
		RecordIndices.Sort([&TerraneAudit](const int32 A, const int32 B)
		{
			const FCarrierLabPhaseIIID1TerraneRecord& RecordA = TerraneAudit.Records[A];
			const FCarrierLabPhaseIIID1TerraneRecord& RecordB = TerraneAudit.Records[B];
			if (RecordA.RecordId != RecordB.RecordId)
			{
				return RecordA.RecordId < RecordB.RecordId;
			}
			if (RecordA.SourcePlateId != RecordB.SourcePlateId)
			{
				return RecordA.SourcePlateId < RecordB.SourcePlateId;
			}
			if (RecordA.SeedLocalTriangleId != RecordB.SeedLocalTriangleId)
			{
				return RecordA.SeedLocalTriangleId < RecordB.SeedLocalTriangleId;
			}
			return RecordA.EvidenceId < RecordB.EvidenceId;
		});

		FCarrierLabPhaseIIID2CollisionGroupRecord Group;
		Group.GroupId = OutAudit.Groups.Num();
		Group.PairKey = PairKey;
		Group.ThresholdKm = OutAudit.ThresholdKm;
		Group.CandidateRecordCount = RecordIndices.Num();

		TArray<FString> TerraneHashes;
		double SumDistanceKm = 0.0;
		double SumSignedVelocity = 0.0;
		uint64 GroupHash = 1469598103934665603ull;
		HashMixString(GroupHash, TEXT("CarrierLab-IIID2-group-v1"));
		HashMix(GroupHash, PairKey + 1ull);
		HashMixDouble(GroupHash, Group.ThresholdKm);
		HashMix(GroupHash, static_cast<uint64>(Group.CandidateRecordCount + 1));

		for (const int32 RecordIndex : RecordIndices)
		{
			const FCarrierLabPhaseIIID1TerraneRecord& Record = TerraneAudit.Records[RecordIndex];
			if (Group.PlateA == INDEX_NONE || Record.SourcePlateId < Group.PlateA)
			{
				Group.PlateA = Record.SourcePlateId;
			}
			if (Group.PlateA == Record.SourcePlateId)
			{
				Group.PlateB = Record.OtherPlateId;
			}
			if (Record.OtherPlateId != INDEX_NONE && (Group.PlateA == INDEX_NONE || Record.OtherPlateId < Group.PlateA))
			{
				Group.PlateB = Group.PlateA;
				Group.PlateA = Record.OtherPlateId;
			}
			else if (Record.OtherPlateId != INDEX_NONE && (Group.PlateB == INDEX_NONE || Record.OtherPlateId > Group.PlateB))
			{
				Group.PlateB = Record.OtherPlateId;
			}

			TerraneHashes.AddUnique(Record.TerraneHash);
			SumSignedVelocity += Record.SignedConvergenceVelocity;
			Group.MaxSignedConvergenceVelocity = Group.ValidDistanceCount == 0
				? Record.SignedConvergenceVelocity
				: FMath::Max(Group.MaxSignedConvergenceVelocity, Record.SignedConvergenceVelocity);
			Group.TotalAreaWeight += Record.AreaWeight;

			int32 RecordValidDistanceCount = 0;
			for (const int32 LocalTriangleId : Record.LocalTriangleIds)
			{
				const double* DistanceKmPtr = ActiveDistanceByPlateTriangle.Find(
					MakePlateTriangleKey(Record.SourcePlateId, LocalTriangleId));
				if (DistanceKmPtr == nullptr || !FMath::IsFinite(*DistanceKmPtr) || *DistanceKmPtr < 0.0)
				{
					continue;
				}

				++RecordValidDistanceCount;
				++Group.ValidDistanceCount;
				SumDistanceKm += *DistanceKmPtr;
				if (*DistanceKmPtr > Group.MaxInterpenetrationKm)
				{
					Group.MaxInterpenetrationKm = *DistanceKmPtr;
					Group.MaxDistanceRecordId = Record.RecordId;
					Group.MaxDistanceEvidenceId = Record.EvidenceId;
					Group.MaxDistanceSourcePlateId = Record.SourcePlateId;
					Group.MaxDistanceOtherPlateId = Record.OtherPlateId;
					Group.MaxDistanceLocalTriangleId = LocalTriangleId;
				}
			}
			if (RecordValidDistanceCount == 0)
			{
				++Group.InvalidDistanceCount;
			}

			HashMix(GroupHash, static_cast<uint64>(Record.RecordId + 1));
			HashMix(GroupHash, static_cast<uint64>(Record.SourcePlateId + 1));
			HashMix(GroupHash, static_cast<uint64>(Record.OtherPlateId + 1));
			HashMix(GroupHash, static_cast<uint64>(Record.SeedLocalTriangleId + 1));
			HashMix(GroupHash, static_cast<uint64>(Record.EvidenceId + 1));
			HashMixDouble(GroupHash, Record.SignedConvergenceVelocity);
			HashMix(GroupHash, static_cast<uint64>(RecordValidDistanceCount + 1));
			HashMixString(GroupHash, Record.TerraneHash);
		}

		TerraneHashes.Sort();
		Group.UniqueTerraneHashCount = TerraneHashes.Num();
		for (const FString& TerraneHash : TerraneHashes)
		{
			HashMixString(GroupHash, TerraneHash);
		}

		Group.MeanInterpenetrationKm = Group.ValidDistanceCount > 0
			? SumDistanceKm / static_cast<double>(Group.ValidDistanceCount)
			: 0.0;
		Group.MeanSignedConvergenceVelocity = Group.CandidateRecordCount > 0
			? SumSignedVelocity / static_cast<double>(Group.CandidateRecordCount)
			: 0.0;
		Group.bAccepted = Group.ValidDistanceCount > 0 && Group.MaxInterpenetrationKm >= OutAudit.ThresholdKm;
		HashMix(GroupHash, static_cast<uint64>(Group.ValidDistanceCount + 1));
		HashMix(GroupHash, static_cast<uint64>(Group.InvalidDistanceCount + 1));
		HashMixDouble(GroupHash, Group.MaxInterpenetrationKm);
		HashMixDouble(GroupHash, Group.MeanInterpenetrationKm);
		HashMixDouble(GroupHash, Group.MeanSignedConvergenceVelocity);
		HashMixDouble(GroupHash, Group.MaxSignedConvergenceVelocity);
		HashMixDouble(GroupHash, Group.TotalAreaWeight);
		HashMix(GroupHash, Group.bAccepted ? 1ull : 0ull);
		Group.GroupHash = HashToString(GroupHash);

		++OutAudit.GroupCount;
		if (Group.bAccepted)
		{
			++OutAudit.AcceptedGroupCount;
		}
		else
		{
			++OutAudit.RejectedGroupCount;
			if (Group.MaxInterpenetrationKm < OutAudit.ThresholdKm)
			{
				++OutAudit.SubThresholdGroupCount;
			}
		}
		OutAudit.InvalidDistanceCount += Group.InvalidDistanceCount;
		OutAudit.MaxInterpenetrationKm = FMath::Max(OutAudit.MaxInterpenetrationKm, Group.MaxInterpenetrationKm);
		OutAudit.Groups.Add(Group);

		HashMix(AuditHash, PairKey + 1ull);
		HashMixString(AuditHash, Group.GroupHash);
	}

	HashMix(AuditHash, static_cast<uint64>(OutAudit.GroupCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.AcceptedGroupCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.RejectedGroupCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.SubThresholdGroupCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.InvalidDistanceCount + 1));
	HashMixDouble(AuditHash, OutAudit.MaxInterpenetrationKm);
	OutAudit.GroupingHash = HashToString(AuditHash);
	return true;
}

bool ACarrierLabVisualizationActor::DetectPhaseIIID3DestinationMass(
	FCarrierLabPhaseIIID3DestinationMassAudit& OutAudit,
	const double InterpenetrationThresholdKm,
	const double DestinationMassThresholdRatio) const
{
	++PhaseIIIDiagnosticCallCounts.DestinationMass;
	FCarrierLabPhaseIIID1TerraneAudit TerraneAudit;
	if (!DetectPhaseIIID1ConnectedTerranes(TerraneAudit))
	{
		return false;
	}

	FCarrierLabPhaseIIID2CollisionGroupingAudit GroupingAudit;
	if (!BuildPhaseIIID2CollisionGroupsFromTerranes(TerraneAudit, GroupingAudit, InterpenetrationThresholdKm))
	{
		return false;
	}
	return BuildPhaseIIID3DestinationMassFromInputs(
		TerraneAudit,
		GroupingAudit,
		OutAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio);
}

bool ACarrierLabVisualizationActor::BuildPhaseIIID3DestinationMassFromInputs(
	const FCarrierLabPhaseIIID1TerraneAudit& TerraneAudit,
	const FCarrierLabPhaseIIID2CollisionGroupingAudit& GroupingAudit,
	FCarrierLabPhaseIIID3DestinationMassAudit& OutAudit,
	const double InterpenetrationThresholdKm,
	const double DestinationMassThresholdRatio,
	const bool bEnableDestinationComponentCache) const
{
	OutAudit = FCarrierLabPhaseIIID3DestinationMassAudit();
	OutAudit.Step = TerraneAudit.Step;
	OutAudit.EventCount = TerraneAudit.EventCount;
	OutAudit.PlateCount = TerraneAudit.PlateCount;
	OutAudit.ResetSerial = TerraneAudit.ResetSerial;
	OutAudit.InterpenetrationThresholdKm = InterpenetrationThresholdKm;
	OutAudit.DestinationMassThresholdRatio = DestinationMassThresholdRatio;
	OutAudit.SourceTerraneRecordCount = TerraneAudit.TerraneRecordCount;
	OutAudit.SourceGroupCount = GroupingAudit.GroupCount;
	OutAudit.InterpenetrationAcceptedGroupCount = GroupingAudit.AcceptedGroupCount;
	OutAudit.SourceGroupingHash = GroupingAudit.GroupingHash;

	TMap<int32, const CarrierLab::FConvergenceSubductionMatrixEvidence*> EvidenceById;
	for (const CarrierLab::FConvergenceSubductionMatrixEvidence& Evidence : State.ConvergenceSubductionMatrixEvidence)
	{
		EvidenceById.Add(Evidence.EvidenceId, &Evidence);
	}

	TMap<uint64, TArray<int32>> TerraneRecordIndicesByPair;
	for (int32 RecordIndex = 0; RecordIndex < TerraneAudit.Records.Num(); ++RecordIndex)
	{
		const FCarrierLabPhaseIIID1TerraneRecord& Record = TerraneAudit.Records[RecordIndex];
		TerraneRecordIndicesByPair.FindOrAdd(Record.PairKey).Add(RecordIndex);
	}

	TArray<FCarrierLabPhaseIIID2CollisionGroupRecord> SortedGroups = GroupingAudit.Groups;
	SortedGroups.Sort([](
		const FCarrierLabPhaseIIID2CollisionGroupRecord& A,
		const FCarrierLabPhaseIIID2CollisionGroupRecord& B)
	{
		if (A.PairKey != B.PairKey)
		{
			return A.PairKey < B.PairKey;
		}
		return A.GroupId < B.GroupId;
	});

	uint64 AuditHash = 1469598103934665603ull;
	HashMixString(AuditHash, TEXT("CarrierLab-IIID3-destination-mass-v1"));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.Step + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.EventCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.PlateCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.ResetSerial + 1));
	HashMixDouble(AuditHash, OutAudit.InterpenetrationThresholdKm);
	HashMixDouble(AuditHash, OutAudit.DestinationMassThresholdRatio);
	HashMixString(AuditHash, OutAudit.SourceGroupingHash);

	double MinAcceptedRatio = TNumericLimits<double>::Max();
	double MinRejectedRatio = TNumericLimits<double>::Max();
	TArray<FPhaseIIID1TerraneComponent> DestinationComponentCache;
	TMap<uint64, int32> DestinationComponentIndexByTriangle;
	for (const FCarrierLabPhaseIIID2CollisionGroupRecord& Group : SortedGroups)
	{
		HashMix(AuditHash, Group.PairKey + 1ull);
		HashMix(AuditHash, static_cast<uint64>(Group.GroupId + 1));
		HashMix(AuditHash, Group.bAccepted ? 1ull : 0ull);
		HashMixString(AuditHash, Group.GroupHash);
		if (!Group.bAccepted)
		{
			++OutAudit.NonAcceptedGroupCount;
			continue;
		}

		TArray<int32>* RecordIndices = TerraneRecordIndicesByPair.Find(Group.PairKey);
		if (RecordIndices == nullptr)
		{
			continue;
		}
		RecordIndices->Sort([&TerraneAudit](const int32 A, const int32 B)
		{
			const FCarrierLabPhaseIIID1TerraneRecord& RecordA = TerraneAudit.Records[A];
			const FCarrierLabPhaseIIID1TerraneRecord& RecordB = TerraneAudit.Records[B];
			if (RecordA.SourcePlateId != RecordB.SourcePlateId)
			{
				return RecordA.SourcePlateId < RecordB.SourcePlateId;
			}
			if (RecordA.OtherPlateId != RecordB.OtherPlateId)
			{
				return RecordA.OtherPlateId < RecordB.OtherPlateId;
			}
			if (RecordA.RecordId != RecordB.RecordId)
			{
				return RecordA.RecordId < RecordB.RecordId;
			}
			return RecordA.EvidenceId < RecordB.EvidenceId;
		});

		for (const int32 RecordIndex : *RecordIndices)
		{
			if (!TerraneAudit.Records.IsValidIndex(RecordIndex))
			{
				continue;
			}
			const FCarrierLabPhaseIIID1TerraneRecord& SourceRecord = TerraneAudit.Records[RecordIndex];

			FCarrierLabPhaseIIID3DestinationMassRecord& MassRecord = OutAudit.Records.AddDefaulted_GetRef();
			MassRecord.RecordId = OutAudit.Records.Num() - 1;
			MassRecord.GroupId = Group.GroupId;
			MassRecord.PairKey = Group.PairKey;
			MassRecord.SourceRecordId = SourceRecord.RecordId;
			MassRecord.SourcePlateId = SourceRecord.SourcePlateId;
			MassRecord.DestinationPlateId = SourceRecord.OtherPlateId;
			MassRecord.SourceSeedLocalTriangleId = SourceRecord.SeedLocalTriangleId;
			MassRecord.EvidenceId = SourceRecord.EvidenceId;
			MassRecord.SourceTerraneAreaWeight = SourceRecord.AreaWeight;
			MassRecord.SourceTerraneTriangleCount = SourceRecord.TriangleCount;
			MassRecord.RequiredDestinationAreaWeight = SourceRecord.AreaWeight * DestinationMassThresholdRatio;
			MassRecord.ThresholdRatio = DestinationMassThresholdRatio;
			MassRecord.bInterpenetrationAccepted = true;
			MassRecord.SourceTerraneHash = SourceRecord.TerraneHash;

			const CarrierLab::FConvergenceSubductionMatrixEvidence* Evidence = EvidenceById.FindRef(SourceRecord.EvidenceId);
			const bool bEvidenceMatches =
				Evidence != nullptr &&
				Evidence->PlateId == SourceRecord.SourcePlateId &&
				Evidence->OtherPlateId == SourceRecord.OtherPlateId &&
				Evidence->LocalTriangleId == SourceRecord.SeedLocalTriangleId;
			if (bEvidenceMatches)
			{
				MassRecord.DestinationSeedLocalTriangleId = Evidence->OtherLocalTriangleId;
			}

			FPhaseIIID1TerraneComponent DestinationComponent;
			const FPhaseIIID1TerraneComponent* DestinationComponentPtr = nullptr;
			bool bDestinationComponentValid = false;
			if (bEvidenceMatches &&
				State.Plates.IsValidIndex(MassRecord.DestinationPlateId))
			{
				const uint64 DestinationSeedKey = MakePlateTriangleKey(
					MassRecord.DestinationPlateId,
					MassRecord.DestinationSeedLocalTriangleId);
				if (bEnableDestinationComponentCache)
				{
					if (const int32* ExistingIndex = DestinationComponentIndexByTriangle.Find(DestinationSeedKey))
					{
						if (DestinationComponentCache.IsValidIndex(*ExistingIndex))
						{
							DestinationComponentPtr = &DestinationComponentCache[*ExistingIndex];
							bDestinationComponentValid = true;
							++PhaseIIIDiagnosticCallCounts.D3DestinationComponentCacheHitCount;
						}
					}
				}

				if (!bDestinationComponentValid)
				{
					const double DestinationComponentStartSeconds = FPlatformTime::Seconds();
					bDestinationComponentValid = BuildPhaseIIID1TerraneComponent(
						State.Plates[MassRecord.DestinationPlateId],
						MassRecord.DestinationSeedLocalTriangleId,
						DestinationComponent);
					PhaseIIIDiagnosticCallCounts.D3DestinationComponentExpansionSeconds +=
						FPlatformTime::Seconds() - DestinationComponentStartSeconds;
					++PhaseIIIDiagnosticCallCounts.D3DestinationComponentBuildCount;
					if (bDestinationComponentValid)
					{
						if (bEnableDestinationComponentCache)
						{
							const int32 NewIndex = DestinationComponentCache.Add(MoveTemp(DestinationComponent));
							DestinationComponentPtr = &DestinationComponentCache[NewIndex];
							for (const int32 LocalTriangleId : DestinationComponentPtr->LocalTriangleIds)
							{
								DestinationComponentIndexByTriangle.Add(
									MakePlateTriangleKey(MassRecord.DestinationPlateId, LocalTriangleId),
									NewIndex);
							}
						}
						else
						{
							DestinationComponentPtr = &DestinationComponent;
						}
					}
				}
			}

			if (bDestinationComponentValid && DestinationComponentPtr != nullptr)
			{
				const CarrierLab::FCarrierPlate& DestinationPlate = State.Plates[MassRecord.DestinationPlateId];
				MassRecord.bDestinationSeedValid = true;
				MassRecord.DestinationTriangleCount = DestinationComponentPtr->LocalTriangleIds.Num();
				MassRecord.DestinationContinentalTriangleCount = DestinationComponentPtr->ContinentalTriangleCount;
				MassRecord.DestinationInnerSeaTriangleCount = DestinationComponentPtr->InnerSeaTriangleCount;
				MassRecord.DestinationContinentalAreaWeight =
					ComputePhaseIIIDComponentAreaWeight(DestinationPlate, *DestinationComponentPtr);
			}
			else
			{
				++OutAudit.MissingDestinationSeedCount;
			}

			MassRecord.DestinationMassRatio = MassRecord.SourceTerraneAreaWeight > UE_DOUBLE_SMALL_NUMBER
				? MassRecord.DestinationContinentalAreaWeight / MassRecord.SourceTerraneAreaWeight
				: 0.0;
			MassRecord.bMassAccepted =
				MassRecord.bDestinationSeedValid &&
				MassRecord.SourceTerraneAreaWeight > UE_DOUBLE_SMALL_NUMBER &&
				MassRecord.DestinationContinentalAreaWeight + 1.0e-12 >= MassRecord.RequiredDestinationAreaWeight;
			if (MassRecord.bMassAccepted)
			{
				++OutAudit.AcceptedMassRecordCount;
				MinAcceptedRatio = FMath::Min(MinAcceptedRatio, MassRecord.DestinationMassRatio);
			}
			else
			{
				++OutAudit.RejectedMassRecordCount;
				MinRejectedRatio = FMath::Min(MinRejectedRatio, MassRecord.DestinationMassRatio);
				if (MassRecord.bDestinationSeedValid)
				{
					++OutAudit.InsufficientDestinationMassCount;
				}
			}

			++OutAudit.TestedMassRecordCount;
			OutAudit.MaxDestinationMassRatio = FMath::Max(OutAudit.MaxDestinationMassRatio, MassRecord.DestinationMassRatio);

			uint64 RecordHash = 1469598103934665603ull;
			HashMixString(RecordHash, TEXT("CarrierLab-IIID3-destination-mass-record-v1"));
			HashMix(RecordHash, static_cast<uint64>(MassRecord.RecordId + 1));
			HashMix(RecordHash, static_cast<uint64>(MassRecord.GroupId + 1));
			HashMix(RecordHash, MassRecord.PairKey + 1ull);
			HashMix(RecordHash, static_cast<uint64>(MassRecord.SourceRecordId + 1));
			HashMix(RecordHash, static_cast<uint64>(MassRecord.SourcePlateId + 1));
			HashMix(RecordHash, static_cast<uint64>(MassRecord.DestinationPlateId + 1));
			HashMix(RecordHash, static_cast<uint64>(MassRecord.SourceSeedLocalTriangleId + 1));
			HashMix(RecordHash, static_cast<uint64>(MassRecord.DestinationSeedLocalTriangleId + 1));
			HashMix(RecordHash, static_cast<uint64>(MassRecord.EvidenceId + 1));
			HashMixDouble(RecordHash, MassRecord.SourceTerraneAreaWeight);
			HashMixDouble(RecordHash, MassRecord.RequiredDestinationAreaWeight);
			HashMixDouble(RecordHash, MassRecord.DestinationContinentalAreaWeight);
			HashMixDouble(RecordHash, MassRecord.DestinationMassRatio);
			HashMix(RecordHash, static_cast<uint64>(MassRecord.SourceTerraneTriangleCount + 1));
			HashMix(RecordHash, static_cast<uint64>(MassRecord.DestinationTriangleCount + 1));
			HashMix(RecordHash, static_cast<uint64>(MassRecord.DestinationContinentalTriangleCount + 1));
			HashMix(RecordHash, static_cast<uint64>(MassRecord.DestinationInnerSeaTriangleCount + 1));
			HashMix(RecordHash, MassRecord.bDestinationSeedValid ? 1ull : 0ull);
			HashMix(RecordHash, MassRecord.bMassAccepted ? 1ull : 0ull);
			HashMixString(RecordHash, MassRecord.SourceTerraneHash);
			MassRecord.DestinationMassHash = HashToString(RecordHash);

			HashMixString(AuditHash, MassRecord.DestinationMassHash);
		}
	}

	OutAudit.MinAcceptedDestinationMassRatio =
		OutAudit.AcceptedMassRecordCount > 0 && MinAcceptedRatio < TNumericLimits<double>::Max()
			? MinAcceptedRatio
			: 0.0;
	OutAudit.MinRejectedDestinationMassRatio =
		OutAudit.RejectedMassRecordCount > 0 && MinRejectedRatio < TNumericLimits<double>::Max()
			? MinRejectedRatio
			: 0.0;
	HashMix(AuditHash, static_cast<uint64>(OutAudit.SourceTerraneRecordCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.SourceGroupCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.InterpenetrationAcceptedGroupCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.TestedMassRecordCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.AcceptedMassRecordCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.RejectedMassRecordCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.InsufficientDestinationMassCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.MissingDestinationSeedCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.NonAcceptedGroupCount + 1));
	HashMixDouble(AuditHash, OutAudit.MaxDestinationMassRatio);
	HashMixDouble(AuditHash, OutAudit.MinAcceptedDestinationMassRatio);
	HashMixDouble(AuditHash, OutAudit.MinRejectedDestinationMassRatio);
	OutAudit.DestinationMassHash = HashToString(AuditHash);
	return true;
}

bool ACarrierLabVisualizationActor::PlanPhaseIIID4SlabBreak(
	FCarrierLabPhaseIIID4SlabBreakPlanAudit& OutAudit,
	const double InterpenetrationThresholdKm,
	const double DestinationMassThresholdRatio) const
{
	++PhaseIIIDiagnosticCallCounts.SlabBreakPlan;
	FCarrierLabPhaseIIID1TerraneAudit TerraneAudit;
	if (!DetectPhaseIIID1ConnectedTerranes(TerraneAudit))
	{
		return false;
	}

	FCarrierLabPhaseIIID2CollisionGroupingAudit GroupingAudit;
	if (!BuildPhaseIIID2CollisionGroupsFromTerranes(TerraneAudit, GroupingAudit, InterpenetrationThresholdKm))
	{
		return false;
	}

	FCarrierLabPhaseIIID3DestinationMassAudit DestinationMassAudit;
	if (!BuildPhaseIIID3DestinationMassFromInputs(
		TerraneAudit,
		GroupingAudit,
		DestinationMassAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio))
	{
		return false;
	}
	return BuildPhaseIIID4SlabBreakFromInputs(
		TerraneAudit,
		DestinationMassAudit,
		OutAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio);
}

bool ACarrierLabVisualizationActor::BuildPhaseIIID4SlabBreakFromInputs(
	const FCarrierLabPhaseIIID1TerraneAudit& TerraneAudit,
	const FCarrierLabPhaseIIID3DestinationMassAudit& DestinationMassAudit,
	FCarrierLabPhaseIIID4SlabBreakPlanAudit& OutAudit,
	const double InterpenetrationThresholdKm,
	const double DestinationMassThresholdRatio) const
{
	OutAudit = FCarrierLabPhaseIIID4SlabBreakPlanAudit();
	OutAudit.Step = DestinationMassAudit.Step;
	OutAudit.EventCount = DestinationMassAudit.EventCount;
	OutAudit.PlateCount = DestinationMassAudit.PlateCount;
	OutAudit.ResetSerial = DestinationMassAudit.ResetSerial;
	OutAudit.InterpenetrationThresholdKm = InterpenetrationThresholdKm;
	OutAudit.DestinationMassThresholdRatio = DestinationMassThresholdRatio;
	OutAudit.DestinationMassRecordCount = DestinationMassAudit.TestedMassRecordCount;
	OutAudit.AcceptedDestinationMassRecordCount = DestinationMassAudit.AcceptedMassRecordCount;
	OutAudit.RejectedDestinationMassRecordCount = DestinationMassAudit.RejectedMassRecordCount;
	OutAudit.InterpenetrationAcceptedGroupCount = DestinationMassAudit.InterpenetrationAcceptedGroupCount;
	OutAudit.SourceDestinationMassHash = DestinationMassAudit.DestinationMassHash;

	TMap<int32, const FCarrierLabPhaseIIID1TerraneRecord*> TerraneRecordById;
	for (const FCarrierLabPhaseIIID1TerraneRecord& Record : TerraneAudit.Records)
	{
		TerraneRecordById.Add(Record.RecordId, &Record);
	}

	TSet<FString> PlannedSourceTerranes;
	uint64 AuditHash = 1469598103934665603ull;
	HashMixString(AuditHash, TEXT("CarrierLab-IIID4-slab-break-plan-v1"));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.Step + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.EventCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.PlateCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.ResetSerial + 1));
	HashMixDouble(AuditHash, OutAudit.InterpenetrationThresholdKm);
	HashMixDouble(AuditHash, OutAudit.DestinationMassThresholdRatio);
	HashMixString(AuditHash, OutAudit.SourceDestinationMassHash);

	for (const FCarrierLabPhaseIIID3DestinationMassRecord& MassRecord : DestinationMassAudit.Records)
	{
		if (!MassRecord.bMassAccepted)
		{
			continue;
		}

		const FCarrierLabPhaseIIID1TerraneRecord* SourceRecord = TerraneRecordById.FindRef(MassRecord.SourceRecordId);
		if (SourceRecord == nullptr)
		{
			++OutAudit.MissingSourceRecordCount;
			continue;
		}
		if (!State.Plates.IsValidIndex(SourceRecord->SourcePlateId))
		{
			++OutAudit.MissingSourceRecordCount;
			continue;
		}

		TArray<int32> RemovedTriangleIds = SourceRecord->LocalTriangleIds;
		RemovedTriangleIds.Sort();
		const FString RemovalSetHash = ComputePhaseIIID4TriangleSetHash(SourceRecord->SourcePlateId, RemovedTriangleIds);
		const FString PlanDedupKey = FString::Printf(TEXT("%d:%s"), SourceRecord->SourcePlateId, *RemovalSetHash);
		if (PlannedSourceTerranes.Contains(PlanDedupKey))
		{
			++OutAudit.DuplicatePlanCandidateCount;
			continue;
		}
		PlannedSourceTerranes.Add(PlanDedupKey);

		const CarrierLab::FCarrierPlate& SourcePlate = State.Plates[SourceRecord->SourcePlateId];
		FCarrierLabPhaseIIID4SlabBreakPlanRecord& Plan = OutAudit.Plans.AddDefaulted_GetRef();
		Plan.PlanId = OutAudit.Plans.Num() - 1;
		Plan.GroupId = MassRecord.GroupId;
		Plan.PairKey = MassRecord.PairKey;
		Plan.SourceRecordId = SourceRecord->RecordId;
		Plan.DestinationMassRecordId = MassRecord.RecordId;
		Plan.SourcePlateId = SourceRecord->SourcePlateId;
		Plan.DestinationPlateId = SourceRecord->OtherPlateId;
		Plan.SourceTriangleCountBefore = SourcePlate.LocalTriangles.Num();
		Plan.SourceVertexCountBefore = SourcePlate.Vertices.Num();
		Plan.bMassAccepted = MassRecord.bMassAccepted;
		Plan.SourceTerraneHash = SourceRecord->TerraneHash;
		Plan.RemovalSetHash = RemovalSetHash;
		Plan.RemovedLocalTriangleIds = RemovedTriangleIds;
		Plan.RemovedAreaWeight = SourceRecord->AreaWeight;

		TSet<int32> RemovedSet;
		for (const int32 LocalTriangleId : RemovedTriangleIds)
		{
			if (RemovedSet.Contains(LocalTriangleId))
			{
				++Plan.DuplicateRemovalTriangleCount;
				continue;
			}
			RemovedSet.Add(LocalTriangleId);
			if (!SourcePlate.LocalTriangles.IsValidIndex(LocalTriangleId))
			{
				++Plan.InvalidMappedTriangleCount;
			}
		}
		Plan.RemovedTriangleCount = RemovedSet.Num();
		Plan.SurvivingTriangleCount = FMath::Max(0, SourcePlate.LocalTriangles.Num() - Plan.RemovedTriangleCount);
		Plan.bWouldDestroySourcePlate = Plan.SurvivingTriangleCount == 0;
		Plan.bRemovalSetMatchesTerrane =
			Plan.DuplicateRemovalTriangleCount == 0 &&
			Plan.InvalidMappedTriangleCount == 0 &&
			Plan.RemovedTriangleCount == SourceRecord->TriangleCount;

		Plan.OldToNewLocalTriangleIds.Init(INDEX_NONE, SourcePlate.LocalTriangles.Num());

		TMap<uint64, int32> OriginalEdgeCounts;
		TMap<uint64, int32> SurvivorEdgeCounts;
		TSet<int32> SurvivingVertexSet;
		int32 NextTriangleIndex = 0;
		for (int32 OldTriangleId = 0; OldTriangleId < SourcePlate.LocalTriangles.Num(); ++OldTriangleId)
		{
			const CarrierLab::FCarrierPlateTriangle& Triangle = SourcePlate.LocalTriangles[OldTriangleId];
			if (!SourcePlate.Vertices.IsValidIndex(Triangle.A) ||
				!SourcePlate.Vertices.IsValidIndex(Triangle.B) ||
				!SourcePlate.Vertices.IsValidIndex(Triangle.C) ||
				Triangle.A == Triangle.B ||
				Triangle.B == Triangle.C ||
				Triangle.C == Triangle.A)
			{
				if (!RemovedSet.Contains(OldTriangleId))
				{
					++Plan.InvalidMappedTriangleCount;
				}
				continue;
			}

			AddPlateTriangleEdgeCounts(Triangle, OriginalEdgeCounts);
			if (RemovedSet.Contains(OldTriangleId))
			{
				continue;
			}

			Plan.OldToNewLocalTriangleIds[OldTriangleId] = NextTriangleIndex++;
			SurvivingVertexSet.Add(Triangle.A);
			SurvivingVertexSet.Add(Triangle.B);
			SurvivingVertexSet.Add(Triangle.C);
			AddPlateTriangleEdgeCounts(Triangle, SurvivorEdgeCounts);
		}

		TArray<int32> SurvivingVertexIds = SurvivingVertexSet.Array();
		SurvivingVertexIds.Sort();
		Plan.OldToNewLocalVertexIds.Init(INDEX_NONE, SourcePlate.Vertices.Num());
		for (int32 NewVertexId = 0; NewVertexId < SurvivingVertexIds.Num(); ++NewVertexId)
		{
			const int32 OldVertexId = SurvivingVertexIds[NewVertexId];
			if (Plan.OldToNewLocalVertexIds.IsValidIndex(OldVertexId))
			{
				Plan.OldToNewLocalVertexIds[OldVertexId] = NewVertexId;
			}
		}
		Plan.SurvivingVertexCount = SurvivingVertexIds.Num();
		Plan.RemovedVertexCount = FMath::Max(0, SourcePlate.Vertices.Num() - Plan.SurvivingVertexCount);

		uint64 SurvivorTopologyHash = 1469598103934665603ull;
		HashMixString(SurvivorTopologyHash, TEXT("CarrierLab-IIID4-survivor-topology-v1"));
		HashMix(SurvivorTopologyHash, static_cast<uint64>(Plan.SourcePlateId + 1));
		HashMix(SurvivorTopologyHash, static_cast<uint64>(Plan.SurvivingTriangleCount + 1));
		HashMix(SurvivorTopologyHash, static_cast<uint64>(Plan.SurvivingVertexCount + 1));
		for (int32 OldTriangleId = 0; OldTriangleId < SourcePlate.LocalTriangles.Num(); ++OldTriangleId)
		{
			if (RemovedSet.Contains(OldTriangleId))
			{
				continue;
			}
			const CarrierLab::FCarrierPlateTriangle& Triangle = SourcePlate.LocalTriangles[OldTriangleId];
			const int32 NewA = Plan.OldToNewLocalVertexIds.IsValidIndex(Triangle.A) ? Plan.OldToNewLocalVertexIds[Triangle.A] : INDEX_NONE;
			const int32 NewB = Plan.OldToNewLocalVertexIds.IsValidIndex(Triangle.B) ? Plan.OldToNewLocalVertexIds[Triangle.B] : INDEX_NONE;
			const int32 NewC = Plan.OldToNewLocalVertexIds.IsValidIndex(Triangle.C) ? Plan.OldToNewLocalVertexIds[Triangle.C] : INDEX_NONE;
			if (NewA == INDEX_NONE || NewB == INDEX_NONE || NewC == INDEX_NONE || NewA == NewB || NewB == NewC || NewC == NewA)
			{
				++Plan.InvalidMappedTriangleCount;
				continue;
			}
			HashMix(SurvivorTopologyHash, static_cast<uint64>(Plan.OldToNewLocalTriangleIds[OldTriangleId] + 1));
			HashMix(SurvivorTopologyHash, static_cast<uint64>(NewA + 1));
			HashMix(SurvivorTopologyHash, static_cast<uint64>(NewB + 1));
			HashMix(SurvivorTopologyHash, static_cast<uint64>(NewC + 1));
			HashMix(SurvivorTopologyHash, static_cast<uint64>(Triangle.SourceTriangleId + 1));
		}

		for (const TPair<uint64, int32>& EdgePair : SurvivorEdgeCounts)
		{
			const int32 SurvivorCount = EdgePair.Value;
			const int32 OriginalCount = OriginalEdgeCounts.FindRef(EdgePair.Key);
			if (SurvivorCount == 1)
			{
				++Plan.BoundaryEdgeCount;
				if (OriginalCount >= 2)
				{
					++Plan.CutBoundaryEdgeCount;
				}
			}
			else if (SurvivorCount == 2)
			{
				++Plan.InteriorEdgeCount;
			}
			else
			{
				++Plan.NonManifoldEdgeCount;
			}
		}

		Plan.OldToNewTriangleMapHash = ComputePhaseIIID4IndexMapHash(
			TEXT("CarrierLab-IIID4-triangle-map-v1"),
			Plan.SourcePlateId,
			Plan.OldToNewLocalTriangleIds);
		Plan.OldToNewVertexMapHash = ComputePhaseIIID4IndexMapHash(
			TEXT("CarrierLab-IIID4-vertex-map-v1"),
			Plan.SourcePlateId,
			Plan.OldToNewLocalVertexIds);
		HashMix(SurvivorTopologyHash, static_cast<uint64>(Plan.BoundaryEdgeCount + 1));
		HashMix(SurvivorTopologyHash, static_cast<uint64>(Plan.CutBoundaryEdgeCount + 1));
		HashMix(SurvivorTopologyHash, static_cast<uint64>(Plan.InteriorEdgeCount + 1));
		HashMix(SurvivorTopologyHash, static_cast<uint64>(Plan.NonManifoldEdgeCount + 1));
		Plan.SurvivorTopologyHash = HashToString(SurvivorTopologyHash);
		Plan.bTopologyValid =
			Plan.bRemovalSetMatchesTerrane &&
			Plan.InvalidMappedTriangleCount == 0 &&
			Plan.NonManifoldEdgeCount == 0 &&
			Plan.SurvivingTriangleCount + Plan.RemovedTriangleCount == Plan.SourceTriangleCountBefore &&
			Plan.SurvivingVertexCount <= Plan.SourceVertexCountBefore;

		uint64 PlanHash = 1469598103934665603ull;
		HashMixString(PlanHash, TEXT("CarrierLab-IIID4-plan-record-v1"));
		HashMix(PlanHash, static_cast<uint64>(Plan.PlanId + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.GroupId + 1));
		HashMix(PlanHash, Plan.PairKey + 1ull);
		HashMix(PlanHash, static_cast<uint64>(Plan.SourceRecordId + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.DestinationMassRecordId + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.SourcePlateId + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.DestinationPlateId + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.SourceTriangleCountBefore + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.SourceVertexCountBefore + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.RemovedTriangleCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.SurvivingTriangleCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.SurvivingVertexCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.RemovedVertexCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.BoundaryEdgeCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.CutBoundaryEdgeCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.InteriorEdgeCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.NonManifoldEdgeCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.InvalidMappedTriangleCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.DuplicateRemovalTriangleCount + 1));
		HashMixDouble(PlanHash, Plan.RemovedAreaWeight);
		HashMix(PlanHash, Plan.bMassAccepted ? 1ull : 0ull);
		HashMix(PlanHash, Plan.bRemovalSetMatchesTerrane ? 1ull : 0ull);
		HashMix(PlanHash, Plan.bTopologyValid ? 1ull : 0ull);
		HashMix(PlanHash, Plan.bWouldDestroySourcePlate ? 1ull : 0ull);
		HashMixString(PlanHash, Plan.SourceTerraneHash);
		HashMixString(PlanHash, Plan.RemovalSetHash);
		HashMixString(PlanHash, Plan.OldToNewTriangleMapHash);
		HashMixString(PlanHash, Plan.OldToNewVertexMapHash);
		HashMixString(PlanHash, Plan.SurvivorTopologyHash);
		Plan.PlanHash = HashToString(PlanHash);

		++OutAudit.PlanCount;
		if (Plan.bTopologyValid)
		{
			++OutAudit.ValidPlanCount;
		}
		else
		{
			++OutAudit.InvalidPlanCount;
		}
		if (Plan.bWouldDestroySourcePlate)
		{
			++OutAudit.WouldDestroySourcePlateCount;
		}
		OutAudit.TotalRemovedTriangleCount += Plan.RemovedTriangleCount;
		OutAudit.TotalSurvivingTriangleCount += Plan.SurvivingTriangleCount;
		OutAudit.TotalRemovedAreaWeight += Plan.RemovedAreaWeight;

		HashMixString(AuditHash, Plan.PlanHash);
	}

	HashMix(AuditHash, static_cast<uint64>(OutAudit.DestinationMassRecordCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.AcceptedDestinationMassRecordCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.RejectedDestinationMassRecordCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.InterpenetrationAcceptedGroupCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.DuplicatePlanCandidateCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.MissingSourceRecordCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.PlanCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.ValidPlanCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.InvalidPlanCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.WouldDestroySourcePlateCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.TotalRemovedTriangleCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.TotalSurvivingTriangleCount + 1));
	HashMixDouble(AuditHash, OutAudit.TotalRemovedAreaWeight);
	OutAudit.SlabBreakPlanHash = HashToString(AuditHash);
	return true;
}

bool ACarrierLabVisualizationActor::PlanPhaseIIID5Suture(
	FCarrierLabPhaseIIID5SuturePlanAudit& OutAudit,
	const double InterpenetrationThresholdKm,
	const double DestinationMassThresholdRatio) const
{
	++PhaseIIIDiagnosticCallCounts.SuturePlan;
	FCarrierLabPhaseIIID1TerraneAudit TerraneAudit;
	if (!DetectPhaseIIID1ConnectedTerranes(TerraneAudit))
	{
		return false;
	}

	FCarrierLabPhaseIIID2CollisionGroupingAudit GroupingAudit;
	if (!BuildPhaseIIID2CollisionGroupsFromTerranes(TerraneAudit, GroupingAudit, InterpenetrationThresholdKm))
	{
		return false;
	}

	FCarrierLabPhaseIIID3DestinationMassAudit DestinationMassAudit;
	if (!BuildPhaseIIID3DestinationMassFromInputs(
		TerraneAudit,
		GroupingAudit,
		DestinationMassAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio))
	{
		return false;
	}

	FCarrierLabPhaseIIID4SlabBreakPlanAudit SlabBreakAudit;
	if (!BuildPhaseIIID4SlabBreakFromInputs(
		TerraneAudit,
		DestinationMassAudit,
		SlabBreakAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio))
	{
		return false;
	}
	return BuildPhaseIIID5SutureFromSlabBreak(
		SlabBreakAudit,
		OutAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio);
}

bool ACarrierLabVisualizationActor::BuildPhaseIIID5SutureFromSlabBreak(
	const FCarrierLabPhaseIIID4SlabBreakPlanAudit& SlabBreakAudit,
	FCarrierLabPhaseIIID5SuturePlanAudit& OutAudit,
	const double InterpenetrationThresholdKm,
	const double DestinationMassThresholdRatio) const
{
	OutAudit = FCarrierLabPhaseIIID5SuturePlanAudit();
	OutAudit.Step = SlabBreakAudit.Step;
	OutAudit.EventCount = SlabBreakAudit.EventCount;
	OutAudit.PlateCount = SlabBreakAudit.PlateCount;
	OutAudit.ResetSerial = SlabBreakAudit.ResetSerial;
	OutAudit.InterpenetrationThresholdKm = InterpenetrationThresholdKm;
	OutAudit.DestinationMassThresholdRatio = DestinationMassThresholdRatio;
	OutAudit.SlabBreakPlanCount = SlabBreakAudit.PlanCount;
	OutAudit.ValidSlabBreakPlanCount = SlabBreakAudit.ValidPlanCount;
	OutAudit.SourceSlabBreakPlanHash = SlabBreakAudit.SlabBreakPlanHash;

	uint64 AuditHash = 1469598103934665603ull;
	HashMixString(AuditHash, TEXT("CarrierLab-IIID5-suture-plan-v1"));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.Step + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.EventCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.PlateCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.ResetSerial + 1));
	HashMixDouble(AuditHash, OutAudit.InterpenetrationThresholdKm);
	HashMixDouble(AuditHash, OutAudit.DestinationMassThresholdRatio);
	HashMixString(AuditHash, OutAudit.SourceSlabBreakPlanHash);

	for (const FCarrierLabPhaseIIID4SlabBreakPlanRecord& SlabPlan : SlabBreakAudit.Plans)
	{
		if (!SlabPlan.bTopologyValid || SlabPlan.bWouldDestroySourcePlate)
		{
			continue;
		}
		if (!State.Plates.IsValidIndex(SlabPlan.SourcePlateId))
		{
			++OutAudit.MissingSourcePlateCount;
			continue;
		}
		if (!State.Plates.IsValidIndex(SlabPlan.DestinationPlateId))
		{
			++OutAudit.MissingDestinationPlateCount;
			continue;
		}

		const CarrierLab::FCarrierPlate& SourcePlate = State.Plates[SlabPlan.SourcePlateId];
		const CarrierLab::FCarrierPlate& DestinationPlate = State.Plates[SlabPlan.DestinationPlateId];
		FCarrierLabPhaseIIID5SuturePlanRecord& Plan = OutAudit.Plans.AddDefaulted_GetRef();
		Plan.PlanId = OutAudit.Plans.Num() - 1;
		Plan.SlabBreakPlanId = SlabPlan.PlanId;
		Plan.GroupId = SlabPlan.GroupId;
		Plan.PairKey = SlabPlan.PairKey;
		Plan.SourcePlateId = SlabPlan.SourcePlateId;
		Plan.DestinationPlateId = SlabPlan.DestinationPlateId;
		Plan.SourceTriangleCountBefore = SourcePlate.LocalTriangles.Num();
		Plan.DestinationTriangleCountBefore = DestinationPlate.LocalTriangles.Num();
		Plan.DestinationVertexCountBefore = DestinationPlate.Vertices.Num();
		Plan.bSlabBreakPlanValid = SlabPlan.bTopologyValid && !SlabPlan.bWouldDestroySourcePlate;
		Plan.SourceTerraneHash = SlabPlan.SourceTerraneHash;
		Plan.RemovalSetHash = SlabPlan.RemovalSetHash;
		Plan.AddedSourceLocalTriangleIds = SlabPlan.RemovedLocalTriangleIds;
		Plan.AddedSourceLocalTriangleIds.Sort();
		Plan.AddedTriangleSetHash = ComputePhaseIIID5TriangleSetHash(
			Plan.SourcePlateId,
			Plan.DestinationPlateId,
			Plan.AddedSourceLocalTriangleIds);

		TSet<int32> DestinationSourceTriangleIds;
		TMap<uint64, int32> DestinationEdgeCountsBefore;
		for (const CarrierLab::FCarrierPlateTriangle& DestinationTriangle : DestinationPlate.LocalTriangles)
		{
			if (DestinationTriangle.SourceTriangleId != INDEX_NONE)
			{
				DestinationSourceTriangleIds.Add(DestinationTriangle.SourceTriangleId);
			}
			if (DestinationPlate.Vertices.IsValidIndex(DestinationTriangle.A) &&
				DestinationPlate.Vertices.IsValidIndex(DestinationTriangle.B) &&
				DestinationPlate.Vertices.IsValidIndex(DestinationTriangle.C) &&
				DestinationTriangle.A != DestinationTriangle.B &&
				DestinationTriangle.B != DestinationTriangle.C &&
				DestinationTriangle.C != DestinationTriangle.A)
			{
				AddPlateTriangleEdgeCounts(DestinationTriangle, DestinationEdgeCountsBefore);
			}
		}
		for (const TPair<uint64, int32>& EdgePair : DestinationEdgeCountsBefore)
		{
			if (EdgePair.Value == 1)
			{
				++Plan.DestinationBoundaryEdgeCountBefore;
			}
		}

		TSet<int32> AddedTriangleSet;
		TSet<int32> AddedSourceVertexSet;
		for (const int32 SourceLocalTriangleId : Plan.AddedSourceLocalTriangleIds)
		{
			if (AddedTriangleSet.Contains(SourceLocalTriangleId))
			{
				continue;
			}
			AddedTriangleSet.Add(SourceLocalTriangleId);
			if (!SourcePlate.LocalTriangles.IsValidIndex(SourceLocalTriangleId))
			{
				++Plan.InvalidSourceTriangleCount;
				continue;
			}

			const CarrierLab::FCarrierPlateTriangle& SourceTriangle = SourcePlate.LocalTriangles[SourceLocalTriangleId];
			if (SourceTriangle.SourceTriangleId != INDEX_NONE &&
				DestinationSourceTriangleIds.Contains(SourceTriangle.SourceTriangleId))
			{
				++Plan.DuplicateDestinationSourceTriangleCount;
			}
			if (!SourcePlate.Vertices.IsValidIndex(SourceTriangle.A) ||
				!SourcePlate.Vertices.IsValidIndex(SourceTriangle.B) ||
				!SourcePlate.Vertices.IsValidIndex(SourceTriangle.C) ||
				SourceTriangle.A == SourceTriangle.B ||
				SourceTriangle.B == SourceTriangle.C ||
				SourceTriangle.C == SourceTriangle.A)
			{
				++Plan.InvalidSourceVertexCount;
				continue;
			}

			AddedSourceVertexSet.Add(SourceTriangle.A);
			AddedSourceVertexSet.Add(SourceTriangle.B);
			AddedSourceVertexSet.Add(SourceTriangle.C);
			Plan.AddedAreaWeight += ComputePlateLocalTriangleAreaWeight(SourcePlate, SourceLocalTriangleId);
		}

		TArray<int32> AddedSourceVertexIds = AddedSourceVertexSet.Array();
		AddedSourceVertexIds.Sort();
		Plan.AddedTriangleCount = AddedTriangleSet.Num();
		Plan.AddedVertexCount = AddedSourceVertexIds.Num();
		Plan.PostSutureTriangleCount = Plan.DestinationTriangleCountBefore + Plan.AddedTriangleCount;
		Plan.PostSutureVertexCount = Plan.DestinationVertexCountBefore + Plan.AddedVertexCount;
		Plan.DestinationOldToNewLocalTriangleIds.Init(INDEX_NONE, DestinationPlate.LocalTriangles.Num());
		for (int32 LocalTriangleId = 0; LocalTriangleId < DestinationPlate.LocalTriangles.Num(); ++LocalTriangleId)
		{
			Plan.DestinationOldToNewLocalTriangleIds[LocalTriangleId] = LocalTriangleId;
		}
		Plan.DestinationOldToNewLocalVertexIds.Init(INDEX_NONE, DestinationPlate.Vertices.Num());
		for (int32 LocalVertexId = 0; LocalVertexId < DestinationPlate.Vertices.Num(); ++LocalVertexId)
		{
			Plan.DestinationOldToNewLocalVertexIds[LocalVertexId] = LocalVertexId;
		}
		Plan.SourceToDestinationAddedTriangleIds.Init(INDEX_NONE, SourcePlate.LocalTriangles.Num());
		Plan.SourceToDestinationAddedVertexIds.Init(INDEX_NONE, SourcePlate.Vertices.Num());
		for (int32 AddedVertexIndex = 0; AddedVertexIndex < AddedSourceVertexIds.Num(); ++AddedVertexIndex)
		{
			const int32 SourceVertexId = AddedSourceVertexIds[AddedVertexIndex];
			if (Plan.SourceToDestinationAddedVertexIds.IsValidIndex(SourceVertexId))
			{
				Plan.SourceToDestinationAddedVertexIds[SourceVertexId] =
					Plan.DestinationVertexCountBefore + AddedVertexIndex;
			}
		}

		TMap<uint64, int32> PostEdgeCounts = DestinationEdgeCountsBefore;
		TMap<uint64, int32> AddedOnlyEdgeCounts;
		uint64 PostTopologyHash = 1469598103934665603ull;
		HashMixString(PostTopologyHash, TEXT("CarrierLab-IIID5-post-suture-topology-v1"));
		HashMix(PostTopologyHash, static_cast<uint64>(Plan.SourcePlateId + 1));
		HashMix(PostTopologyHash, static_cast<uint64>(Plan.DestinationPlateId + 1));
		HashMix(PostTopologyHash, static_cast<uint64>(Plan.DestinationTriangleCountBefore + 1));
		HashMix(PostTopologyHash, static_cast<uint64>(Plan.DestinationVertexCountBefore + 1));
		HashMix(PostTopologyHash, static_cast<uint64>(Plan.AddedTriangleCount + 1));
		HashMix(PostTopologyHash, static_cast<uint64>(Plan.AddedVertexCount + 1));

		for (int32 LocalTriangleId = 0; LocalTriangleId < DestinationPlate.LocalTriangles.Num(); ++LocalTriangleId)
		{
			const CarrierLab::FCarrierPlateTriangle& Triangle = DestinationPlate.LocalTriangles[LocalTriangleId];
			HashMix(PostTopologyHash, static_cast<uint64>(LocalTriangleId + 1));
			HashMix(PostTopologyHash, static_cast<uint64>(Triangle.A + 1));
			HashMix(PostTopologyHash, static_cast<uint64>(Triangle.B + 1));
			HashMix(PostTopologyHash, static_cast<uint64>(Triangle.C + 1));
			HashMix(PostTopologyHash, static_cast<uint64>(Triangle.SourceTriangleId + 1));
		}

		int32 NextAddedTriangleId = Plan.DestinationTriangleCountBefore;
		for (const int32 SourceLocalTriangleId : Plan.AddedSourceLocalTriangleIds)
		{
			if (!SourcePlate.LocalTriangles.IsValidIndex(SourceLocalTriangleId))
			{
				continue;
			}
			const CarrierLab::FCarrierPlateTriangle& SourceTriangle = SourcePlate.LocalTriangles[SourceLocalTriangleId];
			const int32 NewA = Plan.SourceToDestinationAddedVertexIds.IsValidIndex(SourceTriangle.A)
				? Plan.SourceToDestinationAddedVertexIds[SourceTriangle.A]
				: INDEX_NONE;
			const int32 NewB = Plan.SourceToDestinationAddedVertexIds.IsValidIndex(SourceTriangle.B)
				? Plan.SourceToDestinationAddedVertexIds[SourceTriangle.B]
				: INDEX_NONE;
			const int32 NewC = Plan.SourceToDestinationAddedVertexIds.IsValidIndex(SourceTriangle.C)
				? Plan.SourceToDestinationAddedVertexIds[SourceTriangle.C]
				: INDEX_NONE;
			if (NewA == INDEX_NONE || NewB == INDEX_NONE || NewC == INDEX_NONE ||
				NewA == NewB || NewB == NewC || NewC == NewA)
			{
				++Plan.InvalidSourceVertexCount;
				continue;
			}

			Plan.SourceToDestinationAddedTriangleIds[SourceLocalTriangleId] = NextAddedTriangleId++;
			CarrierLab::FCarrierPlateTriangle AddedTriangle = SourceTriangle;
			AddedTriangle.A = NewA;
			AddedTriangle.B = NewB;
			AddedTriangle.C = NewC;
			AddPlateTriangleEdgeCounts(AddedTriangle, PostEdgeCounts);
			AddPlateTriangleEdgeCounts(AddedTriangle, AddedOnlyEdgeCounts);

			HashMix(PostTopologyHash, static_cast<uint64>(SourceLocalTriangleId + 1));
			HashMix(PostTopologyHash, static_cast<uint64>(Plan.SourceToDestinationAddedTriangleIds[SourceLocalTriangleId] + 1));
			HashMix(PostTopologyHash, static_cast<uint64>(NewA + 1));
			HashMix(PostTopologyHash, static_cast<uint64>(NewB + 1));
			HashMix(PostTopologyHash, static_cast<uint64>(NewC + 1));
			HashMix(PostTopologyHash, static_cast<uint64>(SourceTriangle.SourceTriangleId + 1));
		}

		for (const TPair<uint64, int32>& EdgePair : PostEdgeCounts)
		{
			if (EdgePair.Value == 1)
			{
				++Plan.PostBoundaryEdgeCount;
			}
			else if (EdgePair.Value == 2)
			{
				++Plan.PostInteriorEdgeCount;
			}
			else
			{
				++Plan.PostNonManifoldEdgeCount;
			}
		}
		for (const TPair<uint64, int32>& EdgePair : AddedOnlyEdgeCounts)
		{
			if (EdgePair.Value == 1 && PostEdgeCounts.FindRef(EdgePair.Key) == 1)
			{
				++Plan.SutureBoundaryEdgeCount;
			}
		}

		auto TriangleHasBoundaryEdge = [&PostEdgeCounts](
			const int32 A,
			const int32 B,
			const int32 C) -> bool
		{
			return PostEdgeCounts.FindRef(MakeLocalEdgeKey(A, B)) == 1 ||
				PostEdgeCounts.FindRef(MakeLocalEdgeKey(B, C)) == 1 ||
				PostEdgeCounts.FindRef(MakeLocalEdgeKey(C, A)) == 1;
		};
		for (const CarrierLab::FCarrierPlateTriangle& Triangle : DestinationPlate.LocalTriangles)
		{
			if (TriangleHasBoundaryEdge(Triangle.A, Triangle.B, Triangle.C))
			{
				++Plan.PostBoundaryTriangleCount;
			}
		}
		for (const int32 SourceLocalTriangleId : Plan.AddedSourceLocalTriangleIds)
		{
			if (!SourcePlate.LocalTriangles.IsValidIndex(SourceLocalTriangleId))
			{
				continue;
			}
			const CarrierLab::FCarrierPlateTriangle& SourceTriangle = SourcePlate.LocalTriangles[SourceLocalTriangleId];
			const int32 NewA = Plan.SourceToDestinationAddedVertexIds.IsValidIndex(SourceTriangle.A)
				? Plan.SourceToDestinationAddedVertexIds[SourceTriangle.A]
				: INDEX_NONE;
			const int32 NewB = Plan.SourceToDestinationAddedVertexIds.IsValidIndex(SourceTriangle.B)
				? Plan.SourceToDestinationAddedVertexIds[SourceTriangle.B]
				: INDEX_NONE;
			const int32 NewC = Plan.SourceToDestinationAddedVertexIds.IsValidIndex(SourceTriangle.C)
				? Plan.SourceToDestinationAddedVertexIds[SourceTriangle.C]
				: INDEX_NONE;
			if (NewA == INDEX_NONE || NewB == INDEX_NONE || NewC == INDEX_NONE)
			{
				continue;
			}
			if (TriangleHasBoundaryEdge(NewA, NewB, NewC))
			{
				++Plan.PostBoundaryTriangleCount;
				++Plan.AddedBoundaryTriangleCount;
			}
		}

		Plan.DestinationOldToNewTriangleMapHash = ComputePhaseIIID5IndexMapHash(
			TEXT("CarrierLab-IIID5-destination-triangle-map-v1"),
			Plan.SourcePlateId,
			Plan.DestinationPlateId,
			Plan.DestinationOldToNewLocalTriangleIds);
		Plan.DestinationOldToNewVertexMapHash = ComputePhaseIIID5IndexMapHash(
			TEXT("CarrierLab-IIID5-destination-vertex-map-v1"),
			Plan.SourcePlateId,
			Plan.DestinationPlateId,
			Plan.DestinationOldToNewLocalVertexIds);
		Plan.SourceToDestinationAddedTriangleMapHash = ComputePhaseIIID5IndexMapHash(
			TEXT("CarrierLab-IIID5-added-triangle-map-v1"),
			Plan.SourcePlateId,
			Plan.DestinationPlateId,
			Plan.SourceToDestinationAddedTriangleIds);
		Plan.SourceToDestinationAddedVertexMapHash = ComputePhaseIIID5IndexMapHash(
			TEXT("CarrierLab-IIID5-added-vertex-map-v1"),
			Plan.SourcePlateId,
			Plan.DestinationPlateId,
			Plan.SourceToDestinationAddedVertexIds);
		HashMix(PostTopologyHash, static_cast<uint64>(Plan.PostBoundaryEdgeCount + 1));
		HashMix(PostTopologyHash, static_cast<uint64>(Plan.PostInteriorEdgeCount + 1));
		HashMix(PostTopologyHash, static_cast<uint64>(Plan.PostNonManifoldEdgeCount + 1));
		HashMix(PostTopologyHash, static_cast<uint64>(Plan.PostBoundaryTriangleCount + 1));
		HashMix(PostTopologyHash, static_cast<uint64>(Plan.AddedBoundaryTriangleCount + 1));
		HashMix(PostTopologyHash, static_cast<uint64>(Plan.SutureBoundaryEdgeCount + 1));
		Plan.PostSutureTopologyHash = HashToString(PostTopologyHash);

		Plan.bAddsExactlyRemovedTerrane =
			Plan.AddedTriangleCount == SlabPlan.RemovedTriangleCount &&
			Plan.InvalidSourceTriangleCount == 0 &&
			Plan.InvalidSourceVertexCount == 0 &&
			Plan.DuplicateDestinationSourceTriangleCount == 0 &&
			FMath::IsNearlyEqual(Plan.AddedAreaWeight, SlabPlan.RemovedAreaWeight, 1.0e-12);
		Plan.bBoundaryTrackingReinitializable =
			Plan.PostBoundaryEdgeCount > 0 &&
			Plan.PostBoundaryTriangleCount > 0 &&
			Plan.AddedBoundaryTriangleCount > 0 &&
			Plan.SutureBoundaryEdgeCount > 0 &&
			Plan.PostNonManifoldEdgeCount == 0;
		Plan.bTopologyValid =
			Plan.bSlabBreakPlanValid &&
			Plan.bAddsExactlyRemovedTerrane &&
			Plan.bBoundaryTrackingReinitializable &&
			Plan.PostSutureTriangleCount == Plan.DestinationTriangleCountBefore + Plan.AddedTriangleCount &&
			Plan.PostSutureVertexCount == Plan.DestinationVertexCountBefore + Plan.AddedVertexCount;

		uint64 PlanHash = 1469598103934665603ull;
		HashMixString(PlanHash, TEXT("CarrierLab-IIID5-plan-record-v1"));
		HashMix(PlanHash, static_cast<uint64>(Plan.PlanId + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.SlabBreakPlanId + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.GroupId + 1));
		HashMix(PlanHash, Plan.PairKey + 1ull);
		HashMix(PlanHash, static_cast<uint64>(Plan.SourcePlateId + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.DestinationPlateId + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.AddedTriangleCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.AddedVertexCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.PostSutureTriangleCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.PostSutureVertexCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.PostBoundaryEdgeCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.PostInteriorEdgeCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.PostNonManifoldEdgeCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.PostBoundaryTriangleCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.AddedBoundaryTriangleCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.SutureBoundaryEdgeCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.InvalidSourceTriangleCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.InvalidSourceVertexCount + 1));
		HashMix(PlanHash, static_cast<uint64>(Plan.DuplicateDestinationSourceTriangleCount + 1));
		HashMixDouble(PlanHash, Plan.AddedAreaWeight);
		HashMix(PlanHash, Plan.bSlabBreakPlanValid ? 1ull : 0ull);
		HashMix(PlanHash, Plan.bAddsExactlyRemovedTerrane ? 1ull : 0ull);
		HashMix(PlanHash, Plan.bTopologyValid ? 1ull : 0ull);
		HashMix(PlanHash, Plan.bBoundaryTrackingReinitializable ? 1ull : 0ull);
		HashMixString(PlanHash, Plan.SourceTerraneHash);
		HashMixString(PlanHash, Plan.RemovalSetHash);
		HashMixString(PlanHash, Plan.AddedTriangleSetHash);
		HashMixString(PlanHash, Plan.DestinationOldToNewTriangleMapHash);
		HashMixString(PlanHash, Plan.DestinationOldToNewVertexMapHash);
		HashMixString(PlanHash, Plan.SourceToDestinationAddedTriangleMapHash);
		HashMixString(PlanHash, Plan.SourceToDestinationAddedVertexMapHash);
		HashMixString(PlanHash, Plan.PostSutureTopologyHash);
		Plan.PlanHash = HashToString(PlanHash);

		++OutAudit.SuturePlanCount;
		if (Plan.bTopologyValid)
		{
			++OutAudit.ValidSuturePlanCount;
		}
		else
		{
			++OutAudit.InvalidSuturePlanCount;
		}
		if (Plan.bBoundaryTrackingReinitializable)
		{
			++OutAudit.BoundaryReinitializablePlanCount;
		}
		OutAudit.TotalAddedTriangleCount += Plan.AddedTriangleCount;
		OutAudit.TotalAddedVertexCount += Plan.AddedVertexCount;
		OutAudit.TotalAddedAreaWeight += Plan.AddedAreaWeight;

		HashMixString(AuditHash, Plan.PlanHash);
	}

	HashMix(AuditHash, static_cast<uint64>(OutAudit.SlabBreakPlanCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.ValidSlabBreakPlanCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.SuturePlanCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.ValidSuturePlanCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.InvalidSuturePlanCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.BoundaryReinitializablePlanCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.MissingSourcePlateCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.MissingDestinationPlateCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.TotalAddedTriangleCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.TotalAddedVertexCount + 1));
	HashMixDouble(AuditHash, OutAudit.TotalAddedAreaWeight);
	OutAudit.SuturePlanHash = HashToString(AuditHash);
	return true;
}

bool ACarrierLabVisualizationActor::ApplyPhaseIIID6DetachAndSuture(
	FCarrierLabPhaseIIID6TopologyMutationAudit& OutAudit,
	const double InterpenetrationThresholdKm,
	const double DestinationMassThresholdRatio)
{
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}

	FCarrierLabPhaseIIID1TerraneAudit TerraneAudit;
	if (!DetectPhaseIIID1ConnectedTerranes(TerraneAudit))
	{
		return false;
	}
	FCarrierLabPhaseIIID2CollisionGroupingAudit GroupingAudit;
	if (!BuildPhaseIIID2CollisionGroupsFromTerranes(TerraneAudit, GroupingAudit, InterpenetrationThresholdKm))
	{
		return false;
	}
	FCarrierLabPhaseIIID3DestinationMassAudit DestinationMassAudit;
	if (!BuildPhaseIIID3DestinationMassFromInputs(
		TerraneAudit,
		GroupingAudit,
		DestinationMassAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio))
	{
		return false;
	}
	FCarrierLabPhaseIIID4SlabBreakPlanAudit SlabBreakAudit;
	if (!BuildPhaseIIID4SlabBreakFromInputs(
		TerraneAudit,
		DestinationMassAudit,
		SlabBreakAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio))
	{
		return false;
	}
	FCarrierLabPhaseIIID5SuturePlanAudit SutureAudit;
	if (!BuildPhaseIIID5SutureFromSlabBreak(
		SlabBreakAudit,
		SutureAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio))
	{
		return false;
	}
	return ApplyPhaseIIID6DetachAndSutureFromPlans(
		SlabBreakAudit,
		SutureAudit,
		OutAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio);
}

bool ACarrierLabVisualizationActor::ApplyPhaseIIID6DetachAndSutureFromPlans(
	const FCarrierLabPhaseIIID4SlabBreakPlanAudit& SlabBreakAudit,
	const FCarrierLabPhaseIIID5SuturePlanAudit& SutureAudit,
	FCarrierLabPhaseIIID6TopologyMutationAudit& OutAudit,
	const double InterpenetrationThresholdKm,
	const double DestinationMassThresholdRatio)
{
	++PhaseIIIDiagnosticCallCounts.TopologyMutation;
	OutAudit = FCarrierLabPhaseIIID6TopologyMutationAudit();
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCountBefore = CurrentMetrics.EventCount;
	OutAudit.EventCountAfter = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerialBefore = State.ConvergenceTrackingResetSerial;
	OutAudit.ResetSerialAfter = State.ConvergenceTrackingResetSerial;
	OutAudit.InterpenetrationThresholdKm = InterpenetrationThresholdKm;
	OutAudit.DestinationMassThresholdRatio = DestinationMassThresholdRatio;
	OutAudit.SlabBreakPlanCount = SlabBreakAudit.PlanCount;
	OutAudit.ValidSlabBreakPlanCount = SlabBreakAudit.ValidPlanCount;
	OutAudit.SuturePlanCount = SutureAudit.SuturePlanCount;
	OutAudit.ValidSuturePlanCount = SutureAudit.ValidSuturePlanCount;
	OutAudit.InvalidPlanCount = SutureAudit.InvalidSuturePlanCount;
	OutAudit.SourceSlabBreakPlanHash = SlabBreakAudit.SlabBreakPlanHash;
	OutAudit.SourceSuturePlanHash = SutureAudit.SuturePlanHash;

	uint64 AuditHash = 1469598103934665603ull;
	HashMixString(AuditHash, TEXT("CarrierLab-IIID6-topology-mutation-audit-v1"));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.Step + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.EventCountBefore + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.PlateCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.ResetSerialBefore + 1));
	HashMixDouble(AuditHash, OutAudit.InterpenetrationThresholdKm);
	HashMixDouble(AuditHash, OutAudit.DestinationMassThresholdRatio);
	HashMixString(AuditHash, OutAudit.SourceSlabBreakPlanHash);
	HashMixString(AuditHash, OutAudit.SourceSuturePlanHash);

	const FCarrierLabPhaseIIID5SuturePlanRecord* SuturePlan = nullptr;
	for (const FCarrierLabPhaseIIID5SuturePlanRecord& Candidate : SutureAudit.Plans)
	{
		if (Candidate.bTopologyValid && Candidate.bBoundaryTrackingReinitializable)
		{
			if (SuturePlan == nullptr)
			{
				SuturePlan = &Candidate;
			}
			else
			{
				++OutAudit.DeferredValidPlanCount;
			}
		}
	}

	if (SuturePlan == nullptr)
	{
		OutAudit.bNoPlanAvailable = true;
		OutAudit.TopologyMutationHash = HashToString(AuditHash);
		return true;
	}

	const FCarrierLabPhaseIIID4SlabBreakPlanRecord* SlabPlan = nullptr;
	for (const FCarrierLabPhaseIIID4SlabBreakPlanRecord& Candidate : SlabBreakAudit.Plans)
	{
		if (Candidate.PlanId == SuturePlan->SlabBreakPlanId &&
			Candidate.SourcePlateId == SuturePlan->SourcePlateId &&
			Candidate.DestinationPlateId == SuturePlan->DestinationPlateId &&
			Candidate.RemovalSetHash == SuturePlan->RemovalSetHash)
		{
			SlabPlan = &Candidate;
			break;
		}
	}
	if (SlabPlan == nullptr)
	{
		++OutAudit.MissingSlabBreakPlanCount;
		OutAudit.TopologyMutationHash = HashToString(AuditHash);
		return true;
	}
	if (!State.Plates.IsValidIndex(SuturePlan->SourcePlateId) ||
		!State.Plates.IsValidIndex(SuturePlan->DestinationPlateId))
	{
		OutAudit.TopologyMutationHash = HashToString(AuditHash);
		return true;
	}

	CarrierLab::FCarrierPlate SourceBefore = State.Plates[SuturePlan->SourcePlateId];
	CarrierLab::FCarrierPlate DestinationBefore = State.Plates[SuturePlan->DestinationPlateId];

	FCarrierLabPhaseIIID6TopologyMutationRecord& Record = OutAudit.Records.AddDefaulted_GetRef();
	Record.EventId = CurrentMetrics.EventCount + 1;
	Record.Step = CurrentMetrics.Step;
	Record.AppliedSuturePlanId = SuturePlan->PlanId;
	Record.AppliedSlabBreakPlanId = SlabPlan->PlanId;
	Record.GroupId = SuturePlan->GroupId;
	Record.PairKey = SuturePlan->PairKey;
	Record.SourcePlateId = SuturePlan->SourcePlateId;
	Record.DestinationPlateId = SuturePlan->DestinationPlateId;
	Record.SourceTriangleCountBefore = SourceBefore.LocalTriangles.Num();
	Record.SourceVertexCountBefore = SourceBefore.Vertices.Num();
	Record.DestinationTriangleCountBefore = DestinationBefore.LocalTriangles.Num();
	Record.DestinationVertexCountBefore = DestinationBefore.Vertices.Num();
	Record.RemovedTriangleCount = SlabPlan->RemovedTriangleCount;
	Record.AddedTriangleCount = SuturePlan->AddedTriangleCount;
	Record.RemovedVertexCount = SlabPlan->RemovedVertexCount;
	Record.AddedVertexCount = SuturePlan->AddedVertexCount;
	Record.DeferredValidPlanCount = OutAudit.DeferredValidPlanCount;
	Record.SlabBreakPlanHash = SlabPlan->PlanHash;
	Record.SuturePlanHash = SuturePlan->PlanHash;
	Record.bSlabBreakPlanValid = SlabPlan->bTopologyValid && !SlabPlan->bWouldDestroySourcePlate;
	Record.bSuturePlanValid = SuturePlan->bTopologyValid && SuturePlan->bBoundaryTrackingReinitializable;
	Record.bOneCollisionOnly = true;
	Record.SourceContinentalAreaBefore = ComputePlateLocalContinentalAreaWeight(SourceBefore);
	Record.DestinationContinentalAreaBefore = ComputePlateLocalContinentalAreaWeight(DestinationBefore);
	Record.TransferredContinentalArea = ComputePlateLocalContinentalAreaWeightForTriangles(
		SourceBefore,
		SlabPlan->RemovedLocalTriangleIds);

	CarrierLab::FCarrierPlate NewSource = SourceBefore;
	NewSource.Vertices.Reset();
	NewSource.LocalTriangles.Reset();
	NewSource.ActiveBoundaryTriangles.Reset();
	NewSource.ActiveBoundaryTriangleDistancesKm.Reset();
	NewSource.GlobalSampleIdToLocalVertexId.Reset();
	NewSource.Vertices.SetNum(SlabPlan->SurvivingVertexCount);
	for (int32 OldVertexId = 0; OldVertexId < SlabPlan->OldToNewLocalVertexIds.Num(); ++OldVertexId)
	{
		const int32 NewVertexId = SlabPlan->OldToNewLocalVertexIds[OldVertexId];
		if (NewVertexId == INDEX_NONE)
		{
			continue;
		}
		if (!SourceBefore.Vertices.IsValidIndex(OldVertexId) || !NewSource.Vertices.IsValidIndex(NewVertexId))
		{
			++Record.InvalidAppliedVertexCount;
			continue;
		}
		NewSource.Vertices[NewVertexId] = SourceBefore.Vertices[OldVertexId];
	}
	NewSource.LocalTriangles.SetNum(SlabPlan->SurvivingTriangleCount);
	for (int32 OldTriangleId = 0; OldTriangleId < SlabPlan->OldToNewLocalTriangleIds.Num(); ++OldTriangleId)
	{
		const int32 NewTriangleId = SlabPlan->OldToNewLocalTriangleIds[OldTriangleId];
		if (NewTriangleId == INDEX_NONE)
		{
			continue;
		}
		if (!SourceBefore.LocalTriangles.IsValidIndex(OldTriangleId) || !NewSource.LocalTriangles.IsValidIndex(NewTriangleId))
		{
			++Record.InvalidAppliedTriangleCount;
			continue;
		}
		CarrierLab::FCarrierPlateTriangle Triangle = SourceBefore.LocalTriangles[OldTriangleId];
		const int32 NewA = SlabPlan->OldToNewLocalVertexIds.IsValidIndex(Triangle.A) ? SlabPlan->OldToNewLocalVertexIds[Triangle.A] : INDEX_NONE;
		const int32 NewB = SlabPlan->OldToNewLocalVertexIds.IsValidIndex(Triangle.B) ? SlabPlan->OldToNewLocalVertexIds[Triangle.B] : INDEX_NONE;
		const int32 NewC = SlabPlan->OldToNewLocalVertexIds.IsValidIndex(Triangle.C) ? SlabPlan->OldToNewLocalVertexIds[Triangle.C] : INDEX_NONE;
		if (NewA == INDEX_NONE || NewB == INDEX_NONE || NewC == INDEX_NONE ||
			NewA == NewB || NewB == NewC || NewC == NewA)
		{
			++Record.InvalidAppliedTriangleCount;
			continue;
		}
		Triangle.A = NewA;
		Triangle.B = NewB;
		Triangle.C = NewC;
		NewSource.LocalTriangles[NewTriangleId] = Triangle;
	}

	CarrierLab::FCarrierPlate NewDestination = DestinationBefore;
	NewDestination.Vertices.SetNum(SuturePlan->PostSutureVertexCount);
	NewDestination.LocalTriangles.SetNum(SuturePlan->PostSutureTriangleCount);
	for (int32 SourceVertexId = 0; SourceVertexId < SuturePlan->SourceToDestinationAddedVertexIds.Num(); ++SourceVertexId)
	{
		const int32 DestinationVertexId = SuturePlan->SourceToDestinationAddedVertexIds[SourceVertexId];
		if (DestinationVertexId == INDEX_NONE)
		{
			continue;
		}
		if (!SourceBefore.Vertices.IsValidIndex(SourceVertexId) || !NewDestination.Vertices.IsValidIndex(DestinationVertexId))
		{
			++Record.InvalidAppliedVertexCount;
			continue;
		}
		NewDestination.Vertices[DestinationVertexId] = SourceBefore.Vertices[SourceVertexId];
		Record.MaxCopiedElevationDelta = FMath::Max(
			Record.MaxCopiedElevationDelta,
			FMath::Abs(NewDestination.Vertices[DestinationVertexId].Elevation - SourceBefore.Vertices[SourceVertexId].Elevation));
		Record.MaxCopiedHistoricalElevationDelta = FMath::Max(
			Record.MaxCopiedHistoricalElevationDelta,
			FMath::Abs(NewDestination.Vertices[DestinationVertexId].HistoricalElevation - SourceBefore.Vertices[SourceVertexId].HistoricalElevation));
	}
	for (const int32 SourceTriangleId : SuturePlan->AddedSourceLocalTriangleIds)
	{
		if (!SourceBefore.LocalTriangles.IsValidIndex(SourceTriangleId) ||
			!SuturePlan->SourceToDestinationAddedTriangleIds.IsValidIndex(SourceTriangleId))
		{
			++Record.InvalidAppliedTriangleCount;
			continue;
		}
		const int32 DestinationTriangleId = SuturePlan->SourceToDestinationAddedTriangleIds[SourceTriangleId];
		if (!NewDestination.LocalTriangles.IsValidIndex(DestinationTriangleId))
		{
			++Record.InvalidAppliedTriangleCount;
			continue;
		}

		CarrierLab::FCarrierPlateTriangle Triangle = SourceBefore.LocalTriangles[SourceTriangleId];
		const int32 NewA = SuturePlan->SourceToDestinationAddedVertexIds.IsValidIndex(Triangle.A) ? SuturePlan->SourceToDestinationAddedVertexIds[Triangle.A] : INDEX_NONE;
		const int32 NewB = SuturePlan->SourceToDestinationAddedVertexIds.IsValidIndex(Triangle.B) ? SuturePlan->SourceToDestinationAddedVertexIds[Triangle.B] : INDEX_NONE;
		const int32 NewC = SuturePlan->SourceToDestinationAddedVertexIds.IsValidIndex(Triangle.C) ? SuturePlan->SourceToDestinationAddedVertexIds[Triangle.C] : INDEX_NONE;
		if (NewA == INDEX_NONE || NewB == INDEX_NONE || NewC == INDEX_NONE ||
			NewA == NewB || NewB == NewC || NewC == NewA)
		{
			++Record.InvalidAppliedTriangleCount;
			continue;
		}
		Triangle.A = NewA;
		Triangle.B = NewB;
		Triangle.C = NewC;
		NewDestination.LocalTriangles[DestinationTriangleId] = Triangle;
	}

	RebuildPlateLookupLists(NewSource);
	RebuildPlateLookupLists(NewDestination);
	RebuildPlateBoundaryTrackingFromLocalTopology(
		NewSource,
		Record.SourceBoundaryTriangleCountAfter,
		Record.SourceNonManifoldEdgeCountAfter);
	RebuildPlateBoundaryTrackingFromLocalTopology(
		NewDestination,
		Record.DestinationBoundaryTriangleCountAfter,
		Record.DestinationNonManifoldEdgeCountAfter);

	Record.SourceTriangleCountAfter = NewSource.LocalTriangles.Num();
	Record.SourceVertexCountAfter = NewSource.Vertices.Num();
	Record.DestinationTriangleCountAfter = NewDestination.LocalTriangles.Num();
	Record.DestinationVertexCountAfter = NewDestination.Vertices.Num();
	Record.SourceContinentalAreaAfter = ComputePlateLocalContinentalAreaWeight(NewSource);
	Record.DestinationContinentalAreaAfter = ComputePlateLocalContinentalAreaWeight(NewDestination);
	Record.SourceContinentalAreaDelta = Record.SourceContinentalAreaAfter - Record.SourceContinentalAreaBefore;
	Record.DestinationContinentalAreaDelta = Record.DestinationContinentalAreaAfter - Record.DestinationContinentalAreaBefore;
	Record.ContinentalAreaResidual = Record.SourceContinentalAreaDelta + Record.DestinationContinentalAreaDelta;
	Record.SourceTopologyHashAfter = ComputePhaseIIID6PlateTopologyHash(
		TEXT("CarrierLab-IIID6-source-topology-after-v1"),
		NewSource,
		Record.SourceBoundaryTriangleCountAfter,
		Record.SourceNonManifoldEdgeCountAfter);
	Record.DestinationTopologyHashAfter = ComputePhaseIIID6PlateTopologyHash(
		TEXT("CarrierLab-IIID6-destination-topology-after-v1"),
		NewDestination,
		Record.DestinationBoundaryTriangleCountAfter,
		Record.DestinationNonManifoldEdgeCountAfter);

	Record.bSourceTopologyValidAfter =
		Record.InvalidAppliedTriangleCount == 0 &&
		Record.InvalidAppliedVertexCount == 0 &&
		Record.SourceNonManifoldEdgeCountAfter == 0 &&
		Record.SourceTriangleCountAfter + Record.RemovedTriangleCount == Record.SourceTriangleCountBefore &&
		Record.SourceVertexCountAfter + Record.RemovedVertexCount == Record.SourceVertexCountBefore;
	Record.bDestinationTopologyValidAfter =
		Record.InvalidAppliedTriangleCount == 0 &&
		Record.InvalidAppliedVertexCount == 0 &&
		Record.DestinationNonManifoldEdgeCountAfter == 0 &&
		Record.DestinationTriangleCountAfter == Record.DestinationTriangleCountBefore + Record.AddedTriangleCount &&
		Record.DestinationVertexCountAfter == Record.DestinationVertexCountBefore + Record.AddedVertexCount;
	Record.bBoundaryTrackingReinitialized =
		Record.SourceBoundaryTriangleCountAfter > 0 &&
		Record.DestinationBoundaryTriangleCountAfter > 0 &&
		NewSource.ActiveBoundaryTriangles.Num() == NewSource.ActiveBoundaryTriangleDistancesKm.Num() &&
		NewDestination.ActiveBoundaryTriangles.Num() == NewDestination.ActiveBoundaryTriangleDistancesKm.Num();
	Record.bNoUpliftApplied =
		Record.MaxCopiedElevationDelta <= 1.0e-12 &&
		Record.MaxCopiedHistoricalElevationDelta <= 1.0e-12;

	if (!Record.bSlabBreakPlanValid ||
		!Record.bSuturePlanValid ||
		!Record.bSourceTopologyValidAfter ||
		!Record.bDestinationTopologyValidAfter ||
		!Record.bBoundaryTrackingReinitialized ||
		FMath::Abs(Record.ContinentalAreaResidual) > 1.0e-9)
	{
		uint64 FailureHash = 1469598103934665603ull;
		HashMixString(FailureHash, TEXT("CarrierLab-IIID6-rejected-mutation-v1"));
		HashMixString(FailureHash, Record.SlabBreakPlanHash);
		HashMixString(FailureHash, Record.SuturePlanHash);
		Record.MutationHash = HashToString(FailureHash);
		HashMixString(AuditHash, Record.MutationHash);
		OutAudit.TopologyMutationHash = HashToString(AuditHash);
		return true;
	}

	Record.InvalidatedMatrixPairCount = State.ConvergenceSubductionMatrixPairKeys.Num();
	Record.InvalidatedPolarityDecisionCount = State.ConvergenceSubductionPolarityDecisions.Num();
	Record.InvalidatedTriangleHitCount = State.ConvergenceSubductionTriangleHits.Num();
	Record.InvalidatedMatrixEvidenceCount = State.ConvergenceSubductionMatrixEvidence.Num();
	Record.InvalidatedSubductingMarkCount = State.ConvergenceSubductingTriangleMarks.Num();

	State.Plates[SuturePlan->SourcePlateId] = MoveTemp(NewSource);
	State.Plates[SuturePlan->DestinationPlateId] = MoveTemp(NewDestination);
	for (CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		if (Plate.PlateId == SuturePlan->SourcePlateId || Plate.PlateId == SuturePlan->DestinationPlateId)
		{
			continue;
		}
		Plate.ActiveBoundaryTriangles.Reset();
		Plate.ActiveBoundaryTriangleDistancesKm.Reset();
		for (int32 LocalTriangleId = 0; LocalTriangleId < Plate.LocalTriangles.Num(); ++LocalTriangleId)
		{
			if (Plate.LocalTriangles[LocalTriangleId].bBoundary)
			{
				Plate.ActiveBoundaryTriangles.Add(LocalTriangleId);
				Plate.ActiveBoundaryTriangleDistancesKm.Add(0.0);
			}
		}
	}
	State.ConvergenceSubductionMatrixPairKeys.Reset();
	State.ConvergenceSubductionPolarityDecisions.Reset();
	State.ConvergenceSubductionTriangleHits.Reset();
	State.ConvergenceSubductionMatrixEvidence.Reset();
	State.ConvergenceSubductingTriangleMarks.Reset();
	State.ConvergenceObductionTriangleMarks.Reset();
	State.ConvergenceCollisionPendingTriangleKeys.Reset();
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
	State.ConvergenceObductionTriangleMarkDuplicateCount = 0;
	State.ConvergenceObductionTriangleMarkInvalidCount = 0;
	State.ConvergenceHistoricalElevationSnapshotCount = 0;
	State.ConvergenceHistoricalElevationSnapshotVertexCount = 0;
	State.ConvergenceHistoricalElevationDuplicateSnapshotCount = 0;
	State.ConvergenceHistoricalElevationInvalidSnapshotCount = 0;
	++State.ConvergenceTrackingResetSerial;
	Record.bSubductionTrackingInvalidated =
		State.ConvergenceSubductionMatrixPairKeys.IsEmpty() &&
		State.ConvergenceSubductionPolarityDecisions.IsEmpty() &&
		State.ConvergenceSubductionTriangleHits.IsEmpty() &&
		State.ConvergenceSubductionMatrixEvidence.IsEmpty() &&
		State.ConvergenceSubductingTriangleMarks.IsEmpty() &&
		State.ConvergenceObductionTriangleMarks.IsEmpty() &&
		State.ConvergenceCollisionPendingTriangleKeys.IsEmpty();

	++CurrentMetrics.EventCount;
	OutAudit.EventCountAfter = CurrentMetrics.EventCount;
	OutAudit.ResetSerialAfter = State.ConvergenceTrackingResetSerial;
	Record.bApplied = true;
	OutAudit.AppliedMutationCount = 1;
	OutAudit.bMutationAttempted = true;
	OutAudit.bMutationApplied = true;
	OutAudit.bTopologyMutated = true;
	OutAudit.bOneCollisionOnly = true;
	bPlateRayMeshTopologyDirty = true;
	bProjectionRayMeshTopologyDirty = true;
	bRenderMeshTopologyDirty = true;
	CaptureDriftReference();
	ProjectCurrentCarrier();

	uint64 MutationHash = 1469598103934665603ull;
	HashMixString(MutationHash, TEXT("CarrierLab-IIID6-topology-mutation-record-v1"));
	HashMix(MutationHash, static_cast<uint64>(Record.EventId + 1));
	HashMix(MutationHash, static_cast<uint64>(Record.Step + 1));
	HashMix(MutationHash, static_cast<uint64>(Record.SourcePlateId + 1));
	HashMix(MutationHash, static_cast<uint64>(Record.DestinationPlateId + 1));
	HashMix(MutationHash, static_cast<uint64>(Record.RemovedTriangleCount + 1));
	HashMix(MutationHash, static_cast<uint64>(Record.AddedTriangleCount + 1));
	HashMix(MutationHash, static_cast<uint64>(Record.RemovedVertexCount + 1));
	HashMix(MutationHash, static_cast<uint64>(Record.AddedVertexCount + 1));
	HashMixDouble(MutationHash, Record.SourceContinentalAreaDelta);
	HashMixDouble(MutationHash, Record.DestinationContinentalAreaDelta);
	HashMixDouble(MutationHash, Record.TransferredContinentalArea);
	HashMixDouble(MutationHash, Record.ContinentalAreaResidual);
	HashMix(MutationHash, static_cast<uint64>(Record.InvalidatedMatrixPairCount + 1));
	HashMix(MutationHash, static_cast<uint64>(Record.InvalidatedPolarityDecisionCount + 1));
	HashMix(MutationHash, static_cast<uint64>(Record.InvalidatedTriangleHitCount + 1));
	HashMix(MutationHash, static_cast<uint64>(Record.InvalidatedMatrixEvidenceCount + 1));
	HashMix(MutationHash, static_cast<uint64>(Record.InvalidatedSubductingMarkCount + 1));
	HashMix(MutationHash, Record.bApplied ? 1ull : 0ull);
	HashMix(MutationHash, Record.bOneCollisionOnly ? 1ull : 0ull);
	HashMix(MutationHash, Record.bSourceTopologyValidAfter ? 1ull : 0ull);
	HashMix(MutationHash, Record.bDestinationTopologyValidAfter ? 1ull : 0ull);
	HashMix(MutationHash, Record.bBoundaryTrackingReinitialized ? 1ull : 0ull);
	HashMix(MutationHash, Record.bSubductionTrackingInvalidated ? 1ull : 0ull);
	HashMix(MutationHash, Record.bNoUpliftApplied ? 1ull : 0ull);
	HashMixString(MutationHash, Record.SlabBreakPlanHash);
	HashMixString(MutationHash, Record.SuturePlanHash);
	HashMixString(MutationHash, Record.SourceTopologyHashAfter);
	HashMixString(MutationHash, Record.DestinationTopologyHashAfter);
	Record.MutationHash = HashToString(MutationHash);

	HashMixString(AuditHash, Record.MutationHash);
	HashMix(AuditHash, static_cast<uint64>(OutAudit.EventCountAfter + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.ResetSerialAfter + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.AppliedMutationCount + 1));
	HashMix(AuditHash, static_cast<uint64>(OutAudit.DeferredValidPlanCount + 1));
	OutAudit.TopologyMutationHash = HashToString(AuditHash);
	return true;
}

bool ACarrierLabVisualizationActor::PlanPhaseIIID7CollisionUplift(
	FCarrierLabPhaseIIID7CollisionUpliftAudit& OutAudit,
	const double InterpenetrationThresholdKm,
	const double DestinationMassThresholdRatio) const
{
	++PhaseIIIDiagnosticCallCounts.UpliftPlan;
	if (!bInitialized)
	{
		return false;
	}

	FCarrierLabPhaseIIID1TerraneAudit TerraneAudit;
	if (!DetectPhaseIIID1ConnectedTerranes(TerraneAudit))
	{
		return false;
	}
	FCarrierLabPhaseIIID2CollisionGroupingAudit GroupingAudit;
	if (!BuildPhaseIIID2CollisionGroupsFromTerranes(TerraneAudit, GroupingAudit, InterpenetrationThresholdKm))
	{
		return false;
	}
	FCarrierLabPhaseIIID3DestinationMassAudit DestinationMassAudit;
	if (!BuildPhaseIIID3DestinationMassFromInputs(
		TerraneAudit,
		GroupingAudit,
		DestinationMassAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio))
	{
		return false;
	}
	FCarrierLabPhaseIIID4SlabBreakPlanAudit SlabBreakAudit;
	if (!BuildPhaseIIID4SlabBreakFromInputs(
		TerraneAudit,
		DestinationMassAudit,
		SlabBreakAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio))
	{
		return false;
	}
	FCarrierLabPhaseIIID5SuturePlanAudit SutureAudit;
	if (!BuildPhaseIIID5SutureFromSlabBreak(
		SlabBreakAudit,
		SutureAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio))
	{
		return false;
	}
	return BuildPhaseIIID7CollisionUpliftFromPlans(
		GroupingAudit,
		SutureAudit,
		OutAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio);
}

bool ACarrierLabVisualizationActor::VerifyPhaseIIID7InputPipelineEquivalence(
	FCarrierLabPhaseIIID7InputPipelineEquivalenceAudit& OutAudit,
	const double InterpenetrationThresholdKm,
	const double DestinationMassThresholdRatio) const
{
	const FCarrierLabPhaseIIIDiagnosticCallCounts SavedDiagnostics = PhaseIIIDiagnosticCallCounts;
	OutAudit = FCarrierLabPhaseIIID7InputPipelineEquivalenceAudit();
	OutAudit.InterpenetrationThresholdKm = InterpenetrationThresholdKm;
	OutAudit.DestinationMassThresholdRatio = DestinationMassThresholdRatio;
	if (!bInitialized)
	{
		PhaseIIIDiagnosticCallCounts = SavedDiagnostics;
		return false;
	}

	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCount = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerial = State.ConvergenceTrackingResetSerial;

	FCarrierLabPhaseIIID1TerraneAudit TerraneAudit;
	OutAudit.bTerraneBuilt = DetectPhaseIIID1ConnectedTerranes(TerraneAudit);
	if (!OutAudit.bTerraneBuilt)
	{
		PhaseIIIDiagnosticCallCounts = SavedDiagnostics;
		return false;
	}
	OutAudit.TerraneDetectionHash = TerraneAudit.TerraneDetectionHash;

	FCarrierLabPhaseIIID2CollisionGroupingAudit GroupingAudit;
	OutAudit.bGroupingBuilt = BuildPhaseIIID2CollisionGroupsFromTerranes(
		TerraneAudit,
		GroupingAudit,
		InterpenetrationThresholdKm);
	if (!OutAudit.bGroupingBuilt)
	{
		PhaseIIIDiagnosticCallCounts = SavedDiagnostics;
		return false;
	}
	OutAudit.GroupingHash = GroupingAudit.GroupingHash;

	FCarrierLabPhaseIIID3DestinationMassAudit CachedDestinationMassAudit;
	FCarrierLabPhaseIIID4SlabBreakPlanAudit CachedSlabBreakAudit;
	FCarrierLabPhaseIIID5SuturePlanAudit CachedSutureAudit;
	const double CachedStartSeconds = FPlatformTime::Seconds();
	const bool bCachedD3 = BuildPhaseIIID3DestinationMassFromInputs(
		TerraneAudit,
		GroupingAudit,
		CachedDestinationMassAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio,
		true);
	const bool bCachedD4 = bCachedD3 && BuildPhaseIIID4SlabBreakFromInputs(
		TerraneAudit,
		CachedDestinationMassAudit,
		CachedSlabBreakAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio);
	const bool bCachedD5 = bCachedD4 && BuildPhaseIIID5SutureFromSlabBreak(
		CachedSlabBreakAudit,
		CachedSutureAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio);
	OutAudit.CachedPipelineSeconds = FPlatformTime::Seconds() - CachedStartSeconds;
	OutAudit.bCachedPipelineBuilt = bCachedD3 && bCachedD4 && bCachedD5;
	OutAudit.CachedDestinationMassHash = CachedDestinationMassAudit.DestinationMassHash;
	OutAudit.CachedSlabBreakPlanHash = CachedSlabBreakAudit.SlabBreakPlanHash;
	OutAudit.CachedSuturePlanHash = CachedSutureAudit.SuturePlanHash;

	FCarrierLabPhaseIIID3DestinationMassAudit UncachedDestinationMassAudit;
	FCarrierLabPhaseIIID4SlabBreakPlanAudit UncachedSlabBreakAudit;
	FCarrierLabPhaseIIID5SuturePlanAudit UncachedSutureAudit;
	const double UncachedStartSeconds = FPlatformTime::Seconds();
	const bool bUncachedD3 = BuildPhaseIIID3DestinationMassFromInputs(
		TerraneAudit,
		GroupingAudit,
		UncachedDestinationMassAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio,
		false);
	const bool bUncachedD4 = bUncachedD3 && BuildPhaseIIID4SlabBreakFromInputs(
		TerraneAudit,
		UncachedDestinationMassAudit,
		UncachedSlabBreakAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio);
	const bool bUncachedD5 = bUncachedD4 && BuildPhaseIIID5SutureFromSlabBreak(
		UncachedSlabBreakAudit,
		UncachedSutureAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio);
	OutAudit.UncachedPipelineSeconds = FPlatformTime::Seconds() - UncachedStartSeconds;
	OutAudit.bUncachedPipelineBuilt = bUncachedD3 && bUncachedD4 && bUncachedD5;
	OutAudit.UncachedDestinationMassHash = UncachedDestinationMassAudit.DestinationMassHash;
	OutAudit.UncachedSlabBreakPlanHash = UncachedSlabBreakAudit.SlabBreakPlanHash;
	OutAudit.UncachedSuturePlanHash = UncachedSutureAudit.SuturePlanHash;
	OutAudit.bPassed =
		OutAudit.bCachedPipelineBuilt &&
		OutAudit.bUncachedPipelineBuilt &&
		OutAudit.CachedDestinationMassHash == OutAudit.UncachedDestinationMassHash &&
		OutAudit.CachedSlabBreakPlanHash == OutAudit.UncachedSlabBreakPlanHash &&
		OutAudit.CachedSuturePlanHash == OutAudit.UncachedSuturePlanHash;

	PhaseIIIDiagnosticCallCounts = SavedDiagnostics;
	return OutAudit.bPassed;
}

bool ACarrierLabVisualizationActor::BuildPhaseIIID7CollisionUpliftFromPlans(
	const FCarrierLabPhaseIIID2CollisionGroupingAudit& GroupingAudit,
	const FCarrierLabPhaseIIID5SuturePlanAudit& SutureAudit,
	FCarrierLabPhaseIIID7CollisionUpliftAudit& OutAudit,
	const double InterpenetrationThresholdKm,
	const double DestinationMassThresholdRatio) const
{
	OutAudit = FCarrierLabPhaseIIID7CollisionUpliftAudit();
	if (!bInitialized)
	{
		return false;
	}
	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCountBefore = CurrentMetrics.EventCount;
	OutAudit.EventCountAfter = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerialBefore = State.ConvergenceTrackingResetSerial;
	OutAudit.ResetSerialAfter = State.ConvergenceTrackingResetSerial;
	OutAudit.InterpenetrationThresholdKm = InterpenetrationThresholdKm;
	OutAudit.DestinationMassThresholdRatio = DestinationMassThresholdRatio;
	OutAudit.CollisionRadiusConstantKm = PhaseIIIDCollisionRadiusKm;
	OutAudit.CollisionCoefficientPerKm = PhaseIIIDCollisionCoefficientPerKm;
	OutAudit.ReferenceVelocityMmPerYear = PhaseIIICReferenceVelocityMmPerYear;
	OutAudit.PlanetRadiusKm = EarthRadiusKm;
	OutAudit.VisibleElevationHash = CurrentMetrics.VisibleElevationHash;
	OutAudit.CrustStateHash = CurrentMetrics.CrustStateHash;
	OutAudit.bPlannedOnly = true;

	const FCarrierLabPhaseIIID5SuturePlanRecord* SuturePlan = nullptr;
	for (const FCarrierLabPhaseIIID5SuturePlanRecord& Candidate : SutureAudit.Plans)
	{
		if (Candidate.bTopologyValid && Candidate.bBoundaryTrackingReinitializable)
		{
			SuturePlan = &Candidate;
			break;
		}
	}
	if (SuturePlan == nullptr ||
		!State.Plates.IsValidIndex(SuturePlan->SourcePlateId) ||
		!State.Plates.IsValidIndex(SuturePlan->DestinationPlateId))
	{
		OutAudit.bNoUpliftAvailable = true;
		OutAudit.UpliftHash = TEXT("0000000000000000");
		return true;
	}
	OutAudit.SourceSuturePlanHash = SuturePlan->PlanHash;

	const FCarrierLabPhaseIIID2CollisionGroupRecord* GroupRecord = nullptr;
	for (const FCarrierLabPhaseIIID2CollisionGroupRecord& Candidate : GroupingAudit.Groups)
	{
		if (Candidate.GroupId == SuturePlan->GroupId &&
			Candidate.PairKey == SuturePlan->PairKey)
		{
			GroupRecord = &Candidate;
			break;
		}
	}
	if (GroupRecord == nullptr)
	{
		++OutAudit.InvalidInputCount;
		OutAudit.UpliftHash = TEXT("0000000000000000");
		return true;
	}

	const CarrierLab::FCarrierPlate& SourcePlate = State.Plates[SuturePlan->SourcePlateId];
	const CarrierLab::FCarrierPlate& DestinationPlate = State.Plates[SuturePlan->DestinationPlateId];
	const double ReferencePlateAreaWeight = State.Plates.Num() > 0
		? 4.0 * UE_DOUBLE_PI / static_cast<double>(State.Plates.Num())
		: 0.0;
	OutAudit.TerraneAreaWeight = SuturePlan->AddedAreaWeight;
	OutAudit.TerraneAreaKm2 = SuturePlan->AddedAreaWeight * EarthRadiusKm * EarthRadiusKm;
	OutAudit.ReferencePlateAreaKm2 = ReferencePlateAreaWeight * EarthRadiusKm * EarthRadiusKm;
	OutAudit.RelativeVelocityMmPerYear = FMath::Max(0.0, GroupRecord->MaxSignedConvergenceVelocity) * EarthRadiusKm / DeltaTimeMa;
	OutAudit.InfluenceRadiusKm = PhaseIIID7InfluenceRadiusKm(
		PhaseIIIDCollisionRadiusKm,
		OutAudit.RelativeVelocityMmPerYear,
		PhaseIIICReferenceVelocityMmPerYear,
		OutAudit.TerraneAreaKm2,
		OutAudit.ReferencePlateAreaKm2);
	OutAudit.CenterExpectedDeltaKm = PhaseIIID7CollisionDeltaKm(
		PhaseIIIDCollisionCoefficientPerKm,
		OutAudit.TerraneAreaKm2,
		1.0);

	TArray<FVector3d> TerranePoints;
	FVector3d WeightedCentroid = FVector3d::ZeroVector;
	for (const int32 SourceLocalTriangleId : SuturePlan->AddedSourceLocalTriangleIds)
	{
		if (!SourcePlate.LocalTriangles.IsValidIndex(SourceLocalTriangleId))
		{
			++OutAudit.InvalidInputCount;
			continue;
		}
		const CarrierLab::FCarrierPlateTriangle& Triangle = SourcePlate.LocalTriangles[SourceLocalTriangleId];
		const int32 VertexIds[3] = { Triangle.A, Triangle.B, Triangle.C };
		bool bValidTriangle = true;
		FVector3d TriangleSum = FVector3d::ZeroVector;
		for (const int32 VertexId : VertexIds)
		{
			if (!SourcePlate.Vertices.IsValidIndex(VertexId))
			{
				bValidTriangle = false;
				break;
			}
			TerranePoints.Add(SourcePlate.Vertices[VertexId].UnitPosition);
			TriangleSum += SourcePlate.Vertices[VertexId].UnitPosition;
		}
		if (!bValidTriangle)
		{
			++OutAudit.InvalidInputCount;
			continue;
		}
		const FVector3d Barycenter = NormalizeOrFallback(TriangleSum, SourcePlate.Vertices[VertexIds[0]].UnitPosition);
		TerranePoints.Add(Barycenter);
		const double TriangleArea = ComputePlateLocalTriangleAreaWeight(SourcePlate, SourceLocalTriangleId);
		WeightedCentroid += Barycenter * TriangleArea;
	}
	OutAudit.TerraneCentroid = NormalizeOrFallback(
		WeightedCentroid,
		!TerranePoints.IsEmpty() ? TerranePoints[0] : FVector3d::UnitZ());

	if (OutAudit.InfluenceRadiusKm <= UE_DOUBLE_SMALL_NUMBER ||
		OutAudit.CenterExpectedDeltaKm <= 0.0 ||
		TerranePoints.IsEmpty())
	{
		OutAudit.bNoUpliftAvailable = true;
		OutAudit.UpliftHash = TEXT("0000000000000000");
		return true;
	}

	TSet<uint64> UpliftedVertexKeys;
	uint64 UpliftHash = 1469598103934665603ull;
	HashMixString(UpliftHash, TEXT("CarrierLab-IIID7-collision-uplift-plan-v1"));
	HashMix(UpliftHash, static_cast<uint64>(OutAudit.Step + 1));
	HashMix(UpliftHash, static_cast<uint64>(OutAudit.EventCountBefore + 1));
	HashMix(UpliftHash, static_cast<uint64>(SuturePlan->SourcePlateId + 1));
	HashMix(UpliftHash, static_cast<uint64>(SuturePlan->DestinationPlateId + 1));
	HashMixDouble(UpliftHash, OutAudit.CollisionRadiusConstantKm);
	HashMixDouble(UpliftHash, OutAudit.CollisionCoefficientPerKm);
	HashMixDouble(UpliftHash, OutAudit.TerraneAreaKm2);
	HashMixDouble(UpliftHash, OutAudit.ReferencePlateAreaKm2);
	HashMixDouble(UpliftHash, OutAudit.RelativeVelocityMmPerYear);
	HashMixDouble(UpliftHash, OutAudit.InfluenceRadiusKm);

	auto AddCandidateVertex = [&](
		const int32 DestinationVertexId,
		const CarrierLab::FCarrierVertex& Vertex)
	{
		++OutAudit.CandidateVertexCount;
		if (Vertex.ContinentalFraction <= 0.5)
		{
			++OutAudit.SkippedNonContinentalVertexCount;
			return;
		}

		double BestDistanceKm = TNumericLimits<double>::Max();
		for (const FVector3d& TerranePoint : TerranePoints)
		{
			const double Dot = FMath::Clamp(FVector3d::DotProduct(Vertex.UnitPosition, TerranePoint), -1.0, 1.0);
			BestDistanceKm = FMath::Min(BestDistanceKm, FMath::Acos(Dot) * EarthRadiusKm);
		}
		if (!FMath::IsFinite(BestDistanceKm) || BestDistanceKm > OutAudit.InfluenceRadiusKm)
		{
			++OutAudit.SkippedOutsideRadiusCount;
			return;
		}

		const double DistanceTransfer = PhaseIIID7CollisionDistanceTransfer(BestDistanceKm, OutAudit.InfluenceRadiusKm);
		const double DeltaKm = PhaseIIID7CollisionDeltaKm(
			PhaseIIIDCollisionCoefficientPerKm,
			OutAudit.TerraneAreaKm2,
			DistanceTransfer);
		if (!FMath::IsFinite(DeltaKm) || DeltaKm <= 0.0)
		{
			++OutAudit.InvalidInputCount;
			return;
		}

		const FVector3d DirectionFromCentroid = Vertex.UnitPosition - OutAudit.TerraneCentroid;
		const FVector3d UnitDirectionFromCentroid = DirectionFromCentroid.Size() > UE_DOUBLE_SMALL_NUMBER
			? DirectionFromCentroid / DirectionFromCentroid.Size()
			: MakeDeterministicTangent(Vertex.UnitPosition);
		const FVector3d ExpectedFoldDirection = RetangentAndNormalizeVectorField(
			FVector3d::CrossProduct(FVector3d::CrossProduct(Vertex.UnitPosition, UnitDirectionFromCentroid), Vertex.UnitPosition),
			Vertex.UnitPosition);

		FCarrierLabPhaseIIID7CollisionUpliftRecord& Record = OutAudit.Records.AddDefaulted_GetRef();
		Record.RecordId = OutAudit.Records.Num() - 1;
		Record.EventId = CurrentMetrics.EventCount + 1;
		Record.Step = CurrentMetrics.Step;
		Record.SourcePlateId = SuturePlan->SourcePlateId;
		Record.DestinationPlateId = SuturePlan->DestinationPlateId;
		Record.DestinationLocalVertexId = DestinationVertexId;
		Record.GlobalSampleId = Vertex.GlobalSampleId;
		Record.TerraneAreaWeight = OutAudit.TerraneAreaWeight;
		Record.TerraneAreaKm2 = OutAudit.TerraneAreaKm2;
		Record.ReferencePlateAreaKm2 = OutAudit.ReferencePlateAreaKm2;
		Record.RelativeVelocityMmPerYear = OutAudit.RelativeVelocityMmPerYear;
		Record.InfluenceRadiusKm = OutAudit.InfluenceRadiusKm;
		Record.DistanceToTerraneKm = BestDistanceKm;
		Record.DistanceTransfer = DistanceTransfer;
		Record.PreviousElevationKm = Vertex.Elevation;
		Record.AppliedDeltaKm = DeltaKm;
		Record.NewElevationKm = Vertex.Elevation + DeltaKm;
		Record.PreviousFoldMagnitude = Vertex.FoldDirection.Size();
		Record.NewFoldMagnitude = ExpectedFoldDirection.Size();
		Record.VertexUnitPosition = Vertex.UnitPosition;
		Record.TerraneCentroid = OutAudit.TerraneCentroid;
		Record.ExpectedFoldDirection = ExpectedFoldDirection;
		Record.bApplied = false;

		const uint64 VertexKey = MakePlateVertexKey(SuturePlan->DestinationPlateId, DestinationVertexId);
		UpliftedVertexKeys.Add(VertexKey);
		++OutAudit.UpliftRecordCount;
		OutAudit.TotalAppliedDeltaKm += DeltaKm;
		OutAudit.MaxAppliedDeltaKm = FMath::Max(OutAudit.MaxAppliedDeltaKm, DeltaKm);
		if (BestDistanceKm <= UE_DOUBLE_SMALL_NUMBER)
		{
			OutAudit.CenterAppliedDeltaKm = FMath::Max(OutAudit.CenterAppliedDeltaKm, DeltaKm);
		}

		HashMix(UpliftHash, static_cast<uint64>(Record.RecordId + 1));
		HashMix(UpliftHash, static_cast<uint64>(Record.DestinationLocalVertexId + 1));
		HashMix(UpliftHash, static_cast<uint64>(Record.GlobalSampleId + 1));
		HashMixDouble(UpliftHash, Record.DistanceToTerraneKm);
		HashMixDouble(UpliftHash, Record.DistanceTransfer);
		HashMixDouble(UpliftHash, Record.PreviousElevationKm);
		HashMixDouble(UpliftHash, Record.AppliedDeltaKm);
		HashMixDouble(UpliftHash, Record.NewElevationKm);
		HashMixDouble(UpliftHash, Record.ExpectedFoldDirection.X);
		HashMixDouble(UpliftHash, Record.ExpectedFoldDirection.Y);
		HashMixDouble(UpliftHash, Record.ExpectedFoldDirection.Z);
	};

	for (int32 DestinationVertexId = 0; DestinationVertexId < DestinationPlate.Vertices.Num(); ++DestinationVertexId)
	{
		AddCandidateVertex(DestinationVertexId, DestinationPlate.Vertices[DestinationVertexId]);
	}
	for (int32 SourceVertexId = 0; SourceVertexId < SuturePlan->SourceToDestinationAddedVertexIds.Num(); ++SourceVertexId)
	{
		const int32 DestinationVertexId = SuturePlan->SourceToDestinationAddedVertexIds[SourceVertexId];
		if (DestinationVertexId == INDEX_NONE)
		{
			continue;
		}
		if (!SourcePlate.Vertices.IsValidIndex(SourceVertexId))
		{
			++OutAudit.InvalidInputCount;
			continue;
		}
		AddCandidateVertex(DestinationVertexId, SourcePlate.Vertices[SourceVertexId]);
	}

	OutAudit.UniqueUpliftedVertexCount = UpliftedVertexKeys.Num();
	HashMix(UpliftHash, static_cast<uint64>(OutAudit.CandidateVertexCount + 1));
	HashMix(UpliftHash, static_cast<uint64>(OutAudit.UpliftRecordCount + 1));
	HashMix(UpliftHash, static_cast<uint64>(OutAudit.UniqueUpliftedVertexCount + 1));
	HashMixDouble(UpliftHash, OutAudit.TotalAppliedDeltaKm);
	HashMixDouble(UpliftHash, OutAudit.MaxAppliedDeltaKm);
	OutAudit.UpliftHash = HashToString(UpliftHash);
	return true;
}

bool ACarrierLabVisualizationActor::ApplyPhaseIIID7CollisionUplift(
	FCarrierLabPhaseIIID7CollisionUpliftAudit& OutAudit,
	const double InterpenetrationThresholdKm,
	const double DestinationMassThresholdRatio)
{
	const double ApplyStartSeconds = FPlatformTime::Seconds();
	++PhaseIIIDiagnosticCallCounts.UpliftApply;
	if (!bInitialized && !InitializeCarrier())
	{
		PhaseIIIDiagnosticCallCounts.D7ApplyTotalSeconds += FPlatformTime::Seconds() - ApplyStartSeconds;
		return false;
	}

	const double InputPipelineStartSeconds = FPlatformTime::Seconds();
	FCarrierLabPhaseIIID1TerraneAudit TerraneAudit;
	if (!DetectPhaseIIID1ConnectedTerranes(TerraneAudit))
	{
		PhaseIIIDiagnosticCallCounts.D7InputPipelineSeconds += FPlatformTime::Seconds() - InputPipelineStartSeconds;
		PhaseIIIDiagnosticCallCounts.D7ApplyTotalSeconds += FPlatformTime::Seconds() - ApplyStartSeconds;
		return false;
	}
	FCarrierLabPhaseIIID2CollisionGroupingAudit GroupingAudit;
	double StageStartSeconds = FPlatformTime::Seconds();
	if (!BuildPhaseIIID2CollisionGroupsFromTerranes(TerraneAudit, GroupingAudit, InterpenetrationThresholdKm))
	{
		PhaseIIIDiagnosticCallCounts.D7D2GroupingSeconds += FPlatformTime::Seconds() - StageStartSeconds;
		PhaseIIIDiagnosticCallCounts.D7InputPipelineSeconds += FPlatformTime::Seconds() - InputPipelineStartSeconds;
		PhaseIIIDiagnosticCallCounts.D7ApplyTotalSeconds += FPlatformTime::Seconds() - ApplyStartSeconds;
		return false;
	}
	PhaseIIIDiagnosticCallCounts.D7D2GroupingSeconds += FPlatformTime::Seconds() - StageStartSeconds;
	FCarrierLabPhaseIIID3DestinationMassAudit DestinationMassAudit;
	StageStartSeconds = FPlatformTime::Seconds();
	if (!BuildPhaseIIID3DestinationMassFromInputs(
		TerraneAudit,
		GroupingAudit,
		DestinationMassAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio))
	{
		PhaseIIIDiagnosticCallCounts.D7D3DestinationMassSeconds += FPlatformTime::Seconds() - StageStartSeconds;
		PhaseIIIDiagnosticCallCounts.D7InputPipelineSeconds += FPlatformTime::Seconds() - InputPipelineStartSeconds;
		PhaseIIIDiagnosticCallCounts.D7ApplyTotalSeconds += FPlatformTime::Seconds() - ApplyStartSeconds;
		return false;
	}
	PhaseIIIDiagnosticCallCounts.D7D3DestinationMassSeconds += FPlatformTime::Seconds() - StageStartSeconds;
	FCarrierLabPhaseIIID4SlabBreakPlanAudit SlabBreakAudit;
	StageStartSeconds = FPlatformTime::Seconds();
	if (!BuildPhaseIIID4SlabBreakFromInputs(
		TerraneAudit,
		DestinationMassAudit,
		SlabBreakAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio))
	{
		PhaseIIIDiagnosticCallCounts.D7D4SlabBreakSeconds += FPlatformTime::Seconds() - StageStartSeconds;
		PhaseIIIDiagnosticCallCounts.D7InputPipelineSeconds += FPlatformTime::Seconds() - InputPipelineStartSeconds;
		PhaseIIIDiagnosticCallCounts.D7ApplyTotalSeconds += FPlatformTime::Seconds() - ApplyStartSeconds;
		return false;
	}
	PhaseIIIDiagnosticCallCounts.D7D4SlabBreakSeconds += FPlatformTime::Seconds() - StageStartSeconds;
	FCarrierLabPhaseIIID5SuturePlanAudit SutureAudit;
	StageStartSeconds = FPlatformTime::Seconds();
	if (!BuildPhaseIIID5SutureFromSlabBreak(
		SlabBreakAudit,
		SutureAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio))
	{
		PhaseIIIDiagnosticCallCounts.D7D5SutureSeconds += FPlatformTime::Seconds() - StageStartSeconds;
		PhaseIIIDiagnosticCallCounts.D7InputPipelineSeconds += FPlatformTime::Seconds() - InputPipelineStartSeconds;
		PhaseIIIDiagnosticCallCounts.D7ApplyTotalSeconds += FPlatformTime::Seconds() - ApplyStartSeconds;
		return false;
	}
	PhaseIIIDiagnosticCallCounts.D7D5SutureSeconds += FPlatformTime::Seconds() - StageStartSeconds;
	PhaseIIIDiagnosticCallCounts.D7InputPipelineSeconds += FPlatformTime::Seconds() - InputPipelineStartSeconds;

	FCarrierLabPhaseIIID7CollisionUpliftAudit PlannedAudit;
	const double UpliftPlanStartSeconds = FPlatformTime::Seconds();
	if (!BuildPhaseIIID7CollisionUpliftFromPlans(
		GroupingAudit,
		SutureAudit,
		PlannedAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio))
	{
		PhaseIIIDiagnosticCallCounts.D7UpliftPlanSeconds += FPlatformTime::Seconds() - UpliftPlanStartSeconds;
		PhaseIIIDiagnosticCallCounts.D7ApplyTotalSeconds += FPlatformTime::Seconds() - ApplyStartSeconds;
		return false;
	}
	PhaseIIIDiagnosticCallCounts.D7UpliftPlanSeconds += FPlatformTime::Seconds() - UpliftPlanStartSeconds;
	PhaseIIIDiagnosticCallCounts.D7PlannedRecordCount += PlannedAudit.Records.Num();

	const bool bApplied = ApplyPhaseIIID7CollisionUpliftFromPlan(
		PlannedAudit,
		SlabBreakAudit,
		SutureAudit,
		OutAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio);
	PhaseIIIDiagnosticCallCounts.D7ApplyTotalSeconds += FPlatformTime::Seconds() - ApplyStartSeconds;
	return bApplied;
}

bool ACarrierLabVisualizationActor::ApplyPhaseIIID7CollisionUpliftFromPlan(
	const FCarrierLabPhaseIIID7CollisionUpliftAudit& PlannedAudit,
	const FCarrierLabPhaseIIID4SlabBreakPlanAudit& SlabBreakAudit,
	const FCarrierLabPhaseIIID5SuturePlanAudit& SutureAudit,
	FCarrierLabPhaseIIID7CollisionUpliftAudit& OutAudit,
	const double InterpenetrationThresholdKm,
	const double DestinationMassThresholdRatio)
{
	const double ApplyFromPlanStartSeconds = FPlatformTime::Seconds();
	OutAudit = PlannedAudit;
	OutAudit.bPlannedOnly = false;
	if (PlannedAudit.bNoUpliftAvailable || PlannedAudit.Records.IsEmpty())
	{
		++PhaseIIIDiagnosticCallCounts.D7NoUpliftAvailableCount;
		PhaseIIIDiagnosticCallCounts.D7ApplyFromPlanSeconds += FPlatformTime::Seconds() - ApplyFromPlanStartSeconds;
		return true;
	}

	FCarrierLabPhaseIIID6TopologyMutationAudit TopologyAudit;
	const double TopologyStartSeconds = FPlatformTime::Seconds();
	if (!ApplyPhaseIIID6DetachAndSutureFromPlans(
		SlabBreakAudit,
		SutureAudit,
		TopologyAudit,
		InterpenetrationThresholdKm,
		DestinationMassThresholdRatio))
	{
		PhaseIIIDiagnosticCallCounts.D7TopologyMutationSeconds += FPlatformTime::Seconds() - TopologyStartSeconds;
		PhaseIIIDiagnosticCallCounts.D7ApplyFromPlanSeconds += FPlatformTime::Seconds() - ApplyFromPlanStartSeconds;
		return false;
	}
	PhaseIIIDiagnosticCallCounts.D7TopologyMutationSeconds += FPlatformTime::Seconds() - TopologyStartSeconds;
	OutAudit.TopologyAudit = TopologyAudit;
	OutAudit.SourceTopologyMutationHash = TopologyAudit.TopologyMutationHash;
	OutAudit.EventCountAfter = CurrentMetrics.EventCount;
	OutAudit.ResetSerialAfter = State.ConvergenceTrackingResetSerial;
	OutAudit.bTopologyMutationApplied = TopologyAudit.bMutationApplied && TopologyAudit.AppliedMutationCount == 1;
	if (OutAudit.bTopologyMutationApplied)
	{
		++PhaseIIIDiagnosticCallCounts.D7TopologyMutationAppliedCount;
	}

	if (!OutAudit.bTopologyMutationApplied ||
		TopologyAudit.Records.IsEmpty() ||
		TopologyAudit.Records[0].SuturePlanHash != PlannedAudit.SourceSuturePlanHash)
	{
		++OutAudit.InvalidInputCount;
		PhaseIIIDiagnosticCallCounts.D7ApplyFromPlanSeconds += FPlatformTime::Seconds() - ApplyFromPlanStartSeconds;
		return true;
	}

	if (!State.Plates.IsValidIndex(TopologyAudit.Records[0].DestinationPlateId))
	{
		++OutAudit.InvalidInputCount;
		PhaseIIIDiagnosticCallCounts.D7ApplyFromPlanSeconds += FPlatformTime::Seconds() - ApplyFromPlanStartSeconds;
		return true;
	}
	CarrierLab::FCarrierPlate& DestinationPlate = State.Plates[TopologyAudit.Records[0].DestinationPlateId];
	double AppliedDeltaSum = 0.0;
	double MaxRecordResidual = 0.0;
	int32 AppliedRecordCount = 0;
	const double RecordApplyStartSeconds = FPlatformTime::Seconds();
	for (FCarrierLabPhaseIIID7CollisionUpliftRecord& Record : OutAudit.Records)
	{
		if (!DestinationPlate.Vertices.IsValidIndex(Record.DestinationLocalVertexId))
		{
			++OutAudit.InvalidInputCount;
			continue;
		}
		CarrierLab::FCarrierVertex& Vertex = DestinationPlate.Vertices[Record.DestinationLocalVertexId];
		const double PreviousElevation = Vertex.Elevation;
		const double PreviousFoldMagnitude = Vertex.FoldDirection.Size();
		Vertex.Elevation += Record.AppliedDeltaKm;
		Vertex.FoldDirection = Record.ExpectedFoldDirection;
		Record.PreviousElevationKm = PreviousElevation;
		Record.NewElevationKm = Vertex.Elevation;
		Record.PreviousFoldMagnitude = PreviousFoldMagnitude;
		Record.NewFoldMagnitude = Vertex.FoldDirection.Size();
		Record.bApplied = true;
		++AppliedRecordCount;
		AppliedDeltaSum += Record.AppliedDeltaKm;
		MaxRecordResidual = FMath::Max(
			MaxRecordResidual,
			FMath::Abs((Record.NewElevationKm - Record.PreviousElevationKm) - Record.AppliedDeltaKm));
	}
	PhaseIIIDiagnosticCallCounts.D7RecordApplySeconds += FPlatformTime::Seconds() - RecordApplyStartSeconds;
	PhaseIIIDiagnosticCallCounts.D7AppliedRecordCount += AppliedRecordCount;

	OutAudit.TotalAppliedDeltaKm = AppliedDeltaSum;
	OutAudit.FormulaResidualKm = MaxRecordResidual;
	OutAudit.bUpliftApplied = OutAudit.bTopologyMutationApplied && OutAudit.UpliftRecordCount > 0 && MaxRecordResidual <= 1.0e-9;
	bProjectionRayMeshTopologyDirty = true;
	bRenderMeshTopologyDirty = true;
	const double ProjectionStartSeconds = FPlatformTime::Seconds();
	ProjectCurrentCarrier();
	PhaseIIIDiagnosticCallCounts.D7ProjectionRefreshSeconds += FPlatformTime::Seconds() - ProjectionStartSeconds;
	OutAudit.VisibleElevationHash = CurrentMetrics.VisibleElevationHash;
	OutAudit.CrustStateHash = CurrentMetrics.CrustStateHash;

	const double HashStartSeconds = FPlatformTime::Seconds();
	uint64 UpliftHash = 1469598103934665603ull;
	HashMixString(UpliftHash, TEXT("CarrierLab-IIID7-collision-uplift-applied-v1"));
	HashMixString(UpliftHash, PlannedAudit.UpliftHash);
	HashMixString(UpliftHash, TopologyAudit.TopologyMutationHash);
	HashMix(UpliftHash, OutAudit.bTopologyMutationApplied ? 1ull : 0ull);
	HashMix(UpliftHash, OutAudit.bUpliftApplied ? 1ull : 0ull);
	HashMixDouble(UpliftHash, OutAudit.TotalAppliedDeltaKm);
	HashMixDouble(UpliftHash, OutAudit.FormulaResidualKm);
	HashMixString(UpliftHash, OutAudit.VisibleElevationHash);
	HashMixString(UpliftHash, OutAudit.CrustStateHash);
	for (const FCarrierLabPhaseIIID7CollisionUpliftRecord& Record : OutAudit.Records)
	{
		HashMix(UpliftHash, static_cast<uint64>(Record.RecordId + 1));
		HashMix(UpliftHash, static_cast<uint64>(Record.DestinationLocalVertexId + 1));
		HashMixDouble(UpliftHash, Record.PreviousElevationKm);
		HashMixDouble(UpliftHash, Record.AppliedDeltaKm);
		HashMixDouble(UpliftHash, Record.NewElevationKm);
		HashMixDouble(UpliftHash, Record.NewFoldMagnitude);
		HashMix(UpliftHash, Record.bApplied ? 1ull : 0ull);
	}
	OutAudit.UpliftHash = HashToString(UpliftHash);
	PhaseIIIDiagnosticCallCounts.D7ApplyHashSeconds += FPlatformTime::Seconds() - HashStartSeconds;
	PhaseIIIDiagnosticCallCounts.D7ApplyFromPlanSeconds += FPlatformTime::Seconds() - ApplyFromPlanStartSeconds;
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

bool ACarrierLabVisualizationActor::SetPhaseIIID3DestinationPatchForTest(
	const int32 SourcePlateId,
	const int32 DestinationPlateId,
	const int32 NeighborDepth,
	int32& OutSeedLocalTriangleId,
	int32& OutPatchTriangleCount)
{
	OutSeedLocalTriangleId = INDEX_NONE;
	OutPatchTriangleCount = 0;
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}
	if (!State.Plates.IsValidIndex(SourcePlateId) ||
		!State.Plates.IsValidIndex(DestinationPlateId) ||
		SourcePlateId == DestinationPlateId)
	{
		return false;
	}

	TArray<CarrierLab::FConvergenceSubductionMatrixEvidence> EvidenceRecords = State.ConvergenceSubductionMatrixEvidence;
	EvidenceRecords.Sort([](
		const CarrierLab::FConvergenceSubductionMatrixEvidence& A,
		const CarrierLab::FConvergenceSubductionMatrixEvidence& B)
	{
		if (A.EvidenceId != B.EvidenceId)
		{
			return A.EvidenceId < B.EvidenceId;
		}
		if (A.PlateId != B.PlateId)
		{
			return A.PlateId < B.PlateId;
		}
		return A.LocalTriangleId < B.LocalTriangleId;
	});

	for (const CarrierLab::FConvergenceSubductionMatrixEvidence& Evidence : EvidenceRecords)
	{
		if (Evidence.bAccepted &&
			Evidence.PlateId == SourcePlateId &&
			Evidence.OtherPlateId == DestinationPlateId &&
			State.Plates[DestinationPlateId].LocalTriangles.IsValidIndex(Evidence.OtherLocalTriangleId))
		{
			OutSeedLocalTriangleId = Evidence.OtherLocalTriangleId;
			break;
		}
	}
	if (OutSeedLocalTriangleId == INDEX_NONE)
	{
		return false;
	}

	CarrierLab::FCarrierPlate& Plate = State.Plates[DestinationPlateId];
	TSet<int32> PatchTriangles;
	TArray<int32> Frontier;
	PatchTriangles.Add(OutSeedLocalTriangleId);
	Frontier.Add(OutSeedLocalTriangleId);

	const int32 ClampedDepth = FMath::Clamp(NeighborDepth, 0, 64);
	for (int32 Depth = 0; Depth < ClampedDepth; ++Depth)
	{
		TArray<int32> NextFrontier;
		for (const int32 LocalTriangleId : Frontier)
		{
			TArray<int32> Neighbors;
			GetPlateLocalTriangleNeighbors(Plate, LocalTriangleId, Neighbors);
			for (const int32 NeighborId : Neighbors)
			{
				if (!PatchTriangles.Contains(NeighborId))
				{
					PatchTriangles.Add(NeighborId);
					NextFrontier.Add(NeighborId);
				}
			}
		}
		Frontier = MoveTemp(NextFrontier);
		if (Frontier.IsEmpty())
		{
			break;
		}
	}

	TSet<int32> ContinentalVertexIds;
	for (const int32 LocalTriangleId : PatchTriangles)
	{
		if (!Plate.LocalTriangles.IsValidIndex(LocalTriangleId))
		{
			continue;
		}
		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
		ContinentalVertexIds.Add(Triangle.A);
		ContinentalVertexIds.Add(Triangle.B);
		ContinentalVertexIds.Add(Triangle.C);
	}

	Plate.bContinental = true;
	for (int32 LocalVertexId = 0; LocalVertexId < Plate.Vertices.Num(); ++LocalVertexId)
	{
		CarrierLab::FCarrierVertex& Vertex = Plate.Vertices[LocalVertexId];
		const bool bPatchVertex = ContinentalVertexIds.Contains(LocalVertexId);
		Vertex.ContinentalFraction = bPatchVertex ? 1.0 : 0.0;
		Vertex.bContinental = bPatchVertex;
		if (State.Samples.IsValidIndex(Vertex.GlobalSampleId) &&
			State.Samples[Vertex.GlobalSampleId].PlateId == DestinationPlateId)
		{
			State.Samples[Vertex.GlobalSampleId].ContinentalFraction = Vertex.ContinentalFraction;
			State.Samples[Vertex.GlobalSampleId].bContinental = bPatchVertex;
		}
	}

	OutPatchTriangleCount = PatchTriangles.Num();
	ProjectCurrentCarrier();
	return OutPatchTriangleCount > 0;
}

bool ACarrierLabVisualizationActor::SetPhaseIIID3DestinationFrontPatchForTest(
	const int32 SourcePlateId,
	const int32 DestinationPlateId,
	const int32 NeighborDepth,
	int32& OutSeedLocalTriangleId,
	int32& OutPatchTriangleCount)
{
	OutSeedLocalTriangleId = INDEX_NONE;
	OutPatchTriangleCount = 0;
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}
	if (!State.Plates.IsValidIndex(SourcePlateId) ||
		!State.Plates.IsValidIndex(DestinationPlateId) ||
		SourcePlateId == DestinationPlateId)
	{
		return false;
	}

	CarrierLab::FCarrierPlate& Plate = State.Plates[DestinationPlateId];
	TSet<int32> PatchTriangles;
	TArray<int32> Frontier;

	TArray<CarrierLab::FConvergenceSubductionMatrixEvidence> EvidenceRecords = State.ConvergenceSubductionMatrixEvidence;
	EvidenceRecords.Sort([](
		const CarrierLab::FConvergenceSubductionMatrixEvidence& A,
		const CarrierLab::FConvergenceSubductionMatrixEvidence& B)
	{
		if (A.EvidenceId != B.EvidenceId)
		{
			return A.EvidenceId < B.EvidenceId;
		}
		if (A.PlateId != B.PlateId)
		{
			return A.PlateId < B.PlateId;
		}
		return A.LocalTriangleId < B.LocalTriangleId;
	});

	for (const CarrierLab::FConvergenceSubductionMatrixEvidence& Evidence : EvidenceRecords)
	{
		if (!Evidence.bAccepted ||
			Evidence.PlateId != SourcePlateId ||
			Evidence.OtherPlateId != DestinationPlateId ||
			!Plate.LocalTriangles.IsValidIndex(Evidence.OtherLocalTriangleId))
		{
			continue;
		}
		if (OutSeedLocalTriangleId == INDEX_NONE)
		{
			OutSeedLocalTriangleId = Evidence.OtherLocalTriangleId;
		}
		if (!PatchTriangles.Contains(Evidence.OtherLocalTriangleId))
		{
			PatchTriangles.Add(Evidence.OtherLocalTriangleId);
			Frontier.Add(Evidence.OtherLocalTriangleId);
		}
	}
	if (PatchTriangles.IsEmpty())
	{
		return false;
	}

	const int32 ClampedDepth = FMath::Clamp(NeighborDepth, 0, 64);
	for (int32 Depth = 0; Depth < ClampedDepth; ++Depth)
	{
		TArray<int32> NextFrontier;
		for (const int32 LocalTriangleId : Frontier)
		{
			TArray<int32> Neighbors;
			GetPlateLocalTriangleNeighbors(Plate, LocalTriangleId, Neighbors);
			for (const int32 NeighborId : Neighbors)
			{
				if (!PatchTriangles.Contains(NeighborId))
				{
					PatchTriangles.Add(NeighborId);
					NextFrontier.Add(NeighborId);
				}
			}
		}
		Frontier = MoveTemp(NextFrontier);
		if (Frontier.IsEmpty())
		{
			break;
		}
	}

	TSet<int32> ContinentalVertexIds;
	for (const int32 LocalTriangleId : PatchTriangles)
	{
		if (!Plate.LocalTriangles.IsValidIndex(LocalTriangleId))
		{
			continue;
		}
		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
		ContinentalVertexIds.Add(Triangle.A);
		ContinentalVertexIds.Add(Triangle.B);
		ContinentalVertexIds.Add(Triangle.C);
	}

	Plate.bContinental = true;
	for (int32 LocalVertexId = 0; LocalVertexId < Plate.Vertices.Num(); ++LocalVertexId)
	{
		CarrierLab::FCarrierVertex& Vertex = Plate.Vertices[LocalVertexId];
		const bool bPatchVertex = ContinentalVertexIds.Contains(LocalVertexId);
		Vertex.ContinentalFraction = bPatchVertex ? 1.0 : 0.0;
		Vertex.bContinental = bPatchVertex;
		if (State.Samples.IsValidIndex(Vertex.GlobalSampleId) &&
			State.Samples[Vertex.GlobalSampleId].PlateId == DestinationPlateId)
		{
			State.Samples[Vertex.GlobalSampleId].ContinentalFraction = Vertex.ContinentalFraction;
			State.Samples[Vertex.GlobalSampleId].bContinental = bPatchVertex;
		}
	}

	OutPatchTriangleCount = PatchTriangles.Num();
	ProjectCurrentCarrier();
	return OutPatchTriangleCount > 0;
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

int32 ACarrierLabVisualizationActor::GetCarrierLocalTriangleCountForTest() const
{
	int32 Count = 0;
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		Count += Plate.LocalTriangles.Num();
	}
	return Count;
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
	State.ConvergenceObductionTriangleMarks.Reset();
	State.ConvergenceCollisionPendingTriangleKeys.Reset();
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
	State.ConvergenceObductionTriangleMarkDuplicateCount = 0;
	State.ConvergenceObductionTriangleMarkInvalidCount = 0;
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
			State.ConvergenceObductionTriangleMarks.Reset();
			State.ConvergenceCollisionPendingTriangleKeys.Reset();
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
			State.ConvergenceObductionTriangleMarkDuplicateCount = 0;
			State.ConvergenceObductionTriangleMarkInvalidCount = 0;
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
	const double CadenceMa = GetNaturalCadenceDeltaTMa();
	return FMath::Max(1, FMath::RoundToInt(CadenceMa / DeltaTimeMa));
}

double ACarrierLabVisualizationActor::GetNaturalCadenceDeltaTMa() const
{
	const double ObservedSpeed = GetObservedMaxPlateSpeedForCadenceMmPerYear();
	const double Alpha = FMath::Min(1.0, ObservedSpeed / ResamplingReferenceVelocityMmPerYear);
	return (1.0 - Alpha) * ResamplingMMaxMa + Alpha * ResamplingMMinMa;
}

double ACarrierLabVisualizationActor::GetObservedMaxPlateSpeedMmPerYear() const
{
	double MaxSpeed = 0.0;
	for (const FCarrierLabVisualizationMotion& Motion : Motions)
	{
		MaxSpeed = FMath::Max(MaxSpeed, VelocityMmPerYearFromAngularSpeed(Motion.AngularSpeedRadiansPerStep));
	}
	return Motions.Num() > 0 ? MaxSpeed : FMath::Max(0.0, VelocityMmPerYear);
}

double ACarrierLabVisualizationActor::GetObservedMaxPlateSpeedForCadenceMmPerYear() const
{
	return FMath::Max(
		CurrentMetrics.ObservedMaxPlateSpeedSinceLastRemeshMmPerYear,
		GetObservedMaxPlateSpeedMmPerYear());
}

void ACarrierLabVisualizationActor::ResetObservedSpeedWindowForRemesh()
{
	CurrentMetrics.ObservedMaxPlateSpeedMmPerYear = GetObservedMaxPlateSpeedMmPerYear();
	CurrentMetrics.ObservedMaxPlateSpeedSinceLastRemeshMmPerYear =
		CurrentMetrics.ObservedMaxPlateSpeedMmPerYear;
}

void ACarrierLabVisualizationActor::UpdateNaturalCadenceMetrics(const bool bAdvanceObservedSpeedWindow)
{
	CurrentMetrics.ObservedMaxPlateSpeedMmPerYear = GetObservedMaxPlateSpeedMmPerYear();
	if (bAdvanceObservedSpeedWindow ||
		CurrentMetrics.ObservedMaxPlateSpeedSinceLastRemeshMmPerYear <= UE_DOUBLE_SMALL_NUMBER)
	{
		CurrentMetrics.ObservedMaxPlateSpeedSinceLastRemeshMmPerYear = FMath::Max(
			CurrentMetrics.ObservedMaxPlateSpeedSinceLastRemeshMmPerYear,
			CurrentMetrics.ObservedMaxPlateSpeedMmPerYear);
	}

	CurrentMetrics.CadenceDeltaTMa = GetNaturalCadenceDeltaTMa();
	const int32 Cadence = GetNaturalCadenceSteps();
	CurrentMetrics.CadenceSteps = Cadence;
	const int32 ProposedNextStep = ((CurrentMetrics.Step / Cadence) + 1) * Cadence;
	if (CurrentMetrics.NextResampleStep <= 0 ||
		bAdvanceObservedSpeedWindow ||
		ProposedNextStep < CurrentMetrics.NextResampleStep)
	{
		CurrentMetrics.NextResampleStep = ProposedNextStep;
	}
}

void ACarrierLabVisualizationActor::ResetPhaseIIIDiagnosticCallCounts() const
{
	PhaseIIIDiagnosticCallCounts = FCarrierLabPhaseIIIDiagnosticCallCounts();
}

FCarrierLabPhaseIIIDiagnosticCallCounts ACarrierLabVisualizationActor::GetPhaseIIIDiagnosticCallCounts() const
{
	return PhaseIIIDiagnosticCallCounts;
}

bool ACarrierLabVisualizationActor::RunPhaseIIICostDriverAdvanceProbe(FCarrierLabPhaseIIICostDriverStepAudit& OutAudit)
{
	OutAudit = FCarrierLabPhaseIIICostDriverStepAudit();
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}

	auto CountActiveTriangles = [&]()
	{
		int32 Count = 0;
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			Count += Plate.ActiveBoundaryTriangles.Num();
		}
		return Count;
	};

	OutAudit.StepBefore = CurrentMetrics.Step;
	OutAudit.SampleCount = State.Samples.Num();
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ActiveTriangleCountBefore = CountActiveTriangles();
	const double TotalStartSeconds = FPlatformTime::Seconds();

	double StartSeconds = FPlatformTime::Seconds();
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
	OutAudit.MotionSeconds = FPlatformTime::Seconds() - StartSeconds;

	StartSeconds = FPlatformTime::Seconds();
	UpdateConvergenceTrackingDistances();
	OutAudit.DistanceUpdateSeconds = FPlatformTime::Seconds() - StartSeconds;

	StartSeconds = FPlatformTime::Seconds();
	UpdateConvergenceSubductionMatrix();
	OutAudit.MatrixTotalSeconds = FPlatformTime::Seconds() - StartSeconds;
	OutAudit.MatrixBvhBuildSeconds = LastConvergenceMatrixBvhBuildSeconds;
	OutAudit.MatrixRayQuerySeconds = LastConvergenceMatrixRayQuerySeconds;

	StartSeconds = FPlatformTime::Seconds();
	UpdateConvergenceSubductionPolarityDecisions();
	OutAudit.PolaritySeconds = FPlatformTime::Seconds() - StartSeconds;

	StartSeconds = FPlatformTime::Seconds();
	UpdateConvergenceNeighborPropagation();
	OutAudit.NeighborPropagationSeconds = FPlatformTime::Seconds() - StartSeconds;

	StartSeconds = FPlatformTime::Seconds();
	BeginPhaseIIIC5ElevationLedger();
	OutAudit.LedgerSeconds += FPlatformTime::Seconds() - StartSeconds;

	if (bEnablePhaseIIICSubductingMarks)
	{
		StartSeconds = FPlatformTime::Seconds();
		UpdatePhaseIIICSubductingTriangleMarks();
		if (bEnablePhaseIIICObductionUpliftBridge)
		{
			UpdatePhaseIIICObductionTriangleMarks();
		}
		OutAudit.MarkSeconds = FPlatformTime::Seconds() - StartSeconds;
	}
	if (bEnablePhaseIIICOverridingPlateUplift)
	{
		StartSeconds = FPlatformTime::Seconds();
		ApplyPhaseIIIC3OverridingPlateUplift();
		if (bEnablePhaseIIICObductionUpliftBridge)
		{
			ApplyPhaseIIICObductionUplift();
		}
		else
		{
			LastPhaseIIICObductionUpliftAudit = FCarrierLabPhaseIIICObductionUpliftAudit();
			LastPhaseIIICObductionUpliftAudit.Step = CurrentMetrics.Step + 1;
			LastPhaseIIICObductionUpliftAudit.EventCount = CurrentMetrics.EventCount;
			LastPhaseIIICObductionUpliftAudit.PlateCount = State.Plates.Num();
			LastPhaseIIICObductionUpliftAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
			LastPhaseIIICObductionUpliftAudit.bEnabled = false;
			LastPhaseIIICObductionUpliftAudit.bUpliftEnabled = true;
			LastPhaseIIICObductionUpliftAudit.MarkCount = State.ConvergenceObductionTriangleMarks.Num();
		}
		OutAudit.UpliftSeconds = FPlatformTime::Seconds() - StartSeconds;
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
		LastPhaseIIICObductionUpliftAudit = FCarrierLabPhaseIIICObductionUpliftAudit();
		LastPhaseIIICObductionUpliftAudit.Step = CurrentMetrics.Step + 1;
		LastPhaseIIICObductionUpliftAudit.EventCount = CurrentMetrics.EventCount;
		LastPhaseIIICObductionUpliftAudit.PlateCount = State.Plates.Num();
		LastPhaseIIICObductionUpliftAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
		LastPhaseIIICObductionUpliftAudit.bEnabled = bEnablePhaseIIICObductionUpliftBridge;
		LastPhaseIIICObductionUpliftAudit.bUpliftEnabled = false;
		LastPhaseIIICObductionUpliftAudit.MarkCount = State.ConvergenceObductionTriangleMarks.Num();
	}

	StartSeconds = FPlatformTime::Seconds();
	ApplyPhaseIIIC4SlabPull();
	OutAudit.SlabPullSeconds = FPlatformTime::Seconds() - StartSeconds;

	StartSeconds = FPlatformTime::Seconds();
	FinalizePhaseIIIC5ElevationLedger();
	OutAudit.LedgerSeconds += FPlatformTime::Seconds() - StartSeconds;

	++CurrentMetrics.Step;
	UpdateNaturalCadenceMetrics(true);

	OutAudit.StepAfter = CurrentMetrics.Step;
	OutAudit.ActiveTriangleCountAfter = CountActiveTriangles();
	OutAudit.MatrixRayTestCount = State.ConvergenceSubductionMatrixRayTestCount;
	OutAudit.MatrixHitCount = State.ConvergenceSubductionMatrixHitCount;
	OutAudit.MatrixEvidenceCount = State.ConvergenceSubductionMatrixEvidence.Num();
	OutAudit.PolarityDecisionCount = State.ConvergenceSubductionPolarityDecisions.Num();
	OutAudit.NeighborSeedCount = State.ConvergenceNeighborPropagationSeedCount;
	OutAudit.NeighborAddedCount = State.ConvergenceNeighborPropagationAddedCount;
	OutAudit.SubductingMarkCount = State.ConvergenceSubductingTriangleMarks.Num();
	OutAudit.UpliftRecordCount =
		LastPhaseIIIC3UpliftAudit.UpliftRecordCount +
		LastPhaseIIICObductionUpliftAudit.UpliftRecordCount;
	OutAudit.SlabPullContributionCount = LastPhaseIIIC4SlabPullAudit.ContributionCount;
	OutAudit.TotalProcessSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
	return true;
}

void ACarrierLabVisualizationActor::StepOnce()
{
	if (!bInitialized && !InitializeCarrier())
	{
		return;
	}
	if (!AdvanceOneStepWithNaturalResampling())
	{
		ProjectCurrentCarrier();
	}
}

bool ACarrierLabVisualizationActor::ShouldFireNaturalResamplingEvent(const int32 TargetStep) const
{
	return bEnableNaturalResamplingEvents &&
		TargetStep > 0 &&
		GetObservedMaxPlateSpeedMmPerYear() > UE_DOUBLE_SMALL_NUMBER &&
		State.Plates.Num() > 1 &&
		CurrentMetrics.Step >= TargetStep;
}

bool ACarrierLabVisualizationActor::AdvanceOneStepWithNaturalResampling()
{
	const int32 TargetStep = CurrentMetrics.NextResampleStep > 0
		? CurrentMetrics.NextResampleStep
		: GetNaturalCadenceSteps();
	AdvanceOneStep();
	if (ShouldFireNaturalResamplingEvent(TargetStep))
	{
		const bool bApplied = ApplyNaturalResampleEvent();
		if (!bApplied)
		{
			CurrentMetrics.NextResampleStep = TargetStep;
		}
		return bApplied;
	}
	return false;
}

bool ACarrierLabVisualizationActor::ApplyPhaseIIIELiveRemeshEvent()
{
	if (!bInitialized && !InitializeCarrier())
	{
		return false;
	}

	const double StartSeconds = FPlatformTime::Seconds();
	CurrentMetrics.PhaseIIIELastGeneratedCandidateCount = 0;
	CurrentMetrics.PhaseIIIELastAppliedGeneratedCount = 0;
	CurrentMetrics.PhaseIIIELastRiftingPendingCount = 0;
	CurrentMetrics.PhaseIIIELastGeneratedWithNonPositiveSeparationCount = 0;
	CurrentMetrics.PhaseIIIELastInvalidRecordCount = 0;
	CurrentMetrics.PhaseIIIELastUnresolvedMultiHitHoldCount = 0;
	CurrentMetrics.PhaseIIIELastCoalescedMultiHitCount = 0;
	CurrentMetrics.PhaseIIIELastSharedBoundaryTieBreakCount = 0;
	CurrentMetrics.PhaseIIIELastNearestHitTieBreakCount = 0;
	CurrentMetrics.PhaseIIIELastDistanceTieFallbackCount = 0;
	CurrentMetrics.PhaseIIIELastWithinPlateCoincidentHoldCount = 0;
	CurrentMetrics.PhaseIIIELastCrossPlateEqualHoldCount = 0;
	CurrentMetrics.PhaseIIIELastThirdPlateHoldCount = 0;
	CurrentMetrics.PhaseIIIELastTripleJunctionSplitCount = 0;
	PhaseIIIELiveRemeshEventMask.Init(0, State.Samples.Num());

	FCarrierLabPhaseIIIE3RemeshSelectionAudit SelectionAudit;
	if (!RunPhaseIIIE3FilteredRemeshSelectionAudit(SelectionAudit))
	{
		CurrentMetrics.LastRemeshMode = TEXT("phase_iii_e6_live_hold_selection_failed");
		CurrentMetrics.ResampleEventSeconds = FPlatformTime::Seconds() - StartSeconds;
		UE_LOG(LogTemp, Warning, TEXT("CarrierLab IIIE.6 live remesh held: %s"), *CurrentMetrics.LastRemeshMode);
		return false;
	}

	CurrentMetrics.LastGapFillCount = SelectionAudit.DivergentGapRouteCount;
	CurrentMetrics.PolicyResolvedMultiHitCount = SelectionAudit.PolicyWinnerCount;
	CurrentMetrics.PhaseIIIELastUnresolvedMultiHitHoldCount = SelectionAudit.UnresolvedMultiHitCount;
	CurrentMetrics.PhaseIIIELastCoalescedMultiHitCount = SelectionAudit.CoalescedMultiHitCount;
	CurrentMetrics.PhaseIIIELastSharedBoundaryTieBreakCount = SelectionAudit.SharedBoundaryTieBreakCount;
	CurrentMetrics.PhaseIIIELastNearestHitTieBreakCount =
		SelectionAudit.NearestHitCrossPlateDifferentResolvedCount +
		SelectionAudit.NearestHitThirdPlateResolvedCount;
	CurrentMetrics.PhaseIIIELastDistanceTieFallbackCount = SelectionAudit.DistanceTieFallbackCount;
	CurrentMetrics.PhaseIIIELastWithinPlateCoincidentHoldCount = SelectionAudit.WithinPlateCoincidentMultiHitCount;
	CurrentMetrics.PhaseIIIELastCrossPlateEqualHoldCount = SelectionAudit.CrossPlateEqualMultiHitCount;
	CurrentMetrics.PhaseIIIELastThirdPlateHoldCount = SelectionAudit.ThirdPlateMultiHitCount;
	for (const FCarrierLabPhaseIIIE3SelectionRecord& Record : SelectionAudit.Records)
	{
		if (!PhaseIIIELiveRemeshEventMask.IsValidIndex(Record.SampleId))
		{
			continue;
		}
		if (Record.bUnresolvedMultiHit)
		{
			PhaseIIIELiveRemeshEventMask[Record.SampleId] |= PhaseIIIELiveRemeshMaskUnresolvedHold;
		}
		if (Record.bCoalescedDuplicateHit)
		{
			PhaseIIIELiveRemeshEventMask[Record.SampleId] |= PhaseIIIELiveRemeshMaskCoalescedDuplicate;
		}
		if (Record.bUsedSharedBoundaryTieBreak)
		{
			PhaseIIIELiveRemeshEventMask[Record.SampleId] |= PhaseIIIELiveRemeshMaskSharedBoundaryTieBreak;
		}
		if (Record.bUsedDistanceTieFallback)
		{
			PhaseIIIELiveRemeshEventMask[Record.SampleId] |= PhaseIIIELiveRemeshMaskDistanceTieFallback;
		}
	}
	if (SelectionAudit.UnresolvedMultiHitCount > 0)
	{
		CurrentMetrics.LastRemeshMode = FString::Printf(
			TEXT("phase_iii_e6_live_hold_unresolved_multi_hit_%d"),
			SelectionAudit.UnresolvedMultiHitCount);
		CurrentMetrics.ResampleEventSeconds = FPlatformTime::Seconds() - StartSeconds;
		RebuildRenderMesh();
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("CarrierLab IIIE.6 live remesh held: %s (coalesced=%d shared_tiebreak=%d nearest_hit cross/third/tie/unsupported=%d/%d/%d/%d distance_tie_fallback=%d hold_buckets withinCoin/withinSep/crossEq/crossDiff/mixed/third=%d/%d/%d/%d/%d/%d)"),
			*CurrentMetrics.LastRemeshMode,
			SelectionAudit.CoalescedMultiHitCount,
			SelectionAudit.SharedBoundaryTieBreakCount,
			SelectionAudit.NearestHitCrossPlateDifferentResolvedCount,
			SelectionAudit.NearestHitThirdPlateResolvedCount,
			SelectionAudit.NearestHitDistanceTieHeldCount,
			SelectionAudit.NearestHitUnsupportedHeldCount,
			SelectionAudit.DistanceTieFallbackCount,
			SelectionAudit.WithinPlateCoincidentMultiHitCount,
			SelectionAudit.WithinPlateDistanceSeparatedMultiHitCount,
			SelectionAudit.CrossPlateEqualMultiHitCount,
			SelectionAudit.CrossPlateDifferentMultiHitCount,
			SelectionAudit.MixedMaterialMultiHitCount,
			SelectionAudit.ThirdPlateMultiHitCount);
		return false;
	}

	TArray<FCarrierLabPhaseIIIE5RemeshVertexRecord> VertexRecords;
	VertexRecords.SetNum(State.Samples.Num());
	int32 InvalidRecordCount = 0;
	int32 NonSeparatingGapFillCount = 0;
	int32 NoBoundaryPairMissCount = 0;
	int32 GeneratedCandidateCount = 0;
	int32 AppliedGeneratedCount = 0;
	int32 RiftingPendingCount = 0;
	int32 GeneratedWithNonPositiveSeparationCount = 0;

	for (const FCarrierLabPhaseIIIE3SelectionRecord& SelectionRecord : SelectionAudit.Records)
	{
		if (!State.Samples.IsValidIndex(SelectionRecord.SampleId) || !VertexRecords.IsValidIndex(SelectionRecord.SampleId))
		{
			++InvalidRecordCount;
			continue;
		}

		const CarrierLab::FSphereSample& Sample = State.Samples[SelectionRecord.SampleId];
		FCarrierLabPhaseIIIE5RemeshVertexRecord& VertexRecord = VertexRecords[SelectionRecord.SampleId];
		VertexRecord.SampleId = Sample.Id;
		VertexRecord.AssignedPlateId = Sample.PlateId;
		VertexRecord.PreRemeshContinentalFraction = Sample.ContinentalFraction;
		VertexRecord.PreRemeshElevation = Sample.Elevation;
		VertexRecord.ContinentalFraction = Sample.ContinentalFraction;
		VertexRecord.Elevation = Sample.Elevation;
		VertexRecord.HistoricalElevation = Sample.HistoricalElevation;
		VertexRecord.OceanicAge = Sample.OceanicAge;
		VertexRecord.RidgeDirection = Sample.RidgeDirection;
		VertexRecord.FoldDirection = Sample.FoldDirection;

		if (SelectionRecord.SelectionClass == ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit)
		{
			if (!State.Plates.IsValidIndex(SelectionRecord.ResolvedPlateId))
			{
				++InvalidRecordCount;
				continue;
			}
			VertexRecord.AssignedPlateId = SelectionRecord.ResolvedPlateId;
			VertexRecord.bResolvedSingleHit = true;
			VertexRecord.ContinentalFraction = SelectionRecord.ContinentalFraction;
			VertexRecord.Elevation = SelectionRecord.Elevation;
			VertexRecord.HistoricalElevation = SelectionRecord.HistoricalElevation;
			VertexRecord.OceanicAge = SelectionRecord.OceanicAge;
			VertexRecord.RidgeDirection = SelectionRecord.RidgeDirection;
			VertexRecord.FoldDirection = SelectionRecord.FoldDirection;
			continue;
		}

		if (IsPhaseIIIE4DivergentGapRoute(SelectionRecord.SelectionClass))
		{
			FCarrierLabPhaseIIIE4OceanicGenerationRecord OceanicRecord;
			QueryPhaseIIIE4DivergentOceanicFieldsFromCurrentStateForTest(
				Sample.UnitPosition,
				SelectionRecord.SelectionClass,
				OceanicRecord);

			if (!OceanicRecord.bGeneratedOceanicCrust)
			{
				NoBoundaryPairMissCount += OceanicRecord.bBoundaryPairFound ? 0 : 1;
				NonSeparatingGapFillCount += OceanicRecord.bNonSeparatingAnomaly ? 1 : 0;
				++InvalidRecordCount;
				continue;
			}

			++GeneratedCandidateCount;
			GeneratedWithNonPositiveSeparationCount += OceanicRecord.bGeneratedWithNonPositiveSeparation ? 1 : 0;
			VertexRecord.bResolvedSingleHit = false;
			VertexRecord.bDivergentGapRoute = true;
			VertexRecord.OceanicRecord = OceanicRecord;
			VertexRecord.OceanicRecord.SampleId = Sample.Id;
			VertexRecord.bUsedZGammaDistanceProfilePlaceholder = OceanicRecord.bUsedZGammaDistanceProfilePlaceholder;
			VertexRecord.bUsedZGammaGeophysicsDerivedProfile = OceanicRecord.bUsedZGammaGeophysicsDerivedProfile;
			VertexRecord.bPaperFaithfulZGammaProfile = OceanicRecord.bPaperFaithfulZGammaProfile;

			if (Sample.ContinentalFraction > PhaseIIIEContinentalOverwriteThreshold)
			{
				++RiftingPendingCount;
				if (PhaseIIIELiveRemeshEventMask.IsValidIndex(Sample.Id))
				{
					PhaseIIIELiveRemeshEventMask[Sample.Id] |= PhaseIIIELiveRemeshMaskRiftingPending;
				}
				continue;
			}

			if (!State.Plates.IsValidIndex(OceanicRecord.AssignedPlateId))
			{
				++InvalidRecordCount;
				continue;
			}
			++AppliedGeneratedCount;
			VertexRecord.bGeneratedOceanicCrust = true;
			VertexRecord.AssignedPlateId = OceanicRecord.AssignedPlateId;
			VertexRecord.ContinentalFraction = 0.0;
			VertexRecord.Elevation = OceanicRecord.Elevation;
			VertexRecord.HistoricalElevation = 0.0;
			VertexRecord.OceanicAge = OceanicRecord.OceanicAge;
			VertexRecord.RidgeDirection = OceanicRecord.RidgeDirection;
			VertexRecord.FoldDirection = FVector3d::ZeroVector;
			if (PhaseIIIELiveRemeshEventMask.IsValidIndex(Sample.Id))
			{
				PhaseIIIELiveRemeshEventMask[Sample.Id] |= PhaseIIIELiveRemeshMaskAppliedGenerated;
			}
			continue;
		}

		VertexRecord.bUnresolvedMultiHit = SelectionRecord.bUnresolvedMultiHit;
		++InvalidRecordCount;
	}

	CurrentMetrics.LastNonSeparatingGapFillCount = NonSeparatingGapFillCount;
	CurrentMetrics.LastNoBoundaryPairMissCount = NoBoundaryPairMissCount;
	CurrentMetrics.PhaseIIIELastGeneratedWithNonPositiveSeparationCount = GeneratedWithNonPositiveSeparationCount;
	CurrentMetrics.PhaseIIIELastGeneratedCandidateCount = GeneratedCandidateCount;
	CurrentMetrics.PhaseIIIELastAppliedGeneratedCount = AppliedGeneratedCount;
	CurrentMetrics.PhaseIIIELastRiftingPendingCount = RiftingPendingCount;
	CurrentMetrics.PhaseIIIELastInvalidRecordCount = InvalidRecordCount;
	if (InvalidRecordCount > 0)
	{
		CurrentMetrics.LastRemeshMode = FString::Printf(
			TEXT("phase_iii_e6_live_hold_invalid_records_%d_gen_%d_applied_%d_rift_%d_no_boundary_%d_nonsep_%d"),
			InvalidRecordCount,
			GeneratedCandidateCount,
			AppliedGeneratedCount,
			RiftingPendingCount,
			NoBoundaryPairMissCount,
			NonSeparatingGapFillCount);
		CurrentMetrics.ResampleEventSeconds = FPlatformTime::Seconds() - StartSeconds;
		RebuildRenderMesh();
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("CarrierLab IIIE.6 live remesh held: %s (generated=%d applied=%d rifting_pending=%d no_boundary_pair=%d nonseparating=%d)"),
			*CurrentMetrics.LastRemeshMode,
			GeneratedCandidateCount,
			AppliedGeneratedCount,
			RiftingPendingCount,
			NoBoundaryPairMissCount,
			NonSeparatingGapFillCount);
		return false;
	}

	FCarrierLabPhaseIIIE5RemeshInputFixture RebuildInput;
	RebuildInput.bUseExplicitVertexRecords = true;
	RebuildInput.ExplicitVertexRecords = MoveTemp(VertexRecords);

	FCarrierLabPhaseIIIE5TopologyRebuildAudit RebuildAudit;
	if (!RunPhaseIIIE5TopologyRebuildFixtureForTest(RebuildInput, RebuildAudit) || !RebuildAudit.bApplied)
	{
		CurrentMetrics.LastRemeshMode = FString::Printf(
			TEXT("phase_iii_e6_live_hold_rebuild_failed_invalid_%d_unresolved_%d"),
			RebuildAudit.InvalidTriangleCount,
			RebuildAudit.UnresolvedTripleJunctionCount);
		CurrentMetrics.ResampleEventSeconds = FPlatformTime::Seconds() - StartSeconds;
		RebuildRenderMesh();
		UE_LOG(LogTemp, Warning, TEXT("CarrierLab IIIE.6 live remesh held: %s"), *CurrentMetrics.LastRemeshMode);
		return false;
	}

	const int32 EventCount = CurrentMetrics.EventCount;
	ProjectCurrentCarrier();
	CurrentMetrics.EventCount = EventCount;
	CurrentMetrics.LastGapFillCount = GeneratedCandidateCount;
	CurrentMetrics.LastNonSeparatingGapFillCount = NonSeparatingGapFillCount;
	CurrentMetrics.LastNoBoundaryPairMissCount = NoBoundaryPairMissCount;
	CurrentMetrics.PhaseIIIELastGeneratedWithNonPositiveSeparationCount = GeneratedWithNonPositiveSeparationCount;
	CurrentMetrics.PolicyResolvedMultiHitCount = SelectionAudit.PolicyWinnerCount + RebuildAudit.PolicyWinnerCount;
	CurrentMetrics.PhaseIIIELastGeneratedCandidateCount = GeneratedCandidateCount;
	CurrentMetrics.PhaseIIIELastAppliedGeneratedCount = AppliedGeneratedCount;
	CurrentMetrics.PhaseIIIELastRiftingPendingCount = RiftingPendingCount;
	CurrentMetrics.PhaseIIIELastInvalidRecordCount = 0;
	CurrentMetrics.PhaseIIIELastUnresolvedMultiHitHoldCount = 0;
	CurrentMetrics.PhaseIIIELastCoalescedMultiHitCount = SelectionAudit.CoalescedMultiHitCount;
	CurrentMetrics.PhaseIIIELastSharedBoundaryTieBreakCount = SelectionAudit.SharedBoundaryTieBreakCount;
	CurrentMetrics.PhaseIIIELastNearestHitTieBreakCount =
		SelectionAudit.NearestHitCrossPlateDifferentResolvedCount +
		SelectionAudit.NearestHitThirdPlateResolvedCount;
	CurrentMetrics.PhaseIIIELastDistanceTieFallbackCount = SelectionAudit.DistanceTieFallbackCount;
	CurrentMetrics.PhaseIIIELastWithinPlateCoincidentHoldCount = 0;
	CurrentMetrics.PhaseIIIELastCrossPlateEqualHoldCount = 0;
	CurrentMetrics.PhaseIIIELastThirdPlateHoldCount = 0;
	CurrentMetrics.PhaseIIIELastTripleJunctionSplitCount = RebuildAudit.TripleJunctionCentroidSplitCount;
	CurrentMetrics.LastRemeshMode = FString::Printf(
		TEXT("phase_iii_e6_live_apply gen=%d applied=%d rift_pending=%d nonpos_sep=%d coalesced=%d shared_tiebreak=%d nearest_hit=%d distance_tie_fallback=%d majority=%d tj_split=%d"),
		GeneratedCandidateCount,
		AppliedGeneratedCount,
		RiftingPendingCount,
		GeneratedWithNonPositiveSeparationCount,
		SelectionAudit.CoalescedMultiHitCount,
		SelectionAudit.SharedBoundaryTieBreakCount,
		CurrentMetrics.PhaseIIIELastNearestHitTieBreakCount,
		CurrentMetrics.PhaseIIIELastDistanceTieFallbackCount,
		RebuildAudit.MajorityTriangleCount,
		RebuildAudit.TripleJunctionCentroidSplitCount);
	ResetObservedSpeedWindowForRemesh();
	CurrentMetrics.NextResampleStep = 0;
	UpdateNaturalCadenceMetrics(false);
	CurrentMetrics.ResampleEventSeconds = FPlatformTime::Seconds() - StartSeconds;
	CurrentMetrics.ProjectionSeconds += CurrentMetrics.ResampleEventSeconds;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("CarrierLab IIIE.6 live remesh applied: events=%d mode=%s"),
		CurrentMetrics.EventCount,
		*CurrentMetrics.LastRemeshMode);
	return true;
}

bool ACarrierLabVisualizationActor::ApplyNaturalResampleEvent()
{
	return ApplyPhaseIIIELiveRemeshEvent();
}

void ACarrierLabVisualizationActor::TriggerPhaseIIIELiveRemeshEvent()
{
	ApplyPhaseIIIELiveRemeshEvent();
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
	int32 NoBoundaryPairMissCount = 0;
	int32 PolicyResolvedMultiHitCount = 0;
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
				++NoBoundaryPairMissCount;
				NewPlateIds[Sample.Id] = Sample.PlateId;
				NewFractions[Sample.Id] = Sample.ContinentalFraction;
			}
			continue;
		}

		int32 ResolvedPlateId = INDEX_NONE;
		if (PlateHitCount == 1)
		{
			ResolvedPlateId = LowestSetBitIndex(PlateMask);
		}
		else
		{
			++PolicyResolvedMultiHitCount;
			ResolvedPlateId = ChooseCandidatePlateByPolicy(Sample, Candidates, Motions, MultiHitPolicy, RandomTieBreakSeed, CurrentMetrics.Step, CurrentMetrics.EventCount + 1);
		}
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

	ensureMsgf(
		NoBoundaryPairMissCount == 0,
		TEXT("CarrierLab unfiltered resampling hit %d no-boundary-pair misses; this path is a retention hazard and must remain gated."),
		NoBoundaryPairMissCount);

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
	CurrentMetrics.LastNoBoundaryPairMissCount = NoBoundaryPairMissCount;
	CurrentMetrics.PolicyResolvedMultiHitCount = PolicyResolvedMultiHitCount;
	CurrentMetrics.LastRemeshMode = TEXT("legacy_stage_1_5_lab_policy_unfiltered");
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

void ACarrierLabVisualizationActor::ShowOceanicAgeHeatmapLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::OceanicAgeHeatmap);
}

void ACarrierLabVisualizationActor::ShowRidgeDirectionLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::RidgeDirection);
}

void ACarrierLabVisualizationActor::ShowPhaseIIIERemeshSummaryLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::PhaseIIIERemeshSummary);
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
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ACarrierLabVisualizationActor::TriggerPhaseIIIELiveRemeshEvent);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowPlateIdLayer);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowContinentalFractionLayer);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowMissMaskLayer);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowOverlapMaskLayer);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowBoundaryMaskLayer);
	InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowPhaseIIISummaryLayer);
	InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowElevationHeatmapLayer);
	InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowSubductionMaskLayer);
	InputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowDistanceToFrontHeatmapLayer);
	InputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowPhaseIIIERemeshSummaryLayer);
	InputComponent->BindKey(EKeys::O, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowOceanicAgeHeatmapLayer);
	InputComponent->BindKey(EKeys::G, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowRidgeDirectionLayer);
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
	LastConvergenceMatrixBvhBuildSeconds = 0.0;
	LastConvergenceMatrixRayQuerySeconds = 0.0;
	State.ConvergenceSubductionTriangleHits.Reset();
	State.ConvergenceSubductionMatrixEvidence.Reset();
	FString MeshError;
	const double BvhStartSeconds = FPlatformTime::Seconds();
	if (!RefreshPlateRayMeshes(MeshError))
	{
		LastConvergenceMatrixBvhBuildSeconds = FPlatformTime::Seconds() - BvhStartSeconds;
		UE_LOG(LogTemp, Error, TEXT("CarrierLab convergence subduction matrix update failed: %s"), *MeshError);
		return;
	}
	LastConvergenceMatrixBvhBuildSeconds = FPlatformTime::Seconds() - BvhStartSeconds;

	IMeshSpatial::FQueryOptions QueryOptions(2.0 + 1.0e-6);
	TArray<MeshIntersection::FHitIntersectionResult> Hits;
	const double QueryStartSeconds = FPlatformTime::Seconds();
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
	LastConvergenceMatrixRayQuerySeconds = FPlatformTime::Seconds() - QueryStartSeconds;
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

void ACarrierLabVisualizationActor::UpdatePhaseIIICObductionTriangleMarks()
{
	TMap<uint64, CarrierLab::FConvergenceSubductionPolarityDecision> DecisionsByPair;
	for (const CarrierLab::FConvergenceSubductionPolarityDecision& Decision : State.ConvergenceSubductionPolarityDecisions)
	{
		DecisionsByPair.Add(Decision.PairKey, Decision);
	}

	TMap<int32, CarrierLab::FConvergenceSubductionMatrixEvidence> EvidenceById;
	for (const CarrierLab::FConvergenceSubductionMatrixEvidence& Evidence : State.ConvergenceSubductionMatrixEvidence)
	{
		EvidenceById.Add(Evidence.EvidenceId, Evidence);
	}

	TSet<uint64> ExistingTriangleKeys;
	for (const CarrierLab::FConvergenceObductionTriangleMark& Mark : State.ConvergenceObductionTriangleMarks)
	{
		ExistingTriangleKeys.Add(MakePlateTriangleKey(Mark.PlateId, Mark.LocalTriangleId));
	}

	for (const CarrierLab::FConvergenceSubductionTriangleHit& Hit : State.ConvergenceSubductionTriangleHits)
	{
		const CarrierLab::FConvergenceSubductionPolarityDecision* Decision = DecisionsByPair.Find(Hit.PairKey);
		if (Decision == nullptr ||
			Decision->DecisionClass != CarrierLab::EConvergenceSubductionPolarityClass::CollisionCandidate)
		{
			continue;
		}

		if (!State.Plates.IsValidIndex(Hit.PlateId) ||
			!State.Plates[Hit.PlateId].LocalTriangles.IsValidIndex(Hit.LocalTriangleId))
		{
			++State.ConvergenceObductionTriangleMarkInvalidCount;
			continue;
		}

		const CarrierLab::FConvergenceSubductionMatrixEvidence* Evidence = EvidenceById.Find(Hit.EvidenceId);
		if (Evidence == nullptr ||
			!State.Plates.IsValidIndex(Hit.OtherPlateId) ||
			!State.Plates[Hit.OtherPlateId].LocalTriangles.IsValidIndex(Evidence->OtherLocalTriangleId))
		{
			++State.ConvergenceObductionTriangleMarkInvalidCount;
			continue;
		}

		const uint64 TriangleKey = MakePlateTriangleKey(Hit.PlateId, Hit.LocalTriangleId);
		if (ExistingTriangleKeys.Contains(TriangleKey))
		{
			++State.ConvergenceObductionTriangleMarkDuplicateCount;
			continue;
		}

		CarrierLab::FConvergenceObductionTriangleMark& Mark = State.ConvergenceObductionTriangleMarks.AddDefaulted_GetRef();
		Mark.MarkId = State.ConvergenceObductionTriangleMarks.Num() - 1;
		Mark.PairKey = Hit.PairKey;
		Mark.PlateId = Hit.PlateId;
		Mark.OtherPlateId = Hit.OtherPlateId;
		Mark.LocalTriangleId = Hit.LocalTriangleId;
		Mark.OtherLocalTriangleId = Evidence->OtherLocalTriangleId;
		Mark.EvidenceId = Hit.EvidenceId;
		Mark.SignedConvergenceVelocity = Hit.SignedConvergenceVelocity;
		Mark.DecisionClass = Decision->DecisionClass;
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
	case ECarrierLabPhaseIIIC5ElevationLedgerClass::ObductionUplift:
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
	LastPhaseIIIC3UpliftAudit.FoldInfluenceBeta = PhaseIIICFoldDirectionBeta;
	LastPhaseIIIC3UpliftAudit.TrenchDepthKm = PhaseIIICTrenchDepthKm;
	LastPhaseIIIC3UpliftAudit.ContinentalMaxElevationKm = PhaseIIICMaxContinentalElevationKm;

	uint64 UpliftHash = 1469598103934665603ull;
	HashMix(UpliftHash, static_cast<uint64>(LastPhaseIIIC3UpliftAudit.Step + 1));
	HashMix(UpliftHash, static_cast<uint64>(State.ConvergenceSubductingTriangleMarks.Num() + 1));
	HashMixDouble(UpliftHash, PhaseIIICSubductionUpliftMmPerYear);
	HashMixDouble(UpliftHash, PhaseIIICReferenceVelocityMmPerYear);
	HashMixDouble(UpliftHash, PhaseIIICFoldDirectionBeta);
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

			const FVector3d PreviousFoldDirection = OverVertex.FoldDirection;
			FVector3d RelativeFoldStep = FVector3d::ZeroVector;
			FVector3d ExpectedFoldDirection = OverVertex.FoldDirection;
			if (Motions.IsValidIndex(Mark.PlateId) && Motions.IsValidIndex(Mark.OtherPlateId))
			{
				const FVector3d UnderVelocity = FVector3d::CrossProduct(Motions[Mark.PlateId].Axis, OverVertex.UnitPosition) *
					Motions[Mark.PlateId].AngularSpeedRadiansPerStep;
				const FVector3d OverVelocity = FVector3d::CrossProduct(Motions[Mark.OtherPlateId].Axis, OverVertex.UnitPosition) *
					Motions[Mark.OtherPlateId].AngularSpeedRadiansPerStep;
				const FVector3d RawRelativeFoldStep = UnderVelocity - OverVelocity;
				RelativeFoldStep = RawRelativeFoldStep -
					OverVertex.UnitPosition * FVector3d::DotProduct(RawRelativeFoldStep, OverVertex.UnitPosition);
				if (RelativeFoldStep.SquaredLength() > UE_DOUBLE_SMALL_NUMBER)
				{
					ExpectedFoldDirection = RetangentAndNormalizeVectorField(
						PreviousFoldDirection + RelativeFoldStep * PhaseIIICFoldDirectionBeta,
						OverVertex.UnitPosition);
					OverVertex.FoldDirection = ExpectedFoldDirection;
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
			HashMixDouble(UpliftHash, PhaseIIICFoldDirectionBeta);
			HashMixDouble(UpliftHash, PreviousFoldDirection.X);
			HashMixDouble(UpliftHash, PreviousFoldDirection.Y);
			HashMixDouble(UpliftHash, PreviousFoldDirection.Z);
			HashMixDouble(UpliftHash, RelativeFoldStep.X);
			HashMixDouble(UpliftHash, RelativeFoldStep.Y);
			HashMixDouble(UpliftHash, RelativeFoldStep.Z);
			HashMixDouble(UpliftHash, ExpectedFoldDirection.X);
			HashMixDouble(UpliftHash, ExpectedFoldDirection.Y);
			HashMixDouble(UpliftHash, ExpectedFoldDirection.Z);
			HashMixDouble(UpliftHash, OverVertex.FoldDirection.X);
			HashMixDouble(UpliftHash, OverVertex.FoldDirection.Y);
			HashMixDouble(UpliftHash, OverVertex.FoldDirection.Z);

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
			Record.FoldInfluenceBeta = PhaseIIICFoldDirectionBeta;
			Record.OverUnitPosition = OverVertex.UnitPosition;
			Record.PreviousFoldDirection = PreviousFoldDirection;
			Record.RelativeFoldStep = RelativeFoldStep;
			Record.ExpectedFoldDirection = ExpectedFoldDirection;
			Record.NewFoldDirection = OverVertex.FoldDirection;
			Record.FoldDirectionMagnitude = OverVertex.FoldDirection.Size();
		}
	}

	LastPhaseIIIC3UpliftAudit.UniqueUpliftedVertexCount = UpliftedVertexKeys.Num();
	LastPhaseIIIC3UpliftAudit.UpliftHash = HashToString(UpliftHash);
}

void ACarrierLabVisualizationActor::ApplyPhaseIIICObductionUplift()
{
	LastPhaseIIICObductionUpliftAudit = FCarrierLabPhaseIIICObductionUpliftAudit();
	LastPhaseIIICObductionUpliftAudit.Step = CurrentMetrics.Step + 1;
	LastPhaseIIICObductionUpliftAudit.EventCount = CurrentMetrics.EventCount;
	LastPhaseIIICObductionUpliftAudit.PlateCount = State.Plates.Num();
	LastPhaseIIICObductionUpliftAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	LastPhaseIIICObductionUpliftAudit.bEnabled = bEnablePhaseIIICObductionUpliftBridge;
	LastPhaseIIICObductionUpliftAudit.bUpliftEnabled = bEnablePhaseIIICOverridingPlateUplift;
	LastPhaseIIICObductionUpliftAudit.MarkCount = State.ConvergenceObductionTriangleMarks.Num();
	LastPhaseIIICObductionUpliftAudit.DuplicateMarkCount = State.ConvergenceObductionTriangleMarkDuplicateCount;
	LastPhaseIIICObductionUpliftAudit.InvalidMarkCount = State.ConvergenceObductionTriangleMarkInvalidCount;
	LastPhaseIIICObductionUpliftAudit.EffectRadiusKm = PhaseIIICSubductionEffectRadiusKm;
	LastPhaseIIICObductionUpliftAudit.UpliftRateMmPerYear = PhaseIIICSubductionUpliftMmPerYear;
	LastPhaseIIICObductionUpliftAudit.ReferenceVelocityMmPerYear = PhaseIIICReferenceVelocityMmPerYear;
	LastPhaseIIICObductionUpliftAudit.FoldInfluenceBeta = PhaseIIICFoldDirectionBeta;
	LastPhaseIIICObductionUpliftAudit.TrenchDepthKm = PhaseIIICTrenchDepthKm;
	LastPhaseIIICObductionUpliftAudit.ContinentalMaxElevationKm = PhaseIIICMaxContinentalElevationKm;

	uint64 MarkHash = 1469598103934665603ull;
	HashMix(MarkHash, static_cast<uint64>(LastPhaseIIICObductionUpliftAudit.Step + 1));
	HashMix(MarkHash, static_cast<uint64>(State.ConvergenceObductionTriangleMarks.Num() + 1));
	uint64 UpliftHash = 1469598103934665603ull;
	HashMix(UpliftHash, static_cast<uint64>(LastPhaseIIICObductionUpliftAudit.Step + 1));
	HashMix(UpliftHash, static_cast<uint64>(State.ConvergenceObductionTriangleMarks.Num() + 1));
	HashMixDouble(UpliftHash, PhaseIIICSubductionUpliftMmPerYear);
	HashMixDouble(UpliftHash, PhaseIIICReferenceVelocityMmPerYear);
	HashMixDouble(UpliftHash, PhaseIIICFoldDirectionBeta);
	HashMixDouble(UpliftHash, PhaseIIICTrenchDepthKm);
	HashMixDouble(UpliftHash, PhaseIIICMaxContinentalElevationKm);
	HashMixDouble(UpliftHash, PhaseIIICSubductionEffectRadiusKm);

	TMap<uint64, CarrierLab::FConvergenceSubductionPolarityDecision> DecisionsByPair;
	for (const CarrierLab::FConvergenceSubductionPolarityDecision& Decision : State.ConvergenceSubductionPolarityDecisions)
	{
		DecisionsByPair.Add(Decision.PairKey, Decision);
	}
	for (const CarrierLab::FConvergenceSubductionTriangleHit& Hit : State.ConvergenceSubductionTriangleHits)
	{
		const CarrierLab::FConvergenceSubductionPolarityDecision* Decision = DecisionsByPair.Find(Hit.PairKey);
		if (Decision != nullptr &&
			Decision->DecisionClass == CarrierLab::EConvergenceSubductionPolarityClass::CollisionCandidate)
		{
			++LastPhaseIIICObductionUpliftAudit.CollisionCandidateHitCount;
		}
	}

	for (const CarrierLab::FConvergenceObductionTriangleMark& Mark : State.ConvergenceObductionTriangleMarks)
	{
		HashMix(MarkHash, static_cast<uint64>(Mark.MarkId + 1));
		HashMix(MarkHash, static_cast<uint64>(Mark.PlateId + 1));
		HashMix(MarkHash, static_cast<uint64>(Mark.OtherPlateId + 1));
		HashMix(MarkHash, static_cast<uint64>(Mark.LocalTriangleId + 1));
		HashMix(MarkHash, static_cast<uint64>(Mark.OtherLocalTriangleId + 1));
		HashMix(MarkHash, Mark.PairKey + 1ull);
		HashMixDouble(MarkHash, Mark.SignedConvergenceVelocity);
	}

	if (!bEnablePhaseIIICObductionUpliftBridge ||
		!bEnablePhaseIIICOverridingPlateUplift ||
		State.ConvergenceObductionTriangleMarks.IsEmpty())
	{
		LastPhaseIIICObductionUpliftAudit.ObductionMarkHash = HashToString(MarkHash);
		LastPhaseIIICObductionUpliftAudit.ObductionUpliftHash = HashToString(UpliftHash);
		return;
	}

	TSet<uint64> UpliftedVertexKeys;
	for (const CarrierLab::FConvergenceObductionTriangleMark& Mark : State.ConvergenceObductionTriangleMarks)
	{
		if (!State.Plates.IsValidIndex(Mark.PlateId) ||
			!State.Plates.IsValidIndex(Mark.OtherPlateId))
		{
			++LastPhaseIIICObductionUpliftAudit.InvalidInputCount;
			continue;
		}

		const CarrierLab::FCarrierPlate& SourcePlate = State.Plates[Mark.PlateId];
		CarrierLab::FCarrierPlate& DestinationPlate = State.Plates[Mark.OtherPlateId];
		if (!SourcePlate.LocalTriangles.IsValidIndex(Mark.LocalTriangleId))
		{
			++LastPhaseIIICObductionUpliftAudit.InvalidInputCount;
			continue;
		}

		const CarrierLab::FCarrierPlateTriangle& SourceTriangle = SourcePlate.LocalTriangles[Mark.LocalTriangleId];
		const int32 SourceVertexIds[3] = { SourceTriangle.A, SourceTriangle.B, SourceTriangle.C };
		bool bSourceVerticesValid = true;
		FVector3d SourceBarycenter = FVector3d::ZeroVector;
		double SourceElevationKm = 0.0;
		for (const int32 SourceVertexId : SourceVertexIds)
		{
			if (!SourcePlate.Vertices.IsValidIndex(SourceVertexId))
			{
				bSourceVerticesValid = false;
				break;
			}
			const CarrierLab::FCarrierVertex& SourceVertex = SourcePlate.Vertices[SourceVertexId];
			SourceBarycenter += SourceVertex.UnitPosition;
			SourceElevationKm += SourceVertex.Elevation;
		}
		if (!bSourceVerticesValid)
		{
			++LastPhaseIIICObductionUpliftAudit.InvalidInputCount;
			continue;
		}
		SourceBarycenter = NormalizeOrFallback(SourceBarycenter, SourcePlate.Vertices[SourceVertexIds[0]].UnitPosition);
		SourceElevationKm /= 3.0;

		const double SpeedTransfer = PhaseIIIC3SpeedTransfer(
			Mark.SignedConvergenceVelocity,
			PhaseIIICReferenceVelocityMmPerYear);
		const double ReliefTransfer = PhaseIIIC3ReliefTransfer(
			SourceElevationKm,
			PhaseIIICTrenchDepthKm,
			PhaseIIICMaxContinentalElevationKm);
		if (SpeedTransfer <= 0.0 || ReliefTransfer <= 0.0)
		{
			++LastPhaseIIICObductionUpliftAudit.InvalidInputCount;
			continue;
		}

		for (int32 DestinationVertexId = 0; DestinationVertexId < DestinationPlate.Vertices.Num(); ++DestinationVertexId)
		{
			CarrierLab::FCarrierVertex& DestinationVertex = DestinationPlate.Vertices[DestinationVertexId];
			if (DestinationVertex.ContinentalFraction <= 0.5)
			{
				++LastPhaseIIICObductionUpliftAudit.SkippedNonContinentalVertexCount;
				continue;
			}

			const double Dot = FMath::Clamp(FVector3d::DotProduct(SourceBarycenter, DestinationVertex.UnitPosition), -1.0, 1.0);
			const double DistanceKm = FMath::Acos(Dot) * EarthRadiusKm;
			if (!FMath::IsFinite(DistanceKm) || DistanceKm > PhaseIIICSubductionEffectRadiusKm)
			{
				++LastPhaseIIICObductionUpliftAudit.SkippedOutsideRadiusCount;
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
				++LastPhaseIIICObductionUpliftAudit.InvalidInputCount;
				continue;
			}

			const double PreviousElevationKm = DestinationVertex.Elevation;
			DestinationVertex.Elevation += DeltaKm;
			AddPhaseIIIC5ElevationLedgerRecord(
				ECarrierLabPhaseIIIC5ElevationLedgerClass::ObductionUplift,
				Mark.MarkId,
				Mark.OtherPlateId,
				Mark.PlateId,
				Mark.LocalTriangleId,
				DestinationVertexId,
				DestinationVertex.GlobalSampleId,
				PreviousElevationKm,
				DestinationVertex.Elevation,
				Mark.SignedConvergenceVelocity);

			const FVector3d PreviousFoldDirection = DestinationVertex.FoldDirection;
			FVector3d RelativeFoldStep = FVector3d::ZeroVector;
			FVector3d ExpectedFoldDirection = DestinationVertex.FoldDirection;
			if (Motions.IsValidIndex(Mark.PlateId) && Motions.IsValidIndex(Mark.OtherPlateId))
			{
				const FVector3d SourceVelocity = FVector3d::CrossProduct(Motions[Mark.PlateId].Axis, DestinationVertex.UnitPosition) *
					Motions[Mark.PlateId].AngularSpeedRadiansPerStep;
				const FVector3d DestinationVelocity = FVector3d::CrossProduct(Motions[Mark.OtherPlateId].Axis, DestinationVertex.UnitPosition) *
					Motions[Mark.OtherPlateId].AngularSpeedRadiansPerStep;
				const FVector3d RawRelativeFoldStep = SourceVelocity - DestinationVelocity;
				RelativeFoldStep = RawRelativeFoldStep -
					DestinationVertex.UnitPosition * FVector3d::DotProduct(RawRelativeFoldStep, DestinationVertex.UnitPosition);
				if (RelativeFoldStep.SquaredLength() > UE_DOUBLE_SMALL_NUMBER)
				{
					ExpectedFoldDirection = RetangentAndNormalizeVectorField(
						PreviousFoldDirection + RelativeFoldStep * PhaseIIICFoldDirectionBeta,
						DestinationVertex.UnitPosition);
					DestinationVertex.FoldDirection = ExpectedFoldDirection;
				}
			}

			const uint64 VertexKey = MakePlateVertexKey(Mark.OtherPlateId, DestinationVertexId);
			UpliftedVertexKeys.Add(VertexKey);
			++LastPhaseIIICObductionUpliftAudit.UpliftRecordCount;
			LastPhaseIIICObductionUpliftAudit.TotalAppliedDeltaKm += DeltaKm;
			LastPhaseIIICObductionUpliftAudit.MaxAppliedDeltaKm = FMath::Max(
				LastPhaseIIICObductionUpliftAudit.MaxAppliedDeltaKm,
				DeltaKm);

			HashMix(UpliftHash, static_cast<uint64>(Mark.MarkId + 1));
			HashMix(UpliftHash, static_cast<uint64>(Mark.PlateId + 1));
			HashMix(UpliftHash, static_cast<uint64>(Mark.OtherPlateId + 1));
			HashMix(UpliftHash, static_cast<uint64>(Mark.LocalTriangleId + 1));
			HashMix(UpliftHash, static_cast<uint64>(DestinationVertexId + 1));
			HashMix(UpliftHash, static_cast<uint64>(DestinationVertex.GlobalSampleId + 1));
			HashMixDouble(UpliftHash, DistanceKm);
			HashMixDouble(UpliftHash, Mark.SignedConvergenceVelocity);
			HashMixDouble(UpliftHash, SourceElevationKm);
			HashMixDouble(UpliftHash, PreviousElevationKm);
			HashMixDouble(UpliftHash, DeltaKm);
			HashMixDouble(UpliftHash, DestinationVertex.Elevation);
			HashMixDouble(UpliftHash, PhaseIIICFoldDirectionBeta);
			HashMixDouble(UpliftHash, PreviousFoldDirection.X);
			HashMixDouble(UpliftHash, PreviousFoldDirection.Y);
			HashMixDouble(UpliftHash, PreviousFoldDirection.Z);
			HashMixDouble(UpliftHash, RelativeFoldStep.X);
			HashMixDouble(UpliftHash, RelativeFoldStep.Y);
			HashMixDouble(UpliftHash, RelativeFoldStep.Z);
			HashMixDouble(UpliftHash, ExpectedFoldDirection.X);
			HashMixDouble(UpliftHash, ExpectedFoldDirection.Y);
			HashMixDouble(UpliftHash, ExpectedFoldDirection.Z);
			HashMixDouble(UpliftHash, DestinationVertex.FoldDirection.X);
			HashMixDouble(UpliftHash, DestinationVertex.FoldDirection.Y);
			HashMixDouble(UpliftHash, DestinationVertex.FoldDirection.Z);

			FCarrierLabPhaseIIIC3UpliftAuditRecord& Record = LastPhaseIIICObductionUpliftAudit.Records.AddDefaulted_GetRef();
			Record.MarkId = Mark.MarkId;
			Record.UnderPlateId = Mark.PlateId;
			Record.OverPlateId = Mark.OtherPlateId;
			Record.UnderLocalTriangleId = Mark.LocalTriangleId;
			Record.OverLocalVertexId = DestinationVertexId;
			Record.OverGlobalSampleId = DestinationVertex.GlobalSampleId;
			Record.DistanceKm = DistanceKm;
			Record.SignedConvergenceVelocity = Mark.SignedConvergenceVelocity;
			Record.HistoricalElevationKm = SourceElevationKm;
			Record.PreviousElevationKm = PreviousElevationKm;
			Record.AppliedDeltaKm = DeltaKm;
			Record.NewElevationKm = DestinationVertex.Elevation;
			Record.DistanceTransfer = DistanceTransfer;
			Record.SpeedTransfer = SpeedTransfer;
			Record.ReliefTransfer = ReliefTransfer;
			Record.FoldInfluenceBeta = PhaseIIICFoldDirectionBeta;
			Record.OverUnitPosition = DestinationVertex.UnitPosition;
			Record.PreviousFoldDirection = PreviousFoldDirection;
			Record.RelativeFoldStep = RelativeFoldStep;
			Record.ExpectedFoldDirection = ExpectedFoldDirection;
			Record.NewFoldDirection = DestinationVertex.FoldDirection;
			Record.FoldDirectionMagnitude = DestinationVertex.FoldDirection.Size();
		}
	}

	LastPhaseIIICObductionUpliftAudit.UniqueUpliftedVertexCount = UpliftedVertexKeys.Num();
	LastPhaseIIICObductionUpliftAudit.ObductionMarkHash = HashToString(MarkHash);
	LastPhaseIIICObductionUpliftAudit.ObductionUpliftHash = HashToString(UpliftHash);
}

bool ACarrierLabVisualizationActor::BuildPhaseIIIC4SlabPullOracleFromCurrentState(FCarrierLabPhaseIIIC4SlabPullAudit& OutAudit) const
{
	OutAudit = FCarrierLabPhaseIIIC4SlabPullAudit();
	OutAudit.Step = CurrentMetrics.Step;
	OutAudit.EventCount = CurrentMetrics.EventCount;
	OutAudit.PlateCount = State.Plates.Num();
	OutAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
	OutAudit.bMarksEnabled = bEnablePhaseIIICSubductingMarks;
	OutAudit.bSlabPullEnabled = true;
	OutAudit.MarkCount = State.ConvergenceSubductingTriangleMarks.Num();
	OutAudit.SlabPullSpeedMmPerYear = PhaseIIICSlabPullSpeedMmPerYear;
	OutAudit.ReferenceVelocityMmPerYear = PhaseIIICReferenceVelocityMmPerYear;
	OutAudit.SlabPullAngularStep = AngularSpeedRadiansPerStep(PhaseIIICSlabPullSpeedMmPerYear);
	OutAudit.MaxAllowedAngularStep = AngularSpeedRadiansPerStep(PhaseIIICReferenceVelocityMmPerYear);
	OutAudit.MotionHashBefore = ComputeMotionStateHash(Motions);

	uint64 SlabPullHash = 1469598103934665603ull;
	HashMix(SlabPullHash, static_cast<uint64>(OutAudit.Step + 1));
	HashMix(SlabPullHash, static_cast<uint64>(State.ConvergenceSubductingTriangleMarks.Num() + 1));
	HashMix(SlabPullHash, 1ull);
	HashMixDouble(SlabPullHash, PhaseIIICSlabPullSpeedMmPerYear);
	HashMixDouble(SlabPullHash, PhaseIIICReferenceVelocityMmPerYear);
	HashMixDouble(SlabPullHash, OutAudit.SlabPullAngularStep);
	HashMixDouble(SlabPullHash, OutAudit.MaxAllowedAngularStep);

	if (!bEnablePhaseIIICSubductingMarks || State.ConvergenceSubductingTriangleMarks.IsEmpty())
	{
		OutAudit.MotionHashAfter = OutAudit.MotionHashBefore;
		OutAudit.SlabPullHash = HashToString(SlabPullHash);
		return true;
	}

	TMap<int32, FVector3d> ContributionSumsByPlate;
	TMap<int32, int32> ContributionCountsByPlate;
	for (const CarrierLab::FConvergenceSubductingTriangleMark& Mark : State.ConvergenceSubductingTriangleMarks)
	{
		if (!State.Plates.IsValidIndex(Mark.PlateId) ||
			!State.Plates[Mark.PlateId].LocalTriangles.IsValidIndex(Mark.LocalTriangleId) ||
			!Motions.IsValidIndex(Mark.PlateId))
		{
			++OutAudit.InvalidInputCount;
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
			++OutAudit.InvalidInputCount;
			continue;
		}
		FrontBarycenter = NormalizeOrFallback(FrontBarycenter, Plate.Vertices[VertexIds[0]].UnitPosition);

		const FVector3d PlateCenter = Motions[Mark.PlateId].CurrentCenter;
		const FVector3d ContributionRaw = FVector3d::CrossProduct(PlateCenter, FrontBarycenter);
		if (ContributionRaw.SquaredLength() <= UE_DOUBLE_SMALL_NUMBER)
		{
			++OutAudit.InvalidInputCount;
			continue;
		}
		const FVector3d ContributionUnit = ContributionRaw.GetSafeNormal();
		ContributionSumsByPlate.FindOrAdd(Mark.PlateId) += ContributionUnit;
		ContributionCountsByPlate.FindOrAdd(Mark.PlateId) += 1;
		++OutAudit.ContributionCount;

		FCarrierLabPhaseIIIC4SlabPullContributionRecord& Record = OutAudit.Contributions.AddDefaulted_GetRef();
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

	TArray<FCarrierLabVisualizationMotion> ExpectedMotions = Motions;
	TArray<int32> AffectedPlateIds;
	ContributionSumsByPlate.GetKeys(AffectedPlateIds);
	AffectedPlateIds.Sort();
	OutAudit.AffectedPlateCount = AffectedPlateIds.Num();
	for (const int32 PlateId : AffectedPlateIds)
	{
		if (!ExpectedMotions.IsValidIndex(PlateId))
		{
			++OutAudit.InvalidInputCount;
			continue;
		}

		FCarrierLabVisualizationMotion& Motion = ExpectedMotions[PlateId];
		const FVector3d OldAxis = Motion.Axis;
		const double OldAngularSpeed = Motion.AngularSpeedRadiansPerStep;
		const FVector3d OldOmega = OldAxis * OldAngularSpeed;
		const FVector3d ContributionSum = ContributionSumsByPlate[PlateId];
		const FVector3d RawOmega = OldOmega + ContributionSum * OutAudit.SlabPullAngularStep;
		const double RawAngularSpeed = RawOmega.Size();

		FVector3d NewOmega = RawOmega;
		bool bClamped = false;
		if (RawAngularSpeed > OutAudit.MaxAllowedAngularStep && RawAngularSpeed > UE_DOUBLE_SMALL_NUMBER)
		{
			NewOmega = RawOmega / RawAngularSpeed * OutAudit.MaxAllowedAngularStep;
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
		OutAudit.MaxVelocityMmPerYear = FMath::Max(OutAudit.MaxVelocityMmPerYear, NewVelocity);

		FCarrierLabPhaseIIIC4SlabPullPlateRecord& Record = OutAudit.PlateRecords.AddDefaulted_GetRef();
		Record.PlateId = PlateId;
		Record.ContributionCount = ContributionCountsByPlate.Contains(PlateId) ? ContributionCountsByPlate[PlateId] : 0;
		Record.OldAxis = OldAxis;
		Record.NewAxis = Motion.Axis;
		Record.ContributionSum = ContributionSum;
		Record.OldAngularSpeedRadiansPerStep = OldAngularSpeed;
		Record.RawAngularSpeedRadiansPerStep = RawAngularSpeed;
		Record.NewAngularSpeedRadiansPerStep = Motion.AngularSpeedRadiansPerStep;
		Record.MaxAllowedAngularSpeedRadiansPerStep = OutAudit.MaxAllowedAngularStep;
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

	OutAudit.MotionHashAfter = ComputeMotionStateHash(ExpectedMotions);
	OutAudit.SlabPullHash = HashToString(SlabPullHash);
	return true;
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
		if (bEnablePhaseIIICObductionUpliftBridge)
		{
			UpdatePhaseIIICObductionTriangleMarks();
		}
	}
	if (bEnablePhaseIIICOverridingPlateUplift)
	{
		ApplyPhaseIIIC3OverridingPlateUplift();
		if (bEnablePhaseIIICObductionUpliftBridge)
		{
			ApplyPhaseIIICObductionUplift();
		}
		else
		{
			LastPhaseIIICObductionUpliftAudit = FCarrierLabPhaseIIICObductionUpliftAudit();
			LastPhaseIIICObductionUpliftAudit.Step = CurrentMetrics.Step + 1;
			LastPhaseIIICObductionUpliftAudit.EventCount = CurrentMetrics.EventCount;
			LastPhaseIIICObductionUpliftAudit.PlateCount = State.Plates.Num();
			LastPhaseIIICObductionUpliftAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
			LastPhaseIIICObductionUpliftAudit.bEnabled = false;
			LastPhaseIIICObductionUpliftAudit.bUpliftEnabled = true;
			LastPhaseIIICObductionUpliftAudit.MarkCount = State.ConvergenceObductionTriangleMarks.Num();
		}
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
		LastPhaseIIICObductionUpliftAudit = FCarrierLabPhaseIIICObductionUpliftAudit();
		LastPhaseIIICObductionUpliftAudit.Step = CurrentMetrics.Step + 1;
		LastPhaseIIICObductionUpliftAudit.EventCount = CurrentMetrics.EventCount;
		LastPhaseIIICObductionUpliftAudit.PlateCount = State.Plates.Num();
		LastPhaseIIICObductionUpliftAudit.ResetSerial = State.ConvergenceTrackingResetSerial;
		LastPhaseIIICObductionUpliftAudit.bEnabled = bEnablePhaseIIICObductionUpliftBridge;
		LastPhaseIIICObductionUpliftAudit.bUpliftEnabled = false;
		LastPhaseIIICObductionUpliftAudit.MarkCount = State.ConvergenceObductionTriangleMarks.Num();
	}
	ApplyPhaseIIIC4SlabPull();
	FinalizePhaseIIIC5ElevationLedger();
	++CurrentMetrics.Step;
	UpdateNaturalCadenceMetrics(true);
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

	for (const CarrierLab::FConvergenceObductionTriangleMark& Mark : State.ConvergenceObductionTriangleMarks)
	{
		if (!State.Plates.IsValidIndex(Mark.PlateId))
		{
			continue;
		}
		MarkTriangleVertices(State.Plates[Mark.PlateId], Mark.LocalTriangleId, 5);
	}

	for (const uint64 TriangleKey : State.ConvergenceCollisionPendingTriangleKeys)
	{
		const int32 PlateId = static_cast<int32>(TriangleKey >> 32);
		const int32 LocalTriangleId = static_cast<int32>(TriangleKey & 0xffffffffull);
		if (!State.Plates.IsValidIndex(PlateId))
		{
			continue;
		}
		MarkTriangleVertices(State.Plates[PlateId], LocalTriangleId, 6);
	}
}

void ACarrierLabVisualizationActor::UpdatePhaseIIIVisibilityMetrics()
{
	CurrentMetrics.PhaseIIIActiveBoundaryTriangleCount = 0;
	CurrentMetrics.PhaseIIIDistanceToFrontRecordCount = 0;
	CurrentMetrics.PhaseIIISubductionMatrixPairCount = State.ConvergenceSubductionMatrixPairKeys.Num();
	CurrentMetrics.PhaseIIISubductionMatrixEvidenceCount = State.ConvergenceSubductionMatrixEvidence.Num();
	CurrentMetrics.PhaseIIISubductionHitCount = State.ConvergenceSubductionTriangleHits.Num();
	CurrentMetrics.PhaseIIISubductingMarkCount = State.ConvergenceSubductingTriangleMarks.Num();
	CurrentMetrics.PhaseIIIObductionMarkCount = State.ConvergenceObductionTriangleMarks.Num();
	CurrentMetrics.PhaseIIICollisionPendingTriangleCount = State.ConvergenceCollisionPendingTriangleKeys.Num();
	CurrentMetrics.PhaseIIIHistoricalElevationSampleCount = 0;
	CurrentMetrics.PhaseIIIOceanicSampleCount = 0;
	CurrentMetrics.PhaseIIIRidgeDirectionSampleCount = 0;
	CurrentMetrics.PhaseIIIFoldDirectionSampleCount = 0;
	CurrentMetrics.PhaseIIIConvergenceResetSerial = State.ConvergenceTrackingResetSerial;
	CurrentMetrics.PhaseIIIMinVisibleElevationKm = 0.0;
	CurrentMetrics.PhaseIIIMaxVisibleElevationKm = 0.0;
	CurrentMetrics.PhaseIIIMaxOceanicAgeMa = 0.0;

	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		CurrentMetrics.PhaseIIIActiveBoundaryTriangleCount += Plate.ActiveBoundaryTriangles.Num();
		CurrentMetrics.PhaseIIIDistanceToFrontRecordCount += Plate.ActiveBoundaryTriangleDistancesKm.Num();
	}

	bool bHasElevation = false;
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (Sample.ContinentalFraction <= 1.0e-6)
		{
			++CurrentMetrics.PhaseIIIOceanicSampleCount;
		}
		if (Sample.RidgeDirection.SquaredLength() > UE_DOUBLE_SMALL_NUMBER)
		{
			++CurrentMetrics.PhaseIIIRidgeDirectionSampleCount;
		}
		if (Sample.FoldDirection.SquaredLength() > UE_DOUBLE_SMALL_NUMBER)
		{
			++CurrentMetrics.PhaseIIIFoldDirectionSampleCount;
		}
		if (Sample.HistoricalElevation != 0.0)
		{
			++CurrentMetrics.PhaseIIIHistoricalElevationSampleCount;
		}
		if (FMath::IsFinite(Sample.OceanicAge))
		{
			CurrentMetrics.PhaseIIIMaxOceanicAgeMa = FMath::Max(CurrentMetrics.PhaseIIIMaxOceanicAgeMa, Sample.OceanicAge);
		}
		if (FMath::IsFinite(Sample.Elevation))
		{
			CurrentMetrics.PhaseIIIMinVisibleElevationKm = bHasElevation
				? FMath::Min(CurrentMetrics.PhaseIIIMinVisibleElevationKm, Sample.Elevation)
				: Sample.Elevation;
			CurrentMetrics.PhaseIIIMaxVisibleElevationKm = bHasElevation
				? FMath::Max(CurrentMetrics.PhaseIIIMaxVisibleElevationKm, Sample.Elevation)
				: Sample.Elevation;
			bHasElevation = true;
		}
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

	for (const CarrierLab::FConvergenceObductionTriangleMark& Mark : State.ConvergenceObductionTriangleMarks)
	{
		AddTriangle(Mark.PlateId, Mark.LocalTriangleId, 5, -1.0, OutRoleTriangles);
	}

	for (const uint64 TriangleKey : State.ConvergenceCollisionPendingTriangleKeys)
	{
		const int32 PlateId = static_cast<int32>(TriangleKey >> 32);
		const int32 LocalTriangleId = static_cast<int32>(TriangleKey & 0xffffffffull);
		AddTriangle(PlateId, LocalTriangleId, 6, -1.0, OutRoleTriangles);
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
	UpdateNaturalCadenceMetrics(false);

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
	UpdatePhaseIIIVisibilityMetrics();
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
	case ECarrierLabVisualizationLayer::OceanicAgeHeatmap:
		if (!State.Samples.IsValidIndex(SampleId))
		{
			return FLinearColor(0.02f, 0.05f, 0.09f, 1.0f);
		}
		return State.Samples[SampleId].ContinentalFraction <= 1.0e-6
			? OceanicAgeColor(State.Samples[SampleId].OceanicAge)
			: DimMapBase(ContinentalColor(State.Samples[SampleId].ContinentalFraction));
	case ECarrierLabVisualizationLayer::RidgeDirection:
		return State.Samples.IsValidIndex(SampleId)
			? RidgeDirectionColor(State.Samples[SampleId].RidgeDirection, State.Samples[SampleId].UnitPosition)
			: FLinearColor(0.02f, 0.05f, 0.09f, 1.0f);
	case ECarrierLabVisualizationLayer::PhaseIIIERemeshSummary:
	{
		const CarrierLab::FSphereSample* Sample = State.Samples.IsValidIndex(SampleId) ? &State.Samples[SampleId] : nullptr;
		const double ContinentalFraction = RenderContinentalFractions.IsValidIndex(SampleId)
			? RenderContinentalFractions[SampleId]
			: (Sample != nullptr ? Sample->ContinentalFraction : 0.0);
		const double ElevationKm = RenderElevations.IsValidIndex(SampleId)
			? RenderElevations[SampleId]
			: (Sample != nullptr ? Sample->Elevation : 0.0);
		const uint8 RoleMask = SubductionRoleMask.IsValidIndex(SampleId) ? SubductionRoleMask[SampleId] : 0;
		const uint8 EventMask = PhaseIIIELiveRemeshEventMask.IsValidIndex(SampleId) ? PhaseIIIELiveRemeshEventMask[SampleId] : 0;
		return RemeshSummaryColor(
			ContinentalColor(ContinentalFraction),
			RoleMask,
			EventMask,
			ContinentalFraction,
			Sample != nullptr ? Sample->OceanicAge : 0.0,
			ElevationKm,
			Sample != nullptr ? Sample->RidgeDirection : FVector3d::ZeroVector,
			Sample != nullptr ? Sample->UnitPosition : FVector3d::UnitZ());
	}
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
	case ECarrierLabVisualizationLayer::OceanicAgeHeatmap:
		LayerName = TEXT("oceanic age heatmap");
		break;
	case ECarrierLabVisualizationLayer::RidgeDirection:
		LayerName = TEXT("ridge direction");
		break;
	case ECarrierLabVisualizationLayer::PhaseIIIERemeshSummary:
		LayerName = TEXT("phase iiie remesh summary");
		break;
	case ECarrierLabVisualizationLayer::PlateId:
	default:
		break;
	}

	return FString::Printf(
		TEXT("CarrierLab Phase III Viewer | %s | layer=%s\nstep=%d next_resample=%d events=%d iiie_auto_remesh=%s cadence=%d steps / %.1f Ma vmax/current=%.3f/%.3f mm/yr\nsamples=%d plates=%d miss=%d multi=%d boundary_vertices=%d boundary_degenerate=%d gap_fill=%d legacy_nonsep_hold=%d no_boundary_pair=%d policy_multi=%d nan=%d\niiie gen/apply/rift/nonpos/invalid/hold/coalesced/shared/nearest/dtie/tj=%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d hold_buckets within/crossEq/third=%d/%d/%d\nphaseIII active=%d dist_records=%d matrix_pairs/evidence=%d/%d hits=%d sub/obd/coll=%d/%d/%d reset=%d\ncrust ocean=%d ridge=%d fold=%d hist=%d elev=[%.3f, %.3f]km max_age=%.3fMa remesh_mode=%s\nAuthCAF=%.6f ProjCAF=%.6f drift_mean=%.9fkm drift_p95=%.9fkm hash=%s crust_hash=%s conv_hash=%s\nprojection=%.3fs bvh=%.3fs query=%.3fs drift=%.3fs boundary=%.3fs hash_time=%.3fs render=%.3fs resample=%.3fs\nSpace play/pause | . step | R IIIE.6 remesh | 1-9/0 layers | O ocean age | G ridge"),
		bPlaying ? TEXT("PLAY") : TEXT("PAUSED"),
		LayerName,
		CurrentMetrics.Step,
		CurrentMetrics.NextResampleStep,
		CurrentMetrics.EventCount,
		bEnableNaturalResamplingEvents ? TEXT("on") : TEXT("off"),
		CurrentMetrics.CadenceSteps,
		CurrentMetrics.CadenceDeltaTMa,
		CurrentMetrics.ObservedMaxPlateSpeedSinceLastRemeshMmPerYear,
		CurrentMetrics.ObservedMaxPlateSpeedMmPerYear,
		CurrentMetrics.SampleCount,
		CurrentMetrics.PlateCount,
		CurrentMetrics.RawMissCount,
		CurrentMetrics.RawMultiHitCount,
		CurrentMetrics.BoundaryVertexCount,
		CurrentMetrics.BoundaryHitCount,
		CurrentMetrics.LastGapFillCount,
		CurrentMetrics.LastNonSeparatingGapFillCount,
		CurrentMetrics.LastNoBoundaryPairMissCount,
		CurrentMetrics.PolicyResolvedMultiHitCount,
		CurrentMetrics.NaNOrInfCount,
		CurrentMetrics.PhaseIIIELastGeneratedCandidateCount,
		CurrentMetrics.PhaseIIIELastAppliedGeneratedCount,
		CurrentMetrics.PhaseIIIELastRiftingPendingCount,
		CurrentMetrics.PhaseIIIELastGeneratedWithNonPositiveSeparationCount,
		CurrentMetrics.PhaseIIIELastInvalidRecordCount,
		CurrentMetrics.PhaseIIIELastUnresolvedMultiHitHoldCount,
		CurrentMetrics.PhaseIIIELastCoalescedMultiHitCount,
		CurrentMetrics.PhaseIIIELastSharedBoundaryTieBreakCount,
		CurrentMetrics.PhaseIIIELastNearestHitTieBreakCount,
		CurrentMetrics.PhaseIIIELastDistanceTieFallbackCount,
		CurrentMetrics.PhaseIIIELastTripleJunctionSplitCount,
		CurrentMetrics.PhaseIIIELastWithinPlateCoincidentHoldCount,
		CurrentMetrics.PhaseIIIELastCrossPlateEqualHoldCount,
		CurrentMetrics.PhaseIIIELastThirdPlateHoldCount,
		CurrentMetrics.PhaseIIIActiveBoundaryTriangleCount,
		CurrentMetrics.PhaseIIIDistanceToFrontRecordCount,
		CurrentMetrics.PhaseIIISubductionMatrixPairCount,
		CurrentMetrics.PhaseIIISubductionMatrixEvidenceCount,
		CurrentMetrics.PhaseIIISubductionHitCount,
		CurrentMetrics.PhaseIIISubductingMarkCount,
		CurrentMetrics.PhaseIIIObductionMarkCount,
		CurrentMetrics.PhaseIIICollisionPendingTriangleCount,
		CurrentMetrics.PhaseIIIConvergenceResetSerial,
		CurrentMetrics.PhaseIIIOceanicSampleCount,
		CurrentMetrics.PhaseIIIRidgeDirectionSampleCount,
		CurrentMetrics.PhaseIIIFoldDirectionSampleCount,
		CurrentMetrics.PhaseIIIHistoricalElevationSampleCount,
		CurrentMetrics.PhaseIIIMinVisibleElevationKm,
		CurrentMetrics.PhaseIIIMaxVisibleElevationKm,
		CurrentMetrics.PhaseIIIMaxOceanicAgeMa,
		CurrentMetrics.LastRemeshMode.IsEmpty() ? TEXT("none") : *CurrentMetrics.LastRemeshMode,
		CurrentMetrics.AuthoritativeCAF,
		CurrentMetrics.ProjectedCAF,
		CurrentMetrics.DriftErrorMeanKm,
		CurrentMetrics.DriftErrorP95Km,
		*CurrentMetrics.LastHash,
		*CurrentMetrics.CrustStateHash,
		*CurrentMetrics.ConvergenceTrackingHash,
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
