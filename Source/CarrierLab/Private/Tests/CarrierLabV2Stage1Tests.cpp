// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CarrierLabV2Core.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCarrierLabV2Stage1RigidMotionFixturesTest,
	"CarrierLab.V2.Stage1.RigidMotionFixtures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCarrierLabV2Stage1RigidMotionFixturesTest::RunTest(const FString& Parameters)
{
	const TArray<CarrierLab::V2::FCarrierV2Stage1Config> Configs = CarrierLab::V2::FCarrierV2Stage1::MakeMicroFixtureConfigs();
	TestEqual(TEXT("V2-1 has four rigid-motion fixtures"), Configs.Num(), 4);

	bool bAllPassed = true;
	for (const CarrierLab::V2::FCarrierV2Stage1Config& Config : Configs)
	{
		CarrierLab::V2::FCarrierV2Stage1FixtureResult Result;
		CarrierLab::V2::FCarrierV2Stage1::RunFixtureWithReplay(Config, Result);
		AddInfo(FString::Printf(
			TEXT("[CarrierLabV2Stage1 fixture=%s class=%s pass=%s replay=%s max_error_km=%.12g mean_error_km=%.12g unit_error=%.12g material_errors=%d raw_miss=%d raw_overlap=%d divergent=%d convergent=%d repair=%d remesh=%d policy=%s projection_ms=%.3f total_ms=%.3f post_hash=%s replay_post_hash=%s]"),
			*Result.Metrics.FixtureId,
			*Result.Metrics.ExpectedMotionClass,
			Result.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Result.Metrics.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			Result.Metrics.AnalyticMotionMaxErrorKm,
			Result.Metrics.AnalyticMotionMeanErrorKm,
			Result.Metrics.UnitLengthMaxError,
			Result.Metrics.MaterialAttachmentErrorCount,
			Result.Metrics.RawMotionMissCount,
			Result.Metrics.RawMotionOverlapCount,
			Result.Metrics.DivergentCandidateCount,
			Result.Metrics.ConvergentCandidateCount,
			Result.Metrics.MotionRepairCount,
			Result.Metrics.RemeshDuringMotionCount,
			*Result.Metrics.ProjectionCandidatePolicyId,
			Result.Metrics.ProjectionKernelMs,
			Result.Metrics.TotalMs,
			*Result.Metrics.PostMotionAuthorityHash,
			*Result.Metrics.ReplayPostMotionAuthorityHash));

		TestTrue(FString::Printf(TEXT("%s fixture pass"), *Config.FixtureId), Result.Metrics.bFixturePass);
		TestTrue(FString::Printf(TEXT("%s replay deterministic"), *Config.FixtureId), Result.Metrics.bReplayDeterministic);
		TestTrue(FString::Printf(TEXT("%s motion oracle pass"), *Config.FixtureId), Result.Metrics.bMotionOraclePass);
		TestTrue(FString::Printf(TEXT("%s unit length pass"), *Config.FixtureId), Result.Metrics.bUnitLengthPass);
		TestEqual(FString::Printf(TEXT("%s material attachment errors"), *Config.FixtureId), Result.Metrics.MaterialAttachmentErrorCount, 0);
		TestEqual(FString::Printf(TEXT("%s motion repairs"), *Config.FixtureId), Result.Metrics.MotionRepairCount, 0);
		TestEqual(FString::Printf(TEXT("%s remesh during motion"), *Config.FixtureId), Result.Metrics.RemeshDuringMotionCount, 0);
		TestEqual(FString::Printf(TEXT("%s global owner reads"), *Config.FixtureId), Result.Metrics.ProjectionReadsGlobalOwnerCount, 0);

		if (Config.FixtureId == TEXT("FX-007"))
		{
			TestTrue(TEXT("FX-007 exposes a raw post-motion miss"), Result.Metrics.RawMotionMissCount > 0);
			TestTrue(TEXT("FX-007 classifies divergent candidates"), Result.Metrics.DivergentCandidateCount > 0);
		}
		else if (Config.FixtureId == TEXT("FX-008"))
		{
			TestTrue(TEXT("FX-008 exposes a raw post-motion overlap"), Result.Metrics.RawMotionOverlapCount > 0);
			TestTrue(TEXT("FX-008 classifies convergent candidates"), Result.Metrics.ConvergentCandidateCount > 0);
		}
		else
		{
			TestEqual(FString::Printf(TEXT("%s raw motion misses"), *Config.FixtureId), Result.Metrics.RawMotionMissCount, 0);
			TestEqual(FString::Printf(TEXT("%s raw motion overlaps"), *Config.FixtureId), Result.Metrics.RawMotionOverlapCount, 0);
		}

		bAllPassed = bAllPassed && Result.Metrics.bFixturePass;
	}

	TestTrue(TEXT("All V2-1 rigid-motion fixtures passed"), bAllPassed);
	return true;
}

#endif
