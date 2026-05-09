// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIE4Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr double EarthRadiusKm = 6371.0;
	constexpr double VectorTolerance = 1.0e-8;
	constexpr double ScalarTolerance = 1.0e-9;
	constexpr double DistanceToleranceKm = 1.0e-6;
	constexpr double OceanicAgeTolerance = 1.0e-12;
	constexpr double RidgePeakElevationKm = -1.0;
	constexpr double AbyssalElevationKm = -6.0;
	constexpr double ZGammaGeophysicsReferenceDistanceKm = 3000.0;

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

	FVector3d NormalizeOrFallback(const FVector3d& Vector, const FVector3d& Fallback)
	{
		const double Size = Vector.Size();
		return Size > UE_DOUBLE_SMALL_NUMBER ? Vector / Size : Fallback;
	}

	double UnitAngularDistanceRadians(const FVector3d& A, const FVector3d& B)
	{
		return FMath::Acos(FMath::Clamp(FVector3d::DotProduct(A, B), -1.0, 1.0));
	}

	double DistanceKm(const FVector3d& A, const FVector3d& B)
	{
		return EarthRadiusKm * UnitAngularDistanceRadians(NormalizeOrFallback(A, FVector3d::UnitX()), NormalizeOrFallback(B, FVector3d::UnitX()));
	}

	double ZGammaProfileTForDistanceKm(const double DistanceKmValue)
	{
		return ZGammaGeophysicsReferenceDistanceKm > UE_DOUBLE_SMALL_NUMBER
			? FMath::Sqrt(FMath::Clamp(DistanceKmValue / ZGammaGeophysicsReferenceDistanceKm, 0.0, 1.0))
			: 0.0;
	}

	double ZGammaForDistanceKm(const double DistanceKmValue)
	{
		return FMath::Lerp(
			RidgePeakElevationKm,
			AbyssalElevationKm,
			ZGammaProfileTForDistanceKm(DistanceKmValue));
	}

	double BlendElevation(const double Alpha, const double ZBar, const double ZGamma)
	{
		return Alpha * ZBar + (1.0 - Alpha) * ZGamma;
	}

	FVector3d UnitFromLonLat(const double LonDeg, const double LatDeg)
	{
		const double Lon = FMath::DegreesToRadians(LonDeg);
		const double Lat = FMath::DegreesToRadians(LatDeg);
		const double CosLat = FMath::Cos(Lat);
		return FVector3d(CosLat * FMath::Cos(Lon), CosLat * FMath::Sin(Lon), FMath::Sin(Lat));
	}

	FVector3d RetangentAndNormalize(const FVector3d& Vector, const FVector3d& UnitPosition)
	{
		const FVector3d Tangent = Vector - UnitPosition * FVector3d::DotProduct(Vector, UnitPosition);
		const double Size = Tangent.Size();
		return Size > UE_DOUBLE_SMALL_NUMBER ? Tangent / Size : FVector3d::ZeroVector;
	}

	FString SelectionClassName(const ECarrierLabPhaseIIIE3SelectionClass SelectionClass)
	{
		switch (SelectionClass)
		{
		case ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap:
			return TEXT("no-hit divergent gap");
		case ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit:
			return TEXT("resolved single hit");
		case ECarrierLabPhaseIIIE3SelectionClass::DivergentGapAfterFiltering:
			return TEXT("divergent gap after filtering");
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit:
			return TEXT("unresolved same-material multi-hit");
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedMixedMaterialMultiHit:
			return TEXT("unresolved mixed-material multi-hit");
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedThirdPlateMultiHit:
			return TEXT("unresolved third-plate multi-hit");
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

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			OutputRoot = FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("CarrierLab"),
				TEXT("PhaseIII"),
				TEXT("IIIE4"));
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
				TEXT("phase-iii-slice-iiie4-report.md"));
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
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	struct FFixtureSpec
	{
		FString Name;
		FString Purpose;
		FVector3d Sample = FVector3d::UnitX();
		TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe> Edges;
		ECarrierLabPhaseIIIE3SelectionClass SourceSelectionClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
		double SignedSeparationVelocity = 0.002;
		bool bRestoreNonSeparatingAnomalyVeto = false;
		bool bExpectGenerated = true;
		bool bExpectNonSeparatingAnomaly = false;
		bool bExpectGeneratedWithNonPositiveSeparation = false;
		bool bExpectRejectedNonDivergentRoute = false;
		bool bExpectRejectedUnresolvedMultiHit = false;
		bool bExpectBoundaryPairFound = true;
		int32 ExpectedQ1PlateId = 0;
		int32 ExpectedQ2PlateId = 1;
		int32 ExpectedAssignedPlateId = 0;
		FVector3d ExpectedQ1 = FVector3d::UnitX();
		FVector3d ExpectedQ2 = FVector3d::UnitY();
		FVector3d ExpectedQGamma = FVector3d::UnitX();
		double ExpectedQ1Elevation = -4.0;
		double ExpectedQ2Elevation = -2.0;
		double ExpectedQ1DistanceKm = 0.0;
		double ExpectedQ2DistanceKm = 0.0;
		double ExpectedRidgeDistanceKm = 0.0;
		double ExpectedNearestBoundaryDistanceKm = 0.0;
		double ExpectedAlpha = 0.0;
		double ExpectedZBar = 0.0;
		double ExpectedZGamma = 0.0;
		double ExpectedZGammaProfileDistanceKm = 0.0;
		double ExpectedZGammaProfileReferenceDistanceKm = 0.0;
		double ExpectedZGammaProfileT = 0.0;
		double ExpectedElevation = 0.0;
		double ExpectedOceanicAge = 0.0;
		FVector3d ExpectedRidgeDirection = FVector3d::ZeroVector;
		FString ExpectedRecordHash;
		bool bExpectDegenerateRidgeDirection = false;
		bool bExpectZGammaDistanceProfilePlaceholder = false;
		bool bExpectZGammaGeophysicsDerivedProfile = true;
		bool bExpectPaperFaithfulZGammaProfile = false;
	};

	struct FFixtureResult
	{
		FString Name;
		FString Purpose;
		bool bQueryReturned = false;
		bool bPass = false;
		double Seconds = 0.0;
		double ElevationResidual = 0.0;
		double OceanicAgeResidual = 0.0;
		double RidgeDirectionResidual = 0.0;
		double RidgeRadialDot = 0.0;
		double AlphaResidual = 0.0;
		double ZBarResidual = 0.0;
		double ZGammaResidual = 0.0;
		double ZGammaProfileDistanceResidual = 0.0;
		double ZGammaProfileReferenceResidual = 0.0;
		double ZGammaProfileTResidual = 0.0;
		double Q1DistanceResidual = 0.0;
		double Q2DistanceResidual = 0.0;
		double RidgeDistanceResidual = 0.0;
		double NearestBoundaryDistanceResidual = 0.0;
		double QGammaResidual = 0.0;
		FString RecordHash;
		FString ExpectedRecordHash;
		bool bRecordHashMatch = false;
		FCarrierLabPhaseIIIE4OceanicGenerationRecord Record;
	};

	struct FWidthInvariantResult
	{
		bool bPass = false;
		double RidgeDistanceDeltaKm = 0.0;
		double NearestBoundaryDistanceDeltaKm = 0.0;
		double AlphaDelta = 0.0;
		double ZGammaDelta = 0.0;
		double ZGammaProfileTDelta = 0.0;
	};

	void AddPointEdge(
		FFixtureSpec& Fixture,
		const int32 PlateId,
		const double LonDeg,
		const double LatDeg,
		const double Elevation)
	{
		FCarrierLabPhaseIIIE2BoundaryEdgeProbe Edge;
		Edge.PlateId = PlateId;
		Edge.StartUnitPosition = UnitFromLonLat(LonDeg, LatDeg);
		Edge.EndUnitPosition = Edge.StartUnitPosition;
		Edge.StartElevation = Elevation;
		Edge.EndElevation = Elevation;
		Fixture.Edges.Add(Edge);
	}

	void AddDefaultDivergentEdges(FFixtureSpec& Fixture)
	{
		AddPointEdge(Fixture, 0, -20.0, 0.0, -4.0);
		AddPointEdge(Fixture, 1, 20.0, 0.0, -2.0);
		Fixture.ExpectedQ1PlateId = 0;
		Fixture.ExpectedQ2PlateId = 1;
		Fixture.ExpectedAssignedPlateId = 0;
		Fixture.ExpectedQ1 = UnitFromLonLat(-20.0, 0.0);
		Fixture.ExpectedQ2 = UnitFromLonLat(20.0, 0.0);
		Fixture.ExpectedQGamma = FVector3d(1.0, 0.0, 0.0);
		Fixture.ExpectedQ1Elevation = -4.0;
		Fixture.ExpectedQ2Elevation = -2.0;
		Fixture.ExpectedQ1DistanceKm = 2476.1714106209579;
		Fixture.ExpectedQ2DistanceKm = 2476.1714106209579;
		Fixture.ExpectedRidgeDistanceKm = 1111.9492664455888;
		Fixture.ExpectedNearestBoundaryDistanceKm = 2476.1714106209579;
		Fixture.ExpectedAlpha = 0.30989739936914784;
		Fixture.ExpectedZBar = -3.0;
		Fixture.ExpectedZGammaProfileDistanceKm = 1111.9492664455888;
		Fixture.ExpectedZGammaProfileReferenceDistanceKm = ZGammaGeophysicsReferenceDistanceKm;
		Fixture.ExpectedZGammaProfileT = ZGammaProfileTForDistanceKm(Fixture.ExpectedZGammaProfileDistanceKm);
		Fixture.ExpectedZGamma = ZGammaForDistanceKm(Fixture.ExpectedZGammaProfileDistanceKm);
		Fixture.ExpectedElevation = BlendElevation(Fixture.ExpectedAlpha, Fixture.ExpectedZBar, Fixture.ExpectedZGamma);
		Fixture.ExpectedOceanicAge = 0.0;
		Fixture.ExpectedRidgeDirection = FVector3d(0.0, 1.0, 0.0);
	}

	void SetEmptyBoundaryExpectations(FFixtureSpec& Fixture)
	{
		Fixture.ExpectedQ1PlateId = INDEX_NONE;
		Fixture.ExpectedQ2PlateId = INDEX_NONE;
		Fixture.ExpectedAssignedPlateId = INDEX_NONE;
		Fixture.ExpectedQ1 = FVector3d::UnitZ();
		Fixture.ExpectedQ2 = FVector3d::UnitZ();
		Fixture.ExpectedQGamma = FVector3d::UnitZ();
		Fixture.ExpectedQ1Elevation = 0.0;
		Fixture.ExpectedQ2Elevation = 0.0;
		Fixture.ExpectedQ1DistanceKm = 0.0;
		Fixture.ExpectedQ2DistanceKm = 0.0;
		Fixture.ExpectedRidgeDistanceKm = 0.0;
		Fixture.ExpectedNearestBoundaryDistanceKm = 0.0;
		Fixture.ExpectedAlpha = 0.0;
		Fixture.ExpectedZBar = 0.0;
		Fixture.ExpectedZGamma = 0.0;
		Fixture.ExpectedZGammaProfileDistanceKm = 0.0;
		Fixture.ExpectedZGammaProfileReferenceDistanceKm = 0.0;
		Fixture.ExpectedZGammaProfileT = 0.0;
		Fixture.ExpectedElevation = 0.0;
		Fixture.ExpectedOceanicAge = 0.0;
		Fixture.ExpectedRidgeDirection = FVector3d::ZeroVector;
		Fixture.bExpectZGammaDistanceProfilePlaceholder = false;
		Fixture.bExpectZGammaGeophysicsDerivedProfile = false;
		Fixture.bExpectPaperFaithfulZGammaProfile = false;
	}

	FString ComputeRecordHash(const FCarrierLabPhaseIIIE4OceanicGenerationRecord& Record)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMix(Hash, static_cast<uint64>(static_cast<uint8>(Record.SourceSelectionClass)) + 1ull);
		HashMix(Hash, Record.bGeneratedOceanicCrust ? 2ull : 1ull);
		HashMix(Hash, Record.bRejectedNonDivergentRoute ? 2ull : 1ull);
		HashMix(Hash, Record.bRejectedUnresolvedMultiHit ? 2ull : 1ull);
		HashMix(Hash, Record.bBoundaryPairFound ? 2ull : 1ull);
		HashMix(Hash, Record.bNonSeparatingAnomaly ? 2ull : 1ull);
		HashMix(Hash, Record.bGeneratedWithNonPositiveSeparation ? 2ull : 1ull);
		HashMix(Hash, Record.bUsedZGammaDistanceProfilePlaceholder ? 2ull : 1ull);
		HashMix(Hash, Record.bUsedZGammaGeophysicsDerivedProfile ? 2ull : 1ull);
		HashMix(Hash, Record.bPaperFaithfulZGammaProfile ? 2ull : 1ull);
		HashMix(Hash, static_cast<uint64>(Record.Q1PlateId + 2));
		HashMix(Hash, static_cast<uint64>(Record.Q2PlateId + 2));
		HashMix(Hash, static_cast<uint64>(Record.Q1EdgeId + 2));
		HashMix(Hash, static_cast<uint64>(Record.Q2EdgeId + 2));
		HashMix(Hash, static_cast<uint64>(Record.AssignedPlateId + 2));
		HashMixDouble(Hash, Record.SignedSeparationVelocity);
		HashMixDouble(Hash, Record.Q1DistanceKm);
		HashMixDouble(Hash, Record.Q2DistanceKm);
		HashMixDouble(Hash, Record.RidgeDistanceKm);
		HashMixDouble(Hash, Record.NearestBoundaryDistanceKm);
		HashMixDouble(Hash, Record.Alpha);
		HashMixDouble(Hash, Record.ZBarElevation);
		HashMixDouble(Hash, Record.ZGammaElevation);
		HashMixDouble(Hash, Record.ZGammaProfileDistanceKm);
		HashMixDouble(Hash, Record.ZGammaProfileReferenceDistanceKm);
		HashMixDouble(Hash, Record.ZGammaProfileT);
		HashMixDouble(Hash, Record.Elevation);
		HashMixDouble(Hash, Record.OceanicAge);
		HashMixDouble(Hash, Record.RidgeDirection.X);
		HashMixDouble(Hash, Record.RidgeDirection.Y);
		HashMixDouble(Hash, Record.RidgeDirection.Z);
		return HashToString(Hash);
	}

	bool VerifyAgainstFixtureExpectations(
		const FFixtureSpec& Fixture,
		FFixtureResult& OutResult)
	{
		OutResult.Q1DistanceResidual = FMath::Abs(OutResult.Record.Q1DistanceKm - Fixture.ExpectedQ1DistanceKm);
		OutResult.Q2DistanceResidual = FMath::Abs(OutResult.Record.Q2DistanceKm - Fixture.ExpectedQ2DistanceKm);
		OutResult.RidgeDistanceResidual = FMath::Abs(OutResult.Record.RidgeDistanceKm - Fixture.ExpectedRidgeDistanceKm);
		OutResult.NearestBoundaryDistanceResidual = FMath::Abs(OutResult.Record.NearestBoundaryDistanceKm - Fixture.ExpectedNearestBoundaryDistanceKm);
		OutResult.QGammaResidual = (OutResult.Record.QGammaUnitPosition - Fixture.ExpectedQGamma).Size();
		OutResult.AlphaResidual = FMath::Abs(OutResult.Record.Alpha - Fixture.ExpectedAlpha);
		OutResult.ZBarResidual = FMath::Abs(OutResult.Record.ZBarElevation - Fixture.ExpectedZBar);
		OutResult.ZGammaResidual = FMath::Abs(OutResult.Record.ZGammaElevation - Fixture.ExpectedZGamma);
		OutResult.ZGammaProfileDistanceResidual = FMath::Abs(OutResult.Record.ZGammaProfileDistanceKm - Fixture.ExpectedZGammaProfileDistanceKm);
		OutResult.ZGammaProfileReferenceResidual = FMath::Abs(OutResult.Record.ZGammaProfileReferenceDistanceKm - Fixture.ExpectedZGammaProfileReferenceDistanceKm);
		OutResult.ZGammaProfileTResidual = FMath::Abs(OutResult.Record.ZGammaProfileT - Fixture.ExpectedZGammaProfileT);
		OutResult.ElevationResidual = FMath::Abs(OutResult.Record.Elevation - Fixture.ExpectedElevation);
		OutResult.OceanicAgeResidual = FMath::Abs(OutResult.Record.OceanicAge - Fixture.ExpectedOceanicAge);
		OutResult.RidgeDirectionResidual = Fixture.bExpectDegenerateRidgeDirection
			? OutResult.Record.RidgeDirection.Size()
			: (OutResult.Record.RidgeDirection - Fixture.ExpectedRidgeDirection).Size();
		OutResult.RidgeRadialDot = FMath::Abs(FVector3d::DotProduct(
			OutResult.Record.RidgeDirection,
			NormalizeOrFallback(Fixture.Sample, FVector3d::UnitX())));
		OutResult.bRecordHashMatch = OutResult.RecordHash == Fixture.ExpectedRecordHash;

		bool bPass =
			OutResult.Record.Q1PlateId == Fixture.ExpectedQ1PlateId &&
			OutResult.Record.Q2PlateId == Fixture.ExpectedQ2PlateId &&
			OutResult.Record.AssignedPlateId == Fixture.ExpectedAssignedPlateId &&
			OutResult.Record.bUsedZGammaDistanceProfilePlaceholder == Fixture.bExpectZGammaDistanceProfilePlaceholder &&
			OutResult.Record.bUsedZGammaGeophysicsDerivedProfile == Fixture.bExpectZGammaGeophysicsDerivedProfile &&
			OutResult.Record.bPaperFaithfulZGammaProfile == Fixture.bExpectPaperFaithfulZGammaProfile &&
			(OutResult.Record.Q1UnitPosition - Fixture.ExpectedQ1).Size() <= VectorTolerance &&
			(OutResult.Record.Q2UnitPosition - Fixture.ExpectedQ2).Size() <= VectorTolerance &&
			OutResult.QGammaResidual <= VectorTolerance &&
			FMath::Abs(OutResult.Record.Q1Elevation - Fixture.ExpectedQ1Elevation) <= ScalarTolerance &&
			FMath::Abs(OutResult.Record.Q2Elevation - Fixture.ExpectedQ2Elevation) <= ScalarTolerance &&
			OutResult.Q1DistanceResidual <= DistanceToleranceKm &&
			OutResult.Q2DistanceResidual <= DistanceToleranceKm &&
			OutResult.RidgeDistanceResidual <= DistanceToleranceKm &&
			OutResult.NearestBoundaryDistanceResidual <= DistanceToleranceKm &&
			OutResult.AlphaResidual <= ScalarTolerance &&
			OutResult.ZBarResidual <= ScalarTolerance &&
			OutResult.ZGammaResidual <= ScalarTolerance &&
			OutResult.ZGammaProfileDistanceResidual <= DistanceToleranceKm &&
			OutResult.ZGammaProfileReferenceResidual <= DistanceToleranceKm &&
			OutResult.ZGammaProfileTResidual <= ScalarTolerance &&
			OutResult.ElevationResidual <= ScalarTolerance &&
			OutResult.OceanicAgeResidual <= OceanicAgeTolerance &&
			OutResult.RidgeDirectionResidual <= VectorTolerance &&
			OutResult.bRecordHashMatch;

		if (Fixture.bExpectGenerated)
		{
			bPass =
				bPass &&
				(Fixture.bExpectGeneratedWithNonPositiveSeparation
					? OutResult.Record.SignedSeparationVelocity <= 0.0
					: OutResult.Record.SignedSeparationVelocity > 0.0) &&
				(Fixture.bExpectDegenerateRidgeDirection
					? OutResult.Record.RidgeDirectionMagnitude <= VectorTolerance
					: OutResult.Record.RidgeDirectionMagnitude > 1.0 - VectorTolerance) &&
				OutResult.RidgeRadialDot <= VectorTolerance;
		}

		return bPass;
	}

	bool RunFixture(
		ACarrierLabVisualizationActor& Actor,
		const FFixtureSpec& Fixture,
		FFixtureResult& OutResult)
	{
		OutResult = FFixtureResult();
		OutResult.Name = Fixture.Name;
		OutResult.Purpose = Fixture.Purpose;
		OutResult.ExpectedRecordHash = Fixture.ExpectedRecordHash;
		Actor.bRestoreNonSeparatingAnomalyVeto = Fixture.bRestoreNonSeparatingAnomalyVeto;
		const double StartSeconds = FPlatformTime::Seconds();
		OutResult.bQueryReturned = Actor.QueryPhaseIIIE4DivergentOceanicFieldsForTest(
			Fixture.Sample,
			Fixture.SourceSelectionClass,
			Fixture.Edges,
			Fixture.SignedSeparationVelocity,
			OutResult.Record);
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.RecordHash = ComputeRecordHash(OutResult.Record);
		OutResult.bRecordHashMatch = OutResult.RecordHash == Fixture.ExpectedRecordHash;

		OutResult.bPass =
			OutResult.bQueryReturned &&
			OutResult.Record.bGeneratedOceanicCrust == Fixture.bExpectGenerated &&
			OutResult.Record.bNonSeparatingAnomaly == Fixture.bExpectNonSeparatingAnomaly &&
			OutResult.Record.bGeneratedWithNonPositiveSeparation == Fixture.bExpectGeneratedWithNonPositiveSeparation &&
			OutResult.Record.bRejectedNonDivergentRoute == Fixture.bExpectRejectedNonDivergentRoute &&
			OutResult.Record.bRejectedUnresolvedMultiHit == Fixture.bExpectRejectedUnresolvedMultiHit &&
			OutResult.Record.bBoundaryPairFound == Fixture.bExpectBoundaryPairFound &&
			OutResult.Record.bUsedPolicyWinner == false &&
			OutResult.Record.bUsedPriorOwnerFallback == false &&
			VerifyAgainstFixtureExpectations(Fixture, OutResult);

		return OutResult.bPass;
	}

	FFixtureSpec MakeNoHitGenerationFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("no-hit divergent oceanic generation");
		Fixture.Purpose = TEXT("a IIIE.3 no-hit divergent gap uses IIIE.2 q1/q2/qGamma to create age-zero oceanic fields");
		Fixture.Sample = UnitFromLonLat(0.0, 10.0);
		Fixture.SourceSelectionClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
		AddDefaultDivergentEdges(Fixture);
		Fixture.ExpectedRecordHash = TEXT("2f869587566161f9");
		return Fixture;
	}

	FFixtureSpec MakeFilterExhaustedGenerationFixture()
	{
		FFixtureSpec Fixture = MakeNoHitGenerationFixture();
		Fixture.Name = TEXT("filter-exhausted divergent oceanic generation");
		Fixture.Purpose = TEXT("a zero-valid-hit-after-filtering route creates the same paper fields without prior-owner fallback");
		Fixture.SourceSelectionClass = ECarrierLabPhaseIIIE3SelectionClass::DivergentGapAfterFiltering;
		Fixture.ExpectedRecordHash = TEXT("240d4374070ed0cb");
		return Fixture;
	}

	FFixtureSpec MakeNonSeparatingAnomalyFixture()
	{
		FFixtureSpec Fixture = MakeNoHitGenerationFixture();
		Fixture.Name = TEXT("non-positive separation generation observability");
		Fixture.Purpose = TEXT("paper-literal zero-hit generation continues when the q1/q2 signed separating velocity is non-positive and records the condition diagnostically");
		Fixture.SignedSeparationVelocity = -0.002;
		Fixture.bExpectGeneratedWithNonPositiveSeparation = true;
		Fixture.ExpectedRecordHash = TEXT("d8d7e6a7a434cbd0");
		return Fixture;
	}

	FFixtureSpec MakeRestoredNonSeparatingVetoFixture()
	{
		FFixtureSpec Fixture = MakeNoHitGenerationFixture();
		Fixture.Name = TEXT("restored non-separating veto baseline");
		Fixture.Purpose = TEXT("the explicit legacy opt-out reproduces the old CarrierLab-invented non-separating anomaly hold for historical baseline audits");
		Fixture.SignedSeparationVelocity = -0.002;
		Fixture.bRestoreNonSeparatingAnomalyVeto = true;
		Fixture.bExpectGenerated = false;
		Fixture.bExpectNonSeparatingAnomaly = true;
		Fixture.ExpectedAssignedPlateId = INDEX_NONE;
		Fixture.ExpectedRidgeDistanceKm = 0.0;
		Fixture.ExpectedNearestBoundaryDistanceKm = 0.0;
		Fixture.ExpectedAlpha = 0.0;
		Fixture.ExpectedZBar = 0.0;
		Fixture.ExpectedZGamma = 0.0;
		Fixture.ExpectedZGammaProfileDistanceKm = 0.0;
		Fixture.ExpectedZGammaProfileReferenceDistanceKm = 0.0;
		Fixture.ExpectedZGammaProfileT = 0.0;
		Fixture.ExpectedElevation = 0.0;
		Fixture.ExpectedRidgeDirection = FVector3d::ZeroVector;
		Fixture.bExpectZGammaDistanceProfilePlaceholder = false;
		Fixture.bExpectZGammaGeophysicsDerivedProfile = false;
		Fixture.bExpectPaperFaithfulZGammaProfile = false;
		Fixture.ExpectedRecordHash = TEXT("bd36f88f2284a7bc");
		return Fixture;
	}

	FFixtureSpec MakeResolvedRouteRejectedFixture()
	{
		FFixtureSpec Fixture = MakeNoHitGenerationFixture();
		Fixture.Name = TEXT("resolved single-hit route rejected");
		Fixture.Purpose = TEXT("IIIE.4 does not generate oceanic fields for a normal IIIE.3 barycentric transfer route");
		Fixture.SourceSelectionClass = ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit;
		Fixture.bExpectGenerated = false;
		Fixture.bExpectRejectedNonDivergentRoute = true;
		Fixture.bExpectBoundaryPairFound = false;
		SetEmptyBoundaryExpectations(Fixture);
		Fixture.ExpectedRecordHash = TEXT("e470b046e7d717b0");
		return Fixture;
	}

	FFixtureSpec MakeUnresolvedRouteRejectedFixture()
	{
		FFixtureSpec Fixture = MakeNoHitGenerationFixture();
		Fixture.Name = TEXT("unresolved multi-hit route rejected");
		Fixture.Purpose = TEXT("unresolved multi-hit samples stay stop conditions and are not routed to ridge generation");
		Fixture.SourceSelectionClass = ECarrierLabPhaseIIIE3SelectionClass::UnresolvedMixedMaterialMultiHit;
		Fixture.bExpectGenerated = false;
		Fixture.bExpectRejectedNonDivergentRoute = true;
		Fixture.bExpectRejectedUnresolvedMultiHit = true;
		Fixture.bExpectBoundaryPairFound = false;
		SetEmptyBoundaryExpectations(Fixture);
		Fixture.ExpectedRecordHash = TEXT("e38a01c58860d3c6");
		return Fixture;
	}

	FFixtureSpec MakeNoBoundaryPairFixture()
	{
		FFixtureSpec Fixture = MakeNoHitGenerationFixture();
		Fixture.Name = TEXT("no two-plate boundary pair anomaly");
		Fixture.Purpose = TEXT("a divergent route without two different boundary plates is reported and does not synthesize ownership");
		Fixture.Edges.Reset();
		AddPointEdge(Fixture, 0, -20.0, 0.0, -4.0);
		Fixture.bExpectGenerated = false;
		Fixture.bExpectBoundaryPairFound = false;
		SetEmptyBoundaryExpectations(Fixture);
		Fixture.ExpectedRecordHash = TEXT("7b703a36dee02260");
		return Fixture;
	}

	FFixtureSpec MakeAsymmetricElevationFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("asymmetric q1/q2 elevation interpolation");
		Fixture.Purpose = TEXT("fixture-owned constants catch swapped q1/q2 distance weighting and zBar passthrough");
		Fixture.Sample = UnitFromLonLat(0.0, 5.0);
		Fixture.SourceSelectionClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
		AddPointEdge(Fixture, 0, -30.0, 0.0, -3.5);
		AddPointEdge(Fixture, 1, 15.0, 0.0, -1.5);
		Fixture.ExpectedQ1PlateId = 1;
		Fixture.ExpectedQ2PlateId = 0;
		Fixture.ExpectedAssignedPlateId = 1;
		Fixture.ExpectedQ1 = FVector3d(0.96592582628906831, 0.25881904510252074, 0.0);
		Fixture.ExpectedQ2 = FVector3d(0.86602540378443871, -0.49999999999999994, 0.0);
		Fixture.ExpectedQGamma = FVector3d(0.99144486137381038, -0.13052619222005157, 0.0);
		Fixture.ExpectedQ1Elevation = -1.5;
		Fixture.ExpectedQ2Elevation = -3.5;
		Fixture.ExpectedQ1DistanceKm = 1756.1264009914842;
		Fixture.ExpectedQ2DistanceKm = 3377.6022198712876;
		Fixture.ExpectedRidgeDistanceKm = 1001.4149587611595;
		Fixture.ExpectedNearestBoundaryDistanceKm = 1756.1264009914842;
		Fixture.ExpectedAlpha = 0.36315500952304419;
		Fixture.ExpectedZBar = -2.1841524087793913;
		Fixture.ExpectedZGammaProfileDistanceKm = 1001.4149587611595;
		Fixture.ExpectedZGammaProfileReferenceDistanceKm = ZGammaGeophysicsReferenceDistanceKm;
		Fixture.ExpectedZGammaProfileT = ZGammaProfileTForDistanceKm(Fixture.ExpectedZGammaProfileDistanceKm);
		Fixture.ExpectedZGamma = ZGammaForDistanceKm(Fixture.ExpectedZGammaProfileDistanceKm);
		Fixture.ExpectedElevation = BlendElevation(Fixture.ExpectedAlpha, Fixture.ExpectedZBar, Fixture.ExpectedZGamma);
		Fixture.ExpectedOceanicAge = 0.0;
		Fixture.ExpectedRidgeDirection = FVector3d(0.072673655516996297, 0.5520112177799954, -0.83066368359212817);
		Fixture.ExpectedRecordHash = TEXT("ce371b6a0b248d14");
		return Fixture;
	}

	FFixtureSpec MakeOffAxisSampleFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("off-axis sample ridge direction");
		Fixture.Purpose = TEXT("fixture-owned vector constants catch equatorial-only ridge-direction or radial leakage mistakes");
		Fixture.Sample = UnitFromLonLat(5.0, 8.0);
		Fixture.SourceSelectionClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
		AddPointEdge(Fixture, 0, -20.0, 0.0, -4.0);
		AddPointEdge(Fixture, 1, 20.0, 0.0, -2.0);
		Fixture.ExpectedQ1PlateId = 1;
		Fixture.ExpectedQ2PlateId = 0;
		Fixture.ExpectedAssignedPlateId = 1;
		Fixture.ExpectedQ1 = FVector3d(0.93969262078590843, 0.34202014332566871, 0.0);
		Fixture.ExpectedQ2 = FVector3d(0.93969262078590843, -0.34202014332566871, 0.0);
		Fixture.ExpectedQGamma = FVector3d(1.0, 0.0, 0.0);
		Fixture.ExpectedQ1Elevation = -2.0;
		Fixture.ExpectedQ2Elevation = -4.0;
		Fixture.ExpectedQ1DistanceKm = 1885.4975636174381;
		Fixture.ExpectedQ2DistanceKm = 2909.9966057756706;
		Fixture.ExpectedRidgeDistanceKm = 1048.0512255750384;
		Fixture.ExpectedNearestBoundaryDistanceKm = 1885.4975636174381;
		Fixture.ExpectedAlpha = 0.35726394919224691;
		Fixture.ExpectedZBar = -2.7863621545622923;
		Fixture.ExpectedZGammaProfileDistanceKm = 1048.0512255750384;
		Fixture.ExpectedZGammaProfileReferenceDistanceKm = ZGammaGeophysicsReferenceDistanceKm;
		Fixture.ExpectedZGammaProfileT = ZGammaProfileTForDistanceKm(Fixture.ExpectedZGammaProfileDistanceKm);
		Fixture.ExpectedZGamma = ZGammaForDistanceKm(Fixture.ExpectedZGammaProfileDistanceKm);
		Fixture.ExpectedElevation = BlendElevation(Fixture.ExpectedAlpha, Fixture.ExpectedZBar, Fixture.ExpectedZGamma);
		Fixture.ExpectedOceanicAge = 0.0;
		Fixture.ExpectedRidgeDirection = FVector3d(0.0, 0.84984737297332824, -0.52702888217851274);
		Fixture.ExpectedRecordHash = TEXT("10c3eb89d6f4b460");
		return Fixture;
	}

	FFixtureSpec MakeAntipodalBoundaryFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("antipodal qGamma degenerate ridge direction");
		Fixture.Purpose = TEXT("qGamma fallback to sample direction is explicit and does not invent a ridge-direction vector");
		Fixture.Sample = UnitFromLonLat(0.0, 90.0);
		Fixture.SourceSelectionClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
		AddPointEdge(Fixture, 0, 0.0, 0.0, -4.0);
		AddPointEdge(Fixture, 1, 180.0, 0.0, -2.0);
		Fixture.ExpectedQ1PlateId = 0;
		Fixture.ExpectedQ2PlateId = 1;
		Fixture.ExpectedAssignedPlateId = 0;
		Fixture.ExpectedQ1 = FVector3d(1.0, 0.0, 0.0);
		Fixture.ExpectedQ2 = FVector3d(-1.0, 1.2246467991473532e-16, 0.0);
		Fixture.ExpectedQGamma = FVector3d(6.123233995736766e-17, 0.0, 1.0);
		Fixture.ExpectedQ1Elevation = -4.0;
		Fixture.ExpectedQ2Elevation = -2.0;
		Fixture.ExpectedQ1DistanceKm = 10007.543398010288;
		Fixture.ExpectedQ2DistanceKm = 10007.543398010288;
		Fixture.ExpectedRidgeDistanceKm = 0.0;
		Fixture.ExpectedNearestBoundaryDistanceKm = 10007.543398010288;
		Fixture.ExpectedAlpha = 0.0;
		Fixture.ExpectedZBar = -3.0;
		Fixture.ExpectedZGammaProfileDistanceKm = 0.0;
		Fixture.ExpectedZGammaProfileReferenceDistanceKm = ZGammaGeophysicsReferenceDistanceKm;
		Fixture.ExpectedZGammaProfileT = ZGammaProfileTForDistanceKm(Fixture.ExpectedZGammaProfileDistanceKm);
		Fixture.ExpectedZGamma = ZGammaForDistanceKm(Fixture.ExpectedZGammaProfileDistanceKm);
		Fixture.ExpectedElevation = BlendElevation(Fixture.ExpectedAlpha, Fixture.ExpectedZBar, Fixture.ExpectedZGamma);
		Fixture.ExpectedOceanicAge = 0.0;
		Fixture.ExpectedRidgeDirection = FVector3d::ZeroVector;
		Fixture.bExpectDegenerateRidgeDirection = true;
		Fixture.ExpectedRecordHash = TEXT("3c1905f538f28d30");
		return Fixture;
	}

	FFixtureSpec MakeSameRidgeDifferentGapWidthFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("same-ridge different-gap-width zGamma");
		Fixture.Purpose = TEXT("zGamma uses ridge distance rather than alpha or nearest-boundary gap width");
		Fixture.Sample = UnitFromLonLat(0.0, 10.0);
		Fixture.SourceSelectionClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
		AddPointEdge(Fixture, 0, -40.0, 0.0, -4.0);
		AddPointEdge(Fixture, 1, 40.0, 0.0, -2.0);
		Fixture.ExpectedQ1PlateId = 0;
		Fixture.ExpectedQ2PlateId = 1;
		Fixture.ExpectedAssignedPlateId = 0;
		Fixture.ExpectedQ1 = FVector3d(0.76604444311897801, -0.64278760968653925, 0.0);
		Fixture.ExpectedQ2 = FVector3d(0.76604444311897801, 0.64278760968653925, 0.0);
		Fixture.ExpectedQGamma = FVector3d(1.0, 0.0, 0.0);
		Fixture.ExpectedQ1Elevation = -4.0;
		Fixture.ExpectedQ2Elevation = -2.0;
		Fixture.ExpectedQ1DistanceKm = 4561.9343625246975;
		Fixture.ExpectedQ2DistanceKm = 4561.9343625246975;
		Fixture.ExpectedRidgeDistanceKm = 1111.9492664455888;
		Fixture.ExpectedNearestBoundaryDistanceKm = 4561.9343625246975;
		Fixture.ExpectedAlpha = 0.1959767487595421;
		Fixture.ExpectedZBar = -3.0;
		Fixture.ExpectedZGammaProfileDistanceKm = 1111.9492664455888;
		Fixture.ExpectedZGammaProfileReferenceDistanceKm = ZGammaGeophysicsReferenceDistanceKm;
		Fixture.ExpectedZGammaProfileT = ZGammaProfileTForDistanceKm(Fixture.ExpectedZGammaProfileDistanceKm);
		Fixture.ExpectedZGamma = ZGammaForDistanceKm(Fixture.ExpectedZGammaProfileDistanceKm);
		Fixture.ExpectedElevation = BlendElevation(Fixture.ExpectedAlpha, Fixture.ExpectedZBar, Fixture.ExpectedZGamma);
		Fixture.ExpectedOceanicAge = 0.0;
		Fixture.ExpectedRidgeDirection = FVector3d(0.0, 1.0, 0.0);
		Fixture.ExpectedRecordHash = TEXT("6aca0669c499a5b8");
		return Fixture;
	}

	const FFixtureResult* FindResultByName(const TArray<FFixtureResult>& Results, const TCHAR* Name)
	{
		for (const FFixtureResult& Result : Results)
		{
			if (Result.Name == Name)
			{
				return &Result;
			}
		}
		return nullptr;
	}

	FWidthInvariantResult EvaluateSameRidgeWidthInvariant(const TArray<FFixtureResult>& Results)
	{
		FWidthInvariantResult OutResult;
		const FFixtureResult* Default = FindResultByName(Results, TEXT("no-hit divergent oceanic generation"));
		const FFixtureResult* Wide = FindResultByName(Results, TEXT("same-ridge different-gap-width zGamma"));
		if (Default == nullptr || Wide == nullptr)
		{
			return OutResult;
		}

		OutResult.RidgeDistanceDeltaKm = FMath::Abs(Default->Record.RidgeDistanceKm - Wide->Record.RidgeDistanceKm);
		OutResult.NearestBoundaryDistanceDeltaKm = FMath::Abs(Default->Record.NearestBoundaryDistanceKm - Wide->Record.NearestBoundaryDistanceKm);
		OutResult.AlphaDelta = FMath::Abs(Default->Record.Alpha - Wide->Record.Alpha);
		OutResult.ZGammaDelta = FMath::Abs(Default->Record.ZGammaElevation - Wide->Record.ZGammaElevation);
		OutResult.ZGammaProfileTDelta = FMath::Abs(Default->Record.ZGammaProfileT - Wide->Record.ZGammaProfileT);
		OutResult.bPass =
			Default->bPass &&
			Wide->bPass &&
			OutResult.RidgeDistanceDeltaKm <= DistanceToleranceKm &&
			OutResult.NearestBoundaryDistanceDeltaKm > 1.0 &&
			OutResult.AlphaDelta > ScalarTolerance &&
			OutResult.ZGammaDelta <= ScalarTolerance &&
			OutResult.ZGammaProfileTDelta <= ScalarTolerance;
		return OutResult;
	}

	FString BuildJsonLine(const FFixtureResult& Result)
	{
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"purpose\":%s,\"pass\":%s,\"query_returned\":%s,\"source_class\":%s,\"generated\":%s,\"boundary_pair_found\":%s,\"nonseparating_anomaly\":%s,\"generated_nonpositive_separation\":%s,\"rejected_nondivergent_route\":%s,\"rejected_unresolved_multi_hit\":%s,\"zgamma_distance_profile_placeholder\":%s,\"zgamma_geophysics_derived_profile\":%s,\"paper_faithful_zgamma_profile\":%s,\"q1_plate\":%d,\"q2_plate\":%d,\"assigned_plate\":%d,\"signed_separation_velocity\":%.12g,\"q1_distance_km\":%.12g,\"q2_distance_km\":%.12g,\"ridge_distance_km\":%.12g,\"nearest_boundary_distance_km\":%.12g,\"alpha\":%.12g,\"zbar\":%.12g,\"zgamma\":%.12g,\"zgamma_profile_distance_km\":%.12g,\"zgamma_profile_reference_distance_km\":%.12g,\"zgamma_profile_t\":%.12g,\"elevation\":%.12g,\"oceanic_age\":%.12g,\"ridge_direction_magnitude\":%.12g,\"ridge_radial_dot\":%.12g,\"q1_distance_residual_km\":%.12g,\"q2_distance_residual_km\":%.12g,\"ridge_distance_residual_km\":%.12g,\"nearest_boundary_distance_residual_km\":%.12g,\"qgamma_residual\":%.12g,\"alpha_residual\":%.12g,\"zbar_residual\":%.12g,\"zgamma_residual\":%.12g,\"zgamma_profile_distance_residual_km\":%.12g,\"zgamma_profile_reference_residual_km\":%.12g,\"zgamma_profile_t_residual\":%.12g,\"elevation_residual\":%.12g,\"oceanic_age_residual\":%.12g,\"ridge_direction_residual\":%.12g,\"record_hash\":%s,\"expected_record_hash\":%s,\"record_hash_match\":%s}"),
			*JsonString(Result.Name),
			*JsonString(Result.Purpose),
			Result.bPass ? TEXT("true") : TEXT("false"),
			Result.bQueryReturned ? TEXT("true") : TEXT("false"),
			*JsonString(SelectionClassName(Result.Record.SourceSelectionClass)),
			Result.Record.bGeneratedOceanicCrust ? TEXT("true") : TEXT("false"),
			Result.Record.bBoundaryPairFound ? TEXT("true") : TEXT("false"),
			Result.Record.bNonSeparatingAnomaly ? TEXT("true") : TEXT("false"),
			Result.Record.bGeneratedWithNonPositiveSeparation ? TEXT("true") : TEXT("false"),
			Result.Record.bRejectedNonDivergentRoute ? TEXT("true") : TEXT("false"),
			Result.Record.bRejectedUnresolvedMultiHit ? TEXT("true") : TEXT("false"),
			Result.Record.bUsedZGammaDistanceProfilePlaceholder ? TEXT("true") : TEXT("false"),
			Result.Record.bUsedZGammaGeophysicsDerivedProfile ? TEXT("true") : TEXT("false"),
			Result.Record.bPaperFaithfulZGammaProfile ? TEXT("true") : TEXT("false"),
			Result.Record.Q1PlateId,
			Result.Record.Q2PlateId,
			Result.Record.AssignedPlateId,
			Result.Record.SignedSeparationVelocity,
			Result.Record.Q1DistanceKm,
			Result.Record.Q2DistanceKm,
			Result.Record.RidgeDistanceKm,
			Result.Record.NearestBoundaryDistanceKm,
			Result.Record.Alpha,
			Result.Record.ZBarElevation,
			Result.Record.ZGammaElevation,
			Result.Record.ZGammaProfileDistanceKm,
			Result.Record.ZGammaProfileReferenceDistanceKm,
			Result.Record.ZGammaProfileT,
			Result.Record.Elevation,
			Result.Record.OceanicAge,
			Result.Record.RidgeDirectionMagnitude,
			Result.RidgeRadialDot,
			Result.Q1DistanceResidual,
			Result.Q2DistanceResidual,
			Result.RidgeDistanceResidual,
			Result.NearestBoundaryDistanceResidual,
			Result.QGammaResidual,
			Result.AlphaResidual,
			Result.ZBarResidual,
			Result.ZGammaResidual,
			Result.ZGammaProfileDistanceResidual,
			Result.ZGammaProfileReferenceResidual,
			Result.ZGammaProfileTResidual,
			Result.ElevationResidual,
			Result.OceanicAgeResidual,
			Result.RidgeDirectionResidual,
			*JsonString(Result.RecordHash),
			*JsonString(Result.ExpectedRecordHash),
			Result.bRecordHashMatch ? TEXT("true") : TEXT("false"));
	}

	FString BuildReport(
		const TArray<FFixtureResult>& Results,
		const FWidthInvariantResult& WidthInvariant,
		const bool bReplayPass,
		const FString& ReplayHashA,
		const FString& ReplayHashB,
		const FString& MetricsPath)
	{
		bool bFixturesPass = true;
		bool bRecordHashesPass = true;
		int32 RecordHashMatchCount = 0;
		for (const FFixtureResult& Result : Results)
		{
			bFixturesPass = bFixturesPass && Result.bPass;
			bRecordHashesPass = bRecordHashesPass && Result.bRecordHashMatch;
			RecordHashMatchCount += Result.bRecordHashMatch ? 1 : 0;
		}
		const bool bAllPass = bFixturesPass && bRecordHashesPass && WidthInvariant.bPass && bReplayPass;

		FString Report;
		Report += TEXT("# Phase IIIE.4 Divergent Oceanic Field Generation\n\n");
		Report += TEXT("Verdict: ");
		Report += bAllPass ? TEXT("PASS / IIIE.5 TOPOLOGY UNBLOCKED; ZGAMMA PAPER-FIDELITY HOLD") : TEXT("FAIL / HOLD IIIE.5");
		Report += TEXT(". This slice generates audit-only oceanic fields for IIIE.3 divergent gap routes. It does not rebuild topology, mutate the global remesh TDS, reset process state, optimize replay, or resolve multi-hit samples.\n\n");

		Report += TEXT("## Scope\n\n");
		Report += TEXT("- IIIE.4 consumes only `NoHitDivergentGap` and `DivergentGapAfterFiltering` records from IIIE.3.\n");
		Report += TEXT("- It obtains q1/q2/qGamma from the IIIE.2 continuous boundary query and records q1/q2 plate ids, edge ids, qGamma, signed separating velocity, generated elevation, age, and ridge direction.\n");
		Report += TEXT("- Generated divergent crust has `OceanicAge = 0` and `RidgeDirection = retangent(normalize((p - qGamma) x p))`.\n");
		Report += TEXT("- Elevation follows the local extraction contract `z = alpha * zBar(p) + (1 - alpha) * zGamma(p)`, with `alpha = dGamma / (dGamma + dPlate)`.\n");
		Report += TEXT("- `zBar(p)` is compared against fixture-owned expected constants for distance interpolation between q1/q2 boundary elevations.\n");
		Report += TEXT("- Pre-IIIE.6 replaces the Earth-radius linear placeholder with the accepted geophysics-derived extension `zGamma(dGamma) = z_e + (z_o - z_e) * sqrt(clamp(dGamma / 3000 km, 0, 1))`, anchored to thesis Table 3.2 constants: ridge peak `-1 km`, abyssal plain `-6 km`.\n");
		Report += TEXT("- `zGamma` remains a paper-fidelity hold. Thesis section 3.3.2.1 names a generic ridge profile and section 3.3.2.3 defines qGamma, but the local extraction has no closed-form zGamma curve. Generated records therefore report `bUsedZGammaGeophysicsDerivedProfile = true` and `bPaperFaithfulZGammaProfile = false`.\n");
		Report += TEXT("- Resolved single-hit and unresolved multi-hit classes are rejected before field generation. Non-positive signed separating velocity is diagnostic only by default; the explicit `bRestoreNonSeparatingAnomalyVeto` opt-out reproduces the old CarrierLab-invented anomaly hold for historical baseline audits.\n\n");

		Report += TEXT("## Gates\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---:|---|\n");
		for (const FFixtureResult& Result : Results)
		{
			Report += FString::Printf(
				TEXT("| %s | %s | class `%s`, generated `%d`, boundary `%d`, anomaly `%d`, nonpos-generated `%d`, rejected `%d/%d`, q1/q2 `%d/%d`, assigned `%d`, signed velocity `%.6g`, q dist residuals `%.3g/%.3g km`, ridge/nearest residuals `%.3g/%.3g km`, qGamma residual `%.3g`, alpha/elev residuals `%.3g/%.3g`, zGamma profile residuals `%.3g km/%.3g km/%.3g`, placeholder/geophysics/paper `%d/%d/%d`, ridge residual `%.3g`, radial dot `%.3g`, hash `%s`, expected `%s`, match `%d`. |\n"),
				*Result.Name,
				*PassFail(Result.bPass),
				*SelectionClassName(Result.Record.SourceSelectionClass),
				Result.Record.bGeneratedOceanicCrust ? 1 : 0,
				Result.Record.bBoundaryPairFound ? 1 : 0,
				Result.Record.bNonSeparatingAnomaly ? 1 : 0,
				Result.Record.bGeneratedWithNonPositiveSeparation ? 1 : 0,
				Result.Record.bRejectedNonDivergentRoute ? 1 : 0,
				Result.Record.bRejectedUnresolvedMultiHit ? 1 : 0,
				Result.Record.Q1PlateId,
				Result.Record.Q2PlateId,
				Result.Record.AssignedPlateId,
				Result.Record.SignedSeparationVelocity,
				Result.Q1DistanceResidual,
				Result.Q2DistanceResidual,
				Result.RidgeDistanceResidual,
				Result.NearestBoundaryDistanceResidual,
				Result.QGammaResidual,
				Result.AlphaResidual,
				Result.ElevationResidual,
				Result.ZGammaProfileDistanceResidual,
				Result.ZGammaProfileReferenceResidual,
				Result.ZGammaProfileTResidual,
				Result.Record.bUsedZGammaDistanceProfilePlaceholder ? 1 : 0,
				Result.Record.bUsedZGammaGeophysicsDerivedProfile ? 1 : 0,
				Result.Record.bPaperFaithfulZGammaProfile ? 1 : 0,
				Result.RidgeDirectionResidual,
				Result.RidgeRadialDot,
				*Result.RecordHash,
				*Result.ExpectedRecordHash,
				Result.bRecordHashMatch ? 1 : 0);
		}
		Report += FString::Printf(
			TEXT("| Same-ridge zGamma width-invariance | %s | ridge delta `%.3g km`, nearest-boundary delta `%.3g km`, alpha delta `%.3g`, zGamma delta `%.3g`, profile-t delta `%.3g`. Same dGamma with different gap width must keep zGamma fixed while alpha changes. |\n"),
			*PassFail(WidthInvariant.bPass),
			WidthInvariant.RidgeDistanceDeltaKm,
			WidthInvariant.NearestBoundaryDistanceDeltaKm,
			WidthInvariant.AlphaDelta,
			WidthInvariant.ZGammaDelta,
			WidthInvariant.ZGammaProfileTDelta);
		Report += FString::Printf(
			TEXT("| Fixture record-hash regression | %s | `%d/%d` fixture records matched their expected hashes. |\n"),
			*PassFail(bRecordHashesPass),
			RecordHashMatchCount,
			Results.Num());
		Report += FString::Printf(
			TEXT("| Same-seed oceanic-generation replay | %s | Replay hashes `%s` and `%s`. |\n"),
			*PassFail(bReplayPass),
			*ReplayHashA,
			*ReplayHashB);
		Report += TEXT("| zGamma paper-fidelity hold | hold | Generated records intentionally report `bUsedZGammaDistanceProfilePlaceholder=0`, `bUsedZGammaGeophysicsDerivedProfile=1`, and `bPaperFaithfulZGammaProfile=0`; this is an accepted lab extension, not a paper-faithful ridge-profile claim. |\n");

		Report += TEXT("\n## Contract Table\n\n");
		Report += TEXT("| Paper / IIIE.1 requirement | CarrierLab support now | IIIE obligation still ahead | Gate needed |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += TEXT("| Zero valid hits become divergent gap fill | IIIE.4 accepts only no-hit and filter-exhausted route classes | Wire these records into the actual remesh event in IIIE.5 | End-to-end remesh event fixture with zero-hit route |\n");
		Report += TEXT("| q1/q2 are continuous nearest boundary points on different plates | IIIE.4 calls the IIIE.2 query and records q ids, edges, qGamma, distances, and elevations | Preserve this provenance when topology rebuild duplicates/re-indexes samples | Event-log row carries q1/q2/qGamma through rebuild |\n");
		Report += TEXT("| New oceanic crust age is zero | Generation records `OceanicAge = 0` and gates residual against fixture-owned expected constants | Mutate global samples only inside the remesh event | Generated sample field residuals and record-hash regression |\n");
		Report += TEXT("| Ridge direction is `(p - qGamma) x p` retangented/normalized | Generated records have non-zero tangent ridge vectors with near-zero radial dot | Preserve vector fields through duplicate/re-index/re-compact | Vector magnitude and radial-dot oracle |\n");
		Report += TEXT("| zGamma is a ridge profile, not a second use of alpha | Pre-IIIE.6 records separate `ZGammaProfileT = sqrt(dGamma / 3000 km)` and gates same-ridge/different-gap invariance | Keep the geophysics-derived profile labeled as lab policy unless a paper-cited law is recovered | Geophysics-profile flag and paper-fidelity hold gate |\n");
		Report += TEXT("| Gap fill requires separating q1/q2 kinematics | Non-positive signed velocity reports an anomaly and does not generate | Use production plate motions at remesh cadence | Positive/negative velocity fixtures in remesh event |\n");
		Report += TEXT("| Multiple valid hits are stop conditions | Resolved and unresolved non-gap classes are rejected before generation | Keep unresolved counts blocking primary remesh | Multi-hit rejection gate remains required |\n\n");

		Report += TEXT("## Forbidden Policy Checks\n\n");
		Report += TEXT("| Policy | IIIE.4 status |\n");
		Report += TEXT("|---|---|\n");
		Report += TEXT("| Prior global sample owner/fraction fallback | Not used; every generation record keeps `bUsedPriorOwnerFallback = false`. |\n");
		Report += TEXT("| Centroid/random/synthetic winner policy | Not used; missing boundary pairs stay anomalies. |\n");
		Report += TEXT("| Stage 1.5 endpoint/midpoint q1/q2 authority | Not used; q1/q2 come from IIIE.2 continuous boundary provenance. |\n");
		Report += TEXT("| Recovery/repair/backfill/retention/hysteresis/anchoring | Not used; rejected and anomalous routes remain non-generated. |\n");
		Report += TEXT("| Silent unresolved multi-hit resolution | Forbidden; unresolved routes are rejected before oceanic generation. |\n\n");

		Report += TEXT("## Stop Conditions For IIIE.5+\n\n");
		Report += TEXT("- Stop if topology rebuild routes resolved single-hit or unresolved multi-hit samples into divergent oceanic generation.\n");
		Report += TEXT("- Stop if topology rebuild claims `zGamma` or generated elevation is fully paper-faithful while generated records still report `bUsedZGammaGeophysicsDerivedProfile = true` and `bPaperFaithfulZGammaProfile = false`.\n");
		Report += TEXT("- Stop if `zGamma` changes when `dGamma` is unchanged but gap width / nearest-boundary distance changes.\n");
		Report += TEXT("- Stop if a non-positive q1/q2 separating velocity generates oceanic fields.\n");
		Report += TEXT("- Stop if a missing two-plate boundary pair fabricates q1/q2, plate ownership, or elevation.\n");
		Report += TEXT("- Stop if Stage 1.5 prior-owner, endpoint/midpoint, recovery, or anchoring policy becomes authority in the primary IIIE remesh path.\n");
		Report += TEXT("- Stop if IIIE.5 topology rebuild drops q1/q2/qGamma, signed velocity, age, elevation, or ridge-direction evidence from the event log.\n\n");

		Report += TEXT("## Next Slice Boundary\n\n");
		Report += TEXT("IIIE.5 should rebuild plate-local topology from the global TDS assignment: duplicate, re-index, and re-compact per plate while preserving motion and the IIIE.4 generated field records. It must also perform the remesh process-state reset contracted in IIIE.1: invalidate subduction marks, active convergence lists, distance-to-front records, and the subduction matrix, then demonstrate that later IIIB/IIIC steps rebuild them from geometry rather than carrying stale state. Accepted-but-unsutured collision groups still need to populate the `CollisionPending` filter reason before remesh source selection.\n\n");

		Report += FString::Printf(TEXT("Metrics: `%s`.\n"), *MetricsPath);
		return Report;
	}
}

