// Copyright Epic Games, Inc. All Rights Reserved.

#include "SCarrierLabControlPanel.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CarrierLabControlPanel"

namespace
{
	constexpr double LiveRefreshIntervalSeconds = 0.25;
	constexpr double PhaseIISignMargin = 1.0e-6;

	FString JsonStringField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
	{
		FString Value;
		if (Object.IsValid())
		{
			Object->TryGetStringField(FieldName, Value);
		}
		return Value;
	}

	int32 JsonIntField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
	{
		double Value = 0.0;
		if (Object.IsValid())
		{
			Object->TryGetNumberField(FieldName, Value);
		}
		return static_cast<int32>(Value);
	}

	double JsonDoubleField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
	{
		double Value = 0.0;
		if (Object.IsValid())
		{
			Object->TryGetNumberField(FieldName, Value);
		}
		return Value;
	}

	bool JsonBoolField(const TSharedPtr<FJsonObject>& Object, const TCHAR* FieldName)
	{
		bool bValue = false;
		if (Object.IsValid())
		{
			Object->TryGetBoolField(FieldName, bValue);
		}
		return bValue;
	}

	FString PassFail(const bool bPass)
	{
		return bPass ? TEXT("PASS") : TEXT("FAIL");
	}

	FString YesNo(const bool bValue)
	{
		return bValue ? TEXT("yes") : TEXT("no");
	}

	FString PercentString(const int32 Count, const int32 Total)
	{
		const double Percent = Total > 0
			? 100.0 * static_cast<double>(Count) / static_cast<double>(Total)
			: 0.0;
		return FString::Printf(TEXT("%.3f%%"), Percent);
	}
}

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
		MakeShared<ECarrierLabVisualizationLayer>(ECarrierLabVisualizationLayer::DriftError),
		MakeShared<ECarrierLabVisualizationLayer>(ECarrierLabVisualizationLayer::ElevationHeatmap),
		MakeShared<ECarrierLabVisualizationLayer>(ECarrierLabVisualizationLayer::SubductionMask),
		MakeShared<ECarrierLabVisualizationLayer>(ECarrierLabVisualizationLayer::DistanceToFrontHeatmap)
	};

	RefreshTargetActor();
	CaptureLiveProjectionSnapshot();
	LoadLatestSlice1Artifact();

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
					.Text(LOCTEXT("Title", "CarrierLab Diagnostics"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 16))
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					BuildTargetSection()
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					BuildCarrierControls()
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					BuildLiveProjectionSection()
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					BuildLiveContactsSection()
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					BuildGateSection()
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					BuildArtifactSection()
				]
			]
		]
	];
}

void SCarrierLabControlPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
	if (InCurrentTime - LastLiveRefreshSeconds >= LiveRefreshIntervalSeconds)
	{
		RefreshTargetActor();
		CaptureLiveProjectionSnapshot();
		LastLiveRefreshSeconds = InCurrentTime;
	}
}

ACarrierLabVisualizationActor* SCarrierLabControlPanel::GetCarrierActor(const bool bCreateIfMissing)
{
	TargetActorCount = 0;
	TargetSourceText = TEXT("none");
	TargetWarningText.Reset();
	TargetActor.Reset();

	UWorld* World = GEditor != nullptr ? GEditor->GetEditorWorldContext().World() : nullptr;
	if (World == nullptr)
	{
		TargetWarningText = TEXT("No editor world is available.");
		return nullptr;
	}

	ACarrierLabVisualizationActor* SelectedActor = nullptr;
	if (GEditor != nullptr)
	{
		if (USelection* Selection = GEditor->GetSelectedActors())
		{
			for (FSelectionIterator It(*Selection); It; ++It)
			{
				if (ACarrierLabVisualizationActor* Candidate = Cast<ACarrierLabVisualizationActor>(*It))
				{
					SelectedActor = Candidate;
					break;
				}
			}
		}
	}

	ACarrierLabVisualizationActor* FirstActor = nullptr;
	for (TActorIterator<ACarrierLabVisualizationActor> It(World); It; ++It)
	{
		++TargetActorCount;
		if (FirstActor == nullptr)
		{
			FirstActor = *It;
		}
	}

	if (SelectedActor != nullptr)
	{
		TargetActor = SelectedActor;
		TargetSourceText = TEXT("selected actor");
	}
	else if (FirstActor != nullptr)
	{
		TargetActor = FirstActor;
		TargetSourceText = TEXT("first actor in world");
	}
	else if (bCreateIfMissing)
	{
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
			TargetSourceText = TEXT("created actor");
			TargetActorCount = 1;
			World->MarkPackageDirty();
		}
	}

	if (TargetActorCount > 1)
	{
		TargetWarningText = FString::Printf(TEXT("%d actors found, using %s."), TargetActorCount, *TargetSourceText);
	}
	else if (!TargetActor.IsValid())
	{
		TargetWarningText = TEXT("No ACarrierLabVisualizationActor found; Initialize or Reset creates one.");
	}

	return TargetActor.Get();
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

