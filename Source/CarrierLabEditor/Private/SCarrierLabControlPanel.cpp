// Copyright Epic Games, Inc. All Rights Reserved.

#include "SCarrierLabControlPanel.h"

#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CarrierLabControlPanel"

void SCarrierLabControlPanel::Construct(const FArguments& InArgs)
{
	ResolutionOptions = {
		MakeShared<int32>(60000),
		MakeShared<int32>(100000),
		MakeShared<int32>(250000),
		MakeShared<int32>(500000)
	};
	PolicyOptions = {
		MakeShared<ECarrierLabMultiHitPolicy>(ECarrierLabMultiHitPolicy::Centroid),
		MakeShared<ECarrierLabMultiHitPolicy>(ECarrierLabMultiHitPolicy::SyntheticSubduction),
		MakeShared<ECarrierLabMultiHitPolicy>(ECarrierLabMultiHitPolicy::RandomSeeded)
	};
	LayerOptions = {
		MakeShared<ECarrierLabVisualizationLayer>(ECarrierLabVisualizationLayer::PlateId),
		MakeShared<ECarrierLabVisualizationLayer>(ECarrierLabVisualizationLayer::ContinentalFraction),
		MakeShared<ECarrierLabVisualizationLayer>(ECarrierLabVisualizationLayer::MissMask),
		MakeShared<ECarrierLabVisualizationLayer>(ECarrierLabVisualizationLayer::OverlapMask),
		MakeShared<ECarrierLabVisualizationLayer>(ECarrierLabVisualizationLayer::BoundaryMask),
		MakeShared<ECarrierLabVisualizationLayer>(ECarrierLabVisualizationLayer::DriftError)
	};

	RefreshTargetActor();

	ChildSlot
	[
		SNew(SBorder)
		.Padding(10.0f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("Title", "CarrierLab Control Panel"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 4.0f, 0.0f, 8.0f)
				[
					SNew(STextBlock)
					.Text(this, &SCarrierLabControlPanel::GetTargetText)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.80f, 0.90f, 1.0f)))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					BuildControls()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.0f, 10.0f)
				[
					SNew(SSeparator)
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					BuildReadout()
				]
			]
		]
	];
}

void SCarrierLabControlPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (!TargetActor.IsValid())
	{
		RefreshTargetActor();
	}
}

ACarrierLabVisualizationActor* SCarrierLabControlPanel::GetCarrierActor(const bool bCreateIfMissing)
{
	if (TargetActor.IsValid())
	{
		return TargetActor.Get();
	}

	UWorld* World = GEditor != nullptr ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World == nullptr)
	{
		return nullptr;
	}

	if (GEditor != nullptr)
	{
		if (USelection* Selection = GEditor->GetSelectedActors())
		{
			for (FSelectionIterator It(*Selection); It; ++It)
			{
				if (ACarrierLabVisualizationActor* SelectedActor = Cast<ACarrierLabVisualizationActor>(*It))
				{
					TargetActor = SelectedActor;
					return SelectedActor;
				}
			}
		}
	}

	ACarrierLabVisualizationActor* FirstActor = nullptr;
	int32 ActorCount = 0;
	for (TActorIterator<ACarrierLabVisualizationActor> It(World); It; ++It)
	{
		++ActorCount;
		if (FirstActor == nullptr)
		{
			FirstActor = *It;
		}
	}
	if (FirstActor != nullptr)
	{
		TargetActor = FirstActor;
		return FirstActor;
	}

	if (!bCreateIfMissing)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = MakeUniqueObjectName(World, ACarrierLabVisualizationActor::StaticClass(), TEXT("CarrierLabCarrier"));
	ACarrierLabVisualizationActor* NewActor = World->SpawnActor<ACarrierLabVisualizationActor>(
		ACarrierLabVisualizationActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	if (NewActor != nullptr)
	{
		NewActor->bAutoInitialize = false;
		TargetActor = NewActor;
		World->MarkPackageDirty();
	}
	return NewActor;
}

