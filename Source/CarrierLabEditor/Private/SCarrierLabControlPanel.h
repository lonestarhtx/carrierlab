// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CarrierLabVisualizationActor.h"
#include "Widgets/SCompoundWidget.h"

class ACarrierLabVisualizationActor;

class SCarrierLabControlPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCarrierLabControlPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

private:
	TWeakObjectPtr<ACarrierLabVisualizationActor> TargetActor;
	TArray<TSharedPtr<int32>> ResolutionOptions;
	TArray<TSharedPtr<ECarrierLabMultiHitPolicy>> PolicyOptions;
	TArray<TSharedPtr<ECarrierLabVisualizationLayer>> LayerOptions;

	int32 PendingResolution = 60000;
	int32 PendingPlateCount = 40;
	int32 PendingSeed = 42;
	double PendingStepRate = 2.0;
	ECarrierLabMultiHitPolicy PendingPolicy = ECarrierLabMultiHitPolicy::Centroid;
	ECarrierLabVisualizationLayer PendingLayer = ECarrierLabVisualizationLayer::PlateId;

	ACarrierLabVisualizationActor* GetCarrierActor(const bool bCreateIfMissing);
	void ApplyPanelConfigToActor(ACarrierLabVisualizationActor& Actor) const;
	void RefreshTargetActor();

	FReply OnInitializeClicked();
	FReply OnStepClicked();
	FReply OnPlayPauseClicked();
	FReply OnResampleClicked();
	FReply OnResetClicked();

	TSharedRef<SWidget> BuildControls();
	TSharedRef<SWidget> BuildReadout();
	TSharedRef<SWidget> BuildResolutionSelector();
	TSharedRef<SWidget> BuildPolicySelector();
	TSharedRef<SWidget> BuildLayerSelector();

	TSharedRef<SWidget> MakeResolutionWidget(TSharedPtr<int32> Item) const;
	TSharedRef<SWidget> MakePolicyWidget(TSharedPtr<ECarrierLabMultiHitPolicy> Item) const;
	TSharedRef<SWidget> MakeLayerWidget(TSharedPtr<ECarrierLabVisualizationLayer> Item) const;

	void OnResolutionChanged(TSharedPtr<int32> NewValue, ESelectInfo::Type SelectInfo);
	void OnPolicyChanged(TSharedPtr<ECarrierLabMultiHitPolicy> NewValue, ESelectInfo::Type SelectInfo);
	void OnLayerChanged(TSharedPtr<ECarrierLabVisualizationLayer> NewValue, ESelectInfo::Type SelectInfo);
	void OnPlateCountChanged(int32 NewValue);
	void OnSeedChanged(int32 NewValue);
	void OnStepRateChanged(float NewValue);

	TOptional<int32> GetPlateCount() const;
	TOptional<int32> GetSeed() const;
	float GetStepRate() const;

	FText GetTargetText() const;
	FText GetPlayPauseText() const;
	FText GetResolutionText() const;
	FText GetPolicyText() const;
	FText GetLayerText() const;
	FText GetStepRateText() const;
	FText GetReadoutText() const;

	static FString PolicyToString(ECarrierLabMultiHitPolicy Policy);
	static FString LayerToString(ECarrierLabVisualizationLayer Layer);
};
