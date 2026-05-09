// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CarrierLabCarrier.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "GameFramework/Actor.h"
#include "CarrierLabVisualizationActor.generated.h"

class UDynamicMeshComponent;

UENUM(BlueprintType)
enum class ECarrierLabVisualizationLayer : uint8
{
	PlateId UMETA(DisplayName = "Plate Id"),
	ContinentalFraction UMETA(DisplayName = "Continental Fraction"),
	MissMask UMETA(DisplayName = "Miss Mask"),
	OverlapMask UMETA(DisplayName = "Overlap Mask"),
	BoundaryMask UMETA(DisplayName = "Boundary Mask"),
	DriftError UMETA(DisplayName = "Drift Error"),
	PhaseIIISummary UMETA(DisplayName = "Phase III Summary"),
	ElevationHeatmap UMETA(DisplayName = "Elevation Heatmap"),
	SubductionMask UMETA(DisplayName = "Subduction Mask"),
	DistanceToFrontHeatmap UMETA(DisplayName = "Distance To Front Heatmap"),
	OceanicAgeHeatmap UMETA(DisplayName = "Oceanic Age Heatmap"),
	RidgeDirection UMETA(DisplayName = "Ridge Direction"),
	PhaseIIIERemeshSummary UMETA(DisplayName = "Phase IIIE Remesh Summary")
};

UENUM(BlueprintType)
enum class ECarrierLabMultiHitPolicy : uint8
{
	Centroid UMETA(DisplayName = "Centroid"),
	SyntheticSubduction UMETA(DisplayName = "Synthetic Subduction"),
	RandomSeeded UMETA(DisplayName = "Random Seeded")
};

USTRUCT(BlueprintType)
struct FCarrierLabVisualizationMetrics
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 Step = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 EventCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 NextResampleStep = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 CadenceSteps = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 SampleCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 PlateCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 RawMissCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 RawMultiHitCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 BoundaryHitCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 BoundaryVertexCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 NaNOrInfCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 LastGapFillCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 LastNonSeparatingGapFillCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 LastNoBoundaryPairMissCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase IIIE")
	int32 PhaseIIIELastGeneratedWithNonPositiveSeparationCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 PolicyResolvedMultiHitCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase IIIE")
	int32 PhaseIIIELastGeneratedCandidateCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase IIIE")
	int32 PhaseIIIELastAppliedGeneratedCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase IIIE")
	int32 PhaseIIIELastRiftingPendingCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase IIIE")
	int32 PhaseIIIELastInvalidRecordCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase IIIE")
	int32 PhaseIIIELastUnresolvedMultiHitHoldCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase IIIE")
	int32 PhaseIIIELastCoalescedMultiHitCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase IIIE")
	int32 PhaseIIIELastSharedBoundaryTieBreakCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase IIIE")
	int32 PhaseIIIELastNearestHitTieBreakCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase IIIE")
	int32 PhaseIIIELastDistanceTieFallbackCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase IIIE")
	int32 PhaseIIIELastWithinPlateCoincidentHoldCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase IIIE")
	int32 PhaseIIIELastCrossPlateEqualHoldCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase IIIE")
	int32 PhaseIIIELastThirdPlateHoldCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase IIIE")
	int32 PhaseIIIELastTripleJunctionSplitCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double AuthoritativeCAF = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double ProjectedCAF = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double DriftErrorMeanKm = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double DriftErrorP95Km = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double ProjectionSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double BvhBuildSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double ProjectionQuerySeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double DriftMetricsSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double BoundaryMaskSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double HashSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double ResampleEventSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double ObservedMaxPlateSpeedMmPerYear = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double ObservedMaxPlateSpeedSinceLastRemeshMmPerYear = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double CadenceDeltaTMa = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double MeshUpdateSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	FString LastHash;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	FString StateHash;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	FString CrustStateHash;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	FString VisibleElevationHash;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	FString HistoricalElevationHash;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	FString ConvergenceTrackingHash;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	FString LastRemeshMode;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	int32 PhaseIIIActiveBoundaryTriangleCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	int32 PhaseIIIDistanceToFrontRecordCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	int32 PhaseIIISubductionMatrixPairCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	int32 PhaseIIISubductionMatrixEvidenceCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	int32 PhaseIIISubductionHitCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	int32 PhaseIIISubductingMarkCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	int32 PhaseIIIObductionMarkCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	int32 PhaseIIICollisionPendingTriangleCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	int32 PhaseIIIHistoricalElevationSampleCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	int32 PhaseIIIOceanicSampleCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	int32 PhaseIIIRidgeDirectionSampleCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	int32 PhaseIIIFoldDirectionSampleCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	int32 PhaseIIIConvergenceResetSerial = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	double PhaseIIIMinVisibleElevationKm = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	double PhaseIIIMaxVisibleElevationKm = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics|Phase III")
	double PhaseIIIMaxOceanicAgeMa = 0.0;
};

struct FCarrierLabPhaseIIICostDriverStepAudit
{
	int32 StepBefore = 0;
	int32 StepAfter = 0;
	int32 SampleCount = 0;
	int32 PlateCount = 0;
	int32 ActiveTriangleCountBefore = 0;
	int32 ActiveTriangleCountAfter = 0;
	int32 MatrixRayTestCount = 0;
	int32 MatrixHitCount = 0;
	int32 MatrixEvidenceCount = 0;
	int32 PolarityDecisionCount = 0;
	int32 NeighborSeedCount = 0;
	int32 NeighborAddedCount = 0;
	int32 SubductingMarkCount = 0;
	int32 UpliftRecordCount = 0;
	int32 SlabPullContributionCount = 0;
	double MotionSeconds = 0.0;
	double DistanceUpdateSeconds = 0.0;
	double MatrixTotalSeconds = 0.0;
	double MatrixBvhBuildSeconds = 0.0;
	double MatrixRayQuerySeconds = 0.0;
	double PolaritySeconds = 0.0;
	double NeighborPropagationSeconds = 0.0;
	double LedgerSeconds = 0.0;
	double MarkSeconds = 0.0;
	double UpliftSeconds = 0.0;
	double SlabPullSeconds = 0.0;
	double TotalProcessSeconds = 0.0;
};

struct FCarrierLabPhaseIIIDiagnosticCallCounts
{
	int32 DetectTerranes = 0;
	int32 GroupCollisions = 0;
	int32 DestinationMass = 0;
	int32 SlabBreakPlan = 0;
	int32 SuturePlan = 0;
	int32 TopologyMutation = 0;
	int32 UpliftPlan = 0;
	int32 UpliftApply = 0;
	int32 D1SortedHitCount = 0;
	int32 D1CollisionCandidateHitCount = 0;
	int32 D1ComponentBuildCount = 0;
	int32 D1ComponentCacheHitCount = 0;
	int32 D1ExpandedContinentalTriangleCount = 0;
	int32 D1ScannedOceanicTriangleCount = 0;
	int32 D1InnerSeaTriangleCount = 0;
	int32 D1RecordCount = 0;
	int32 D7PlannedRecordCount = 0;
	int32 D7AppliedRecordCount = 0;
	int32 D7TopologyMutationAppliedCount = 0;
	int32 D7NoUpliftAvailableCount = 0;
	int32 D3DestinationComponentBuildCount = 0;
	int32 D3DestinationComponentCacheHitCount = 0;
	double D1DecisionIndexSeconds = 0.0;
	double D1HitSortSeconds = 0.0;
	double D1HitClassificationSeconds = 0.0;
	double D1ComponentExpansionSeconds = 0.0;
	double D1InnerSeaScanSeconds = 0.0;
	double D1RecordConstructionSeconds = 0.0;
	double D1AuditHashSeconds = 0.0;
	double D7ApplyTotalSeconds = 0.0;
	double D7InputPipelineSeconds = 0.0;
	double D7D2GroupingSeconds = 0.0;
	double D7D3DestinationMassSeconds = 0.0;
	double D7D4SlabBreakSeconds = 0.0;
	double D7D5SutureSeconds = 0.0;
	double D3DestinationComponentExpansionSeconds = 0.0;
	double D7UpliftPlanSeconds = 0.0;
	double D7ApplyFromPlanSeconds = 0.0;
	double D7TopologyMutationSeconds = 0.0;
	double D7RecordApplySeconds = 0.0;
	double D7ProjectionRefreshSeconds = 0.0;
	double D7ApplyHashSeconds = 0.0;
};

struct FCarrierLabVisualizationMotion
{
	FVector3d Axis = FVector3d::UnitZ();
	FVector3d CurrentCenter = FVector3d::UnitZ();
	double AngularSpeedRadiansPerStep = 0.0;
};

struct FCarrierLabVizPlateMesh
{
	UE::Geometry::FDynamicMesh3 Mesh;
	TUniquePtr<UE::Geometry::FDynamicMeshAABBTree3> Tree;
	TMap<int32, int32> MeshTriangleIdToLocalTriangleId;
	int32 PlateId = INDEX_NONE;
};

struct FCarrierLabVizProjectionTriangleRef
{
	int32 PlateId = INDEX_NONE;
	int32 LocalTriangleId = INDEX_NONE;
};

struct FCarrierLabVizProjectionMesh
{
	UE::Geometry::FDynamicMesh3 Mesh;
	TUniquePtr<UE::Geometry::FDynamicMeshAABBTree3> Tree;
	TArray<FCarrierLabVizProjectionTriangleRef> TriangleRefsByMeshTriangleId;
	TArray<int32> PlateVertexOffsets;
	int32 PlateCount = 0;
};

enum class ECarrierLabPhaseIIMotionFixture : uint8
{
	Default,
	Zero,
	ForcedConvergence,
	ForcedDivergence,
	TripleJunction
};

enum class ECarrierLabPhaseIIContactClass : uint8
{
	Divergent,
	Convergent,
	TransformLowMargin,
	ThirdPlate
};

enum class ECarrierLabPhaseIIPolaritySource : uint8
{
	None,
	MixedMaterial,
	FixtureSpecified,
	SameMaterialAmbiguous,
	ThirdPlateOutOfScope
};

enum class ECarrierLabPhaseIITriangleLabel : uint8
{
	None,
	Subducting,
	Overriding,
	CollisionCandidate,
	Ambiguous
};

struct FCarrierLabPhaseIIContactRecord
{
	int32 ContactId = INDEX_NONE;
	int32 Step = 0;
	int32 SampleId = INDEX_NONE;
	int32 PlateA = INDEX_NONE;
	int32 PlateB = INDEX_NONE;
	int32 PlateALocalTriangleId = INDEX_NONE;
	int32 PlateBLocalTriangleId = INDEX_NONE;
	FVector3d EvidenceUnitPosition = FVector3d::UnitZ();
	double SignedConvergenceVelocity = 0.0;
	double VelocityMargin = 0.0;
	bool bBoundaryEvidence = false;
	bool bThirdPlate = false;
	ECarrierLabPhaseIIContactClass ContactClass = ECarrierLabPhaseIIContactClass::TransformLowMargin;
};

struct FCarrierLabPhaseIITriangleLabelConfig
{
	bool bUseFixturePolarity = false;
	int32 FixtureUnderPlate = INDEX_NONE;
	int32 FixtureOverPlate = INDEX_NONE;
};

struct FCarrierLabPhaseIITriangleLabelRecord
{
	int32 LabelId = INDEX_NONE;
	int32 ContactId = INDEX_NONE;
	int32 Step = 0;
	int32 SampleId = INDEX_NONE;
	int32 PlateId = INDEX_NONE;
	int32 OtherPlateId = INDEX_NONE;
	int32 LocalTriangleId = INDEX_NONE;
	int32 SourceGlobalTriangleId = INDEX_NONE;
	FVector3d EvidenceUnitPosition = FVector3d::UnitZ();
	double SignedConvergenceVelocity = 0.0;
	double VelocityMargin = 0.0;
	double DistanceFromContactKm = 0.0;
	ECarrierLabPhaseIITriangleLabel Label = ECarrierLabPhaseIITriangleLabel::None;
	ECarrierLabPhaseIIPolaritySource PolaritySource = ECarrierLabPhaseIIPolaritySource::None;
	bool bFromThirdPlateContact = false;
	FString LabelReason;
};

struct FCarrierLabPhaseIIContactMetrics
{
	int32 Step = 0;
	int32 SampleCount = 0;
	int32 PlateCount = 0;
	int32 RawEvidenceSampleCount = 0;
	int32 ContactRecordCount = 0;
	int32 ConvergentContactCount = 0;
	int32 DivergentContactCount = 0;
	int32 TransformLowMarginContactCount = 0;
	int32 ThirdPlateContactCount = 0;
	int32 SubductionCandidateCount = 0;
	int32 BoundaryEvidenceCount = 0;
	int32 NaNOrInfCount = 0;
	double ContactDetectionSeconds = 0.0;
	FString ContactLogHash;
};

struct FCarrierLabPhaseIITriangleLabelMetrics
{
	int32 Step = 0;
	int32 ContactRecordCount = 0;
	int32 LabelRecordCount = 0;
	int32 UniqueLabeledTriangleCount = 0;
	int32 LabelableContactCount = 0;
	int32 FixtureSpecifiedPolarityContactCount = 0;
	int32 MixedMaterialPolarityContactCount = 0;
	int32 SameMaterialAmbiguousContactCount = 0;
	int32 ThirdPlateOutOfScopeContactCount = 0;
	int32 NonConvergentSkippedContactCount = 0;
	int32 SubductingLabelCount = 0;
	int32 OverridingLabelCount = 0;
	int32 AmbiguousLabelCount = 0;
	int32 CollisionCandidateLabelCount = 0;
	int32 LabelsFromThirdPlateContactCount = 0;
	int32 NaNOrInfCount = 0;
	int32 MaxLabelsPerContact = 0;
	double TriangleLabelSeconds = 0.0;
	double UniqueLabeledTriangleAreaProxy = 0.0;
	FString TriangleLabelHash;
};

enum class ECarrierLabPhaseIIFilterDecisionClass : uint8
{
	CandidateFiltered,
	ResolvedSingle,
	GapFill,
	UnresolvedMultiHit,
	FilterExhausted
};

enum class ECarrierLabPhaseIIMaterialEventClass : uint8
{
	Preserved,
	SingleHitTransfer,
	ConsumedBySubduction,
	OverwrittenByGapFill,
	UnresolvedSameMaterialMultiHit,
	UnresolvedTripleJunctionMultiHit,
	UnresolvedMixedMaterialMultiHit,
	FilterExhaustedUnknown,
	NumericResidual
};

enum class ECarrierLabPhaseIISourceTriangleUniformity : uint8
{
	Unknown,
	UniformOceanic,
	UniformContinental,
	Mixed
};

