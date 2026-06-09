// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CarrierLabV2Core.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCarrierLabV2Stage5Q1Q2RebuildFixturesTest,
	"CarrierLab.V2.Stage5.Q1Q2GapFillRebuildFixtures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCarrierLabV2Stage5Q1Q2RebuildFixturesTest::RunTest(const FString& Parameters)
{
	const TArray<CarrierLab::V2::FCarrierV2Stage5Config> Configs = CarrierLab::V2::FCarrierV2Stage5::MakeMicroFixtureConfigs();
	TestEqual(TEXT("V2-5 has two q1/q2 rebuild fixtures"), Configs.Num(), 2);

	bool bAllPassed = true;
	bool bSawRebuildOnlyFixture = false;
	bool bSawGapFillFixture = false;

	for (const CarrierLab::V2::FCarrierV2Stage5Config& Config : Configs)
	{
		CarrierLab::V2::FCarrierV2Stage5FixtureResult Result;
		CarrierLab::V2::FCarrierV2Stage5::RunFixtureWithReplay(Config, Result);
		AddInfo(FString::Printf(
			TEXT("[CarrierLabV2Stage5 fixture=%s remesh=%s pass=%s replay=%s inherited=%s raw_hits=%lld filtered_sub=%d filtered_coll=%d valid=%d zero=%d q1q2_queries=%d q1q2_pairs=%d q1q2_diff=%d qgamma=%d generated=%d no_pair=%d rebuilt_triangles=%d/%d rebuilt_vertices=%d mixed=%d reset=%d post_marks=%d forbidden(owner=%d centroid=%d random=%d nearest=%d repair=%d retain=%d q1q2_prior=%d q1q2_discrete=%d material_without_gap=%d terrain=%d) sampling_hash=%s replay_sampling_hash=%s topology_hash=%s replay_topology_hash=%s sampling_ms=%.3f rebuild_ms=%.3f total_ms=%.3f]"),
			*Result.Metrics.FixtureId,
			*Result.Metrics.ExpectedRemeshClass,
			Result.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Result.Metrics.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			Result.Metrics.bInheritedStage3Pass ? TEXT("true") : TEXT("false"),
			Result.Metrics.RawHitCountTotal,
			Result.Metrics.FilteredSubductingHitCount,
			Result.Metrics.FilteredCollidingHitCount,
			Result.Metrics.ValidHitAfterFilterCount,
			Result.Metrics.ZeroValidHitCount,
			Result.Metrics.Q1Q2BoundaryQueryCount,
			Result.Metrics.Q1Q2BoundaryPairCount,
			Result.Metrics.Q1Q2DifferentPlatePairCount,
			Result.Metrics.QGammaComputedCount,
			Result.Metrics.GeneratedOceanicCount,
			Result.Metrics.GapFillNoBoundaryPairCount,
			Result.Metrics.RebuiltTriangleAssignmentCount,
			Result.Metrics.GlobalTriangleCount,
			Result.Metrics.RebuiltLocalVertexCountSum,
			Result.Metrics.MixedVertexTriangleCount,
			Result.Metrics.ProcessStateResetCount,
			Result.Metrics.PostResetTriangleMarkCount,
			Result.Metrics.PriorOwnerFallbackCount,
			Result.Metrics.CentroidPrimaryResolutionCount,
			Result.Metrics.RandomPrimaryResolutionCount,
			Result.Metrics.NearestPrimaryResolutionCount,
			Result.Metrics.OwnershipRepairDuringRemeshCount,
			Result.Metrics.RetentionHysteresisAnchorCount,
			Result.Metrics.Q1Q2PriorOwnerLookupCount,
			Result.Metrics.Q1Q2DiscreteApproxCount,
			Result.Metrics.MaterialCreatedWithoutGapFillCount,
			Result.Metrics.TerrainBeautyMutationCount,
			*Result.Metrics.GlobalSamplingHash,
			*Result.Metrics.ReplayGlobalSamplingHash,
			*Result.Metrics.RebuiltTopologyHash,
			*Result.Metrics.ReplayRebuiltTopologyHash,
			Result.Metrics.GlobalSamplingMs,
			Result.Metrics.TopologyRebuildMs,
			Result.Metrics.TotalMs));

		TestTrue(FString::Printf(TEXT("%s fixture pass"), *Config.FixtureId), Result.Metrics.bFixturePass);
		TestTrue(FString::Printf(TEXT("%s replay deterministic"), *Config.FixtureId), Result.Metrics.bReplayDeterministic);
		TestTrue(FString::Printf(TEXT("%s inherited V2-3 pass"), *Config.FixtureId), Result.Metrics.bInheritedStage3Pass);
		TestTrue(FString::Printf(TEXT("%s source filter pass"), *Config.FixtureId), Result.Metrics.bSourceFilterPass);
		TestTrue(FString::Printf(TEXT("%s q1/q2 gap fill pass"), *Config.FixtureId), Result.Metrics.bQ1Q2GapFillPass);
		TestTrue(FString::Printf(TEXT("%s topology rebuild pass"), *Config.FixtureId), Result.Metrics.bTopologyRebuildPass);
		TestTrue(FString::Printf(TEXT("%s process reset pass"), *Config.FixtureId), Result.Metrics.bProcessResetPass);
		TestTrue(FString::Printf(TEXT("%s no forbidden fallback pass"), *Config.FixtureId), Result.Metrics.bNoForbiddenFallbackPass);
		TestEqual(FString::Printf(TEXT("%s selected filtered hits"), *Config.FixtureId), Result.Metrics.SelectedFilteredHitCount, 0);
		TestEqual(FString::Printf(TEXT("%s unresolved post-filter multihits"), *Config.FixtureId), Result.Metrics.PostFilterUnresolvedMultihitCount, 0);
		TestEqual(FString::Printf(TEXT("%s prior owner fallback"), *Config.FixtureId), Result.Metrics.PriorOwnerFallbackCount, 0);
		TestEqual(FString::Printf(TEXT("%s centroid resolver"), *Config.FixtureId), Result.Metrics.CentroidPrimaryResolutionCount, 0);
		TestEqual(FString::Printf(TEXT("%s random resolver"), *Config.FixtureId), Result.Metrics.RandomPrimaryResolutionCount, 0);
		TestEqual(FString::Printf(TEXT("%s nearest resolver"), *Config.FixtureId), Result.Metrics.NearestPrimaryResolutionCount, 0);
		TestEqual(FString::Printf(TEXT("%s ownership repair"), *Config.FixtureId), Result.Metrics.OwnershipRepairDuringRemeshCount, 0);
		TestEqual(FString::Printf(TEXT("%s retention/hysteresis/anchor"), *Config.FixtureId), Result.Metrics.RetentionHysteresisAnchorCount, 0);
		TestEqual(FString::Printf(TEXT("%s q1/q2 prior owner lookup"), *Config.FixtureId), Result.Metrics.Q1Q2PriorOwnerLookupCount, 0);
		TestEqual(FString::Printf(TEXT("%s q1/q2 discrete approximation"), *Config.FixtureId), Result.Metrics.Q1Q2DiscreteApproxCount, 0);
		TestEqual(FString::Printf(TEXT("%s material without gap fill"), *Config.FixtureId), Result.Metrics.MaterialCreatedWithoutGapFillCount, 0);
		TestEqual(FString::Printf(TEXT("%s terrain beauty mutation"), *Config.FixtureId), Result.Metrics.TerrainBeautyMutationCount, 0);
		TestEqual(FString::Printf(TEXT("%s topology rebuild count"), *Config.FixtureId), Result.Metrics.TopologyRebuildCount, 1);
		TestEqual(FString::Printf(TEXT("%s process state reset"), *Config.FixtureId), Result.Metrics.ProcessStateResetCount, 1);
		TestEqual(FString::Printf(TEXT("%s post-reset marks"), *Config.FixtureId), Result.Metrics.PostResetTriangleMarkCount, 0);
		TestEqual(FString::Printf(TEXT("%s unassigned triangles"), *Config.FixtureId), Result.Metrics.UnassignedTriangleCount, 0);
		TestEqual(FString::Printf(TEXT("%s rebuilt triangle assignments"), *Config.FixtureId), Result.Metrics.RebuiltTriangleAssignmentCount, Result.Metrics.GlobalTriangleCount);
		TestEqual(FString::Printf(TEXT("%s rebuilt local triangles"), *Config.FixtureId), Result.Metrics.RebuiltLocalTriangleCountSum, Result.Metrics.GlobalTriangleCount);
		TestEqual(FString::Printf(TEXT("%s generated oceanic matches zero-valid samples"), *Config.FixtureId), Result.Metrics.GeneratedOceanicCount, Result.Metrics.ZeroValidHitCount);
		TestEqual(FString::Printf(TEXT("%s q1/q2 pairs match zero-valid samples"), *Config.FixtureId), Result.Metrics.Q1Q2BoundaryPairCount, Result.Metrics.ZeroValidHitCount);
		TestEqual(FString::Printf(TEXT("%s qGamma records match zero-valid samples"), *Config.FixtureId), Result.Metrics.QGammaComputedCount, Result.Metrics.ZeroValidHitCount);
		TestEqual(FString::Printf(TEXT("%s no missing q1/q2 pair fallback"), *Config.FixtureId), Result.Metrics.GapFillNoBoundaryPairCount, 0);

		if (Config.FixtureId == TEXT("FX-014"))
		{
			bSawRebuildOnlyFixture = true;
			TestTrue(TEXT("FX-014 filters subducting hits"), Result.Metrics.FilteredSubductingHitCount > 0);
			TestFalse(TEXT("FX-014 does not require generated oceanic"), Config.bRequireGeneratedOceanic);
		}
		else if (Config.FixtureId == TEXT("FX-015"))
		{
			bSawGapFillFixture = true;
			TestTrue(TEXT("FX-015 filters colliding hits"), Result.Metrics.FilteredCollidingHitCount > 0);
			TestTrue(TEXT("FX-015 creates oceanic material through q1/q2"), Result.Metrics.GeneratedOceanicCount > 0);
			TestTrue(TEXT("FX-015 records continuous q1/q2 pairs"), Result.Metrics.Q1Q2BoundaryPairCount > 0);
			TestEqual(TEXT("FX-015 q1/q2 pairs use different plates"), Result.Metrics.Q1Q2DifferentPlatePairCount, Result.Metrics.GeneratedOceanicCount);
		}

		bAllPassed = bAllPassed && Result.Metrics.bFixturePass;
	}

	TestTrue(TEXT("FX-014 rebuild/reset fixture ran"), bSawRebuildOnlyFixture);
	TestTrue(TEXT("FX-015 q1/q2 gap-fill fixture ran"), bSawGapFillFixture);
	TestTrue(TEXT("All V2-5 q1/q2 gap-fill rebuild fixtures passed"), bAllPassed);
	return true;
}

#endif