void SCarrierLabControlPanel::CaptureLiveProjectionSnapshot()
{
	if (!TargetActor.IsValid())
	{
		LiveProjection = FLiveProjectionSnapshot();
		return;
	}

	LiveProjection.bHasSnapshot = true;
	LiveProjection.bInitialized = TargetActor->IsCarrierInitialized();
	LiveProjection.bPlaying = TargetActor->IsPlaying();
	LiveProjection.ActorLabel = TargetActor->GetActorLabel();
	LiveProjection.CapturedAt = FDateTime::UtcNow();
	LiveProjection.Metrics = TargetActor->CurrentMetrics;
}

void SCarrierLabControlPanel::LoadLatestSlice1Artifact()
{
	Slice1Artifact = FSlice1ArtifactSnapshot();

	const FString SliceRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseII"), TEXT("Slice1")));
	TArray<FString> MetricsFiles;
	IFileManager::Get().FindFilesRecursive(MetricsFiles, *SliceRoot, TEXT("metrics.jsonl"), true, false);
	if (MetricsFiles.Num() == 0)
	{
		Slice1Artifact.StatusText = TEXT("No Slice 1 artifact found");
		return;
	}

	FString LatestMetricsPath;
	FDateTime LatestTimestamp = FDateTime::MinValue();
	for (const FString& MetricsPath : MetricsFiles)
	{
		const FFileStatData StatData = IFileManager::Get().GetStatData(*MetricsPath);
		if (StatData.bIsValid && StatData.ModificationTime > LatestTimestamp)
		{
			LatestTimestamp = StatData.ModificationTime;
			LatestMetricsPath = MetricsPath;
		}
	}

	if (LatestMetricsPath.IsEmpty())
	{
		Slice1Artifact.StatusText = TEXT("No Slice 1 artifact found");
		return;
	}

	FString ParseError;
	if (!ParseSlice1Artifact(LatestMetricsPath, LatestTimestamp, ParseError))
	{
		Slice1Artifact.StatusText = FString::Printf(TEXT("Latest Slice 1 artifact could not be loaded: %s"), *ParseError);
	}
}

