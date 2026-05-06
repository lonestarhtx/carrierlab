// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIE2Commandlet.h"

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
	constexpr double DistanceToleranceKm = 1.0e-6;
	constexpr double EdgeTTolerance = 1.0e-8;

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
		HashMix(Hash, static_cast<uint64>(Value.Len() + 1));
		for (const TCHAR Ch : Value)
		{
			HashMix(Hash, static_cast<uint64>(Ch) + 1ull);
		}
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

	FVector3d UnitFromLonLat(const double LonDeg, const double LatDeg)
	{
		const double Lon = FMath::DegreesToRadians(LonDeg);
		const double Lat = FMath::DegreesToRadians(LatDeg);
		const double CosLat = FMath::Cos(Lat);
		return FVector3d(CosLat * FMath::Cos(Lon), CosLat * FMath::Sin(Lon), FMath::Sin(Lat));
	}

	double DistanceKm(const FVector3d& A, const FVector3d& B)
	{
		return EarthRadiusKm * UnitAngularDistanceRadians(NormalizeOrFallback(A, FVector3d::UnitX()), NormalizeOrFallback(B, FVector3d::UnitX()));
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
				TEXT("IIIE2"));
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
				TEXT("phase-iii-slice-iiie2-report.md"));
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

	struct FDiscreteBoundaryPoint
	{
		int32 PlateId = INDEX_NONE;
		FVector3d UnitPosition = FVector3d::UnitZ();
	};

	struct FFixtureSpec
	{
		FString Name;
		FString Purpose;
		FVector3d Sample = FVector3d::UnitX();
		TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe> Edges;
		bool bExpectFound = true;
		int32 ExpectedQ1PlateId = INDEX_NONE;
		int32 ExpectedQ2PlateId = INDEX_NONE;
		int32 ExpectedQ1EdgeId = INDEX_NONE;
		int32 ExpectedQ2EdgeId = INDEX_NONE;
		double ExpectedQ1EdgeT = 0.0;
		double ExpectedQ2EdgeT = 0.0;
		FVector3d ExpectedQ1 = FVector3d::UnitX();
		FVector3d ExpectedQ2 = FVector3d::UnitY();
		bool bExpectDegenerateQGamma = false;
	};

	struct FFixtureResult
	{
		FString Name;
		FString Purpose;
		bool bQueryReturned = false;
		bool bPass = false;
		double Seconds = 0.0;
		double Q1VectorResidual = 0.0;
		double Q2VectorResidual = 0.0;
		double QGammaVectorResidual = 0.0;
		double Q1DistanceResidualKm = 0.0;
		double Q2DistanceResidualKm = 0.0;
		double Q1EdgeTResidual = 0.0;
		double Q2EdgeTResidual = 0.0;
		double QGammaInputNormResidual = 0.0;
		double DiscreteQ1ExtraDistanceKm = 0.0;
		FString AuditHash;
		FCarrierLabPhaseIIIE2BoundaryQueryAudit Audit;
	};

	struct FStatePathResult
	{
		bool bBuiltEdges = false;
		bool bExplicitReturned = false;
		bool bStateReturned = false;
		bool bPass = false;
		int32 BoundaryEdgeCount = 0;
		double Q1VectorResidual = 0.0;
		double Q2VectorResidual = 0.0;
		double QGammaVectorResidual = 0.0;
		FString ExplicitHash;
		FString StateHash;
		FCarrierLabPhaseIIIE2BoundaryQueryAudit ExplicitAudit;
		FCarrierLabPhaseIIIE2BoundaryQueryAudit StateAudit;
	};

	void AddEdge(
		FFixtureSpec& Fixture,
		const int32 PlateId,
		const double StartLon,
		const double StartLat,
		const double EndLon,
		const double EndLat,
		const double StartContinentalFraction = 0.0,
		const double EndContinentalFraction = 0.0)
	{
		FCarrierLabPhaseIIIE2BoundaryEdgeProbe Edge;
		Edge.PlateId = PlateId;
		Edge.StartUnitPosition = UnitFromLonLat(StartLon, StartLat);
		Edge.EndUnitPosition = UnitFromLonLat(EndLon, EndLat);
		Edge.StartContinentalFraction = StartContinentalFraction;
		Edge.EndContinentalFraction = EndContinentalFraction;
		Fixture.Edges.Add(Edge);
	}

	FString ComputeAuditHash(const FCarrierLabPhaseIIIE2BoundaryQueryAudit& Audit)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMix(Hash, Audit.bFound ? 2ull : 1ull);
		HashMix(Hash, static_cast<uint64>(Audit.BoundaryEdgeCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.DistinctPlateCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.Q1PlateId + 2));
		HashMix(Hash, static_cast<uint64>(Audit.Q2PlateId + 2));
		HashMix(Hash, static_cast<uint64>(Audit.Q1EdgeId + 2));
		HashMix(Hash, static_cast<uint64>(Audit.Q2EdgeId + 2));
		HashMixDouble(Hash, Audit.Q1DistanceKm);
		HashMixDouble(Hash, Audit.Q2DistanceKm);
		HashMixDouble(Hash, Audit.Q1EdgeT);
		HashMixDouble(Hash, Audit.Q2EdgeT);
		HashMixDouble(Hash, Audit.Q1UnitPosition.X);
		HashMixDouble(Hash, Audit.Q1UnitPosition.Y);
		HashMixDouble(Hash, Audit.Q1UnitPosition.Z);
		HashMixDouble(Hash, Audit.Q2UnitPosition.X);
		HashMixDouble(Hash, Audit.Q2UnitPosition.Y);
		HashMixDouble(Hash, Audit.Q2UnitPosition.Z);
		HashMixDouble(Hash, Audit.QGammaUnitPosition.X);
		HashMixDouble(Hash, Audit.QGammaUnitPosition.Y);
		HashMixDouble(Hash, Audit.QGammaUnitPosition.Z);
		HashMixDouble(Hash, Audit.QGammaInputNorm);
		HashMixDouble(Hash, Audit.QGammaUnitResidual);
		return HashToString(Hash);
	}

	bool IsBetterDiscretePoint(
		const FDiscreteBoundaryPoint& Candidate,
		const double CandidateDistanceKm,
		const FDiscreteBoundaryPoint& Best,
		const double BestDistanceKm)
	{
		if (Best.PlateId == INDEX_NONE)
		{
			return true;
		}
		if (CandidateDistanceKm < BestDistanceKm - 1.0e-9)
		{
			return true;
		}
		if (!FMath::IsNearlyEqual(CandidateDistanceKm, BestDistanceKm, 1.0e-9))
		{
			return false;
		}
		return Candidate.PlateId < Best.PlateId;
	}

	bool FindDiscreteDiagnosticQ1(
		const TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe>& Edges,
		const FVector3d& Sample,
		FDiscreteBoundaryPoint& OutQ1,
		double& OutDistanceKm)
	{
		OutQ1 = FDiscreteBoundaryPoint();
		OutDistanceKm = TNumericLimits<double>::Max();
		for (const FCarrierLabPhaseIIIE2BoundaryEdgeProbe& Edge : Edges)
		{
			const FVector3d Points[3] = {
				NormalizeOrFallback(Edge.StartUnitPosition, FVector3d::UnitX()),
				NormalizeOrFallback(Edge.EndUnitPosition, FVector3d::UnitY()),
				NormalizeOrFallback(Edge.StartUnitPosition + Edge.EndUnitPosition, Edge.StartUnitPosition)
			};
			for (const FVector3d& Point : Points)
			{
				FDiscreteBoundaryPoint Candidate;
				Candidate.PlateId = Edge.PlateId;
				Candidate.UnitPosition = Point;
				const double CandidateDistanceKm = DistanceKm(Sample, Point);
				if (IsBetterDiscretePoint(Candidate, CandidateDistanceKm, OutQ1, OutDistanceKm))
				{
					OutQ1 = Candidate;
					OutDistanceKm = CandidateDistanceKm;
				}
			}
		}
		return OutQ1.PlateId != INDEX_NONE;
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
		OutResult.bQueryReturned = Actor.QueryPhaseIIIE2ContinuousBoundaryPairForTest(Fixture.Sample, Fixture.Edges, OutResult.Audit);
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.AuditHash = ComputeAuditHash(OutResult.Audit);

		if (!Fixture.bExpectFound)
		{
			OutResult.bPass =
				!OutResult.bQueryReturned &&
				!OutResult.Audit.bFound &&
				OutResult.Audit.DistinctPlateCount < 2 &&
				OutResult.Audit.BoundaryEdgeCount == Fixture.Edges.Num();
			return OutResult.bPass;
		}

		const FVector3d ExpectedQGamma = NormalizeOrFallback(Fixture.ExpectedQ1 + Fixture.ExpectedQ2, Fixture.Sample);
		OutResult.Q1VectorResidual = (OutResult.Audit.Q1UnitPosition - Fixture.ExpectedQ1).Size();
		OutResult.Q2VectorResidual = (OutResult.Audit.Q2UnitPosition - Fixture.ExpectedQ2).Size();
		OutResult.QGammaVectorResidual = (OutResult.Audit.QGammaUnitPosition - ExpectedQGamma).Size();
		OutResult.Q1DistanceResidualKm = FMath::Abs(OutResult.Audit.Q1DistanceKm - DistanceKm(Fixture.Sample, Fixture.ExpectedQ1));
		OutResult.Q2DistanceResidualKm = FMath::Abs(OutResult.Audit.Q2DistanceKm - DistanceKm(Fixture.Sample, Fixture.ExpectedQ2));
		OutResult.Q1EdgeTResidual = FMath::Abs(OutResult.Audit.Q1EdgeT - Fixture.ExpectedQ1EdgeT);
		OutResult.Q2EdgeTResidual = FMath::Abs(OutResult.Audit.Q2EdgeT - Fixture.ExpectedQ2EdgeT);
		OutResult.QGammaInputNormResidual = Fixture.bExpectDegenerateQGamma
			? OutResult.Audit.QGammaInputNorm
			: FMath::Abs(OutResult.Audit.QGammaInputNorm - (Fixture.ExpectedQ1 + Fixture.ExpectedQ2).Size());

		FDiscreteBoundaryPoint DiscreteQ1;
		double DiscreteQ1DistanceKm = 0.0;
		if (FindDiscreteDiagnosticQ1(Fixture.Edges, Fixture.Sample, DiscreteQ1, DiscreteQ1DistanceKm))
		{
			OutResult.DiscreteQ1ExtraDistanceKm = DiscreteQ1DistanceKm - OutResult.Audit.Q1DistanceKm;
		}

		OutResult.bPass =
			OutResult.bQueryReturned &&
			OutResult.Audit.bFound &&
			OutResult.Audit.Q1PlateId == Fixture.ExpectedQ1PlateId &&
			OutResult.Audit.Q2PlateId == Fixture.ExpectedQ2PlateId &&
			OutResult.Audit.Q1EdgeId == Fixture.ExpectedQ1EdgeId &&
			OutResult.Audit.Q2EdgeId == Fixture.ExpectedQ2EdgeId &&
			OutResult.Audit.Q1PlateId != OutResult.Audit.Q2PlateId &&
			OutResult.Q1VectorResidual <= VectorTolerance &&
			OutResult.Q2VectorResidual <= VectorTolerance &&
			OutResult.QGammaVectorResidual <= VectorTolerance &&
			OutResult.Q1DistanceResidualKm <= DistanceToleranceKm &&
			OutResult.Q2DistanceResidualKm <= DistanceToleranceKm &&
			OutResult.Q1EdgeTResidual <= EdgeTTolerance &&
			OutResult.Q2EdgeTResidual <= EdgeTTolerance &&
			(!Fixture.bExpectDegenerateQGamma || OutResult.Audit.QGammaInputNorm <= VectorTolerance) &&
			OutResult.Audit.QGammaUnitResidual <= VectorTolerance;
		return OutResult.bPass;
	}

	bool RunStatePathFixture(ACarrierLabVisualizationActor& Actor, FStatePathResult& OutResult)
	{
		OutResult = FStatePathResult();
		Actor.SampleCount = 512;
		Actor.PlateCount = 4;
		Actor.Seed = 42;
		Actor.ContinentalPlateFraction = 0.50;
		if (!Actor.InitializeCarrier())
		{
			return false;
		}

		TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe> CurrentStateEdges;
		OutResult.bBuiltEdges = Actor.BuildPhaseIIIE2BoundaryEdgesFromCurrentStateForTest(CurrentStateEdges);
		OutResult.BoundaryEdgeCount = CurrentStateEdges.Num();
		const FVector3d Sample = UnitFromLonLat(10.0, 10.0);
		OutResult.bExplicitReturned = Actor.QueryPhaseIIIE2ContinuousBoundaryPairForTest(Sample, CurrentStateEdges, OutResult.ExplicitAudit);
		OutResult.bStateReturned = Actor.QueryPhaseIIIE2ContinuousBoundaryPairFromCurrentStateForTest(Sample, OutResult.StateAudit);
		OutResult.ExplicitHash = ComputeAuditHash(OutResult.ExplicitAudit);
		OutResult.StateHash = ComputeAuditHash(OutResult.StateAudit);
		OutResult.Q1VectorResidual = (OutResult.ExplicitAudit.Q1UnitPosition - OutResult.StateAudit.Q1UnitPosition).Size();
		OutResult.Q2VectorResidual = (OutResult.ExplicitAudit.Q2UnitPosition - OutResult.StateAudit.Q2UnitPosition).Size();
		OutResult.QGammaVectorResidual = (OutResult.ExplicitAudit.QGammaUnitPosition - OutResult.StateAudit.QGammaUnitPosition).Size();
		OutResult.bPass =
			OutResult.bBuiltEdges &&
			OutResult.BoundaryEdgeCount > 0 &&
			OutResult.bExplicitReturned &&
			OutResult.bStateReturned &&
			OutResult.ExplicitAudit.bFound &&
			OutResult.StateAudit.bFound &&
			OutResult.ExplicitAudit.DistinctPlateCount >= 2 &&
			OutResult.StateAudit.DistinctPlateCount >= 2 &&
			OutResult.ExplicitAudit.Q1PlateId == OutResult.StateAudit.Q1PlateId &&
			OutResult.ExplicitAudit.Q2PlateId == OutResult.StateAudit.Q2PlateId &&
			OutResult.ExplicitAudit.Q1EdgeId == OutResult.StateAudit.Q1EdgeId &&
			OutResult.ExplicitAudit.Q2EdgeId == OutResult.StateAudit.Q2EdgeId &&
			OutResult.Q1VectorResidual <= VectorTolerance &&
			OutResult.Q2VectorResidual <= VectorTolerance &&
			OutResult.QGammaVectorResidual <= VectorTolerance &&
			OutResult.ExplicitHash == OutResult.StateHash;
		return OutResult.bPass;
	}

	FString BuildJsonLine(const FFixtureResult& Result)
	{
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"purpose\":%s,\"pass\":%s,\"query_returned\":%s,\"found\":%s,\"seconds\":%.9f,\"edge_count\":%d,\"distinct_plate_count\":%d,\"q1_plate\":%d,\"q2_plate\":%d,\"q1_edge\":%d,\"q2_edge\":%d,\"q1_distance_km\":%.12f,\"q2_distance_km\":%.12f,\"q1_t\":%.12f,\"q2_t\":%.12f,\"qgamma_input_norm\":%.12g,\"qgamma_residual\":%.12g,\"q1_vector_residual\":%.12g,\"q2_vector_residual\":%.12g,\"qgamma_vector_residual\":%.12g,\"discrete_q1_extra_distance_km\":%.12f,\"audit_hash\":%s}"),
			*JsonString(Result.Name),
			*JsonString(Result.Purpose),
			Result.bPass ? TEXT("true") : TEXT("false"),
			Result.bQueryReturned ? TEXT("true") : TEXT("false"),
			Result.Audit.bFound ? TEXT("true") : TEXT("false"),
			Result.Seconds,
			Result.Audit.BoundaryEdgeCount,
			Result.Audit.DistinctPlateCount,
			Result.Audit.Q1PlateId,
			Result.Audit.Q2PlateId,
			Result.Audit.Q1EdgeId,
			Result.Audit.Q2EdgeId,
			Result.Audit.Q1DistanceKm,
			Result.Audit.Q2DistanceKm,
			Result.Audit.Q1EdgeT,
			Result.Audit.Q2EdgeT,
			Result.Audit.QGammaInputNorm,
			Result.Audit.QGammaUnitResidual,
			Result.Q1VectorResidual,
			Result.Q2VectorResidual,
			Result.QGammaVectorResidual,
			Result.DiscreteQ1ExtraDistanceKm,
			*JsonString(Result.AuditHash));
	}

	FString BuildStatePathJsonLine(const FStatePathResult& Result)
	{
		return FString::Printf(
			TEXT("{\"fixture\":\"actor-state boundary path\",\"pass\":%s,\"built_edges\":%s,\"explicit_returned\":%s,\"state_returned\":%s,\"edge_count\":%d,\"q1_plate\":%d,\"q2_plate\":%d,\"q1_vector_residual\":%.12g,\"q2_vector_residual\":%.12g,\"qgamma_vector_residual\":%.12g,\"explicit_hash\":%s,\"state_hash\":%s}"),
			Result.bPass ? TEXT("true") : TEXT("false"),
			Result.bBuiltEdges ? TEXT("true") : TEXT("false"),
			Result.bExplicitReturned ? TEXT("true") : TEXT("false"),
			Result.bStateReturned ? TEXT("true") : TEXT("false"),
			Result.BoundaryEdgeCount,
			Result.StateAudit.Q1PlateId,
			Result.StateAudit.Q2PlateId,
			Result.Q1VectorResidual,
			Result.Q2VectorResidual,
			Result.QGammaVectorResidual,
			*JsonString(Result.ExplicitHash),
			*JsonString(Result.StateHash));
	}

	FString BuildReport(
		const TArray<FFixtureResult>& Results,
		const bool bReplayPass,
		const FString& ReplayHashA,
		const FString& ReplayHashB,
		const FStatePathResult& StatePath,
		const FString& MetricsPath)
	{
		bool bFixturePass = true;
		for (const FFixtureResult& Result : Results)
		{
			bFixturePass = bFixturePass && Result.bPass;
		}
		const bool bPass = bFixturePass && bReplayPass && StatePath.bPass;

		FString Report;
		Report += TEXT("# Phase IIIE.2 Continuous q1/q2/qGamma Contract\n\n");
		Report += TEXT("Verdict: ");
		Report += bPass ? TEXT("PASS / IIIE.3 UNBLOCKED") : TEXT("FAIL / HOLD IIIE.3");
		Report += TEXT(". This slice implements the paper-faithful continuous divergent-gap provenance query only. It does not implement remesh source filtering, gap-field mutation, topology rebuild, process-state reset, optimization, or oceanic generation.\n\n");

		Report += TEXT("## Scope\n\n");
		Report += TEXT("- Primary IIIE gap-fill provenance now has a continuous closest-point-on-boundary-edge query surface for q1 and q2, with q2 required to come from a different plate than q1.\n");
		Report += TEXT("- qGamma is computed as the normalized spherical midpoint provenance `normalize(q1 + q2)`.\n");
		Report += TEXT("- Degenerate qGamma input is observable through `QGammaInputNorm`; the query falls back to the sample direction only when q1 + q2 has zero usable length.\n");
		Report += TEXT("- Exact equal-distance q1 ties use deterministic `(plate id, edge id)` ordering as a named lab convention for stable diagnostics; this is not an ownership or remesh-winner rule.\n");
		Report += TEXT("- Endpoint/midpoint boundary sampling remains diagnostic-only in this checkpoint; it is not used by the primary IIIE.2 query.\n");
		Report += TEXT("- The Stage 1.5 resampling path remains lab-policy code and is not promoted into primary Phase IIIE remesh.\n\n");
		Report += TEXT("- IIIB independent-signature regression is intentionally absent here because IIIE.2 is state-free provenance math; it returns in IIIE.3 when the slice consumes plate-local BVHs and process state.\n\n");

		Report += TEXT("## Gates\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---:|---|\n");
		for (const FFixtureResult& Result : Results)
		{
			Report += FString::Printf(
				TEXT("| %s | %s | q1 plate/edge `%d/%d`, q2 plate/edge `%d/%d`, q1 residual `%.3g`, q2 residual `%.3g`, qGamma residual `%.3g`, qGamma input norm `%.3g`, qGamma unit residual `%.3g`, hash `%s`. |\n"),
				*Result.Name,
				*PassFail(Result.bPass),
				Result.Audit.Q1PlateId,
				Result.Audit.Q1EdgeId,
				Result.Audit.Q2PlateId,
				Result.Audit.Q2EdgeId,
				Result.Q1VectorResidual,
				Result.Q2VectorResidual,
				Result.QGammaVectorResidual,
				Result.Audit.QGammaInputNorm,
				Result.Audit.QGammaUnitResidual,
				*Result.AuditHash);
		}
		Report += FString::Printf(
			TEXT("| Actor-state boundary path | %s | built `%d` current-state boundary edges; explicit hash `%s`, state hash `%s`, q1/q2/qGamma residuals `%.3g` / `%.3g` / `%.3g`. |\n"),
			*PassFail(StatePath.bPass),
			StatePath.BoundaryEdgeCount,
			*StatePath.ExplicitHash,
			*StatePath.StateHash,
			StatePath.Q1VectorResidual,
			StatePath.Q2VectorResidual,
			StatePath.QGammaVectorResidual);
		Report += FString::Printf(
			TEXT("| Same-seed provenance replay | %s | Replay hashes `%s` and `%s`. |\n"),
			*PassFail(bReplayPass),
			*ReplayHashA,
			*ReplayHashB);
		Report += TEXT("\n## Diagnostic Approximation Check\n\n");
		Report += TEXT("| Fixture | Continuous q1 distance km | Endpoint/midpoint diagnostic extra distance km | Interpretation |\n");
		Report += TEXT("|---|---:|---:|---|\n");
		for (const FFixtureResult& Result : Results)
		{
			if (!Result.Audit.bFound)
			{
				continue;
			}
			Report += FString::Printf(
				TEXT("| %s | `%.9f` | `%.9f` | Diagnostic only; the primary query uses the continuous edge point. |\n"),
				*Result.Name,
				Result.Audit.Q1DistanceKm,
				Result.DiscreteQ1ExtraDistanceKm);
		}

		Report += TEXT("\n## Stop Conditions\n\n");
		Report += TEXT("- Hold IIIE.3 if any primary query result uses an endpoint/midpoint approximation as authority.\n");
		Report += TEXT("- Hold IIIE.3 if q2 can come from the same plate as q1.\n");
		Report += TEXT("- Hold IIIE.3 if zero two-plate boundary candidates trigger any IIIE.1-forbidden remesh winner, recovery, or ownership-retention policy.\n");
		Report += TEXT("- Hold IIIE.3 if qGamma is not a normalized spherical midpoint of q1 and q2.\n\n");

		Report += TEXT("## Next Slice Boundary\n\n");
		Report += TEXT("IIIE.3 should wire this query into filtered remesh source selection: center-out rays, process-state triangle invisibility, unresolved multi-hit gating, and zero-hit classification. It should still leave topology rebuild and process-state reset to later IIIE slices unless explicitly reprioritized.\n\n");

		Report += FString::Printf(TEXT("Metrics: `%s`.\n"), *MetricsPath);
		return Report;
	}

	FFixtureSpec MakeInteriorFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("interior continuous edge projection");
		Fixture.Purpose = TEXT("q1 is the closest continuous point inside a boundary edge, not an endpoint/midpoint sample");
		Fixture.Sample = UnitFromLonLat(10.0, 10.0);
		AddEdge(Fixture, 0, -30.0, 0.0, 30.0, 0.0, 0.10, 0.70);
		AddEdge(Fixture, 1, 100.0, 0.0, 120.0, 0.0, 0.20, 0.40);
		Fixture.ExpectedQ1PlateId = 0;
		Fixture.ExpectedQ2PlateId = 1;
		Fixture.ExpectedQ1EdgeId = 0;
		Fixture.ExpectedQ2EdgeId = 1;
		Fixture.ExpectedQ1EdgeT = 2.0 / 3.0;
		Fixture.ExpectedQ2EdgeT = 0.0;
		Fixture.ExpectedQ1 = UnitFromLonLat(10.0, 0.0);
		Fixture.ExpectedQ2 = UnitFromLonLat(100.0, 0.0);
		return Fixture;
	}

	FFixtureSpec MakeEndpointFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("arc endpoint clipping");
		Fixture.Purpose = TEXT("outside-edge projections clamp to the nearest endpoint of the continuous boundary arc");
		Fixture.Sample = UnitFromLonLat(0.0, 0.0);
		AddEdge(Fixture, 0, 20.0, 0.0, 40.0, 0.0);
		AddEdge(Fixture, 1, 100.0, 0.0, 120.0, 0.0);
		Fixture.ExpectedQ1PlateId = 0;
		Fixture.ExpectedQ2PlateId = 1;
		Fixture.ExpectedQ1EdgeId = 0;
		Fixture.ExpectedQ2EdgeId = 1;
		Fixture.ExpectedQ1EdgeT = 0.0;
		Fixture.ExpectedQ2EdgeT = 0.0;
		Fixture.ExpectedQ1 = UnitFromLonLat(20.0, 0.0);
		Fixture.ExpectedQ2 = UnitFromLonLat(100.0, 0.0);
		return Fixture;
	}

	FFixtureSpec MakeDifferentPlateFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("different-plate q2 selection");
		Fixture.Purpose = TEXT("q2 ignores additional nearest edges from q1 plate and selects the nearest edge from another plate");
		Fixture.Sample = UnitFromLonLat(10.0, 8.0);
		AddEdge(Fixture, 0, -20.0, 0.0, 40.0, 0.0);
		AddEdge(Fixture, 0, 10.0, 0.0, 20.0, 0.0);
		AddEdge(Fixture, 1, 80.0, 0.0, 100.0, 0.0);
		Fixture.ExpectedQ1PlateId = 0;
		Fixture.ExpectedQ2PlateId = 1;
		Fixture.ExpectedQ1EdgeId = 0;
		Fixture.ExpectedQ2EdgeId = 2;
		Fixture.ExpectedQ1EdgeT = 0.5;
		Fixture.ExpectedQ2EdgeT = 0.0;
		Fixture.ExpectedQ1 = UnitFromLonLat(10.0, 0.0);
		Fixture.ExpectedQ2 = UnitFromLonLat(80.0, 0.0);
		return Fixture;
	}

	FFixtureSpec MakeDegenerateQGammaFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("degenerate qGamma fallback is observable");
		Fixture.Purpose = TEXT("antipodal q1/q2 provenance records near-zero qGamma input norm and falls back to the sample direction");
		Fixture.Sample = UnitFromLonLat(0.0, 0.0);
		AddEdge(Fixture, 0, 0.0, 0.0, 0.0, 0.0);
		AddEdge(Fixture, 1, 180.0, 0.0, 180.0, 0.0);
		Fixture.ExpectedQ1PlateId = 0;
		Fixture.ExpectedQ2PlateId = 1;
		Fixture.ExpectedQ1EdgeId = 0;
		Fixture.ExpectedQ2EdgeId = 1;
		Fixture.ExpectedQ1EdgeT = 0.0;
		Fixture.ExpectedQ2EdgeT = 0.0;
		Fixture.ExpectedQ1 = UnitFromLonLat(0.0, 0.0);
		Fixture.ExpectedQ2 = UnitFromLonLat(180.0, 0.0);
		Fixture.bExpectDegenerateQGamma = true;
		return Fixture;
	}

	FFixtureSpec MakeTieBreakFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("deterministic equal-distance tie convention");
		Fixture.Purpose = TEXT("exact q1 ties are stable by plate id then edge id, and are recorded as a diagnostic convention rather than ownership authority");
		Fixture.Sample = UnitFromLonLat(0.0, 0.0);
		AddEdge(Fixture, 1, 20.0, 0.0, 20.0, 0.0);
		AddEdge(Fixture, 0, 20.0, 0.0, 20.0, 0.0);
		Fixture.ExpectedQ1PlateId = 0;
		Fixture.ExpectedQ2PlateId = 1;
		Fixture.ExpectedQ1EdgeId = 1;
		Fixture.ExpectedQ2EdgeId = 0;
		Fixture.ExpectedQ1EdgeT = 0.0;
		Fixture.ExpectedQ2EdgeT = 0.0;
		Fixture.ExpectedQ1 = UnitFromLonLat(20.0, 0.0);
		Fixture.ExpectedQ2 = UnitFromLonLat(20.0, 0.0);
		return Fixture;
	}

	FFixtureSpec MakeNoTwoPlateFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("no two-plate boundary anomaly");
		Fixture.Purpose = TEXT("a one-plate boundary set is an explicit anomaly and does not fall back to prior ownership");
		Fixture.Sample = UnitFromLonLat(10.0, 10.0);
		AddEdge(Fixture, 0, -30.0, 0.0, 30.0, 0.0);
		Fixture.bExpectFound = false;
		return Fixture;
	}
}

