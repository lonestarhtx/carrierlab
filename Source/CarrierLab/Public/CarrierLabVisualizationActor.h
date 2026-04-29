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
	BoundaryMask UMETA(DisplayName = "Boundary Mask")
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
	int32 LastGapFillCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	int32 LastNonSeparatingGapFillCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double AuthoritativeCAF = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double ProjectedCAF = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double ProjectionSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CarrierLab|Metrics")
	double MeshUpdateSeconds = 0.0;
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

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void TogglePlay();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void StepOnce();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void ApplyResampleEvent();

	UFUNCTION(BlueprintCallable, Category = "CarrierLab|Controls")
	void SetVisualizationLayer(ECarrierLabVisualizationLayer NewLayer);

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
	void ShowHud() const;
	FString BuildHudText() const;

	CarrierLab::FCarrierState State;
	TArray<FCarrierLabVisualizationMotion> Motions;
	TArray<int32> RenderPlateIds;
	TArray<double> RenderContinentalFractions;
	TArray<uint8> MissMask;
	TArray<uint8> OverlapMask;
	TArray<uint8> BoundaryMask;
	double StepAccumulator = 0.0;
	bool bInitialized = false;
	bool bPlaying = false;
};