struct FCarrierLabPhaseIIFilterDecisionRecord
{
	int32 DecisionId = INDEX_NONE;
	int32 EventId = 0;
	int32 Step = 0;
	int32 SampleId = INDEX_NONE;
	int32 RawCandidateCount = 0;
	int32 RawPlateCount = 0;
	int32 FilteredCandidateCount = 0;
	int32 PostFilterCandidateCount = 0;
	int32 PostFilterPlateCount = 0;
	int32 ResolvedPlateId = INDEX_NONE;
	int32 FilteredPlateId = INDEX_NONE;
	int32 FilteredLocalTriangleId = INDEX_NONE;
	int32 SourceContactId = INDEX_NONE;
	int32 SourceLabelId = INDEX_NONE;
	bool bBoundaryEvidence = false;
	bool bThirdPlateEvidence = false;
	ECarrierLabPhaseIIFilterDecisionClass DecisionClass = ECarrierLabPhaseIIFilterDecisionClass::ResolvedSingle;
};

struct FCarrierLabPhaseIIMaterialRecord
{
	int32 RecordId = INDEX_NONE;
	int32 EventId = 0;
	int32 Step = 0;
	int32 SampleId = INDEX_NONE;
	int32 SourcePlateId = INDEX_NONE;
	int32 TargetPlateId = INDEX_NONE;
	int32 SourceContactId = INDEX_NONE;
	int32 SourceLabelId = INDEX_NONE;
	int32 HitPlateId = INDEX_NONE;
	int32 HitLocalTriangleId = INDEX_NONE;
	int32 HitTriangleContinentalVertexCount = INDEX_NONE;
	int32 RawPlateCount = 0;
	int32 PostFilterPlateCount = 0;
	double AreaWeight = 0.0;
	double ContinentalBefore = 0.0;
	double ContinentalAfter = 0.0;
	double ContinentalDelta = 0.0;
	double OceanicBefore = 0.0;
	double OceanicAfter = 0.0;
	double OceanicDelta = 0.0;
	bool bMaterialChanged = false;
	bool bPlateChanged = false;
	bool bThirdPlateEvidence = false;
	bool bNonSeparatingGap = false;
	ECarrierLabPhaseIIMaterialEventClass EventClass = ECarrierLabPhaseIIMaterialEventClass::Preserved;
	ECarrierLabPhaseIIFilterDecisionClass DecisionClass = ECarrierLabPhaseIIFilterDecisionClass::ResolvedSingle;
	ECarrierLabPhaseIISourceTriangleUniformity HitTriangleUniformity = ECarrierLabPhaseIISourceTriangleUniformity::Unknown;
};

struct FCarrierLabPhaseIIMaterialLedgerMetrics
{
	int32 EventId = 0;
	int32 Step = 0;
	int32 SampleCount = 0;
	int32 RecordCount = 0;
	int32 ChangedRecordCount = 0;
	int32 PlateChangedRecordCount = 0;
	int32 PreservedRecordCount = 0;
	int32 SingleHitTransferRecordCount = 0;
	int32 SubductionRecordCount = 0;
	int32 GapFillRecordCount = 0;
	int32 NonSeparatingGapFillRecordCount = 0;
	int32 UnresolvedSameMaterialRecordCount = 0;
	int32 UnresolvedTripleJunctionRecordCount = 0;
	int32 UnresolvedMixedMaterialRecordCount = 0;
	int32 FilterExhaustedRecordCount = 0;
	double TotalArea = 0.0;
	double ContinentalMassBefore = 0.0;
	double ContinentalMassAfter = 0.0;
	double OceanicMassBefore = 0.0;
	double OceanicMassAfter = 0.0;
	double LedgerContinentalDelta = 0.0;
	double LedgerOceanicDelta = 0.0;
	double ContinentalDeltaResidual = 0.0;
	double OceanicDeltaResidual = 0.0;
	double SingleHitTransferContinentalLoss = 0.0;
	double SingleHitTransferContinentalGain = 0.0;
	int32 SingleHitUniformContinentalRecordCount = 0;
	int32 SingleHitUniformOceanicRecordCount = 0;
	int32 SingleHitMixedTriangleRecordCount = 0;
	int32 SingleHitUnknownTriangleRecordCount = 0;
	double SingleHitUniformContinentalLoss = 0.0;
	double SingleHitUniformContinentalGain = 0.0;
	double SingleHitUniformOceanicLoss = 0.0;
	double SingleHitUniformOceanicGain = 0.0;
	double SingleHitMixedTriangleLoss = 0.0;
	double SingleHitMixedTriangleGain = 0.0;
	double SingleHitUnknownTriangleLoss = 0.0;
	double SingleHitUnknownTriangleGain = 0.0;
	double SubductionContinentalLoss = 0.0;
	double SubductionContinentalGain = 0.0;
	double GapFillContinentalLoss = 0.0;
	double GapFillContinentalGain = 0.0;
	double UnresolvedSameMaterialContinentalLoss = 0.0;
	double UnresolvedSameMaterialContinentalGain = 0.0;
	double UnresolvedSameMaterialContinentalDelta = 0.0;
	double UnresolvedTripleJunctionContinentalLoss = 0.0;
	double UnresolvedTripleJunctionContinentalGain = 0.0;
	double UnresolvedTripleJunctionContinentalDelta = 0.0;
	double UnresolvedMixedMaterialContinentalLoss = 0.0;
	double UnresolvedMixedMaterialContinentalGain = 0.0;
	double UnresolvedMixedMaterialContinentalDelta = 0.0;
	double FilterExhaustedContinentalLoss = 0.0;
	double FilterExhaustedContinentalGain = 0.0;
	double FilterExhaustedContinentalDelta = 0.0;
	double MaxPerPlateContinentalResidual = 0.0;
	int32 MaxPerPlateContinentalResidualPlateId = INDEX_NONE;
	FString MaterialLedgerHash;
};

struct FCarrierLabPhaseIIResamplingFilterMetrics
{
	int32 EventId = 0;
	int32 Step = 0;
	int32 SampleCount = 0;
	int32 RawMultiHitSampleCount = 0;
	int32 RawMissSampleCount = 0;
	int32 RawThirdPlateSampleCount = 0;
	int32 SubductingLabelInputCount = 0;
	int32 PersistentSubductingMarkInputCount = 0;
	int32 AmbiguousLabelInputCount = 0;
	int32 ThirdPlateLabelInputCount = 0;
	int32 FilteredCandidateCount = 0;
	int32 FilteredSampleCount = 0;
	int32 PostFilterSingleHitSampleCount = 0;
	int32 PostFilterMultiHitSampleCount = 0;
	int32 PostFilterNonBoundaryMultiHitSampleCount = 0;
	int32 UnresolvedMultiHitSampleCount = 0;
	int32 FilterExhaustedSampleCount = 0;
	int32 GapFillCount = 0;
	int32 NonSeparatingGapFillCount = 0;
	int32 NoBoundaryPairMissCount = 0;
	int32 UnexpectedFilteredPlateCount = 0;
	int32 DecisionsFromThirdPlateLabelCount = 0;
	double AuthoritativeCAFBefore = 0.0;
	double AuthoritativeCAFAfter = 0.0;
	double ProjectedCAFBefore = 0.0;
	double ProjectedCAFAfter = 0.0;
	double MaxPlateAreaDeltaPercent = 0.0;
	int32 MaxPlateAreaDeltaPlateId = INDEX_NONE;
	double FilterSeconds = 0.0;
	double ResampleEventSeconds = 0.0;
	FString ProjectionHashBefore;
	FString ProjectionHashAfter;
	FString StateHashBefore;
	FString StateHashAfter;
	FString FilterDecisionHash;
	FString MaterialLedgerHash;
};

struct FCarrierLabPhaseIIIA4FieldAudit
{
	int32 SampleCount = 0;
	int32 PlateVertexCount = 0;
	int32 NonZeroSampleFieldCount = 0;
	int32 NonZeroPlateVertexFieldCount = 0;
	int32 SmearedSampleCount = 0;
	double MaxAbsSampleElevation = 0.0;
	double MaxAbsSampleOceanicAge = 0.0;
	double MaxSampleVectorMagnitude = 0.0;
	double MaxSampleVectorRadialDot = 0.0;
	double MaxAbsPlateVertexElevation = 0.0;
	double MaxAbsPlateVertexOceanicAge = 0.0;
	double MaxPlateVertexVectorMagnitude = 0.0;
	double MaxPlateVertexVectorRadialDot = 0.0;
	double MinPositiveSampleElevation = 0.0;
	double MaxPositiveSampleElevation = 0.0;
	double MeanPositiveSampleElevation = 0.0;
};

struct FCarrierLabPhaseIIIA4SeedMetrics
{
	int32 PlateId = INDEX_NONE;
	int32 SeededVertexCount = 0;
	int32 ZeroedVertexCount = 0;
	int32 BoundaryTriangleCount = 0;
	double SeedElevation = 10.0;
	double SeedOceanicAge = 64.0;
};

struct FCarrierLabPhaseIIIE2BoundaryEdgeProbe
{
	int32 PlateId = INDEX_NONE;
	FVector3d StartUnitPosition = FVector3d::UnitX();
	FVector3d EndUnitPosition = FVector3d::UnitY();
	double StartContinentalFraction = 0.0;
	double EndContinentalFraction = 0.0;
	double StartElevation = 0.0;
	double EndElevation = 0.0;
};

struct FCarrierLabPhaseIIIE2BoundaryQueryAudit
{
	bool bFound = false;
	int32 BoundaryEdgeCount = 0;
	int32 DistinctPlateCount = 0;
	int32 Q1PlateId = INDEX_NONE;
	int32 Q2PlateId = INDEX_NONE;
	int32 Q1EdgeId = INDEX_NONE;
	int32 Q2EdgeId = INDEX_NONE;
	double Q1DistanceKm = 0.0;
	double Q2DistanceKm = 0.0;
	double Q1EdgeT = 0.0;
	double Q2EdgeT = 0.0;
	double Q1ContinentalFraction = 0.0;
	double Q2ContinentalFraction = 0.0;
	double Q1Elevation = 0.0;
	double Q2Elevation = 0.0;
	double QGammaInputNorm = 0.0;
	double QGammaUnitResidual = 0.0;
	FVector3d SampleUnitPosition = FVector3d::UnitZ();
	FVector3d Q1UnitPosition = FVector3d::UnitZ();
	FVector3d Q2UnitPosition = FVector3d::UnitZ();
	FVector3d QGammaUnitPosition = FVector3d::UnitZ();
};

enum class ECarrierLabPhaseIIIE3FilterReason : uint8
{
	None,
	Subducting,
	ObductionPending,
	CollisionPending
};

enum class ECarrierLabPhaseIIIE3SelectionClass : uint8
{
	NoHitDivergentGap,
	ResolvedSingleHit,
	DivergentGapAfterFiltering,
	UnresolvedSameMaterialMultiHit,
	UnresolvedMixedMaterialMultiHit,
	UnresolvedThirdPlateMultiHit
};

enum class ECarrierLabPhaseIIIE3MultiHitBucket : uint8
{
	None,
	WithinPlateCoincident,
	WithinPlateDistanceSeparated,
	CrossPlateEqual,
	CrossPlateDifferent,
	MixedMaterial,
	ThirdPlate
};

enum class ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule : uint8
{
	None,
	ContinentalPriority,
	OlderOceanicAge,
	LowerPlateId
};

enum class ECarrierLabPhaseIIIE65NearestHitResult : uint8
{
	NotApplied,
	UniqueNearestCrossPlateDifferent,
	UniqueNearestThirdPlate,
	DistanceTieHeld,
	UnsupportedHeld
};

enum class ECarrierLabPhaseIIIE66DistanceTieFallbackLayer : uint8
{
	None,
	ContinentalPriority,
	OlderOceanicAge,
	LowerPlateId,
	Unresolved
};

enum class ECarrierLabPhaseIIIE62BarycentricShape : uint8
{
	Unknown,
	Vertex,
	Edge,
	Interior
};

enum class ECarrierLabPhaseIIIE62HoldShape : uint8
{
	TwoPlateSameSourceTriangle,
	TwoPlateSharedGlobalEdge,
	TwoPlateSharedGlobalVertexOnly,
	TwoPlateNoSharedGlobalVertices,
	TwoPlateFieldMismatch,
	ThreePlateCommonGlobalVertex,
	ThreePlateEdgePlusIntruder,
	ThreePlateNoCommonSourceVertex,
	NonBoundaryOrInteriorOverlap,
	InvalidOrUnclassified
};

struct FCarrierLabPhaseIIIE3CandidateProbe
{
	int32 PlateId = INDEX_NONE;
	int32 LocalTriangleId = INDEX_NONE;
	int32 SourceTriangleId = INDEX_NONE;
	int32 GlobalVertexIds[3] = { INDEX_NONE, INDEX_NONE, INDEX_NONE };
	FVector3d Bary = FVector3d(1.0, 0.0, 0.0);
	double Distance = 1.0;
	double ContinentalFraction = 0.0;
	double Elevation = 0.0;
	double HistoricalElevation = 0.0;
	double OceanicAge = 0.0;
	double PlateContinentalFraction = 0.0;
	double PlateOceanicAge = 0.0;
	FVector3d RidgeDirection = FVector3d::ZeroVector;
	FVector3d FoldDirection = FVector3d::ZeroVector;
	ECarrierLabPhaseIIIE3FilterReason FilterReason = ECarrierLabPhaseIIIE3FilterReason::None;
	bool bBoundary = true;
	bool bHasSourceTopologySnapshot = false;
	bool bHasPlateAggregateOverride = false;
};

