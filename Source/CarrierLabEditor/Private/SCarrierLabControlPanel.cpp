// Copyright Epic Games, Inc. All Rights Reserved.

#include "SCarrierLabControlPanel.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
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

	FText SecondsText(const double Seconds)
	{
		return FText::FromString(FString::Printf(TEXT("%.4fs"), Seconds));
	}

	FLinearColor CardBackground()
	{
		return FLinearColor(0.035f, 0.038f, 0.043f, 1.0f);
	}

	FLinearColor ReadOnlyColor()
	{
		return FLinearColor(0.20f, 0.42f, 0.72f, 1.0f);
	}

	FLinearColor MutationColor()
	{
		return FLinearColor(0.72f, 0.32f, 0.24f, 1.0f);
	}

	FLinearColor PassColor()
	{
		return FLinearColor(0.32f, 0.64f, 0.28f, 1.0f);
	}

	FLinearColor NeutralColor()
	{
		return FLinearColor(0.46f, 0.50f, 0.56f, 1.0f);
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
		MakeShared<ECarrierLabVisualizationLayer>(ECarrierLabVisualizationLayer::PhaseIIISummary),
		MakeShared<ECarrierLabVisualizationLayer>(ECarrierLabVisualizationLayer::ElevationHeatmap),
		MakeShared<ECarrierLabVisualizationLayer>(ECarrierLabVisualizationLayer::SubductionMask),
		MakeShared<ECarrierLabVisualizationLayer>(ECarrierLabVisualizationLayer::DistanceToFrontHeatmap)
	};

	RefreshTargetActor();
	CaptureLiveProjectionSnapshot();
	LoadLatestSlice1Artifact();
	LoadLatestMapArtifact();

	ChildSlot
	[
		BuildShell()
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
	Actor.bEnableNaturalResamplingEvents = bPendingAutoResample;
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
	bPendingAutoResample = Actor->bEnableNaturalResamplingEvents;
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

void SCarrierLabControlPanel::LoadLatestMapArtifact()
{
	LatestMapArtifact = FLatestMapArtifactSnapshot();

	const FString MapRoot = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIICMapExport")));
	TArray<FString> MetricsFiles;
	IFileManager::Get().FindFilesRecursive(MetricsFiles, *MapRoot, TEXT("metrics.jsonl"), true, false);
	if (MetricsFiles.Num() == 0)
	{
		LatestMapArtifact.StatusText = TEXT("No Phase III map export artifact found.");
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
		LatestMapArtifact.StatusText = TEXT("No Phase III map export artifact found.");
		return;
	}

	FString ParseError;
	if (!ParseLatestMapArtifact(LatestMetricsPath, LatestTimestamp, ParseError))
	{
		LatestMapArtifact.StatusText = FString::Printf(TEXT("Latest map export could not be loaded: %s"), *ParseError);
	}
}

bool SCarrierLabControlPanel::ParseLatestMapArtifact(const FString& MetricsPath, const FDateTime& ArtifactTimestamp, FString& OutError)
{
	FString FileContents;
	if (!FFileHelper::LoadFileToString(FileContents, *MetricsPath))
	{
		OutError = FString::Printf(TEXT("could not read %s"), *MetricsPath);
		return false;
	}

	TArray<FString> Lines;
	FileContents.ParseIntoArrayLines(Lines, false);

	TSharedPtr<FJsonObject> PreferredObject;
	TSharedPtr<FJsonObject> LastObject;
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

		const FString ContactSheet = JsonStringField(JsonObject, TEXT("contact_sheet"));
		if (ContactSheet.IsEmpty())
		{
			continue;
		}

		LastObject = JsonObject;
		const FString Scenario = JsonStringField(JsonObject, TEXT("scenario"));
		const int32 Replay = JsonIntField(JsonObject, TEXT("replay"));
		if (Scenario.Equals(TEXT("process_layer_default"), ESearchCase::IgnoreCase) && Replay == 1)
		{
			PreferredObject = JsonObject;
		}
		else if (Scenario.Equals(TEXT("default_40_plate_process_cadence"), ESearchCase::IgnoreCase) && Replay == 1)
		{
			PreferredObject = JsonObject;
		}
	}

	const TSharedPtr<FJsonObject> SelectedObject = PreferredObject.IsValid() ? PreferredObject : LastObject;
	if (!SelectedObject.IsValid())
	{
		OutError = TEXT("metrics.jsonl did not contain map export rows");
		return false;
	}

	LatestMapArtifact = FLatestMapArtifactSnapshot();
	LatestMapArtifact.bHasArtifact = true;
	LatestMapArtifact.StatusText = TEXT("latest Phase III map export loaded");
	LatestMapArtifact.MetricsPath = FPaths::ConvertRelativePathToFull(MetricsPath);
	LatestMapArtifact.OutputRoot = FPaths::GetPath(LatestMapArtifact.MetricsPath);
	LatestMapArtifact.ContactSheetPath = FPaths::ConvertRelativePathToFull(JsonStringField(SelectedObject, TEXT("contact_sheet")));
	LatestMapArtifact.ScenarioName = JsonStringField(SelectedObject, TEXT("scenario"));
	LatestMapArtifact.ArtifactTimestamp = ArtifactTimestamp;
	LatestMapArtifact.LoadedAt = FDateTime::UtcNow();
	LatestMapArtifact.Replay = JsonIntField(SelectedObject, TEXT("replay"));
	LatestMapArtifact.Step = JsonIntField(SelectedObject, TEXT("step"));
	LatestMapArtifact.EventCount = JsonIntField(SelectedObject, TEXT("event_count"));
	LatestMapArtifact.NextResampleStep = JsonIntField(SelectedObject, TEXT("next_resample_step"));
	LatestMapArtifact.CadenceSteps = JsonIntField(SelectedObject, TEXT("cadence_steps"));
	LatestMapArtifact.CadenceDeltaTMa = JsonDoubleField(SelectedObject, TEXT("cadence_delta_t_ma"));
	LatestMapArtifact.ObservedMaxPlateSpeedMmPerYear = JsonDoubleField(SelectedObject, TEXT("observed_max_plate_speed_mm_per_year"));
	const TArray<TSharedPtr<FJsonValue>>* ExportArray = nullptr;
	if (SelectedObject->TryGetArrayField(TEXT("map_exports"), ExportArray) && ExportArray != nullptr)
	{
		LatestMapArtifact.ExportedMapCount = ExportArray->Num();
	}
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

FReply SCarrierLabControlPanel::OnLoadMapsClicked()
{
	LoadLatestMapArtifact();
	return FReply::Handled();
}

FReply SCarrierLabControlPanel::OnOpenContactSheetClicked()
{
	if (LatestMapArtifact.bHasArtifact && IFileManager::Get().FileExists(*LatestMapArtifact.ContactSheetPath))
	{
		FPlatformProcess::LaunchFileInDefaultExternalApplication(*LatestMapArtifact.ContactSheetPath);
	}
	return FReply::Handled();
}

FReply SCarrierLabControlPanel::OnOpenMapFolderClicked()
{
	if (LatestMapArtifact.bHasArtifact)
	{
		const FString Folder = IFileManager::Get().DirectoryExists(*LatestMapArtifact.ContactSheetPath)
			? LatestMapArtifact.ContactSheetPath
			: FPaths::GetPath(LatestMapArtifact.ContactSheetPath);
		if (!Folder.IsEmpty() && IFileManager::Get().DirectoryExists(*Folder))
		{
			FPlatformProcess::LaunchFileInDefaultExternalApplication(*Folder);
		}
	}
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

TSharedRef<SWidget> SCarrierLabControlPanel::BuildShell()
{
	return SNew(SBorder)
		.Padding(10.0f)
		.BorderBackgroundColor(FLinearColor(0.018f, 0.020f, 0.024f, 1.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				BuildSidebar()
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(8.0f, 0.0f)
			[
				SNew(SSeparator).Orientation(Orient_Vertical)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					BuildTopStatusBar()
				]
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					BuildMainSwitcher()
				]
			]
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildSidebar()
{
	return SNew(SBox)
		.WidthOverride(210.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 12.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WorkbenchTitle", "CarrierLab Workbench"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
			[
				BuildSidebarButton(LOCTEXT("SidebarLiveActor", "Live Actor"), EPanelPage::LiveActor)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
			[
				BuildSidebarButton(LOCTEXT("SidebarContacts", "Read-Only Contacts"), EPanelPage::Contacts)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
			[
				BuildSidebarButton(LOCTEXT("SidebarMaps", "Checkpoint Maps"), EPanelPage::Maps)
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 2.0f)
			[
				BuildSidebarButton(LOCTEXT("SidebarGates", "Gates"), EPanelPage::Gates)
			]
			+ SVerticalBox::Slot().FillHeight(1.0f)
			[
				SNew(SSpacer)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return TargetActor.IsValid()
						? FText::FromString(FString::Printf(TEXT("Connected: %s"), *TargetActor->GetActorLabel()))
						: LOCTEXT("NoActorFooter", "No actor connected");
				})
				.ColorAndOpacity(FSlateColor(FLinearColor(0.74f, 0.78f, 0.84f, 1.0f)))
				.AutoWrapText(true)
			]
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildSidebarButton(const FText& Label, const EPanelPage Page) const
{
	return SNew(SButton)
		.ButtonColorAndOpacity(TAttribute<FSlateColor>::CreateLambda([this, Page]()
		{
			return FSlateColor(ActivePage == Page ? ReadOnlyColor() : FLinearColor(0.06f, 0.065f, 0.075f, 1.0f));
		}))
		.OnClicked_Lambda([this, Page]()
		{
			const_cast<SCarrierLabControlPanel*>(this)->ActivePage = Page;
			return FReply::Handled();
		})
		[
			SNew(STextBlock)
			.Text(Label)
			.Font(FCoreStyle::GetDefaultFontStyle("Regular", 11))
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildTopStatusBar()
{
	return BuildCard(
		LOCTEXT("TopStatusTitle", "Current Source"),
		SNew(SGridPanel)
		.FillColumn(1, 1.0f)
		.FillColumn(3, 1.0f)
		+ SGridPanel::Slot(0, 0).Padding(0.0f, 0.0f, 14.0f, 5.0f)
		[
			BuildLabeledValue(LOCTEXT("StatusActor", "Actor"), TAttribute<FText>::CreateLambda([this]()
			{
				return TargetActor.IsValid()
					? FText::FromString(TargetActor->GetActorLabel())
					: LOCTEXT("StatusNoActor", "NO ACTOR");
			}))
		]
		+ SGridPanel::Slot(1, 0).Padding(0.0f, 0.0f, 14.0f, 5.0f)
		[
			BuildLabeledValue(LOCTEXT("StatusState", "State"), TAttribute<FText>::CreateLambda([this]()
			{
				if (!LiveProjection.bHasSnapshot)
				{
					return LOCTEXT("StatusUnavailable", "Unavailable");
				}
				if (!LiveProjection.bInitialized)
				{
					return LOCTEXT("StatusUninitialized", "Uninitialized");
				}
				return LiveProjection.bPlaying ? LOCTEXT("StatusPlaying", "Playing") : LOCTEXT("StatusPaused", "Paused");
			}))
		]
		+ SGridPanel::Slot(2, 0).Padding(0.0f, 0.0f, 14.0f, 5.0f)
		[
			BuildLabeledValue(LOCTEXT("StatusStep", "Step / Myr"), TAttribute<FText>::CreateLambda([this]()
			{
				if (!LiveProjection.bHasSnapshot)
				{
					return LOCTEXT("StatusDash", "-");
				}
				const int32 Step = LiveProjection.Metrics.Step;
				return FText::FromString(FString::Printf(TEXT("%d / %.1f Ma"), Step, static_cast<double>(Step) * 2.0));
			}))
		]
		+ SGridPanel::Slot(3, 0).Padding(0.0f, 0.0f, 0.0f, 5.0f)
		[
			BuildLabeledValue(LOCTEXT("StatusLayer", "Layer"), TAttribute<FText>::CreateLambda([this]()
			{
				return FText::FromString(LayerToString(PendingLayer));
			}))
		]
		+ SGridPanel::Slot(0, 1).Padding(0.0f, 0.0f, 14.0f, 0.0f)
		[
			BuildLabeledValue(LOCTEXT("StatusEvents", "Events"), TAttribute<FText>::CreateLambda([this]()
			{
				return LiveProjection.bHasSnapshot
					? FText::AsNumber(LiveProjection.Metrics.EventCount)
					: LOCTEXT("StatusEventsDash", "-");
			}))
		]
		+ SGridPanel::Slot(1, 1).Padding(0.0f, 0.0f, 14.0f, 0.0f)
		[
			BuildLabeledValue(LOCTEXT("StatusCadence", "Cadence"), TAttribute<FText>::CreateLambda([this]()
			{
				if (!LiveProjection.bHasSnapshot)
				{
					return LOCTEXT("StatusCadenceDash", "-");
				}
				return FText::FromString(FString::Printf(
					TEXT("%d steps / %.1f Ma"),
					LiveProjection.Metrics.CadenceSteps,
					LiveProjection.Metrics.CadenceDeltaTMa));
			}))
		]
		+ SGridPanel::Slot(2, 1).Padding(0.0f, 0.0f, 14.0f, 0.0f)
		[
			BuildLabeledValue(LOCTEXT("StatusNextResample", "Next Resample"), TAttribute<FText>::CreateLambda([this]()
			{
				return LiveProjection.bHasSnapshot
					? FText::AsNumber(LiveProjection.Metrics.NextResampleStep)
					: LOCTEXT("StatusNextDash", "-");
			}))
		]
		+ SGridPanel::Slot(3, 1)
		[
			BuildLabeledValue(LOCTEXT("StatusSpeedAuto", "Speed / Auto"), TAttribute<FText>::CreateLambda([this]()
			{
				if (!LiveProjection.bHasSnapshot)
				{
					return LOCTEXT("StatusSpeedDash", "-");
				}
				return FText::FromString(FString::Printf(
					TEXT("%.2f mm/yr / %s"),
					LiveProjection.Metrics.ObservedMaxPlateSpeedMmPerYear,
					bPendingAutoResample ? TEXT("on") : TEXT("off")));
			}))
		]);
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildMainSwitcher()
{
	return SNew(SWidgetSwitcher)
		.WidgetIndex_Lambda([this]()
		{
			return static_cast<int32>(ActivePage);
		})
		+ SWidgetSwitcher::Slot()
		[
			BuildLiveActorPage()
		]
		+ SWidgetSwitcher::Slot()
		[
			BuildContactsPage()
		]
		+ SWidgetSwitcher::Slot()
		[
			BuildMapsPage()
		]
		+ SWidgetSwitcher::Slot()
		[
			BuildGatesPage()
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildLiveActorPage()
{
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FMargin(6.0f))
				+ SUniformGridPanel::Slot(0, 0)
				[
					BuildMetricCard(
						LOCTEXT("MetricAuthCAF", "Auth CAF"),
						TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? FText::FromString(FString::Printf(TEXT("%.6f"), LiveProjection.Metrics.AuthoritativeCAF)) : LOCTEXT("MetricDashA", "-"); }),
						LOCTEXT("MetricAuthCAFDetail", "Authority material"),
						NeutralColor())
				]
				+ SUniformGridPanel::Slot(1, 0)
				[
					BuildMetricCard(
						LOCTEXT("MetricProjectedCAF", "Projected CAF"),
						TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? FText::FromString(FString::Printf(TEXT("%.6f"), LiveProjection.Metrics.ProjectedCAF)) : LOCTEXT("MetricDashB", "-"); }),
						LOCTEXT("MetricProjectedCAFDetail", "Projected output"),
						NeutralColor())
				]
				+ SUniformGridPanel::Slot(2, 0)
				[
					BuildMetricCard(
						LOCTEXT("MetricMiss", "Miss %"),
						TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? FText::FromString(PercentString(LiveProjection.Metrics.RawMissCount, LiveProjection.Metrics.SampleCount)) : LOCTEXT("MetricDashC", "-"); }),
						TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? FText::FromString(FString::Printf(TEXT("%d raw"), LiveProjection.Metrics.RawMissCount)) : LOCTEXT("MetricNoSnapshotC", "No snapshot"); }),
						NeutralColor())
				]
				+ SUniformGridPanel::Slot(3, 0)
				[
					BuildMetricCard(
						LOCTEXT("MetricMultiHit", "Multi-hit %"),
						TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? FText::FromString(PercentString(LiveProjection.Metrics.RawMultiHitCount, LiveProjection.Metrics.SampleCount)) : LOCTEXT("MetricDashD", "-"); }),
						TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? FText::FromString(FString::Printf(TEXT("%d raw"), LiveProjection.Metrics.RawMultiHitCount)) : LOCTEXT("MetricNoSnapshotD", "No snapshot"); }),
						NeutralColor())
				]
				+ SUniformGridPanel::Slot(0, 1)
				[
					BuildMetricCard(
						LOCTEXT("MetricDriftP95", "Drift p95"),
						TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? FText::FromString(FString::Printf(TEXT("%.9f km"), LiveProjection.Metrics.DriftErrorP95Km)) : LOCTEXT("MetricDashE", "-"); }),
						TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? FText::FromString(FString::Printf(TEXT("mean %.9f km"), LiveProjection.Metrics.DriftErrorMeanKm)) : LOCTEXT("MetricNoSnapshotE", "No snapshot"); }),
						NeutralColor())
				]
				+ SUniformGridPanel::Slot(1, 1)
				[
					BuildMetricCard(
						LOCTEXT("MetricNaNInf", "NaN/Inf"),
						TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? FText::AsNumber(LiveProjection.Metrics.NaNOrInfCount) : LOCTEXT("MetricDashF", "-"); }),
						TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot && LiveProjection.Metrics.NaNOrInfCount == 0 ? LOCTEXT("MetricNoBadValues", "PASS: none") : LOCTEXT("MetricBadValues", "Check numeric health"); }),
						LiveProjection.bHasSnapshot && LiveProjection.Metrics.NaNOrInfCount == 0 ? PassColor() : NeutralColor())
				]
				+ SUniformGridPanel::Slot(2, 1)
				[
					BuildMetricCard(
						LOCTEXT("MetricProjectionHash", "Projection hash"),
						TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? FText::FromString(ShortHash(LiveProjection.Metrics.LastHash)) : LOCTEXT("MetricDashG", "-"); }),
						LOCTEXT("MetricProjectionHashDetail", "Current output"),
						NeutralColor())
				]
				+ SUniformGridPanel::Slot(3, 1)
				[
					BuildMetricCard(
						LOCTEXT("MetricStateHash", "State hash"),
						TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? FText::FromString(ShortHash(LiveProjection.Metrics.StateHash)) : LOCTEXT("MetricDashH", "-"); }),
						LOCTEXT("MetricStateHashDetail", "Carrier authority"),
						NeutralColor())
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				BuildCarrierControls()
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				BuildCard(
					LOCTEXT("TimingCardTitle", "Timing"),
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildLabeledValue(LOCTEXT("TimingProjection", "Projection"), TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? SecondsText(LiveProjection.Metrics.ProjectionSeconds) : LOCTEXT("TimingDashA", "-"); }))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildLabeledValue(LOCTEXT("TimingQuery", "Query"), TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? SecondsText(LiveProjection.Metrics.ProjectionQuerySeconds) : LOCTEXT("TimingDashB", "-"); }))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildLabeledValue(LOCTEXT("TimingDrift", "Drift"), TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? SecondsText(LiveProjection.Metrics.DriftMetricsSeconds) : LOCTEXT("TimingDashC", "-"); }))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f).Padding(0.0f, 0.0f, 8.0f, 0.0f)
					[
						BuildLabeledValue(LOCTEXT("TimingRender", "Render"), TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? SecondsText(LiveProjection.Metrics.MeshUpdateSeconds) : LOCTEXT("TimingDashD", "-"); }))
					]
					+ SHorizontalBox::Slot().FillWidth(1.0f)
					[
						BuildLabeledValue(LOCTEXT("TimingResample", "Resample"), TAttribute<FText>::CreateLambda([this]() { return LiveProjection.bHasSnapshot ? SecondsText(LiveProjection.Metrics.ResampleEventSeconds) : LOCTEXT("TimingDashE", "-"); }))
					])
			]
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildContactsPage()
{
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildCard(
					LOCTEXT("ContactsActionTitle", "Read-Only Evidence"),
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						BuildActionButton(
							LOCTEXT("DetectContactsAction", "Detect Contacts"),
							LOCTEXT("DetectContactsActionDetail", "Read-only: checks contacts and hash unchanged state."),
							ReadOnlyColor(),
							FOnClicked::CreateSP(this, &SCarrierLabControlPanel::OnDetectContactsClicked),
							true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.Text(this, &SCarrierLabControlPanel::GetLiveContactSummaryText)
						.AutoWrapText(true)
					])
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				BuildCard(
					LOCTEXT("ContactGateTitle", "Read-Only Result"),
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						BuildGateRow(
							LOCTEXT("ContactProjectionHashGate", "Projection hash unchanged"),
							LOCTEXT("ContactProjectionHashSource", "Live Detect Contacts"),
							TAttribute<FText>::CreateLambda([this]()
							{
								return LiveContact.bHasAttempt && LiveContact.bSucceeded
									? FText::FromString(PassFail(LiveContact.bProjectionHashUnchanged))
									: LOCTEXT("ContactProjectionNotRun", "not run");
							}),
							TAttribute<FSlateColor>::CreateLambda([this]()
							{
								return FSlateColor(LiveContact.bHasAttempt && LiveContact.bSucceeded && LiveContact.bProjectionHashUnchanged ? PassColor() : NeutralColor());
							}))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
					[
						BuildGateRow(
							LOCTEXT("ContactStateHashGate", "State hash unchanged"),
							LOCTEXT("ContactStateHashSource", "Live Detect Contacts"),
							TAttribute<FText>::CreateLambda([this]()
							{
								return LiveContact.bHasAttempt && LiveContact.bSucceeded
									? FText::FromString(PassFail(LiveContact.bStateHashUnchanged))
									: LOCTEXT("ContactStateNotRun", "not run");
							}),
							TAttribute<FSlateColor>::CreateLambda([this]()
							{
								return FSlateColor(LiveContact.bHasAttempt && LiveContact.bSucceeded && LiveContact.bStateHashUnchanged ? PassColor() : NeutralColor());
							}))
					])
			]
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildMapsPage()
{
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildCard(
					LOCTEXT("LatestMapsTitle", "Latest Maps"),
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock)
						.Text(this, &SCarrierLabControlPanel::GetLatestMapSummaryText)
						.AutoWrapText(true)
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
						[
							BuildActionButton(
								LOCTEXT("LoadLatestMaps", "Load Latest Maps"),
								LOCTEXT("LoadLatestMapsDetail", "Read-only artifact discovery."),
								ReadOnlyColor(),
								FOnClicked::CreateSP(this, &SCarrierLabControlPanel::OnLoadMapsClicked))
						]
						+ SHorizontalBox::Slot().AutoWidth().Padding(0.0f, 0.0f, 6.0f, 0.0f)
						[
							BuildActionButton(
								LOCTEXT("OpenContactSheet", "Open Contact Sheet"),
								LOCTEXT("OpenContactSheetDetail", "Opens latest PNG in the OS viewer."),
								ReadOnlyColor(),
								FOnClicked::CreateSP(this, &SCarrierLabControlPanel::OnOpenContactSheetClicked))
						]
						+ SHorizontalBox::Slot().AutoWidth()
						[
							BuildActionButton(
								LOCTEXT("OpenMapFolder", "Open Map Folder"),
								LOCTEXT("OpenMapFolderDetail", "Opens the latest replay folder."),
								ReadOnlyColor(),
								FOnClicked::CreateSP(this, &SCarrierLabControlPanel::OnOpenMapFolderClicked))
						]
					])
			]
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildGatesPage()
{
	return SNew(SScrollBox)
		+ SScrollBox::Slot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				BuildCard(
					LOCTEXT("GateRowsTitle", "Source-Aware Gates"),
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						BuildGateRow(
							LOCTEXT("GateNoMutation", "No mutation"),
							LOCTEXT("GateNoMutationSource", "Live Detect Contacts"),
							TAttribute<FText>::CreateLambda([this]()
							{
								if (!LiveContact.bHasAttempt || !LiveContact.bSucceeded)
								{
									return LOCTEXT("GateNoMutationNotRun", "not run");
								}
								return FText::FromString(PassFail(LiveContact.bProjectionHashUnchanged && LiveContact.bStateHashUnchanged));
							}),
							TAttribute<FSlateColor>::CreateLambda([this]()
							{
								return FSlateColor(LiveContact.bHasAttempt && LiveContact.bSucceeded && LiveContact.bProjectionHashUnchanged && LiveContact.bStateHashUnchanged ? PassColor() : NeutralColor());
							}))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
					[
						BuildGateRow(
							LOCTEXT("GateReplayHash", "Replay contact hash match"),
							LOCTEXT("GateReplayHashSource", "Slice 1 artifact"),
							TAttribute<FText>::CreateLambda([this]() { return Slice1Artifact.bHasArtifact ? FText::FromString(PassFail(Slice1Artifact.bReplayHashMatch)) : LOCTEXT("GateArtifactMissingA", "no artifact"); }),
							TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Slice1Artifact.bHasArtifact && Slice1Artifact.bReplayHashMatch ? PassColor() : NeutralColor()); }))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
					[
						BuildGateRow(
							LOCTEXT("GateNoSubduction", "No-subduction controls"),
							LOCTEXT("GateNoSubductionSource", "Slice 1 artifact"),
							TAttribute<FText>::CreateLambda([this]() { return Slice1Artifact.bHasArtifact ? FText::FromString(PassFail(Slice1Artifact.bNoSubductionControls)) : LOCTEXT("GateArtifactMissingB", "no artifact"); }),
							TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Slice1Artifact.bHasArtifact && Slice1Artifact.bNoSubductionControls ? PassColor() : NeutralColor()); }))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
					[
						BuildGateRow(
							LOCTEXT("GateConvergenceSign", "Convergence sign"),
							LOCTEXT("GateConvergenceSignSource", "Slice 1 artifact"),
							TAttribute<FText>::CreateLambda([this]() { return Slice1Artifact.bHasArtifact ? FText::FromString(PassFail(Slice1Artifact.bConvergenceSign)) : LOCTEXT("GateArtifactMissingC", "no artifact"); }),
							TAttribute<FSlateColor>::CreateLambda([this]() { return FSlateColor(Slice1Artifact.bHasArtifact && Slice1Artifact.bConvergenceSign ? PassColor() : NeutralColor()); }))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 5.0f, 0.0f, 0.0f)
					[
						BuildGateRow(
							LOCTEXT("GateThirdPlate", "Third-plate explicitness"),
							LOCTEXT("GateThirdPlateSource", "Slice 1 artifact or live contacts"),
							TAttribute<FText>::CreateLambda([this]()
							{
								if (Slice1Artifact.bHasArtifact)
								{
									return FText::FromString(PassFail(Slice1Artifact.bThirdPlateExplicitness));
								}
								return LiveContact.bSucceeded && LiveContact.Metrics.ThirdPlateContactCount > 0
									? LOCTEXT("GateThirdPlateSeen", "SEEN")
									: LOCTEXT("GateThirdPlateNoSource", "no source");
							}),
							TAttribute<FSlateColor>::CreateLambda([this]()
							{
								return FSlateColor((Slice1Artifact.bHasArtifact && Slice1Artifact.bThirdPlateExplicitness) || (LiveContact.bSucceeded && LiveContact.Metrics.ThirdPlateContactCount > 0) ? PassColor() : NeutralColor());
							}))
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 10.0f, 0.0f, 0.0f)
					[
						BuildActionButton(
							LOCTEXT("LoadLatestArtifactAction", "Load Latest Artifact"),
							LOCTEXT("LoadLatestArtifactDetail", "Read-only Slice 1 checkpoint evidence."),
							ReadOnlyColor(),
							FOnClicked::CreateSP(this, &SCarrierLabControlPanel::OnLoadArtifactClicked))
					])
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f, 0.0f, 0.0f)
			[
				BuildArtifactSection()
			]
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildCard(const FText& Title, const TSharedRef<SWidget>& Body) const
{
	return SNew(SBorder)
		.Padding(10.0f)
		.BorderBackgroundColor(CardBackground())
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Title)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 8.0f)
			[
				SNew(SSeparator)
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				Body
			]
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildMetricCard(const FText& Label, TAttribute<FText> Value, TAttribute<FText> Detail, const FLinearColor& AccentColor) const
{
	return SNew(SBorder)
		.Padding(8.0f)
		.BorderBackgroundColor(FLinearColor(
			FMath::Max(CardBackground().R, AccentColor.R * 0.10f),
			FMath::Max(CardBackground().G, AccentColor.G * 0.10f),
			FMath::Max(CardBackground().B, AccentColor.B * 0.10f),
			1.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Label)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.78f, 0.82f, 0.88f, 1.0f)))
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(Value)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 15))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Detail)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.64f, 0.68f, 0.74f, 1.0f)))
			]
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildGateRow(const FText& Label, TAttribute<FText> Source, TAttribute<FText> Status, TAttribute<FSlateColor> StatusColor) const
{
	return SNew(SBorder)
		.Padding(7.0f)
		.BorderBackgroundColor(FLinearColor(0.028f, 0.030f, 0.035f, 1.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(Label)
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				]
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(Source)
					.ColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.66f, 0.72f, 1.0f)))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Status)
				.ColorAndOpacity(StatusColor)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildActionButton(TAttribute<FText> Label, TAttribute<FText> Detail, const FLinearColor& Color, const FOnClicked& OnClicked, const bool bRequiresExistingActor) const
{
	return SNew(SButton)
		.ButtonColorAndOpacity(FSlateColor(Color))
		.OnClicked(OnClicked)
		.IsEnabled_Lambda([this, bRequiresExistingActor]()
		{
			return !bRequiresExistingActor || TargetActor.IsValid();
		})
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Label)
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock)
				.Text(Detail)
				.ColorAndOpacity(FSlateColor(FLinearColor(0.82f, 0.86f, 0.90f, 1.0f)))
			]
		];
}

