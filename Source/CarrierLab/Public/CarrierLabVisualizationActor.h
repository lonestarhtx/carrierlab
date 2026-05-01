// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CarrierLabCarrier.h"
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
};

struct FCarrierLabVisualizationMotion
{
	FVector3d Axis = FVector3d::UnitZ();
	FVector3d CurrentCenter = FVector3d::UnitZ();
	double AngularSpeedRadiansPerStep = 0.0;
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

private:
	void BindInputControls();
	void AdvanceOneStep();
	void ProjectCurrentCarrier();
	void RebuildRenderMesh();
	bool BuildRenderMeshTopology();
	FLinearColor ColorForSample(int32 SampleId) const;
	void ShowHud() const;
	FString BuildHudText() const;

	CarrierLab::FCarrierState State;
	TArray<FCarrierLabVisualizationMotion> Motions;
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
	bool bRenderMeshTopologyDirty = true;
	bool bInitialized = false;
	bool bPlaying = false;

	void CaptureDriftReference();
	void ComputeDriftMetrics();
	void ComputePlateBoundaryMask();
	void UpdateLastHash();
};
