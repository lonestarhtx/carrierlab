// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIFCrustFieldSubstrateCommandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace
{
	constexpr int32 DefaultSampleCount = 100000;
	constexpr int32 DefaultPlateCount = 40;
	constexpr int32 DefaultSeed = 42;
	constexpr int32 ManualRemeshStep = 16;
	constexpr double DefaultContinentalPlateFraction = 0.30;
	constexpr double DefaultVelocityMmPerYear = 66.6666666667;
	constexpr int32 ContactThumbWidth = 512;
	constexpr int32 ContactThumbHeight = 256;

	struct FDiagnosticImage
	{
		FString Name;
		FString Label;
		FString Path;
		FString Hash;
		int32 Width = 0;
		int32 Height = 0;
		TArray<FColor> Pixels;
	};

	FString BoolText(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	FString PassFail(const bool bPass)
	{
		return bPass ? TEXT("pass") : TEXT("fail");
	}

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

	FString ResolvePath(const FString& Params, const TCHAR* Key, const FString& DefaultPath)
	{
		FString Value;
		if (!FParse::Value(*Params, Key, Value))
		{
			Value = DefaultPath;
		}
		else if (FPaths::IsRelative(Value))
		{
			Value = FPaths::Combine(FPaths::ProjectDir(), Value);
		}
		return FPaths::ConvertRelativePathToFull(Value);
	}

	FString GetOutputRoot(const FString& Params)
	{
		return ResolvePath(
			Params,
			TEXT("Out="),
			FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("CarrierLab"),
				TEXT("PhaseIII"),
				TEXT("IIIFCrustFieldSubstrate")));
	}

	FString GetReportPath(const FString& Params)
	{
		return ResolvePath(
			Params,
			TEXT("Report="),
			FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("docs"),
				TEXT("checkpoints"),
				TEXT("phase-iii-slice-iiif-crust-field-substrate.md")));
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

	FString LayerName(const ECarrierLabVisualizationLayer Layer)
	{
		switch (Layer)
		{
		case ECarrierLabVisualizationLayer::StatePlateId: return TEXT("StatePlateId");
		case ECarrierLabVisualizationLayer::PlateProjectionMismatch: return TEXT("PlateProjectionMismatch");
		case ECarrierLabVisualizationLayer::ProjectionDiagnostics: return TEXT("ProjectionDiagnostics");
		case ECarrierLabVisualizationLayer::ContinentalFraction: return TEXT("ContinentalFraction");
		case ECarrierLabVisualizationLayer::PhaseIIIERemeshSummary: return TEXT("PhaseIIIERemeshSummary");
		case ECarrierLabVisualizationLayer::OceanicAgeHeatmap: return TEXT("OceanicAge");
		case ECarrierLabVisualizationLayer::BathymetricElevation: return TEXT("BathymetricElevation");
		case ECarrierLabVisualizationLayer::CrustSubstrateClass: return TEXT("CrustSubstrateClass");
		default: return TEXT("Layer");
		}
	}

	FString LayerLabel(const ECarrierLabVisualizationLayer Layer)
	{
		switch (Layer)
		{
		case ECarrierLabVisualizationLayer::StatePlateId: return TEXT("state plate ownership");
		case ECarrierLabVisualizationLayer::PlateProjectionMismatch: return TEXT("projection mismatch");
		case ECarrierLabVisualizationLayer::ProjectionDiagnostics: return TEXT("projection diagnostics");
		case ECarrierLabVisualizationLayer::ContinentalFraction: return TEXT("continental fraction");
		case ECarrierLabVisualizationLayer::PhaseIIIERemeshSummary: return TEXT("remesh provenance");
		case ECarrierLabVisualizationLayer::OceanicAgeHeatmap: return TEXT("oceanic age");
		case ECarrierLabVisualizationLayer::BathymetricElevation: return TEXT("bathymetric elevation");
		case ECarrierLabVisualizationLayer::CrustSubstrateClass: return TEXT("crust substrate class");
		default: return TEXT("layer");
		}
	}

	ACarrierLabVisualizationActor* SpawnIIIFActor(UWorld& World)
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
		Actor->SampleCount = DefaultSampleCount;
		Actor->PlateCount = DefaultPlateCount;
		Actor->Seed = DefaultSeed;
		Actor->ContinentalPlateFraction = DefaultContinentalPlateFraction;
		Actor->VelocityMmPerYear = DefaultVelocityMmPerYear;
		Actor->bEnableNaturalResamplingEvents = false;
		Actor->bEnablePhaseIIIE3DuplicateHitCoalescing = true;
		Actor->bEnablePhaseIIIE3SharedBoundaryTieBreak = true;
		Actor->bEnablePhaseIIIE3NearestHitTieBreak = true;
		Actor->bExtendPhaseIIIE3NearestHitToMixedMaterial = true;
		Actor->bEnablePhaseIIIE3DistanceTieFallback = true;
		Actor->bRestoreNonSeparatingAnomalyVeto = false;
		Actor->bEnablePhaseIIIE6MaterialCoherenceGuard = true;
		Actor->bEnablePhaseIIIE6ResolvedHitMaterialPreservation = true;
		Actor->ConfigurePhaseIIICProcessLayer(true, false);
		Actor->bEnablePhaseIIIOceanicAgeLifecycle = true;
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	bool SaveImage(const FString& OutputRoot, FDiagnosticImage& Image)
	{
		Image.Path = FPaths::Combine(OutputRoot, Image.Name + TEXT(".png"));
		Image.Hash = HashToString(HashPixels(Image.Pixels));
		return SavePng(Image.Path, Image.Pixels, Image.Width, Image.Height);
	}

	bool CaptureLayer(
		const FString& OutputRoot,
		const ACarrierLabVisualizationActor& Actor,
		const ECarrierLabVisualizationLayer Layer,
		const FString& Prefix,
		TArray<FDiagnosticImage>& Images)
	{
		FDiagnosticImage Image;
		Image.Name = Prefix + LayerName(Layer);
		Image.Label = Prefix + TEXT(" ") + LayerLabel(Layer);
		if (!Actor.BuildVisualizationLayerMap(Layer, Image.Pixels, Image.Width, Image.Height))
		{
			return false;
		}
		if (!SaveImage(OutputRoot, Image))
		{
			return false;
		}
		Images.Add(MoveTemp(Image));
		return true;
	}

	bool WriteContactSheet(const FString& OutputRoot, const TArray<FDiagnosticImage>& Images, FString& OutPath)
	{
		const int32 Columns = 3;
		const int32 Rows = FMath::DivideAndRoundUp(Images.Num(), Columns);
		const int32 SheetWidth = ContactThumbWidth * Columns;
		const int32 SheetHeight = ContactThumbHeight * Rows;
		TArray<FColor> SheetPixels;
		SheetPixels.Init(FColor(6, 9, 12, 255), SheetWidth * SheetHeight);

		for (int32 Index = 0; Index < Images.Num(); ++Index)
		{
			const int32 Column = Index % Columns;
			const int32 Row = Index / Columns;
			BlitThumbnail(
				Images[Index].Pixels,
				Images[Index].Width,
				Images[Index].Height,
				SheetPixels,
				SheetWidth,
				Column * ContactThumbWidth,
				Row * ContactThumbHeight);
		}

		OutPath = FPaths::Combine(OutputRoot, TEXT("ContactSheet.png"));
		return SavePng(OutPath, SheetPixels, SheetWidth, SheetHeight);
	}

	FString SnapshotJson(const FCarrierLabIIIFCrustFieldSnapshot& Snapshot)
	{
		return FString::Printf(
			TEXT("{\"step\":%d,\"event_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"plate_vertex_count\":%d,\"invalid_sample_fields\":%d,\"invalid_plate_vertex_fields\":%d,\"sample_oceanic_count\":%d,\"plate_oceanic_vertex_count\":%d,\"sample_oceanic_strict_above_sea\":%d,\"plate_oceanic_strict_above_sea\":%d,\"sample_oceanic_positive_age\":%d,\"plate_oceanic_positive_age\":%d,\"sample_elevation_min_km\":%.9f,\"sample_elevation_max_km\":%.9f,\"sample_elevation_mean_km\":%.9f,\"sample_continental_max_elevation_km\":%.9f,\"sample_oceanic_min_elevation_km\":%.9f,\"sample_oceanic_max_elevation_km\":%.9f,\"sample_oceanic_mean_elevation_km\":%.9f,\"sample_oceanic_max_age_ma\":%.9f,\"sample_oceanic_mean_age_ma\":%.9f,\"plate_vertex_min_elevation_km\":%.9f,\"plate_vertex_max_elevation_km\":%.9f,\"plate_vertex_mean_elevation_km\":%.9f,\"plate_vertex_continental_max_elevation_km\":%.9f,\"plate_oceanic_min_elevation_km\":%.9f,\"plate_oceanic_max_elevation_km\":%.9f,\"plate_oceanic_mean_elevation_km\":%.9f,\"plate_oceanic_max_age_ma\":%.9f,\"plate_oceanic_mean_age_ma\":%.9f,\"plate_vertex_with_sample_id\":%d,\"plate_vertex_sample_material_mismatch\":%d,\"plate_vertex_sample_field_mismatch\":%d,\"max_vertex_sample_elevation_delta_km\":%.9f,\"mean_vertex_sample_elevation_delta_km\":%.9f,\"max_vertex_sample_historical_elevation_delta_km\":%.9f,\"mean_vertex_sample_historical_elevation_delta_km\":%.9f,\"max_vertex_sample_oceanic_age_delta_ma\":%.9f,\"mean_vertex_sample_oceanic_age_delta_ma\":%.9f,\"max_vertex_sample_ridge_direction_delta\":%.9f,\"max_vertex_sample_fold_direction_delta\":%.9f,\"crust_hash\":%s}"),
			Snapshot.Step,
			Snapshot.EventCount,
			Snapshot.SampleCount,
			Snapshot.PlateCount,
			Snapshot.PlateVertexCount,
			Snapshot.InvalidSampleFieldCount,
			Snapshot.InvalidPlateVertexFieldCount,
			Snapshot.SampleOceanicCount,
			Snapshot.PlateOceanicVertexCount,
			Snapshot.SampleOceanicStrictAboveSeaLevelCount,
			Snapshot.PlateOceanicStrictAboveSeaLevelCount,
			Snapshot.SampleOceanicPositiveAgeCount,
			Snapshot.PlateOceanicPositiveAgeCount,
			Snapshot.SampleMinElevationKm,
			Snapshot.SampleMaxElevationKm,
			Snapshot.SampleMeanElevationKm,
			Snapshot.SampleContinentalMaxElevationKm,
			Snapshot.SampleOceanicMinElevationKm,
			Snapshot.SampleOceanicMaxElevationKm,
			Snapshot.SampleOceanicMeanElevationKm,
			Snapshot.SampleOceanicMaxAgeMa,
			Snapshot.SampleOceanicMeanAgeMa,
			Snapshot.PlateVertexMinElevationKm,
			Snapshot.PlateVertexMaxElevationKm,
			Snapshot.PlateVertexMeanElevationKm,
			Snapshot.PlateVertexContinentalMaxElevationKm,
			Snapshot.PlateOceanicMinElevationKm,
			Snapshot.PlateOceanicMaxElevationKm,
			Snapshot.PlateOceanicMeanElevationKm,
			Snapshot.PlateOceanicMaxAgeMa,
			Snapshot.PlateOceanicMeanAgeMa,
			Snapshot.PlateVertexWithSampleIdCount,
			Snapshot.PlateVertexSampleMaterialMismatchCount,
			Snapshot.PlateVertexSampleFieldMismatchCount,
			Snapshot.MaxVertexSampleElevationDeltaKm,
			Snapshot.MeanVertexSampleElevationDeltaKm,
			Snapshot.MaxVertexSampleHistoricalElevationDeltaKm,
			Snapshot.MeanVertexSampleHistoricalElevationDeltaKm,
			Snapshot.MaxVertexSampleOceanicAgeDeltaMa,
			Snapshot.MeanVertexSampleOceanicAgeDeltaMa,
			Snapshot.MaxVertexSampleRidgeDirectionDelta,
			Snapshot.MaxVertexSampleFoldDirectionDelta,
			*JsonString(Snapshot.CrustStateHash));
	}

	FString ClassCountsJson(const FCarrierLabIIIFCrustSubstrateClassCounts& Counts)
	{
		return FString::Printf(
			TEXT("{\"invalid\":%d,\"continental_land\":%d,\"continental_submerged\":%d,\"oceanic_bathymetry\":%d,\"oceanic_sea_level_clamp\":%d,\"generated_oceanic_crust\":%d,\"rifting_pending_continental_preservation\":%d,\"oceanic_above_sea_level\":%d}"),
			Counts.Invalid,
			Counts.ContinentalLand,
			Counts.ContinentalSubmerged,
			Counts.OceanicBathymetry,
			Counts.OceanicSeaLevelClamp,
			Counts.GeneratedOceanicCrust,
			Counts.RiftingPendingContinentalPreservation,
			Counts.OceanicAboveSeaLevel);
	}

	FString ImagesJson(const TArray<FDiagnosticImage>& Images, const FString& ContactSheetPath)
	{
		TArray<FString> Parts;
		for (const FDiagnosticImage& Image : Images)
		{
			Parts.Add(FString::Printf(
				TEXT("{\"name\":%s,\"label\":%s,\"hash\":%s,\"path\":%s}"),
				*JsonString(Image.Name),
				*JsonString(Image.Label),
				*JsonString(Image.Hash),
				*JsonString(Image.Path)));
		}
		Parts.Add(FString::Printf(TEXT("{\"name\":\"ContactSheet\",\"label\":\"contact sheet\",\"path\":%s}"), *JsonString(ContactSheetPath)));
		return FString::Printf(TEXT("[%s]"), *FString::Join(Parts, TEXT(",")));
	}

	FString AuditJson(
		const FCarrierLabIIIFCrustFieldSubstrateAudit& Audit,
		const TArray<FDiagnosticImage>& Images,
		const FString& ContactSheetPath)
	{
		return FString::Printf(
			TEXT("{\"slice\":\"IIIF\",\"goal\":\"crust_field_substrate\",\"scenario\":\"editor_default_step16_manual_remesh\",\"ran\":%s,\"remesh_audit_ran\":%s,\"remesh_applied\":%s,\"oceanic_age_lifecycle_enabled\":%s,\"verdict\":%s,\"verdict_reason\":%s,\"step_before\":%d,\"step_after\":%d,\"event_count_before\":%d,\"event_count_after\":%d,\"max_allowed_oceanic_elevation_km\":%.9f,\"max_allowed_continental_elevation_km\":%.9f,\"min_allowed_elevation_km\":%.9f,\"post_oceanic_strict_above_sea_growth_fraction\":%.9f,\"remesh_verdict\":%s,\"remesh_verdict_reason\":%s,\"last_remesh_mode\":%s,\"pre\":%s,\"post\":%s,\"pre_class_counts\":%s,\"post_class_counts\":%s,\"images\":%s}"),
			*BoolText(Audit.bRan),
			*BoolText(Audit.bRemeshAuditRan),
			*BoolText(Audit.bRemeshApplied),
			*BoolText(Audit.bOceanicAgeLifecycleEnabled),
			*JsonString(Audit.Verdict),
			*JsonString(Audit.VerdictReason),
			Audit.StepBefore,
			Audit.StepAfter,
			Audit.EventCountBefore,
			Audit.EventCountAfter,
			Audit.MaxAllowedOceanicElevationKm,
			Audit.MaxAllowedContinentalElevationKm,
			Audit.MinAllowedElevationKm,
			Audit.PostOceanicStrictAboveSeaLevelGrowthFraction,
			*JsonString(Audit.RemeshAudit.Verdict),
			*JsonString(Audit.RemeshAudit.VerdictReason),
			*JsonString(Audit.RemeshAudit.LastRemeshMode),
			*SnapshotJson(Audit.Pre),
			*SnapshotJson(Audit.Post),
			*ClassCountsJson(Audit.PreClassCounts),
			*ClassCountsJson(Audit.PostClassCounts),
			*ImagesJson(Images, ContactSheetPath));
	}

	FString ImagePurpose(const FString& ImageName)
	{
		if (ImageName == TEXT("PreStatePlateId"))
		{
			return TEXT("Baseline authoritative sample ownership before remesh.");
		}
		if (ImageName == TEXT("PreContinentalFraction"))
		{
			return TEXT("Baseline material map before remesh; used to detect continental fragmentation.");
		}
		if (ImageName == TEXT("PreBathymetricElevation"))
		{
			return TEXT("Baseline global-sample terrain. Plate-local metrics in the report show hidden live carrier elevation.");
		}
		if (ImageName == TEXT("PreOceanicAge"))
		{
			return TEXT("Baseline global-sample oceanic age; compare with plate-local age metrics.");
		}
		if (ImageName == TEXT("PostBathymetricElevation"))
		{
			return TEXT("Primary visual check for impossible post-remesh landmass or oceanic uplift.");
		}
		if (ImageName == TEXT("PreCrustSubstrateClass"))
		{
			return TEXT("Baseline substrate classification: continental land, submerged continental crust, oceanic bathymetry, and sea-level oceanic clamp.");
		}
		if (ImageName == TEXT("PostCrustSubstrateClass"))
		{
			return TEXT("Explains apparent land/ocean ambiguity by separating continental land, submerged continental crust, oceanic bathymetry, sea-level oceanic clamp, generated oceanic crust, rifting-pending preservation, and invalid oceanic land.");
		}
		if (ImageName == TEXT("PostOceanicAge"))
		{
			return TEXT("Checks whether aged oceanic crust survives remesh while generated crust starts young.");
		}
		if (ImageName == TEXT("PostPhaseIIIERemeshSummary"))
		{
			return TEXT("Shows generated oceanic and rifting-pending remesh provenance.");
		}
		if (ImageName == TEXT("PostPlateProjectionMismatch"))
		{
			return TEXT("Separates authoritative state problems from projected visualization mismatch.");
		}
		if (ImageName == TEXT("PostProjectionDiagnostics"))
		{
			return TEXT("Condenses miss, overlap, and boundary ray health into one projection QA map.");
		}
		return TEXT("Curated diagnostic map.");
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FString& MetricsPath,
		const FString& ContactSheetPath,
		const FCarrierLabIIIFCrustFieldSubstrateAudit& Audit,
		const TArray<FDiagnosticImage>& Images)
	{
		const bool bAgeGate = Audit.bOceanicAgeLifecycleEnabled &&
			(Audit.Pre.PlateOceanicPositiveAgeCount > 0 || Audit.Post.PlateOceanicPositiveAgeCount > 0 || Audit.Post.SampleOceanicPositiveAgeCount > 0);
		const bool bPreBounds =
			Audit.Pre.PlateVertexMaxElevationKm <= Audit.MaxAllowedContinentalElevationKm + 1.0e-6 &&
			Audit.Pre.PlateOceanicMaxElevationKm <= Audit.MaxAllowedOceanicElevationKm + 1.0e-6 &&
			Audit.Pre.PlateVertexMinElevationKm >= Audit.MinAllowedElevationKm - 1.0e-6;
		const bool bPostBounds =
			Audit.Post.SampleMaxElevationKm <= Audit.MaxAllowedContinentalElevationKm + 1.0e-6 &&
			Audit.Post.SampleOceanicMaxElevationKm <= Audit.MaxAllowedOceanicElevationKm + 1.0e-6 &&
			Audit.Post.SampleMinElevationKm >= Audit.MinAllowedElevationKm - 1.0e-6 &&
			Audit.Post.PlateVertexMaxElevationKm <= Audit.MaxAllowedContinentalElevationKm + 1.0e-6 &&
			Audit.Post.PlateOceanicMaxElevationKm <= Audit.MaxAllowedOceanicElevationKm + 1.0e-6 &&
			Audit.Post.PlateVertexMinElevationKm >= Audit.MinAllowedElevationKm - 1.0e-6;
		const bool bPostVertexRecords = Audit.Post.PlateVertexSampleMaterialMismatchCount == 0 &&
			Audit.Post.PlateVertexSampleFieldMismatchCount == 0;

		FString Report;
		Report += TEXT("# IIIF: Crust Field Substrate\n\n");
		Report += FString::Printf(TEXT("Verdict: **%s**. %s\n\n"), *Audit.Verdict, *Audit.VerdictReason);
		Report += TEXT("This checkpoint is IIIF. It defines the crust-field substrate that later rifting, erosion, and tectonic processes depend on. It does not implement rifting or IIIG erosion, and erosion is not allowed to hide these bounds.\n\n");

		Report += TEXT("## Scenario\n\n");
		Report += TEXT("| Field | Value |\n");
		Report += TEXT("|---|---:|\n");
		Report += FString::Printf(TEXT("| sample count | %d |\n"), DefaultSampleCount);
		Report += FString::Printf(TEXT("| plate count | %d |\n"), DefaultPlateCount);
		Report += FString::Printf(TEXT("| seed | %d |\n"), DefaultSeed);
		Report += FString::Printf(TEXT("| continental plate fraction | %.2f |\n"), DefaultContinentalPlateFraction);
		Report += FString::Printf(TEXT("| velocity mm/yr | %.10f |\n"), DefaultVelocityMmPerYear);
		Report += TEXT("| Phase III process layer | on |\n");
		Report += TEXT("| slab pull | off |\n");
		Report += FString::Printf(TEXT("| oceanic age lifecycle | %s |\n"), *BoolText(Audit.bOceanicAgeLifecycleEnabled));
		Report += FString::Printf(TEXT("| manual remesh step | %d |\n\n"), ManualRemeshStep);

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---:|---|\n");
		Report += FString::Printf(TEXT("| Oceanic age lifecycle | %s | pre plate positive `%d`, post plate positive `%d`, post sample positive `%d` |\n"), *PassFail(bAgeGate), Audit.Pre.PlateOceanicPositiveAgeCount, Audit.Post.PlateOceanicPositiveAgeCount, Audit.Post.SampleOceanicPositiveAgeCount);
		Report += FString::Printf(TEXT("| Pre-remesh plate-local bounds | %s | elevation `%.6f..%.6f km`; oceanic max `%.6f km`; allowed `%.6f..%.6f km`, oceanic `<= %.6f km` |\n"), *PassFail(bPreBounds), Audit.Pre.PlateVertexMinElevationKm, Audit.Pre.PlateVertexMaxElevationKm, Audit.Pre.PlateOceanicMaxElevationKm, Audit.MinAllowedElevationKm, Audit.MaxAllowedContinentalElevationKm, Audit.MaxAllowedOceanicElevationKm);
		Report += FString::Printf(TEXT("| Manual remesh applied | %s | `%s` |\n"), *PassFail(Audit.bRemeshApplied), *Audit.RemeshAudit.LastRemeshMode);
		Report += FString::Printf(TEXT("| Remesh coherence audit | %s | `%s`: %s |\n"), *PassFail(!Audit.RemeshAudit.Verdict.StartsWith(TEXT("FAIL")) && Audit.RemeshAudit.bRan), *Audit.RemeshAudit.Verdict, *Audit.RemeshAudit.VerdictReason);
		Report += FString::Printf(TEXT("| Post-remesh crust bounds | %s | sample `%.6f..%.6f km`, sample oceanic max `%.6f km`; plate `%.6f..%.6f km`, plate oceanic max `%.6f km` |\n"), *PassFail(bPostBounds), Audit.Post.SampleMinElevationKm, Audit.Post.SampleMaxElevationKm, Audit.Post.SampleOceanicMaxElevationKm, Audit.Post.PlateVertexMinElevationKm, Audit.Post.PlateVertexMaxElevationKm, Audit.Post.PlateOceanicMaxElevationKm);
		Report += FString::Printf(TEXT("| Post-remesh vertex records | %s | material mismatches `%d`, field mismatches `%d`, max elevation delta `%.9f km`, max age delta `%.9f Ma` |\n\n"), *PassFail(bPostVertexRecords), Audit.Post.PlateVertexSampleMaterialMismatchCount, Audit.Post.PlateVertexSampleFieldMismatchCount, Audit.Post.MaxVertexSampleElevationDeltaKm, Audit.Post.MaxVertexSampleOceanicAgeDeltaMa);

		Report += TEXT("## Numbers First\n\n");
		Report += TEXT("| Metric | Pre samples | Post samples | Pre plate vertices | Post plate vertices |\n");
		Report += TEXT("|---|---:|---:|---:|---:|\n");
		Report += FString::Printf(TEXT("| elevation range km | %.6f..%.6f | %.6f..%.6f | %.6f..%.6f | %.6f..%.6f |\n"), Audit.Pre.SampleMinElevationKm, Audit.Pre.SampleMaxElevationKm, Audit.Post.SampleMinElevationKm, Audit.Post.SampleMaxElevationKm, Audit.Pre.PlateVertexMinElevationKm, Audit.Pre.PlateVertexMaxElevationKm, Audit.Post.PlateVertexMinElevationKm, Audit.Post.PlateVertexMaxElevationKm);
		Report += FString::Printf(TEXT("| oceanic elevation range km | %.6f..%.6f | %.6f..%.6f | %.6f..%.6f | %.6f..%.6f |\n"), Audit.Pre.SampleOceanicMinElevationKm, Audit.Pre.SampleOceanicMaxElevationKm, Audit.Post.SampleOceanicMinElevationKm, Audit.Post.SampleOceanicMaxElevationKm, Audit.Pre.PlateOceanicMinElevationKm, Audit.Pre.PlateOceanicMaxElevationKm, Audit.Post.PlateOceanicMinElevationKm, Audit.Post.PlateOceanicMaxElevationKm);
		Report += FString::Printf(TEXT("| oceanic above sea level strict | %d | %d | %d | %d |\n"), Audit.Pre.SampleOceanicStrictAboveSeaLevelCount, Audit.Post.SampleOceanicStrictAboveSeaLevelCount, Audit.Pre.PlateOceanicStrictAboveSeaLevelCount, Audit.Post.PlateOceanicStrictAboveSeaLevelCount);
		Report += FString::Printf(TEXT("| oceanic positive age count | %d | %d | %d | %d |\n"), Audit.Pre.SampleOceanicPositiveAgeCount, Audit.Post.SampleOceanicPositiveAgeCount, Audit.Pre.PlateOceanicPositiveAgeCount, Audit.Post.PlateOceanicPositiveAgeCount);
		Report += FString::Printf(TEXT("| oceanic max age Ma | %.6f | %.6f | %.6f | %.6f |\n"), Audit.Pre.SampleOceanicMaxAgeMa, Audit.Post.SampleOceanicMaxAgeMa, Audit.Pre.PlateOceanicMaxAgeMa, Audit.Post.PlateOceanicMaxAgeMa);
		Report += FString::Printf(TEXT("| invalid crust fields | %d | %d | %d | %d |\n\n"), Audit.Pre.InvalidSampleFieldCount, Audit.Post.InvalidSampleFieldCount, Audit.Pre.InvalidPlateVertexFieldCount, Audit.Post.InvalidPlateVertexFieldCount);

		Report += TEXT("## Authority Coherence\n\n");
		Report += TEXT("| Metric | Pre | Post |\n");
		Report += TEXT("|---|---:|---:|\n");
		Report += FString::Printf(TEXT("| plate vertices with sample ids | %d | %d |\n"), Audit.Pre.PlateVertexWithSampleIdCount, Audit.Post.PlateVertexWithSampleIdCount);
		Report += FString::Printf(TEXT("| material mismatches | %d | %d |\n"), Audit.Pre.PlateVertexSampleMaterialMismatchCount, Audit.Post.PlateVertexSampleMaterialMismatchCount);
		Report += FString::Printf(TEXT("| field mismatches | %d | %d |\n"), Audit.Pre.PlateVertexSampleFieldMismatchCount, Audit.Post.PlateVertexSampleFieldMismatchCount);
		Report += FString::Printf(TEXT("| max elevation delta km | %.9f | %.9f |\n"), Audit.Pre.MaxVertexSampleElevationDeltaKm, Audit.Post.MaxVertexSampleElevationDeltaKm);
		Report += FString::Printf(TEXT("| mean elevation delta km | %.9f | %.9f |\n"), Audit.Pre.MeanVertexSampleElevationDeltaKm, Audit.Post.MeanVertexSampleElevationDeltaKm);
		Report += FString::Printf(TEXT("| max oceanic age delta Ma | %.9f | %.9f |\n\n"), Audit.Pre.MaxVertexSampleOceanicAgeDeltaMa, Audit.Post.MaxVertexSampleOceanicAgeDeltaMa);

		Report += TEXT("## Remesh Context\n\n");
		Report += TEXT("| Metric | Value |\n");
		Report += TEXT("|---|---:|\n");
		Report += FString::Printf(TEXT("| remesh verdict | `%s` |\n"), *Audit.RemeshAudit.Verdict);
		Report += FString::Printf(TEXT("| generated candidates | %d |\n"), Audit.RemeshAudit.PhaseIIIELastGeneratedCandidateCount);
		Report += FString::Printf(TEXT("| applied generated | %d |\n"), Audit.RemeshAudit.PhaseIIIELastAppliedGeneratedCount);
		Report += FString::Printf(TEXT("| rifting pending | %d |\n"), Audit.RemeshAudit.PhaseIIIELastRiftingPendingCount);
		Report += FString::Printf(TEXT("| material-preserved records | %d |\n"), Audit.RemeshAudit.PhaseIIIELastMaterialPreservedRecordCount);
		Report += FString::Printf(TEXT("| plate regularized samples | %d |\n"), Audit.RemeshAudit.PhaseIIIELastPlateComponentRegularizedSampleCount);
		Report += FString::Printf(TEXT("| oceanic strict above-sea growth fraction | %.9f |\n\n"), Audit.PostOceanicStrictAboveSeaLevelGrowthFraction);

		Report += TEXT("## Substrate Classification\n\n");
		Report += TEXT("| Class | Pre samples | Post samples |\n");
		Report += TEXT("|---|---:|---:|\n");
		Report += FString::Printf(TEXT("| continental land | %d | %d |\n"), Audit.PreClassCounts.ContinentalLand, Audit.PostClassCounts.ContinentalLand);
		Report += FString::Printf(TEXT("| continental shelf/submerged crust | %d | %d |\n"), Audit.PreClassCounts.ContinentalSubmerged, Audit.PostClassCounts.ContinentalSubmerged);
		Report += FString::Printf(TEXT("| oceanic bathymetry | %d | %d |\n"), Audit.PreClassCounts.OceanicBathymetry, Audit.PostClassCounts.OceanicBathymetry);
		Report += FString::Printf(TEXT("| sea-level oceanic clamp | %d | %d |\n"), Audit.PreClassCounts.OceanicSeaLevelClamp, Audit.PostClassCounts.OceanicSeaLevelClamp);
		Report += FString::Printf(TEXT("| generated oceanic crust | %d | %d |\n"), Audit.PreClassCounts.GeneratedOceanicCrust, Audit.PostClassCounts.GeneratedOceanicCrust);
		Report += FString::Printf(TEXT("| rifting-pending continental preservation | %d | %d |\n"), Audit.PreClassCounts.RiftingPendingContinentalPreservation, Audit.PostClassCounts.RiftingPendingContinentalPreservation);
		Report += FString::Printf(TEXT("| oceanic above-sea invalid | %d | %d |\n"), Audit.PreClassCounts.OceanicAboveSeaLevel, Audit.PostClassCounts.OceanicAboveSeaLevel);
		Report += FString::Printf(TEXT("| invalid / non-finite | %d | %d |\n\n"), Audit.PreClassCounts.Invalid, Audit.PostClassCounts.Invalid);

		Report += TEXT("## Images\n\n");
		Report += FString::Printf(TEXT("Contact sheet: `%s`\n\n"), *ContactSheetPath);
		Report += TEXT("| Image | Hash | Why it is included | Path |\n");
		Report += TEXT("|---|---|---|---|\n");
		for (const FDiagnosticImage& Image : Images)
		{
			Report += FString::Printf(TEXT("| `%s` | `%s` | %s | `%s` |\n"), *Image.Name, *Image.Hash, *ImagePurpose(Image.Name), *Image.Path);
		}

		Report += TEXT("\n## Decision\n\n");
		if (Audit.Verdict == TEXT("PASS_IIIF_CRUST_FIELD_SUBSTRATE"))
		{
			Report += TEXT("IIIF crust-field substrate is closed for this scenario. IIIG rifting and IIII surface processes remain downstream consumers of this gate.\n\n");
		}
		else
		{
			Report += TEXT("IIIF remains blocked. Fix the failing substrate gate above before adding rifting, erosion, or other downstream topology/process work.\n\n");
		}
		Report += FString::Printf(TEXT("Metrics: `%s`.\nOutput root: `%s`.\n"), *MetricsPath, *OutputRoot);
		return Report;
	}
}

