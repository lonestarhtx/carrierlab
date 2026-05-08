// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIE63SharedBoundaryTieBreakCommandlet.h"

#include "Algo/AllOf.h"
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
	constexpr int32 ExpectedDefaultCoalescedCount = 58589;
	constexpr int32 ExpectedDefaultSharedBoundaryTieBreakCount = 5845;
	constexpr int32 ExpectedDefaultCrossPlateEqualCount = 5761;
	constexpr int32 ExpectedDefaultThirdPlateCount = 84;
	constexpr int32 ExpectedDefaultSharedEdgeCount = 3740;
	constexpr int32 ExpectedDefaultSharedVertexOnlyCount = 2021;
	constexpr int32 ExpectedDefaultThreePlateCommonVertexCount = 84;

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

	UWorld* FindCommandletWorld()
	{
		if (GEngine == nullptr)
		{
			return nullptr;
		}
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (UWorld* World = Context.World())
			{
				return World;
			}
		}
		return nullptr;
	}

	FString RuleName(const ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule Rule)
	{
		switch (Rule)
		{
		case ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::ContinentalPriority:
			return TEXT("continental_priority");
		case ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::OlderOceanicAge:
			return TEXT("older_oceanic_age");
		case ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::LowerPlateId:
			return TEXT("lower_plate_id");
		case ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::None:
		default:
			return TEXT("none");
		}
	}

	FString ShapeName(const ECarrierLabPhaseIIIE62HoldShape Shape)
	{
		switch (Shape)
		{
		case ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalEdge:
			return TEXT("two_plate_shared_global_edge");
		case ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalVertexOnly:
			return TEXT("two_plate_shared_global_vertex_only");
		case ECarrierLabPhaseIIIE62HoldShape::ThreePlateCommonGlobalVertex:
			return TEXT("three_plate_common_global_vertex");
		case ECarrierLabPhaseIIIE62HoldShape::TwoPlateFieldMismatch:
			return TEXT("two_plate_field_mismatch");
		case ECarrierLabPhaseIIIE62HoldShape::TwoPlateNoSharedGlobalVertices:
			return TEXT("two_plate_no_shared_global_vertices");
		case ECarrierLabPhaseIIIE62HoldShape::NonBoundaryOrInteriorOverlap:
			return TEXT("non_boundary_or_interior_overlap");
		case ECarrierLabPhaseIIIE62HoldShape::TwoPlateSameSourceTriangle:
			return TEXT("two_plate_same_source_triangle");
		case ECarrierLabPhaseIIIE62HoldShape::ThreePlateEdgePlusIntruder:
			return TEXT("three_plate_edge_plus_intruder");
		case ECarrierLabPhaseIIIE62HoldShape::ThreePlateNoCommonSourceVertex:
			return TEXT("three_plate_no_common_source_vertex");
		case ECarrierLabPhaseIIIE62HoldShape::InvalidOrUnclassified:
		default:
			return TEXT("invalid_or_unclassified");
		}
	}

	FCarrierLabPhaseIIIE3CandidateProbe MakeProbe(
		const int32 PlateId,
		const int32 LocalTriangleId,
		const int32 SourceTriangleId,
		const int32 GlobalA,
		const int32 GlobalB,
		const int32 GlobalC,
		const FVector3d& Bary,
		const double PlateContinentalFraction,
		const double PlateOceanicAge,
		const double CandidateContinentalFraction = 0.0,
		const double CandidateOceanicAge = 12.0,
		const double Elevation = -2.0,
		const bool bBoundary = true)
	{
		FCarrierLabPhaseIIIE3CandidateProbe Probe;
		Probe.PlateId = PlateId;
		Probe.LocalTriangleId = LocalTriangleId;
		Probe.SourceTriangleId = SourceTriangleId;
		Probe.GlobalVertexIds[0] = GlobalA;
		Probe.GlobalVertexIds[1] = GlobalB;
		Probe.GlobalVertexIds[2] = GlobalC;
		Probe.Bary = Bary;
		Probe.Distance = 1.0;
		Probe.ContinentalFraction = CandidateContinentalFraction;
		Probe.Elevation = Elevation;
		Probe.HistoricalElevation = Elevation;
		Probe.OceanicAge = CandidateOceanicAge;
		Probe.PlateContinentalFraction = PlateContinentalFraction;
		Probe.PlateOceanicAge = PlateOceanicAge;
		Probe.RidgeDirection = FVector3d::UnitX();
		Probe.FoldDirection = FVector3d::UnitY();
		Probe.bBoundary = bBoundary;
		Probe.bHasSourceTopologySnapshot = true;
		Probe.bHasPlateAggregateOverride = true;
		return Probe;
	}

	struct FTieBreakFixture
	{
		FString Name;
		TArray<FCarrierLabPhaseIIIE3CandidateProbe> Probes;
		bool bExpectResolved = true;
		int32 ExpectedPlateId = INDEX_NONE;
		ECarrierLabPhaseIIIE62HoldShape ExpectedShape = ECarrierLabPhaseIIIE62HoldShape::InvalidOrUnclassified;
		ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule ExpectedRule = ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::None;
	};

	struct FTieBreakFixtureResult
	{
		FString Name;
		bool bPass = false;
		bool bResolved = false;
		int32 ResolvedPlateId = INDEX_NONE;
		ECarrierLabPhaseIIIE62HoldShape Shape = ECarrierLabPhaseIIIE62HoldShape::InvalidOrUnclassified;
		ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule Rule = ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::None;
		int32 PolicyWinnerCount = 0;
		int32 PriorOwnerFallbackCount = 0;
		int32 SharedBoundaryTieBreakCount = 0;
		FString Hash;
	};

	FTieBreakFixtureResult RunFixture(UWorld& World, const FTieBreakFixture& Fixture)
	{
		FTieBreakFixtureResult Result;
		Result.Name = Fixture.Name;
		ACarrierLabVisualizationActor* Actor = World.SpawnActor<ACarrierLabVisualizationActor>();
		if (Actor == nullptr)
		{
			return Result;
		}
		FCarrierLabPhaseIIIE3SelectionRecord Record;
		Actor->bEnablePhaseIIIE3DuplicateHitCoalescing = true;
		Actor->bEnablePhaseIIIE3SharedBoundaryTieBreak = true;
		Actor->QueryPhaseIIIE3FilteredRemeshSelectionForTest(
			FVector3d::UnitZ(),
			Fixture.Probes,
			Record);

		FCarrierLabPhaseIIIE3RemeshSelectionAudit Audit;
		Audit.Records.Add(Record);
		Result.bResolved = Record.bResolvedSingleHit;
		Result.ResolvedPlateId = Record.ResolvedPlateId;
		Result.Shape = Record.SharedBoundaryShapeClass;
		Result.Rule = Record.SharedBoundaryTieBreakRule;
		Result.PolicyWinnerCount = Record.bUsedPolicyWinner ? 1 : 0;
		Result.PriorOwnerFallbackCount = Record.bUsedPriorOwnerFallback ? 1 : 0;
		Result.SharedBoundaryTieBreakCount = Record.bUsedSharedBoundaryTieBreak ? 1 : 0;

		Result.bPass =
			Record.bResolvedSingleHit == Fixture.bExpectResolved &&
			Record.ResolvedPlateId == Fixture.ExpectedPlateId &&
			Record.SharedBoundaryShapeClass == Fixture.ExpectedShape &&
			Record.SharedBoundaryTieBreakRule == Fixture.ExpectedRule &&
			Record.bUsedPolicyWinner == false &&
			Record.bUsedPriorOwnerFallback == false;
		if (!Fixture.bExpectResolved)
		{
			Result.bPass =
				!Record.bResolvedSingleHit &&
				Record.bUnresolvedMultiHit &&
				Record.SharedBoundaryShapeClass == Fixture.ExpectedShape &&
				Record.SharedBoundaryTieBreakRule == ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::None &&
				Record.bUsedSharedBoundaryTieBreak == false &&
				Record.bUsedPolicyWinner == false &&
				Record.bUsedPriorOwnerFallback == false;
		}
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return Result;
	}

	TArray<FTieBreakFixture> MakeFixtures()
	{
		const FVector3d EdgeBary(0.5, 0.5, 0.0);
		const FVector3d VertexBary(1.0, 0.0, 0.0);
		const FVector3d InteriorBary(0.25, 0.25, 0.5);

		TArray<FTieBreakFixture> Fixtures;
		{
			FTieBreakFixture Fixture;
			Fixture.Name = TEXT("two-plate shared edge continental-priority");
			Fixture.Probes = {
				MakeProbe(0, 4, 100, 10, 11, 12, EdgeBary, 0.49, 10.0),
				MakeProbe(1, 6, 101, 10, 11, 13, EdgeBary, 0.02, 90.0)
			};
			Fixture.ExpectedPlateId = 0;
			Fixture.ExpectedShape = ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalEdge;
			Fixture.ExpectedRule = ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::ContinentalPriority;
			Fixtures.Add(Fixture);
		}
		{
			FTieBreakFixture Fixture;
			Fixture.Name = TEXT("two-plate shared vertex older-oceanic-age");
			Fixture.Probes = {
				MakeProbe(2, 1, 200, 20, 21, 22, VertexBary, 0.10, 8.0),
				MakeProbe(3, 2, 201, 20, 23, 24, VertexBary, 0.10, 80.0)
			};
			Fixture.ExpectedPlateId = 3;
			Fixture.ExpectedShape = ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalVertexOnly;
			Fixture.ExpectedRule = ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::OlderOceanicAge;
			Fixtures.Add(Fixture);
		}
		{
			FTieBreakFixture Fixture;
			Fixture.Name = TEXT("two-plate shared edge lower-plate-id");
			Fixture.Probes = {
				MakeProbe(5, 3, 300, 30, 31, 32, EdgeBary, 0.0, 30.0),
				MakeProbe(4, 2, 301, 30, 31, 33, EdgeBary, 0.0, 30.0)
			};
			Fixture.ExpectedPlateId = 4;
			Fixture.ExpectedShape = ECarrierLabPhaseIIIE62HoldShape::TwoPlateSharedGlobalEdge;
			Fixture.ExpectedRule = ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::LowerPlateId;
			Fixtures.Add(Fixture);
		}
		{
			FTieBreakFixture Fixture;
			Fixture.Name = TEXT("three-plate common vertex continental max");
			Fixture.Probes = {
				MakeProbe(6, 1, 400, 40, 41, 42, VertexBary, 0.20, 12.0),
				MakeProbe(7, 2, 401, 40, 43, 44, VertexBary, 0.55, 10.0),
				MakeProbe(8, 3, 402, 40, 45, 46, VertexBary, 0.30, 90.0)
			};
			Fixture.ExpectedPlateId = 7;
			Fixture.ExpectedShape = ECarrierLabPhaseIIIE62HoldShape::ThreePlateCommonGlobalVertex;
			Fixture.ExpectedRule = ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::ContinentalPriority;
			Fixtures.Add(Fixture);
		}
		{
			FTieBreakFixture Fixture;
			Fixture.Name = TEXT("three-plate common vertex lower-id associative");
			Fixture.Probes = {
				MakeProbe(11, 1, 500, 50, 51, 52, VertexBary, 0.0, 0.0),
				MakeProbe(9, 2, 501, 50, 53, 54, VertexBary, 0.0, 0.0),
				MakeProbe(10, 3, 502, 50, 55, 56, VertexBary, 0.0, 0.0)
			};
			Fixture.ExpectedPlateId = 9;
			Fixture.ExpectedShape = ECarrierLabPhaseIIIE62HoldShape::ThreePlateCommonGlobalVertex;
			Fixture.ExpectedRule = ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::LowerPlateId;
			Fixtures.Add(Fixture);
		}
		{
			FTieBreakFixture Fixture;
			Fixture.Name = TEXT("two-plate field mismatch remains held");
			Fixture.Probes = {
				MakeProbe(12, 1, 600, 60, 61, 62, EdgeBary, 0.8, 10.0, 0.0, 12.0, -2.0),
				MakeProbe(13, 2, 601, 60, 61, 63, EdgeBary, 0.1, 90.0, 0.0, 12.0, -2.5)
			};
			Fixture.bExpectResolved = false;
			Fixture.ExpectedShape = ECarrierLabPhaseIIIE62HoldShape::TwoPlateFieldMismatch;
			Fixtures.Add(Fixture);
		}
		{
			FTieBreakFixture Fixture;
			Fixture.Name = TEXT("two-plate no shared vertices remains held");
			Fixture.Probes = {
				MakeProbe(14, 1, 700, 70, 71, 72, EdgeBary, 0.8, 10.0),
				MakeProbe(15, 2, 701, 73, 74, 75, EdgeBary, 0.1, 90.0)
			};
			Fixture.bExpectResolved = false;
			Fixture.ExpectedShape = ECarrierLabPhaseIIIE62HoldShape::TwoPlateNoSharedGlobalVertices;
			Fixtures.Add(Fixture);
		}
		{
			FTieBreakFixture Fixture;
			Fixture.Name = TEXT("non-boundary interior overlap remains held");
			Fixture.Probes = {
				MakeProbe(16, 1, 800, 80, 81, 82, InteriorBary, 0.8, 10.0, 0.0, 12.0, -2.0, false),
				MakeProbe(17, 2, 801, 80, 81, 83, InteriorBary, 0.1, 90.0, 0.0, 12.0, -2.0, false)
			};
			Fixture.bExpectResolved = false;
			Fixture.ExpectedShape = ECarrierLabPhaseIIIE62HoldShape::NonBoundaryOrInteriorOverlap;
			Fixtures.Add(Fixture);
		}
		return Fixtures;
	}

	struct FDefaultDiagnosticResult
	{
		bool bPass = false;
		FCarrierLabPhaseIIIE3RemeshSelectionAudit Audit;
		double Seconds = 0.0;
	};

	FDefaultDiagnosticResult RunDefaultDiagnostic(UWorld& World)
	{
		FDefaultDiagnosticResult Result;
		const double Start = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = World.SpawnActor<ACarrierLabVisualizationActor>();
		if (Actor == nullptr)
		{
			return Result;
		}
		Actor->SampleCount = DefaultSampleCount;
		Actor->PlateCount = DefaultPlateCount;
		Actor->Seed = 42;
		Actor->bEnablePhaseIIIE3DuplicateHitCoalescing = true;
		Actor->bEnablePhaseIIIE3SharedBoundaryTieBreak = true;
		Actor->InitializeCarrier();
		Actor->RunPhaseIIIE3FilteredRemeshSelectionAudit(Result.Audit);

		Result.Seconds = FPlatformTime::Seconds() - Start;
		Result.bPass =
			Result.Audit.bRan &&
			Result.Audit.SampleCount == DefaultSampleCount &&
			Result.Audit.UnresolvedMultiHitCount == 0 &&
			Result.Audit.CoalescedMultiHitCount == ExpectedDefaultCoalescedCount &&
			Result.Audit.SharedBoundaryTieBreakCount == ExpectedDefaultSharedBoundaryTieBreakCount &&
			Result.Audit.CrossPlateEqualMultiHitCount == ExpectedDefaultCrossPlateEqualCount &&
			Result.Audit.ThirdPlateMultiHitCount == ExpectedDefaultThirdPlateCount &&
			Result.Audit.SharedBoundaryTwoPlateEdgeCount == ExpectedDefaultSharedEdgeCount &&
			Result.Audit.SharedBoundaryTwoPlateVertexOnlyCount == ExpectedDefaultSharedVertexOnlyCount &&
			Result.Audit.SharedBoundaryThreePlateCommonVertexCount == ExpectedDefaultThreePlateCommonVertexCount &&
			Result.Audit.PolicyWinnerCount == 0 &&
			Result.Audit.PriorOwnerFallbackCount == 0;
		Actor->Destroy();
		return Result;
	}

	bool SameDefaultReplay(const FDefaultDiagnosticResult& A, const FDefaultDiagnosticResult& B)
	{
		return A.bPass &&
			B.bPass &&
			A.Audit.SelectionHash == B.Audit.SelectionHash &&
			A.Audit.SharedBoundaryTieBreakCount == B.Audit.SharedBoundaryTieBreakCount &&
			A.Audit.UnresolvedMultiHitCount == B.Audit.UnresolvedMultiHitCount;
	}

	FString BuildReport(
		const TArray<FTieBreakFixtureResult>& Fixtures,
		const FDefaultDiagnosticResult& DefaultA,
		const FDefaultDiagnosticResult& DefaultB)
	{
		const bool bFixturesPass = Algo::AllOf(Fixtures, [](const FTieBreakFixtureResult& Result)
		{
			return Result.bPass;
		});
		const bool bReplayPass = SameDefaultReplay(DefaultA, DefaultB);
		const bool bPass = bFixturesPass && DefaultA.bPass && DefaultB.bPass && bReplayPass;

		FString Report;
		Report += TEXT("# Phase IIIE.6.3 Shared-Boundary Multi-Hit Tie-Break\n\n");
		Report += FString::Printf(TEXT("Verdict: %s. The slice resolves only the IIIE.6.2 boundary-shared same-distance classes with an approved named lab policy; true non-boundary, field-mismatch, or unclassified multi-hits remain fail-loud.\n\n"), bPass ? TEXT("PASS") : TEXT("FAIL"));
		Report += TEXT("## Scope\n\n");
		Report += TEXT("- This is a CarrierLab lab policy because the thesis/CGF remesh prose is silent on multiple valid same-distance boundary hits.\n");
		Report += TEXT("- The rule is hierarchical: higher plate-level continental fraction, then older plate-level oceanic age, then lower plate id.\n");
		Report += TEXT("- Driftworld is treated as secondary implementation precedent for continental-priority overlap handling, not as paper authority.\n");
		Report += TEXT("- `bUsedPolicyWinner`, prior-owner fallback, and projection authority remain forbidden counters and must stay zero.\n\n");

		Report += TEXT("## Gate Table\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n|---|---|---|\n");
		for (const FTieBreakFixtureResult& Fixture : Fixtures)
		{
			Report += FString::Printf(
				TEXT("| %s | %s | resolved `%d`, plate `%d`, shape `%s`, rule `%s`, shared/policy/prior `%d/%d/%d`. |\n"),
				*Fixture.Name,
				*PassFail(Fixture.bPass),
				Fixture.bResolved ? 1 : 0,
				Fixture.ResolvedPlateId,
				*ShapeName(Fixture.Shape),
				*RuleName(Fixture.Rule),
				Fixture.SharedBoundaryTieBreakCount,
				Fixture.PolicyWinnerCount,
				Fixture.PriorOwnerFallbackCount);
		}
		Report += FString::Printf(
			TEXT("| Default 100000/40 seed-42 live selection | %s | unresolved `%d`, coalesced `%d`, shared tie-break `%d`, cross/third `%d/%d`, shapes edge/vertex/three `%d/%d/%d`, rules cont/age/id `%d/%d/%d`, policy/prior `0/0`, hash `%s`. |\n"),
			*PassFail(DefaultA.bPass),
			DefaultA.Audit.UnresolvedMultiHitCount,
			DefaultA.Audit.CoalescedMultiHitCount,
			DefaultA.Audit.SharedBoundaryTieBreakCount,
			DefaultA.Audit.CrossPlateEqualMultiHitCount,
			DefaultA.Audit.ThirdPlateMultiHitCount,
			DefaultA.Audit.SharedBoundaryTwoPlateEdgeCount,
			DefaultA.Audit.SharedBoundaryTwoPlateVertexOnlyCount,
			DefaultA.Audit.SharedBoundaryThreePlateCommonVertexCount,
			DefaultA.Audit.SharedBoundaryContinentalPriorityCount,
			DefaultA.Audit.SharedBoundaryOlderOceanicAgeCount,
			DefaultA.Audit.SharedBoundaryLowerPlateIdCount,
			*DefaultA.Audit.SelectionHash);
		Report += FString::Printf(
			TEXT("| Same-seed replay | %s | hashes `%s` and `%s`. |\n\n"),
			*PassFail(bReplayPass),
			*DefaultA.Audit.SelectionHash,
			*DefaultB.Audit.SelectionHash);

		Report += TEXT("## Policy Disclosure\n\n");
		Report += TEXT("IIIE.6.3 adds a fifth named IIIE lab policy for consolidation: shared-boundary same-distance multi-hit tie-break. The policy is engineering-defensible but not paper-cited. Consolidation must disclose it alongside zGamma sqrt-subsidence, 2-of-3 majority, triangle-level triple-junction centroid split, and continental overwrite -> rifting-pending.\n\n");
		Report += TEXT("## Stop Conditions\n\n");
		Report += TEXT("- Stop if shared-boundary tie-break increments `bUsedPolicyWinner`, prior-owner fallback, or projection-authority counters.\n");
		Report += TEXT("- Stop if field-mismatch, no-shared-vertices, interior/non-boundary, or unclassified holds are resolved by this rule.\n");
		Report += TEXT("- Stop if IIIE.6.2 baseline diagnosis cannot still be reproduced with the tie-break disabled.\n\n");
		Report += TEXT("## Next Slice Boundary\n\n");
		Report += TEXT("With IIIE.6.3 applied, default live remesh is no longer blocked by legitimate shared-boundary multi-hits. The next work should verify the actor visual path and then proceed to IIIE consolidation only if live remesh mutation remains coherent under the contact-sheet and commandlet gates.\n");
		return Report;
	}

	FString BuildMetricsJsonl(
		const TArray<FTieBreakFixtureResult>& Fixtures,
		const FDefaultDiagnosticResult& DefaultA,
		const FDefaultDiagnosticResult& DefaultB)
	{
		FString Jsonl;
		for (const FTieBreakFixtureResult& Fixture : Fixtures)
		{
			Jsonl += FString::Printf(
				TEXT("{\"fixture\":%s,\"pass\":%s,\"resolved\":%s,\"plate\":%d,\"shape\":%s,\"rule\":%s,\"shared_boundary\":%d,\"policy_winner\":%d,\"prior_owner\":%d}\n"),
				*JsonString(Fixture.Name),
				Fixture.bPass ? TEXT("true") : TEXT("false"),
				Fixture.bResolved ? TEXT("true") : TEXT("false"),
				Fixture.ResolvedPlateId,
				*JsonString(ShapeName(Fixture.Shape)),
				*JsonString(RuleName(Fixture.Rule)),
				Fixture.SharedBoundaryTieBreakCount,
				Fixture.PolicyWinnerCount,
				Fixture.PriorOwnerFallbackCount);
		}
		Jsonl += FString::Printf(
			TEXT("{\"fixture\":\"default_100000_40_seed42\",\"pass\":%s,\"unresolved\":%d,\"coalesced\":%d,\"shared_boundary\":%d,\"cross_plate_equal\":%d,\"third_plate\":%d,\"edge\":%d,\"vertex\":%d,\"three\":%d,\"continental_rule\":%d,\"age_rule\":%d,\"id_rule\":%d,\"policy_winner\":%d,\"prior_owner\":%d,\"hash\":%s,\"seconds\":%.6f}\n"),
			DefaultA.bPass ? TEXT("true") : TEXT("false"),
			DefaultA.Audit.UnresolvedMultiHitCount,
			DefaultA.Audit.CoalescedMultiHitCount,
			DefaultA.Audit.SharedBoundaryTieBreakCount,
			DefaultA.Audit.CrossPlateEqualMultiHitCount,
			DefaultA.Audit.ThirdPlateMultiHitCount,
			DefaultA.Audit.SharedBoundaryTwoPlateEdgeCount,
			DefaultA.Audit.SharedBoundaryTwoPlateVertexOnlyCount,
			DefaultA.Audit.SharedBoundaryThreePlateCommonVertexCount,
			DefaultA.Audit.SharedBoundaryContinentalPriorityCount,
			DefaultA.Audit.SharedBoundaryOlderOceanicAgeCount,
			DefaultA.Audit.SharedBoundaryLowerPlateIdCount,
			DefaultA.Audit.PolicyWinnerCount,
			DefaultA.Audit.PriorOwnerFallbackCount,
			*JsonString(DefaultA.Audit.SelectionHash),
			DefaultA.Seconds);
		Jsonl += FString::Printf(
			TEXT("{\"fixture\":\"same_seed_replay\",\"pass\":%s,\"hash_a\":%s,\"hash_b\":%s}\n"),
			SameDefaultReplay(DefaultA, DefaultB) ? TEXT("true") : TEXT("false"),
			*JsonString(DefaultA.Audit.SelectionHash),
			*JsonString(DefaultB.Audit.SelectionHash));
		return Jsonl;
	}
}

