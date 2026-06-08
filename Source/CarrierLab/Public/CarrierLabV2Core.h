// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace CarrierLab::V2
{
	enum class ECarrierV2MaterialClass : uint8
	{
		Unknown = 0,
		Oceanic = 1,
		Continental = 2,
		Mixed = 3
	};

	enum class ECarrierV2FixtureKind : uint8
	{
		Positive = 0,
		Negative = 1,
		Scale = 2
	};

	struct FCarrierV2MaterialRecord
	{
		ECarrierV2MaterialClass MaterialClass = ECarrierV2MaterialClass::Unknown;
		double ContinentalFraction = 0.0;
		FString Provenance = TEXT("invalid");
	};

	struct FCarrierV2Stage0Config
	{
		FString FixtureId = TEXT("FX-000");
		FString FixtureName = TEXT("SinglePlateIdentity");
		FString FixtureSubstrateId = TEXT("handcrafted_octahedron");
		FString PartitionPolicyId = TEXT("explicit_fixture_partition");
		ECarrierV2FixtureKind FixtureKind = ECarrierV2FixtureKind::Positive;
		int32 SampleCount = 6;
		int32 PlateCount = 1;
		int32 Seed = 42;
		double RayEpsilon = 1.0e-9;
		bool bUseFibonacciSubstrate = false;
		bool bDeliberateTopologyHole = false;
		bool bDeliberateDuplicatedOverlap = false;
		FString ExpectedFailureReason = TEXT("none");
	};

	struct FCarrierV2SubstrateSample
	{
		int32 SampleId = INDEX_NONE;
		FVector3d UnitPosition = FVector3d::ZeroVector;
		double AreaWeight = 0.0;
	};

	struct FCarrierV2SubstrateTriangle
	{
		int32 TriangleId = INDEX_NONE;
		int32 SampleIds[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
	};

	struct FCarrierV2PlateVertex
	{
		int32 LocalVertexId = INDEX_NONE;
		int32 SourceSampleId = INDEX_NONE;
		FVector3d UnitPosition = FVector3d::ZeroVector;
		FCarrierV2MaterialRecord Material;
	};

	struct FCarrierV2PlateTriangle
	{
		int32 LocalTriangleId = INDEX_NONE;
		int32 SourceTriangleId = INDEX_NONE;
		int32 LocalVertexIds[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
		int32 SourceSampleIds[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
		bool bBoundary = false;
	};

	struct FCarrierV2Plate
	{
		int32 PlateId = INDEX_NONE;
		TArray<FCarrierV2PlateVertex> LocalVertices;
		TArray<FCarrierV2PlateTriangle> LocalTriangles;
		TMap<int32, int32> SourceSampleToLocalVertex;
	};

	struct FCarrierV2ProjectionHit
	{
		int32 SampleId = INDEX_NONE;
		int32 PlateId = INDEX_NONE;
		int32 LocalTriangleId = INDEX_NONE;
		int32 SourceTriangleId = INDEX_NONE;
		FVector3d Barycentric = FVector3d::ZeroVector;
		double HitT = 0.0;
		bool bBoundaryDegenerate = false;
		double ContinentalFraction = 0.0;
	};

	struct FCarrierV2ProjectionResult
	{
		int32 SampleId = INDEX_NONE;
		int32 RawHitCount = 0;
		int32 SelectedHitIndex = INDEX_NONE;
		bool bMiss = false;
		bool bNonDegenerateOverlap = false;
		bool bBoundaryDegenerate = false;
	};

	struct FCarrierV2Stage0Metrics
	{
		FString RunId;
		FString StageId = TEXT("V2-0");
		FString FixtureId;
		FString FixtureName;
		FString FixtureKind;
		int32 SampleCount = 0;
		int32 TriangleCount = 0;
		int32 PlateCount = 0;
		double RayEpsilon = 0.0;
		FString PartitionPolicyId;
		FString FixtureSubstrateId;
		FString InputHash;
		FString CarrierHash;
		FString ProjectionHash;
		FString MetricsHash;
		int32 GlobalSampleCount = 0;
		int32 GlobalTriangleCount = 0;
		int32 LocalPlateVertexCountSum = 0;
		int32 LocalPlateTriangleCountSum = 0;
		int32 TopologyDuplicateErrorCount = 0;
		int32 TopologyHoleErrorCount = 0;
		int64 RawHitCountTotal = 0;
		int64 RayTriangleTestCount = 0;
		int32 NonDegenerateMissCount = 0;
		int32 NonDegenerateOverlapCount = 0;
		int32 BoundaryDegenerateCount = 0;
		int32 BoundaryPolicySelectedCount = 0;
		int32 ProjectionReadsGlobalOwnerCount = 0;
		double MaterialAuthorityProjectedDelta = 0.0;
		FString ExpectedFailureReason = TEXT("none");
		FString ObservedFailureReason = TEXT("none");
		bool bFixturePass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("FAIL_IMPLEMENTATION");
		double BuildSubstrateMs = 0.0;
		double BuildPlateLocalMs = 0.0;
		double ProjectionKernelMs = 0.0;
		double MetricsMs = 0.0;
		double ReportMs = 0.0;
		double TotalMs = 0.0;
		double PeakMemoryMb = 0.0;
		bool bReplayDeterministic = false;
		FString ReplayCarrierHash;
		FString ReplayProjectionHash;
		FString ReplayMetricsHash;
	};

	struct FCarrierV2FixtureResult
	{
		FCarrierV2Stage0Config Config;
		FCarrierV2Stage0Metrics Metrics;
		bool bCompleted = false;
		FString Error;
	};

	struct FCarrierV2Stage0SuiteResult
	{
		TArray<FCarrierV2FixtureResult> Results;
		bool bMicroGatePass = false;
		bool bScale50kPass = false;
		bool bAttempted250k = false;
		bool bScale250kPass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("REVISE_V2_0");
		FString OutputRoot;
		FString MetricsPath;
		FString ReportPath;
	};

	class CARRIERLAB_API FCarrierV2Stage0
	{
	public:
		static TArray<FCarrierV2Stage0Config> MakeMicroFixtureConfigs();
		static FCarrierV2Stage0Config MakeScaleConfig(int32 SampleCount, bool bComparisonScale);
		static bool RunFixtureWithReplay(const FCarrierV2Stage0Config& Config, FCarrierV2FixtureResult& OutResult);
		static FString MetricsToJson(const FCarrierV2FixtureResult& Result);
		static FString BuildCheckpointReport(
			const FCarrierV2Stage0SuiteResult& Suite,
			const FString& CommandLine,
			const FString& CommitSha);
	};
}
