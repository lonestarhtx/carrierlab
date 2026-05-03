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
	struct FLiveProjectionSnapshot
	{
		bool bHasSnapshot = false;
		bool bInitialized = false;
		bool bPlaying = false;
		FString ActorLabel;
		FDateTime CapturedAt;
		FCarrierLabVisualizationMetrics Metrics;
	};

	struct FLiveContactDetectionSnapshot
	{
		bool bHasAttempt = false;
		bool bSucceeded = false;
		FString ErrorText;
		FString TargetActorLabel;
		FDateTime CapturedAt;
		FString ProjectionHashBefore;
		FString ProjectionHashAfter;
		FString StateHashBefore;
		FString StateHashAfter;
		bool bProjectionHashUnchanged = false;
		bool bStateHashUnchanged = false;
		FCarrierLabPhaseIIContactMetrics Metrics;
	};

	struct FSlice1ArtifactFixtureSummary
	{
		FString Fixture;
		int32 ReplayCount = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 Step = 0;
		int32 RawEvidenceSampleCount = 0;
		int32 ContactRecordCount = 0;
		int32 ConvergentContactCount = 0;
		int32 DivergentContactCount = 0;
		int32 TransformLowMarginContactCount = 0;
		int32 ThirdPlateContactCount = 0;
		int32 SubductionCandidateCount = 0;
		int32 BoundaryEvidenceCount = 0;
		double PairSignedConvergenceVelocity = 0.0;
		double ContactDetectionSeconds = 0.0;
		FString ContactLogHash;
		bool bContactHashMatch = true;
		bool bProjectionHashUnchanged = true;
		bool bStateHashUnchanged = true;
	};

	struct FSlice1ArtifactSnapshot
	{
		bool bHasArtifact = false;
		FString StatusText = TEXT("No Slice 1 artifact loaded.");
		FString MetricsPath;
		FString CheckpointReportPath;
		FDateTime ArtifactTimestamp;
		FDateTime LoadedAt;
		TArray<FSlice1ArtifactFixtureSummary> FixtureRows;
		bool bReplayHashMatch = false;
		bool bNoSubductionControls = false;
		bool bConvergenceSign = false;
		bool bThirdPlateExplicitness = false;
	};

	TWeakObjectPtr<ACarrierLabVisualizationActor> TargetActor;
	TArray<TSharedPtr<int32>> ResolutionOptions;
	TArray<TSharedPtr<ECarrierLabMultiHitPolicy>> PolicyOptions;
	TArray<TSharedPtr<ECarrierLabVisualizationLayer>> LayerOptions;
	FLiveProjectionSnapshot LiveProjection;
	FLiveContactDetectionSnapshot LiveContact;
	FSlice1ArtifactSnapshot Slice1Artifact;

	int32 PendingResolution = 60000;
	int32 PendingPlateCount = 40;
	int32 PendingSeed = 42;
	double PendingStepRate = 2.0;
	bool bPendingAutoResample = false;
	ECarrierLabMultiHitPolicy PendingPolicy = ECarrierLabMultiHitPolicy::Centroid;
	ECarrierLabVisualizationLayer PendingLayer = ECarrierLabVisualizationLayer::PlateId;
	double LastLiveRefreshSeconds = -1000.0;
	int32 TargetActorCount = 0;
	FString TargetSourceText;
	FString TargetWarningText;

	ACarrierLabVisualizationActor* GetCarrierActor(const bool bCreateIfMissing);
	void ApplyPanelConfigToActor(ACarrierLabVisualizationActor& Actor) const;
	void RefreshTargetActor();
	void CaptureLiveProjectionSnapshot();
	void LoadLatestSlice1Artifact();
	bool ParseSlice1Artifact(const FString& MetricsPath, const FDateTime& ArtifactTimestamp, FString& OutError);

	FReply OnInitializeClicked();
	FReply OnStepClicked();
	FReply OnPlayPauseClicked();
	FReply OnResampleClicked();
	FReply OnResetClicked();
	FReply OnDetectContactsClicked();
	FReply OnLoadArtifactClicked();

	TSharedRef<SWidget> BuildSection(const FText& Title, const TSharedRef<SWidget>& Body) const;
	TSharedRef<SWidget> BuildTargetSection();
	TSharedRef<SWidget> BuildCarrierControls();
	TSharedRef<SWidget> BuildLiveProjectionSection();
	TSharedRef<SWidget> BuildLiveContactsSection();
	TSharedRef<SWidget> BuildGateSection();
	TSharedRef<SWidget> BuildArtifactSection();
	TSharedRef<SWidget> BuildControls();
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
	FText GetTargetWarningText() const;
	EVisibility GetTargetWarningVisibility() const;
	FText GetPlayPauseText() const;
	FText GetResolutionText() const;
	FText GetPolicyText() const;
	FText GetLayerText() const;
	FText GetStepRateText() const;
	FText GetLiveProjectionSummaryText() const;
	FText GetLiveTimingText() const;
	FText GetLiveContactSummaryText() const;
	FText GetGateSummaryText() const;
	FText GetArtifactProvenanceText() const;
	FText GetArtifactRowsText() const;

	static FString PolicyToString(ECarrierLabMultiHitPolicy Policy);
	static FString LayerToString(ECarrierLabVisualizationLayer Layer);
	static FString FormatTimestamp(const FDateTime& Timestamp);
};
