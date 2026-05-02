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

	TArray<FObservabilityScenario> BuildScenarios()
	{
		TArray<FObservabilityScenario> Scenarios;

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

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("Observability"), TEXT("Maps"), Stamp);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	FString ResolveReportPath(const FString& Params)
	{
		FString ReportPath;
		if (!FParse::Value(*Params, TEXT("Report="), ReportPath))
		{
			ReportPath = FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("docs"),
				TEXT("checkpoints"),
				TEXT("phase-iii-observability-maps.md"));
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
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	bool ApplyMaterialFixture(ACarrierLabVisualizationActor& Actor, const FObservabilityScenario& Scenario)
	{
		switch (Scenario.MaterialFixture)
		{
		case EObservabilityMaterialFixture::MixedPlate0Continental:
			return Actor.SetPlateContinentalForTest(0, true) &&
				Actor.SetPlateContinentalForTest(1, false);
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
			TEXT("\"propagation_seed_hits\":%d,\"propagation_added\":%d,\"map_exports\":[%s],\"contact_sheet\":%s}"),
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
			*FString::Join(LayerJson, TEXT(",")),
			*JsonString(Result.ContactSheetPath));
	}

	FString BuildReport(
		const FString& OutputRoot,
		const TArray<FObservabilityScenarioResult>& Results,
		const bool bOverallPass)
	{
		FString Report;
		Report += TEXT("# Phase III Observability Map Export Checkpoint\n\n");
		Report += TEXT("Status: read-only observability patch before IIIC entry reconciliation.\n\n");
		Report += TEXT("## Scope\n\n");
		Report += TEXT("This checkpoint exports the Phase III actor-only spatial sanity layers to filled Mollweide-style PNG artifacts. It does not add process mutation, resampling behavior, triangle consumption, material transfer, forbidden authority fallback patterns, or projection-derived carrier authority.\n\n");
		Report += TEXT("Fixtures:\n\n");
		for (const FObservabilityScenarioResult& Result : Results)
		{
			Report += FString::Printf(
				TEXT("- `%s`: %s (`%dk / %d plates / seed %d / %d rigid steps / %s motion / %s material / centroid policy`).\n"),
				*Result.Scenario.Name,
				*Result.Scenario.Description,
				Result.Scenario.SampleCount / 1000,
				Result.Scenario.PlateCount,
				ObservabilitySeed,
				Result.Scenario.StepCount,
				*MotionFixtureName(Result.Scenario.MotionFixture),
				*MaterialFixtureName(Result.Scenario.MaterialFixture));
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
		Report += TEXT("- `ElevationHeatmap` uses the filled continental/oceanic base map when elevation is still zero, then overlays positive/negative elevation once IIIC.2/IIIC.3 mutate the scalar field.\n");
		Report += TEXT("- `SubductionMask` uses the filled base map plus IIIB polarity-derived role overlays; the forced-convergence fixture is the human-inspection map, not persistent IIIC subducting-triangle authority.\n");
		Report += TEXT("- `DistanceToFrontHeatmap` uses the filled base map plus active boundary distance overlays; the default baseline may be sparse, while the forced fixture intentionally exercises propagated front state.\n\n");
		Report += TEXT("## Recommendation\n\n");
		Report += bOverallPass
			? TEXT("Go for the docs-only IIIC entry reconciliation checkpoint. These exports are read-only and stable enough to serve as pre-mutation spatial sanity artifacts.\n")
			: TEXT("No-go for IIIC entry reconciliation until the failed export/read-only/determinism gate is investigated.\n");
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
	const FString OutputRoot = GetOutputRoot(Params);
	const FString ReportPath = ResolveReportPath(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);

	TArray<FObservabilityScenarioResult> Results;
	bool bOverallPass = true;
	for (const FObservabilityScenario& Scenario : BuildScenarios())
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

	const FString Report = BuildReport(OutputRoot, Results, bOverallPass);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("CarrierLab Phase III observability maps %s. Metrics: %s Report: %s"),
		bOverallPass ? TEXT("passed") : TEXT("failed"),
		*MetricsPath,
		*ReportPath);
	return bOverallPass ? 0 : 1;
}
