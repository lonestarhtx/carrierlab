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

	struct FCarrierV2MotionSpec
	{
		FVector3d Axis = FVector3d(0.0, 0.0, 1.0);
		double AngularSpeedRadPerMa = 0.0;
	};

	struct FCarrierV2Stage1Config
	{
		FCarrierV2Stage0Config BaseConfig;
		FString FixtureId = TEXT("FX-005");
		FString FixtureName = TEXT("ZeroMotionReplay");
		FString ExpectedMotionClass = TEXT("stable");
		FString SourceStatus = TEXT("source_explicit");
		FString ProjectionCandidatePolicyId = TEXT("all_plate_inverse_ray_static_aabb");
		TArray<FCarrierV2MotionSpec> PlateMotions;
		int32 MotionStepCount = 1;
		double DtMa = 1.0;
		double PlanetRadiusKm = 6371.0;
		double MotionToleranceKm = 1.0e-6;
		double UnitLengthTolerance = 1.0e-10;
		double RayParallelTolerance = 1.0e-12;
		double RayTMinTolerance = 1.0e-9;
		double BarycentricSlop = 1.0e-9;
		double BoundaryBand = 1.0e-9;
		double MotionOracleToleranceKm = 1.0e-6;
		double ExpectedMaxStepKernelMs = 0.0;
		bool bUseFullTriangleScanForProjection = false;
		bool bRunBruteforceProjectionOracle = true;
		bool bUseAngularCapPlateBroadphase = false;
		bool bRunAllPlateBroadphaseEquivalence = false;
		double BroadphaseAngularMarginRad = 0.05;
		bool bRequireNoMotionGapOrOverlap = false;
		bool bRequireDivergentCandidate = false;
		bool bRequireConvergentCandidate = false;
		bool bRequireThirdPlateIntrusion = false;
		bool bRequireOracleFrameSensitivity = false;
	};

	struct FCarrierV2Stage1Metrics
	{
		FString RunId;
		FString StageId = TEXT("V2-1");
		FString FixtureId;
		FString FixtureName;
		FString FixtureKind;
		FString ExpectedMotionClass;
		FString SourceStatus;
		FString ProjectionCandidatePolicyId;
		int32 SampleCount = 0;
		int32 TriangleCount = 0;
		int32 PlateCount = 0;
		int32 MotionStepCount = 0;
		double DtMa = 0.0;
		double TotalMotionMa = 0.0;
		double PlanetRadiusKm = 0.0;
		double MotionToleranceKm = 0.0;
		double UnitLengthTolerance = 0.0;
		double RayEpsilon = 0.0;
		double RayParallelTolerance = 0.0;
		double RayTMinTolerance = 0.0;
		double BarycentricSlop = 0.0;
		double BoundaryBand = 0.0;
		double MotionOracleToleranceKm = 0.0;
		double ExpectedMaxStepKernelMs = 0.0;
		FString ConfigHash;
		FString PreMotionAuthorityHash;
		FString PostMotionAuthorityHash;
		FString ProjectionOutputHash;
		FString MetricsHash;
		int32 GlobalSampleCount = 0;
		int32 GlobalTriangleCount = 0;
		int32 LocalPlateVertexCountSum = 0;
		int32 LocalPlateTriangleCountSum = 0;
		int32 MotionVertexCount = 0;
		double AnalyticMotionMaxErrorKm = 0.0;
		double AnalyticMotionMeanErrorKm = 0.0;
		double UnitLengthMaxError = 0.0;
		double RotatedVectorMaxError = 0.0;
		int32 RotatedVectorCount = 0;
		int32 MaterialAttachmentErrorCount = 0;
		int32 RawMotionMissCount = 0;
		int32 RawMotionOverlapCount = 0;
		double RawMotionMissFraction = 0.0;
		double RawMotionOverlapFraction = 0.0;
		FString TopMissPlatePairs;
		FString TopOverlapPlatePairs;
		int32 BoundaryDegenerateCount = 0;
		int32 BoundaryPolicySelectedCount = 0;
		int32 DivergentCandidateCount = 0;
		int32 ConvergentCandidateCount = 0;
		int32 ThirdPlateIntrusionCount = 0;
		int32 MaterialInterpolationCount = 0;
		int32 MotionRepairCount = 0;
		int32 RemeshDuringMotionCount = 0;
		int32 PrimaryResolverConsumedCount = 0;
		int32 ProjectionReadsGlobalOwnerCount = 0;
		int32 TreeTriangleCountSum = 0;
		int64 RayQueryCount = 0;
		int64 AabbHitCountTotal = 0;
		int64 BruteForceHitCountTotal = 0;
		int32 AabbBruteforceClassificationMismatchCount = 0;
		int32 LegacyMovedFrameBruteforceMismatchCount = 0;
		int64 BroadphaseCandidateQueryCount = 0;
		int64 BroadphaseSkippedPlateQueryCount = 0;
		int64 AllPlateEquivalenceRayQueryCount = 0;
		int32 BroadphaseEquivalenceMismatchCount = 0;
		int64 RawHitCountTotal = 0;
		int64 RayTriangleTestCount = 0;
		bool bAabbBruteforceEquivalencePass = false;
		bool bOracleFrameSensitivityPass = false;
		bool bBroadphaseEquivalencePass = false;
		bool bMotionOraclePass = false;
		bool bUnitLengthPass = false;
		bool bMaterialAttachmentPass = false;
		bool bPerformanceBudgetPass = false;
		bool bProjectionExpectationPass = false;
		bool bNoRepairOrRemeshPass = false;
		bool bReplayDeterministic = false;
		bool bFixturePass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("FAIL_IMPLEMENTATION");
		FString ReplayPostMotionAuthorityHash;
		FString ReplayProjectionOutputHash;
		FString ReplayMetricsHash;
		double BuildSubstrateMs = 0.0;
		double BuildPlateLocalMs = 0.0;
		double PlateAabbBuildMs = 0.0;
		double MotionApplyMs = 0.0;
		double InverseRayProjectionKernelMs = 0.0;
		double ProjectionKernelMs = 0.0;
		double MetricsMs = 0.0;
		double StepKernelMs = 0.0;
		double StepWithDiagnosticsMs = 0.0;
		double TotalMs = 0.0;
		double PeakMemoryMb = 0.0;
	};

	struct FCarrierV2Stage1FixtureResult
	{
		FCarrierV2Stage1Config Config;
		FCarrierV2Stage1Metrics Metrics;
		bool bCompleted = false;
		FString Error;
	};

	struct FCarrierV2Stage1SuiteResult
	{
		TArray<FCarrierV2Stage1FixtureResult> Results;
		bool bMicroGatePass = false;
		bool bScale50kPass = false;
		bool bAttempted100k = false;
		bool bScale100kPass = false;
		bool bAttempted250k = false;
		bool bScale250kPass = false;
		bool bAttempted500k = false;
		bool bScale500kPass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("REVISE_V2_1");
		FString OutputRoot;
		FString MetricsPath;
		FString ReportPath;
	};

	struct FCarrierV2ProcessCandidateRecord
	{
		int32 CandidateId = INDEX_NONE;
		int32 SampleId = INDEX_NONE;
		TArray<int32> PlateIds;
		TArray<int32> LocalTriangleIds;
		TArray<int32> ContactHitPlateIds;
		TArray<int32> ContactHitLocalTriangleIds;
		FString CandidateClass = TEXT("convergent_contact");
		FString EvidenceKind = TEXT("moved_geometry_multihit");
		bool bThirdPlateVisible = false;
		bool bPolarityCandidate = false;
		bool bAccepted = false;
	};

	struct FCarrierV2Stage2Config
	{
		FCarrierV2Stage1Config MotionConfig;
		FString FixtureId = TEXT("FX-008");
		FString FixtureName = TEXT("ForcedConvergenceContactDryRun");
		FString ExpectedProcessClass = TEXT("convergent_contact_dry_run");
		FString ContactPolicyId = TEXT("moved_geometry_full_scan_dry_run");
		bool bUseFullTriangleScanForContact = true;
		bool bRequireNoContactCandidates = false;
		bool bRequireContactCandidate = false;
		bool bRequireThirdPlateIntrusion = false;
		bool bRequirePolarityCandidate = false;
		int32 ExpectedMinimumContactCandidates = 0;
		int32 ExpectedMinimumThirdPlateIntrusions = 0;
		int32 ExpectedMinimumPolarityCandidates = 0;
	};

	struct FCarrierV2Stage2Metrics
	{
		FString RunId;
		FString StageId = TEXT("V2-2");
		FString FixtureId;
		FString FixtureName;
		FString FixtureKind;
		FString ExpectedMotionClass;
		FString ExpectedProcessClass;
		FString SourceStatus;
		FString ProjectionCandidatePolicyId;
		FString ContactPolicyId;
		int32 SampleCount = 0;
		int32 TriangleCount = 0;
		int32 PlateCount = 0;
		int32 MotionStepCount = 0;
		double DtMa = 0.0;
		double TotalMotionMa = 0.0;
		double PlanetRadiusKm = 0.0;
		double MotionToleranceKm = 0.0;
		double UnitLengthTolerance = 0.0;
		double RayEpsilon = 0.0;
		FString ConfigHash;
		FString PreMotionAuthorityHash;
		FString PostMotionAuthorityHash;
		FString ProjectionOutputHash;
		FString ProcessStateHash;
		FString MetricsHash;
		int32 GlobalSampleCount = 0;
		int32 GlobalTriangleCount = 0;
		int32 LocalPlateVertexCountSum = 0;
		int32 LocalPlateTriangleCountSum = 0;
		int32 MotionVertexCount = 0;
		double AnalyticMotionMaxErrorKm = 0.0;
		double AnalyticMotionMeanErrorKm = 0.0;
		double UnitLengthMaxError = 0.0;
		int32 MaterialAttachmentErrorCount = 0;
		int32 RawMotionMissCount = 0;
		int32 RawMotionOverlapCount = 0;
		int32 BoundaryDegenerateCount = 0;
		int32 BoundaryPolicySelectedCount = 0;
		int32 DivergentCandidateCount = 0;
		int32 ConvergentCandidateCount = 0;
		int32 MotionRepairCount = 0;
		int32 RemeshDuringMotionCount = 0;
		int32 ProjectionReadsGlobalOwnerCount = 0;
		int32 ContactCandidateCount = 0;
		int32 AcceptedConvergenceEvidenceCount = 0;
		int32 RejectedContactCount = 0;
		int32 ThirdPlateIntrusionCount = 0;
		int32 PolarityCandidateCount = 0;
		int32 ProcessMutationCount = 0;
		int32 CentroidPrimaryResolutionCount = 0;
		int32 RandomPrimaryResolutionCount = 0;
		int32 NearestPrimaryResolutionCount = 0;
		int32 ProjectionOwnerLabelEvidenceCount = 0;
		int32 OverlapConsumedBeforeProcessStateCount = 0;
		int32 MaterialMutationCount = 0;
		int32 RemeshDuringProcessDryRunCount = 0;
		int32 GapFillDuringProcessDryRunCount = 0;
		int64 RawHitCountTotal = 0;
		int64 RayTriangleTestCount = 0;
		int64 ContactRawHitCountTotal = 0;
		int64 ContactRayTriangleTestCount = 0;
		bool bMotionOraclePass = false;
		bool bUnitLengthPass = false;
		bool bMaterialAttachmentPass = false;
		bool bProjectionExpectationPass = false;
		bool bNoRepairOrRemeshPass = false;
		bool bContactEvidencePass = false;
		bool bDryRunNoMutationPass = false;
		bool bNoPrimaryResolverPass = false;
		bool bThirdPlatePass = false;
		bool bPolarityPass = false;
		bool bReplayDeterministic = false;
		bool bFixturePass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("FAIL_IMPLEMENTATION");
		FString ReplayPostMotionAuthorityHash;
		FString ReplayProjectionOutputHash;
		FString ReplayProcessStateHash;
		FString ReplayMetricsHash;
		double BuildSubstrateMs = 0.0;
		double BuildPlateLocalMs = 0.0;
		double MotionApplyMs = 0.0;
		double ProjectionKernelMs = 0.0;
		double ContactDetectionMs = 0.0;
		double MetricsMs = 0.0;
		double TotalMs = 0.0;
		double PeakMemoryMb = 0.0;
	};

	struct FCarrierV2Stage2FixtureResult
	{
		FCarrierV2Stage2Config Config;
		FCarrierV2Stage2Metrics Metrics;
		TArray<FCarrierV2ProcessCandidateRecord> ProcessCandidates;
		bool bCompleted = false;
		FString Error;
	};

	struct FCarrierV2Stage2SuiteResult
	{
		TArray<FCarrierV2Stage2FixtureResult> Results;
		bool bMicroGatePass = false;
		bool bScale50kPass = false;
		bool bAttempted250k = false;
		bool bScale250kPass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("REVISE_V2_2");
		FString OutputRoot;
		FString MetricsPath;
		FString ReportPath;
	};

	struct FCarrierV2Stage3PlateMaterialProfile
	{
		int32 PlateId = INDEX_NONE;
		ECarrierV2MaterialClass MaterialClass = ECarrierV2MaterialClass::Unknown;
		double OceanicAgeMa = 0.0;
		FString Provenance = TEXT("unset");
	};

	struct FCarrierV2TriangleProcessMarkRecord
	{
		int32 MarkId = INDEX_NONE;
		int32 EventId = INDEX_NONE;
		int32 SourceCandidateId = INDEX_NONE;
		int32 SampleId = INDEX_NONE;
		int32 PlateId = INDEX_NONE;
		int32 LocalTriangleId = INDEX_NONE;
		FString MarkClass = TEXT("unknown");
		FString Provenance = TEXT("unset");
	};

	struct FCarrierV2ProcessEventRecord
	{
		int32 EventId = INDEX_NONE;
		int32 SourceCandidateId = INDEX_NONE;
		int32 SampleId = INDEX_NONE;
		FString EventClass = TEXT("unknown");
		FString ProcessClass = TEXT("unknown");
		int32 SourcePlateId = INDEX_NONE;
		int32 DestinationPlateId = INDEX_NONE;
		int32 SourceLocalTriangleId = INDEX_NONE;
		int32 DestinationLocalTriangleId = INDEX_NONE;
		int32 SubductingPlateId = INDEX_NONE;
		int32 OverridingPlateId = INDEX_NONE;
		int32 SubductingLocalTriangleId = INDEX_NONE;
		ECarrierV2MaterialClass SourceMaterialClass = ECarrierV2MaterialClass::Unknown;
		ECarrierV2MaterialClass DestinationMaterialClass = ECarrierV2MaterialClass::Unknown;
		double SourceOceanicAgeMa = 0.0;
		double DestinationOceanicAgeMa = 0.0;
		bool bOlderOceanicSubducts = false;
		bool bHasProvenance = false;
		FString Provenance = TEXT("unset");
	};

	struct FCarrierV2Stage3Config
	{
		FCarrierV2Stage2Config ContactConfig;
		FString FixtureId = TEXT("FX-010");
		FString FixtureName = TEXT("OceanOceanAgePolarity");
		FString ExpectedProcessClass = TEXT("ocean_ocean_age_polarity");
		FString ProcessMutationPolicyId = TEXT("process_state_event_marks_no_remesh");
		TArray<FCarrierV2Stage3PlateMaterialProfile> PlateMaterialProfiles;
		bool bRequireSubductionMark = false;
		bool bRequireOceanicAgePolarity = false;
		bool bRequireCollisionCandidate = false;
		bool bRequireProcessEvents = false;
		bool bEnableSlabPull = false;
		int32 ExpectedMinimumSubductionEvents = 0;
		int32 ExpectedMinimumSubductingTriangleMarks = 0;
		int32 ExpectedMinimumCollisionCandidates = 0;
		int32 ExpectedMinimumCollisionEvents = 0;
		int32 ExpectedMinimumProcessEvents = 0;
	};

	struct FCarrierV2Stage3Metrics
	{
		FString RunId;
		FString StageId = TEXT("V2-3");
		FString FixtureId;
		FString FixtureName;
		FString FixtureKind;
		FString ExpectedMotionClass;
		FString ExpectedProcessClass;
		FString SourceStatus;
		FString ProjectionCandidatePolicyId;
		FString ContactPolicyId;
		FString ProcessMutationPolicyId;
		int32 SampleCount = 0;
		int32 TriangleCount = 0;
		int32 PlateCount = 0;
		int32 MotionStepCount = 0;
		double DtMa = 0.0;
		double TotalMotionMa = 0.0;
		double PlanetRadiusKm = 0.0;
		double MotionToleranceKm = 0.0;
		double UnitLengthTolerance = 0.0;
		double RayEpsilon = 0.0;
		FString ConfigHash;
		FString Stage2ProcessStateHash;
		FString ProcessStateHash;
		FString MetricsHash;
		int32 GlobalSampleCount = 0;
		int32 GlobalTriangleCount = 0;
		int32 LocalPlateVertexCountSum = 0;
		int32 LocalPlateTriangleCountSum = 0;
		int32 MotionVertexCount = 0;
		double AnalyticMotionMaxErrorKm = 0.0;
		double AnalyticMotionMeanErrorKm = 0.0;
		double UnitLengthMaxError = 0.0;
		int32 MaterialAttachmentErrorCount = 0;
		int32 RawMotionMissCount = 0;
		int32 RawMotionOverlapCount = 0;
		int32 DivergentCandidateCount = 0;
		int32 ConvergentCandidateCount = 0;
		int32 ContactCandidateCount = 0;
		int32 AcceptedConvergenceEvidenceCount = 0;
		int32 ThirdPlateIntrusionCount = 0;
		int32 PolarityCandidateCount = 0;
		int32 ThirdPlatePassthroughCount = 0;
		int32 ProcessEventCount = 0;
		int32 ProcessEventWithProvenanceCount = 0;
		int32 ProcessEventWithoutProvenanceCount = 0;
		int32 SubductionEventCount = 0;
		int32 OceanicAgePolarityDecisionCount = 0;
		int32 OlderOceanicSubductingPassCount = 0;
		int32 SubductingTriangleMarkCount = 0;
		int32 CollisionCandidateCount = 0;
		int32 CollisionEventCount = 0;
		int32 TerraneDetachTriangleCount = 0;
		int32 TerraneSutureTriangleCount = 0;
		int32 MaterialDestroyedWithoutProcessCount = 0;
		int32 MaterialCreatedWithoutProcessCount = 0;
		int32 TopologyMutationWithoutEventCount = 0;
		int32 MaterialMutationCount = 0;
		int32 RemeshDuringProcessMutationCount = 0;
		int32 TopologyRebuildDuringProcessMutationCount = 0;
		int32 GapFillDuringProcessMutationCount = 0;
		int32 DivergentGenerationDuringProcessMutationCount = 0;
		int32 TerrainBeautyMutationCount = 0;
		int32 OwnershipRepairDuringProcessMutationCount = 0;
		int32 CentroidPrimaryResolutionCount = 0;
		int32 RandomPrimaryResolutionCount = 0;
		int32 NearestPrimaryResolutionCount = 0;
		int32 ProjectionOwnerLabelEvidenceCount = 0;
		int32 OverlapConsumedBeforeProcessStateCount = 0;
		bool bSlabPullEnabled = false;
		double SlabPullAxisDeltaDeg = 0.0;
		bool bInheritedStage2Pass = false;
		bool bMotionOraclePass = false;
		bool bUnitLengthPass = false;
		bool bMaterialAttachmentPass = false;
		bool bProjectionExpectationPass = false;
		bool bContactEvidencePass = false;
		bool bProcessEventProvenancePass = false;
		bool bSubductionPolarityPass = false;
		bool bCollisionCandidatePass = false;
		bool bProcessEventExpectationPass = false;
		bool bNoForbiddenMutationPass = false;
		bool bSlabPullPass = false;
		bool bReplayDeterministic = false;
		bool bFixturePass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("FAIL_IMPLEMENTATION");
		FString ReplayStage2ProcessStateHash;
		FString ReplayProcessStateHash;
		FString ReplayMetricsHash;
		double BuildSubstrateMs = 0.0;
		double BuildPlateLocalMs = 0.0;
		double MotionApplyMs = 0.0;
		double ProjectionKernelMs = 0.0;
		double ContactDetectionMs = 0.0;
		double ProcessMutationMs = 0.0;
		double MetricsMs = 0.0;
		double TotalMs = 0.0;
		double PeakMemoryMb = 0.0;
	};

	struct FCarrierV2Stage3FixtureResult
	{
		FCarrierV2Stage3Config Config;
		FCarrierV2Stage3Metrics Metrics;
		FCarrierV2Stage2FixtureResult Stage2Result;
		TArray<FCarrierV2Stage3PlateMaterialProfile> PlateMaterialProfiles;
		TArray<FCarrierV2ProcessEventRecord> ProcessEvents;
		TArray<FCarrierV2TriangleProcessMarkRecord> TriangleMarks;
		bool bCompleted = false;
		FString Error;
	};

	struct FCarrierV2Stage3SuiteResult
	{
		TArray<FCarrierV2Stage3FixtureResult> Results;
		bool bMicroGatePass = false;
		bool bScale50kPass = false;
		bool bAttempted250k = false;
		bool bScale250kPass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("REVISE_V2_3");
		FString OutputRoot;
		FString MetricsPath;
		FString ReportPath;
	};

	struct FCarrierV2RemeshSampleRecord
	{
		int32 SampleId = INDEX_NONE;
		int32 RawHitCount = 0;
		int32 FilteredSubductingHitCount = 0;
		int32 FilteredCollidingHitCount = 0;
		int32 ValidHitCount = 0;
		int32 SelectedPlateId = INDEX_NONE;
		int32 SelectedLocalTriangleId = INDEX_NONE;
		int32 SelectedSourceTriangleId = INDEX_NONE;
		double SelectedContinentalFraction = 0.0;
		bool bZeroValidHit = false;
		bool bPostFilterUnresolvedMultihit = false;
		bool bSelectedFilteredHit = false;
		FString SelectionProvenance = TEXT("unset");
	};

	struct FCarrierV2Stage4Config
	{
		FCarrierV2Stage3Config ProcessConfig;
		FString FixtureId = TEXT("FX-012");
		FString FixtureName = TEXT("FilteredSubductionOverlapSampling");
		FString ExpectedRemeshClass = TEXT("global_tds_filtered_overlap_sampling");
		FString RemeshSamplingPolicyId = TEXT("global_tds_aabb_all_hit_filter_no_q1q2");
		FString RemeshTriggerReason = TEXT("manual_fixture");
		bool bRequireFilteredSubductingHit = false;
		bool bRequireFilteredCollidingHit = false;
		bool bRequireValidHitAfterFilter = false;
		bool bAllowDeferredGapFill = true;
		int32 ExpectedMinimumFilteredSubductingHits = 0;
		int32 ExpectedMinimumFilteredCollidingHits = 0;
		int32 ExpectedMinimumValidHits = 0;
	};

	struct FCarrierV2Stage4Metrics
	{
		FString RunId;
		FString StageId = TEXT("V2-4");
		FString FixtureId;
		FString FixtureName;
		FString FixtureKind;
		FString ExpectedRemeshClass;
		FString SourceStatus;
		FString RemeshSamplingPolicyId;
		FString RemeshTriggerReason;
		int32 SampleCount = 0;
		int32 TriangleCount = 0;
		int32 PlateCount = 0;
		double RayEpsilon = 0.0;
		FString ConfigHash;
		FString Stage3ProcessStateHash;
		FString RemeshInputHash;
		FString GlobalSamplingHash;
		FString MetricsHash;
		int32 GlobalSampleCount = 0;
		int32 GlobalTriangleCount = 0;
		int32 LocalPlateVertexCountSum = 0;
		int32 LocalPlateTriangleCountSum = 0;
		int32 PretreatedPlateCount = 0;
		int32 FullySubductedPlateDestroyedCount = 0;
		int32 AabbMeshTriangleCount = 0;
		int64 AabbRayQueryCount = 0;
		int64 RawHitCountTotal = 0;
		int32 FilteredSubductingHitCount = 0;
		int32 FilteredCollidingHitCount = 0;
		int32 ValidHitAfterFilterCount = 0;
		int32 ZeroValidHitCount = 0;
		int32 GapFillDeferredCount = 0;
		int32 PostFilterUnresolvedMultihitCount = 0;
		int32 PostFilterBoundaryOnlyMultihitCount = 0;
		int32 BoundaryDegenerateHitCount = 0;
		int32 MaterialInterpolationCount = 0;
		int32 SelectedFilteredHitCount = 0;
		int32 PriorOwnerFallbackCount = 0;
		int32 CentroidPrimaryResolutionCount = 0;
		int32 RandomPrimaryResolutionCount = 0;
		int32 NearestPrimaryResolutionCount = 0;
		int32 OwnershipRepairDuringRemeshCount = 0;
		int32 RetentionHysteresisAnchorCount = 0;
		int32 GeneratedOceanicCount = 0;
		int32 Q1Q2DeferredCount = 0;
		int32 Q1Q2DiscreteApproxCount = 0;
		int32 Q1Q2PriorOwnerLookupCount = 0;
		int32 TopologyRebuildDuringSamplingCount = 0;
		int32 ProcessStateResetCount = 0;
		int32 TerrainBeautyMutationCount = 0;
		int32 MaterialCreatedWithoutGapFillCount = 0;
		int32 MaterialDestroyedWithoutProcessCount = 0;
		bool bInheritedStage3Pass = false;
		bool bSourceFilterPass = false;
		bool bValidSelectionPass = false;
		bool bNoForbiddenFallbackPass = false;
		bool bDeferredGapFillPass = false;
		bool bNoPrematureTopologyPass = false;
		bool bReplayDeterministic = false;
		bool bFixturePass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("FAIL_IMPLEMENTATION");
		FString ReplayStage3ProcessStateHash;
		FString ReplayGlobalSamplingHash;
		FString ReplayMetricsHash;
		double Stage3Ms = 0.0;
		double BuildSubstrateMs = 0.0;
		double BuildPlateLocalMs = 0.0;
		double MotionApplyMs = 0.0;
		double AabbBuildMs = 0.0;
		double GlobalSamplingMs = 0.0;
		double MetricsMs = 0.0;
		double TotalMs = 0.0;
		double PeakMemoryMb = 0.0;
	};

	struct FCarrierV2Stage4FixtureResult
	{
		FCarrierV2Stage4Config Config;
		FCarrierV2Stage4Metrics Metrics;
		FCarrierV2Stage3FixtureResult Stage3Result;
		TArray<FCarrierV2RemeshSampleRecord> SampleRecords;
		bool bCompleted = false;
		FString Error;
	};

	struct FCarrierV2Stage4SuiteResult
	{
		TArray<FCarrierV2Stage4FixtureResult> Results;
		bool bMicroGatePass = false;
		bool bScale50kPass = false;
		bool bAttempted250k = false;
		bool bScale250kPass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("REVISE_V2_4");
		FString OutputRoot;
		FString MetricsPath;
		FString ReportPath;
	};

	struct FCarrierV2Stage5SampleRecord
	{
		int32 SampleId = INDEX_NONE;
		int32 RawHitCount = 0;
		int32 FilteredSubductingHitCount = 0;
		int32 FilteredCollidingHitCount = 0;
		int32 ValidHitCount = 0;
		int32 SelectedPlateId = INDEX_NONE;
		int32 SelectedLocalTriangleId = INDEX_NONE;
		int32 SelectedSourceTriangleId = INDEX_NONE;
		double SelectedContinentalFraction = 0.0;
		bool bZeroValidHit = false;
		bool bGeneratedOceanic = false;
		bool bPostFilterUnresolvedMultihit = false;
		bool bSelectedFilteredHit = false;
		bool bBoundaryPairFound = false;
		bool bQ1Q2DifferentPlates = false;
		int32 Q1PlateId = INDEX_NONE;
		int32 Q2PlateId = INDEX_NONE;
		int32 AssignedPlateId = INDEX_NONE;
		double Q1DistanceRad = 0.0;
		double Q2DistanceRad = 0.0;
		double QGammaDistanceRad = 0.0;
		double QGammaAlpha = 0.0;
		double Q1BoundaryContinentalFraction = 0.0;
		double Q2BoundaryContinentalFraction = 0.0;
		FString SelectionProvenance = TEXT("unset");
	};

	struct FCarrierV2Stage5Config
	{
		FCarrierV2Stage4Config SamplingConfig;
		FString FixtureId = TEXT("FX-014");
		FString FixtureName = TEXT("DivergentGapFillAndRebuild");
		FString ExpectedRemeshClass = TEXT("global_tds_q1q2_gap_fill_topology_rebuild");
		FString RemeshSamplingPolicyId = TEXT("global_tds_aabb_filter_q1q2_continuous_rebuild_reset");
		FString TrianglePartitionPolicyId = TEXT("global_triangle_vertex_assignment_majority_area_count_tiebreak");
		FString RemeshTriggerReason = TEXT("manual_fixture");
		bool bRequireGeneratedOceanic = false;
		bool bRequireContinuousQ1Q2 = false;
		bool bRequireTopologyRebuild = true;
		bool bRequireProcessReset = true;
		int32 ExpectedMinimumGeneratedOceanic = 0;
		int32 ExpectedMinimumQ1Q2Pairs = 0;
	};

	struct FCarrierV2Stage5Metrics
	{
		FString RunId;
		FString StageId = TEXT("V2-5");
		FString FixtureId;
		FString FixtureName;
		FString FixtureKind;
		FString ExpectedRemeshClass;
		FString SourceStatus;
		FString RemeshSamplingPolicyId;
		FString TrianglePartitionPolicyId;
		FString RemeshTriggerReason;
		int32 SampleCount = 0;
		int32 TriangleCount = 0;
		int32 PlateCount = 0;
		double RayEpsilon = 0.0;
		FString ConfigHash;
		FString Stage3ProcessStateHash;
		FString RemeshInputHash;
		FString GlobalSamplingHash;
		FString GapFillHash;
		FString RebuiltTopologyHash;
		FString MetricsHash;
		int32 GlobalSampleCount = 0;
		int32 GlobalTriangleCount = 0;
		int32 LocalPlateVertexCountSum = 0;
		int32 LocalPlateTriangleCountSum = 0;
		int32 PretreatedPlateCount = 0;
		int32 FullySubductedPlateDestroyedCount = 0;
		int32 BoundaryEdgeCount = 0;
		int32 AabbMeshTriangleCount = 0;
		int64 AabbRayQueryCount = 0;
		int64 RawHitCountTotal = 0;
		int32 FilteredSubductingHitCount = 0;
		int32 FilteredCollidingHitCount = 0;
		int32 ValidHitAfterFilterCount = 0;
		int32 ZeroValidHitCount = 0;
		int32 Q1Q2GapFillCount = 0;
		int32 Q1Q2BoundaryQueryCount = 0;
		int32 Q1Q2BoundaryPairCount = 0;
		int32 Q1Q2DifferentPlatePairCount = 0;
		int32 QGammaComputedCount = 0;
		int32 GeneratedOceanicCount = 0;
		int32 GapFillNoBoundaryPairCount = 0;
		int32 PostFilterUnresolvedMultihitCount = 0;
		int32 PostFilterBoundaryOnlyMultihitCount = 0;
		int32 BoundaryDegenerateHitCount = 0;
		int32 MaterialInterpolationCount = 0;
		int32 SelectedFilteredHitCount = 0;
		int32 PriorOwnerFallbackCount = 0;
		int32 CentroidPrimaryResolutionCount = 0;
		int32 RandomPrimaryResolutionCount = 0;
		int32 NearestPrimaryResolutionCount = 0;
		int32 OwnershipRepairDuringRemeshCount = 0;
		int32 RetentionHysteresisAnchorCount = 0;
		int32 Q1Q2DiscreteApproxCount = 0;
		int32 Q1Q2PriorOwnerLookupCount = 0;
		int32 TopologyRebuildCount = 0;
		int32 RebuiltPlateCount = 0;
		int32 RebuiltLocalVertexCountSum = 0;
		int32 RebuiltLocalTriangleCountSum = 0;
		int32 RebuiltTriangleAssignmentCount = 0;
		int32 MixedVertexTriangleCount = 0;
		int32 MajorityTriangleAssignmentCount = 0;
		int32 ThreeWayTriangleAssignmentCount = 0;
		int32 UnassignedTriangleCount = 0;
		int32 ProcessStateResetCount = 0;
		int32 PreResetTriangleMarkCount = 0;
		int32 PostResetTriangleMarkCount = 0;
		int32 TerrainBeautyMutationCount = 0;
		int32 MaterialCreatedWithoutGapFillCount = 0;
		int32 MaterialDestroyedWithoutProcessCount = 0;
		bool bInheritedStage3Pass = false;
		bool bSourceFilterPass = false;
		bool bQ1Q2GapFillPass = false;
		bool bTopologyRebuildPass = false;
		bool bProcessResetPass = false;
		bool bNoForbiddenFallbackPass = false;
		bool bReplayDeterministic = false;
		bool bFixturePass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("FAIL_IMPLEMENTATION");
		FString ReplayStage3ProcessStateHash;
		FString ReplayGlobalSamplingHash;
		FString ReplayGapFillHash;
		FString ReplayRebuiltTopologyHash;
		FString ReplayMetricsHash;
		double Stage3Ms = 0.0;
		double BuildSubstrateMs = 0.0;
		double BuildPlateLocalMs = 0.0;
		double MotionApplyMs = 0.0;
		double AabbBuildMs = 0.0;
		double GlobalSamplingMs = 0.0;
		double TopologyRebuildMs = 0.0;
		double MetricsMs = 0.0;
		double TotalMs = 0.0;
		double PeakMemoryMb = 0.0;
	};

	struct FCarrierV2Stage5FixtureResult
	{
		FCarrierV2Stage5Config Config;
		FCarrierV2Stage5Metrics Metrics;
		FCarrierV2Stage3FixtureResult Stage3Result;
		TArray<FCarrierV2Stage5SampleRecord> SampleRecords;
		TArray<FCarrierV2Plate> RebuiltPlates;
		bool bCompleted = false;
		FString Error;
	};

	struct FCarrierV2Stage5SuiteResult
	{
		TArray<FCarrierV2Stage5FixtureResult> Results;
		bool bMicroGatePass = false;
		bool bScale50kPass = false;
		bool bAttempted250k = false;
		bool bScale250kPass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("REVISE_V2_5");
		FString OutputRoot;
		FString MetricsPath;
		FString ReportPath;
	};

	struct FCarrierV2Milestone2SampleRecord
	{
		int32 SampleId = INDEX_NONE;
		int32 RawHitCount = 0;
		int32 ValidSingleHitCount = 0;
		int32 SelectedPlateId = INDEX_NONE;
		int32 SelectedLocalTriangleId = INDEX_NONE;
		int32 SelectedSourceTriangleId = INDEX_NONE;
		int32 AssignedPlateId = INDEX_NONE;
		double SelectedContinentalFraction = 0.0;
		bool bSingleHitWritten = false;
		bool bDivergentZeroHit = false;
		bool bGeneratedOceanic = false;
		bool bNondegenerateOverlapBlocked = false;
		bool bBoundaryOnlyOverlap = false;
		bool bBoundaryPairFound = false;
		bool bQ1Q2DifferentPlates = false;
		int32 Q1PlateId = INDEX_NONE;
		int32 Q2PlateId = INDEX_NONE;
		double Q1DistanceRad = 0.0;
		double Q2DistanceRad = 0.0;
		double QGammaDistanceRad = 0.0;
		double QGammaAlpha = 0.0;
		double Q1BoundaryContinentalFraction = 0.0;
		double Q2BoundaryContinentalFraction = 0.0;
		FName SelectionProvenance = TEXT("unset");
	};

	struct FCarrierV2Milestone2Config
	{
		FCarrierV2Stage1Config MotionConfig;
		FString FixtureId = TEXT("M2-FX-001");
		FString FixtureName = TEXT("NoMotionResampleNoop");
		FString CarrierCycleClass = TEXT("scheduled_motion_resample_writeback");
		FString ResamplePolicyId = TEXT("single_hit_or_continuous_q1q2_overlap_blocked");
		FString TrianglePartitionPolicyId = TEXT("global_triangle_vertex_assignment_majority_area_count_tiebreak");
		FString ResampleTriggerReason = TEXT("scheduled_fixture_window");
		int32 ResampleCadenceSteps = 1;
		int32 LifecycleWindowCount = 1;
		bool bRequireSingleHitWrites = false;
		bool bRequireDivergentGapFill = false;
		bool bRequireOverlapBlocked = false;
		bool bRequireFullTopologyRebuild = true;
		bool bRequireMaterialConservation = false;
		bool bRequireSharpnessPreservation = false;
		bool bInjectPriorOwnerLabelsForNegativeControl = false;
		bool bScaleCharacterization = false;
		int32 ExpectedMinimumSingleHitWrites = 0;
		int32 ExpectedMinimumGapFill = 0;
		int32 ExpectedMinimumOverlapBlocked = 0;
		double MaterialConservationTolerance = 1.0e-9;
		double TotalVariationTolerance = 1.0e-9;
		double ExpectedMaxStepKernelMs = 0.0;
	};

	struct FCarrierV2Milestone2Metrics
	{
		FString RunId;
		FString StageId = TEXT("M2");
		FString FixtureId;
		FString FixtureName;
		FString FixtureKind;
		FString CarrierCycleClass;
		FString SourceStatus;
		FString ResamplePolicyId;
		FString TrianglePartitionPolicyId;
		FString ResampleTriggerReason;
		int32 SampleCount = 0;
		int32 TriangleCount = 0;
		int32 PlateCount = 0;
		int32 ResampleCadenceSteps = 0;
		int32 LifecycleWindowCount = 0;
		double DtMa = 0.0;
		double TotalMotionMa = 0.0;
		double PlanetRadiusKm = 0.0;
		double RayEpsilon = 0.0;
		double BroadphaseAngularMarginRad = 0.0;
		double MaxPlateMotionAngleRad = 0.0;
		bool bUsedAngularCapBroadphase = false;
		bool bBroadphaseMarginGatePass = false;
		FString ConfigHash;
		FString PreCycleAuthorityHash;
		FString PostCycleAuthorityHash;
		FString ResampleOutputHash;
		FString RebuiltTopologyHash;
		FString MetricsHash;
		int32 GlobalSampleCount = 0;
		int32 GlobalTriangleCount = 0;
		int32 LocalPlateVertexCountSum = 0;
		int32 LocalPlateTriangleCountSum = 0;
		int32 RebuiltPlateCount = 0;
		int32 RebuiltLocalVertexCountSum = 0;
		int32 RebuiltLocalTriangleCountSum = 0;
		int32 RebuiltTriangleAssignmentCount = 0;
		int32 MixedVertexTriangleCount = 0;
		int32 MajorityTriangleAssignmentCount = 0;
		int32 ThreeWayTriangleAssignmentCount = 0;
		int32 UnassignedTriangleCount = 0;
		int32 UnassignedTriangleBudget = 0;
		int32 BoundaryEdgeCount = 0;
		int64 AabbRayQueryCount = 0;
		int64 RawHitCountTotal = 0;
		int32 BroadphaseEquivalenceMismatchCount = 0;
		int32 ValidSingleHitWriteCount = 0;
		int32 MaterialInterpolationCount = 0;
		int32 DivergentZeroHitCount = 0;
		int32 Q1Q2GapFillCount = 0;
		int32 Q1Q2BoundaryQueryCount = 0;
		int32 Q1Q2BoundaryPairCount = 0;
		int32 Q1Q2DifferentPlatePairCount = 0;
		int32 QGammaComputedCount = 0;
		int32 GeneratedOceanicCount = 0;
		int32 GapFillNoBoundaryPairCount = 0;
		int32 NondegenerateOverlapBlockedCount = 0;
		int32 BoundaryOnlyOverlapCount = 0;
		int32 CrossPlateBoundaryOnlyOverlapCount = 0;
		int32 SamePlateBoundaryOnlyMultihitCount = 0;
		int32 DeferredOverlapSampleCount = 0;
		double DeferredOverlapAreaWeight = 0.0;
		double DeferredOverlapContinentalMassEstimate = 0.0;
		int32 UnsupportedOverlapWriteAttemptCount = 0;
		int32 PriorOwnerReadCount = 0;
		int32 PriorOwnerFallbackCount = 0;
		int32 GlobalOwnerReadCount = 0;
		int32 CentroidPrimaryResolutionCount = 0;
		int32 RandomPrimaryResolutionCount = 0;
		int32 NearestPrimaryResolutionCount = 0;
		int32 OwnershipRepairDuringResampleCount = 0;
		int32 RetentionHysteresisAnchorCount = 0;
		int32 Q1Q2DiscreteApproxCount = 0;
		int32 Q1Q2PriorOwnerLookupCount = 0;
		int32 TerrainBeautyMutationCount = 0;
		int32 SubductionMutationCount = 0;
		int32 CollisionMutationCount = 0;
		int32 RiftingMutationCount = 0;
		int32 TopologyRebuildCount = 0;
		int32 ProcessStateResetCount = 0;
		int32 RemeshWindowCount = 0;
		double MaterialMassBefore = 0.0;
		double MaterialMassAfter = 0.0;
		double MaterialConservationDelta = 0.0;
		double TotalVariationBefore = 0.0;
		double TotalVariationAfter = 0.0;
		double TotalVariationDelta = 0.0;
		FString TopMissPlatePairs;
		FString TopOverlapPlatePairs;
		bool bSingleHitTransferPass = false;
		bool bDivergentGapFillPass = false;
		bool bOverlapPolicyPass = false;
		bool bTopologyRebuildPass = false;
		bool bUnassignedTriangleBudgetPass = true;
		bool bLifecycleConservationPass = false;
		bool bNoForbiddenFallbackPass = false;
		bool bPerformanceBudgetPass = true;
		bool bPaperResampleCycleBudgetPass = true;
		bool bReplayDeterministic = false;
		bool bFixturePass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("FAIL_IMPLEMENTATION");
		FString ReplayPreCycleAuthorityHash;
		FString ReplayPostCycleAuthorityHash;
		FString ReplayResampleOutputHash;
		FString ReplayRebuiltTopologyHash;
		FString ReplayMetricsHash;
		double BuildSubstrateMs = 0.0;
		double BuildPlateLocalMs = 0.0;
		double MotionApplyMs = 0.0;
		double AabbBuildMs = 0.0;
		double ResampleMs = 0.0;
		double TopologyRebuildMs = 0.0;
		double MetricsMs = 0.0;
		double StepKernelMs = 0.0;
		double FullCarrierCycleMs = 0.0;
		double PaperResampleCycleBudgetMs = 0.0;
		double TotalMs = 0.0;
		double PeakMemoryMb = 0.0;
	};

	struct FCarrierV2Milestone2FixtureResult
	{
		FCarrierV2Milestone2Config Config;
		FCarrierV2Milestone2Metrics Metrics;
		TArray<FCarrierV2Milestone2SampleRecord> SampleRecords;
		TArray<FCarrierV2Plate> RebuiltPlates;
		bool bCompleted = false;
		FString Error;
	};

	struct FCarrierV2Milestone2SuiteResult
	{
		TArray<FCarrierV2Milestone2FixtureResult> Results;
		bool bMicroGatePass = false;
		bool bScale50kPass = false;
		bool bScale100kReported = false;
		bool bAttempted250k = false;
		bool bScale250kPass = false;
		bool bAttempted500k = false;
		bool bScale500kPass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("REVISE_MILESTONE_2");
		FString OutputRoot;
		FString MetricsPath;
		FString ReportPath;
		FString NotAttempted500kReason;
	};

	struct FCarrierV2Milestone3PinnedM2Baseline
	{
		FString FixtureId;
		FString ExpectedPostCycleAuthorityHash;
		FString ExpectedResampleOutputHash;
		FString ExpectedRebuiltTopologyHash;
	};

	struct FCarrierV2Milestone3ContactRecord
	{
		int32 ContactId = INDEX_NONE;
		int32 PlateA = INDEX_NONE;
		int32 PlateB = INDEX_NONE;
		int32 PlateC = INDEX_NONE;
		int32 LocalTriangleA = INDEX_NONE;
		int32 LocalTriangleB = INDEX_NONE;
		int32 SourceEdgeA = INDEX_NONE;
		int32 SourceEdgeB = INDEX_NONE;
		double SignedOpeningRate = 0.0;
		double PlateAContinentalFraction = 0.0;
		double PlateBContinentalFraction = 0.0;
		FString ContactClass = TEXT("unknown");
		FString PolarityClass = TEXT("none");
		FString Provenance = TEXT("unset");
	};

	struct FCarrierV2Milestone3TriangleLabelRecord
	{
		int32 LabelId = INDEX_NONE;
		int32 ContactId = INDEX_NONE;
		int32 PlateId = INDEX_NONE;
		int32 LocalTriangleId = INDEX_NONE;
		int32 SourceTriangleId = INDEX_NONE;
		FString LabelClass = TEXT("unknown");
		FString Provenance = TEXT("unset");
		bool bFilterActive = false;
	};

	struct FCarrierV2Milestone3SampleRecord
	{
		int32 SampleId = INDEX_NONE;
		int32 RawHitCount = 0;
		int32 FilteredSubductingHitCount = 0;
		int32 FilteredCollidingHitCount = 0;
		int32 ValidHitCount = 0;
		int32 SelectedPlateId = INDEX_NONE;
		int32 SelectedLocalTriangleId = INDEX_NONE;
		int32 SelectedSourceTriangleId = INDEX_NONE;
		int32 AssignedPlateId = INDEX_NONE;
		double SelectedContinentalFraction = 0.0;
		bool bSingleHitWritten = false;
		bool bSelectedFilteredHit = false;
		bool bFilterExhausted = false;
		bool bPostFilterUnresolvedMultihit = false;
		bool bDeferredNondivergentGap = false;
		bool bGeneratedOceanic = false;
		bool bPreviouslyBlockedSample = false;
		bool bPreviouslyBlockedBecameQ1Q2Oceanic = false;
		bool bBoundaryPairFound = false;
		bool bQ1Q2DifferentPlates = false;
		int32 Q1PlateId = INDEX_NONE;
		int32 Q2PlateId = INDEX_NONE;
		double Q1DistanceRad = 0.0;
		double Q2DistanceRad = 0.0;
		double QGammaDistanceRad = 0.0;
		double QGammaAlpha = 0.0;
		double Q1Q2OpeningRate = 0.0;
		FString SelectionProvenance = TEXT("unset");
	};

	struct FCarrierV2Milestone3Config
	{
		FCarrierV2Milestone2Config CarrierCycleConfig;
		FString FixtureId = TEXT("M3-FX-001");
		FString FixtureName = TEXT("PinnedM2Baseline");
		FString ProcessFilterPolicyId = TEXT("contact_labels_filter_subducting_colliding_only");
		FString ContactEvidencePolicyId = TEXT("plate_local_shared_source_edge_interior_probe_signed_opening");
		FString PolarityPolicyId = TEXT("contact_local_material_conservative");
		FString PolarityMode = TEXT("auto_contact_local_material");
		TArray<FCarrierV2Milestone3PinnedM2Baseline> PinnedM2Baselines;
		bool bRunPinnedM2BaselinesOnly = false;
		bool bEnableProcessFilters = true;
		bool bAllowFixtureSpecifiedPolarity = false;
		bool bForceAllOverlapHitsFiltered = false;
		bool bForceNoFilterLabels = false;
		bool bScaleCharacterization = false;
		bool bRequirePinnedM2Baseline = false;
		bool bRequireFilterInertNoop = false;
		bool bRequireHolePumpTripwire = false;
		bool bRequireFilteredSingleSource = false;
		bool bRequireFilterExhausted = false;
		bool bRequirePostFilterMultihit = false;
		bool bRequireTrueDivergentGap = false;
		bool bRequireDivergenceRejected = false;
		bool bRequirePolaritySwap = false;
		bool bRequireOceanOceanAmbiguous = false;
		bool bRequireContinentalCollisionCandidate = false;
		bool bRequireThirdPlateIntrusion = false;
		bool bRequireResolutionInvariantLabels = false;
		bool bRequireResolvableScale = false;
		bool bRequireOnlyDivergentContacts = false;
		bool bRequireOnlyConvergentContacts = false;
		bool bRequireMixedContactSigns = false;
		bool bRequireAutoOceanContinentContact = false;
		bool bRequireScalePumpSafety = false;
		int32 ExpectedMinimumContacts = 0;
		int32 ExpectedMinimumConvergentContacts = 0;
		int32 ExpectedMinimumDivergentContacts = 0;
		int32 ExpectedMinimumOceanContinentContacts = 0;
		int32 ExpectedMinimumFilterLabels = 0;
		int32 ExpectedMinimumFilteredSingleSource = 0;
		int32 ExpectedMinimumDivergentGapFill = 0;
		int32 ExpectedMaximumHoleCountGrowth = 0;
		double OpeningRateTolerance = 1.0e-12;
		double ContinentalFractionThreshold = 0.5;
		double PaperResampleCycleBudgetMs = 3580.0;
		double ProcessLaneBudgetMs = 260.0;
		int32 UnassignedTriangleBudget = 0;
	};

	struct FCarrierV2Milestone3Metrics
	{
		FString RunId;
		FString StageId = TEXT("M3");
		FString FixtureId;
		FString FixtureName;
		FString FixtureKind;
		FString ProcessFilterPolicyId;
		FString ContactEvidencePolicyId;
		FString PolarityPolicyId;
		FString PolarityMode;
		FString ConfigHash;
		FString PreCycleAuthorityHash;
		FString PostCycleAuthorityHash;
		FString ContactLabelHash;
		FString ResampleDecisionHash;
		FString RebuiltTopologyHash;
		FString MetricsHash;
		int32 GlobalSampleCount = 0;
		int32 GlobalTriangleCount = 0;
		int32 PlateCount = 0;
		int32 LifecycleWindowCount = 0;
		double MaxPlateMotionAngleRad = 0.0;
		int32 PinnedM2BaselineComparedCount = 0;
		int32 PinnedM2BaselineMismatchCount = 0;
		int32 ContactEvidenceCount = 0;
		int32 ConvergentContactCount = 0;
		int32 DivergentContactCount = 0;
		int32 TransformLowMarginContactCount = 0;
		int32 ThirdPlateContactCount = 0;
		int32 SignedOpeningRateSampleCount = 0;
		double SignedOpeningRateMin = 0.0;
		double SignedOpeningRateMax = 0.0;
		double SignedOpeningRateMean = 0.0;
		int32 OceanContinentContactCount = 0;
		int32 OceanOceanAmbiguousContactCount = 0;
		int32 ContinentalCollisionCandidateCount = 0;
		int32 SubductingTriangleLabelCount = 0;
		int32 CollidingTriangleLabelCount = 0;
		int32 FilterActiveTriangleLabelCount = 0;
		int32 PostResetTriangleLabelCount = 0;
		int64 AabbRayQueryCount = 0;
		int64 RawHitCountTotal = 0;
		int32 FilteredSubductingHitCount = 0;
		int32 FilteredCollidingHitCount = 0;
		int32 ValidSingleHitWriteCount = 0;
		int32 FilteredSingleSourceWriteCount = 0;
		int32 FilterExhaustedSampleCount = 0;
		int32 PostFilterUnresolvedMultihitCount = 0;
		int32 Q1Q2BoundaryQueryCount = 0;
		int32 Q1Q2BoundaryPairCount = 0;
		int32 Q1Q2DivergentAcceptedCount = 0;
		int32 Q1Q2DivergenceRejectedCount = 0;
		int32 Q1Q2RejectedByOpeningRateCount = 0;
		int32 Q1Q2RejectedByProcessFilterCount = 0;
		int32 Q1Q2RejectedBySamePlateCount = 0;
		int32 Q1Q2GapFillCount = 0;
		int32 GeneratedOceanicCount = 0;
		int32 GapFillNoBoundaryPairCount = 0;
		int32 PreviouslyBlockedSampleCount = 0;
		int32 PreviouslyBlockedBecameQ1Q2OceanicCount = 0;
		int32 PreviouslyBlockedQ1Q2OceanicNonOpeningCount = 0;
		int32 DeferredOverlapSampleCount = 0;
		int32 DeferredNondivergentGapCount = 0;
		int32 HoleCountWindow0 = 0;
		int32 HoleCountFinal = 0;
		int32 HoleCountGrowth = 0;
		int32 RebuiltPlateCount = 0;
		int32 RebuiltLocalVertexCountSum = 0;
		int32 RebuiltLocalTriangleCountSum = 0;
		int32 RebuiltTriangleAssignmentCount = 0;
		int32 MixedVertexTriangleCount = 0;
		int32 MajorityTriangleAssignmentCount = 0;
		int32 ThreeWayTriangleAssignmentCount = 0;
		int32 UnassignedTriangleCount = 0;
		int32 UnassignedTriangleBudget = 0;
		int32 UnsupportedOverlapWriteAttemptCount = 0;
		int32 PriorOwnerReadCount = 0;
		int32 PriorOwnerFallbackCount = 0;
		int32 GlobalOwnerReadCount = 0;
		int32 CentroidPrimaryResolutionCount = 0;
		int32 RandomPrimaryResolutionCount = 0;
		int32 NearestPrimaryResolutionCount = 0;
		int32 OwnershipRepairDuringResampleCount = 0;
		int32 RetentionHysteresisAnchorCount = 0;
		int32 Q1Q2DiscreteApproxCount = 0;
		int32 Q1Q2PriorOwnerLookupCount = 0;
		int32 TerrainBeautyMutationCount = 0;
		int32 SubductionMaterialMutationCount = 0;
		int32 CollisionMaterialMutationCount = 0;
		int32 RiftingMutationCount = 0;
		int32 TopologyRebuildCount = 0;
		int32 ProcessStateResetCount = 0;
		int32 RemeshWindowCount = 0;
		bool bPinnedM2BaselinePass = true;
		bool bFilterInertNoopPass = true;
		bool bContactEvidencePass = true;
		bool bSignedContactDirectionPass = true;
		bool bProcessFilterEvidencePass = true;
		bool bHolePumpTripwirePass = true;
		bool bScalePumpSafetyPass = true;
		bool bQ1Q2DivergencePass = true;
		bool bOverlapFilterPolicyPass = true;
		bool bTopologyBudgetPass = true;
		bool bNoForbiddenFallbackPass = false;
		bool bPerformanceBudgetPass = true;
		bool bResolutionInvariantLabelPass = true;
		bool bReplayDeterministic = false;
		bool bFixturePass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("FAIL_IMPLEMENTATION");
		FString ReplayPostCycleAuthorityHash;
		FString ReplayContactLabelHash;
		FString ReplayResampleDecisionHash;
		FString ReplayRebuiltTopologyHash;
		FString ReplayMetricsHash;
		double BuildSubstrateMs = 0.0;
		double BuildPlateLocalMs = 0.0;
		double MotionApplyMs = 0.0;
		double AabbBuildMs = 0.0;
		double ContactEvidenceMs = 0.0;
		double ResampleFilterMs = 0.0;
		double TopologyRebuildMs = 0.0;
		double MetricsMs = 0.0;
		double ProcessLaneMs = 0.0;
		double FullCarrierCycleMs = 0.0;
		double TotalMs = 0.0;
		double PeakMemoryMb = 0.0;
	};

	struct FCarrierV2Milestone3FixtureResult
	{
		FCarrierV2Milestone3Config Config;
		FCarrierV2Milestone3Metrics Metrics;
		TArray<FCarrierV2Milestone3ContactRecord> Contacts;
		TArray<FCarrierV2Milestone3TriangleLabelRecord> TriangleLabels;
		TArray<FCarrierV2Milestone3SampleRecord> SampleRecords;
		TArray<FCarrierV2Plate> RebuiltPlates;
		bool bCompleted = false;
		FString Error;
	};

	struct FCarrierV2Milestone3SuiteResult
	{
		TArray<FCarrierV2Milestone3FixtureResult> Results;
		bool bMicroGatePass = false;
		bool bPinnedM2BaselinePass = false;
		bool bScale50kPass = false;
		bool bAttempted250k = false;
		bool bScale250kPass = false;
		bool bAttempted500k = false;
		bool bScale500kPass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("REVISE_MILESTONE_3");
		FString OutputRoot;
		FString MetricsPath;
		FString ReportPath;
		FString NotAttempted500kReason;
	};

	struct FCarrierV2Milestone4PinnedM3Baseline
	{
		FString FixtureId;
		FString ExpectedPostCycleAuthorityHash;
		FString ExpectedContactLabelHash;
		FString ExpectedResampleDecisionHash;
		FString ExpectedRebuiltTopologyHash;
	};

	struct FCarrierV2Milestone4CrustFieldRecord
	{
		int32 PlateId = INDEX_NONE;
		int32 LocalVertexId = INDEX_NONE;
		int32 SourceSampleId = INDEX_NONE;
		double ElevationKm = 0.0;
		double OceanicAgeMa = 0.0;
		bool bOceanicAgeValid = false;
		FVector3d RidgeDirection = FVector3d::ZeroVector;
		FVector3d FoldDirection = FVector3d::ZeroVector;
		bool bRidgeDirectionValid = false;
		bool bFoldDirectionValid = false;
		FString Provenance = TEXT("unset");
	};

	struct FCarrierV2Milestone4SampleRecord
	{
		int32 SampleId = INDEX_NONE;
		int32 RawHitCount = 0;
		int32 ValidHitCount = 0;
		int32 AssignedPlateId = INDEX_NONE;
		bool bSingleHitWritten = false;
		bool bGeneratedOceanic = false;
		bool bDeferred = false;
		bool bPreviouslyBlockedSample = false;
		bool bPreviouslyBlockedBecameQ1Q2Oceanic = false;
		bool bDangerousNonOpeningQ1Q2Oceanic = false;
		double Q1Q2OpeningRate = 0.0;
		double RidgeDirectionAngularResidualDeg = 0.0;
		double ScalarOracleResidual = 0.0;
		double VectorTangentResidual = 0.0;
		double VectorNormResidual = 0.0;
		FString SelectionProvenance = TEXT("unset");
	};

	struct FCarrierV2Milestone4TrackingRecord
	{
		int32 WindowIndex = 0;
		int32 StepIndex = 0;
		int32 PlateA = INDEX_NONE;
		int32 PlateB = INDEX_NONE;
		int32 LocalTriangleA = INDEX_NONE;
		int32 LocalTriangleB = INDEX_NONE;
		uint64 FrontKey = 0;
		double SignedOpeningRate = 0.0;
		double StepDistanceKm = 0.0;
		double AccumulatedDistanceKm = 0.0;
		double OracleDistanceKm = 0.0;
		FString TrackingClass = TEXT("unset");
	};

	struct FCarrierV2Milestone4Config
	{
		FCarrierV2Milestone3Config ProcessConfig;
		FString FixtureId = TEXT("M4-FX-001");
		FString FixtureName = TEXT("PinnedM3RBaseline");
		FString FieldStoragePolicyId = TEXT("plate_local_m4_crust_field_layer");
		FString TrackingPolicyId = TEXT("per_step_rederived_fronts_no_persistent_ids");
		FString AgePolarityPolicyId = TEXT("ocean_ocean_older_age_subducts_equal_defers");
		TArray<FCarrierV2Milestone4PinnedM3Baseline> PinnedM3Baselines;
		bool bRunPinnedM3BaselinesOnly = false;
		bool bRunM3RCharacterizationOnly = false;
		bool bEnableFieldStorage = true;
		bool bUseNeutralFields = false;
		bool bScaleCharacterization = false;
		bool bPaperRegimeCharacterization = false;
		bool bRequirePinnedM3Baseline = false;
		bool bRequireFieldInertNoop = false;
		bool bRequireScalarTransfer = false;
		bool bRequireVectorRotation = false;
		bool bRequireVectorTangent = false;
		bool bRequireQ1Q2OceanicFields = false;
		bool bRequireOceanOceanAgePolarity = false;
		bool bRequireOceanOceanEqualAgeDeferral = false;
		bool bRequireOnlyDivergentContacts = false;
		bool bRequireMixedSignalSamePair = false;
		bool bRequireDistanceOracle = false;
		bool bRequireTrackingReset = false;
		bool bRequireFrontContinuityNoIds = false;
		bool bRequireScaleFieldCycle = false;
		bool bRequirePaperRegimeCharacterization = false;
		int32 ExpectedMinimumFieldTransfers = 0;
		int32 ExpectedMinimumGeneratedOceanicFields = 0;
		int32 ExpectedMinimumOceanOceanAgeContacts = 0;
		int32 ExpectedMinimumEqualAgeDeferrals = 0;
		int32 ExpectedMinimumDivergentContacts = 0;
		int32 ExpectedMinimumMixedSignalPairs = 0;
		int32 ExpectedMinimumActiveFronts = 0;
		int32 ExpectedMinimumTrackingResets = 0;
		double ScalarOracleTolerance = 1.0e-9;
		double VectorOracleTolerance = 1.0e-9;
		double RidgeDirectionToleranceDeg = 1.0e-4;
		double TangentTolerance = 1.0e-9;
		double DistanceOracleToleranceKm = 1.0e-6;
		double MinimumFrontContinuityRatio = 0.75;
		double PaperResampleCycleBudgetMs = 3580.0;
		double ProcessTrackingBudgetMs = 260.0;
	};

	struct FCarrierV2Milestone4Metrics
	{
		FString RunId;
		FString StageId = TEXT("M4");
		FString FixtureId;
		FString FixtureName;
		FString FixtureKind;
		FString FieldStoragePolicyId;
		FString TrackingPolicyId;
		FString AgePolarityPolicyId;
		FString ConfigHash;
		FString M3RBaselineHash;
		FString PreFieldAuthorityHash;
		FString PostFieldAuthorityHash;
		FString VectorFieldHash;
		FString ProcessTrackingHash;
		FString ResampleFieldHash;
		FString RebuiltTopologyHash;
		FString MetricsHash;
		FString ReplayPostFieldAuthorityHash;
		FString ReplayVectorFieldHash;
		FString ReplayProcessTrackingHash;
		FString ReplayResampleFieldHash;
		FString ReplayRebuiltTopologyHash;
		int32 GlobalSampleCount = 0;
		int32 GlobalTriangleCount = 0;
		int32 PlateCount = 0;
		int32 LifecycleWindowCount = 0;
		int32 MotionStepCount = 0;
		double DtMa = 0.0;
		double TotalMotionMa = 0.0;
		double V0MmPerYr = 100.0;
		double VmMmPerYr = 0.0;
		double CadenceAlpha = 0.0;
		double CadenceDeltaT = 0.0;
		int32 PinnedM3BaselineComparedCount = 0;
		int32 PinnedM3BaselineMismatchCount = 0;
		int32 FieldVertexCount = 0;
		int32 FieldTransferSingleSourceCount = 0;
		int32 FieldTransferQ1Q2Count = 0;
		int32 FieldTransferDeferredCount = 0;
		int32 FieldTransferFilterExhaustedCount = 0;
		int32 FieldTransferUnresolvedCount = 0;
		double ElevationScalarResidualMax = 0.0;
		double OceanicAgeScalarResidualMax = 0.0;
		double AgeAdvanceTotalMa = 0.0;
		int32 Q1Q2AgeResetCount = 0;
		int32 InvalidAgeCount = 0;
		int32 OceanOceanAgePolarityContactCount = 0;
		int32 OceanOceanOlderSubductingLabelCount = 0;
		int32 OceanOceanYoungerSubductingLabelCount = 0;
		int32 OceanOceanEqualAgeDeferralCount = 0;
		int32 OceanContinentContactCount = 0;
		int32 ContinentalCollisionCandidateCount = 0;
		int32 ThirdPlateContactCount = 0;
		int32 ConvergentContactCount = 0;
		int32 DivergentContactCount = 0;
		int32 TransformLowMarginContactCount = 0;
		int32 MixedSignalSamePairCount = 0;
		double SignedOpeningRateMin = 0.0;
		double SignedOpeningRateMax = 0.0;
		double VectorRotationResidualMax = 0.0;
		double VectorTangentResidualMax = 0.0;
		double VectorNormResidualMax = 0.0;
		double Q1Q2RidgeDirectionResidualDegMax = 0.0;
		int32 RawHitCountTotal = 0;
		int32 ValidSingleHitWriteCount = 0;
		int32 GeneratedOceanicCount = 0;
		int32 Q1Q2DivergentAcceptedCount = 0;
		int32 Q1Q2RejectedByOpeningRateCount = 0;
		int32 Q1Q2RejectedByProcessFilterCount = 0;
		int32 Q1Q2RejectedBySamePlateCount = 0;
		int32 PreviouslyBlockedSampleCount = 0;
		int32 PreviouslyBlockedBecameQ1Q2OceanicCount = 0;
		int32 DangerousNonOpeningQ1Q2OceanicCount = 0;
		int32 HoleCountWindow0 = 0;
		int32 HoleCountFinal = 0;
		int32 HoleCountGrowth = 0;
		int32 UnassignedTriangleCount = 0;
		int32 UnassignedTriangleBudget = 0;
		int32 ActiveFrontCount = 0;
		int32 FrontBirthCount = 0;
		int32 FrontRetirementCount = 0;
		int32 TrackingResetCount = 0;
		int32 PersistentFrontIdStoreCount = 0;
		int32 FrontContinuityMatchedCount = 0;
		int32 FrontContinuityCandidateCount = 0;
		double FrontContinuityRatio = 0.0;
		double DistanceMinKm = 0.0;
		double DistanceMeanKm = 0.0;
		double DistanceMaxKm = 0.0;
		double DistanceOracleResidualKmMax = 0.0;
		double SubductionMatrixDensity = 0.0;
		int32 SubductionMatrixEntryCount = 0;
		int32 SubductionMatrixResetCount = 0;
		int32 PriorOwnerFallbackCount = 0;
		int32 GlobalOwnerReadCount = 0;
		int32 CentroidPrimaryResolutionCount = 0;
		int32 RandomPrimaryResolutionCount = 0;
		int32 NearestPrimaryResolutionCount = 0;
		int32 OwnershipRepairDuringResampleCount = 0;
		int32 TerrainBeautyMutationCount = 0;
		int32 SubductionMaterialMutationCount = 0;
		int32 CollisionMaterialMutationCount = 0;
		double BuildSubstrateMs = 0.0;
		double BuildPlateLocalMs = 0.0;
		double AabbBuildMs = 0.0;
		double MotionApplyMs = 0.0;
		double VectorRotationMs = 0.0;
		double TrackingMs = 0.0;
		double ContactProcessMs = 0.0;
		double ResampleFieldMs = 0.0;
		double TopologyRebuildMs = 0.0;
		double DiagnosticsMs = 0.0;
		double FullCarrierCycleMs = 0.0;
		double TotalMs = 0.0;
		double PeakMemoryMb = 0.0;
		bool bPinnedM3BaselinePass = false;
		bool bFieldInertNoopPass = true;
		bool bScalarTransferPass = true;
		bool bVectorRotationPass = true;
		bool bVectorTangentPass = true;
		bool bQ1Q2OceanicFieldPass = true;
		bool bOceanOceanAgePolarityPass = true;
		bool bOceanOceanEqualAgeDeferralPass = true;
		bool bSignedContactDirectionPass = true;
		bool bMixedSignalSamePairPass = true;
		bool bDistanceOraclePass = true;
		bool bTrackingResetPass = true;
		bool bFrontContinuityNoIdsPass = true;
		bool bPaperRegimeCharacterizationPass = true;
		bool bScaleFieldCyclePass = true;
		bool bDangerousPumpAuditPass = true;
		bool bTopologyBudgetPass = true;
		bool bHoleGrowthBudgetPass = true;
		bool bNoForbiddenFallbackPass = true;
		bool bPerformanceBudgetPass = true;
		bool bReplayDeterministic = false;
		bool bFixturePass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("REVISE_MILESTONE_4_FIXTURE");
	};

	struct FCarrierV2Milestone4FixtureResult
	{
		FCarrierV2Milestone4Config Config;
		FCarrierV2Milestone4Metrics Metrics;
		TArray<FCarrierV2Milestone4CrustFieldRecord> FieldRecords;
		TArray<FCarrierV2Milestone4SampleRecord> SampleRecords;
		TArray<FCarrierV2Milestone4TrackingRecord> TrackingRecords;
		TArray<FCarrierV2Milestone3ContactRecord> Contacts;
		TArray<FCarrierV2Milestone3TriangleLabelRecord> TriangleLabels;
		TArray<FCarrierV2Plate> RebuiltPlates;
		bool bCompleted = false;
		FString Error;
	};

	struct FCarrierV2Milestone4SuiteResult
	{
		TArray<FCarrierV2Milestone4FixtureResult> Results;
		bool bMicroGatePass = false;
		bool bPinnedM3BaselinePass = false;
		bool bPaperRegimeCharacterizationPass = false;
		bool bScale50kPass = false;
		bool bAttempted100k = false;
		bool bScale100kPass = false;
		bool bAttempted250k = false;
		bool bScale250kPass = false;
		bool bAttempted500k = false;
		bool bScale500kPass = false;
		bool bStageGatePass = false;
		FString Verdict = TEXT("REVISE_MILESTONE_4");
		FString OutputRoot;
		FString MetricsPath;
		FString ReportPath;
		FString NotAttempted100kReason;
		FString NotAttempted500kReason;
	};

	struct FCarrierV2FoundationStepperSampleVisual
	{
		int32 SampleId = INDEX_NONE;
		FVector3d UnitPosition = FVector3d::ZeroVector;
		int32 ColdStartPlateId = INDEX_NONE;
		int32 AssignedPlateId = INDEX_NONE;
		int32 RawHitCount = 0;
		int32 FilteredSubductingHitCount = 0;
		int32 FilteredCollidingHitCount = 0;
		int32 ValidHitCount = 0;
		double ContinentalFraction = 0.0;
		bool bZeroValidHit = false;
		bool bGeneratedOceanic = false;
		bool bBoundaryPairFound = false;
		bool bQ1Q2DifferentPlates = false;
		bool bProcessMarked = false;
		bool bSubductingProcessMarked = false;
		bool bCollidingProcessMarked = false;
		bool bPostResetProcessMarked = false;
		FString SelectionProvenance = TEXT("unset");
	};

	struct FCarrierV2FoundationStepperTriangleVisual
	{
		int32 TriangleId = INDEX_NONE;
		int32 SampleIds[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
		int32 ColdStartPlateId = INDEX_NONE;
		int32 RebuiltPlateId = INDEX_NONE;
		bool bProcessMarked = false;
		bool bSubductingProcessMarked = false;
		bool bCollidingProcessMarked = false;
		bool bPostResetProcessMarked = false;
		bool bMixedVertexAssignment = false;
		bool bUnassignedAfterRebuild = false;
	};

	struct FCarrierV2FoundationStepperSnapshot
	{
		FCarrierV2Stage5Config Config;
		FCarrierV2Stage5FixtureResult Stage5Result;
		TArray<FCarrierV2FoundationStepperSampleVisual> Samples;
		TArray<FCarrierV2FoundationStepperTriangleVisual> Triangles;
		FString Summary;
		bool bCompleted = false;
		bool bFixturePass = false;
		bool bReplayDeterministic = false;
		bool bForbiddenFallbackDetected = false;
		FString Error;
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

	class CARRIERLAB_API FCarrierV2Stage1
	{
	public:
		static TArray<FCarrierV2Stage1Config> MakeMicroFixtureConfigs();
		static FCarrierV2Stage1Config MakeScaleConfig(int32 SampleCount, bool bComparisonScale);
		static bool RunFixtureWithReplay(const FCarrierV2Stage1Config& Config, FCarrierV2Stage1FixtureResult& OutResult);
		static FString MetricsToJson(const FCarrierV2Stage1FixtureResult& Result);
		static FString BuildCheckpointReport(
			const FCarrierV2Stage1SuiteResult& Suite,
			const FString& CommandLine,
			const FString& CommitSha);
	};

	class CARRIERLAB_API FCarrierV2Stage2
	{
	public:
		static TArray<FCarrierV2Stage2Config> MakeMicroFixtureConfigs();
		static FCarrierV2Stage2Config MakeScaleConfig(int32 SampleCount, bool bComparisonScale);
		static bool RunFixtureWithReplay(const FCarrierV2Stage2Config& Config, FCarrierV2Stage2FixtureResult& OutResult);
		static FString MetricsToJson(const FCarrierV2Stage2FixtureResult& Result);
		static FString BuildCheckpointReport(
			const FCarrierV2Stage2SuiteResult& Suite,
			const FString& CommandLine,
			const FString& CommitSha);
	};

	class CARRIERLAB_API FCarrierV2Stage3
	{
	public:
		static TArray<FCarrierV2Stage3Config> MakeMicroFixtureConfigs();
		static FCarrierV2Stage3Config MakeScaleConfig(int32 SampleCount, bool bComparisonScale);
		static bool RunFixtureWithReplay(const FCarrierV2Stage3Config& Config, FCarrierV2Stage3FixtureResult& OutResult);
		static FString MetricsToJson(const FCarrierV2Stage3FixtureResult& Result);
		static FString BuildCheckpointReport(
			const FCarrierV2Stage3SuiteResult& Suite,
			const FString& CommandLine,
			const FString& CommitSha);
	};

	class CARRIERLAB_API FCarrierV2Stage4
	{
	public:
		static TArray<FCarrierV2Stage4Config> MakeMicroFixtureConfigs();
		static FCarrierV2Stage4Config MakeScaleConfig(int32 SampleCount, bool bComparisonScale);
		static bool RunFixtureWithReplay(const FCarrierV2Stage4Config& Config, FCarrierV2Stage4FixtureResult& OutResult);
		static FString MetricsToJson(const FCarrierV2Stage4FixtureResult& Result);
		static FString BuildCheckpointReport(
			const FCarrierV2Stage4SuiteResult& Suite,
			const FString& CommandLine,
			const FString& CommitSha);
	};

	class CARRIERLAB_API FCarrierV2Stage5
	{
	public:
		static TArray<FCarrierV2Stage5Config> MakeMicroFixtureConfigs();
		static FCarrierV2Stage5Config MakeScaleConfig(int32 SampleCount, bool bComparisonScale);
		static bool RunFixtureWithReplay(const FCarrierV2Stage5Config& Config, FCarrierV2Stage5FixtureResult& OutResult);
		static FString MetricsToJson(const FCarrierV2Stage5FixtureResult& Result);
		static FString BuildCheckpointReport(
			const FCarrierV2Stage5SuiteResult& Suite,
			const FString& CommandLine,
			const FString& CommitSha);
	};

	class CARRIERLAB_API FCarrierV2Milestone2
	{
	public:
		static TArray<FCarrierV2Milestone2Config> MakeMicroFixtureConfigs();
		static FCarrierV2Milestone2Config MakeScaleConfig(int32 SampleCount, bool bComparisonScale);
		static bool RunFixtureWithReplay(const FCarrierV2Milestone2Config& Config, FCarrierV2Milestone2FixtureResult& OutResult);
		static FString MetricsToJson(const FCarrierV2Milestone2FixtureResult& Result);
		static FString BuildCheckpointReport(
			const FCarrierV2Milestone2SuiteResult& Suite,
			const FString& CommandLine,
			const FString& CommitSha);
	};

	class CARRIERLAB_API FCarrierV2Milestone3
	{
	public:
		static TArray<FCarrierV2Milestone3Config> MakeMicroFixtureConfigs();
		static FCarrierV2Milestone3Config MakeScaleConfig(int32 SampleCount, bool bComparisonScale);
		static bool RunFixtureWithReplay(const FCarrierV2Milestone3Config& Config, FCarrierV2Milestone3FixtureResult& OutResult);
		static FString MetricsToJson(const FCarrierV2Milestone3FixtureResult& Result);
		static FString BuildCheckpointReport(
			const FCarrierV2Milestone3SuiteResult& Suite,
			const FString& CommandLine,
			const FString& CommitSha);
	};

	class CARRIERLAB_API FCarrierV2Milestone4
	{
	public:
		static TArray<FCarrierV2Milestone4Config> MakeMicroFixtureConfigs();
		static FCarrierV2Milestone4Config MakePaperRegimeCharacterizationConfig();
		static FCarrierV2Milestone4Config MakeScaleConfig(int32 SampleCount, int32 LifecycleWindowCount, bool bComparisonScale);
		static bool RunFixtureWithReplay(const FCarrierV2Milestone4Config& Config, FCarrierV2Milestone4FixtureResult& OutResult);
		static FString MetricsToJson(const FCarrierV2Milestone4FixtureResult& Result);
		static FString BuildCheckpointReport(
			const FCarrierV2Milestone4SuiteResult& Suite,
			const FString& CommandLine,
			const FString& CommitSha);
	};

	class CARRIERLAB_API FCarrierV2FoundationStepper
	{
	public:
		static bool BuildSnapshot(
			const FCarrierV2Stage5Config& Config,
			FCarrierV2FoundationStepperSnapshot& OutSnapshot);
	};
}