bool SCarrierLabControlPanel::ParseSlice1Artifact(const FString& MetricsPath, const FDateTime& ArtifactTimestamp, FString& OutError)
{
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *MetricsPath))
	{
		OutError = FString::Printf(TEXT("could not read %s"), *MetricsPath);
		return false;
	}

	Slice1Artifact = FSlice1ArtifactSnapshot();
	Slice1Artifact.bHasArtifact = true;
	Slice1Artifact.MetricsPath = FPaths::ConvertRelativePathToFull(MetricsPath);
	Slice1Artifact.ArtifactTimestamp = ArtifactTimestamp;
	Slice1Artifact.LoadedAt = FDateTime::UtcNow();
	Slice1Artifact.StatusText = TEXT("latest loaded artifact");

	const FString ReportPath = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectDir(), TEXT("docs"), TEXT("checkpoints"), TEXT("phase-ii-slice-1-report.md")));
	if (IFileManager::Get().FileExists(*ReportPath))
	{
		Slice1Artifact.CheckpointReportPath = ReportPath;
	}

	TArray<FString> Lines;
	FileContents.ParseIntoArrayLines(Lines, false);
	TMap<FString, int32> FixtureIndexByName;
	for (const FString& Line : Lines)
	{
		if (Line.TrimStartAndEnd().IsEmpty())
		{
			continue;
		}

		TSharedPtr<FJsonObject> JsonObject;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Line);
		if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
		{
			continue;
		}

		const FString Fixture = JsonStringField(JsonObject, TEXT("fixture"));
		if (Fixture.IsEmpty())
		{
			continue;
		}

		int32* ExistingIndex = FixtureIndexByName.Find(Fixture);
		if (ExistingIndex == nullptr)
		{
			ExistingIndex = &FixtureIndexByName.Add(Fixture, Slice1Artifact.FixtureRows.Num());
			FSlice1ArtifactFixtureSummary& NewRow = Slice1Artifact.FixtureRows.AddDefaulted_GetRef();
			NewRow.Fixture = Fixture;
		}

		FSlice1ArtifactFixtureSummary& Row = Slice1Artifact.FixtureRows[*ExistingIndex];
		const FString ContactHash = JsonStringField(JsonObject, TEXT("contact_log_hash"));
		const bool bProjectionHashUnchanged = JsonBoolField(JsonObject, TEXT("projection_hash_unchanged"));
		const bool bStateHashUnchanged = JsonBoolField(JsonObject, TEXT("state_hash_unchanged"));
		if (Row.ReplayCount == 0)
		{
			Row.SampleCount = JsonIntField(JsonObject, TEXT("sample_count"));
			Row.PlateCount = JsonIntField(JsonObject, TEXT("plate_count"));
			Row.Step = JsonIntField(JsonObject, TEXT("step"));
			Row.RawEvidenceSampleCount = JsonIntField(JsonObject, TEXT("raw_evidence_sample_count"));
			Row.ContactRecordCount = JsonIntField(JsonObject, TEXT("contact_record_count"));
			Row.ConvergentContactCount = JsonIntField(JsonObject, TEXT("convergent_contact_count"));
			Row.DivergentContactCount = JsonIntField(JsonObject, TEXT("divergent_contact_count"));
			Row.TransformLowMarginContactCount = JsonIntField(JsonObject, TEXT("transform_low_margin_contact_count"));
			Row.ThirdPlateContactCount = JsonIntField(JsonObject, TEXT("third_plate_contact_count"));
			Row.SubductionCandidateCount = JsonIntField(JsonObject, TEXT("subduction_candidate_count"));
			Row.BoundaryEvidenceCount = JsonIntField(JsonObject, TEXT("boundary_evidence_count"));
			Row.PairSignedConvergenceVelocity = JsonDoubleField(JsonObject, TEXT("pair_signed_convergence_velocity"));
			Row.ContactDetectionSeconds = JsonDoubleField(JsonObject, TEXT("contact_detection_seconds"));
			Row.ContactLogHash = ContactHash;
			Row.bProjectionHashUnchanged = bProjectionHashUnchanged;
			Row.bStateHashUnchanged = bStateHashUnchanged;
		}
		else
		{
			Row.bContactHashMatch = Row.bContactHashMatch && Row.ContactLogHash == ContactHash;
			Row.bProjectionHashUnchanged = Row.bProjectionHashUnchanged && bProjectionHashUnchanged;
			Row.bStateHashUnchanged = Row.bStateHashUnchanged && bStateHashUnchanged;
			Row.ContactDetectionSeconds =
				((Row.ContactDetectionSeconds * static_cast<double>(Row.ReplayCount)) +
				JsonDoubleField(JsonObject, TEXT("contact_detection_seconds"))) /
				static_cast<double>(Row.ReplayCount + 1);
		}
		++Row.ReplayCount;
	}

	if (Slice1Artifact.FixtureRows.Num() == 0)
	{
		OutError = TEXT("metrics.jsonl did not contain fixture rows");
		Slice1Artifact.bHasArtifact = false;
		return false;
	}

	Slice1Artifact.bReplayHashMatch = true;
	for (const FSlice1ArtifactFixtureSummary& Row : Slice1Artifact.FixtureRows)
	{
		Slice1Artifact.bReplayHashMatch = Slice1Artifact.bReplayHashMatch && Row.ReplayCount >= 2 && Row.bContactHashMatch;
		Slice1Artifact.bThirdPlateExplicitness = Slice1Artifact.bThirdPlateExplicitness || Row.ThirdPlateContactCount > 0;
	}

	bool bFoundZeroMotion = false;
	bool bFoundSinglePlate = false;
	bool bFoundForcedDivergence = false;
	bool bZeroMotionNoSubduction = false;
	bool bSinglePlateNoSubduction = false;
	bool bForcedDivergenceNoSubduction = false;
	bool bForcedConvergencePositive = false;
	bool bForcedDivergenceNegative = false;
	for (const FSlice1ArtifactFixtureSummary& Row : Slice1Artifact.FixtureRows)
	{
		if (Row.Fixture.Equals(TEXT("zero_motion"), ESearchCase::IgnoreCase))
		{
			bFoundZeroMotion = true;
			bZeroMotionNoSubduction = Row.SubductionCandidateCount == 0;
		}
		else if (Row.Fixture.Equals(TEXT("single_plate"), ESearchCase::IgnoreCase))
		{
			bFoundSinglePlate = true;
			bSinglePlateNoSubduction = Row.SubductionCandidateCount == 0;
		}
		else if (Row.Fixture.Equals(TEXT("forced_divergence"), ESearchCase::IgnoreCase))
		{
			bFoundForcedDivergence = true;
			bForcedDivergenceNoSubduction = Row.SubductionCandidateCount == 0;
			bForcedDivergenceNegative = Row.PairSignedConvergenceVelocity < -PhaseIISignMargin;
		}
		else if (Row.Fixture.Equals(TEXT("forced_convergence"), ESearchCase::IgnoreCase))
		{
			bForcedConvergencePositive = Row.PairSignedConvergenceVelocity > PhaseIISignMargin;
		}
	}
	Slice1Artifact.bNoSubductionControls =
		bFoundZeroMotion && bFoundSinglePlate && bFoundForcedDivergence &&
		bZeroMotionNoSubduction && bSinglePlateNoSubduction && bForcedDivergenceNoSubduction;
	Slice1Artifact.bConvergenceSign = bForcedConvergencePositive && bForcedDivergenceNegative;
	return true;
}