void SCarrierLabControlPanel::ApplyPanelConfigToActor(ACarrierLabVisualizationActor& Actor) const
{
	Actor.SampleCount = PendingResolution;
	Actor.PlateCount = PendingPlateCount;
	Actor.Seed = PendingSeed;
	Actor.StepsPerSecond = PendingStepRate;
	Actor.SetMultiHitPolicy(PendingPolicy);
	Actor.SetVisualizationLayer(PendingLayer);
}

void SCarrierLabControlPanel::RefreshTargetActor()
{
	ACarrierLabVisualizationActor* Actor = GetCarrierActor(false);
	if (Actor == nullptr)
	{
		return;
	}
	PendingResolution = Actor->SampleCount;
	PendingPlateCount = Actor->PlateCount;
	PendingSeed = Actor->Seed;
	PendingStepRate = Actor->StepsPerSecond;
	PendingPolicy = Actor->MultiHitPolicy;
	PendingLayer = Actor->VisualizationLayer;
}

FReply SCarrierLabControlPanel::OnInitializeClicked()
{
	if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(true))
	{
		ApplyPanelConfigToActor(*Actor);
		Actor->InitializeCarrier();
	}
	return FReply::Handled();
}

FReply SCarrierLabControlPanel::OnStepClicked()
{
	if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(false))
	{
		ApplyPanelConfigToActor(*Actor);
		Actor->StepOnce();
	}
	return FReply::Handled();
}

FReply SCarrierLabControlPanel::OnPlayPauseClicked()
{
	if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(false))
	{
		ApplyPanelConfigToActor(*Actor);
		Actor->TogglePlay();
	}
	return FReply::Handled();
}

FReply SCarrierLabControlPanel::OnResampleClicked()
{
	if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(false))
	{
		ApplyPanelConfigToActor(*Actor);
		Actor->ApplyResampleEvent();
	}
	return FReply::Handled();
}

FReply SCarrierLabControlPanel::OnResetClicked()
{
	if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(true))
	{
		ApplyPanelConfigToActor(*Actor);
		Actor->ResetCarrier();
	}
	return FReply::Handled();
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildControls()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 3.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SButton).Text(LOCTEXT("Initialize", "Initialize")).OnClicked(this, &SCarrierLabControlPanel::OnInitializeClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SButton).Text(LOCTEXT("Step", "Step")).OnClicked(this, &SCarrierLabControlPanel::OnStepClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SButton).Text(this, &SCarrierLabControlPanel::GetPlayPauseText).OnClicked(this, &SCarrierLabControlPanel::OnPlayPauseClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SButton).Text(LOCTEXT("ResampleNow", "Resample Now")).OnClicked(this, &SCarrierLabControlPanel::OnResampleClicked)
			]
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton).Text(LOCTEXT("Reset", "Reset")).OnClicked(this, &SCarrierLabControlPanel::OnResetClicked)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			BuildResolutionSelector()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 6.0f, 0.0f)
			[
				SNew(SNumericEntryBox<int32>)
				.LabelVAlign(VAlign_Center)
				.Label()
				[
					SNew(STextBlock).Text(LOCTEXT("PlateCount", "Plates"))
				]
				.MinValue(1)
				.MaxValue(63)
				.Value(this, &SCarrierLabControlPanel::GetPlateCount)
				.OnValueChanged(this, &SCarrierLabControlPanel::OnPlateCountChanged)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SNumericEntryBox<int32>)
				.LabelVAlign(VAlign_Center)
				.Label()
				[
					SNew(STextBlock).Text(LOCTEXT("Seed", "Seed"))
				]
				.Value(this, &SCarrierLabControlPanel::GetSeed)
				.OnValueChanged(this, &SCarrierLabControlPanel::OnSeedChanged)
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			BuildPolicySelector()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			BuildLayerSelector()
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).Text(this, &SCarrierLabControlPanel::GetStepRateText)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SSlider)
				.MinValue(0.1f)
				.MaxValue(30.0f)
				.Value(this, &SCarrierLabControlPanel::GetStepRate)
				.OnValueChanged(this, &SCarrierLabControlPanel::OnStepRateChanged)
			]
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildReadout()
{
	return SNew(SBorder)
		.Padding(8.0f)
		[
			SNew(STextBlock)
			.Text(this, &SCarrierLabControlPanel::GetReadoutText)
			.AutoWrapText(true)
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildResolutionSelector()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("Resolution", "Resolution"))
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(SComboBox<TSharedPtr<int32>>)
			.OptionsSource(&ResolutionOptions)
			.OnGenerateWidget(this, &SCarrierLabControlPanel::MakeResolutionWidget)
			.OnSelectionChanged(this, &SCarrierLabControlPanel::OnResolutionChanged)
			[
				SNew(STextBlock).Text(this, &SCarrierLabControlPanel::GetResolutionText)
			]
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildPolicySelector()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("Policy", "Multi-hit policy"))
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(SComboBox<TSharedPtr<ECarrierLabMultiHitPolicy>>)
			.OptionsSource(&PolicyOptions)
			.OnGenerateWidget(this, &SCarrierLabControlPanel::MakePolicyWidget)
			.OnSelectionChanged(this, &SCarrierLabControlPanel::OnPolicyChanged)
			[
				SNew(STextBlock).Text(this, &SCarrierLabControlPanel::GetPolicyText)
			]
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildLayerSelector()
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(STextBlock).Text(LOCTEXT("Layer", "Layer"))
		]
		+ SHorizontalBox::Slot().FillWidth(1.0f)
		[
			SNew(SComboBox<TSharedPtr<ECarrierLabVisualizationLayer>>)
			.OptionsSource(&LayerOptions)
			.OnGenerateWidget(this, &SCarrierLabControlPanel::MakeLayerWidget)
			.OnSelectionChanged(this, &SCarrierLabControlPanel::OnLayerChanged)
			[
				SNew(STextBlock).Text(this, &SCarrierLabControlPanel::GetLayerText)
			]
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::MakeResolutionWidget(TSharedPtr<int32> Item) const
{
	return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? FString::Printf(TEXT("%d"), *Item) : TEXT("")));
}