TSharedRef<SWidget> SCarrierLabControlPanel::BuildLabeledValue(const FText& Label, TAttribute<FText> Value) const
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(Label)
			.ColorAndOpacity(FSlateColor(FLinearColor(0.62f, 0.66f, 0.72f, 1.0f)))
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock)
			.Text(Value)
			.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
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
			SNew(SUniformGridPanel)
			.SlotPadding(FMargin(4.0f))
			+ SUniformGridPanel::Slot(0, 0)
			[
				BuildActionButton(
					LOCTEXT("Initialize", "Initialize"),
					LOCTEXT("InitializeDetail", "Cold start carrier."),
					MutationColor(),
					FOnClicked::CreateSP(this, &SCarrierLabControlPanel::OnInitializeClicked))
			]
			+ SUniformGridPanel::Slot(1, 0)
			[
				BuildActionButton(
					LOCTEXT("Step", "Step"),
					LOCTEXT("StepDetail", "Advance one step."),
					MutationColor(),
					FOnClicked::CreateSP(this, &SCarrierLabControlPanel::OnStepClicked),
					true)
			]
			+ SUniformGridPanel::Slot(2, 0)
			[
				BuildActionButton(
					TAttribute<FText>::CreateLambda([this]() { return GetPlayPauseText(); }),
					LOCTEXT("PlayPauseDetail", "Toggle continuous run."),
					MutationColor(),
					FOnClicked::CreateSP(this, &SCarrierLabControlPanel::OnPlayPauseClicked),
					true)
			]
			+ SUniformGridPanel::Slot(3, 0)
			[
				BuildActionButton(
					LOCTEXT("ResampleNow", "Lab Resample Now"),
					LOCTEXT("ResampleNowDetail", "Stage 1.5 lab-policy remesh."),
					MutationColor(),
					FOnClicked::CreateSP(this, &SCarrierLabControlPanel::OnResampleClicked),
					true)
			]
			+ SUniformGridPanel::Slot(4, 0)
			[
				BuildActionButton(
					LOCTEXT("Reset", "Reset"),
					LOCTEXT("ResetDetail", "Clear and restart."),
					MutationColor(),
					FOnClicked::CreateSP(this, &SCarrierLabControlPanel::OnResetClicked))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f, 0.0f, 6.0f)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("MutationNote", "Lab Resample Now mutates carrier state through the Stage 1.5 lab-policy remesh path until IIIE replaces it. It is not paper-primary evidence."))
			.ColorAndOpacity(FSlateColor(FLinearColor(1.0f, 0.75f, 0.25f, 1.0f)))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 0.0f, 0.0f, 6.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([this]()
			{
				return bPendingAutoResample ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
			})
			.OnCheckStateChanged_Lambda([this](const ECheckBoxState NewState)
			{
				bPendingAutoResample = NewState == ECheckBoxState::Checked;
				if (ACarrierLabVisualizationActor* Actor = GetCarrierActor(false))
				{
					Actor->bEnableNaturalResamplingEvents = bPendingAutoResample;
				}
			})
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AutoResample", "Auto resample at observed-speed cadence"))
			]
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
		TEXT("cadence: %d steps / %.3f Ma | observed max speed: %.6f mm/yr | auto resample: %s\n")
		TEXT("samples: %d | plates: %d | miss: %s (%d) | multi-hit: %s (%d) | policy-resolved multi-hit: %d | boundary hits: %d | NaN/Inf: %d\n")
		TEXT("last remesh mode: %s\n")
		TEXT("Auth CAF: %.6f | Projected CAF: %.6f | drift mean: %.9f km | drift p95: %.9f km\n")
		TEXT("projection hash: %s | state hash: %s"),
		*FormatTimestamp(LiveProjection.CapturedAt),
		*LiveProjection.ActorLabel,
		LiveProjection.bInitialized ? TEXT("yes") : TEXT("no"),
		LiveProjection.bPlaying ? TEXT("yes") : TEXT("no"),
		Metrics.Step,
		Metrics.NextResampleStep,
		Metrics.EventCount,
		Metrics.CadenceSteps,
		Metrics.CadenceDeltaTMa,
		Metrics.ObservedMaxPlateSpeedMmPerYear,
		bPendingAutoResample ? TEXT("on") : TEXT("off"),
		Metrics.SampleCount,
		Metrics.PlateCount,
		*PercentString(Metrics.RawMissCount, Metrics.SampleCount),
		Metrics.RawMissCount,
		*PercentString(Metrics.RawMultiHitCount, Metrics.SampleCount),
		Metrics.RawMultiHitCount,
		Metrics.PolicyResolvedMultiHitCount,
		Metrics.BoundaryHitCount,
		Metrics.NaNOrInfCount,
		Metrics.LastRemeshMode.IsEmpty() ? TEXT("none") : *Metrics.LastRemeshMode,
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

