// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIObservabilityCommandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace
{
	constexpr int32 ObservabilitySeed = 42;
	constexpr double ObservabilityVelocityMmPerYear = 66.6666666667;
	constexpr double ObservabilitySeedElevationKm = 2.5;
	constexpr double ObservabilitySlabPullSpeedMmPerYear = 8.0;
	constexpr double ObservabilityReferenceVelocityMmPerYear = 100.0;
	constexpr int32 ContactThumbWidth = 512;
	constexpr int32 ContactThumbHeight = 256;

	enum class EObservabilityMaterialFixture : uint8
	{
		Default,
		MixedPlate0Continental
	};

	struct FObservabilityScenario
	{
		FString Name;
		FString Description;
		int32 SampleCount = 60000;
		int32 PlateCount = 40;
		int32 StepCount = 40;
		double ContinentalPlateFraction = 0.30;
		ECarrierLabPhaseIIMotionFixture MotionFixture = ECarrierLabPhaseIIMotionFixture::Default;
		EObservabilityMaterialFixture MaterialFixture = EObservabilityMaterialFixture::Default;
		bool bSeedMixedElevation = false;
		bool bEnableIIICProcessLayer = false;
		bool bEnableIIICSlabPull = false;
	};

	FString JsonString(const FString& Value)
	{
		FString Escaped;
		Escaped.Reserve(Value.Len() + 2);
		for (const TCHAR Ch : Value)
		{
			switch (Ch)
			{
			case TEXT('\\'): Escaped += TEXT("\\\\"); break;
			case TEXT('"'): Escaped += TEXT("\\\""); break;
			case TEXT('\n'): Escaped += TEXT("\\n"); break;
			case TEXT('\r'): Escaped += TEXT("\\r"); break;
			case TEXT('\t'): Escaped += TEXT("\\t"); break;
			default: Escaped.AppendChar(Ch); break;
			}
		}
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}

	void HashMix(uint64& Hash, const uint64 Value)
	{
		Hash ^= Value;
		Hash *= 1099511628211ull;
	}

	FString HashToString(const uint64 Hash)
	{
		return FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Hash));
	}

	FString LayerName(const ECarrierLabVisualizationLayer Layer)
	{
		switch (Layer)
		{
		case ECarrierLabVisualizationLayer::ElevationHeatmap:
			return TEXT("ElevationHeatmap");
		case ECarrierLabVisualizationLayer::SubductionMask:
			return TEXT("SubductionMask");
		case ECarrierLabVisualizationLayer::DistanceToFrontHeatmap:
			return TEXT("DistanceToFrontHeatmap");
		default:
			return TEXT("UnknownLayer");
		}
	}

	FString PassFail(const bool bPass)
	{
		return bPass ? TEXT("pass") : TEXT("fail");
	}

	FString MotionFixtureName(const ECarrierLabPhaseIIMotionFixture Fixture)
	{
		switch (Fixture)
		{
		case ECarrierLabPhaseIIMotionFixture::Zero:
			return TEXT("zero_motion");
		case ECarrierLabPhaseIIMotionFixture::ForcedConvergence:
			return TEXT("forced_convergence");
		case ECarrierLabPhaseIIMotionFixture::ForcedDivergence:
			return TEXT("forced_divergence");
		case ECarrierLabPhaseIIMotionFixture::Default:
		default:
			return TEXT("default");
		}
	}

	FString MaterialFixtureName(const EObservabilityMaterialFixture Fixture)
	{
		switch (Fixture)
		{
		case EObservabilityMaterialFixture::MixedPlate0Continental:
			return TEXT("mixed_plate0_continental");
		case EObservabilityMaterialFixture::Default:
		default:
			return TEXT("default");
		}
	}

	TArray<FObservabilityScenario> BuildScenarios(const bool bIIICMode)
	{
		TArray<FObservabilityScenario> Scenarios;
		if (bIIICMode)
		{
			FObservabilityScenario ProcessLayer;
			ProcessLayer.Name = TEXT("process_layer_default");
			ProcessLayer.Description = TEXT("Consolidated IIIC process-layer fixture: marks, trench elevation split, and overriding uplift enabled; slab pull remains off.");
			ProcessLayer.SampleCount = 10000;
			ProcessLayer.PlateCount = 2;
			ProcessLayer.StepCount = 1;
			ProcessLayer.ContinentalPlateFraction = 0.50;
			ProcessLayer.MotionFixture = ECarrierLabPhaseIIMotionFixture::ForcedConvergence;
			ProcessLayer.MaterialFixture = EObservabilityMaterialFixture::MixedPlate0Continental;
			ProcessLayer.bSeedMixedElevation = true;
			ProcessLayer.bEnableIIICProcessLayer = true;
			ProcessLayer.bEnableIIICSlabPull = false;
			Scenarios.Add(ProcessLayer);
			return Scenarios;
		}

		FObservabilityScenario Baseline;
		Baseline.Name = TEXT("default_40_plate_baseline");
		Baseline.Description = TEXT("Default 40-plate pre-IIIC baseline. Elevation is expected to be flat, and IIIB-derived masks may be sparse before IIIC persistent marks exist.");
		Baseline.SampleCount = 60000;
		Baseline.PlateCount = 40;
		Baseline.StepCount = 40;
		Baseline.ContinentalPlateFraction = 0.30;
		Baseline.MotionFixture = ECarrierLabPhaseIIMotionFixture::Default;
		Baseline.MaterialFixture = EObservabilityMaterialFixture::Default;
		Scenarios.Add(Baseline);

		FObservabilityScenario Forced;
		Forced.Name = TEXT("forced_convergence_mixed");
		Forced.Description = TEXT("Two-plate forced-convergence mixed-material fixture that exercises IIIB polarity roles and distance-to-front masks for human spatial sanity checks.");
		Forced.SampleCount = 60000;
		Forced.PlateCount = 2;
		Forced.StepCount = 8;
		Forced.ContinentalPlateFraction = 0.30;
		Forced.MotionFixture = ECarrierLabPhaseIIMotionFixture::ForcedConvergence;
		Forced.MaterialFixture = EObservabilityMaterialFixture::MixedPlate0Continental;
		Scenarios.Add(Forced);

		return Scenarios;
	}

	bool IsIIICMode(const FString& Params)
	{
		return Params.Contains(TEXT("-IIIC")) ||
			Params.Contains(TEXT("Mode=IIIC")) ||
			Params.Contains(TEXT("Mode=iiic"));
	}

	FString GetOutputRoot(const FString& Params, const bool bIIICMode)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = bIIICMode
				? FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIICMapExport"), Stamp)
				: FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("Observability"), TEXT("Maps"), Stamp);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	FString ResolveReportPath(const FString& Params, const bool bIIICMode)
	{
		FString ReportPath;
		if (!FParse::Value(*Params, TEXT("Report="), ReportPath))
		{
			ReportPath = FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("docs"),
				TEXT("checkpoints"),
				bIIICMode ? TEXT("phase-iii-iiic-map-export.md") : TEXT("phase-iii-observability-maps.md"));
		}
		else if (FPaths::IsRelative(ReportPath))
		{
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), ReportPath);
		}
		return FPaths::ConvertRelativePathToFull(ReportPath);
	}

	UWorld* GetCommandletWorld()
	{
		if (GEngine != nullptr)
		{
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.World() != nullptr)
				{
					return Context.World();
				}
			}
		}
		return GWorld;
	}

	ACarrierLabVisualizationActor* SpawnActor(UWorld& World, const FObservabilityScenario& Scenario)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParams.bDeferConstruction = true;
		ACarrierLabVisualizationActor* Actor = World.SpawnActor<ACarrierLabVisualizationActor>(
			ACarrierLabVisualizationActor::StaticClass(),
			FTransform::Identity,
			SpawnParams);
		if (Actor == nullptr)
		{
			return nullptr;
		}

		Actor->bAutoInitialize = false;
		Actor->bPlayOnBegin = false;
		Actor->bShowHud = false;
		Actor->SampleCount = Scenario.SampleCount;
		Actor->PlateCount = Scenario.PlateCount;
		Actor->Seed = ObservabilitySeed;
		Actor->ContinentalPlateFraction = Scenario.ContinentalPlateFraction;
		Actor->VelocityMmPerYear = ObservabilityVelocityMmPerYear;
		Actor->MultiHitPolicy = ECarrierLabMultiHitPolicy::Centroid;
		Actor->VisualizationLayer = ECarrierLabVisualizationLayer::PlateId;
		Actor->PhaseIIICSlabPullSpeedMmPerYear = ObservabilitySlabPullSpeedMmPerYear;
		Actor->PhaseIIICReferenceVelocityMmPerYear = ObservabilityReferenceVelocityMmPerYear;
		Actor->ConfigurePhaseIIICProcessLayer(Scenario.bEnableIIICProcessLayer, Scenario.bEnableIIICSlabPull);
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	bool ApplyMaterialFixture(ACarrierLabVisualizationActor& Actor, const FObservabilityScenario& Scenario)
	{
		switch (Scenario.MaterialFixture)
		{
		case EObservabilityMaterialFixture::MixedPlate0Continental:
		{
			const bool bMaterialOk =
				Actor.SetPlateContinentalForTest(0, true) &&
				Actor.SetPlateContinentalForTest(1, false);
			if (!bMaterialOk)
			{
				return false;
			}
			return !Scenario.bSeedMixedElevation || Actor.SetPlateElevationForTest(1, ObservabilitySeedElevationKm);
		}
		case EObservabilityMaterialFixture::Default:
		default:
			return true;
		}
	}

	bool SavePng(const FString& Path, const TArray<FColor>& Pixels, const int32 Width, const int32 Height)
	{
		IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		const TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
		if (!ImageWrapper.IsValid())
		{
			return false;
		}
		if (!ImageWrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Width, Height, ERGBFormat::BGRA, 8))
		{
			return false;
		}
		return FFileHelper::SaveArrayToFile(ImageWrapper->GetCompressed(90), *Path);
	}

	uint64 HashPixels(const TArray<FColor>& Pixels)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMix(Hash, static_cast<uint64>(Pixels.Num() + 1));
		for (const FColor& Pixel : Pixels)
		{
			HashMix(Hash, static_cast<uint64>(Pixel.DWColor()));
		}
		return Hash;
	}

	int32 CountNonBackgroundPixels(const TArray<FColor>& Pixels)
	{
		int32 Count = 0;
		const FColor Background(0, 0, 0, 255);
		for (const FColor& Pixel : Pixels)
		{
			if (Pixel != Background)
			{
				++Count;
			}
		}
		return Count;
	}

	void BlitThumbnail(
		const TArray<FColor>& Source,
		const int32 SourceWidth,
		const int32 SourceHeight,
		TArray<FColor>& Dest,
		const int32 DestWidth,
		const int32 DestX,
		const int32 DestY)
	{
		for (int32 Y = 0; Y < ContactThumbHeight; ++Y)
		{
			const int32 SourceY = FMath::Clamp((Y * SourceHeight) / ContactThumbHeight, 0, SourceHeight - 1);
			for (int32 X = 0; X < ContactThumbWidth; ++X)
			{
				const int32 SourceX = FMath::Clamp((X * SourceWidth) / ContactThumbWidth, 0, SourceWidth - 1);
				Dest[(DestY + Y) * DestWidth + (DestX + X)] = Source[SourceY * SourceWidth + SourceX];
			}
		}
	}

	struct FLayerExport
	{
		ECarrierLabVisualizationLayer Layer = ECarrierLabVisualizationLayer::PlateId;
		FString Name;
		FString Path;
		FString Hash;
		int32 NonBackgroundPixelCount = 0;
	};

	struct FObservabilityReplay
	{
		FString ScenarioName;
		int32 Replay = 0;
		bool bCompleted = false;
		double TotalSeconds = 0.0;
		FString ProjectionHashBefore;
		FString ProjectionHashAfter;
		FString StateHashBefore;
		FString StateHashAfter;
		FString CrustStateHashBefore;
		FString CrustStateHashAfter;
		FString ConvergenceTrackingHashBefore;
		FString ConvergenceTrackingHashAfter;
		FCarrierLabPhaseIIIB1TrackingAudit TrackingAudit;
		FCarrierLabPhaseIIIB2DistanceAudit DistanceAudit;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit MatrixAudit;
		FCarrierLabPhaseIIIB4PolarityAudit PolarityAudit;
		FCarrierLabPhaseIIIB6NeighborPropagationAudit PropagationAudit;
		FCarrierLabPhaseIIIB7HashClosureAudit ClosureAudit;
		FCarrierLabPhaseIIIC1SubductingMarkAudit IIICMarkAudit;
		FCarrierLabPhaseIIIC2ElevationAudit IIICElevationAudit;
		FCarrierLabPhaseIIIC3UpliftAudit IIICUpliftAudit;
		FCarrierLabPhaseIIIC4SlabPullAudit IIICSlabPullAudit;
		FCarrierLabPhaseIIIC5ElevationLedgerAudit IIICLedgerAudit;
		TArray<FLayerExport> LayerExports;
		FString ContactSheetPath;
		bool bExportReadOnly = false;
	};

	struct FObservabilityScenarioResult
	{
		FObservabilityScenario Scenario;
		FObservabilityReplay A;
		FObservabilityReplay B;
		bool bRanA = false;
		bool bRanB = false;
		bool bLayerHashesStable = false;
		bool bOverallPass = false;
	};

	FString LayerExportJson(const FLayerExport& Export)
	{
		return FString::Printf(
			TEXT("{\"layer\":%s,\"path\":%s,\"hash\":%s,\"non_background_pixels\":%d}"),
			*JsonString(Export.Name),
			*JsonString(Export.Path),
			*JsonString(Export.Hash),
			Export.NonBackgroundPixelCount);
	}

	bool WriteMaps(
		const FString& ReplayDir,
		const ACarrierLabVisualizationActor& Actor,
		TArray<FLayerExport>& OutLayerExports,
		FString& OutContactSheetPath)
	{
		IFileManager::Get().MakeDirectory(*ReplayDir, true);
		const ECarrierLabVisualizationLayer Layers[] =
		{
			ECarrierLabVisualizationLayer::ElevationHeatmap,
			ECarrierLabVisualizationLayer::SubductionMask,
			ECarrierLabVisualizationLayer::DistanceToFrontHeatmap
		};

		struct FLayerPixels
		{
			FLayerExport Export;
			TArray<FColor> Pixels;
			int32 Width = 0;
			int32 Height = 0;
		};

		TArray<FLayerPixels> Images;
		for (const ECarrierLabVisualizationLayer Layer : Layers)
		{
			FLayerPixels Image;
			Image.Export.Layer = Layer;
			Image.Export.Name = LayerName(Layer);
			if (!Actor.BuildVisualizationLayerMap(Layer, Image.Pixels, Image.Width, Image.Height))
			{
				return false;
			}
			Image.Export.Path = FPaths::Combine(ReplayDir, Image.Export.Name + TEXT(".png"));
			Image.Export.Hash = HashToString(HashPixels(Image.Pixels));
			Image.Export.NonBackgroundPixelCount = CountNonBackgroundPixels(Image.Pixels);
			if (!SavePng(Image.Export.Path, Image.Pixels, Image.Width, Image.Height))
			{
				return false;
			}
			Images.Add(MoveTemp(Image));
		}

		const int32 ContactWidth = ContactThumbWidth * Images.Num();
		const int32 ContactHeight = ContactThumbHeight;
		TArray<FColor> ContactPixels;
		ContactPixels.Init(FColor(0, 0, 0, 255), ContactWidth * ContactHeight);
		for (int32 Index = 0; Index < Images.Num(); ++Index)
		{
			BlitThumbnail(
				Images[Index].Pixels,
				Images[Index].Width,
				Images[Index].Height,
				ContactPixels,
				ContactWidth,
				Index * ContactThumbWidth,
				0);
		}

		OutContactSheetPath = FPaths::Combine(ReplayDir, TEXT("ContactSheet.png"));
		if (!SavePng(OutContactSheetPath, ContactPixels, ContactWidth, ContactHeight))
		{
			return false;
		}

		OutLayerExports.Reset(Images.Num());
		for (const FLayerPixels& Image : Images)
		{
			OutLayerExports.Add(Image.Export);
		}
		return true;
	}

	bool RunReplay(
		const FObservabilityScenario& Scenario,
		const int32 Replay,
		const FString& OutputRoot,
		FObservabilityReplay& OutResult)
	{
		OutResult = FObservabilityReplay();
		OutResult.ScenarioName = Scenario.Name;
		OutResult.Replay = Replay;
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, Scenario);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier())
		{
			Actor->Destroy();
			return false;
		}
		if (!ApplyMaterialFixture(*Actor, Scenario))
		{
			Actor->Destroy();
			return false;
		}
		Actor->ConfigurePhaseIIMotionFixture(Scenario.MotionFixture);
		for (int32 Step = 0; Step < Scenario.StepCount; ++Step)
		{
			Actor->StepOnce();
		}

		Actor->GetPhaseIIIB1TrackingAudit(OutResult.TrackingAudit);
		Actor->GetPhaseIIIB2DistanceAudit(OutResult.DistanceAudit);
		Actor->GetPhaseIIIB3SubductionMatrixAudit(OutResult.MatrixAudit);
		Actor->GetPhaseIIIB4PolarityAudit(OutResult.PolarityAudit);
		Actor->GetPhaseIIIB6NeighborPropagationAudit(OutResult.PropagationAudit);
		Actor->GetPhaseIIIB7HashClosureAudit(OutResult.ClosureAudit);
		Actor->GetPhaseIIIC1SubductingMarkAudit(OutResult.IIICMarkAudit);
		Actor->GetPhaseIIIC2ElevationAudit(OutResult.IIICElevationAudit);
		Actor->GetPhaseIIIC3UpliftAudit(OutResult.IIICUpliftAudit);
		Actor->GetPhaseIIIC4SlabPullAudit(OutResult.IIICSlabPullAudit);
		Actor->GetPhaseIIIC5ElevationLedgerAudit(OutResult.IIICLedgerAudit);

		OutResult.ProjectionHashBefore = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashBefore = Actor->CurrentMetrics.StateHash;
		OutResult.CrustStateHashBefore = Actor->CurrentMetrics.CrustStateHash;
		OutResult.ConvergenceTrackingHashBefore = Actor->CurrentMetrics.ConvergenceTrackingHash;

		const FString ReplayDir = FPaths::Combine(OutputRoot, Scenario.Name, FString::Printf(TEXT("replay_%d"), Replay));
		if (!WriteMaps(ReplayDir, *Actor, OutResult.LayerExports, OutResult.ContactSheetPath))
		{
			Actor->Destroy();
			return false;
		}

		OutResult.ProjectionHashAfter = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashAfter = Actor->CurrentMetrics.StateHash;
		OutResult.CrustStateHashAfter = Actor->CurrentMetrics.CrustStateHash;
		OutResult.ConvergenceTrackingHashAfter = Actor->CurrentMetrics.ConvergenceTrackingHash;
		OutResult.bExportReadOnly =
			OutResult.ProjectionHashBefore == OutResult.ProjectionHashAfter &&
			OutResult.StateHashBefore == OutResult.StateHashAfter &&
			OutResult.CrustStateHashBefore == OutResult.CrustStateHashAfter &&
			OutResult.ConvergenceTrackingHashBefore == OutResult.ConvergenceTrackingHashAfter;
		OutResult.TotalSeconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bCompleted = true;

		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return true;
	}

	bool LayerHashesMatch(const FObservabilityReplay& A, const FObservabilityReplay& B)
	{
		if (A.LayerExports.Num() != B.LayerExports.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.LayerExports.Num(); ++Index)
		{
			if (A.LayerExports[Index].Name != B.LayerExports[Index].Name ||
				A.LayerExports[Index].Hash != B.LayerExports[Index].Hash)
			{
				return false;
			}
		}
		return true;
	}

	FString ReplayJson(const FObservabilityReplay& Result)
	{
		TArray<FString> LayerJson;
		for (const FLayerExport& Export : Result.LayerExports)
		{
			LayerJson.Add(LayerExportJson(Export));
		}

		return FString::Printf(
			TEXT("{\"scenario\":%s,\"replay\":%d,\"completed\":%s,\"total_seconds\":%.6f,")
			TEXT("\"step\":%d,\"projection_hash_before\":%s,\"projection_hash_after\":%s,")
			TEXT("\"state_hash_before\":%s,\"state_hash_after\":%s,")
			TEXT("\"crust_state_hash_before\":%s,\"crust_state_hash_after\":%s,")
			TEXT("\"convergence_tracking_hash_before\":%s,\"convergence_tracking_hash_after\":%s,")
			TEXT("\"export_read_only\":%s,\"active_triangles\":%d,\"distance_records\":%d,")
			TEXT("\"matrix_pairs\":%d,\"polarity_decisions\":%d,\"subduction_polarity_decisions\":%d,")
			TEXT("\"propagation_seed_hits\":%d,\"propagation_added\":%d,")
			TEXT("\"iiic_process_layer\":%s,\"iiic_slab_pull\":%s,\"iiic_marks\":%d,")
			TEXT("\"iiic_trench_records\":%d,\"iiic_uplift_records\":%d,\"iiic_ledger_records\":%d,")
			TEXT("\"iiic_actual_delta_km\":%.15f,\"iiic_ledger_residual_km\":%.15e,")
			TEXT("\"map_exports\":[%s],\"contact_sheet\":%s}"),
			*JsonString(Result.ScenarioName),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.TotalSeconds,
			Result.ClosureAudit.Step,
			*JsonString(Result.ProjectionHashBefore),
			*JsonString(Result.ProjectionHashAfter),
			*JsonString(Result.StateHashBefore),
			*JsonString(Result.StateHashAfter),
			*JsonString(Result.CrustStateHashBefore),
			*JsonString(Result.CrustStateHashAfter),
			*JsonString(Result.ConvergenceTrackingHashBefore),
			*JsonString(Result.ConvergenceTrackingHashAfter),
			Result.bExportReadOnly ? TEXT("true") : TEXT("false"),
			Result.TrackingAudit.ActiveBoundaryTriangleCount,
			Result.DistanceAudit.DistanceRecordCount,
			Result.MatrixAudit.MatrixPairCount,
			Result.PolarityAudit.DecisionCount,
			Result.PolarityAudit.SubductionPolarityCount,
			Result.PropagationAudit.PropagationSeedHitCount,
			Result.PropagationAudit.PropagationAddedCount,
			Result.IIICMarkAudit.bEnabled ? TEXT("true") : TEXT("false"),
			Result.IIICSlabPullAudit.bSlabPullEnabled ? TEXT("true") : TEXT("false"),
			Result.IIICMarkAudit.MarkCount,
			Result.IIICLedgerAudit.TrenchRecordCount,
			Result.IIICLedgerAudit.UpliftRecordCount,
			Result.IIICLedgerAudit.RecordCount,
			Result.IIICLedgerAudit.ActualVisibleElevationDeltaKm,
			Result.IIICLedgerAudit.VisibleElevationResidualKm,
			*FString::Join(LayerJson, TEXT(",")),
			*JsonString(Result.ContactSheetPath));
	}

	FString BuildReport(
		const FString& OutputRoot,
		const TArray<FObservabilityScenarioResult>& Results,
		const bool bOverallPass,
		const bool bIIICMode)
	{
		FString Report;
		Report += bIIICMode
			? TEXT("# Phase III IIIC Map Export Checkpoint\n\n")
			: TEXT("# Phase III Observability Map Export Checkpoint\n\n");
		Report += bIIICMode
			? TEXT("Status: read-only spatial sanity export for the consolidated IIIC process layer.\n\n")
			: TEXT("Status: read-only observability patch before IIIC entry reconciliation.\n\n");
		Report += TEXT("## Scope\n\n");
		Report += bIIICMode
			? TEXT("This checkpoint exports filled Mollweide-style PNG artifacts after the consolidated IIIC process layer has run with slab pull off. The export path is read-only: it must not add process mutation, resampling behavior, triangle consumption, material transfer, forbidden authority fallback patterns, or projection-derived carrier authority.\n\n")
			: TEXT("This checkpoint exports the Phase III actor-only spatial sanity layers to filled Mollweide-style PNG artifacts. It does not add process mutation, resampling behavior, triangle consumption, material transfer, forbidden authority fallback patterns, or projection-derived carrier authority.\n\n");
		Report += TEXT("Fixtures:\n\n");
		for (const FObservabilityScenarioResult& Result : Results)
		{
			Report += FString::Printf(
				TEXT("- `%s`: %s (`%dk / %d plates / seed %d / %d rigid steps / %s motion / %s material / IIIC process %s / slab pull %s / centroid policy`).\n"),
				*Result.Scenario.Name,
				*Result.Scenario.Description,
				Result.Scenario.SampleCount / 1000,
				Result.Scenario.PlateCount,
				ObservabilitySeed,
				Result.Scenario.StepCount,
				*MotionFixtureName(Result.Scenario.MotionFixture),
				*MaterialFixtureName(Result.Scenario.MaterialFixture),
				Result.Scenario.bEnableIIICProcessLayer ? TEXT("on") : TEXT("off"),
				Result.Scenario.bEnableIIICSlabPull ? TEXT("on") : TEXT("off"));
		}
		Report += TEXT("\n");
		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---:|---|\n");
		Report += FString::Printf(
			TEXT("| map exports written | %s | output root `%s` |\n"),
			*PassFail(bOverallPass),
			*OutputRoot);
		for (const FObservabilityScenarioResult& Result : Results)
		{
			Report += FString::Printf(
				TEXT("| `%s` export read-only | %s | replay 0 `%s`, replay 1 `%s` |\n"),
				*Result.Scenario.Name,
				*PassFail(Result.A.bExportReadOnly && Result.B.bExportReadOnly),
				Result.A.bExportReadOnly ? TEXT("unchanged hashes") : TEXT("hash changed"),
				Result.B.bExportReadOnly ? TEXT("unchanged hashes") : TEXT("hash changed"));
			Report += FString::Printf(
				TEXT("| `%s` same-seed map hashes | %s | replay hashes byte-identical per layer |\n"),
				*Result.Scenario.Name,
				*PassFail(Result.bLayerHashesStable));
		}
		Report += TEXT("\n");

		Report += TEXT("## Tracking State Used For Maps\n\n");
		Report += TEXT("| Scenario | Replay | Step | Active triangles | Distance records | Matrix pairs | Polarity decisions | Subduction polarity | Propagation seeds | Propagation added | Convergence hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FObservabilityScenarioResult& ScenarioResult : Results)
		{
			for (const FObservabilityReplay* Result : { &ScenarioResult.A, &ScenarioResult.B })
			{
				Report += FString::Printf(
					TEXT("| `%s` | %d | %d | %d | %d | %d | %d | %d | %d | %d | `%s` |\n"),
					*ScenarioResult.Scenario.Name,
					Result->Replay,
					Result->ClosureAudit.Step,
					Result->TrackingAudit.ActiveBoundaryTriangleCount,
					Result->DistanceAudit.DistanceRecordCount,
					Result->MatrixAudit.MatrixPairCount,
					Result->PolarityAudit.DecisionCount,
					Result->PolarityAudit.SubductionPolarityCount,
					Result->PropagationAudit.PropagationSeedHitCount,
					Result->PropagationAudit.PropagationAddedCount,
					*Result->ConvergenceTrackingHashBefore);
			}
		}

		Report += TEXT("\n## IIIC State Used For Maps\n\n");
		Report += TEXT("| Scenario | Replay | Process layer | Slab pull | Marks | Trench records | Uplift records | Ledger records | Actual delta km | Ledger residual km | Mark hash | Elevation hash | Uplift hash | Ledger hash |\n");
		Report += TEXT("|---|---:|---|---|---:|---:|---:|---:|---:|---:|---|---|---|---|\n");
		for (const FObservabilityScenarioResult& ScenarioResult : Results)
		{
			for (const FObservabilityReplay* Result : { &ScenarioResult.A, &ScenarioResult.B })
			{
				Report += FString::Printf(
					TEXT("| `%s` | %d | %s | %s | %d | %d | %d | %d | %.12f | %.12e | `%s` | `%s` | `%s` | `%s` |\n"),
					*ScenarioResult.Scenario.Name,
					Result->Replay,
					Result->IIICMarkAudit.bEnabled ? TEXT("on") : TEXT("off"),
					Result->IIICSlabPullAudit.bSlabPullEnabled ? TEXT("on") : TEXT("off"),
					Result->IIICMarkAudit.MarkCount,
					Result->IIICLedgerAudit.TrenchRecordCount,
					Result->IIICLedgerAudit.UpliftRecordCount,
					Result->IIICLedgerAudit.RecordCount,
					Result->IIICLedgerAudit.ActualVisibleElevationDeltaKm,
					Result->IIICLedgerAudit.VisibleElevationResidualKm,
					*Result->IIICMarkAudit.SubductingMarkHash,
					*Result->IIICElevationAudit.VisibleElevationHash,
					*Result->IIICUpliftAudit.UpliftHash,
					*Result->IIICLedgerAudit.ElevationLedgerHash);
			}
		}

		Report += TEXT("\n## Exported Maps\n\n");
		Report += TEXT("| Scenario | Replay | Layer | Hash | Non-background pixels | Path |\n");
		Report += TEXT("|---|---:|---|---|---:|---|\n");
		for (const FObservabilityScenarioResult& ScenarioResult : Results)
		{
			for (const FObservabilityReplay* Result : { &ScenarioResult.A, &ScenarioResult.B })
			{
				for (const FLayerExport& Export : Result->LayerExports)
				{
					Report += FString::Printf(
						TEXT("| `%s` | %d | `%s` | `%s` | %d | `%s` |\n"),
						*ScenarioResult.Scenario.Name,
						Result->Replay,
						*Export.Name,
						*Export.Hash,
						Export.NonBackgroundPixelCount,
						*Export.Path);
				}
				Report += FString::Printf(
					TEXT("| `%s` | %d | `ContactSheet` | n/a | n/a | `%s` |\n"),
					*ScenarioResult.Scenario.Name,
					Result->Replay,
					*Result->ContactSheetPath);
			}
		}

		Report += TEXT("\n## Interpretation\n\n");
		Report += bIIICMode
			? TEXT("- `ElevationHeatmap` should show the IIIC.2 trench split and IIIC.3 overriding uplift as scalar-field color overlays on the filled continental/oceanic base map.\n")
			: TEXT("- `ElevationHeatmap` uses the filled continental/oceanic base map when elevation is still zero, then overlays positive/negative elevation once IIIC.2/IIIC.3 mutate the scalar field.\n");
		Report += bIIICMode
			? TEXT("- `SubductionMask` should show the consolidated IIIC subducting/overriding roles produced from persistent plate-local marks, not only the earlier pre-mutation IIIB inspection overlay.\n")
			: TEXT("- `SubductionMask` uses the filled base map plus IIIB polarity-derived role overlays; the forced-convergence fixture is the human-inspection map, not persistent IIIC subducting-triangle authority.\n");
		Report += bIIICMode
			? TEXT("- `DistanceToFrontHeatmap` remains the front-distance spatial context for the same fixture; it is diagnostic context, not a source of authority.\n\n")
			: TEXT("- `DistanceToFrontHeatmap` uses the filled base map plus active boundary distance overlays; the default baseline may be sparse, while the forced fixture intentionally exercises propagated front state.\n\n");
		Report += TEXT("## Recommendation\n\n");
		if (bIIICMode)
		{
			Report += bOverallPass
				? TEXT("IIIC map export passes. These images are suitable human spatial sanity artifacts for the consolidated IIIC process layer before Phase IIID work begins.\n")
				: TEXT("IIIC map export fails. Pause before Phase IIID and investigate the failed export/read-only/determinism gate.\n");
		}
		else
		{
			Report += bOverallPass
				? TEXT("Go for the docs-only IIIC entry reconciliation checkpoint. These exports are read-only and stable enough to serve as pre-mutation spatial sanity artifacts.\n")
				: TEXT("No-go for IIIC entry reconciliation until the failed export/read-only/determinism gate is investigated.\n");
		}
		return Report;
	}
}

