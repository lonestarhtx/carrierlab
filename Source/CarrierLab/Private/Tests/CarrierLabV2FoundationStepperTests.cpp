// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CarrierLabV2Core.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCarrierLabV2FoundationStepperSnapshotTest,
	"CarrierLab.V2.FoundationStepper.Snapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCarrierLabV2FoundationStepperSnapshotTest::RunTest(const FString& Parameters)
{
	const TArray<CarrierLab::V2::FCarrierV2Stage5Config> Configs = CarrierLab::V2::FCarrierV2Stage5::MakeMicroFixtureConfigs();
	TestEqual(TEXT("foundation stepper uses the two V2-5 micro fixtures"), Configs.Num(), 2);

	bool bSawFX014 = false;
	bool bSawFX015 = false;

	for (const CarrierLab::V2::FCarrierV2Stage5Config& Config : Configs)
	{
		CarrierLab::V2::FCarrierV2FoundationStepperSnapshot Snapshot;
		const bool bBuilt = CarrierLab::V2::FCarrierV2FoundationStepper::BuildSnapshot(Config, Snapshot);
		const CarrierLab::V2::FCarrierV2Stage5Metrics& Metrics = Snapshot.Stage5Result.Metrics;

		AddInfo(FString::Printf(
			TEXT("[FoundationStepper fixture=%s built=%s pass=%s replay=%s samples=%d/%d triangles=%d/%d zero=%d generated=%d rebuilt=%d/%d post_marks=%d forbidden=%s summary=%s]"),
			*Config.FixtureId,
			bBuilt ? TEXT("true") : TEXT("false"),
			Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Metrics.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			Snapshot.Samples.Num(),
			Metrics.GlobalSampleCount,
			Snapshot.Triangles.Num(),
			Metrics.GlobalTriangleCount,
			Metrics.ZeroValidHitCount,
			Metrics.GeneratedOceanicCount,
			Metrics.RebuiltTriangleAssignmentCount,
			Metrics.GlobalTriangleCount,
			Metrics.PostResetTriangleMarkCount,
			Snapshot.bForbiddenFallbackDetected ? TEXT("true") : TEXT("false"),
			*Snapshot.Summary));

		TestTrue(FString::Printf(TEXT("%s snapshot built"), *Config.FixtureId), bBuilt);
		TestTrue(FString::Printf(TEXT("%s snapshot completed"), *Config.FixtureId), Snapshot.bCompleted);
		TestTrue(FString::Printf(TEXT("%s fixture pass"), *Config.FixtureId), Snapshot.bFixturePass);
		TestTrue(FString::Printf(TEXT("%s replay deterministic"), *Config.FixtureId), Snapshot.bReplayDeterministic);
		TestFalse(FString::Printf(TEXT("%s no forbidden fallback surfaced"), *Config.FixtureId), Snapshot.bForbiddenFallbackDetected);
		TestEqual(FString::Printf(TEXT("%s visual samples"), *Config.FixtureId), Snapshot.Samples.Num(), Metrics.GlobalSampleCount);
		TestEqual(FString::Printf(TEXT("%s visual triangles"), *Config.FixtureId), Snapshot.Triangles.Num(), Metrics.GlobalTriangleCount);
		TestEqual(FString::Printf(TEXT("%s post-reset marks"), *Config.FixtureId), Metrics.PostResetTriangleMarkCount, 0);

		int32 GeneratedOceanicVisualCount = 0;
		int32 ZeroValidVisualCount = 0;
		int32 PostResetSampleMarkCount = 0;
		for (const CarrierLab::V2::FCarrierV2FoundationStepperSampleVisual& Sample : Snapshot.Samples)
		{
			if (Sample.bGeneratedOceanic)
			{
				++GeneratedOceanicVisualCount;
			}
			if (Sample.bZeroValidHit)
			{
				++ZeroValidVisualCount;
			}
			if (Sample.bPostResetProcessMarked)
			{
				++PostResetSampleMarkCount;
			}
		}

		int32 RebuiltAssignedTriangleVisualCount = 0;
		int32 UnassignedTriangleVisualCount = 0;
		int32 ProcessMarkedTriangleVisualCount = 0;
		int32 PostResetTriangleMarkVisualCount = 0;
		for (const CarrierLab::V2::FCarrierV2FoundationStepperTriangleVisual& Triangle : Snapshot.Triangles)
		{
			if (!Triangle.bUnassignedAfterRebuild)
			{
				++RebuiltAssignedTriangleVisualCount;
			}
			else
			{
				++UnassignedTriangleVisualCount;
			}
			if (Triangle.bProcessMarked)
			{
				++ProcessMarkedTriangleVisualCount;
			}
			if (Triangle.bPostResetProcessMarked)
			{
				++PostResetTriangleMarkVisualCount;
			}
		}

		TestEqual(FString::Printf(TEXT("%s visual generated oceanic samples"), *Config.FixtureId), GeneratedOceanicVisualCount, Metrics.GeneratedOceanicCount);
		TestEqual(FString::Printf(TEXT("%s visual zero-valid samples"), *Config.FixtureId), ZeroValidVisualCount, Metrics.ZeroValidHitCount);
		TestEqual(FString::Printf(TEXT("%s visual rebuilt triangle assignments"), *Config.FixtureId), RebuiltAssignedTriangleVisualCount, Metrics.RebuiltTriangleAssignmentCount);
		TestEqual(FString::Printf(TEXT("%s visual unassigned triangles"), *Config.FixtureId), UnassignedTriangleVisualCount, Metrics.UnassignedTriangleCount);
		TestEqual(FString::Printf(TEXT("%s visual post-reset sample marks"), *Config.FixtureId), PostResetSampleMarkCount, 0);
		TestEqual(FString::Printf(TEXT("%s visual post-reset triangle marks"), *Config.FixtureId), PostResetTriangleMarkVisualCount, 0);
		TestTrue(FString::Printf(TEXT("%s exposes pre-reset process marks"), *Config.FixtureId), Metrics.PreResetTriangleMarkCount == 0 || ProcessMarkedTriangleVisualCount > 0);

		if (Config.FixtureId == TEXT("FX-014"))
		{
			bSawFX014 = true;
		}
		else if (Config.FixtureId == TEXT("FX-015"))
		{
			bSawFX015 = true;
			TestTrue(TEXT("FX-015 snapshot shows q1/q2 oceanic generation"), GeneratedOceanicVisualCount > 0);
		}
	}

	TestTrue(TEXT("foundation stepper checked FX-014"), bSawFX014);
	TestTrue(TEXT("foundation stepper checked FX-015"), bSawFX015);
	return true;
}

#endif