struct FCarrierLabPhaseIIIE3SelectionRecord
{
	int32 SampleId = INDEX_NONE;
	FVector3d SampleUnitPosition = FVector3d::UnitZ();
	int32 RawCandidateCount = 0;
	int32 RawPlateCount = 0;
	int32 FilteredCandidateCount = 0;
	int32 FilteredSubductingCount = 0;
	int32 FilteredObductionPendingCount = 0;
	int32 FilteredCollisionPendingCount = 0;
	int32 PostFilterCandidateCount = 0;
	int32 PostFilterPlateCount = 0;
	int32 EffectiveCandidateCount = 0;
	int32 CoalescedCandidateCount = 0;
	int32 CoalescedDuplicateHitCount = 0;
	bool bResolvedSingleHit = false;
	bool bDivergentGapRoute = false;
	bool bUnresolvedMultiHit = false;
	bool bCoalescedDuplicateHit = false;
	bool bCoalescingRejectedByFieldMismatch = false;
	bool bUsedSharedBoundaryTieBreak = false;
	bool bUsedNearestHitTieBreak = false;
	bool bUsedDistanceTieFallback = false;
	bool bUsedPolicyWinner = false;
	bool bUsedPriorOwnerFallback = false;
	ECarrierLabPhaseIIIE62HoldShape SharedBoundaryShapeClass = ECarrierLabPhaseIIIE62HoldShape::InvalidOrUnclassified;
	ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule SharedBoundaryTieBreakRule = ECarrierLabPhaseIIIE63SharedBoundaryTieBreakRule::None;
	int32 SharedBoundaryTieBreakCandidateCount = 0;
	int32 SharedBoundaryTieBreakPlateCount = 0;
	double SharedBoundaryTieBreakContinentalMargin = 0.0;
	double SharedBoundaryTieBreakOceanicAgeMargin = 0.0;
	TArray<int32> SharedBoundaryCandidatePlateIds;
	TArray<double> SharedBoundaryCandidateContinentalFractions;
	TArray<double> SharedBoundaryCandidateOceanicAges;
	ECarrierLabPhaseIIIE65NearestHitResult NearestHitResultClass = ECarrierLabPhaseIIIE65NearestHitResult::NotApplied;
	double NearestHitGapKm = 0.0;
	double NearestHitToleranceKm = 0.0;
	int32 NearestHitCandidateCount = 0;
	int32 NearestHitDistinctPlateCount = 0;
	int32 NearestHitProcessMarkedRefCount = 0;
	ECarrierLabPhaseIIIE66DistanceTieFallbackLayer DistanceTieFallbackLayer = ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::None;
	int32 DistanceTieFallbackCandidateCount = 0;
	int32 DistanceTieFallbackPlateCount = 0;
	double DistanceTieFallbackContinentalMargin = 0.0;
	double DistanceTieFallbackOceanicAgeMargin = 0.0;
	int32 ResolvedPlateId = INDEX_NONE;
	int32 ResolvedLocalTriangleId = INDEX_NONE;
	FVector3d ResolvedBary = FVector3d(1.0, 0.0, 0.0);
	ECarrierLabPhaseIIIE3SelectionClass SelectionClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
	ECarrierLabPhaseIIIE3MultiHitBucket MultiHitBucket = ECarrierLabPhaseIIIE3MultiHitBucket::None;
	double MultiHitMaxRayDistanceResidualKm = 0.0;
	double MultiHitMaxScalarResidual = 0.0;
	double MultiHitMaxElevationResidualKm = 0.0;
	double MultiHitMaxUnitVectorResidual = 0.0;
	double ContinentalFraction = 0.0;
	double Elevation = 0.0;
	double HistoricalElevation = 0.0;
	double OceanicAge = 0.0;
	FVector3d RidgeDirection = FVector3d::ZeroVector;
	FVector3d FoldDirection = FVector3d::ZeroVector;
};

struct FCarrierLabPhaseIIIE3RemeshSelectionAudit
{
	bool bRan = false;
	int32 SampleCount = 0;
	int32 RawHitSampleCount = 0;
	int32 RawMissSampleCount = 0;
	int32 RawMultiHitSampleCount = 0;
	int32 RawThirdPlateHitSampleCount = 0;
	int32 FilteredCandidateCount = 0;
	int32 FilteredSubductingCount = 0;
	int32 FilteredObductionPendingCount = 0;
	int32 FilteredCollisionPendingCount = 0;
	int32 PostFilterSingleHitCount = 0;
	int32 DivergentGapRouteCount = 0;
	int32 UnresolvedMultiHitCount = 0;
	int32 UnresolvedSameMaterialMultiHitCount = 0;
	int32 UnresolvedMixedMaterialMultiHitCount = 0;
	int32 UnresolvedThirdPlateMultiHitCount = 0;
	int32 WithinPlateCoincidentMultiHitCount = 0;
	int32 WithinPlateDistanceSeparatedMultiHitCount = 0;
	int32 CrossPlateEqualMultiHitCount = 0;
	int32 CrossPlateDifferentMultiHitCount = 0;
	int32 MixedMaterialMultiHitCount = 0;
	int32 ThirdPlateMultiHitCount = 0;
	int32 CoalescedMultiHitCount = 0;
	int32 CoalescedCandidateCount = 0;
	int32 CoalescingFieldMismatchHoldCount = 0;
	int32 SharedBoundaryTieBreakCount = 0;
	int32 SharedBoundaryTwoPlateEdgeCount = 0;
	int32 SharedBoundaryTwoPlateVertexOnlyCount = 0;
	int32 SharedBoundaryThreePlateCommonVertexCount = 0;
	int32 SharedBoundaryContinentalPriorityCount = 0;
	int32 SharedBoundaryOlderOceanicAgeCount = 0;
	int32 SharedBoundaryLowerPlateIdCount = 0;
	int32 NearestHitCrossPlateDifferentResolvedCount = 0;
	int32 NearestHitThirdPlateResolvedCount = 0;
	int32 NearestHitDistanceTieHeldCount = 0;
	int32 NearestHitUnsupportedHeldCount = 0;
	bool bNearestHitTieBreakDisabled = false;
	int32 DistanceTieFallbackCount = 0;
	int32 DistanceTieFallbackCrossPlateDifferentCount = 0;
	int32 DistanceTieFallbackThirdPlateCount = 0;
	int32 DistanceTieFallbackLayer1WinsCount = 0;
	int32 DistanceTieFallbackLayer2WinsCount = 0;
	int32 DistanceTieFallbackLayer3WinsCount = 0;
	int32 DistanceTieFallbackCrossPlateDifferentLayer1Count = 0;
	int32 DistanceTieFallbackCrossPlateDifferentLayer2Count = 0;
	int32 DistanceTieFallbackCrossPlateDifferentLayer3Count = 0;
	int32 DistanceTieFallbackThirdPlateLayer1Count = 0;
	int32 DistanceTieFallbackThirdPlateLayer2Count = 0;
	int32 DistanceTieFallbackThirdPlateLayer3Count = 0;
	bool bDistanceTieFallbackDisabled = false;
	double MaxMultiHitRayDistanceResidualKm = 0.0;
	double MaxMultiHitScalarResidual = 0.0;
	double MaxMultiHitElevationResidualKm = 0.0;
	double MaxMultiHitUnitVectorResidual = 0.0;
	int32 PriorOwnerFallbackCount = 0;
	int32 PolicyWinnerCount = 0;
	FString SelectionHash;
	TArray<FCarrierLabPhaseIIIE3SelectionRecord> Records;
};

struct FCarrierLabPhaseIIIE62CandidateSnapshot
{
	int32 CandidateIndex = INDEX_NONE;
	int32 PlateId = INDEX_NONE;
	int32 LocalTriangleId = INDEX_NONE;
	int32 SourceTriangleId = INDEX_NONE;
	int32 GlobalVertexIds[3] = { INDEX_NONE, INDEX_NONE, INDEX_NONE };
	FVector3d Bary = FVector3d(1.0, 0.0, 0.0);
	ECarrierLabPhaseIIIE62BarycentricShape BarycentricShape = ECarrierLabPhaseIIIE62BarycentricShape::Unknown;
	bool bBoundary = false;
	double Distance = 0.0;
	double RayDistanceResidualKm = 0.0;
	double ScalarResidual = 0.0;
	double ElevationResidualKm = 0.0;
	double UnitVectorResidual = 0.0;
	ECarrierLabPhaseIIIE3FilterReason FilterReason = ECarrierLabPhaseIIIE3FilterReason::None;
};

struct FCarrierLabPhaseIIIE62HoldRecord
{
	int32 SampleId = INDEX_NONE;
	ECarrierLabPhaseIIIE3SelectionClass SelectionClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
	ECarrierLabPhaseIIIE3MultiHitBucket MultiHitBucket = ECarrierLabPhaseIIIE3MultiHitBucket::None;
	ECarrierLabPhaseIIIE62HoldShape HoldShape = ECarrierLabPhaseIIIE62HoldShape::InvalidOrUnclassified;
	int32 CandidateCount = 0;
	int32 DistinctPlateCount = 0;
	int32 DistinctSourceTriangleCount = 0;
	int32 SharedGlobalVertexCount = 0;
	bool bAllBoundary = false;
	bool bHasInteriorCandidate = false;
	bool bFieldMismatch = false;
	double MaxRayDistanceResidualKm = 0.0;
	double MaxScalarResidual = 0.0;
	double MaxElevationResidualKm = 0.0;
	double MaxUnitVectorResidual = 0.0;
	TArray<FCarrierLabPhaseIIIE62CandidateSnapshot> Candidates;
};

struct FCarrierLabPhaseIIIE62CrossPlateMultiHitAudit
{
	bool bRan = false;
	int32 SampleCount = 0;
	int32 SelectionUnresolvedMultiHitCount = 0;
	int32 SelectionCrossPlateEqualMultiHitCount = 0;
	int32 SelectionThirdPlateMultiHitCount = 0;
	int32 CoalescedMultiHitCount = 0;
	int32 DiagnosedHoldCount = 0;
	int32 CandidateSnapshotCount = 0;
	int32 TwoPlateSameSourceTriangleCount = 0;
	int32 TwoPlateSharedGlobalEdgeCount = 0;
	int32 TwoPlateSharedGlobalVertexOnlyCount = 0;
	int32 TwoPlateNoSharedGlobalVerticesCount = 0;
	int32 TwoPlateFieldMismatchCount = 0;
	int32 ThreePlateCommonGlobalVertexCount = 0;
	int32 ThreePlateEdgePlusIntruderCount = 0;
	int32 ThreePlateNoCommonSourceVertexCount = 0;
	int32 NonBoundaryOrInteriorOverlapCount = 0;
	int32 InvalidOrUnclassifiedCount = 0;
	int32 PriorOwnerFallbackCount = 0;
	int32 PolicyWinnerCount = 0;
	int32 ProjectionAuthorityCount = 0;
	FString SelectionHash;
	FString DiagnosisHash;
	TArray<FCarrierLabPhaseIIIE62HoldRecord> Records;
};

struct FCarrierLabPhaseIIIE64CandidateDiagnostic
{
	FCarrierLabPhaseIIIE62CandidateSnapshot Snapshot;
	bool bSubductingMarked = false;
	bool bObductionPendingMarked = false;
	bool bCollisionPendingMarked = false;
	bool bNearestCandidate = false;
	double PlateContinentalFraction = 0.0;
	double PlateOceanicAge = 0.0;
};

struct FCarrierLabPhaseIIIE64HoldRecord
{
	int32 SampleId = INDEX_NONE;
	int32 Step = 0;
	FVector3d SampleUnitPosition = FVector3d::UnitZ();
	ECarrierLabPhaseIIIE3SelectionClass SelectionClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
	ECarrierLabPhaseIIIE3MultiHitBucket MultiHitBucket = ECarrierLabPhaseIIIE3MultiHitBucket::None;
	int32 CandidateCount = 0;
	int32 DistinctPlateCount = 0;
	bool bHasUniqueNearest = false;
	bool bNearestDistanceTie = false;
	double NearestDistanceGapKm = 0.0;
	bool bAnyCandidateProcessMarked = false;
	bool bAnySubductingMarked = false;
	bool bAnyObductionPendingMarked = false;
	bool bAnyCollisionPendingMarked = false;
	bool bNearestIsMostContinentalPlate = false;
	bool bNearestIsOlderOceanicPlate = false;
	bool bNearestIsLowerPlateId = false;
	bool bContinentalPlateTie = false;
	bool bOceanicAgePlateTie = false;
	TArray<FCarrierLabPhaseIIIE64CandidateDiagnostic> Candidates;
};

struct FCarrierLabPhaseIIIE64PostMotionMultiHitAudit
{
	bool bRan = false;
	int32 Step = 0;
	int32 SampleCount = 0;
	int32 PlateCount = 0;
	int32 SelectionUnresolvedMultiHitCount = 0;
	int32 SelectionCrossPlateDifferentMultiHitCount = 0;
	int32 SelectionThirdPlateMultiHitCount = 0;
	int32 SelectionCrossPlateEqualMultiHitCount = 0;
	int32 CoalescedMultiHitCount = 0;
	int32 SharedBoundaryTieBreakCount = 0;
	int32 DiagnosedHoldCount = 0;
	int32 CrossPlateDifferentHoldCount = 0;
	int32 ThirdPlateHoldCount = 0;
	int32 ProcessMarkedHoldCount = 0;
	int32 SubductingMarkedHoldCount = 0;
	int32 ObductionPendingMarkedHoldCount = 0;
	int32 CollisionPendingMarkedHoldCount = 0;
	int32 UniqueNearestCrossPlateDifferentCount = 0;
	int32 DistanceTieCrossPlateDifferentCount = 0;
	int32 UniqueNearestThirdPlateCount = 0;
	int32 DistanceTieThirdPlateCount = 0;
	int32 NearestMostContinentalCount = 0;
	int32 NearestOlderOceanicCount = 0;
	int32 NearestLowerPlateIdCount = 0;
	int32 PriorOwnerFallbackCount = 0;
	int32 PolicyWinnerCount = 0;
	int32 ProjectionAuthorityCount = 0;
	double MaxNearestDistanceGapKm = 0.0;
	double MedianNearestDistanceGapKm = 0.0;
	double P95NearestDistanceGapKm = 0.0;
	FString SelectionHash;
	FString DiagnosisHash;
	TArray<FCarrierLabPhaseIIIE64HoldRecord> Records;
};

enum class ECarrierLabPhaseIIIE67InvalidRecordReason : uint8
{
	None,
	InvalidSampleIndex,
	InvalidSampleUnitPosition,
	SampleFieldOutOfRange,
	ResolvedHitInvalidPlate,
	DivergentGapNoBoundaryPair,
	DivergentGapNonSeparating,
	DivergentGapGenerationOtherFailure,
	GeneratedGapInvalidAssignedPlate,
	UnhandledSelectionClass
};

struct FCarrierLabPhaseIIIE67InvalidRecord
{
	int32 SampleId = INDEX_NONE;
	int32 Step = 0;
	ECarrierLabPhaseIIIE67InvalidRecordReason Reason = ECarrierLabPhaseIIIE67InvalidRecordReason::None;
	ECarrierLabPhaseIIIE3SelectionClass SelectionClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
	ECarrierLabPhaseIIIE3MultiHitBucket MultiHitBucket = ECarrierLabPhaseIIIE3MultiHitBucket::None;
	int32 ResolvedPlateId = INDEX_NONE;
	int32 OceanicAssignedPlateId = INDEX_NONE;
	int32 FilteredSubductingCount = 0;
	int32 FilteredObductionPendingCount = 0;
	int32 FilteredCollisionPendingCount = 0;
	int32 RawCandidateCount = 0;
	int32 PostFilterCandidateCount = 0;
	bool bBoundaryPairFound = false;
	bool bNonSeparatingAnomaly = false;
	bool bGeneratedWithNonPositiveSeparation = false;
	bool bGeneratedOceanicCrust = false;
	bool bSampleUnitValid = true;
	bool bSampleFieldsValid = true;
	double SignedSeparationVelocity = 0.0;
	double ContinentalFraction = 0.0;
	double Elevation = 0.0;
	double HistoricalElevation = 0.0;
	double OceanicAge = 0.0;
	double LatitudeDegrees = 0.0;
	double LongitudeDegrees = 0.0;
	int32 SpatialLonBin = 0;
	int32 SpatialLatBin = 0;
};