FReply SCarrierLabControlPanel::OnInitializeClicked()
{
	if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(true))
	{
		ApplyPanelConfigToActor(*Actor);
		Actor->InitializeCarrier();
		CaptureLiveProjectionSnapshot();
	}
	return FReply::Handled();
}

FReply SCarrierLabControlPanel::OnStepClicked()
{
	if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(false))
	{
		ApplyPanelConfigToActor(*Actor);
		Actor->StepOnce();
		CaptureLiveProjectionSnapshot();
	}
	return FReply::Handled();
}

FReply SCarrierLabControlPanel::OnPlayPauseClicked()
{
	if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(false))
	{
		ApplyPanelConfigToActor(*Actor);
		Actor->TogglePlay();
		CaptureLiveProjectionSnapshot();
	}
	return FReply::Handled();
}

FReply SCarrierLabControlPanel::OnResampleClicked()
{
	if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(false))
	{
		ApplyPanelConfigToActor(*Actor);
		Actor->ApplyResampleEvent();
		CaptureLiveProjectionSnapshot();
	}
	return FReply::Handled();
}

FReply SCarrierLabControlPanel::OnResetClicked()
{
	if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(true))
	{
		ApplyPanelConfigToActor(*Actor);
		Actor->ResetCarrier();
		CaptureLiveProjectionSnapshot();
	}
	return FReply::Handled();
}

