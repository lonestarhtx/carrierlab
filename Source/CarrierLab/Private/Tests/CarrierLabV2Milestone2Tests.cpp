// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CarrierLabV2Core.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCarrierLabV2Milestone2CarrierCycleFixturesTest,
	"CarrierLab.V2.Milestone2.CarrierCycleFixtures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCarrierLabV2Milestone2CarrierCycleFixturesTest::RunTest(const FString& Parameters)
{
	const TArray<CarrierLab::V2::FCarrierV2Milestone2Config> Configs = CarrierLab::V2::FCarrierV2Milestone2::MakeMicroFixtureConfigs();
	TestEqual(TEXT("M2 has six required micro fixtures"), Configs.Num(), 6);

	bool bAllPassed = true;
	bool bSawNoop = false;
	bool bSawTransfer = false;
	bool bSawGapFill = false;
	bool bSawOverlapBlocked = false;
	bool bSawLifecycle = false;
	bool bSawPriorOwnerControl = false;

	for (const CarrierLab::V2::FCarrierV2Milestone2Config& Config : Configs)
	{
		CarrierLab::V2::FCarrierV2Milestone2FixtureResult Result;
		CarrierLab::V2::FCarrierV2Milestone2::RunFixtureWithReplay(Config, Result);
		AddInfo(FString::Printf(
			TEXT("[CarrierLabM2 fixture=%s pass=%s replay=%s single=%d gaps=%d q1q2=%d qgamma=%d generated=%d no_pair=%d overlap_blocked=%d unsupported_overlap_writes=%d owner_reads=%d resolvers=%d/%d/%d rebuilds=%d rebuilt=%d/%d unassigned=%d material_delta=%.12f tv_delta=%.12f hashes=%s/%s/%s replay=%s/%s/%s step_ms=%.3f total_ms=%.3f]"),
			*Result.Metrics.FixtureId,
			Result.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Result.Metrics.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			Result.Metrics.ValidSingleHitWriteCount,
			Result.Metrics.DivergentZeroHitCount,
			Result.Metrics.Q1Q2BoundaryPairCount,
			Result.Metrics.QGammaComputedCount,
			Result.Metrics.GeneratedOceanicCount,
			Result.Metrics.GapFillNoBoundaryPairCount,
			Result.Metrics.NondegenerateOverlapBlockedCount,
			Result.Metrics.UnsupportedOverlapWriteAttemptCount,
			Result.Metrics.PriorOwnerReadCount + Result.Metrics.GlobalOwnerReadCount,
			Result.Metrics.CentroidPrimaryResolutionCount,
			Result.Metrics.RandomPrimaryResolutionCount,
			Result.Metrics.NearestPrimaryResolutionCount,
			Result.Metrics.TopologyRebuildCount,
			Result.Metrics.RebuiltTriangleAssignmentCount,
			Result.Metrics.GlobalTriangleCount,
			Result.Metrics.UnassignedTriangleCount,
			Result.Metrics.MaterialConservationDelta,
			Result.Metrics.TotalVariationDelta,
			*Result.Metrics.PostCycleAuthorityHash,
			*Result.Metrics.ResampleOutputHash,
			*Result.Metrics.RebuiltTopologyHash,
			*Result.Metrics.ReplayPostCycleAuthorityHash,
			*Result.Metrics.ReplayResampleOutputHash,
			*Result.Metrics.ReplayRebuiltTopologyHash,
			Result.Metrics.StepKernelMs,
			Result.Metrics.TotalMs));

		TestTrue(FString::Printf(TEXT("%s fixture pass"), *Config.FixtureId), Result.Metrics.bFixturePass);
		TestTrue(FString::Printf(TEXT("%s replay deterministic"), *Config.FixtureId), Result.Metrics.bReplayDeterministic);
		TestTrue(FString::Printf(TEXT("%s single-hit pass"), *Config.FixtureId), Result.Metrics.bSingleHitTransferPass);
		TestTrue(FString::Printf(TEXT("%s q1/q2 pass"), *Config.FixtureId), Result.Metrics.bDivergentGapFillPass);
		TestTrue(FString::Printf(TEXT("%s overlap policy pass"), *Config.FixtureId), Result.Metrics.bOverlapPolicyPass);
		TestTrue(FString::Printf(TEXT("%s topology pass"), *Config.FixtureId), Result.Metrics.bTopologyRebuildPass);
		TestTrue(FString::Printf(TEXT("%s lifecycle pass"), *Config.FixtureId), Result.Metrics.bLifecycleConservationPass);
		TestTrue(FString::Printf(TEXT("%s no forbidden fallback pass"), *Config.FixtureId), Result.Metrics.bNoForbiddenFallbackPass);
		TestEqual(FString::Printf(TEXT("%s unsupported overlap write attempts"), *Config.FixtureId), Result.Metrics.UnsupportedOverlapWriteAttemptCount, 0);
		TestEqual(FString::Printf(TEXT("%s prior owner reads"), *Config.FixtureId), Result.Metrics.PriorOwnerReadCount, 0);
		TestEqual(FString::Printf(TEXT("%s global owner reads"), *Config.FixtureId), Result.Metrics.GlobalOwnerReadCount, 0);
		TestEqual(FString::Printf(TEXT("%s centroid resolver"), *Config.FixtureId), Result.Metrics.CentroidPrimaryResolutionCount, 0);
		TestEqual(FString::Printf(TEXT("%s random resolver"), *Config.FixtureId), Result.Metrics.RandomPrimaryResolutionCount, 0);
		TestEqual(FString::Printf(TEXT("%s nearest resolver"), *Config.FixtureId), Result.Metrics.NearestPrimaryResolutionCount, 0);
		TestEqual(FString::Printf(TEXT("%s q1/q2 prior owner lookup"), *Config.FixtureId), Result.Metrics.Q1Q2PriorOwnerLookupCount, 0);
		TestEqual(FString::Printf(TEXT("%s q1/q2 discrete approximation"), *Config.FixtureId), Result.Metrics.Q1Q2DiscreteApproxCount, 0);
		TestEqual(FString::Printf(TEXT("%s subduction mutation"), *Config.FixtureId), Result.Metrics.SubductionMutationCount, 0);
		TestEqual(FString::Printf(TEXT("%s collision mutation"), *Config.FixtureId), Result.Metrics.CollisionMutationCount, 0);
		TestEqual(FString::Printf(TEXT("%s rifting mutation"), *Config.FixtureId), Result.Metrics.RiftingMutationCount, 0);
		TestEqual(FString::Printf(TEXT("%s terrain mutation"), *Config.FixtureId), Result.Metrics.TerrainBeautyMutationCount, 0);

		if (Config.FixtureId == TEXT("M2-FX-001"))
		{
			bSawNoop = true;
			TestEqual(TEXT("M2-FX-001 writes every sample"), Result.Metrics.ValidSingleHitWriteCount, Result.Metrics.GlobalSampleCount);
			TestEqual(TEXT("M2-FX-001 rebuilds all triangles"), Result.Metrics.RebuiltTriangleAssignmentCount, Result.Metrics.GlobalTriangleCount);
		}
		else if (Config.FixtureId == TEXT("M2-FX-002"))
		{
			bSawTransfer = true;
			TestTrue(TEXT("M2-FX-002 writes single-source material"), Result.Metrics.ValidSingleHitWriteCount > 0);
		}
		else if (Config.FixtureId == TEXT("M2-FX-003"))
		{
			bSawGapFill = true;
			TestTrue(TEXT("M2-FX-003 creates q1/q2 gap fill"), Result.Metrics.Q1Q2GapFillCount > 0);
			TestEqual(TEXT("M2-FX-003 q1/q2 pairs use different plates"), Result.Metrics.Q1Q2DifferentPlatePairCount, Result.Metrics.Q1Q2GapFillCount);
		}
		else if (Config.FixtureId == TEXT("M2-FX-004"))
		{
			bSawOverlapBlocked = true;
			TestTrue(TEXT("M2-FX-004 blocks nondegenerate overlap"), Result.Metrics.NondegenerateOverlapBlockedCount > 0);
			TestEqual(TEXT("M2-FX-004 does not write unsupported overlap"), Result.Metrics.UnsupportedOverlapWriteAttemptCount, 0);
		}
		else if (Config.FixtureId == TEXT("M2-FX-005"))
		{
			bSawLifecycle = true;
			TestEqual(TEXT("M2-FX-005 runs three lifecycle windows"), Result.Metrics.TopologyRebuildCount, 3);
		}
		else if (Config.FixtureId == TEXT("M2-FX-006"))
		{
			bSawPriorOwnerControl = true;
			TestTrue(TEXT("M2-FX-006 injects prior-owner control"), Config.bInjectPriorOwnerLabelsForNegativeControl);
			TestEqual(TEXT("M2-FX-006 does not read injected prior owner"), Result.Metrics.PriorOwnerReadCount, 0);
		}

		bAllPassed = bAllPassed && Result.Metrics.bFixturePass;
	}

	TestTrue(TEXT("No-op fixture ran"), bSawNoop);
	TestTrue(TEXT("Single-source transfer fixture ran"), bSawTransfer);
	TestTrue(TEXT("Divergent q1/q2 fixture ran"), bSawGapFill);
	TestTrue(TEXT("Overlap-blocked fixture ran"), bSawOverlapBlocked);
	TestTrue(TEXT("Lifecycle fixture ran"), bSawLifecycle);
	TestTrue(TEXT("Prior-owner negative control ran"), bSawPriorOwnerControl);
	TestTrue(TEXT("All M2 carrier cycle fixtures passed"), bAllPassed);
	return true;
}

#endif