UCarrierLabPhaseIIIFCrustFieldSubstrateCommandlet::UCarrierLabPhaseIIIFCrustFieldSubstrateCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIFCrustFieldSubstrateCommandlet::Main(const FString& Params)
{
	const double StartSeconds = FPlatformTime::Seconds();
	const FString OutputRoot = GetOutputRoot(Params);
	const FString ReportPath = GetReportPath(Params);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("phase-iii-slice-iiif-crust-field-substrate.jsonl"));
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	TArray<FString> ExistingPngs;
	IFileManager::Get().FindFiles(ExistingPngs, *FPaths::Combine(OutputRoot, TEXT("*.png")), true, false);
	for (const FString& ExistingPng : ExistingPngs)
	{
		IFileManager::Get().Delete(*FPaths::Combine(OutputRoot, ExistingPng), false, true);
	}

	UWorld* World = GetCommandletWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab IIIF crust field substrate failed: no commandlet world."));
		return 1;
	}

	ACarrierLabVisualizationActor* Actor = SpawnIIIFActor(*World);
	if (Actor == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab IIIF crust field substrate failed: actor spawn failed."));
		return 1;
	}
	if (!Actor->InitializeCarrier())
	{
		Actor->Destroy();
		UE_LOG(LogTemp, Error, TEXT("CarrierLab IIIF crust field substrate failed: carrier initialization failed."));
		return 1;
	}

	for (int32 Step = 0; Step < ManualRemeshStep; ++Step)
	{
		Actor->StepOnce();
	}

	TArray<ECarrierLabVisualizationLayer> PreLayers;
	PreLayers.Add(ECarrierLabVisualizationLayer::StatePlateId);
	PreLayers.Add(ECarrierLabVisualizationLayer::ContinentalFraction);
	PreLayers.Add(ECarrierLabVisualizationLayer::BathymetricElevation);
	PreLayers.Add(ECarrierLabVisualizationLayer::CrustSubstrateClass);
	PreLayers.Add(ECarrierLabVisualizationLayer::OceanicAgeHeatmap);

	TArray<ECarrierLabVisualizationLayer> PostLayers;
	PostLayers.Add(ECarrierLabVisualizationLayer::BathymetricElevation);
	PostLayers.Add(ECarrierLabVisualizationLayer::CrustSubstrateClass);
	PostLayers.Add(ECarrierLabVisualizationLayer::OceanicAgeHeatmap);
	PostLayers.Add(ECarrierLabVisualizationLayer::PhaseIIIERemeshSummary);
	PostLayers.Add(ECarrierLabVisualizationLayer::PlateProjectionMismatch);
	PostLayers.Add(ECarrierLabVisualizationLayer::ProjectionDiagnostics);

	TArray<FDiagnosticImage> Images;
	bool bImagesOk = true;
	for (const ECarrierLabVisualizationLayer Layer : PreLayers)
	{
		bImagesOk = CaptureLayer(OutputRoot, *Actor, Layer, TEXT("Pre"), Images) && bImagesOk;
	}

	FCarrierLabIIIFCrustFieldSubstrateAudit Audit;
	const bool bAuditOk = Actor->RunPhaseIIIFCrustFieldSubstrateAudit(Audit);

	for (const ECarrierLabVisualizationLayer Layer : PostLayers)
	{
		bImagesOk = CaptureLayer(OutputRoot, *Actor, Layer, TEXT("Post"), Images) && bImagesOk;
	}

	FString ContactSheetPath;
	const bool bContactSheetOk = WriteContactSheet(OutputRoot, Images, ContactSheetPath);
	const FString MetricsJson = AuditJson(Audit, Images, ContactSheetPath) + LINE_TERMINATOR;
	const bool bMetricsOk = FFileHelper::SaveStringToFile(MetricsJson, *MetricsPath);
	const FString Report = BuildReport(OutputRoot, MetricsPath, ContactSheetPath, Audit, Images);
	const bool bReportOk = FFileHelper::SaveStringToFile(Report, *ReportPath);

	Actor->Destroy();
	CollectGarbage(RF_NoFlags);

	const bool bGatePass = Audit.Verdict.StartsWith(TEXT("PASS"));
	const bool bCommandletOk = bAuditOk && Audit.bRan && bImagesOk && bContactSheetOk && bMetricsOk && bReportOk && bGatePass;
	if (bCommandletOk)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("CarrierLab IIIF crust field substrate passed in %.3f s. Verdict=%s Metrics=%s Report=%s ContactSheet=%s"),
			FPlatformTime::Seconds() - StartSeconds,
			*Audit.Verdict,
			*MetricsPath,
			*ReportPath,
			*ContactSheetPath);
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("CarrierLab IIIF crust field substrate blocked in %.3f s. Verdict=%s Metrics=%s Report=%s ContactSheet=%s"),
			FPlatformTime::Seconds() - StartSeconds,
			*Audit.Verdict,
			*MetricsPath,
			*ReportPath,
			*ContactSheetPath);
	}
	return bCommandletOk ? 0 : 1;
}