UCarrierLabPhaseIIIE4Commandlet::UCarrierLabPhaseIIIE4Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIE4Commandlet::Main(const FString& Params)
{
	UWorld* World = GetCommandletWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPhaseIIIE4Commandlet could not find a commandlet world."));
		return 1;
	}

	ACarrierLabVisualizationActor* Actor = SpawnActor(*World);
	if (Actor == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPhaseIIIE4Commandlet could not spawn CarrierLabVisualizationActor."));
		return 1;
	}

	TArray<FFixtureSpec> Fixtures;
	Fixtures.Add(MakeNoHitGenerationFixture());
	Fixtures.Add(MakeFilterExhaustedGenerationFixture());
	Fixtures.Add(MakeNonSeparatingAnomalyFixture());
	Fixtures.Add(MakeRestoredNonSeparatingVetoFixture());
	Fixtures.Add(MakeResolvedRouteRejectedFixture());
	Fixtures.Add(MakeUnresolvedRouteRejectedFixture());
	Fixtures.Add(MakeNoBoundaryPairFixture());
	Fixtures.Add(MakeAsymmetricElevationFixture());
	Fixtures.Add(MakeOffAxisSampleFixture());
	Fixtures.Add(MakeAntipodalBoundaryFixture());
	Fixtures.Add(MakeSameRidgeDifferentGapWidthFixture());

	TArray<FFixtureResult> Results;
	Results.Reserve(Fixtures.Num());
	for (const FFixtureSpec& Fixture : Fixtures)
	{
		FFixtureResult Result;
		RunFixture(*Actor, Fixture, Result);
		Results.Add(Result);
	}
	const FWidthInvariantResult WidthInvariant = EvaluateSameRidgeWidthInvariant(Results);

	FFixtureResult ReplayA;
	FFixtureResult ReplayB;
	RunFixture(*Actor, MakeNoHitGenerationFixture(), ReplayA);
	RunFixture(*Actor, MakeNoHitGenerationFixture(), ReplayB);
	const bool bReplayPass = ReplayA.bPass && ReplayB.bPass && ReplayA.RecordHash == ReplayB.RecordHash;

	Actor->Destroy();
	CollectGarbage(RF_NoFlags);

	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("phase-iii-slice-iiie4-metrics.jsonl"));
	FString JsonLines;
	for (const FFixtureResult& Result : Results)
	{
		JsonLines += BuildJsonLine(Result);
		JsonLines += TEXT("\n");
	}
	JsonLines += FString::Printf(
		TEXT("{\"fixture\":\"same_ridge_zgamma_width_invariance\",\"pass\":%s,\"ridge_distance_delta_km\":%.12g,\"nearest_boundary_distance_delta_km\":%.12g,\"alpha_delta\":%.12g,\"zgamma_delta\":%.12g,\"zgamma_profile_t_delta\":%.12g}\n"),
		WidthInvariant.bPass ? TEXT("true") : TEXT("false"),
		WidthInvariant.RidgeDistanceDeltaKm,
		WidthInvariant.NearestBoundaryDistanceDeltaKm,
		WidthInvariant.AlphaDelta,
		WidthInvariant.ZGammaDelta,
		WidthInvariant.ZGammaProfileTDelta);
	JsonLines += FString::Printf(
		TEXT("{\"fixture\":\"same_seed_oceanic_generation_replay\",\"pass\":%s,\"hash_a\":%s,\"hash_b\":%s}\n"),
		bReplayPass ? TEXT("true") : TEXT("false"),
		*JsonString(ReplayA.RecordHash),
		*JsonString(ReplayB.RecordHash));

	const FString ReportPath = GetReportPath(Params);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	const bool bMetricsWritten = FFileHelper::SaveStringToFile(JsonLines, *MetricsPath);
	const FString Report = BuildReport(Results, WidthInvariant, bReplayPass, ReplayA.RecordHash, ReplayB.RecordHash, MetricsPath);
	const bool bReportWritten = FFileHelper::SaveStringToFile(Report, *ReportPath);

	bool bPass = WidthInvariant.bPass && bReplayPass && bMetricsWritten && bReportWritten;
	for (const FFixtureResult& Result : Results)
	{
		bPass = bPass && Result.bPass;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("CarrierLab IIIE.4 gates: %s. Report=%s Metrics=%s"),
		*PassFail(bPass),
		*ReportPath,
		*MetricsPath);
	return bPass ? 0 : 1;
}