FText SCarrierLabControlPanel::GetLatestMapSummaryText() const
{
	if (!LatestMapArtifact.bHasArtifact)
	{
		return FText::FromString(LatestMapArtifact.StatusText);
	}

	return FText::FromString(FString::Printf(
		TEXT("Source: latest Phase III map artifact\n")
		TEXT("scenario: %s | replay: %d | step: %d / %.1f Ma | events: %d\n")
		TEXT("cadence: %d steps / %.1f Ma | next resample: %d | observed max speed: %.3f mm/yr\n")
		TEXT("maps: %d | loaded: %s | artifact timestamp: %s\n")
		TEXT("contact sheet: %s\n")
		TEXT("output root: %s\n")
		TEXT("metrics.jsonl: %s"),
		LatestMapArtifact.ScenarioName.IsEmpty() ? TEXT("unknown") : *LatestMapArtifact.ScenarioName,
		LatestMapArtifact.Replay,
		LatestMapArtifact.Step,
		static_cast<double>(LatestMapArtifact.Step) * 2.0,
		LatestMapArtifact.EventCount,
		LatestMapArtifact.CadenceSteps,
		LatestMapArtifact.CadenceDeltaTMa,
		LatestMapArtifact.NextResampleStep,
		LatestMapArtifact.ObservedMaxPlateSpeedMmPerYear,
		LatestMapArtifact.ExportedMapCount,
		*FormatTimestamp(LatestMapArtifact.LoadedAt),
		*FormatTimestamp(LatestMapArtifact.ArtifactTimestamp),
		*LatestMapArtifact.ContactSheetPath,
		*LatestMapArtifact.OutputRoot,
		*LatestMapArtifact.MetricsPath));
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
	case ECarrierLabVisualizationLayer::PhaseIIISummary:
		return TEXT("PhaseIIISummary");
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

FString SCarrierLabControlPanel::ShortHash(const FString& Hash, const int32 MaxChars)
{
	if (Hash.IsEmpty())
	{
		return TEXT("-");
	}
	return Hash.Len() <= MaxChars ? Hash : Hash.Left(MaxChars);
}

#undef LOCTEXT_NAMESPACE
