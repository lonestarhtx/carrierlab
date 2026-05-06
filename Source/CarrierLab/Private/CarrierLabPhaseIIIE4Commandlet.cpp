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
	constexpr double RidgePeakElevationKm = -1.0;
	constexpr double AbyssalElevationKm = -6.0;

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
		bool bExpectGenerated = true;
		bool bExpectNonSeparatingAnomaly = false;
		bool bExpectRejectedNonDivergentRoute = false;
		bool bExpectRejectedUnresolvedMultiHit = false;
		bool bExpectBoundaryPairFound = true;
		int32 ExpectedQ1PlateId = 0;
		int32 ExpectedQ2PlateId = 1;
		int32 ExpectedAssignedPlateId = 0;
		FVector3d ExpectedQ1 = FVector3d::UnitX();
		FVector3d ExpectedQ2 = FVector3d::UnitY();
		double ExpectedQ1Elevation = -4.0;
		double ExpectedQ2Elevation = -2.0;
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
		FString RecordHash;
		FCarrierLabPhaseIIIE4OceanicGenerationRecord Record;
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
		Fixture.ExpectedQ1Elevation = -4.0;
		Fixture.ExpectedQ2Elevation = -2.0;
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
		HashMixDouble(Hash, Record.Elevation);
		HashMixDouble(Hash, Record.OceanicAge);
		HashMixDouble(Hash, Record.RidgeDirection.X);
		HashMixDouble(Hash, Record.RidgeDirection.Y);
		HashMixDouble(Hash, Record.RidgeDirection.Z);
		return HashToString(Hash);
	}

	void ComputeIndependentOracle(
		const FFixtureSpec& Fixture,
		double& OutAlpha,
		double& OutZBar,
		double& OutZGamma,
		double& OutElevation,
		FVector3d& OutRidgeDirection)
	{
		const FVector3d Sample = NormalizeOrFallback(Fixture.Sample, FVector3d::UnitX());
		const FVector3d Q1 = NormalizeOrFallback(Fixture.ExpectedQ1, FVector3d::UnitX());
		const FVector3d Q2 = NormalizeOrFallback(Fixture.ExpectedQ2, FVector3d::UnitY());
		const FVector3d QGamma = NormalizeOrFallback(Q1 + Q2, Sample);
		const double Q1DistanceKm = DistanceKm(Sample, Q1);
		const double Q2DistanceKm = DistanceKm(Sample, Q2);
		const double QDistanceDenominator = Q1DistanceKm + Q2DistanceKm;
		const double QElevationT = QDistanceDenominator > UE_DOUBLE_SMALL_NUMBER ? Q1DistanceKm / QDistanceDenominator : 0.5;
		OutZBar = FMath::Lerp(Fixture.ExpectedQ1Elevation, Fixture.ExpectedQ2Elevation, QElevationT);
		const double RidgeDistanceKm = DistanceKm(Sample, QGamma);
		const double NearestBoundaryDistanceKm = FMath::Min(Q1DistanceKm, Q2DistanceKm);
		const double ElevationDenominator = RidgeDistanceKm + NearestBoundaryDistanceKm;
		OutAlpha = ElevationDenominator > UE_DOUBLE_SMALL_NUMBER
			? FMath::Clamp(RidgeDistanceKm / ElevationDenominator, 0.0, 1.0)
			: 0.0;
		OutZGamma = FMath::Lerp(RidgePeakElevationKm, AbyssalElevationKm, OutAlpha);
		OutElevation = OutAlpha * OutZBar + (1.0 - OutAlpha) * OutZGamma;
		OutRidgeDirection = RetangentAndNormalize(FVector3d::CrossProduct(Sample - QGamma, Sample), Sample);
	}

	bool RunFixture(
		ACarrierLabVisualizationActor& Actor,
		const FFixtureSpec& Fixture,
		FFixtureResult& OutResult)
	{
		OutResult = FFixtureResult();
		OutResult.Name = Fixture.Name;
		OutResult.Purpose = Fixture.Purpose;
		const double StartSeconds = FPlatformTime::Seconds();
		OutResult.bQueryReturned = Actor.QueryPhaseIIIE4DivergentOceanicFieldsForTest(
			Fixture.Sample,
			Fixture.SourceSelectionClass,
			Fixture.Edges,
			Fixture.SignedSeparationVelocity,
			OutResult.Record);
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.RecordHash = ComputeRecordHash(OutResult.Record);

		OutResult.bPass =
			OutResult.bQueryReturned &&
			OutResult.Record.bGeneratedOceanicCrust == Fixture.bExpectGenerated &&
			OutResult.Record.bNonSeparatingAnomaly == Fixture.bExpectNonSeparatingAnomaly &&
			OutResult.Record.bRejectedNonDivergentRoute == Fixture.bExpectRejectedNonDivergentRoute &&
			OutResult.Record.bRejectedUnresolvedMultiHit == Fixture.bExpectRejectedUnresolvedMultiHit &&
			OutResult.Record.bBoundaryPairFound == Fixture.bExpectBoundaryPairFound &&
			OutResult.Record.bUsedPolicyWinner == false &&
			OutResult.Record.bUsedPriorOwnerFallback == false;

		if (Fixture.bExpectGenerated)
		{
			double ExpectedAlpha = 0.0;
			double ExpectedZBar = 0.0;
			double ExpectedZGamma = 0.0;
			double ExpectedElevation = 0.0;
			FVector3d ExpectedRidgeDirection = FVector3d::ZeroVector;
			ComputeIndependentOracle(Fixture, ExpectedAlpha, ExpectedZBar, ExpectedZGamma, ExpectedElevation, ExpectedRidgeDirection);
			OutResult.AlphaResidual = FMath::Abs(OutResult.Record.Alpha - ExpectedAlpha);
			OutResult.ZBarResidual = FMath::Abs(OutResult.Record.ZBarElevation - ExpectedZBar);
			OutResult.ZGammaResidual = FMath::Abs(OutResult.Record.ZGammaElevation - ExpectedZGamma);
			OutResult.ElevationResidual = FMath::Abs(OutResult.Record.Elevation - ExpectedElevation);
			OutResult.OceanicAgeResidual = FMath::Abs(OutResult.Record.OceanicAge);
			OutResult.RidgeDirectionResidual = (OutResult.Record.RidgeDirection - ExpectedRidgeDirection).Size();
			OutResult.RidgeRadialDot = FMath::Abs(FVector3d::DotProduct(OutResult.Record.RidgeDirection, NormalizeOrFallback(Fixture.Sample, FVector3d::UnitX())));
			OutResult.bPass =
				OutResult.bPass &&
				OutResult.Record.Q1PlateId == Fixture.ExpectedQ1PlateId &&
				OutResult.Record.Q2PlateId == Fixture.ExpectedQ2PlateId &&
				OutResult.Record.AssignedPlateId == Fixture.ExpectedAssignedPlateId &&
				OutResult.Record.SignedSeparationVelocity > 0.0 &&
				OutResult.Record.RidgeDirectionMagnitude > 1.0 - VectorTolerance &&
				OutResult.RidgeRadialDot <= VectorTolerance &&
				OutResult.AlphaResidual <= ScalarTolerance &&
				OutResult.ZBarResidual <= ScalarTolerance &&
				OutResult.ZGammaResidual <= ScalarTolerance &&
				OutResult.ElevationResidual <= ScalarTolerance &&
				OutResult.OceanicAgeResidual <= ScalarTolerance &&
				OutResult.RidgeDirectionResidual <= VectorTolerance;
		}

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
		return Fixture;
	}

	FFixtureSpec MakeFilterExhaustedGenerationFixture()
	{
		FFixtureSpec Fixture = MakeNoHitGenerationFixture();
		Fixture.Name = TEXT("filter-exhausted divergent oceanic generation");
		Fixture.Purpose = TEXT("a zero-valid-hit-after-filtering route creates the same paper fields without prior-owner fallback");
		Fixture.SourceSelectionClass = ECarrierLabPhaseIIIE3SelectionClass::DivergentGapAfterFiltering;
		return Fixture;
	}

	FFixtureSpec MakeNonSeparatingAnomalyFixture()
	{
		FFixtureSpec Fixture = MakeNoHitGenerationFixture();
		Fixture.Name = TEXT("non-separating q1/q2 anomaly");
		Fixture.Purpose = TEXT("gap fill does not fire when the q1/q2 signed separating velocity is non-positive");
		Fixture.SignedSeparationVelocity = -0.002;
		Fixture.bExpectGenerated = false;
		Fixture.bExpectNonSeparatingAnomaly = true;
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
		Fixture.ExpectedQ1PlateId = INDEX_NONE;
		Fixture.ExpectedQ2PlateId = INDEX_NONE;
		Fixture.ExpectedAssignedPlateId = INDEX_NONE;
		return Fixture;
	}

	FString BuildJsonLine(const FFixtureResult& Result)
	{
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"purpose\":%s,\"pass\":%s,\"query_returned\":%s,\"source_class\":%s,\"generated\":%s,\"boundary_pair_found\":%s,\"nonseparating_anomaly\":%s,\"rejected_nondivergent_route\":%s,\"rejected_unresolved_multi_hit\":%s,\"q1_plate\":%d,\"q2_plate\":%d,\"assigned_plate\":%d,\"signed_separation_velocity\":%.12g,\"alpha\":%.12g,\"zbar\":%.12g,\"zgamma\":%.12g,\"elevation\":%.12g,\"oceanic_age\":%.12g,\"ridge_direction_magnitude\":%.12g,\"ridge_radial_dot\":%.12g,\"elevation_residual\":%.12g,\"record_hash\":%s}"),
			*JsonString(Result.Name),
			*JsonString(Result.Purpose),
			Result.bPass ? TEXT("true") : TEXT("false"),
			Result.bQueryReturned ? TEXT("true") : TEXT("false"),
			*JsonString(SelectionClassName(Result.Record.SourceSelectionClass)),
			Result.Record.bGeneratedOceanicCrust ? TEXT("true") : TEXT("false"),
			Result.Record.bBoundaryPairFound ? TEXT("true") : TEXT("false"),
			Result.Record.bNonSeparatingAnomaly ? TEXT("true") : TEXT("false"),
			Result.Record.bRejectedNonDivergentRoute ? TEXT("true") : TEXT("false"),
			Result.Record.bRejectedUnresolvedMultiHit ? TEXT("true") : TEXT("false"),
			Result.Record.Q1PlateId,
			Result.Record.Q2PlateId,
			Result.Record.AssignedPlateId,
			Result.Record.SignedSeparationVelocity,
			Result.Record.Alpha,
			Result.Record.ZBarElevation,
			Result.Record.ZGammaElevation,
			Result.Record.Elevation,
			Result.Record.OceanicAge,
			Result.Record.RidgeDirectionMagnitude,
			Result.RidgeRadialDot,
			Result.ElevationResidual,
			*JsonString(Result.RecordHash));
	}

	FString BuildReport(
		const TArray<FFixtureResult>& Results,
		const bool bReplayPass,
		const FString& ReplayHashA,
		const FString& ReplayHashB,
		const FString& MetricsPath)
	{
		bool bFixturesPass = true;
		for (const FFixtureResult& Result : Results)
		{
			bFixturesPass = bFixturesPass && Result.bPass;
		}
		const bool bAllPass = bFixturesPass && bReplayPass;

		FString Report;
		Report += TEXT("# Phase IIIE.4 Divergent Oceanic Field Generation\n\n");
		Report += TEXT("Verdict: ");
		Report += bAllPass ? TEXT("PASS / IIIE.5 UNBLOCKED") : TEXT("FAIL / HOLD IIIE.5");
		Report += TEXT(". This slice generates audit-only oceanic fields for IIIE.3 divergent gap routes. It does not rebuild topology, mutate the global remesh TDS, reset process state, optimize replay, or resolve multi-hit samples.\n\n");

		Report += TEXT("## Scope\n\n");
		Report += TEXT("- IIIE.4 consumes only `NoHitDivergentGap` and `DivergentGapAfterFiltering` records from IIIE.3.\n");
		Report += TEXT("- It obtains q1/q2/qGamma from the IIIE.2 continuous boundary query and records q1/q2 plate ids, edge ids, qGamma, signed separating velocity, generated elevation, age, and ridge direction.\n");
		Report += TEXT("- Generated divergent crust has `OceanicAge = 0` and `RidgeDirection = retangent(normalize((p - qGamma) x p))`.\n");
		Report += TEXT("- Elevation follows the local extraction contract `z = alpha * zBar(p) + (1 - alpha) * zGamma(p)`, with `alpha = dGamma / (dGamma + dPlate)`.\n");
		Report += TEXT("- `zBar(p)` is independently distance-interpolated between the q1/q2 boundary elevations. `zGamma(p)` uses the named IIIE.4 ridge-profile convention anchored to thesis Table 3.2 constants: ridge peak `-1 km`, abyssal plain `-6 km`, linearly parameterized by the same ridge-to-boundary alpha until a more detailed profile curve is extracted.\n");
		Report += TEXT("- Resolved single-hit and unresolved multi-hit classes are rejected before field generation. Non-positive signed separating velocity is an anomaly, not a fallback.\n\n");

		Report += TEXT("## Gates\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---:|---|\n");
		for (const FFixtureResult& Result : Results)
		{
			Report += FString::Printf(
				TEXT("| %s | %s | class `%s`, generated `%d`, boundary `%d`, anomaly `%d`, rejected `%d/%d`, q1/q2 `%d/%d`, assigned `%d`, signed velocity `%.6g`, age `%.3g`, elevation residual `%.3g`, ridge residual `%.3g`, radial dot `%.3g`, hash `%s`. |\n"),
				*Result.Name,
				*PassFail(Result.bPass),
				*SelectionClassName(Result.Record.SourceSelectionClass),
				Result.Record.bGeneratedOceanicCrust ? 1 : 0,
				Result.Record.bBoundaryPairFound ? 1 : 0,
				Result.Record.bNonSeparatingAnomaly ? 1 : 0,
				Result.Record.bRejectedNonDivergentRoute ? 1 : 0,
				Result.Record.bRejectedUnresolvedMultiHit ? 1 : 0,
				Result.Record.Q1PlateId,
				Result.Record.Q2PlateId,
				Result.Record.AssignedPlateId,
				Result.Record.SignedSeparationVelocity,
				Result.Record.OceanicAge,
				Result.ElevationResidual,
				Result.RidgeDirectionResidual,
				Result.RidgeRadialDot,
				*Result.RecordHash);
		}
		Report += FString::Printf(
			TEXT("| Same-seed oceanic-generation replay | %s | Replay hashes `%s` and `%s`. |\n"),
			*PassFail(bReplayPass),
			*ReplayHashA,
			*ReplayHashB);

		Report += TEXT("\n## Contract Table\n\n");
		Report += TEXT("| Paper / IIIE.1 requirement | CarrierLab support now | IIIE obligation still ahead | Gate needed |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += TEXT("| Zero valid hits become divergent gap fill | IIIE.4 accepts only no-hit and filter-exhausted route classes | Wire these records into the actual remesh event in IIIE.5 | End-to-end remesh event fixture with zero-hit route |\n");
		Report += TEXT("| q1/q2 are continuous nearest boundary points on different plates | IIIE.4 calls the IIIE.2 query and records q ids, edges, qGamma, distances, and elevations | Preserve this provenance when topology rebuild duplicates/re-indexes samples | Event-log row carries q1/q2/qGamma through rebuild |\n");
		Report += TEXT("| New oceanic crust age is zero | Generation records `OceanicAge = 0` and gates residual against an independent oracle | Mutate global samples only inside the remesh event | Generated sample field residuals |\n");
		Report += TEXT("| Ridge direction is `(p - qGamma) x p` retangented/normalized | Generated records have non-zero tangent ridge vectors with near-zero radial dot | Preserve vector fields through duplicate/re-index/re-compact | Vector magnitude and radial-dot oracle |\n");
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

		Report += TEXT("## Stop Conditions For IIIE.5\n\n");
		Report += TEXT("- Stop if topology rebuild routes resolved single-hit or unresolved multi-hit samples into divergent oceanic generation.\n");
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
	Fixtures.Add(MakeResolvedRouteRejectedFixture());
	Fixtures.Add(MakeUnresolvedRouteRejectedFixture());
	Fixtures.Add(MakeNoBoundaryPairFixture());

	TArray<FFixtureResult> Results;
	Results.Reserve(Fixtures.Num());
	for (const FFixtureSpec& Fixture : Fixtures)
	{
		FFixtureResult Result;
		RunFixture(*Actor, Fixture, Result);
		Results.Add(Result);
	}

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
		TEXT("{\"fixture\":\"same_seed_oceanic_generation_replay\",\"pass\":%s,\"hash_a\":%s,\"hash_b\":%s}\n"),
		bReplayPass ? TEXT("true") : TEXT("false"),
		*JsonString(ReplayA.RecordHash),
		*JsonString(ReplayB.RecordHash));

	const FString ReportPath = GetReportPath(Params);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	const bool bMetricsWritten = FFileHelper::SaveStringToFile(JsonLines, *MetricsPath);
	const FString Report = BuildReport(Results, bReplayPass, ReplayA.RecordHash, ReplayB.RecordHash, MetricsPath);
	const bool bReportWritten = FFileHelper::SaveStringToFile(Report, *ReportPath);

	bool bPass = bReplayPass && bMetricsWritten && bReportWritten;
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
