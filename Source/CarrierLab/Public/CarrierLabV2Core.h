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

	class CARRIERLAB_API FCarrierV2FoundationStepper
	{
	public:
		static bool BuildSnapshot(
			const FCarrierV2Stage5Config& Config,
			FCarrierV2FoundationStepperSnapshot& OutSnapshot);
	};
}
