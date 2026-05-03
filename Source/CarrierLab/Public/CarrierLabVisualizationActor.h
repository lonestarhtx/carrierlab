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
	DistanceToFrontHeatmap UMETA(DisplayName = "Distance To Front Heatmap")
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
	OverridingUplift = 1
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Motion", meta = (ClampMin = "0.1", ClampMax = "120.0"))
	double StepsPerSecond = 2.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Visualization")
	ECarrierLabVisualizationLayer VisualizationLayer = ECarrierLabVisualizationLayer::PlateId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Policy")
	ECarrierLabMultiHitPolicy MultiHitPolicy = ECarrierLabMultiHitPolicy::Centroid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Policy")
	int32 RandomTieBreakSeed = 915042;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	bool bEnablePhaseIIICSubductingMarks = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	bool bEnablePhaseIIICVisibleHistoricalElevation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	double PhaseIIICTrenchDepthKm = -10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	bool bEnablePhaseIIICOverridingPlateUplift = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	double PhaseIIICSubductionUpliftMmPerYear = 0.6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	double PhaseIIICReferenceVelocityMmPerYear = 100.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	double PhaseIIICMaxContinentalElevationKm = 10.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	double PhaseIIICSubductionEffectRadiusKm = 1800.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	bool bEnablePhaseIIICSlabPull = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CarrierLab|Phase III")
	double PhaseIIICSlabPullSpeedMmPerYear = 8.0;

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

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void StepOnce();

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
	bool GetPhaseIIIB1TrackingAudit(FCarrierLabPhaseIIIB1TrackingAudit& OutAudit) const;
	bool GetPhaseIIIB2DistanceAudit(FCarrierLabPhaseIIIB2DistanceAudit& OutAudit) const;
	bool GetPhaseIIIB3SubductionMatrixAudit(FCarrierLabPhaseIIIB3SubductionMatrixAudit& OutAudit) const;
	bool GetPhaseIIIB4PolarityAudit(FCarrierLabPhaseIIIB4PolarityAudit& OutAudit) const;
	bool GetPhaseIIIB6NeighborPropagationAudit(FCarrierLabPhaseIIIB6NeighborPropagationAudit& OutAudit) const;
	bool GetPhaseIIIB7HashClosureAudit(FCarrierLabPhaseIIIB7HashClosureAudit& OutAudit) const;
	bool GetPhaseIIIC1SubductingMarkAudit(FCarrierLabPhaseIIIC1SubductingMarkAudit& OutAudit) const;
	bool GetPhaseIIIC2ElevationAudit(FCarrierLabPhaseIIIC2ElevationAudit& OutAudit) const;
	bool GetPhaseIIIC3UpliftAudit(FCarrierLabPhaseIIIC3UpliftAudit& OutAudit) const;
	bool GetPhaseIIIC4SlabPullAudit(FCarrierLabPhaseIIIC4SlabPullAudit& OutAudit) const;
	bool GetPhaseIIIC5ElevationLedgerAudit(FCarrierLabPhaseIIIC5ElevationLedgerAudit& OutAudit) const;
	bool SetPlateContinentalForTest(int32 PlateId, bool bContinental);
	bool SetPlateElevationForTest(int32 PlateId, double ElevationKm);
	bool SetPlateOceanicAgeForTest(int32 PlateId, double OceanicAgeMa);
	bool SeedPhaseIIIB3NonConvergentEvidenceForTest(FCarrierLabPhaseIIIB3SubductionMatrixAudit& OutAudit);
	bool SeedPhaseIIIB6SingleConvergentTriangleForTest(
		int32 PreferredUnderPlateId,
		FCarrierLabPhaseIIIB6SeedMetrics& OutSeedMetrics);

private:
	void BindInputControls();
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
	void ApplyPhaseIIIC3OverridingPlateUplift();
	void ApplyPhaseIIIC4SlabPull();
	void ProjectCurrentCarrier();
	bool RefreshPlateRayMeshes(FString& OutError);
	bool RefreshProjectionRayMesh(FString& OutError);
	void RebuildRenderMesh();
	bool BuildRenderMeshTopology();
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
	TArray<double> DistanceToFrontKmBySample;
	FCarrierLabPhaseIIIC3UpliftAudit LastPhaseIIIC3UpliftAudit;
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
	void UpdateLastHash();
};
