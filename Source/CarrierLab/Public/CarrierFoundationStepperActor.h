// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CarrierLabV2Core.h"
#include "GameFramework/Actor.h"
#include "CarrierFoundationStepperActor.generated.h"

class UDynamicMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class ECarrierFoundationStepperFixture : uint8
{
	FX014 UMETA(DisplayName = "FX-014 Rebuild Reset"),
	FX015 UMETA(DisplayName = "FX-015 Q1/Q2 Gap Fill"),
	InspectableScale UMETA(DisplayName = "Inspectable Scale")
};

UENUM(BlueprintType)
enum class ECarrierFoundationStepperStep : uint8
{
	ColdStart UMETA(DisplayName = "Cold Start"),
	RigidMotion UMETA(DisplayName = "Rigid Motion"),
	ContactCandidates UMETA(DisplayName = "Contact Candidates"),
	ProcessMarking UMETA(DisplayName = "Process Marking"),
	FilteredSampling UMETA(DisplayName = "Filtered Global Sampling"),
	Q1Q2GapFill UMETA(DisplayName = "Q1/Q2 Gap Fill"),
	TopologyRebuild UMETA(DisplayName = "Topology Rebuild"),
	ProcessReset UMETA(DisplayName = "Process Reset")
};

UENUM(BlueprintType)
enum class ECarrierFoundationStepperLayer : uint8
{
	StepDefault UMETA(DisplayName = "Step Default"),
	PlateAssignment UMETA(DisplayName = "Plate Assignment"),
	ContactCandidates UMETA(DisplayName = "Contact Candidates"),
	ProcessMarks UMETA(DisplayName = "Process Marks"),
	FilteredSampling UMETA(DisplayName = "Filtered Sampling"),
	Q1Q2GapFill UMETA(DisplayName = "Q1/Q2 Gap Fill"),
	RebuiltTopology UMETA(DisplayName = "Rebuilt Topology"),
	ProcessReset UMETA(DisplayName = "Process Reset"),
	ForbiddenFallbacks UMETA(DisplayName = "Forbidden Fallbacks"),
	ReplayDeterminism UMETA(DisplayName = "Replay Determinism")
};

USTRUCT(BlueprintType)
struct FCarrierFoundationStepperActorMetrics
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	FString FixtureId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	FString Verdict;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	FString StepName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	FString LayerName;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	int32 SampleCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	int32 TriangleCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	int32 ContactCandidateCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	int32 ProcessEventCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	int32 PreResetTriangleMarkCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	int32 PostResetTriangleMarkCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	int32 ZeroValidHitCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	int32 GeneratedOceanicCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	int32 RebuiltTriangleAssignmentCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	int32 UnassignedTriangleCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	bool bFixturePass = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	bool bReplayDeterministic = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	bool bForbiddenFallbackDetected = false;
};

UCLASS(Blueprintable)
class CARRIERLAB_API ACarrierFoundationStepperActor : public AActor
{
	GENERATED_BODY()

public:
	ACarrierFoundationStepperActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation")
	TObjectPtr<UDynamicMeshComponent> MeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation")
	TObjectPtr<UTextRenderComponent> StatusText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carrier Foundation")
	ECarrierFoundationStepperFixture Fixture = ECarrierFoundationStepperFixture::InspectableScale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carrier Foundation", meta = (ClampMin = "128", ClampMax = "50000", ToolTip = "Used only by Inspectable Scale. Keep small for interactive stepping; set 50000 for paper-scale inspection."))
	int32 InspectableScaleSampleCount = 2000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carrier Foundation")
	ECarrierFoundationStepperStep CurrentStep = ECarrierFoundationStepperStep::ColdStart;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carrier Foundation")
	ECarrierFoundationStepperLayer VisualizationLayer = ECarrierFoundationStepperLayer::StepDefault;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carrier Foundation", meta = (ClampMin = "1.0"))
	double SphereRadius = 250.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carrier Foundation")
	bool bShowWireframeOverlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carrier Foundation")
	bool bAutoBuildOnConstruction = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Carrier Foundation", meta = (ClampMin = "8.0"))
	float StatusTextWorldSize = 18.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	FCarrierFoundationStepperActorMetrics Metrics;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	FString LastSnapshotSummary;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Carrier Foundation|Metrics")
	FString LastError;

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Carrier Foundation")
	void RebuildFoundationSnapshot();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Carrier Foundation")
	void StepForward();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Carrier Foundation")
	void StepBackward();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Carrier Foundation")
	void ResetToColdStart();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Carrier Foundation")
	void UseFX014();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Carrier Foundation")
	void UseFX015();

	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Carrier Foundation")
	void UseInspectableScale();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	CarrierLab::V2::FCarrierV2FoundationStepperSnapshot Snapshot;
	bool bSnapshotValid = false;
	bool bRenderMeshTopologyDirty = true;
	int32 CachedRenderMeshSampleCount = 0;
	int32 CachedRenderMeshTriangleCount = 0;
	ECarrierFoundationStepperFixture CachedFixture = ECarrierFoundationStepperFixture::InspectableScale;
	int32 CachedInspectableScaleSampleCount = 0;

	CarrierLab::V2::FCarrierV2Stage5Config BuildConfigForFixture() const;
	void UpdateMetricsFromSnapshot();
	void UpdateStatusText();
	void RebuildRenderMesh();
	bool BuildRenderMeshTopology();
	FLinearColor ColorForTriangleCorner(
		const CarrierLab::V2::FCarrierV2FoundationStepperTriangleVisual& Triangle,
		const CarrierLab::V2::FCarrierV2FoundationStepperSampleVisual& Sample) const;
	FLinearColor ColorForPlate(int32 PlateId) const;
	ECarrierFoundationStepperLayer EffectiveLayer() const;
	FString BuildStatusText() const;
};