struct FCarrierLabPhaseIIIE67ApplyPathInvalidRecordsAudit
{
	bool bRan = false;
	int32 Step = 0;
	int32 SampleCount = 0;
	int32 PlateCount = 0;
	int32 SelectionResolvedSingleHitCount = 0;
	int32 SelectionDivergentGapRouteCount = 0;
	int32 SelectionUnresolvedMultiHitCount = 0;
	int32 InvalidRecordCount = 0;
	int32 InvalidSampleIndexCount = 0;
	int32 InvalidSampleUnitPositionCount = 0;
	int32 SampleFieldOutOfRangeCount = 0;
	int32 ResolvedHitInvalidPlateCount = 0;
	int32 DivergentGapNoBoundaryPairCount = 0;
	int32 DivergentGapNonSeparatingCount = 0;
	int32 DivergentGapGenerationOtherFailureCount = 0;
	int32 GeneratedGapInvalidAssignedPlateCount = 0;
	int32 UnhandledSelectionClassCount = 0;
	int32 GeneratedCandidateCount = 0;
	int32 AppliedGeneratedCount = 0;
	int32 RiftingPendingCount = 0;
	int32 InvalidNoHitDivergentGapCount = 0;
	int32 InvalidFilterExhaustedDivergentGapCount = 0;
	int32 InvalidResolvedSingleHitCount = 0;
	int32 InvalidUnhandledSelectionCount = 0;
	int32 InvalidWithAnyProcessFilterCount = 0;
	int32 InvalidWithSubductingFilterCount = 0;
	int32 InvalidWithObductionFilterCount = 0;
	int32 InvalidWithCollisionFilterCount = 0;
	int32 RiftingPendingWithAnyProcessFilterCount = 0;
	int32 NoBoundaryPairMissCount = 0;
	int32 NonSeparatingGapFillCount = 0;
	int32 GeneratedWithNonPositiveSeparationCount = 0;
	double NonPositiveSeparationMinMagnitude = 0.0;
	double NonPositiveSeparationMedianMagnitude = 0.0;
	double NonPositiveSeparationMaxMagnitude = 0.0;
	FString NonPositiveSeparationSpatialHash;
	double TotalSeconds = 0.0;
	double SelectionSeconds = 0.0;
	double RecordBuildSeconds = 0.0;
	double DivergentQuerySeconds = 0.0;
	double ResolvedRecordSeconds = 0.0;
	double ValidationSeconds = 0.0;
	FString SelectionHash;
	FString DiagnosisHash;
	TArray<int32> SpatialInvalidCounts;
	TArray<FCarrierLabPhaseIIIE67InvalidRecord> Records;
};

struct FCarrierLabPhaseIIIE4OceanicGenerationRecord
{
	int32 SampleId = INDEX_NONE;
	FVector3d SampleUnitPosition = FVector3d::UnitZ();
	ECarrierLabPhaseIIIE3SelectionClass SourceSelectionClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
	bool bGeneratedOceanicCrust = false;
	bool bRejectedNonDivergentRoute = false;
	bool bRejectedUnresolvedMultiHit = false;
	bool bBoundaryPairFound = false;
	bool bNonSeparatingAnomaly = false;
	bool bGeneratedWithNonPositiveSeparation = false;
	bool bUsedPolicyWinner = false;
	bool bUsedPriorOwnerFallback = false;
	int32 Q1PlateId = INDEX_NONE;
	int32 Q2PlateId = INDEX_NONE;
	int32 Q1EdgeId = INDEX_NONE;
	int32 Q2EdgeId = INDEX_NONE;
	int32 AssignedPlateId = INDEX_NONE;
	double SignedSeparationVelocity = 0.0;
	double Q1DistanceKm = 0.0;
	double Q2DistanceKm = 0.0;
	double RidgeDistanceKm = 0.0;
	double NearestBoundaryDistanceKm = 0.0;
	double Alpha = 0.0;
	double Q1Elevation = 0.0;
	double Q2Elevation = 0.0;
	double ZBarElevation = 0.0;
	double ZGammaElevation = 0.0;
	double ZGammaProfileDistanceKm = 0.0;
	double ZGammaProfileReferenceDistanceKm = 0.0;
	double ZGammaProfileT = 0.0;
	double Elevation = 0.0;
	double OceanicAge = 0.0;
	double RidgeDirectionMagnitude = 0.0;
	double RidgeDirectionRadialDot = 0.0;
	double QGammaInputNorm = 0.0;
	double QGammaUnitResidual = 0.0;
	bool bUsedZGammaDistanceProfilePlaceholder = false;
	bool bUsedZGammaGeophysicsDerivedProfile = false;
	bool bPaperFaithfulZGammaProfile = false;
	FVector3d Q1UnitPosition = FVector3d::UnitZ();
	FVector3d Q2UnitPosition = FVector3d::UnitZ();
	FVector3d QGammaUnitPosition = FVector3d::UnitZ();
	FVector3d RidgeDirection = FVector3d::ZeroVector;
};

enum class ECarrierLabPhaseIIIE5TriangleAssignmentClass : uint8
{
	AllVerticesSamePlate,
	MajorityTwoOfThree,
	TripleJunctionCentroidSplit,
	UnresolvedTripleJunction,
	Invalid
};

struct FCarrierLabPhaseIIIE5RemeshVertexRecord
{
	int32 SampleId = INDEX_NONE;
	int32 AssignedPlateId = INDEX_NONE;
	bool bResolvedSingleHit = false;
	bool bDivergentGapRoute = false;
	bool bGeneratedOceanicCrust = false;
	bool bUnresolvedMultiHit = false;
	bool bUsedPolicyWinner = false;
	bool bUsedPriorOwnerFallback = false;
	bool bUsedProjectionOwnerFallback = false;
	bool bUsedZGammaDistanceProfilePlaceholder = false;
	bool bUsedZGammaGeophysicsDerivedProfile = false;
	bool bPaperFaithfulZGammaProfile = false;
	double PreRemeshContinentalFraction = 0.0;
	double PreRemeshElevation = 0.0;
	double ContinentalFraction = 0.0;
	double Elevation = 0.0;
	double HistoricalElevation = 0.0;
	double OceanicAge = 0.0;
	FVector3d RidgeDirection = FVector3d::ZeroVector;
	FVector3d FoldDirection = FVector3d::ZeroVector;
	FCarrierLabPhaseIIIE4OceanicGenerationRecord OceanicRecord;
};

struct FCarrierLabPhaseIIIE5RemeshInputFixture
{
	bool bForceAllSamplesToPlateZero = false;
	bool bSeedProcessStateBeforeRemesh = false;
	bool bInjectGeneratedOceanicRecord = false;
	bool bUseExplicitVertexRecords = false;
	int32 OverrideTriangleId = INDEX_NONE;
	int32 OverridePlateA = INDEX_NONE;
	int32 OverridePlateB = INDEX_NONE;
	int32 OverridePlateC = INDEX_NONE;
	FCarrierLabPhaseIIIE4OceanicGenerationRecord GeneratedOceanicRecord;
	TArray<FCarrierLabPhaseIIIE5RemeshVertexRecord> ExplicitVertexRecords;
};

struct FCarrierLabPhaseIIIE5TriangleRebuildRecord
{
	int32 GlobalTriangleId = INDEX_NONE;
	int32 VertexA = INDEX_NONE;
	int32 VertexB = INDEX_NONE;
	int32 VertexC = INDEX_NONE;
	int32 PlateA = INDEX_NONE;
	int32 PlateB = INDEX_NONE;
	int32 PlateC = INDEX_NONE;
	int32 AssignedPlateId = INDEX_NONE;
	int32 LocalTriangleId = INDEX_NONE;
	int32 LocalTriangleCount = 0;
	bool bBoundary = false;
	ECarrierLabPhaseIIIE5TriangleAssignmentClass AssignmentClass = ECarrierLabPhaseIIIE5TriangleAssignmentClass::Invalid;
};

struct FCarrierLabPhaseIIIE5PlateRebuildRecord
{
	int32 PlateId = INDEX_NONE;
	int32 SampleCount = 0;
	int32 TriangleCount = 0;
	int32 VertexCount = 0;
	bool bLocalTriangleIndicesCompact = false;
	bool bLocalVertexIndicesCompact = false;
	bool bMotionPreserved = false;
	FString TopologyHash;
};

struct FCarrierLabPhaseIIIE5ProcessResetAudit
{
	int32 ResetSerialBefore = 0;
	int32 ResetSerialAfter = 0;
	int32 ActiveTriangleCountBefore = 0;
	int32 ActiveTriangleCountAfter = 0;
	int32 DistanceRecordCountBefore = 0;
	int32 DistanceRecordCountAfter = 0;
	int32 MatrixPairCountBefore = 0;
	int32 MatrixPairCountAfter = 0;
	int32 MatrixEvidenceCountBefore = 0;
	int32 MatrixEvidenceCountAfter = 0;
	int32 SubductingMarkCountBefore = 0;
	int32 SubductingMarkCountAfter = 0;
	int32 ObductionMarkCountBefore = 0;
	int32 ObductionMarkCountAfter = 0;
	int32 CollisionPendingTriangleCountBefore = 0;
	int32 CollisionPendingTriangleCountAfter = 0;
	int32 DistanceToFrontRecordCountBefore = 0;
	int32 DistanceToFrontRecordCountAfter = 0;
	bool bResetSerialAdvanced = false;
	bool bProcessStateEmptyAfter = false;
};

struct FCarrierLabPhaseIIIE5TopologyRebuildAudit
{
	bool bRan = false;
	bool bApplied = false;
	int32 SampleCount = 0;
	int32 GlobalTriangleCount = 0;
	int32 AssignedTriangleCount = 0;
	int32 AllSameTriangleCount = 0;
	int32 MajorityTriangleCount = 0;
	int32 TripleJunctionCentroidSplitCount = 0;
	int32 TripleJunctionCentroidSplitLocalTriangleCount = 0;
	int32 TripleJunctionCentroidSplitSyntheticVertexCount = 0;
	int32 UnresolvedTripleJunctionCount = 0;
	int32 InvalidTriangleCount = 0;
	int32 MissingVertexAssignmentCount = 0;
	int32 InvalidAssignedPlateCount = 0;
	int32 GeneratedOceanicVertexCount = 0;
	int32 PreservedGeneratedOceanicVertexCount = 0;
	int32 PriorOwnerFallbackCount = 0;
	int32 ProjectionOwnerFallbackCount = 0;
	int32 PolicyWinnerCount = 0;
	int32 UnresolvedMultiHitRoutedCount = 0;
	bool bNoPriorOwnerFallback = false;
	bool bNoProjectionOwnerFallback = false;
	bool bNoPolicyWinner = false;
	bool bNoUnresolvedMultiHitRouted = false;
	bool bNoDuplicateTriangleAuthority = false;
	bool bTripleJunctionCentroidSplitApplied = false;
	bool bPlateLocalTopologyCompact = false;
	bool bMotionPreserved = false;
	bool bQProvenancePreserved = false;
	bool bZGammaHoldPreserved = false;
	bool bFixtureOwnedVertexAssignmentRecords = false;
	FString MotionHashBefore;
	FString MotionHashAfter;
	FString TopologyHash;
	FString AssignmentHash;
	FCarrierLabPhaseIIIE5ProcessResetAudit ResetAudit;
	TArray<FCarrierLabPhaseIIIE5TriangleRebuildRecord> TriangleRecords;
	TArray<FCarrierLabPhaseIIIE5PlateRebuildRecord> PlateRecords;
	TArray<FCarrierLabPhaseIIIE5RemeshVertexRecord> VertexRecords;
};

struct FCarrierLabPhaseIIIE5CollisionPendingWireAudit
{
	bool bRan = false;
	bool bSeededFromAcceptedCollisionGroups = false;
	bool bUsedFixtureOwnedAcceptedGroup = false;
	int32 AcceptedGroupCount = 0;
	int32 PendingTriangleKeyCount = 0;
	int32 FilteredCollisionPendingCount = 0;
	int32 FilteredSubductingCount = 0;
	int32 FilteredObductionPendingCount = 0;
	FString GroupingHash;
	FString SelectionHash;
};

struct FCarrierLabPhaseIIIB1TrackingAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 SourceBoundaryTriangleCount = 0;
	int32 ActiveBoundaryTriangleCount = 0;
	int32 MissingBoundaryTriangleCount = 0;
	int32 NonBoundaryActiveTriangleCount = 0;
	int32 DuplicateActiveTriangleCount = 0;
	int32 InvalidActiveTriangleCount = 0;
	int32 EmptyActivePlateCount = 0;
	int32 ResetSerial = 0;
	FString ConvergenceTrackingHash;
};

struct FCarrierLabPhaseIIIB2DistanceAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 SourceBoundaryTriangleCount = 0;
	int32 ActiveBoundaryTriangleCount = 0;
	int32 DistanceRecordCount = 0;
	int32 MissingDistanceRecordCount = 0;
	int32 NonFiniteDistanceCount = 0;
	int32 NegativeDistanceCount = 0;
	int32 OverThresholdActiveTriangleCount = 0;
	int32 DistanceCulledTriangleCount = 0;
	int32 EmptyActivePlateCount = 0;
	int32 ResetSerial = 0;
	double DistanceThresholdKm = 1800.0;
	double MinDistanceKm = 0.0;
	double MeanDistanceKm = 0.0;
	double MaxDistanceKm = 0.0;
	int32 ProbePlateId = INDEX_NONE;
	int32 ProbeLocalTriangleId = INDEX_NONE;
	double ProbeDistanceKm = 0.0;
	double ProbeStepDistanceKm = 0.0;
	FString ConvergenceTrackingHash;
};

struct FCarrierLabPhaseIIIB3SubductionMatrixAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	int32 ActiveBoundaryTriangleCount = 0;
	int32 DistanceRecordCount = 0;
	int32 MatrixPairCount = 0;
	int32 InvalidMatrixPairCount = 0;
	int32 SelfMatrixPairCount = 0;
	int32 RayTestCount = 0;
	int32 HitCount = 0;
	int32 BoundaryHitCount = 0;
	int32 NonConvergentHitCount = 0;
	int32 AcceptedLocalPositiveHitCount = 0;
	int32 RejectedLocalNonPositiveHitCount = 0;
	int32 ProbePlateA = INDEX_NONE;
	int32 ProbePlateB = INDEX_NONE;
	double ProbeSignedConvergenceVelocity = 0.0;
	TArray<CarrierLab::FConvergenceSubductionMatrixEvidence> AcceptedEvidence;
	TArray<CarrierLab::FConvergenceSubductionMatrixEvidence> RejectedEvidence;
	FString MatrixEvidenceHash;
	FString ConvergenceTrackingHash;
};

struct FCarrierLabPhaseIIIB4PolarityDecisionAudit
{
	uint64 PairKey = 0;
	int32 PlateA = INDEX_NONE;
	int32 PlateB = INDEX_NONE;
	int32 UnderPlate = INDEX_NONE;
	int32 OverPlate = INDEX_NONE;
	double PlateAContinentalFraction = 0.0;
	double PlateBContinentalFraction = 0.0;
	double PlateAOceanicAge = 0.0;
	double PlateBOceanicAge = 0.0;
	CarrierLab::EConvergenceSubductionPolarityClass DecisionClass = CarrierLab::EConvergenceSubductionPolarityClass::None;
};