UCarrierLabPhaseIIIObservabilityCommandlet::UCarrierLabPhaseIIIObservabilityCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIObservabilityCommandlet::Main(const FString& Params)
{
	const bool bIIICMode = IsIIICMode(Params);
	const FString OutputRoot = GetOutputRoot(Params, bIIICMode);
	const FString ReportPath = ResolveReportPath(Params, bIIICMode);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);

	TArray<FObservabilityScenarioResult> Results;
	bool bOverallPass = true;
	for (const FObservabilityScenario& Scenario : BuildScenarios(bIIICMode))
	{
		FObservabilityScenarioResult Result;
		Result.Scenario = Scenario;
		Result.bRanA = RunReplay(Scenario, 0, OutputRoot, Result.A);
		Result.bRanB = RunReplay(Scenario, 1, OutputRoot, Result.B);
		Result.bLayerHashesStable = Result.bRanA && Result.bRanB && LayerHashesMatch(Result.A, Result.B);
		Result.bOverallPass = Result.bRanA && Result.bRanB && Result.A.bExportReadOnly && Result.B.bExportReadOnly && Result.bLayerHashesStable;
		bOverallPass = bOverallPass && Result.bOverallPass;
		Results.Add(MoveTemp(Result));
	}

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FString MetricsJsonl;
	for (const FObservabilityScenarioResult& Result : Results)
	{
		MetricsJsonl += ReplayJson(Result.A);
		MetricsJsonl += LINE_TERMINATOR;
		MetricsJsonl += ReplayJson(Result.B);
		MetricsJsonl += LINE_TERMINATOR;
	}
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);

	const FString Report = BuildReport(OutputRoot, Results, bOverallPass, bIIICMode);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("CarrierLab Phase III maps %s. Metrics: %s Report: %s"),
		bOverallPass ? TEXT("passed") : TEXT("failed"),
		*MetricsPath,
		*ReportPath);
	return bOverallPass ? 0 : 1;
}