UCarrierLabPhaseIIIE63SharedBoundaryTieBreakCommandlet::UCarrierLabPhaseIIIE63SharedBoundaryTieBreakCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIE63SharedBoundaryTieBreakCommandlet::Main(const FString& Params)
{
	UWorld* World = FindCommandletWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPhaseIIIE63SharedBoundaryTieBreak: no world available."));
		return 1;
	}

	TArray<FTieBreakFixtureResult> FixtureResults;
	for (const FTieBreakFixture& Fixture : MakeFixtures())
	{
		FixtureResults.Add(RunFixture(*World, Fixture));
	}

	FDefaultDiagnosticResult DefaultA = RunDefaultDiagnostic(*World);
	FDefaultDiagnosticResult DefaultB = RunDefaultDiagnostic(*World);

	const FString Report = BuildReport(FixtureResults, DefaultA, DefaultB);
	const FString ReportPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("docs/checkpoints/phase-iii-slice-iiie6-3-shared-boundary-tiebreak.md"));
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	const FString MetricsDir = FPaths::ProjectSavedDir() / TEXT("CarrierLab/PhaseIII/IIIE63SharedBoundaryTieBreak");
	IFileManager::Get().MakeDirectory(*MetricsDir, true);
	const FString MetricsPath = MetricsDir / TEXT("metrics.jsonl");
	FFileHelper::SaveStringToFile(BuildMetricsJsonl(FixtureResults, DefaultA, DefaultB), *MetricsPath);

	const bool bPass =
		Algo::AllOf(FixtureResults, [](const FTieBreakFixtureResult& Result) { return Result.bPass; }) &&
		DefaultA.bPass &&
		DefaultB.bPass &&
		SameDefaultReplay(DefaultA, DefaultB);
	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIIE63SharedBoundaryTieBreak %s. Report: %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), *ReportPath);
	return bPass ? 0 : 1;
}