UCarrierLabPhaseIIIE2Commandlet::UCarrierLabPhaseIIIE2Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIE2Commandlet::Main(const FString& Params)
{
	UWorld* World = GetCommandletWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPhaseIIIE2Commandlet could not find a commandlet world."));
		return 1;
	}

	ACarrierLabVisualizationActor* Actor = SpawnActor(*World);
	if (Actor == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPhaseIIIE2Commandlet could not spawn CarrierLabVisualizationActor."));
		return 1;
	}

	TArray<FFixtureSpec> Fixtures;
	Fixtures.Add(MakeInteriorFixture());
	Fixtures.Add(MakeEndpointFixture());
	Fixtures.Add(MakeDifferentPlateFixture());
	Fixtures.Add(MakeDegenerateQGammaFixture());
	Fixtures.Add(MakeTieBreakFixture());
	Fixtures.Add(MakeNoTwoPlateFixture());

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
	RunFixture(*Actor, MakeInteriorFixture(), ReplayA);
	RunFixture(*Actor, MakeInteriorFixture(), ReplayB);
	const bool bReplayPass = ReplayA.bPass && ReplayB.bPass && ReplayA.AuditHash == ReplayB.AuditHash;

	FStatePathResult StatePath;
	RunStatePathFixture(*Actor, StatePath);

	Actor->Destroy();
	CollectGarbage(RF_NoFlags);

	FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("phase-iii-slice-iiie2-metrics.jsonl"));
	FString JsonLines;
	for (const FFixtureResult& Result : Results)
	{
		JsonLines += BuildJsonLine(Result);
		JsonLines += TEXT("\n");
	}
	JsonLines += BuildJsonLine(ReplayA);
	JsonLines += TEXT("\n");
	JsonLines += BuildJsonLine(ReplayB);
	JsonLines += TEXT("\n");
	JsonLines += FString::Printf(
		TEXT("{\"fixture\":\"same-seed provenance replay\",\"pass\":%s,\"hash_a\":%s,\"hash_b\":%s}\n"),
		bReplayPass ? TEXT("true") : TEXT("false"),
		*JsonString(ReplayA.AuditHash),
		*JsonString(ReplayB.AuditHash));
	JsonLines += BuildStatePathJsonLine(StatePath);
	JsonLines += TEXT("\n");

	const FString ReportPath = GetReportPath(Params);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	const FString Report = BuildReport(Results, bReplayPass, ReplayA.AuditHash, ReplayB.AuditHash, StatePath, MetricsPath);
	const bool bMetricsWritten = FFileHelper::SaveStringToFile(JsonLines, *MetricsPath);
	const bool bReportWritten = FFileHelper::SaveStringToFile(Report, *ReportPath);

	bool bPass = bReplayPass && StatePath.bPass && bMetricsWritten && bReportWritten;
	for (const FFixtureResult& Result : Results)
	{
		bPass = bPass && Result.bPass;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("CarrierLab IIIE.2 gates: %s. Report=%s Metrics=%s"),
		*PassFail(bPass),
		*ReportPath,
		*MetricsPath);
	return bPass ? 0 : 1;
}