struct FCarrierLabPhaseIIIB4PolarityAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	int32 MatrixPairCount = 0;
	int32 DecisionCount = 0;
	int32 OceanicUnderContinentalCount = 0;
	int32 CollisionCandidateCount = 0;
	int32 OceanOceanDeferredCount = 0;
	int32 OlderOceanicUnderYoungerOceanicCount = 0;
	int32 InvalidDecisionCount = 0;
	int32 MissingDecisionCount = 0;
	int32 SubductionPolarityCount = 0;
	int32 ProbePlateA = INDEX_NONE;
	int32 ProbePlateB = INDEX_NONE;
	int32 ProbeUnderPlate = INDEX_NONE;
	int32 ProbeOverPlate = INDEX_NONE;
	double ProbePlateAContinentalFraction = 0.0;
	double ProbePlateBContinentalFraction = 0.0;
	double ProbePlateAOceanicAge = 0.0;
	double ProbePlateBOceanicAge = 0.0;
	CarrierLab::EConvergenceSubductionPolarityClass ProbeDecisionClass = CarrierLab::EConvergenceSubductionPolarityClass::None;
	TArray<FCarrierLabPhaseIIIB4PolarityDecisionAudit> Decisions;
	FString PolarityHash;
	FString ConvergenceTrackingHash;
};

struct FCarrierLabPhaseIIIB6SeedMetrics
{
	int32 SeedPlateId = INDEX_NONE;
	int32 SeedOtherPlateId = INDEX_NONE;
	int32 SeedLocalTriangleId = INDEX_NONE;
	int32 SeedNeighborCandidateCount = 0;
	double SeedStepDistanceKm = 0.0;
	double MaxSeedNeighborDistanceKm = 0.0;
	uint64 SeedPairKey = 0;
};

struct FCarrierLabPhaseIIIB6NeighborPropagationAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	int32 ActiveTriangleCount = 0;
	int32 TotalPlateLocalTriangleCount = 0;
	int32 DistanceRecordCount = 0;
	int32 NonBoundaryActiveTriangleCount = 0;
	int32 OverThresholdActiveTriangleCount = 0;
	int32 PropagationSeedHitCount = 0;
	int32 PropagationAddedCount = 0;
	int32 PropagationDuplicateCount = 0;
	int32 PropagationDistanceRejectedCount = 0;
	int32 PropagationInvalidCount = 0;
	int32 ActivePlateCount = 0;
	int32 MaxActiveTrianglesOnPlate = 0;
	int32 ProbePlateId = INDEX_NONE;
	int32 ProbeLocalTriangleId = INDEX_NONE;
	int32 SeedEvidenceId = INDEX_NONE;
	int32 SeedPlateId = INDEX_NONE;
	int32 SeedOtherPlateId = INDEX_NONE;
	int32 SeedLocalTriangleId = INDEX_NONE;
	double SeedSignedConvergenceVelocity = 0.0;
	double ProbeDistanceKm = 0.0;
	double DistanceThresholdKm = 1800.0;
	double MaxDistanceKm = 0.0;
	FString ConvergenceTrackingHash;
};

struct FCarrierLabPhaseIIIB7HashClosureAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 SampleCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	int32 ActiveTriangleCount = 0;
	int32 DistanceRecordCount = 0;
	int32 MatrixPairCount = 0;
	int32 MatrixRayTestCount = 0;
	int32 MatrixHitCount = 0;
	int32 MatrixBoundaryHitCount = 0;
	int32 MatrixNonConvergentHitCount = 0;
	int32 PolarityDecisionCount = 0;
	int32 TriangleHitCount = 0;
	int32 PropagationSeedHitCount = 0;
	int32 PropagationAddedCount = 0;
	int32 PropagationDuplicateCount = 0;
	int32 PropagationDistanceRejectedCount = 0;
	int32 PropagationInvalidCount = 0;
	FString ProjectionHash;
	FString StateHash;
	FString CrustStateHash;
	FString MetricsConvergenceTrackingHash;
	FString ComputedConvergenceTrackingHash;
	bool bMetricsHashMatchesComputed = false;
};

struct FCarrierLabPhaseIIIC1SubductingMarkAuditRecord
{
	int32 MarkId = INDEX_NONE;
	uint64 PairKey = 0;
	int32 PlateId = INDEX_NONE;
	int32 OtherPlateId = INDEX_NONE;
	int32 LocalTriangleId = INDEX_NONE;
	int32 EvidenceId = INDEX_NONE;
	double SignedConvergenceVelocity = 0.0;
	CarrierLab::EConvergenceSubductionPolarityClass DecisionClass = CarrierLab::EConvergenceSubductionPolarityClass::None;
	bool bHistoricalElevationSnapshotTaken = false;
	int32 HistoricalElevationSnapshotVertexCount = 0;
	double HistoricalElevationSnapshotMin = 0.0;
	double HistoricalElevationSnapshotMax = 0.0;
	double VisibleElevationAppliedKm = 0.0;
};

struct FCarrierLabPhaseIIIC1SubductingMarkAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	bool bEnabled = false;
	int32 MarkCount = 0;
	int32 DuplicateMarkCount = 0;
	int32 InvalidMarkCount = 0;
	int32 UnderPlateMismatchCount = 0;
	int32 NonSubductionDecisionCount = 0;
	TArray<FCarrierLabPhaseIIIC1SubductingMarkAuditRecord> Records;
	FString SubductingMarkHash;
};

struct FCarrierLabPhaseIIIC2ElevationAuditRecord
{
	int32 MarkId = INDEX_NONE;
	int32 PlateId = INDEX_NONE;
	int32 OtherPlateId = INDEX_NONE;
	int32 LocalTriangleId = INDEX_NONE;
	int32 SnapshotVertexCount = 0;
	double HistoricalElevationMin = 0.0;
	double HistoricalElevationMax = 0.0;
	double VisibleElevationMin = 0.0;
	double VisibleElevationMax = 0.0;
	double AppliedTrenchDepthKm = 0.0;
};

struct FCarrierLabPhaseIIIC2ElevationAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	bool bMarksEnabled = false;
	bool bElevationSplitEnabled = false;
	int32 MarkCount = 0;
	int32 SnapshotMarkCount = 0;
	int32 MissingSnapshotCount = 0;
	int32 InvalidSnapshotCount = 0;
	int32 SnapshotVertexCount = 0;
	int32 DuplicateSnapshotCount = 0;
	int32 StateSnapshotCount = 0;
	int32 StateSnapshotVertexCount = 0;
	int32 StateInvalidSnapshotCount = 0;
	double TrenchDepthKm = -10.0;
	double VisibleElevationMin = 0.0;
	double VisibleElevationMax = 0.0;
	double HistoricalElevationMin = 0.0;
	double HistoricalElevationMax = 0.0;
	FString VisibleElevationHash;
	FString HistoricalElevationHash;
	FString CrustStateHash;
	TArray<FCarrierLabPhaseIIIC2ElevationAuditRecord> Records;
};

struct FCarrierLabPhaseIIIC3UpliftAuditRecord
{
	int32 MarkId = INDEX_NONE;
	int32 UnderPlateId = INDEX_NONE;
	int32 OverPlateId = INDEX_NONE;
	int32 UnderLocalTriangleId = INDEX_NONE;
	int32 OverLocalVertexId = INDEX_NONE;
	int32 OverGlobalSampleId = INDEX_NONE;
	double DistanceKm = 0.0;
	double SignedConvergenceVelocity = 0.0;
	double HistoricalElevationKm = 0.0;
	double PreviousElevationKm = 0.0;
	double AppliedDeltaKm = 0.0;
	double NewElevationKm = 0.0;
	double DistanceTransfer = 0.0;
	double SpeedTransfer = 0.0;
	double ReliefTransfer = 0.0;
	double FoldInfluenceBeta = 1.0;
	FVector3d OverUnitPosition = FVector3d::ZeroVector;
	FVector3d PreviousFoldDirection = FVector3d::ZeroVector;
	FVector3d RelativeFoldStep = FVector3d::ZeroVector;
	FVector3d ExpectedFoldDirection = FVector3d::ZeroVector;
	FVector3d NewFoldDirection = FVector3d::ZeroVector;
	double FoldDirectionMagnitude = 0.0;
};

struct FCarrierLabPhaseIIIC3UpliftAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	bool bMarksEnabled = false;
	bool bElevationSplitEnabled = false;
	bool bUpliftEnabled = false;
	int32 MarkCount = 0;
	int32 UpliftRecordCount = 0;
	int32 UniqueUpliftedVertexCount = 0;
	int32 SkippedNonContinentalVertexCount = 0;
	int32 SkippedOutsideRadiusCount = 0;
	int32 InvalidInputCount = 0;
	double EffectRadiusKm = 1800.0;
	double UpliftRateMmPerYear = 0.6;
	double ReferenceVelocityMmPerYear = 100.0;
	double FoldInfluenceBeta = 1.0;
	double TrenchDepthKm = -10.0;
	double ContinentalMaxElevationKm = 10.0;
	double TotalAppliedDeltaKm = 0.0;
	double MaxAppliedDeltaKm = 0.0;
	FString UpliftHash;
	FString VisibleElevationHash;
	FString HistoricalElevationHash;
	FString CrustStateHash;
	TArray<FCarrierLabPhaseIIIC3UpliftAuditRecord> Records;
};

struct FCarrierLabPhaseIIICObductionUpliftAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	bool bEnabled = false;
	bool bUpliftEnabled = false;
	int32 CollisionCandidateHitCount = 0;
	int32 MarkCount = 0;
	int32 DuplicateMarkCount = 0;
	int32 InvalidMarkCount = 0;
	int32 UpliftRecordCount = 0;
	int32 UniqueUpliftedVertexCount = 0;
	int32 SkippedNonContinentalVertexCount = 0;
	int32 SkippedOutsideRadiusCount = 0;
	int32 InvalidInputCount = 0;
	double EffectRadiusKm = 1800.0;
	double UpliftRateMmPerYear = 0.6;
	double ReferenceVelocityMmPerYear = 100.0;
	double FoldInfluenceBeta = 1.0;
	double TrenchDepthKm = -10.0;
	double ContinentalMaxElevationKm = 10.0;
	double TotalAppliedDeltaKm = 0.0;
	double MaxAppliedDeltaKm = 0.0;
	FString ObductionMarkHash;
	FString ObductionUpliftHash;
	FString VisibleElevationHash;
	FString CrustStateHash;
	TArray<FCarrierLabPhaseIIIC3UpliftAuditRecord> Records;
};

struct FCarrierLabPhaseIIIC4SlabPullContributionRecord
{
	int32 MarkId = INDEX_NONE;
	int32 PlateId = INDEX_NONE;
	int32 OtherPlateId = INDEX_NONE;
	int32 LocalTriangleId = INDEX_NONE;
	FVector3d PlateCenter = FVector3d::ZeroVector;
	FVector3d FrontBarycenter = FVector3d::ZeroVector;
	FVector3d ContributionUnit = FVector3d::ZeroVector;
	double SignedConvergenceVelocity = 0.0;
};

struct FCarrierLabPhaseIIIC4SlabPullPlateRecord
{
	int32 PlateId = INDEX_NONE;
	int32 ContributionCount = 0;
	FVector3d OldAxis = FVector3d::UnitZ();
	FVector3d NewAxis = FVector3d::UnitZ();
	FVector3d ContributionSum = FVector3d::ZeroVector;
	double OldAngularSpeedRadiansPerStep = 0.0;
	double RawAngularSpeedRadiansPerStep = 0.0;
	double NewAngularSpeedRadiansPerStep = 0.0;
	double MaxAllowedAngularSpeedRadiansPerStep = 0.0;
	double NewVelocityMmPerYear = 0.0;
	bool bClampedToReferenceSpeed = false;
};

struct FCarrierLabPhaseIIIC4SlabPullAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	bool bMarksEnabled = false;
	bool bSlabPullEnabled = false;
	int32 MarkCount = 0;
	int32 ContributionCount = 0;
	int32 AffectedPlateCount = 0;
	int32 InvalidInputCount = 0;
	double SlabPullSpeedMmPerYear = 8.0;
	double ReferenceVelocityMmPerYear = 100.0;
	double SlabPullAngularStep = 0.0;
	double MaxAllowedAngularStep = 0.0;
	double MaxOracleResidual = 0.0;
	double MaxVelocityMmPerYear = 0.0;
	FString MotionHashBefore;
	FString MotionHashAfter;
	FString SlabPullHash;
	TArray<FCarrierLabPhaseIIIC4SlabPullContributionRecord> Contributions;
	TArray<FCarrierLabPhaseIIIC4SlabPullPlateRecord> PlateRecords;
};

enum class ECarrierLabPhaseIIIC5ElevationLedgerClass : uint8
{
	TrenchVisibleElevation = 0,
	OverridingUplift = 1,
	ObductionUplift = 2
};

struct FCarrierLabPhaseIIIC5ElevationLedgerRecord
{
	int32 RecordId = INDEX_NONE;
	int32 Step = 0;
	int32 MarkId = INDEX_NONE;
	int32 PlateId = INDEX_NONE;
	int32 OtherPlateId = INDEX_NONE;
	int32 LocalTriangleId = INDEX_NONE;
	int32 LocalVertexId = INDEX_NONE;
	int32 GlobalSampleId = INDEX_NONE;
	double PreviousElevationKm = 0.0;
	double NewElevationKm = 0.0;
	double DeltaKm = 0.0;
	double SignedConvergenceVelocity = 0.0;
	ECarrierLabPhaseIIIC5ElevationLedgerClass LedgerClass = ECarrierLabPhaseIIIC5ElevationLedgerClass::TrenchVisibleElevation;
};

struct FCarrierLabPhaseIIIC5ElevationLedgerAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	bool bMarksEnabled = false;
	bool bElevationSplitEnabled = false;
	bool bUpliftEnabled = false;
	bool bSlabPullEnabled = false;
	int32 RecordCount = 0;
	int32 TrenchRecordCount = 0;
	int32 UpliftRecordCount = 0;
	int32 UniqueVertexCount = 0;
	double ActualVisibleElevationBeforeKm = 0.0;
	double ActualVisibleElevationAfterKm = 0.0;
	double ActualVisibleElevationDeltaKm = 0.0;
	double LedgerVisibleElevationDeltaKm = 0.0;
	double TrenchVisibleElevationDeltaKm = 0.0;
	double UpliftVisibleElevationDeltaKm = 0.0;
	double VisibleElevationResidualKm = 0.0;
	FString ElevationLedgerHash;
	FString VisibleElevationHash;
	FString HistoricalElevationHash;
	FString CrustStateHash;
	TArray<FCarrierLabPhaseIIIC5ElevationLedgerRecord> Records;
};

