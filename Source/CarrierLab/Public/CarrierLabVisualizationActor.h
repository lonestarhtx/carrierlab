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
	DriftError UMETA(DisplayName = "Drift Error")
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

	void ConfigurePhaseIIMotionFixture(ECarrierLabPhaseIIMotionFixture Fixture);
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

private:
	void BindInputControls();
	void AdvanceOneStep();
	void ProjectCurrentCarrier();
	bool RefreshPlateRayMeshes(FString& OutError);
	bool RefreshProjectionRayMesh(FString& OutError);
	void RebuildRenderMesh();
	bool BuildRenderMeshTopology();
	FLinearColor ColorForSample(int32 SampleId) const;
	void ShowHud() const;
	FString BuildHudText() const;

	CarrierLab::FCarrierState State;
	TArray<FCarrierLabVisualizationMotion> Motions;
	TArray<FCarrierLabVizPlateMesh> PlateRayMeshes;
	FCarrierLabVizProjectionMesh ProjectionRayMesh;
	TArray<int32> RenderPlateIds;
	TArray<double> RenderContinentalFractions;
	TArray<double> DriftErrorKmBySample;
	TArray<TArray<FVector3d>> DriftReferencePositions;
	TArray<uint8> MissMask;
	TArray<uint8> OverlapMask;
	TArray<uint8> BoundaryMask;
	TArray<uint8> PlateBoundaryMask;
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
	void UpdateLastHash();
};