FReply SCarrierLabControlPanel::OnDetectContactsClicked()
{
	LiveContact = FLiveContactDetectionSnapshot();
	LiveContact.bHasAttempt = true;
	LiveContact.CapturedAt = FDateTime::UtcNow();

	ACarrierLabVisualizationActor* Actor = GetCarrierActor(false);
	if (Actor == nullptr)
	{
		LiveContact.ErrorText = TEXT("No live actor target is available.");
		return FReply::Handled();
	}
	if (!Actor->IsCarrierInitialized())
	{
		LiveContact.TargetActorLabel = Actor->GetActorLabel();
		LiveContact.ErrorText = TEXT("Initialize the actor before read-only contact detection.");
		return FReply::Handled();
	}

	LiveContact.TargetActorLabel = Actor->GetActorLabel();
	LiveContact.ProjectionHashBefore = Actor->CurrentMetrics.LastHash;
	LiveContact.StateHashBefore = Actor->CurrentMetrics.StateHash;

	TArray<FCarrierLabPhaseIIContactRecord> Contacts;
	FCarrierLabPhaseIIContactMetrics ContactMetrics;
	const bool bDetected = Actor->DetectPhaseIIContacts(Contacts, ContactMetrics);
	LiveContact.ProjectionHashAfter = Actor->CurrentMetrics.LastHash;
	LiveContact.StateHashAfter = Actor->CurrentMetrics.StateHash;
	LiveContact.bProjectionHashUnchanged = LiveContact.ProjectionHashBefore == LiveContact.ProjectionHashAfter;
	LiveContact.bStateHashUnchanged = LiveContact.StateHashBefore == LiveContact.StateHashAfter;
	LiveContact.Metrics = ContactMetrics;
	LiveContact.bSucceeded = bDetected;
	if (!bDetected)
	{
		LiveContact.ErrorText = TEXT("DetectPhaseIIContacts returned false.");
	}

	CaptureLiveProjectionSnapshot();
	return FReply::Handled();
}

FReply SCarrierLabControlPanel::OnLoadArtifactClicked()
{
	LoadLatestSlice1Artifact();
	return FReply::Handled();
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildSection(const FText& Title, const TSharedRef<SWidget>& Body) const
{
	return SNew(SBorder)
		.Padding(8.0f)
		.BorderBackgroundColor(FLinearColor(0.035f, 0.035f, 0.035f, 1.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Title)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 6.0f)
			[
				SNew(SSeparator)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Body
			]
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildTargetSection()
{
	return BuildSection(
		LOCTEXT("TargetSection", "Target"),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(this, &SCarrierLabControlPanel::GetTargetText)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.80f, 0.90f, 1.0f)))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(this, &SCarrierLabControlPanel::GetTargetWarningText)
			.Visibility(this, &SCarrierLabControlPanel::GetTargetWarningVisibility)
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.75f, 0.25f, 1.0f)))
			.AutoWrapText(true)
		]);
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildCarrierControls()
{
	return BuildSection(
		LOCTEXT("CarrierControlsSection", "Carrier Controls - Mutating"),
		SNew(SVerticalBox)
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
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 6.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MutationNote", "Resample Now mutates carrier state. It is not Phase II read-only evidence."))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.75f, 0.25f, 1.0f)))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			BuildControls()
		]);
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildControls()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
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
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			BuildPolicySelector()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 0.0f)
		[
			BuildLayerSelector()
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
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

TSharedRef<SWidget> SCarrierLabControlPanel::BuildLiveProjectionSection()
{
	return BuildSection(
		LOCTEXT("LiveProjectionSection", "Live Actor - Projection Metrics"),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(this, &SCarrierLabControlPanel::GetLiveProjectionSummaryText)
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(this, &SCarrierLabControlPanel::GetLiveTimingText)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.84f, 0.88f, 1.0f)))
			.AutoWrapText(true)
		]);
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildLiveContactsSection()
{
	return BuildSection(
		LOCTEXT("LiveContactsSection", "Live Actor - Read-Only Contacts"),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("DetectContacts", "Detect Contacts"))
				.OnClicked(this, &SCarrierLabControlPanel::OnDetectContactsClicked)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DetectContactsNote", "Button-only read-only check: captures projection/state hashes before and after contact detection."))
				.ColorAndOpacity(FSlateColor(FLinearColor(0.72f, 0.80f, 0.90f, 1.0f)))
				.AutoWrapText(true)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(this, &SCarrierLabControlPanel::GetLiveContactSummaryText)
			.AutoWrapText(true)
		]);
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildGateSection()
{
	return BuildSection(
		LOCTEXT("GateSection", "Source-Aware Gate Panel"),
		SNew(STextBlock)
		.Text(this, &SCarrierLabControlPanel::GetGateSummaryText)
		.AutoWrapText(true));
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildArtifactSection()
{
	return BuildSection(
		LOCTEXT("ArtifactSection", "Checkpoint Artifact - Slice 1"),
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("LoadLatestArtifact", "Load Latest Artifact"))
				.OnClicked(this, &SCarrierLabControlPanel::OnLoadArtifactClicked)
			]
			+ SHorizontalBox::Slot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(8.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ArtifactNote", "Artifact gates are historical evidence from the latest loaded commandlet run, not current code passed."))
				.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.75f, 0.25f, 1.0f)))
				.AutoWrapText(true)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(this, &SCarrierLabControlPanel::GetArtifactProvenanceText)
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
		[
			SNew(STextBlock)
			.Text(this, &SCarrierLabControlPanel::GetArtifactRowsText)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.84f, 0.88f, 1.0f)))
			.AutoWrapText(true)
		]);
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
		return FText::FromString(FString::Printf(
			TEXT("Target: %s (%s) | initialized: %s | playing: %s"),
			*TargetActor->GetActorLabel(),
			*TargetSourceText,
			TargetActor->IsCarrierInitialized() ? TEXT("yes") : TEXT("no"),
			TargetActor->IsPlaying() ? TEXT("yes") : TEXT("no")));
	}
	return LOCTEXT("NoTarget", "Target: no ACarrierLabVisualizationActor found.");
}

