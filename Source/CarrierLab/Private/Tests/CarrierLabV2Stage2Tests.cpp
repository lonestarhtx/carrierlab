// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CarrierLabV2Core.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCarrierLabV2Stage2ContactDryRunFixturesTest,
	"CarrierLab.V2.Stage2.ContactDryRunFixtures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCarrierLabV2Stage2ContactDryRunFixturesTest::RunTest(const FString& Parameters)
{
	const TArray<CarrierLab::V2::FCarrierV2Stage2Config> Configs = CarrierLab::V2::FCarrierV2Stage2::MakeMicroFixtureConfigs();
	TestEqual(TEXT("V2-2 has three contact dry-run fixtures"), Configs.Num(), 3);

	bool bAllPassed = true;
	for (const CarrierLab::V2::FCarrierV2Stage2Config& Config : Configs)
	{
		CarrierLab::V2::FCarrierV2Stage2FixtureResult Result;
		CarrierLab::V2::FCarrierV2Stage2::RunFixtureWithReplay(Config, Result);
		AddInfo(FString::Printf(
			TEXT("[CarrierLabV2Stage2 fixture=%s process=%s pass=%s replay=%s contacts=%d accepted=%d rejected=%d third=%d polarity=%d mutations=%d material_mutations=%d remesh=%d gap_fill=%d resolvers=(%d,%d,%d) owner_label_evidence=%d process_hash=%s replay_process_hash=%s contact_ms=%.3f total_ms=%.3f]"),
			*Result.Metrics.FixtureId,
			*Result.Metrics.ExpectedProcessClass,
			Result.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Result.Metrics.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			Result.Metrics.ContactCandidateCount,
			Result.Metrics.AcceptedConvergenceEvidenceCount,
			Result.Metrics.RejectedContactCount,
			Result.Metrics.ThirdPlateIntrusionCount,
			Result.Metrics.PolarityCandidateCount,
			Result.Metrics.ProcessMutationCount,
			Result.Metrics.MaterialMutationCount,
			Result.Metrics.RemeshDuringProcessDryRunCount,
			Result.Metrics.GapFillDuringProcessDryRunCount,
			Result.Metrics.CentroidPrimaryResolutionCount,
			Result.Metrics.RandomPrimaryResolutionCount,
			Result.Metrics.NearestPrimaryResolutionCount,
			Result.Metrics.ProjectionOwnerLabelEvidenceCount,
			*Result.Metrics.ProcessStateHash,
			*Result.Metrics.ReplayProcessStateHash,
			Result.Metrics.ContactDetectionMs,
			Result.Metrics.TotalMs));

		TestTrue(FString::Printf(TEXT("%s fixture pass"), *Config.FixtureId), Result.Metrics.bFixturePass);
		TestTrue(FString::Printf(TEXT("%s replay deterministic"), *Config.FixtureId), Result.Metrics.bReplayDeterministic);
		TestTrue(FString::Printf(TEXT("%s inherited motion oracle pass"), *Config.FixtureId), Result.Metrics.bMotionOraclePass);
		TestTrue(FString::Printf(TEXT("%s inherited projection expectation pass"), *Config.FixtureId), Result.Metrics.bProjectionExpectationPass);
		TestEqual(FString::Printf(TEXT("%s process mutations"), *Config.FixtureId), Result.Metrics.ProcessMutationCount, 0);
		TestEqual(FString::Printf(TEXT("%s material mutations"), *Config.FixtureId), Result.Metrics.MaterialMutationCount, 0);
		TestEqual(FString::Printf(TEXT("%s process remeshes"), *Config.FixtureId), Result.Metrics.RemeshDuringProcessDryRunCount, 0);
		TestEqual(FString::Printf(TEXT("%s process gap fills"), *Config.FixtureId), Result.Metrics.GapFillDuringProcessDryRunCount, 0);
		TestEqual(FString::Printf(TEXT("%s centroid resolver"), *Config.FixtureId), Result.Metrics.CentroidPrimaryResolutionCount, 0);
		TestEqual(FString::Printf(TEXT("%s random resolver"), *Config.FixtureId), Result.Metrics.RandomPrimaryResolutionCount, 0);
		TestEqual(FString::Printf(TEXT("%s nearest resolver"), *Config.FixtureId), Result.Metrics.NearestPrimaryResolutionCount, 0);
		TestEqual(FString::Printf(TEXT("%s projection owner label evidence"), *Config.FixtureId), Result.Metrics.ProjectionOwnerLabelEvidenceCount, 0);
		TestEqual(FString::Printf(TEXT("%s overlap consumed before process state"), *Config.FixtureId), Result.Metrics.OverlapConsumedBeforeProcessStateCount, 0);

		if (Config.FixtureId == TEXT("FX-007"))
		{
			TestTrue(TEXT("FX-007 keeps divergent miss evidence visible"), Result.Metrics.RawMotionMissCount > 0);
			TestEqual(TEXT("FX-007 has no contact candidates"), Result.Metrics.ContactCandidateCount, 0);
		}
		else if (Config.FixtureId == TEXT("FX-008"))
		{
			TestTrue(TEXT("FX-008 records contact candidates"), Result.Metrics.ContactCandidateCount > 0);
			TestTrue(TEXT("FX-008 records polarity candidates"), Result.Metrics.PolarityCandidateCount > 0);
			TestEqual(TEXT("FX-008 does not require third-plate evidence"), Result.Metrics.ThirdPlateIntrusionCount, 0);
		}
		else if (Config.FixtureId == TEXT("FX-009"))
		{
			TestTrue(TEXT("FX-009 records contact candidates"), Result.Metrics.ContactCandidateCount > 0);
			TestTrue(TEXT("FX-009 records third-plate evidence"), Result.Metrics.ThirdPlateIntrusionCount > 0);
			TestTrue(TEXT("FX-009 records polarity candidates"), Result.Metrics.PolarityCandidateCount > 0);
		}

		bAllPassed = bAllPassed && Result.Metrics.bFixturePass;
	}

	TestTrue(TEXT("All V2-2 contact dry-run fixtures passed"), bAllPassed);
	return true;
}

#endif