struct FCarrierLabPhaseIIID1TerraneRecord
{
	int32 RecordId = INDEX_NONE;
	uint64 PairKey = 0;
	int32 SourcePlateId = INDEX_NONE;
	int32 OtherPlateId = INDEX_NONE;
	int32 SeedLocalTriangleId = INDEX_NONE;
	int32 EvidenceId = INDEX_NONE;
	double SignedConvergenceVelocity = 0.0;
	int32 TriangleCount = 0;
	int32 ContinentalTriangleCount = 0;
	int32 InnerSeaTriangleCount = 0;
	int32 VertexCount = 0;
	double MeanContinentalFraction = 0.0;
	double AreaWeight = 0.0;
	FString TerraneHash;
	TArray<int32> LocalTriangleIds;
};

struct FCarrierLabPhaseIIID1TerraneAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	int32 CollisionCandidateHitCount = 0;
	int32 TerraneRecordCount = 0;
	int32 TotalTerraneTriangleCount = 0;
	int32 TotalContinentalTriangleCount = 0;
	int32 TotalInnerSeaTriangleCount = 0;
	int32 MaxTerraneTriangleCount = 0;
	int32 InvalidSeedCount = 0;
	int32 NonCollisionDecisionHitCount = 0;
	int32 NonContinentalSeedCount = 0;
	int32 EmptyTerraneCount = 0;
	TArray<FCarrierLabPhaseIIID1TerraneRecord> Records;
	FString TerraneDetectionHash;
};

struct FCarrierLabPhaseIIID2CollisionGroupRecord
{
	int32 GroupId = INDEX_NONE;
	uint64 PairKey = 0;
	int32 PlateA = INDEX_NONE;
	int32 PlateB = INDEX_NONE;
	int32 CandidateRecordCount = 0;
	int32 UniqueTerraneHashCount = 0;
	int32 ValidDistanceCount = 0;
	int32 InvalidDistanceCount = 0;
	int32 MaxDistanceRecordId = INDEX_NONE;
	int32 MaxDistanceEvidenceId = INDEX_NONE;
	int32 MaxDistanceSourcePlateId = INDEX_NONE;
	int32 MaxDistanceOtherPlateId = INDEX_NONE;
	int32 MaxDistanceLocalTriangleId = INDEX_NONE;
	double MaxInterpenetrationKm = 0.0;
	double MeanInterpenetrationKm = 0.0;
	double MeanSignedConvergenceVelocity = 0.0;
	double MaxSignedConvergenceVelocity = 0.0;
	double ThresholdKm = 300.0;
	double TotalAreaWeight = 0.0;
	bool bAccepted = false;
	FString GroupHash;
};

struct FCarrierLabPhaseIIID2CollisionGroupingAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	double ThresholdKm = 300.0;
	int32 TerraneRecordCount = 0;
	int32 GroupCount = 0;
	int32 AcceptedGroupCount = 0;
	int32 RejectedGroupCount = 0;
	int32 SubThresholdGroupCount = 0;
	int32 InvalidDistanceCount = 0;
	double MaxInterpenetrationKm = 0.0;
	FString SourceTerraneDetectionHash;
	TArray<FCarrierLabPhaseIIID2CollisionGroupRecord> Groups;
	FString GroupingHash;
};

struct FCarrierLabPhaseIIID3DestinationMassRecord
{
	int32 RecordId = INDEX_NONE;
	int32 GroupId = INDEX_NONE;
	uint64 PairKey = 0;
	int32 SourceRecordId = INDEX_NONE;
	int32 SourcePlateId = INDEX_NONE;
	int32 DestinationPlateId = INDEX_NONE;
	int32 SourceSeedLocalTriangleId = INDEX_NONE;
	int32 DestinationSeedLocalTriangleId = INDEX_NONE;
	int32 EvidenceId = INDEX_NONE;
	double SourceTerraneAreaWeight = 0.0;
	double RequiredDestinationAreaWeight = 0.0;
	double DestinationContinentalAreaWeight = 0.0;
	double DestinationMassRatio = 0.0;
	double ThresholdRatio = 0.5;
	int32 SourceTerraneTriangleCount = 0;
	int32 DestinationTriangleCount = 0;
	int32 DestinationContinentalTriangleCount = 0;
	int32 DestinationInnerSeaTriangleCount = 0;
	bool bInterpenetrationAccepted = false;
	bool bDestinationSeedValid = false;
	bool bMassAccepted = false;
	FString SourceTerraneHash;
	FString DestinationMassHash;
};

struct FCarrierLabPhaseIIID3DestinationMassAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	double InterpenetrationThresholdKm = 300.0;
	double DestinationMassThresholdRatio = 0.5;
	int32 SourceTerraneRecordCount = 0;
	int32 SourceGroupCount = 0;
	int32 InterpenetrationAcceptedGroupCount = 0;
	int32 TestedMassRecordCount = 0;
	int32 AcceptedMassRecordCount = 0;
	int32 RejectedMassRecordCount = 0;
	int32 InsufficientDestinationMassCount = 0;
	int32 MissingDestinationSeedCount = 0;
	int32 NonAcceptedGroupCount = 0;
	double MaxDestinationMassRatio = 0.0;
	double MinAcceptedDestinationMassRatio = 0.0;
	double MinRejectedDestinationMassRatio = 0.0;
	FString SourceGroupingHash;
	TArray<FCarrierLabPhaseIIID3DestinationMassRecord> Records;
	FString DestinationMassHash;
};

struct FCarrierLabPhaseIIID4SlabBreakPlanRecord
{
	int32 PlanId = INDEX_NONE;
	int32 GroupId = INDEX_NONE;
	uint64 PairKey = 0;
	int32 SourceRecordId = INDEX_NONE;
	int32 DestinationMassRecordId = INDEX_NONE;
	int32 SourcePlateId = INDEX_NONE;
	int32 DestinationPlateId = INDEX_NONE;
	int32 SourceTriangleCountBefore = 0;
	int32 SourceVertexCountBefore = 0;
	int32 RemovedTriangleCount = 0;
	int32 SurvivingTriangleCount = 0;
	int32 SurvivingVertexCount = 0;
	int32 RemovedVertexCount = 0;
	int32 BoundaryEdgeCount = 0;
	int32 CutBoundaryEdgeCount = 0;
	int32 InteriorEdgeCount = 0;
	int32 NonManifoldEdgeCount = 0;
	int32 InvalidMappedTriangleCount = 0;
	int32 DuplicateRemovalTriangleCount = 0;
	double RemovedAreaWeight = 0.0;
	bool bMassAccepted = false;
	bool bRemovalSetMatchesTerrane = false;
	bool bTopologyValid = false;
	bool bWouldDestroySourcePlate = false;
	FString SourceTerraneHash;
	FString RemovalSetHash;
	FString OldToNewTriangleMapHash;
	FString OldToNewVertexMapHash;
	FString SurvivorTopologyHash;
	FString PlanHash;
	TArray<int32> RemovedLocalTriangleIds;
	TArray<int32> OldToNewLocalTriangleIds;
	TArray<int32> OldToNewLocalVertexIds;
};

struct FCarrierLabPhaseIIID4SlabBreakPlanAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	double InterpenetrationThresholdKm = 300.0;
	double DestinationMassThresholdRatio = 0.5;
	int32 DestinationMassRecordCount = 0;
	int32 AcceptedDestinationMassRecordCount = 0;
	int32 RejectedDestinationMassRecordCount = 0;
	int32 InterpenetrationAcceptedGroupCount = 0;
	int32 DuplicatePlanCandidateCount = 0;
	int32 MissingSourceRecordCount = 0;
	int32 PlanCount = 0;
	int32 ValidPlanCount = 0;
	int32 InvalidPlanCount = 0;
	int32 WouldDestroySourcePlateCount = 0;
	int32 TotalRemovedTriangleCount = 0;
	int32 TotalSurvivingTriangleCount = 0;
	double TotalRemovedAreaWeight = 0.0;
	FString SourceDestinationMassHash;
	TArray<FCarrierLabPhaseIIID4SlabBreakPlanRecord> Plans;
	FString SlabBreakPlanHash;
};

struct FCarrierLabPhaseIIID5SuturePlanRecord
{
	int32 PlanId = INDEX_NONE;
	int32 SlabBreakPlanId = INDEX_NONE;
	int32 GroupId = INDEX_NONE;
	uint64 PairKey = 0;
	int32 SourcePlateId = INDEX_NONE;
	int32 DestinationPlateId = INDEX_NONE;
	int32 SourceTriangleCountBefore = 0;
	int32 DestinationTriangleCountBefore = 0;
	int32 DestinationVertexCountBefore = 0;
	int32 AddedTriangleCount = 0;
	int32 AddedVertexCount = 0;
	int32 PostSutureTriangleCount = 0;
	int32 PostSutureVertexCount = 0;
	int32 DestinationBoundaryEdgeCountBefore = 0;
	int32 PostBoundaryEdgeCount = 0;
	int32 PostInteriorEdgeCount = 0;
	int32 PostNonManifoldEdgeCount = 0;
	int32 PostBoundaryTriangleCount = 0;
	int32 AddedBoundaryTriangleCount = 0;
	int32 SutureBoundaryEdgeCount = 0;
	int32 InvalidSourceTriangleCount = 0;
	int32 InvalidSourceVertexCount = 0;
	int32 DuplicateDestinationSourceTriangleCount = 0;
	double AddedAreaWeight = 0.0;
	bool bSlabBreakPlanValid = false;
	bool bAddsExactlyRemovedTerrane = false;
	bool bTopologyValid = false;
	bool bBoundaryTrackingReinitializable = false;
	FString SourceTerraneHash;
	FString RemovalSetHash;
	FString AddedTriangleSetHash;
	FString DestinationOldToNewTriangleMapHash;
	FString DestinationOldToNewVertexMapHash;
	FString SourceToDestinationAddedTriangleMapHash;
	FString SourceToDestinationAddedVertexMapHash;
	FString PostSutureTopologyHash;
	FString PlanHash;
	TArray<int32> AddedSourceLocalTriangleIds;
	TArray<int32> DestinationOldToNewLocalTriangleIds;
	TArray<int32> DestinationOldToNewLocalVertexIds;
	TArray<int32> SourceToDestinationAddedTriangleIds;
	TArray<int32> SourceToDestinationAddedVertexIds;
};

struct FCarrierLabPhaseIIID5SuturePlanAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	double InterpenetrationThresholdKm = 300.0;
	double DestinationMassThresholdRatio = 0.5;
	int32 SlabBreakPlanCount = 0;
	int32 ValidSlabBreakPlanCount = 0;
	int32 SuturePlanCount = 0;
	int32 ValidSuturePlanCount = 0;
	int32 InvalidSuturePlanCount = 0;
	int32 BoundaryReinitializablePlanCount = 0;
	int32 MissingSourcePlateCount = 0;
	int32 MissingDestinationPlateCount = 0;
	int32 TotalAddedTriangleCount = 0;
	int32 TotalAddedVertexCount = 0;
	double TotalAddedAreaWeight = 0.0;
	FString SourceSlabBreakPlanHash;
	TArray<FCarrierLabPhaseIIID5SuturePlanRecord> Plans;
	FString SuturePlanHash;
};

struct FCarrierLabPhaseIIID7InputPipelineEquivalenceAudit
{
	int32 Step = 0;
	int32 EventCount = 0;
	int32 PlateCount = 0;
	int32 ResetSerial = 0;
	double InterpenetrationThresholdKm = 300.0;
	double DestinationMassThresholdRatio = 0.5;
	bool bPassed = false;
	bool bTerraneBuilt = false;
	bool bGroupingBuilt = false;
	bool bCachedPipelineBuilt = false;
	bool bUncachedPipelineBuilt = false;
	double CachedPipelineSeconds = 0.0;
	double UncachedPipelineSeconds = 0.0;
	FString TerraneDetectionHash;
	FString GroupingHash;
	FString CachedDestinationMassHash;
	FString UncachedDestinationMassHash;
	FString CachedSlabBreakPlanHash;
	FString UncachedSlabBreakPlanHash;
	FString CachedSuturePlanHash;
	FString UncachedSuturePlanHash;
};

struct FCarrierLabPhaseIIID6TopologyMutationRecord
{
	int32 EventId = INDEX_NONE;
	int32 Step = 0;
	int32 AppliedSuturePlanId = INDEX_NONE;
	int32 AppliedSlabBreakPlanId = INDEX_NONE;
	int32 GroupId = INDEX_NONE;
	uint64 PairKey = 0;
	int32 SourcePlateId = INDEX_NONE;
	int32 DestinationPlateId = INDEX_NONE;
	int32 SourceTriangleCountBefore = 0;
	int32 SourceTriangleCountAfter = 0;
	int32 SourceVertexCountBefore = 0;
	int32 SourceVertexCountAfter = 0;
	int32 DestinationTriangleCountBefore = 0;
	int32 DestinationTriangleCountAfter = 0;
	int32 DestinationVertexCountBefore = 0;
	int32 DestinationVertexCountAfter = 0;
	int32 RemovedTriangleCount = 0;
	int32 AddedTriangleCount = 0;
	int32 RemovedVertexCount = 0;
	int32 AddedVertexCount = 0;
	int32 DeferredValidPlanCount = 0;
	int32 InvalidAppliedTriangleCount = 0;
	int32 InvalidAppliedVertexCount = 0;
	int32 SourceBoundaryTriangleCountAfter = 0;
	int32 DestinationBoundaryTriangleCountAfter = 0;
	int32 SourceNonManifoldEdgeCountAfter = 0;
	int32 DestinationNonManifoldEdgeCountAfter = 0;
	int32 InvalidatedMatrixPairCount = 0;
	int32 InvalidatedPolarityDecisionCount = 0;
	int32 InvalidatedTriangleHitCount = 0;
	int32 InvalidatedMatrixEvidenceCount = 0;
	int32 InvalidatedSubductingMarkCount = 0;
	double SourceContinentalAreaBefore = 0.0;
	double SourceContinentalAreaAfter = 0.0;
	double DestinationContinentalAreaBefore = 0.0;
	double DestinationContinentalAreaAfter = 0.0;
	double SourceContinentalAreaDelta = 0.0;
	double DestinationContinentalAreaDelta = 0.0;
	double TransferredContinentalArea = 0.0;
	double ContinentalAreaResidual = 0.0;
	double MaxCopiedElevationDelta = 0.0;
	double MaxCopiedHistoricalElevationDelta = 0.0;
	bool bApplied = false;
	bool bOneCollisionOnly = false;
	bool bSlabBreakPlanValid = false;
	bool bSuturePlanValid = false;
	bool bSourceTopologyValidAfter = false;
	bool bDestinationTopologyValidAfter = false;
	bool bBoundaryTrackingReinitialized = false;
	bool bSubductionTrackingInvalidated = false;
	bool bNoUpliftApplied = false;
	FString SlabBreakPlanHash;
	FString SuturePlanHash;
	FString SourceTopologyHashAfter;
	FString DestinationTopologyHashAfter;
	FString MutationHash;
};

