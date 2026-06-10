// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CarrierLabV2Core.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCarrierLabV2Stage3ProcessMutationFixturesTest,
	"CarrierLab.V2.Stage3.ProcessMutationFixtures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCarrierLabV2Stage3ProcessMutationFixturesTest::RunTest(const FString& Parameters)
{
	const TArray<CarrierLab::V2::FCarrierV2Stage3Config> Configs = CarrierLab::V2::FCarrierV2Stage3::MakeMicroFixtureConfigs();
	TestEqual(TEXT("V2-3 has two process mutation fixtures"), Configs.Num(), 2);

	bool bAllPassed = true;
	for (const CarrierLab::V2::FCarrierV2Stage3Config& Config : Configs)
	{
		CarrierLab::V2::FCarrierV2Stage3FixtureResult Result;
		CarrierLab::V2::FCarrierV2Stage3::RunFixtureWithReplay(Config, Result);
		AddInfo(FString::Printf(
			TEXT("[CarrierLabV2Stage3 fixture=%s process=%s pass=%s replay=%s contacts=%d events=%d provenance=%d/%d subduction=%d marks=%d age_decisions=%d older_subducted=%d collision=%d detach=%d suture=%d forbidden(material=%d remesh=%d rebuild=%d gap_fill=%d divergent=%d terrain=%d repair=%d resolvers=%d/%d/%d) process_hash=%s replay_process_hash=%s process_ms=%.3f total_ms=%.3f]"),
			*Result.Metrics.FixtureId,
			*Result.Metrics.ExpectedProcessClass,
			Result.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Result.Metrics.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			Result.Metrics.ContactCandidateCount,
			Result.Metrics.ProcessEventCount,
			Result.Metrics.ProcessEventWithProvenanceCount,
			Result.Metrics.ProcessEventWithoutProvenanceCount,
			Result.Metrics.SubductionEventCount,
			Result.Metrics.SubductingTriangleMarkCount,
			Result.Metrics.OceanicAgePolarityDecisionCount,
			Result.Metrics.OlderOceanicSubductingPassCount,
			Result.Metrics.CollisionEventCount,
			Result.Metrics.TerraneDetachTriangleCount,
			Result.Metrics.TerraneSutureTriangleCount,
			Result.Metrics.MaterialMutationCount,
			Result.Metrics.RemeshDuringProcessMutationCount,
			Result.Metrics.TopologyRebuildDuringProcessMutationCount,
			Result.Metrics.GapFillDuringProcessMutationCount,
			Result.Metrics.DivergentGenerationDuringProcessMutationCount,
			Result.Metrics.TerrainBeautyMutationCount,
			Result.Metrics.OwnershipRepairDuringProcessMutationCount,
			Result.Metrics.CentroidPrimaryResolutionCount,
			Result.Metrics.RandomPrimaryResolutionCount,
			Result.Metrics.NearestPrimaryResolutionCount,
			*Result.Metrics.ProcessStateHash,
			*Result.Metrics.ReplayProcessStateHash,
			Result.Metrics.ProcessMutationMs,
			Result.Metrics.TotalMs));

		TestTrue(FString::Printf(TEXT("%s fixture pass"), *Config.FixtureId), Result.Metrics.bFixturePass);
		TestTrue(FString::Printf(TEXT("%s replay deterministic"), *Config.FixtureId), Result.Metrics.bReplayDeterministic);
		TestTrue(FString::Printf(TEXT("%s inherited V2-2 pass"), *Config.FixtureId), Result.Metrics.bInheritedStage2Pass);
		TestTrue(FString::Printf(TEXT("%s process event provenance pass"), *Config.FixtureId), Result.Metrics.bProcessEventProvenancePass);
		TestTrue(FString::Printf(TEXT("%s no forbidden mutation pass"), *Config.FixtureId), Result.Metrics.bNoForbiddenMutationPass);
		TestEqual(FString::Printf(TEXT("%s material mutations"), *Config.FixtureId), Result.Metrics.MaterialMutationCount, 0);
		TestEqual(FString::Printf(TEXT("%s material destroyed without process"), *Config.FixtureId), Result.Metrics.MaterialDestroyedWithoutProcessCount, 0);
		TestEqual(FString::Printf(TEXT("%s material created without process"), *Config.FixtureId), Result.Metrics.MaterialCreatedWithoutProcessCount, 0);
		TestEqual(FString::Printf(TEXT("%s topology mutation without event"), *Config.FixtureId), Result.Metrics.TopologyMutationWithoutEventCount, 0);
		TestEqual(FString::Printf(TEXT("%s remesh during process"), *Config.FixtureId), Result.Metrics.RemeshDuringProcessMutationCount, 0);
		TestEqual(FString::Printf(TEXT("%s topology rebuild during process"), *Config.FixtureId), Result.Metrics.TopologyRebuildDuringProcessMutationCount, 0);
		TestEqual(FString::Printf(TEXT("%s divergent generation during process"), *Config.FixtureId), Result.Metrics.DivergentGenerationDuringProcessMutationCount, 0);
		TestEqual(FString::Printf(TEXT("%s terrain beauty mutation"), *Config.FixtureId), Result.Metrics.TerrainBeautyMutationCount, 0);
		TestEqual(FString::Printf(TEXT("%s ownership repair during process"), *Config.FixtureId), Result.Metrics.OwnershipRepairDuringProcessMutationCount, 0);
		TestEqual(FString::Printf(TEXT("%s centroid resolver"), *Config.FixtureId), Result.Metrics.CentroidPrimaryResolutionCount, 0);
		TestEqual(FString::Printf(TEXT("%s random resolver"), *Config.FixtureId), Result.Metrics.RandomPrimaryResolutionCount, 0);
		TestEqual(FString::Printf(TEXT("%s nearest resolver"), *Config.FixtureId), Result.Metrics.NearestPrimaryResolutionCount, 0);
		TestFalse(FString::Printf(TEXT("%s slab pull disabled"), *Config.FixtureId), Result.Metrics.bSlabPullEnabled);
		TestEqual(FString::Printf(TEXT("%s slab pull axis delta"), *Config.FixtureId), Result.Metrics.SlabPullAxisDeltaDeg, 0.0);

		if (Config.FixtureId == TEXT("FX-010"))
		{
			TestTrue(TEXT("FX-010 records subduction events"), Result.Metrics.SubductionEventCount > 0);
			TestTrue(TEXT("FX-010 records subducting marks"), Result.Metrics.SubductingTriangleMarkCount > 0);
			TestTrue(TEXT("FX-010 has oceanic age polarity decisions"), Result.Metrics.OceanicAgePolarityDecisionCount > 0);
			TestEqual(TEXT("FX-010 older oceanic always subducts"), Result.Metrics.OlderOceanicSubductingPassCount, Result.Metrics.OceanicAgePolarityDecisionCount);
			TestTrue(TEXT("FX-010 first event exists"), Result.ProcessEvents.Num() > 0);
			if (Result.ProcessEvents.Num() > 0)
			{
				TestEqual(TEXT("FX-010 older plate 0 is subducting"), Result.ProcessEvents[0].SubductingPlateId, 0);
			}
			TestEqual(TEXT("FX-010 does not record collision events"), Result.Metrics.CollisionEventCount, 0);
		}
		else if (Config.FixtureId == TEXT("FX-011"))
		{
			TestTrue(TEXT("FX-011 records collision candidates"), Result.Metrics.CollisionCandidateCount > 0);
			TestTrue(TEXT("FX-011 records collision events"), Result.Metrics.CollisionEventCount > 0);
			TestTrue(TEXT("FX-011 records suture marks"), Result.Metrics.TerraneSutureTriangleCount > 0);
			TestEqual(TEXT("FX-011 does not record subduction events"), Result.Metrics.SubductionEventCount, 0);
			TestEqual(TEXT("FX-011 does not record subducting marks"), Result.Metrics.SubductingTriangleMarkCount, 0);
		}

		bAllPassed = bAllPassed && Result.Metrics.bFixturePass;
	}

	bool bThirdPlateRegressionPassed = false;
	for (const CarrierLab::V2::FCarrierV2Stage2Config& Stage2Config : CarrierLab::V2::FCarrierV2Stage2::MakeMicroFixtureConfigs())
	{
		if (Stage2Config.FixtureId != TEXT("FX-009"))
		{
			continue;
		}
		CarrierLab::V2::FCarrierV2Stage2FixtureResult ThirdPlateResult;
		CarrierLab::V2::FCarrierV2Stage2::RunFixtureWithReplay(Stage2Config, ThirdPlateResult);
		AddInfo(FString::Printf(
			TEXT("[CarrierLabV2Stage3 regression=FX-009 pass=%s replay=%s contacts=%d third=%d polarity=%d mutations=%d process_hash=%s]"),
			ThirdPlateResult.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			ThirdPlateResult.Metrics.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			ThirdPlateResult.Metrics.ContactCandidateCount,
			ThirdPlateResult.Metrics.ThirdPlateIntrusionCount,
			ThirdPlateResult.Metrics.PolarityCandidateCount,
			ThirdPlateResult.Metrics.ProcessMutationCount,
			*ThirdPlateResult.Metrics.ProcessStateHash));
		TestTrue(TEXT("FX-009 third-plate regression fixture pass"), ThirdPlateResult.Metrics.bFixturePass);
		TestTrue(TEXT("FX-009 third-plate visibility remains visible"), ThirdPlateResult.Metrics.ThirdPlateIntrusionCount > 0);
		TestEqual(TEXT("FX-009 remains dry-run only"), ThirdPlateResult.Metrics.ProcessMutationCount, 0);
		bThirdPlateRegressionPassed = ThirdPlateResult.Metrics.bFixturePass && ThirdPlateResult.Metrics.ThirdPlateIntrusionCount > 0;
		break;
	}

	TestTrue(TEXT("All V2-3 process mutation fixtures passed"), bAllPassed);
	TestTrue(TEXT("V2-2 third-plate regression remains intact"), bThirdPlateRegressionPassed);
	return true;
}

#endif