TSharedRef<SWidget> SCarrierLabControlPanel::MakePolicyWidget(TSharedPtr<ECarrierLabMultiHitPolicy> Item) const
{
	return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? PolicyToString(*Item) : FString()));
}

TSharedRef<SWidget> SCarrierLabControlPanel::MakeLayerWidget(TSharedPtr<ECarrierLabVisualizationLayer> Item) const
{
	return SNew(STextBlock).Text(FText::FromString(Item.IsValid() ? LayerToString(*Item) : FString()));
}

void SCarrierLabControlPanel::OnResolutionChanged(TSharedPtr<int32> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		PendingResolution = *NewValue;
		if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(false))
		{
			Actor->SampleCount = PendingResolution;
		}
	}
}

void SCarrierLabControlPanel::OnPolicyChanged(TSharedPtr<ECarrierLabMultiHitPolicy> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		PendingPolicy = *NewValue;
		if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(false))
		{
			Actor->SetMultiHitPolicy(PendingPolicy);
		}
	}
}

void SCarrierLabControlPanel::OnLayerChanged(TSharedPtr<ECarrierLabVisualizationLayer> NewValue, ESelectInfo::Type SelectInfo)
{
	if (NewValue.IsValid())
	{
		PendingLayer = *NewValue;
		if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(false))
		{
			Actor->SetVisualizationLayer(PendingLayer);
		}
	}
}

void SCarrierLabControlPanel::OnPlateCountChanged(const int32 NewValue)
{
	PendingPlateCount = FMath::Clamp(NewValue, 1, 63);
	if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(false))
	{
		Actor->PlateCount = PendingPlateCount;
	}
}

void SCarrierLabControlPanel::OnSeedChanged(const int32 NewValue)
{
	PendingSeed = NewValue;
	if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(false))
	{
		Actor->Seed = PendingSeed;
	}
}

void SCarrierLabControlPanel::OnStepRateChanged(const float NewValue)
{
	PendingStepRate = FMath::Clamp(static_cast<double>(NewValue), 0.1, 30.0);
	if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(false))
	{
		Actor->StepsPerSecond = PendingStepRate;
	}
}

TOptional<int32> SCarrierLabControlPanel::GetPlateCount() const
{
	return PendingPlateCount;
}