struct FCarrierLabPhaseIIID6TopologyMutationAudit
{
	int32 Step = 0;
	int32 EventCountBefore = 0;
	int32 EventCountAfter = 0;
	int32 PlateCount = 0;
	int32 ResetSerialBefore = 0;
	int32 ResetSerialAfter = 0;
	double InterpenetrationThresholdKm = 300.0;
	double DestinationMassThresholdRatio = 0.5;
	int32 SlabBreakPlanCount = 0;
	int32 ValidSlabBreakPlanCount = 0;
	int32 SuturePlanCount = 0;
	int32 ValidSuturePlanCount = 0;
	int32 AppliedMutationCount = 0;
	int32 DeferredValidPlanCount = 0;
	int32 InvalidPlanCount = 0;
	int32 MissingSlabBreakPlanCount = 0;
	bool bMutationAttempted = false;
	bool bMutationApplied = false;
	bool bTopologyMutated = false;
	bool bOneCollisionOnly = false;
	bool bNoPlanAvailable = false;
	TArray<FCarrierLabPhaseIIID6TopologyMutationRecord> Records;
	FString SourceSlabBreakPlanHash;
	FString SourceSuturePlanHash;
	FString TopologyMutationHash;
};

struct FCarrierLabPhaseIIID7CollisionUpliftRecord
{
	int32 RecordId = INDEX_NONE;
	int32 EventId = INDEX_NONE;
	int32 Step = 0;
	int32 SourcePlateId = INDEX_NONE;
	int32 DestinationPlateId = INDEX_NONE;
	int32 DestinationLocalVertexId = INDEX_NONE;
	int32 GlobalSampleId = INDEX_NONE;
	double TerraneAreaWeight = 0.0;
	double TerraneAreaKm2 = 0.0;
	double ReferencePlateAreaKm2 = 0.0;
	double RelativeVelocityMmPerYear = 0.0;
	double InfluenceRadiusKm = 0.0;
	double DistanceToTerraneKm = 0.0;
	double DistanceTransfer = 0.0;
	double PreviousElevationKm = 0.0;
	double AppliedDeltaKm = 0.0;
	double NewElevationKm = 0.0;
	double PreviousFoldMagnitude = 0.0;
	double NewFoldMagnitude = 0.0;
	FVector3d VertexUnitPosition = FVector3d::ZeroVector;
	FVector3d TerraneCentroid = FVector3d::ZeroVector;
	FVector3d ExpectedFoldDirection = FVector3d::ZeroVector;
	bool bApplied = false;
};

struct FCarrierLabPhaseIIID7CollisionUpliftAudit
{
	int32 Step = 0;
	int32 EventCountBefore = 0;
	int32 EventCountAfter = 0;
	int32 PlateCount = 0;
	int32 ResetSerialBefore = 0;
	int32 ResetSerialAfter = 0;
	double InterpenetrationThresholdKm = 300.0;
	double DestinationMassThresholdRatio = 0.5;
	double CollisionRadiusConstantKm = 4200.0;
	double CollisionCoefficientPerKm = 1.3e-5;
	double ReferenceVelocityMmPerYear = 100.0;
	double PlanetRadiusKm = 6371.0;
	double TerraneAreaWeight = 0.0;
	double TerraneAreaKm2 = 0.0;
	double ReferencePlateAreaKm2 = 0.0;
	double RelativeVelocityMmPerYear = 0.0;
	double InfluenceRadiusKm = 0.0;
	int32 CandidateVertexCount = 0;
	int32 UpliftRecordCount = 0;
	int32 UniqueUpliftedVertexCount = 0;
	int32 SkippedOutsideRadiusCount = 0;
	int32 SkippedNonContinentalVertexCount = 0;
	int32 InvalidInputCount = 0;
	double TotalAppliedDeltaKm = 0.0;
	double MaxAppliedDeltaKm = 0.0;
	double CenterExpectedDeltaKm = 0.0;
	double CenterAppliedDeltaKm = 0.0;
	double MaxOutsideRadiusDeltaKm = 0.0;
	double FormulaResidualKm = 0.0;
	bool bPlannedOnly = false;
	bool bTopologyMutationApplied = false;
	bool bUpliftApplied = false;
	bool bNoUpliftAvailable = false;
	FVector3d TerraneCentroid = FVector3d::ZeroVector;
	FString SourceSuturePlanHash;
	FString SourceTopologyMutationHash;
	FString UpliftHash;
	FString VisibleElevationHash;
	FString CrustStateHash;
	FCarrierLabPhaseIIID6TopologyMutationAudit TopologyAudit;
	TArray<FCarrierLabPhaseIIID7CollisionUpliftRecord> Records;
};

struct FCarrierLabPhaseIIIProcessOverlayTriangle
{
	FVector3d A = FVector3d::ZeroVector;
	FVector3d B = FVector3d::ZeroVector;
	FVector3d C = FVector3d::ZeroVector;
	int32 PlateId = INDEX_NONE;
	uint8 Role = 0;
	uint8 BoundaryEdgeMask = 0;
	double DistanceKm = -1.0;
};

UCLASS(Blueprintable)
class CARRIERLAB_API ACarrierLabVisualizationActor : public AActor
{
	GENERATED_BODY()

public:
	ACarrierLabVisualizationActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Visualization")
	TObjectPtr<UDynamicMeshComponent> MeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Carrier", meta = (ClampMin = "12"))
	int32 SampleCount = 10000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Carrier", meta = (ClampMin = "1", ClampMax = "63"))
	int32 PlateCount = 40;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Carrier")
	int32 Seed = 42;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Carrier", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	double ContinentalPlateFraction = 0.30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Carrier", meta = (ClampMin = "1.0"))
	double SphereRadius = 5000.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Motion", meta = (ClampMin = "0.0"))
	double VelocityMmPerYear = 66.6666666667;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Motion")
	bool bEnableNaturalResamplingEvents = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Motion", meta = (ClampMin = "0.1", ClampMax = "120.0"))
	double StepsPerSecond = 2.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Visualization")
	ECarrierLabVisualizationLayer VisualizationLayer = ECarrierLabVisualizationLayer::PhaseIIIERemeshSummary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Policy")
	ECarrierLabMultiHitPolicy MultiHitPolicy = ECarrierLabMultiHitPolicy::Centroid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Policy")
	int32 RandomTieBreakSeed = 915042;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	bool bEnablePhaseIIICSubductingMarks = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	bool bEnablePhaseIIICVisibleHistoricalElevation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase IIIE")
	bool bEnablePhaseIIIE3DuplicateHitCoalescing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase IIIE")
	bool bEnablePhaseIIIE3SharedBoundaryTieBreak = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase IIIE")
	bool bEnablePhaseIIIE3NearestHitTieBreak = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase IIIE")
	bool bEnablePhaseIIIE3DistanceTieFallback = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase IIIE")
	bool bRestoreNonSeparatingAnomalyVeto = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	double PhaseIIICTrenchDepthKm = -10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	bool bEnablePhaseIIICOverridingPlateUplift = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	bool bEnablePhaseIIICObductionUpliftBridge = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	double PhaseIIICSubductionUpliftMmPerYear = 0.6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	double PhaseIIICReferenceVelocityMmPerYear = 100.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	double PhaseIIICFoldDirectionBeta = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	double PhaseIIICMaxContinentalElevationKm = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	double PhaseIIICSubductionEffectRadiusKm = 1800.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	bool bEnablePhaseIIICSlabPull = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	double PhaseIIICSlabPullSpeedMmPerYear = 8.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	double PhaseIIIDCollisionRadiusKm = 4200.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	double PhaseIIIDCollisionCoefficientPerKm = 1.3e-5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Visualization")
	bool bAutoInitialize = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Visualization")
	bool bPlayOnBegin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Visualization")
	bool bShowHud = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Visualization")
	bool bShowWireframeOverlay = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	FCarrierLabVisualizationMetrics CurrentMetrics;

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Visualization")
	bool InitializeCarrier();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Visualization")
	bool ResetCarrier();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void TogglePlay();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void SetPlaying(bool bNewPlaying);

	UFUNCTION(BlueprintPure, Category = "CarrierLab|Controls")
	bool IsPlaying() const { return bPlaying; }

	UFUNCTION(BlueprintPure, Category = "CarrierLab|Visualization")
	bool IsCarrierInitialized() const { return bInitialized; }

	UFUNCTION(BlueprintPure, Category = "CarrierLab|Metrics")
	int32 GetNaturalCadenceSteps() const;

	UFUNCTION(BlueprintPure, Category = "CarrierLab|Metrics")
	double GetNaturalCadenceDeltaTMa() const;

	UFUNCTION(BlueprintPure, Category = "CarrierLab|Metrics")
	double GetObservedMaxPlateSpeedMmPerYear() const;

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void StepOnce();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	bool ApplyPhaseIIIELiveRemeshEvent();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void TriggerPhaseIIIELiveRemeshEvent();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void ApplyResampleEvent();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void SetVisualizationLayer(ECarrierLabVisualizationLayer NewLayer);

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void SetMultiHitPolicy(ECarrierLabMultiHitPolicy NewPolicy);

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void ShowPlateIdLayer();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void ShowContinentalFractionLayer();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void ShowMissMaskLayer();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void ShowOverlapMaskLayer();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void ShowBoundaryMaskLayer();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void ShowPhaseIIISummaryLayer();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void ShowElevationHeatmapLayer();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void ShowSubductionMaskLayer();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void ShowDistanceToFrontHeatmapLayer();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void ShowOceanicAgeHeatmapLayer();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void ShowRidgeDirectionLayer();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void ShowPhaseIIIERemeshSummaryLayer();

	bool BuildVisualizationLayerMap(
		ECarrierLabVisualizationLayer Layer,
		TArray<FColor>& OutPixels,
		int32& OutWidth,
		int32& OutHeight) const;

	void ConfigurePhaseIIMotionFixture(ECarrierLabPhaseIIMotionFixture Fixture);
	void ConfigurePhaseIIICProcessLayer(bool bEnabled, bool bInEnableSlabPull = false);
	bool DetectPhaseIIContacts(TArray<FCarrierLabPhaseIIContactRecord>& OutContacts, FCarrierLabPhaseIIContactMetrics& OutMetrics);
	bool BuildPhaseIITriangleLabels(
		const TArray<FCarrierLabPhaseIIContactRecord>& Contacts,
		const FCarrierLabPhaseIITriangleLabelConfig& Config,
		TArray<FCarrierLabPhaseIITriangleLabelRecord>& OutLabels,
		FCarrierLabPhaseIITriangleLabelMetrics& OutMetrics) const;
	bool ApplyPhaseIIResamplingFilterEvent(
		const TArray<FCarrierLabPhaseIITriangleLabelRecord>& Labels,
		TArray<FCarrierLabPhaseIIFilterDecisionRecord>& OutDecisions,
		FCarrierLabPhaseIIResamplingFilterMetrics& OutMetrics,
		TArray<FCarrierLabPhaseIIMaterialRecord>* OutMaterialRecords = nullptr,
		FCarrierLabPhaseIIMaterialLedgerMetrics* OutMaterialMetrics = nullptr);
	bool GetPhaseIIMotion(int32 PlateId, FCarrierLabVisualizationMotion& OutMotion) const;
	double ComputePhaseIIPairSignedConvergenceVelocity(int32 PlateA, int32 PlateB) const;
	bool GetPhaseIIIA1ElevationAudit(
		int32& OutSampleCount,
		int32& OutPlateVertexCount,
		double& OutMaxAbsSampleElevation,
		double& OutMaxAbsPlateVertexElevation) const;
	bool GetPhaseIIIA2CrustFieldAudit(
		int32& OutSampleCount,
		int32& OutPlateVertexCount,
		double& OutMaxAbsSampleElevation,
		double& OutMaxAbsPlateVertexElevation,
		double& OutMaxAbsSampleOceanicAge,
		double& OutMaxAbsPlateVertexOceanicAge) const;
	bool GetPhaseIIIA3VectorFieldAudit(
		int32& OutSampleCount,
		int32& OutPlateVertexCount,
		double& OutMaxSampleVectorMagnitude,
		double& OutMaxPlateVertexVectorMagnitude,
		double& OutMaxPlateVertexRadialDot) const;
	bool SeedPhaseIIIA3VectorAuditProbe(
		int32& OutPlateId,
		int32& OutLocalVertexId,
		FVector3d& OutInitialPosition,
		FVector3d& OutInitialRidgeDirection,
		FVector3d& OutInitialFoldDirection,
		FVector3d& OutRotationAxis,
		double& OutAngularSpeedRadiansPerStep);
	bool GetPhaseIIIA3VectorAuditProbe(
		int32 PlateId,
		int32 LocalVertexId,
		FVector3d& OutPosition,
		FVector3d& OutRidgeDirection,
		FVector3d& OutFoldDirection) const;
	bool SeedPhaseIIIA4BoundarySmearProbe(
		int32 PreferredPlateId,
		FCarrierLabPhaseIIIA4SeedMetrics& OutSeedMetrics);
	bool GetPhaseIIIA4FieldAudit(
		double SeedElevation,
		FCarrierLabPhaseIIIA4FieldAudit& OutAudit) const;
	bool GetPhaseIIIA4FieldAuditForSamples(
		const TArray<int32>& SampleIds,
		double SeedElevation,
		FCarrierLabPhaseIIIA4FieldAudit& OutAudit) const;
	bool QueryPhaseIIIE2ContinuousBoundaryPairForTest(
		const FVector3d& SamplePosition,
		const TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe>& BoundaryEdges,
		FCarrierLabPhaseIIIE2BoundaryQueryAudit& OutAudit) const;
	bool BuildPhaseIIIE2BoundaryEdgesFromCurrentStateForTest(
		TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe>& OutBoundaryEdges) const;
	bool QueryPhaseIIIE2ContinuousBoundaryPairFromCurrentStateForTest(
		const FVector3d& SamplePosition,
		FCarrierLabPhaseIIIE2BoundaryQueryAudit& OutAudit) const;
	bool QueryPhaseIIIE3FilteredRemeshSelectionForTest(
		const FVector3d& SamplePosition,
		const TArray<FCarrierLabPhaseIIIE3CandidateProbe>& CandidateProbes,
		FCarrierLabPhaseIIIE3SelectionRecord& OutRecord) const;
	bool QueryPhaseIIIE4DivergentOceanicFieldsForTest(
		const FVector3d& SamplePosition,
		ECarrierLabPhaseIIIE3SelectionClass SourceSelectionClass,
		const TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe>& BoundaryEdges,
		double SignedSeparationVelocity,
		FCarrierLabPhaseIIIE4OceanicGenerationRecord& OutRecord) const;
	bool QueryPhaseIIIE4DivergentOceanicFieldsFromCurrentStateForTest(
		const FVector3d& SamplePosition,
		ECarrierLabPhaseIIIE3SelectionClass SourceSelectionClass,
		FCarrierLabPhaseIIIE4OceanicGenerationRecord& OutRecord) const;
	bool RunPhaseIIIE5TopologyRebuildFixtureForTest(
		const FCarrierLabPhaseIIIE5RemeshInputFixture& Fixture,
		FCarrierLabPhaseIIIE5TopologyRebuildAudit& OutAudit);
	bool RunPhaseIIIE5CollisionPendingWireFixtureForTest(
		FCarrierLabPhaseIIIE5CollisionPendingWireAudit& OutAudit,
		FCarrierLabPhaseIIIE3RemeshSelectionAudit& OutSelectionAudit,
		double InterpenetrationThresholdKm = 300.0);
	bool RefreshPhaseIIIMetricsForTest();
	bool RunPhaseIIIE3FilteredRemeshSelectionAudit(FCarrierLabPhaseIIIE3RemeshSelectionAudit& OutAudit);
	bool RunPhaseIIIE3FilteredRemeshSelectionAuditForSamples(
		const TArray<int32>& SampleIds,
		FCarrierLabPhaseIIIE3RemeshSelectionAudit& OutAudit);
	bool DiagnosePhaseIIIE62HoldSnapshotsForTest(
		int32 SampleId,
		ECarrierLabPhaseIIIE3SelectionClass SelectionClass,
		ECarrierLabPhaseIIIE3MultiHitBucket MultiHitBucket,
		const TArray<FCarrierLabPhaseIIIE62CandidateSnapshot>& CandidateSnapshots,
		FCarrierLabPhaseIIIE62HoldRecord& OutRecord) const;
	bool RunPhaseIIIE62CrossPlateMultiHitDiagnosisAudit(
		FCarrierLabPhaseIIIE62CrossPlateMultiHitAudit& OutAudit);
	bool RunPhaseIIIE64PostMotionMultiHitDiagnosisAudit(
		FCarrierLabPhaseIIIE64PostMotionMultiHitAudit& OutAudit);
	bool RunPhaseIIIE67ApplyPathInvalidRecordsDiagnosisAudit(
		FCarrierLabPhaseIIIE67ApplyPathInvalidRecordsAudit& OutAudit);
	bool GetPhaseIIIB1TrackingAudit(FCarrierLabPhaseIIIB1TrackingAudit& OutAudit) const;
	bool GetPhaseIIIB2DistanceAudit(FCarrierLabPhaseIIIB2DistanceAudit& OutAudit) const;
	bool GetPhaseIIIB3SubductionMatrixAudit(FCarrierLabPhaseIIIB3SubductionMatrixAudit& OutAudit) const;
	bool GetPhaseIIIB4PolarityAudit(FCarrierLabPhaseIIIB4PolarityAudit& OutAudit) const;
	bool GetPhaseIIIB6NeighborPropagationAudit(FCarrierLabPhaseIIIB6NeighborPropagationAudit& OutAudit) const;
	bool GetPhaseIIIB7HashClosureAudit(FCarrierLabPhaseIIIB7HashClosureAudit& OutAudit) const;
	bool GetPhaseIIIC1SubductingMarkAudit(FCarrierLabPhaseIIIC1SubductingMarkAudit& OutAudit) const;
	bool GetPhaseIIIC2ElevationAudit(FCarrierLabPhaseIIIC2ElevationAudit& OutAudit) const;
	bool GetPhaseIIIC3UpliftAudit(FCarrierLabPhaseIIIC3UpliftAudit& OutAudit) const;
	bool GetPhaseIIICObductionUpliftAudit(FCarrierLabPhaseIIICObductionUpliftAudit& OutAudit) const;
	bool GetPhaseIIIC4SlabPullAudit(FCarrierLabPhaseIIIC4SlabPullAudit& OutAudit) const;
	bool BuildPhaseIIIC4SlabPullOracleFromCurrentState(FCarrierLabPhaseIIIC4SlabPullAudit& OutAudit) const;
	bool GetPhaseIIIC5ElevationLedgerAudit(FCarrierLabPhaseIIIC5ElevationLedgerAudit& OutAudit) const;
	bool DetectPhaseIIID1ConnectedTerranes(FCarrierLabPhaseIIID1TerraneAudit& OutAudit) const;
	bool DetectPhaseIIID2CollisionGroups(
		FCarrierLabPhaseIIID2CollisionGroupingAudit& OutAudit,
		double InterpenetrationThresholdKm = 300.0) const;
	bool DetectPhaseIIID3DestinationMass(
		FCarrierLabPhaseIIID3DestinationMassAudit& OutAudit,
		double InterpenetrationThresholdKm = 300.0,
		double DestinationMassThresholdRatio = 0.5) const;
	bool PlanPhaseIIID4SlabBreak(
		FCarrierLabPhaseIIID4SlabBreakPlanAudit& OutAudit,
		double InterpenetrationThresholdKm = 300.0,
		double DestinationMassThresholdRatio = 0.5) const;
	bool PlanPhaseIIID5Suture(
		FCarrierLabPhaseIIID5SuturePlanAudit& OutAudit,
		double InterpenetrationThresholdKm = 300.0,
		double DestinationMassThresholdRatio = 0.5) const;
	bool ApplyPhaseIIID6DetachAndSuture(
		FCarrierLabPhaseIIID6TopologyMutationAudit& OutAudit,
		double InterpenetrationThresholdKm = 300.0,
		double DestinationMassThresholdRatio = 0.5);
	bool PlanPhaseIIID7CollisionUplift(
		FCarrierLabPhaseIIID7CollisionUpliftAudit& OutAudit,
		double InterpenetrationThresholdKm = 300.0,
		double DestinationMassThresholdRatio = 0.5) const;
	bool VerifyPhaseIIID7InputPipelineEquivalence(
		FCarrierLabPhaseIIID7InputPipelineEquivalenceAudit& OutAudit,
		double InterpenetrationThresholdKm = 300.0,
		double DestinationMassThresholdRatio = 0.5) const;
	bool ApplyPhaseIIID7CollisionUplift(
		FCarrierLabPhaseIIID7CollisionUpliftAudit& OutAudit,
		double InterpenetrationThresholdKm = 300.0,
		double DestinationMassThresholdRatio = 0.5);
	bool GetPhaseIIIProcessOverlayTriangles(
		TArray<FCarrierLabPhaseIIIProcessOverlayTriangle>& OutRoleTriangles,
		TArray<FCarrierLabPhaseIIIProcessOverlayTriangle>& OutDistanceTriangles,
		TArray<FCarrierLabPhaseIIIProcessOverlayTriangle>& OutPlateBoundaryTriangles) const;
	bool SetPlateContinentalForTest(int32 PlateId, bool bContinental);
	bool SetPhaseIIID3DestinationPatchForTest(
		int32 SourcePlateId,
		int32 DestinationPlateId,
		int32 NeighborDepth,
		int32& OutSeedLocalTriangleId,
		int32& OutPatchTriangleCount);
	bool SetPhaseIIID3DestinationFrontPatchForTest(
		int32 SourcePlateId,
		int32 DestinationPlateId,
		int32 NeighborDepth,
		int32& OutSeedLocalTriangleId,
		int32& OutPatchTriangleCount);
	bool SetPlateElevationForTest(int32 PlateId, double ElevationKm);
	bool SetPlateOceanicAgeForTest(int32 PlateId, double OceanicAgeMa);
	int32 GetCarrierLocalTriangleCountForTest() const;
	bool SeedPhaseIIIB3NonConvergentEvidenceForTest(FCarrierLabPhaseIIIB3SubductionMatrixAudit& OutAudit);
	bool SeedPhaseIIIB6SingleConvergentTriangleForTest(
		int32 PreferredUnderPlateId,
		FCarrierLabPhaseIIIB6SeedMetrics& OutSeedMetrics);
	bool RunPhaseIIICostDriverAdvanceProbe(FCarrierLabPhaseIIICostDriverStepAudit& OutAudit);
	void ResetPhaseIIIDiagnosticCallCounts() const;
	FCarrierLabPhaseIIIDiagnosticCallCounts GetPhaseIIIDiagnosticCallCounts() const;

