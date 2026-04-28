// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_DEV_AUTOMATION_TESTS

#include "CarrierLabCarrier.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCarrierLabStage0ColdStartSmokeTest,
	"CarrierLab.Stage0.ColdStartSmoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCarrierLabStage0ColdStartSmokeTest::RunTest(const FString& Parameters)
{
	CarrierLab::FStage0Config Config;
	Config.SampleCount = 512;
	Config.PlateCount = 12;
	Config.Seed = 42;
	Config.ContinentalPlateFraction = 0.30;

	CarrierLab::FCarrierState StateA;
	FString ErrorA;
	TestTrue(TEXT("Cold-start carrier builds"), CarrierLab::FCarrierLabStage0::BuildColdStartCarrier(Config, StateA, ErrorA));
	if (!ErrorA.IsEmpty())
	{
		AddInfo(ErrorA);
	}

	CarrierLab::FCarrierState StateB;
	FString ErrorB;
	TestTrue(TEXT("Cold-start carrier rebuilds for determinism"), CarrierLab::FCarrierLabStage0::BuildColdStartCarrier(Config, StateB, ErrorB));
	if (!ErrorB.IsEmpty())
	{
		AddInfo(ErrorB);
	}

	const CarrierLab::FStage0Metrics MetricsA = CarrierLab::FCarrierLabStage0::ProjectColdStart(StateA);
	const CarrierLab::FStage0Metrics MetricsB = CarrierLab::FCarrierLabStage0::ProjectColdStart(StateB);

	TestEqual(TEXT("All samples have raw hits"), MetricsA.RawHitCount, Config.SampleCount);
	TestEqual(TEXT("No raw misses"), MetricsA.RawMissCount, 0);
	TestEqual(TEXT("No non-degenerate misses"), MetricsA.NonDegenerateMissCount, 0);
	TestEqual(TEXT("No non-degenerate overlaps"), MetricsA.NonDegenerateOverlapCount, 0);
	TestEqual(TEXT("No NaN/Inf samples"), MetricsA.NaNOrInfCount, 0);
	TestEqual(TEXT("Euler characteristic remains spherical"), MetricsA.EulerCharacteristic, 2);
	TestEqual(TEXT("Plate-local triangle copy covers global TDS"), MetricsA.PlateLocalTriangleCount, MetricsA.TriangleCount);
	TestTrue(TEXT("Ray-triangle projection executed"), MetricsA.RayTriangleTestCount > 0);
	TestTrue(TEXT("Boundary degeneracy is tracked separately"), MetricsA.BoundaryDegenerateOverlapCount >= 0);
	TestEqual(TEXT("Deterministic hash is stable"), MetricsA.DeterminismHash, MetricsB.DeterminismHash);
	TestTrue(TEXT("Projected CAF matches authoritative CAF"),
		FMath::IsNearlyEqual(MetricsA.AuthoritativeContinentalAreaFraction, MetricsA.ProjectedContinentalAreaFraction, 1.0e-12));

	AddInfo(FString::Printf(
		TEXT("[CarrierLabStage0 sample_count=%d plate_count=%d tris=%d local_tris=%d local_vertices=%d edges=%d euler=%d ray_tests=%lld hit=%d miss=%d multi=%d boundary_degenerate=%d third_plate_intrusion=%d caf=%.6f hash=%s]"),
		MetricsA.SampleCount,
		MetricsA.PlateCount,
		MetricsA.TriangleCount,
		MetricsA.PlateLocalTriangleCount,
		MetricsA.PlateLocalVertexCount,
		MetricsA.EdgeCount,
		MetricsA.EulerCharacteristic,
		MetricsA.RayTriangleTestCount,
		MetricsA.RawHitCount,
		MetricsA.RawMissCount,
		MetricsA.RawMultiHitCount,
		MetricsA.BoundaryDegenerateOverlapCount,
		MetricsA.ThirdPlateIntrusionCount,
		MetricsA.AuthoritativeContinentalAreaFraction,
		*MetricsA.DeterminismHash));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FCarrierLabStage0ColdStartDeterminismTest,
	"CarrierLab.Stage0.ColdStartDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCarrierLabStage0ColdStartDeterminismTest::RunTest(const FString& Parameters)
{
	CarrierLab::FStage0Config Config;
	Config.SampleCount = 1024;
	Config.PlateCount = 40;
	Config.Seed = 42;
	Config.ContinentalPlateFraction = 0.30;

	CarrierLab::FCarrierState StateA;
	CarrierLab::FCarrierState StateB;
	FString Error;
	TestTrue(TEXT("First 1024/40 cold-start carrier builds"), CarrierLab::FCarrierLabStage0::BuildColdStartCarrier(Config, StateA, Error));
	if (!Error.IsEmpty())
	{
		AddInfo(Error);
	}
	Error.Reset();
	TestTrue(TEXT("Second 1024/40 cold-start carrier builds"), CarrierLab::FCarrierLabStage0::BuildColdStartCarrier(Config, StateB, Error));
	if (!Error.IsEmpty())
	{
		AddInfo(Error);
	}

	const CarrierLab::FStage0Metrics MetricsA = CarrierLab::FCarrierLabStage0::ProjectColdStart(StateA);
	const CarrierLab::FStage0Metrics MetricsB = CarrierLab::FCarrierLabStage0::ProjectColdStart(StateB);

	TestEqual(TEXT("No raw misses at 1024/40"), MetricsA.RawMissCount, 0);
	TestEqual(TEXT("No non-degenerate overlaps at 1024/40"), MetricsA.NonDegenerateOverlapCount, 0);
	TestEqual(TEXT("Euler characteristic remains spherical at 1024/40"), MetricsA.EulerCharacteristic, 2);
	TestEqual(TEXT("Plate-local triangle copy covers global TDS at 1024/40"), MetricsA.PlateLocalTriangleCount, MetricsA.TriangleCount);
	TestTrue(TEXT("Ray-triangle projection executed at 1024/40"), MetricsA.RayTriangleTestCount > 0);
	TestEqual(TEXT("Deterministic hash is stable at 1024/40"), MetricsA.DeterminismHash, MetricsB.DeterminismHash);

	AddInfo(FString::Printf(
		TEXT("[CarrierLabStage0 sample_count=%d plate_count=%d tris=%d local_tris=%d local_vertices=%d edges=%d euler=%d ray_tests=%lld hit=%d miss=%d multi=%d boundary_degenerate=%d third_plate_intrusion=%d caf=%.6f hash=%s]"),
		MetricsA.SampleCount,
		MetricsA.PlateCount,
		MetricsA.TriangleCount,
		MetricsA.PlateLocalTriangleCount,
		MetricsA.PlateLocalVertexCount,
		MetricsA.EdgeCount,
		MetricsA.EulerCharacteristic,
		MetricsA.RayTriangleTestCount,
		MetricsA.RawHitCount,
		MetricsA.RawMissCount,
		MetricsA.RawMultiHitCount,
		MetricsA.BoundaryDegenerateOverlapCount,
		MetricsA.ThirdPlateIntrusionCount,
		MetricsA.AuthoritativeContinentalAreaFraction,
		*MetricsA.DeterminismHash));

	return true;
}

#endif
