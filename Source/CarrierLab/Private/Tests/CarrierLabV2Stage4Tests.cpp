// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CarrierLabV2Core.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCarrierLabV2Stage4FilteredSamplingFixturesTest,
	"CarrierLab.V2.Stage4.FilteredGlobalSamplingFixtures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCarrierLabV2Stage4FilteredSamplingFixturesTest::RunTest(const FString& Parameters)
{
	const TArray<CarrierLab::V2::FCarrierV2Stage4Config> Configs = CarrierLab::V2::FCarrierV2Stage4::MakeMicroFixtureConfigs();
	TestEqual(TEXT("V2-4 has two filtered sampling fixtures"), Configs.Num(), 2);

	bool bAllPassed = true;
	bool bSawSubductionFixture = false;
	bool bSawCollisionFixture = false;

	for (const CarrierLab::V2::FCarrierV2Stage4Config& Config : Configs)
	{
		CarrierLab::V2::FCarrierV2Stage4FixtureResult Result;
		CarrierLab::V2::FCarrierV2Stage4::RunFixtureWithReplay(Config, Result);
		AddInfo(FString::Printf(
			TEXT("[CarrierLabV2Stage4 fixture=%s remesh=%s pass=%s replay=%s inherited=%s raw_hits=%lld filtered_sub=%d filtered_coll=%d valid=%d zero=%d deferred=%d unresolved=%d boundary_only=%d selected_filtered=%d forbidden(owner=%d centroid=%d random=%d nearest=%d repair=%d retain=%d q1q2_prior=%d q1q2_discrete=%d topo=%d reset=%d terrain=%d) sampling_hash=%s replay_sampling_hash=%s sampling_ms=%.3f total_ms=%.3f]"),
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
			Result.Metrics.GapFillDeferredCount,
			Result.Metrics.PostFilterUnresolvedMultihitCount,
			Result.Metrics.PostFilterBoundaryOnlyMultihitCount,
			Result.Metrics.SelectedFilteredHitCount,
			Result.Metrics.PriorOwnerFallbackCount,
			Result.Metrics.CentroidPrimaryResolutionCount,
			Result.Metrics.RandomPrimaryResolutionCount,
			Result.Metrics.NearestPrimaryResolutionCount,
			Result.Metrics.OwnershipRepairDuringRemeshCount,
			Result.Metrics.RetentionHysteresisAnchorCount,
			Result.Metrics.Q1Q2PriorOwnerLookupCount,
			Result.Metrics.Q1Q2DiscreteApproxCount,
			Result.Metrics.TopologyRebuildDuringSamplingCount,
			Result.Metrics.ProcessStateResetCount,
			Result.Metrics.TerrainBeautyMutationCount,
			*Result.Metrics.GlobalSamplingHash,
			*Result.Metrics.ReplayGlobalSamplingHash,
			Result.Metrics.GlobalSamplingMs,
			Result.Metrics.TotalMs));

		TestTrue(FString::Printf(TEXT("%s fixture pass"), *Config.FixtureId), Result.Metrics.bFixturePass);
		TestTrue(FString::Printf(TEXT("%s replay deterministic"), *Config.FixtureId), Result.Metrics.bReplayDeterministic);
		TestTrue(FString::Printf(TEXT("%s inherited V2-3 pass"), *Config.FixtureId), Result.Metrics.bInheritedStage3Pass);
		TestTrue(FString::Printf(TEXT("%s source filter pass"), *Config.FixtureId), Result.Metrics.bSourceFilterPass);
		TestTrue(FString::Printf(TEXT("%s valid selection pass"), *Config.FixtureId), Result.Metrics.bValidSelectionPass);
		TestTrue(FString::Printf(TEXT("%s deferred gap ledger pass"), *Config.FixtureId), Result.Metrics.bDeferredGapFillPass);
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
		TestEqual(FString::Printf(TEXT("%s generated oceanic"), *Config.FixtureId), Result.Metrics.GeneratedOceanicCount, 0);
		TestEqual(FString::Printf(TEXT("%s topology rebuild during sampling"), *Config.FixtureId), Result.Metrics.TopologyRebuildDuringSamplingCount, 0);
		TestEqual(FString::Printf(TEXT("%s process state reset"), *Config.FixtureId), Result.Metrics.ProcessStateResetCount, 0);
		TestEqual(FString::Printf(TEXT("%s terrain beauty mutation"), *Config.FixtureId), Result.Metrics.TerrainBeautyMutationCount, 0);
		TestEqual(FString::Printf(TEXT("%s deferred gaps match zero valid"), *Config.FixtureId), Result.Metrics.GapFillDeferredCount, Result.Metrics.ZeroValidHitCount);
		TestTrue(FString::Printf(TEXT("%s has valid hits"), *Config.FixtureId), Result.Metrics.ValidHitAfterFilterCount > 0);

		if (Config.FixtureId == TEXT("FX-012"))
		{
			bSawSubductionFixture = true;
			TestTrue(TEXT("FX-012 filters subducting hits"), Result.Metrics.FilteredSubductingHitCount > 0);
			TestEqual(TEXT("FX-012 does not require collision filtering"), Result.Metrics.FilteredCollidingHitCount, 0);
		}
		else if (Config.FixtureId == TEXT("FX-013"))
		{
			bSawCollisionFixture = true;
			TestTrue(TEXT("FX-013 filters colliding hits"), Result.Metrics.FilteredCollidingHitCount > 0);
			TestTrue(TEXT("FX-013 records q1/q2-deferred gap candidates"), Result.Metrics.GapFillDeferredCount > 0);
		}

		bAllPassed = bAllPassed && Result.Metrics.bFixturePass;
	}

	TestTrue(TEXT("FX-012 subduction-filter fixture ran"), bSawSubductionFixture);
	TestTrue(TEXT("FX-013 collision-filter fixture ran"), bSawCollisionFixture);
	TestTrue(TEXT("All V2-4 filtered global sampling fixtures passed"), bAllPassed);
	return true;
}

#endif