private:
	void BindInputControls();
	bool AdvanceOneStepWithNaturalResampling();
	bool ShouldFireNaturalResamplingEvent(int32 TargetStep) const;
	bool ApplyNaturalResampleEvent();
	void AdvanceOneStep();
	void UpdateConvergenceTrackingDistances();
	void UpdateConvergenceSubductionMatrix();
	void UpdateConvergenceSubductionPolarityDecisions();
	void UpdateConvergenceNeighborPropagation();
	void UpdatePhaseIIICSubductingTriangleMarks();
	void BeginPhaseIIIC5ElevationLedger();
	void AddPhaseIIIC5ElevationLedgerRecord(
		ECarrierLabPhaseIIIC5ElevationLedgerClass LedgerClass,
		int32 MarkId,
		int32 PlateId,
		int32 OtherPlateId,
		int32 LocalTriangleId,
		int32 LocalVertexId,
		int32 GlobalSampleId,
		double PreviousElevationKm,
		double NewElevationKm,
		double SignedConvergenceVelocity);
	void FinalizePhaseIIIC5ElevationLedger();
	double SumPlateVisibleElevationKm() const;
	bool ApplyPhaseIIIC2ElevationSplitToMark(CarrierLab::FConvergenceSubductingTriangleMark& Mark);
	void UpdatePhaseIIICObductionTriangleMarks();
	void ApplyPhaseIIIC3OverridingPlateUplift();
	void ApplyPhaseIIICObductionUplift();
	void ApplyPhaseIIIC4SlabPull();
	bool BuildPhaseIIID2CollisionGroupsFromTerranes(
		const FCarrierLabPhaseIIID1TerraneAudit& TerraneAudit,
		FCarrierLabPhaseIIID2CollisionGroupingAudit& OutAudit,
		double InterpenetrationThresholdKm) const;
	bool BuildPhaseIIID3DestinationMassFromInputs(
		const FCarrierLabPhaseIIID1TerraneAudit& TerraneAudit,
		const FCarrierLabPhaseIIID2CollisionGroupingAudit& GroupingAudit,
		FCarrierLabPhaseIIID3DestinationMassAudit& OutAudit,
		double InterpenetrationThresholdKm,
		double DestinationMassThresholdRatio,
		bool bEnableDestinationComponentCache = true) const;
	bool BuildPhaseIIID4SlabBreakFromInputs(
		const FCarrierLabPhaseIIID1TerraneAudit& TerraneAudit,
		const FCarrierLabPhaseIIID3DestinationMassAudit& DestinationMassAudit,
		FCarrierLabPhaseIIID4SlabBreakPlanAudit& OutAudit,
		double InterpenetrationThresholdKm,
		double DestinationMassThresholdRatio) const;
	bool BuildPhaseIIID5SutureFromSlabBreak(
		const FCarrierLabPhaseIIID4SlabBreakPlanAudit& SlabBreakAudit,
		FCarrierLabPhaseIIID5SuturePlanAudit& OutAudit,
		double InterpenetrationThresholdKm,
		double DestinationMassThresholdRatio) const;
	bool BuildPhaseIIID7CollisionUpliftFromPlans(
		const FCarrierLabPhaseIIID2CollisionGroupingAudit& GroupingAudit,
		const FCarrierLabPhaseIIID5SuturePlanAudit& SutureAudit,
		FCarrierLabPhaseIIID7CollisionUpliftAudit& OutAudit,
		double InterpenetrationThresholdKm,
		double DestinationMassThresholdRatio) const;
	bool ApplyPhaseIIID6DetachAndSutureFromPlans(
		const FCarrierLabPhaseIIID4SlabBreakPlanAudit& SlabBreakAudit,
		const FCarrierLabPhaseIIID5SuturePlanAudit& SutureAudit,
		FCarrierLabPhaseIIID6TopologyMutationAudit& OutAudit,
		double InterpenetrationThresholdKm,
		double DestinationMassThresholdRatio);
	bool ApplyPhaseIIID7CollisionUpliftFromPlan(
		const FCarrierLabPhaseIIID7CollisionUpliftAudit& PlannedAudit,
		const FCarrierLabPhaseIIID4SlabBreakPlanAudit& SlabBreakAudit,
		const FCarrierLabPhaseIIID5SuturePlanAudit& SutureAudit,
		FCarrierLabPhaseIIID7CollisionUpliftAudit& OutAudit,
		double InterpenetrationThresholdKm,
		double DestinationMassThresholdRatio);
	void ProjectCurrentCarrier();
	bool RefreshPlateRayMeshes(FString& OutError);
	bool RefreshProjectionRayMesh(FString& OutError);
	void RebuildRenderMesh();
	bool BuildRenderMeshTopology();
	void ResetObservedSpeedWindowForRemesh();
	void UpdateNaturalCadenceMetrics(bool bAdvanceObservedSpeedWindow);
	double GetObservedMaxPlateSpeedForCadenceMmPerYear() const;
	FLinearColor ColorForSampleLayer(int32 SampleId, ECarrierLabVisualizationLayer Layer) const;
	FLinearColor ColorForSample(int32 SampleId) const;
	void ShowHud() const;
	FString BuildHudText() const;

	CarrierLab::FCarrierState State;
	TArray<FCarrierLabVisualizationMotion> Motions;
	TArray<FCarrierLabVizPlateMesh> PlateRayMeshes;
	FCarrierLabVizProjectionMesh ProjectionRayMesh;
	TArray<int32> RenderPlateIds;
	TArray<double> RenderContinentalFractions;
	TArray<double> RenderElevations;
	TArray<double> DriftErrorKmBySample;
	TArray<TArray<FVector3d>> DriftReferencePositions;
	TArray<uint8> MissMask;
	TArray<uint8> OverlapMask;
	TArray<uint8> BoundaryMask;
	TArray<uint8> PlateBoundaryMask;
	TArray<uint8> SubductionRoleMask;
	TArray<uint8> PhaseIIIELiveRemeshEventMask;
	TArray<double> DistanceToFrontKmBySample;
	mutable FCarrierLabPhaseIIIDiagnosticCallCounts PhaseIIIDiagnosticCallCounts;
	double LastConvergenceMatrixBvhBuildSeconds = 0.0;
	double LastConvergenceMatrixRayQuerySeconds = 0.0;
	FCarrierLabPhaseIIIC3UpliftAudit LastPhaseIIIC3UpliftAudit;
	FCarrierLabPhaseIIICObductionUpliftAudit LastPhaseIIICObductionUpliftAudit;
	FCarrierLabPhaseIIIC4SlabPullAudit LastPhaseIIIC4SlabPullAudit;
	FCarrierLabPhaseIIIC5ElevationLedgerAudit LastPhaseIIIC5ElevationLedgerAudit;
	int32 CachedRenderMeshSampleCount = 0;
	int32 CachedRenderMeshTriangleCount = 0;
	double StepAccumulator = 0.0;
	int32 DriftReferenceStep = 0;
	bool bPlateRayMeshTopologyDirty = true;
	bool bProjectionRayMeshTopologyDirty = true;
	bool bRenderMeshTopologyDirty = true;
	bool bInitialized = false;
	bool bPlaying = false;

	void CaptureDriftReference();
	void ComputeDriftMetrics();
	void ComputePlateBoundaryMask();
	void ComputePhaseIIIObservabilityMasks();
	void UpdatePhaseIIIVisibilityMetrics();
	void UpdateLastHash();
};
