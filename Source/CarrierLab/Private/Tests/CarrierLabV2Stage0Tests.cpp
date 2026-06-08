// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CarrierLabV2Core.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCarrierLabV2Stage0EntryFixturesTest,
	"CarrierLab.V2.Stage0.EntryFixtures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCarrierLabV2Stage0EntryFixturesTest::RunTest(const FString& Parameters)
{
	const TArray<CarrierLab::V2::FCarrierV2Stage0Config> Configs = CarrierLab::V2::FCarrierV2Stage0::MakeMicroFixtureConfigs();
	TestEqual(TEXT("V2-0 has five entry fixtures"), Configs.Num(), 5);

	bool bAllPassed = true;
	for (const CarrierLab::V2::FCarrierV2Stage0Config& Config : Configs)
	{
		CarrierLab::V2::FCarrierV2FixtureResult Result;
		CarrierLab::V2::FCarrierV2Stage0::RunFixtureWithReplay(Config, Result);
		AddInfo(FString::Printf(
			TEXT("[CarrierLabV2Stage0 fixture=%s kind=%s pass=%s observed=%s expected=%s samples=%d tris=%d local_tris=%d raw_hits=%lld nondeg_miss=%d nondeg_overlap=%d boundary=%d topo_hole=%d topo_dup=%d projection_ms=%.3f total_ms=%.3f hash=%s replay_hash=%s]"),
			*Result.Metrics.FixtureId,
			*Result.Metrics.FixtureKind,
			Result.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			*Result.Metrics.ObservedFailureReason,
			*Result.Metrics.ExpectedFailureReason,
			Result.Metrics.GlobalSampleCount,
			Result.Metrics.GlobalTriangleCount,
			Result.Metrics.LocalPlateTriangleCountSum,
			Result.Metrics.RawHitCountTotal,
			Result.Metrics.NonDegenerateMissCount,
			Result.Metrics.NonDegenerateOverlapCount,
			Result.Metrics.BoundaryDegenerateCount,
			Result.Metrics.TopologyHoleErrorCount,
			Result.Metrics.TopologyDuplicateErrorCount,
			Result.Metrics.ProjectionKernelMs,
			Result.Metrics.TotalMs,
			*Result.Metrics.CarrierHash,
			*Result.Metrics.ReplayCarrierHash));

		TestTrue(FString::Printf(TEXT("%s fixture pass"), *Config.FixtureId), Result.Metrics.bFixturePass);
		TestTrue(FString::Printf(TEXT("%s replay deterministic"), *Config.FixtureId), Result.Metrics.bReplayDeterministic);
		TestEqual(FString::Printf(TEXT("%s global owner reads"), *Config.FixtureId), Result.Metrics.ProjectionReadsGlobalOwnerCount, 0);

		if (Config.FixtureKind == CarrierLab::V2::ECarrierV2FixtureKind::Negative)
		{
			TestEqual(
				FString::Printf(TEXT("%s expected negative-control reason"), *Config.FixtureId),
				Result.Metrics.ObservedFailureReason,
				Config.ExpectedFailureReason);
		}
		else
		{
			TestEqual(FString::Printf(TEXT("%s nondegenerate misses"), *Config.FixtureId), Result.Metrics.NonDegenerateMissCount, 0);
			TestEqual(FString::Printf(TEXT("%s nondegenerate overlaps"), *Config.FixtureId), Result.Metrics.NonDegenerateOverlapCount, 0);
			TestEqual(FString::Printf(TEXT("%s topology holes"), *Config.FixtureId), Result.Metrics.TopologyHoleErrorCount, 0);
			TestEqual(FString::Printf(TEXT("%s topology duplicates"), *Config.FixtureId), Result.Metrics.TopologyDuplicateErrorCount, 0);
			TestTrue(
				FString::Printf(TEXT("%s material projection delta"), *Config.FixtureId),
				FMath::IsNearlyZero(Result.Metrics.MaterialAuthorityProjectedDelta, 1.0e-6));
		}

		bAllPassed = bAllPassed && Result.Metrics.bFixturePass;
	}

	TestTrue(TEXT("All V2-0 entry fixtures passed"), bAllPassed);
	return true;
}

#endif