FText SCarrierLabControlPanel::GetTargetWarningText() const
{
	return FText::FromString(TargetWarningText);
}

EVisibility SCarrierLabControlPanel::GetTargetWarningVisibility() const
{
	return TargetWarningText.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
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

FText SCarrierLabControlPanel::GetLiveProjectionSummaryText() const
{
	if (!LiveProjection.bHasSnapshot)
	{
		return LOCTEXT("NoLiveProjection", "No live actor snapshot yet.");
	}
	const FCarrierLabVisualizationMetrics& Metrics = LiveProjection.Metrics;
	return FText::FromString(FString::Printf(
		TEXT("Source: Live Actor snapshot @ %s\n")
		TEXT("actor: %s | initialized: %s | playing: %s | step: %d | next resample: %d | events: %d\n")
		TEXT("samples: %d | plates: %d | miss: %s (%d) | multi-hit: %s (%d) | boundary hits: %d | NaN/Inf: %d\n")
		TEXT("Auth CAF: %.6f | Projected CAF: %.6f | drift mean: %.9f km | drift p95: %.9f km\n")
		TEXT("projection hash: %s | state hash: %s"),
		*FormatTimestamp(LiveProjection.CapturedAt),
		*LiveProjection.ActorLabel,
		LiveProjection.bInitialized ? TEXT("yes") : TEXT("no"),
		LiveProjection.bPlaying ? TEXT("yes") : TEXT("no"),
		Metrics.Step,
		Metrics.NextResampleStep,
		Metrics.EventCount,
		Metrics.SampleCount,
		Metrics.PlateCount,
		*PercentString(Metrics.RawMissCount, Metrics.SampleCount),
		Metrics.RawMissCount,
		*PercentString(Metrics.RawMultiHitCount, Metrics.SampleCount),
		Metrics.RawMultiHitCount,
		Metrics.BoundaryHitCount,
		Metrics.NaNOrInfCount,
		Metrics.AuthoritativeCAF,
		Metrics.ProjectedCAF,
		Metrics.DriftErrorMeanKm,
		Metrics.DriftErrorP95Km,
		*Metrics.LastHash,
		*Metrics.StateHash));
}

FText SCarrierLabControlPanel::GetLiveTimingText() const
{
	if (!LiveProjection.bHasSnapshot)
	{
		return FText::GetEmpty();
	}
	const FCarrierLabVisualizationMetrics& Metrics = LiveProjection.Metrics;
	return FText::FromString(FString::Printf(
		TEXT("Timing: projection %.6fs | BVH %.6fs | query %.6fs | drift %.6fs | boundary %.6fs | hash %.6fs | render %.6fs | resample %.6fs"),
		Metrics.ProjectionSeconds,
		Metrics.BvhBuildSeconds,
		Metrics.ProjectionQuerySeconds,
		Metrics.DriftMetricsSeconds,
		Metrics.BoundaryMaskSeconds,
		Metrics.HashSeconds,
		Metrics.MeshUpdateSeconds,
		Metrics.ResampleEventSeconds));
}

FText SCarrierLabControlPanel::GetLiveContactSummaryText() const
{
	if (!LiveContact.bHasAttempt)
	{
		return LOCTEXT("NoLiveContact", "Detect Contacts has not been run. Contact detection is button-only and read-only.");
	}
	if (!LiveContact.bSucceeded)
	{
		return FText::FromString(FString::Printf(
			TEXT("Source: Live Actor last Detect Contacts @ %s\n")
			TEXT("target: %s\n")
			TEXT("status: FAIL | %s"),
			*FormatTimestamp(LiveContact.CapturedAt),
			*LiveContact.TargetActorLabel,
			*LiveContact.ErrorText));
	}

	const FCarrierLabPhaseIIContactMetrics& Metrics = LiveContact.Metrics;
	return FText::FromString(FString::Printf(
		TEXT("Source: Live Actor last Detect Contacts @ %s\n")
		TEXT("target: %s\n")
		TEXT("projection hash unchanged: %s | before: %s | after: %s\n")
		TEXT("state hash unchanged: %s | before: %s | after: %s\n")
		TEXT("contact hash: %s | contact seconds: %.6f\n")
		TEXT("raw evidence: %d | records: %d | convergent: %d | divergent: %d | low-margin: %d | third-plate: %d | subduction candidates: %d | boundary evidence: %d | NaN/Inf: %d"),
		*FormatTimestamp(LiveContact.CapturedAt),
		*LiveContact.TargetActorLabel,
		*PassFail(LiveContact.bProjectionHashUnchanged),
		*LiveContact.ProjectionHashBefore,
		*LiveContact.ProjectionHashAfter,
		*PassFail(LiveContact.bStateHashUnchanged),
		*LiveContact.StateHashBefore,
		*LiveContact.StateHashAfter,
		*Metrics.ContactLogHash,
		Metrics.ContactDetectionSeconds,
		Metrics.RawEvidenceSampleCount,
		Metrics.ContactRecordCount,
		Metrics.ConvergentContactCount,
		Metrics.DivergentContactCount,
		Metrics.TransformLowMarginContactCount,
		Metrics.ThirdPlateContactCount,
		Metrics.SubductionCandidateCount,
		Metrics.BoundaryEvidenceCount,
		Metrics.NaNOrInfCount));
}

FText SCarrierLabControlPanel::GetGateSummaryText() const
{
	FString Text;
	if (!LiveContact.bHasAttempt)
	{
		Text += TEXT("No mutation | Source: Live Actor last Detect Contacts | Status: not run\n");
	}
	else if (!LiveContact.bSucceeded)
	{
		Text += TEXT("No mutation | Source: Live Actor last Detect Contacts | Status: no valid check\n");
	}
	else
	{
		Text += FString::Printf(
			TEXT("No mutation | Source: Live Actor last Detect Contacts | Status: %s\n"),
			*PassFail(LiveContact.bProjectionHashUnchanged && LiveContact.bStateHashUnchanged));
	}

	if (Slice1Artifact.bHasArtifact)
	{
		Text += FString::Printf(TEXT("Replay contact hash match | Source: Checkpoint Artifact latest loaded artifact | Status: %s\n"), *PassFail(Slice1Artifact.bReplayHashMatch));
		Text += FString::Printf(TEXT("No-subduction controls | Source: Checkpoint Artifact latest loaded artifact | Status: %s\n"), *PassFail(Slice1Artifact.bNoSubductionControls));
		Text += FString::Printf(TEXT("Convergence sign | Source: Checkpoint Artifact latest loaded artifact | Status: %s\n"), *PassFail(Slice1Artifact.bConvergenceSign));
		Text += FString::Printf(TEXT("Third-plate explicitness | Source: Checkpoint Artifact latest loaded artifact | Status: %s"), *PassFail(Slice1Artifact.bThirdPlateExplicitness));
	}
	else
	{
		Text += TEXT("Replay contact hash match | Source: Checkpoint Artifact | Status: no artifact\n");
		Text += TEXT("No-subduction controls | Source: Checkpoint Artifact | Status: no artifact\n");
		Text += TEXT("Convergence sign | Source: Checkpoint Artifact | Status: no artifact\n");
		if (LiveContact.bSucceeded && LiveContact.Metrics.ThirdPlateContactCount > 0)
		{
			Text += TEXT("Third-plate explicitness | Source: Live Actor last Detect Contacts with third-plate evidence | Status: SEEN");
		}
		else
		{
			Text += TEXT("Third-plate explicitness | Source: no loaded artifact | Status: no source");
		}
	}
	return FText::FromString(Text);
}

FText SCarrierLabControlPanel::GetArtifactProvenanceText() const
{
	if (!Slice1Artifact.bHasArtifact)
	{
		return FText::FromString(Slice1Artifact.StatusText.IsEmpty() ? FString(TEXT("No Slice 1 artifact found")) : Slice1Artifact.StatusText);
	}

	return FText::FromString(FString::Printf(
		TEXT("Source: latest loaded artifact (historical evidence, not current code passed)\n")
		TEXT("metrics.jsonl: %s\n")
		TEXT("artifact timestamp: %s\n")
		TEXT("checkpoint report: %s\n")
		TEXT("loaded: %s"),
		*Slice1Artifact.MetricsPath,
		*FormatTimestamp(Slice1Artifact.ArtifactTimestamp),
		Slice1Artifact.CheckpointReportPath.IsEmpty() ? TEXT("not found") : *Slice1Artifact.CheckpointReportPath,
		*FormatTimestamp(Slice1Artifact.LoadedAt)));
}

FText SCarrierLabControlPanel::GetArtifactRowsText() const
{
	if (!Slice1Artifact.bHasArtifact)
	{
		return LOCTEXT("NoArtifactRows", "No artifact fixture rows loaded.");
	}

	FString Text = TEXT("Fixture rows from metrics.jsonl:\n");
	for (const FSlice1ArtifactFixtureSummary& Row : Slice1Artifact.FixtureRows)
	{
		Text += FString::Printf(
			TEXT("%s | replays=%d | replay_hash=%s | no_mutation=%s | records=%d | conv=%d | div=%d | low=%d | third=%d | candidates=%d | sign=%.12f | contact_s=%.6f | hash=%s\n"),
			*Row.Fixture,
			Row.ReplayCount,
			*PassFail(Row.ReplayCount >= 2 && Row.bContactHashMatch),
			*PassFail(Row.bProjectionHashUnchanged && Row.bStateHashUnchanged),
			Row.ContactRecordCount,
			Row.ConvergentContactCount,
			Row.DivergentContactCount,
			Row.TransformLowMarginContactCount,
			Row.ThirdPlateContactCount,
			Row.SubductionCandidateCount,
			Row.PairSignedConvergenceVelocity,
			Row.ContactDetectionSeconds,
			*Row.ContactLogHash);
	}
	return FText::FromString(Text);
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
	case ECarrierLabVisualizationLayer::ElevationHeatmap:
		return TEXT("ElevationHeatmap");
	case ECarrierLabVisualizationLayer::SubductionMask:
		return TEXT("SubductionMask");
	case ECarrierLabVisualizationLayer::DistanceToFrontHeatmap:
		return TEXT("DistanceToFront");
	case ECarrierLabVisualizationLayer::PlateId:
	default:
		return TEXT("PlateId");
	}
}

FString SCarrierLabControlPanel::FormatTimestamp(const FDateTime& Timestamp)
{
	if (Timestamp.GetTicks() <= 0)
	{
		return TEXT("none");
	}
	return Timestamp.ToString(TEXT("%Y-%m-%d %H:%M:%S UTC"));
}

#undef LOCTEXT_NAMESPACE