TOptional<int32> SCarrierLabControlPanel::GetSeed() const
{
	return PendingSeed;
}

float SCarrierLabControlPanel::GetStepRate() const
{
	return static_cast<float>(PendingStepRate);
}

FText SCarrierLabControlPanel::GetTargetText() const
{
	if (TargetActor.IsValid())
	{
		return FText::FromString(FString::Printf(TEXT("Target: %s"), *TargetActor->GetActorLabel()));
	}
	return LOCTEXT("NoTarget", "Target: no ACarrierLabVisualizationActor found; Initialize or Reset creates one.");
}

FText SCarrierLabControlPanel::GetPlayPauseText() const
{
	return TargetActor.IsValid() && TargetActor->IsPlaying()
		? LOCTEXT("Pause", "Pause")
		: LOCTEXT("Play", "Play");
}

FText SCarrierLabControlPanel::GetResolutionText() const
{
	return FText::FromString(FString::Printf(TEXT("%d"), PendingResolution));
}

FText SCarrierLabControlPanel::GetPolicyText() const
{
	return FText::FromString(PolicyToString(PendingPolicy));
}

FText SCarrierLabControlPanel::GetLayerText() const
{
	return FText::FromString(LayerToString(PendingLayer));
}

FText SCarrierLabControlPanel::GetStepRateText() const
{
	return FText::FromString(FString::Printf(TEXT("Play step rate: %.2f steps/s"), PendingStepRate));
}

FText SCarrierLabControlPanel::GetReadoutText() const
{
	if (!TargetActor.IsValid())
	{
		return LOCTEXT("NoReadout", "No carrier actor is currently targeted.");
	}
	const FCarrierLabVisualizationMetrics& Metrics = TargetActor->CurrentMetrics;
	const double MissPct = Metrics.SampleCount > 0 ? 100.0 * static_cast<double>(Metrics.RawMissCount) / static_cast<double>(Metrics.SampleCount) : 0.0;
	const double MultiPct = Metrics.SampleCount > 0 ? 100.0 * static_cast<double>(Metrics.RawMultiHitCount) / static_cast<double>(Metrics.SampleCount) : 0.0;
	return FText::FromString(FString::Printf(
		TEXT("step: %d\nnext resample step: %d\nAuthCAF: %.6f\nProjectedCAF: %.6f\nmiss: %.3f%%\nmulti-hit: %.3f%%\ndrift mean: %.9f km\ndrift p95: %.9f km\nNaN/Inf: %d\nlast hash: %s"),
		Metrics.Step,
		Metrics.NextResampleStep,
		Metrics.AuthoritativeCAF,
		Metrics.ProjectedCAF,
		MissPct,
		MultiPct,
		Metrics.DriftErrorMeanKm,
		Metrics.DriftErrorP95Km,
		Metrics.NaNOrInfCount,
		*Metrics.LastHash));
}

FString SCarrierLabControlPanel::PolicyToString(const ECarrierLabMultiHitPolicy Policy)
{
	switch (Policy)
	{
	case ECarrierLabMultiHitPolicy::SyntheticSubduction:
		return TEXT("synthetic");
	case ECarrierLabMultiHitPolicy::RandomSeeded:
		return TEXT("random");
	case ECarrierLabMultiHitPolicy::Centroid:
	default:
		return TEXT("centroid");
	}
}

FString SCarrierLabControlPanel::LayerToString(const ECarrierLabVisualizationLayer Layer)
{
	switch (Layer)
	{
	case ECarrierLabVisualizationLayer::ContinentalFraction:
		return TEXT("ContinentalFraction");
	case ECarrierLabVisualizationLayer::MissMask:
		return TEXT("Miss");
	case ECarrierLabVisualizationLayer::OverlapMask:
		return TEXT("Overlap");
	case ECarrierLabVisualizationLayer::BoundaryMask:
		return TEXT("Boundary");
	case ECarrierLabVisualizationLayer::DriftError:
		return TEXT("DriftError");
	case ECarrierLabVisualizationLayer::PlateId:
	default:
		return TEXT("PlateId");
	}
}

#undef LOCTEXT_NAMESPACE
