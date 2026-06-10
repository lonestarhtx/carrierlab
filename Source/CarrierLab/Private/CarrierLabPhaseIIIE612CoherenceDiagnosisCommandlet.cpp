// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIE612CoherenceDiagnosisCommandlet.h"

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

	uint8 GlyphRow(const TCHAR Ch, const int32 Row)
	{
		const TCHAR Upper = FChar::ToUpper(Ch);
		switch (Upper)
		{
		case TEXT('A'): { static constexpr uint8 R[] = { 14, 17, 17, 31, 17, 17, 17 }; return R[Row]; }
		case TEXT('B'): { static constexpr uint8 R[] = { 30, 17, 17, 30, 17, 17, 30 }; return R[Row]; }
		case TEXT('C'): { static constexpr uint8 R[] = { 14, 17, 16, 16, 16, 17, 14 }; return R[Row]; }
		case TEXT('D'): { static constexpr uint8 R[] = { 30, 17, 17, 17, 17, 17, 30 }; return R[Row]; }
		case TEXT('E'): { static constexpr uint8 R[] = { 31, 16, 16, 30, 16, 16, 31 }; return R[Row]; }
		case TEXT('F'): { static constexpr uint8 R[] = { 31, 16, 16, 30, 16, 16, 16 }; return R[Row]; }
		case TEXT('G'): { static constexpr uint8 R[] = { 14, 16, 16, 23, 17, 17, 15 }; return R[Row]; }
		case TEXT('H'): { static constexpr uint8 R[] = { 17, 17, 17, 31, 17, 17, 17 }; return R[Row]; }
		case TEXT('I'): { static constexpr uint8 R[] = { 14, 4, 4, 4, 4, 4, 14 }; return R[Row]; }
		case TEXT('J'): { static constexpr uint8 R[] = { 7, 2, 2, 2, 18, 18, 12 }; return R[Row]; }
		case TEXT('K'): { static constexpr uint8 R[] = { 17, 18, 20, 24, 20, 18, 17 }; return R[Row]; }
		case TEXT('L'): { static constexpr uint8 R[] = { 16, 16, 16, 16, 16, 16, 31 }; return R[Row]; }
		case TEXT('M'): { static constexpr uint8 R[] = { 17, 27, 21, 21, 17, 17, 17 }; return R[Row]; }
		case TEXT('N'): { static constexpr uint8 R[] = { 17, 25, 21, 19, 17, 17, 17 }; return R[Row]; }
		case TEXT('O'): { static constexpr uint8 R[] = { 14, 17, 17, 17, 17, 17, 14 }; return R[Row]; }
		case TEXT('P'): { static constexpr uint8 R[] = { 30, 17, 17, 30, 16, 16, 16 }; return R[Row]; }
		case TEXT('Q'): { static constexpr uint8 R[] = { 14, 17, 17, 17, 21, 18, 13 }; return R[Row]; }
		case TEXT('R'): { static constexpr uint8 R[] = { 30, 17, 17, 30, 20, 18, 17 }; return R[Row]; }
		case TEXT('S'): { static constexpr uint8 R[] = { 15, 16, 16, 14, 1, 1, 30 }; return R[Row]; }
		case TEXT('T'): { static constexpr uint8 R[] = { 31, 4, 4, 4, 4, 4, 4 }; return R[Row]; }
		case TEXT('U'): { static constexpr uint8 R[] = { 17, 17, 17, 17, 17, 17, 14 }; return R[Row]; }
		case TEXT('V'): { static constexpr uint8 R[] = { 17, 17, 17, 17, 17, 10, 4 }; return R[Row]; }
		case TEXT('W'): { static constexpr uint8 R[] = { 17, 17, 17, 21, 21, 21, 10 }; return R[Row]; }
		case TEXT('X'): { static constexpr uint8 R[] = { 17, 17, 10, 4, 10, 17, 17 }; return R[Row]; }
		case TEXT('Y'): { static constexpr uint8 R[] = { 17, 17, 10, 4, 4, 4, 4 }; return R[Row]; }
		case TEXT('Z'): { static constexpr uint8 R[] = { 31, 1, 2, 4, 8, 16, 31 }; return R[Row]; }
		case TEXT('0'): { static constexpr uint8 R[] = { 14, 17, 19, 21, 25, 17, 14 }; return R[Row]; }
		case TEXT('1'): { static constexpr uint8 R[] = { 4, 12, 4, 4, 4, 4, 14 }; return R[Row]; }
		case TEXT('2'): { static constexpr uint8 R[] = { 14, 17, 1, 2, 4, 8, 31 }; return R[Row]; }
		case TEXT('3'): { static constexpr uint8 R[] = { 30, 1, 1, 14, 1, 1, 30 }; return R[Row]; }
		case TEXT('4'): { static constexpr uint8 R[] = { 2, 6, 10, 18, 31, 2, 2 }; return R[Row]; }
		case TEXT('5'): { static constexpr uint8 R[] = { 31, 16, 16, 30, 1, 1, 30 }; return R[Row]; }
		case TEXT('6'): { static constexpr uint8 R[] = { 14, 16, 16, 30, 17, 17, 14 }; return R[Row]; }
		case TEXT('7'): { static constexpr uint8 R[] = { 31, 1, 2, 4, 8, 8, 8 }; return R[Row]; }
		case TEXT('8'): { static constexpr uint8 R[] = { 14, 17, 17, 14, 17, 17, 14 }; return R[Row]; }
		case TEXT('9'): { static constexpr uint8 R[] = { 14, 17, 17, 15, 1, 1, 14 }; return R[Row]; }
		case TEXT('-'): { static constexpr uint8 R[] = { 0, 0, 0, 31, 0, 0, 0 }; return R[Row]; }
		case TEXT('/'): { static constexpr uint8 R[] = { 1, 1, 2, 4, 8, 16, 16 }; return R[Row]; }
		default:
			return 0;
		}
	}

	void SetPixelSafe(TArray<FColor>& Pixels, const int32 Width, const int32 Height, const int32 X, const int32 Y, const FColor& Color)
	{
		if (X >= 0 && X < Width && Y >= 0 && Y < Height)
		{
			Pixels[Y * Width + X] = Color;
		}
	}

	void DrawTextSmall(TArray<FColor>& Pixels, const int32 Width, const int32 Height, const int32 X, const int32 Y, const FString& Text, const FColor& Color, const int32 Scale = 2)
	{
		int32 CursorX = X;
		for (const TCHAR Ch : Text)
		{
			if (Ch == TEXT(' '))
			{
				CursorX += 4 * Scale;
				continue;
			}
			for (int32 Row = 0; Row < 7; ++Row)
			{
				const uint8 Bits = GlyphRow(Ch, Row);
				for (int32 Col = 0; Col < 5; ++Col)
				{
					if ((Bits & (1u << (4 - Col))) == 0)
					{
						continue;
					}
					for (int32 Sy = 0; Sy < Scale; ++Sy)
					{
						for (int32 Sx = 0; Sx < Scale; ++Sx)
						{
							SetPixelSafe(Pixels, Width, Height, CursorX + Col * Scale + Sx, Y + Row * Scale + Sy, Color);
						}
					}
				}
			}
			CursorX += 6 * Scale;
		}
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
				TEXT("IIIE612CoherenceDiagnosis")));
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
				TEXT("phase-iii-slice-iiie6-12-coherence-diagnosis.md")));
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
		case ECarrierLabVisualizationLayer::PlateId:
			return TEXT("PlateId");
		case ECarrierLabVisualizationLayer::StatePlateId:
			return TEXT("StatePlateId");
		case ECarrierLabVisualizationLayer::PlateProjectionMismatch:
			return TEXT("PlateProjectionMismatch");
		case ECarrierLabVisualizationLayer::ProjectionDiagnostics:
			return TEXT("ProjectionDiagnostics");
		case ECarrierLabVisualizationLayer::ContinentalFraction:
			return TEXT("ContinentalFraction");
		case ECarrierLabVisualizationLayer::MissMask:
			return TEXT("MissMask");
		case ECarrierLabVisualizationLayer::OverlapMask:
			return TEXT("OverlapMask");
		case ECarrierLabVisualizationLayer::BoundaryMask:
			return TEXT("BoundaryMask");
		case ECarrierLabVisualizationLayer::PhaseIIIERemeshSummary:
			return TEXT("PhaseIIIERemeshSummary");
		case ECarrierLabVisualizationLayer::OceanicAgeHeatmap:
			return TEXT("OceanicAge");
		case ECarrierLabVisualizationLayer::ElevationHeatmap:
			return TEXT("Elevation");
		case ECarrierLabVisualizationLayer::BathymetricElevation:
			return TEXT("BathymetricElevation");
		default:
			return TEXT("Layer");
		}
	}

	FString LayerLabel(const ECarrierLabVisualizationLayer Layer)
	{
		switch (Layer)
		{
		case ECarrierLabVisualizationLayer::PlateId:
			return TEXT("RENDER PLATE");
		case ECarrierLabVisualizationLayer::StatePlateId:
			return TEXT("STATE PLATE");
		case ECarrierLabVisualizationLayer::PlateProjectionMismatch:
			return TEXT("PLATE DIFF");
		case ECarrierLabVisualizationLayer::ProjectionDiagnostics:
			return TEXT("PROJECTION DIAG");
		case ECarrierLabVisualizationLayer::ContinentalFraction:
			return TEXT("CONTINENTAL");
		case ECarrierLabVisualizationLayer::MissMask:
			return TEXT("MISS MASK");
		case ECarrierLabVisualizationLayer::OverlapMask:
			return TEXT("OVERLAP");
		case ECarrierLabVisualizationLayer::BoundaryMask:
			return TEXT("BOUNDARY");
		case ECarrierLabVisualizationLayer::PhaseIIIERemeshSummary:
			return TEXT("IIIE REMESH");
		case ECarrierLabVisualizationLayer::OceanicAgeHeatmap:
			return TEXT("OCEANIC AGE");
		case ECarrierLabVisualizationLayer::ElevationHeatmap:
			return TEXT("ELEVATION");
		case ECarrierLabVisualizationLayer::BathymetricElevation:
			return TEXT("BATHYMETRIC");
		default:
			return TEXT("LAYER");
		}
	}

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

	ACarrierLabVisualizationActor* SpawnEditorDefaultActor(UWorld& World)
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
		Image.Label = Prefix.ToUpper() + TEXT(" ") + LayerLabel(Layer);
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
		const int32 HeaderHeight = 32;
		const int32 SheetWidth = ContactThumbWidth * Columns;
		const int32 SheetHeight = (ContactThumbHeight + HeaderHeight) * Rows;
		TArray<FColor> SheetPixels;
		SheetPixels.Init(FColor(4, 7, 10, 255), SheetWidth * SheetHeight);

		for (int32 Index = 0; Index < Images.Num(); ++Index)
		{
			const int32 Column = Index % Columns;
			const int32 Row = Index / Columns;
			const int32 PanelX = Column * ContactThumbWidth;
			const int32 PanelY = Row * (ContactThumbHeight + HeaderHeight);
			for (int32 Y = 0; Y < HeaderHeight; ++Y)
			{
				for (int32 X = 0; X < ContactThumbWidth; ++X)
				{
					SheetPixels[(PanelY + Y) * SheetWidth + PanelX + X] = FColor(16, 21, 27, 255);
				}
			}
			DrawTextSmall(SheetPixels, SheetWidth, SheetHeight, PanelX + 10, PanelY + 9, Images[Index].Label, FColor(232, 238, 242, 255), 2);
			BlitThumbnail(
				Images[Index].Pixels,
				Images[Index].Width,
				Images[Index].Height,
				SheetPixels,
				SheetWidth,
				PanelX,
				PanelY + HeaderHeight);
		}

		OutPath = FPaths::Combine(OutputRoot, TEXT("ContactSheet.png"));
		return SavePng(OutPath, SheetPixels, SheetWidth, SheetHeight);
	}

	FString IntArrayJson(const TArray<int32>& Values, const int32 MaxCount)
	{
		TArray<FString> Parts;
		const int32 Count = FMath::Min(Values.Num(), MaxCount);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Parts.Add(FString::Printf(TEXT("%d"), Values[Index]));
		}
		return FString::Printf(TEXT("[%s]"), *FString::Join(Parts, TEXT(",")));
	}

	FString SnapshotJson(const FCarrierLabPhaseIIIE612SnapshotMetrics& Snapshot)
	{
		return FString::Printf(
			TEXT("{\"step\":%d,\"event_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"authoritative_caf\":%.9f,\"projected_caf\":%.9f,\"continental_samples\":%d,\"oceanic_samples\":%d,\"above_sea_level_samples\":%d,\"continental_above_sea_level_samples\":%d,\"oceanic_above_sea_level_samples\":%d,\"above_sea_level_fraction\":%.9f,\"continental_above_sea_level_fraction\":%.9f,\"oceanic_above_sea_level_fraction\":%.9f,\"min_elevation_km\":%.9f,\"max_elevation_km\":%.9f,\"mean_elevation_km\":%.9f,\"continental_min_elevation_km\":%.9f,\"continental_max_elevation_km\":%.9f,\"continental_mean_elevation_km\":%.9f,\"oceanic_min_elevation_km\":%.9f,\"oceanic_max_elevation_km\":%.9f,\"oceanic_mean_elevation_km\":%.9f,\"continental_components\":%d,\"largest_continental_component_size\":%d,\"largest_continental_component_area\":%.9f,\"state_plate_components\":%d,\"projected_plate_components\":%d,\"state_plate_salt_pepper\":%d,\"projected_plate_salt_pepper\":%d,\"continental_state_plate_salt_pepper\":%d,\"continental_projected_plate_salt_pepper\":%d,\"continental_material_salt_pepper\":%d,\"top_continental_component_sizes\":%s,\"projection_hash\":%s,\"state_hash\":%s,\"crust_hash\":%s}"),
			Snapshot.Step,
			Snapshot.EventCount,
			Snapshot.SampleCount,
			Snapshot.PlateCount,
			Snapshot.AuthoritativeCAF,
			Snapshot.ProjectedCAF,
			Snapshot.ContinentalSampleCount,
			Snapshot.OceanicSampleCount,
			Snapshot.AboveSeaLevelSampleCount,
			Snapshot.ContinentalAboveSeaLevelSampleCount,
			Snapshot.OceanicAboveSeaLevelSampleCount,
			Snapshot.AboveSeaLevelFraction,
			Snapshot.ContinentalAboveSeaLevelFraction,
			Snapshot.OceanicAboveSeaLevelFraction,
			Snapshot.MinElevationKm,
			Snapshot.MaxElevationKm,
			Snapshot.MeanElevationKm,
			Snapshot.ContinentalMinElevationKm,
			Snapshot.ContinentalMaxElevationKm,
			Snapshot.ContinentalMeanElevationKm,
			Snapshot.OceanicMinElevationKm,
			Snapshot.OceanicMaxElevationKm,
			Snapshot.OceanicMeanElevationKm,
			Snapshot.ContinentalComponentCount,
			Snapshot.LargestContinentalComponentSize,
			Snapshot.LargestContinentalComponentArea,
			Snapshot.StatePlateComponentCount,
			Snapshot.ProjectedPlateComponentCount,
			Snapshot.StatePlateSaltPepperSampleCount,
			Snapshot.ProjectedPlateSaltPepperSampleCount,
			Snapshot.ContinentalStatePlateSaltPepperSampleCount,
			Snapshot.ContinentalProjectedPlateSaltPepperSampleCount,
			Snapshot.ContinentalMaterialSaltPepperSampleCount,
			*IntArrayJson(Snapshot.ContinentalComponentSizes, 8),
			*JsonString(Snapshot.ProjectionHash),
			*JsonString(Snapshot.StateHash),
			*JsonString(Snapshot.CrustStateHash));
	}

	FString AttributionJson(const FCarrierLabPhaseIIIE612AttributionCounts& Attribution)
	{
		return FString::Printf(
			TEXT("{\"mixed_material_nearest\":%d,\"cross_or_third_nearest\":%d,\"distance_tie_fallback\":%d,\"generated_ocean\":%d,\"rifting_pending\":%d,\"majority_rebuild_context\":%d,\"triple_junction_context\":%d,\"shared_boundary\":%d,\"coalesced_duplicate\":%d,\"other\":%d}"),
			Attribution.MixedMaterialNearest,
			Attribution.CrossOrThirdNearest,
			Attribution.DistanceTieFallback,
			Attribution.GeneratedOcean,
			Attribution.RiftingPending,
			Attribution.MajorityRebuildContext,
			Attribution.TripleJunctionContext,
			Attribution.SharedBoundary,
			Attribution.CoalescedDuplicate,
			Attribution.Other);
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
		Parts.Add(FString::Printf(TEXT("{\"name\":\"ContactSheet\",\"label\":\"CONTACT SHEET\",\"path\":%s}"), *JsonString(ContactSheetPath)));
		return FString::Printf(TEXT("[%s]"), *FString::Join(Parts, TEXT(",")));
	}

	FString ImagePurpose(const FString& ImageName)
	{
		if (ImageName == TEXT("PreStatePlateId"))
		{
			return TEXT("Baseline authoritative plate ownership before the manual remesh.");
		}
		if (ImageName == TEXT("PreContinentalFraction"))
		{
			return TEXT("Baseline continental/oceanic material distribution before the remesh.");
		}
		if (ImageName == TEXT("PreBathymetricElevation"))
		{
			return TEXT("Baseline sea-level and terrain read before the remesh.");
		}
		if (ImageName == TEXT("PostStatePlateId"))
		{
			return TEXT("Authoritative plate ownership after remesh; this is the primary topology result.");
		}
		if (ImageName == TEXT("PostPlateProjectionMismatch"))
		{
			return TEXT("Render/state agreement check; green means the viewport plate stream matches authoritative state.");
		}
		if (ImageName == TEXT("PostContinentalFraction"))
		{
			return TEXT("Post-remesh material distribution; compare with the baseline material map.");
		}
		if (ImageName == TEXT("PostPhaseIIIERemeshSummary"))
		{
			return TEXT("Where remesh-generated/rifting records affected the surface.");
		}
		if (ImageName == TEXT("PostProjectionDiagnostics"))
		{
			return TEXT("Projection ray health in one view: red=miss, orange=overlap, white=plate boundary.");
		}
		if (ImageName == TEXT("PostBathymetricElevation"))
		{
			return TEXT("Human-readable terrain outcome after remesh, sourced from authoritative sample elevation.");
		}
		return TEXT("Diagnostic layer captured for visual inspection.");
	}

	FString AuditJson(
		const FCarrierLabPhaseIIIE612CoherenceDiagnosisAudit& Audit,
		const TArray<FDiagnosticImage>& Images,
		const FString& ContactSheetPath)
	{
		return FString::Printf(
			TEXT("{\"slice\":\"IIIE.6.12\",\"scenario\":\"editor_default_step16_manual_remesh\",\"ran\":%s,\"selection_ran\":%s,\"remesh_applied\":%s,\"verdict\":%s,\"verdict_reason\":%s,\"sample_count\":%d,\"plate_count\":%d,\"step_before\":%d,\"step_after\":%d,\"event_count_before\":%d,\"event_count_after\":%d,\"last_remesh_mode\":%s,\"state_plate_changed\":%d,\"state_plate_changed_fraction\":%.9f,\"continental_state_plate_changed\":%d,\"continental_state_plate_changed_fraction\":%.9f,\"projected_plate_changed\":%d,\"projected_plate_changed_fraction\":%.9f,\"continental_projected_plate_changed\":%d,\"continental_projected_plate_changed_fraction\":%.9f,\"material_class_changed\":%d,\"material_class_changed_fraction\":%.9f,\"material_class_changed_fraction_of_continental\":%.9f,\"global_authoritative_caf_delta\":%.9f,\"global_projected_caf_delta\":%.9f,\"max_per_plate_caf_delta\":%.9f,\"max_per_plate_caf_delta_plate\":%d,\"continental_largest_component_loss_fraction\":%.9f,\"state_plate_component_growth_fraction\":%.9f,\"projected_plate_component_growth_fraction\":%.9f,\"state_plate_salt_pepper_growth_fraction\":%.9f,\"projected_plate_salt_pepper_growth_fraction\":%.9f,\"above_sea_level_growth_fraction\":%.9f,\"continental_above_sea_level_growth_fraction\":%.9f,\"oceanic_above_sea_level_growth_fraction\":%.9f,\"last_generated_candidates\":%d,\"last_applied_generated\":%d,\"last_rifting_pending\":%d,\"last_material_preserved_records\":%d,\"last_mixed_material_nearest_material_preserved\":%d,\"last_plate_component_regularized_samples\":%d,\"last_nearest_hit\":%d,\"last_mixed_material_nearest\":%d,\"last_distance_tie_fallback\":%d,\"last_triple_junction_split\":%d,\"selection_hash\":%s,\"pre\":%s,\"post\":%s,\"attribution\":%s,\"images\":%s}"),
			*BoolText(Audit.bRan),
			*BoolText(Audit.bSelectionAuditRan),
			*BoolText(Audit.bRemeshApplied),
			*JsonString(Audit.Verdict),
			*JsonString(Audit.VerdictReason),
			Audit.SampleCount,
			Audit.PlateCount,
			Audit.StepBefore,
			Audit.StepAfter,
			Audit.EventCountBefore,
			Audit.EventCountAfter,
			*JsonString(Audit.LastRemeshMode),
			Audit.TotalStatePlateChangedSampleCount,
			Audit.StatePlateChangedFraction,
			Audit.ContinentalStatePlateChangedSampleCount,
			Audit.ContinentalStatePlateChangedFraction,
			Audit.TotalProjectedPlateChangedSampleCount,
			Audit.ProjectedPlateChangedFraction,
			Audit.ContinentalProjectedPlateChangedSampleCount,
			Audit.ContinentalProjectedPlateChangedFraction,
			Audit.MaterialClassChangedSampleCount,
			Audit.MaterialClassChangedFraction,
			Audit.MaterialClassChangedFractionOfContinental,
			Audit.GlobalAuthoritativeCAFDelta,
			Audit.GlobalProjectedCAFDelta,
			Audit.MaxPerPlateCAFDelta,
			Audit.MaxPerPlateCAFDeltaPlateId,
			Audit.ContinentalLargestComponentLossFraction,
			Audit.StatePlateComponentGrowthFraction,
			Audit.ProjectedPlateComponentGrowthFraction,
			Audit.StatePlateSaltPepperGrowthFraction,
			Audit.ProjectedPlateSaltPepperGrowthFraction,
			Audit.AboveSeaLevelGrowthFraction,
			Audit.ContinentalAboveSeaLevelGrowthFraction,
			Audit.OceanicAboveSeaLevelGrowthFraction,
			Audit.PhaseIIIELastGeneratedCandidateCount,
			Audit.PhaseIIIELastAppliedGeneratedCount,
			Audit.PhaseIIIELastRiftingPendingCount,
			Audit.PhaseIIIELastMaterialPreservedRecordCount,
			Audit.PhaseIIIELastMixedMaterialNearestMaterialPreservedCount,
			Audit.PhaseIIIELastPlateComponentRegularizedSampleCount,
			Audit.PhaseIIIELastNearestHitTieBreakCount,
			Audit.PhaseIIIELastNearestHitMixedMaterialCount,
			Audit.PhaseIIIELastDistanceTieFallbackCount,
			Audit.PhaseIIIELastTripleJunctionSplitCount,
			*JsonString(Audit.SelectionAudit.SelectionHash),
			*SnapshotJson(Audit.Pre),
			*SnapshotJson(Audit.Post),
			*AttributionJson(Audit.Attribution),
			*ImagesJson(Images, ContactSheetPath));
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FString& MetricsPath,
		const FString& ContactSheetPath,
		const FCarrierLabPhaseIIIE612CoherenceDiagnosisAudit& Audit,
		const TArray<FDiagnosticImage>& Images)
	{
		FString Report;
		Report += TEXT("# Phase IIIE.6.12 Coherence Diagnosis\n\n");
		Report += FString::Printf(TEXT("Verdict: **%s**. %s\n\n"), *Audit.Verdict, *Audit.VerdictReason);
		Report += TEXT("This is a diagnostic-only slice for the editor-default manual remesh at step 16. It does not add a resolver, change remesh policy, or treat global samples as tectonic authority.\n\n");

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
		Report += TEXT("| mixed-material nearest hit | on |\n");
		Report += TEXT("| non-separating veto | off |\n");
		Report += FString::Printf(TEXT("| manual remesh step | %d |\n\n"), ManualRemeshStep);

		Report += TEXT("## Numbers First\n\n");
		Report += TEXT("| Metric | Pre | Post | Delta |\n");
		Report += TEXT("|---|---:|---:|---:|\n");
		Report += FString::Printf(TEXT("| authoritative CAF | %.9f | %.9f | %.9f |\n"), Audit.Pre.AuthoritativeCAF, Audit.Post.AuthoritativeCAF, Audit.GlobalAuthoritativeCAFDelta);
		Report += FString::Printf(TEXT("| projected CAF | %.9f | %.9f | %.9f |\n"), Audit.Pre.ProjectedCAF, Audit.Post.ProjectedCAF, Audit.GlobalProjectedCAFDelta);
		Report += FString::Printf(TEXT("| continental components | %d | %d | %d |\n"), Audit.Pre.ContinentalComponentCount, Audit.Post.ContinentalComponentCount, Audit.Post.ContinentalComponentCount - Audit.Pre.ContinentalComponentCount);
		Report += FString::Printf(TEXT("| largest continental component size | %d | %d | %.9f loss |\n"), Audit.Pre.LargestContinentalComponentSize, Audit.Post.LargestContinentalComponentSize, Audit.ContinentalLargestComponentLossFraction);
		Report += FString::Printf(TEXT("| state plate components | %d | %d | %.9f growth |\n"), Audit.Pre.StatePlateComponentCount, Audit.Post.StatePlateComponentCount, Audit.StatePlateComponentGrowthFraction);
		Report += FString::Printf(TEXT("| projected plate components | %d | %d | %.9f growth |\n"), Audit.Pre.ProjectedPlateComponentCount, Audit.Post.ProjectedPlateComponentCount, Audit.ProjectedPlateComponentGrowthFraction);
		Report += FString::Printf(TEXT("| state plate salt-pepper | %d | %d | %.9f growth |\n"), Audit.Pre.StatePlateSaltPepperSampleCount, Audit.Post.StatePlateSaltPepperSampleCount, Audit.StatePlateSaltPepperGrowthFraction);
		Report += FString::Printf(TEXT("| projected plate salt-pepper | %d | %d | %.9f growth |\n"), Audit.Pre.ProjectedPlateSaltPepperSampleCount, Audit.Post.ProjectedPlateSaltPepperSampleCount, Audit.ProjectedPlateSaltPepperGrowthFraction);
		Report += FString::Printf(TEXT("| material salt-pepper | %d | %d | %d |\n"), Audit.Pre.ContinentalMaterialSaltPepperSampleCount, Audit.Post.ContinentalMaterialSaltPepperSampleCount, Audit.Post.ContinentalMaterialSaltPepperSampleCount - Audit.Pre.ContinentalMaterialSaltPepperSampleCount);
		Report += FString::Printf(TEXT("| above-sea-level samples | %d | %d | %.9f growth |\n"), Audit.Pre.AboveSeaLevelSampleCount, Audit.Post.AboveSeaLevelSampleCount, Audit.AboveSeaLevelGrowthFraction);
		Report += FString::Printf(TEXT("| continental above-sea-level samples | %d | %d | %.9f growth |\n"), Audit.Pre.ContinentalAboveSeaLevelSampleCount, Audit.Post.ContinentalAboveSeaLevelSampleCount, Audit.ContinentalAboveSeaLevelGrowthFraction);
		Report += FString::Printf(TEXT("| oceanic above-sea-level samples | %d | %d | %.9f growth |\n"), Audit.Pre.OceanicAboveSeaLevelSampleCount, Audit.Post.OceanicAboveSeaLevelSampleCount, Audit.OceanicAboveSeaLevelGrowthFraction);
		Report += FString::Printf(TEXT("| elevation range km | %.6f..%.6f | %.6f..%.6f | mean %.6f -> %.6f |\n"), Audit.Pre.MinElevationKm, Audit.Pre.MaxElevationKm, Audit.Post.MinElevationKm, Audit.Post.MaxElevationKm, Audit.Pre.MeanElevationKm, Audit.Post.MeanElevationKm);
		Report += FString::Printf(TEXT("| oceanic elevation range km | %.6f..%.6f | %.6f..%.6f | mean %.6f -> %.6f |\n\n"), Audit.Pre.OceanicMinElevationKm, Audit.Pre.OceanicMaxElevationKm, Audit.Post.OceanicMinElevationKm, Audit.Post.OceanicMaxElevationKm, Audit.Pre.OceanicMeanElevationKm, Audit.Post.OceanicMeanElevationKm);

		Report += TEXT("## Changed Samples\n\n");
		Report += TEXT("| Metric | Count | Fraction |\n");
		Report += TEXT("|---|---:|---:|\n");
		Report += FString::Printf(TEXT("| state `PlateId` changed | %d | %.9f |\n"), Audit.TotalStatePlateChangedSampleCount, Audit.StatePlateChangedFraction);
		Report += FString::Printf(TEXT("| continental state `PlateId` changed | %d | %.9f |\n"), Audit.ContinentalStatePlateChangedSampleCount, Audit.ContinentalStatePlateChangedFraction);
		Report += FString::Printf(TEXT("| projected `PlateId` changed | %d | %.9f |\n"), Audit.TotalProjectedPlateChangedSampleCount, Audit.ProjectedPlateChangedFraction);
		Report += FString::Printf(TEXT("| continental projected `PlateId` changed | %d | %.9f |\n"), Audit.ContinentalProjectedPlateChangedSampleCount, Audit.ContinentalProjectedPlateChangedFraction);
		Report += FString::Printf(TEXT("| material class changed | %d | %.9f |\n"), Audit.MaterialClassChangedSampleCount, Audit.MaterialClassChangedFraction);
		Report += FString::Printf(TEXT("| material class changed / pre-continental | %d | %.9f |\n\n"), Audit.MaterialClassChangedSampleCount, Audit.MaterialClassChangedFractionOfContinental);

		Report += TEXT("## Attribution For State Plate Changes\n\n");
		Report += TEXT("| Source class | Count |\n");
		Report += TEXT("|---|---:|\n");
		Report += FString::Printf(TEXT("| mixed-material nearest | %d |\n"), Audit.Attribution.MixedMaterialNearest);
		Report += FString::Printf(TEXT("| cross/third nearest | %d |\n"), Audit.Attribution.CrossOrThirdNearest);
		Report += FString::Printf(TEXT("| distance-tie fallback | %d |\n"), Audit.Attribution.DistanceTieFallback);
		Report += FString::Printf(TEXT("| generated ocean | %d |\n"), Audit.Attribution.GeneratedOcean);
		Report += FString::Printf(TEXT("| rifting-pending | %d |\n"), Audit.Attribution.RiftingPending);
		Report += FString::Printf(TEXT("| majority rebuild context | %d |\n"), Audit.Attribution.MajorityRebuildContext);
		Report += FString::Printf(TEXT("| triple-junction context | %d |\n"), Audit.Attribution.TripleJunctionContext);
		Report += FString::Printf(TEXT("| shared boundary | %d |\n"), Audit.Attribution.SharedBoundary);
		Report += FString::Printf(TEXT("| coalesced duplicate | %d |\n"), Audit.Attribution.CoalescedDuplicate);
		Report += FString::Printf(TEXT("| other | %d |\n\n"), Audit.Attribution.Other);

		Report += TEXT("## Remesh Counters\n\n");
		Report += TEXT("| Counter | Value |\n");
		Report += TEXT("|---|---:|\n");
		Report += FString::Printf(TEXT("| selection audit ran | %s |\n"), *BoolText(Audit.bSelectionAuditRan));
		Report += FString::Printf(TEXT("| remesh applied | %s |\n"), *BoolText(Audit.bRemeshApplied));
		Report += FString::Printf(TEXT("| last remesh mode | `%s` |\n"), *Audit.LastRemeshMode);
		Report += FString::Printf(TEXT("| generated candidates | %d |\n"), Audit.PhaseIIIELastGeneratedCandidateCount);
		Report += FString::Printf(TEXT("| applied generated | %d |\n"), Audit.PhaseIIIELastAppliedGeneratedCount);
		Report += FString::Printf(TEXT("| rifting pending | %d |\n"), Audit.PhaseIIIELastRiftingPendingCount);
		Report += FString::Printf(TEXT("| material-preserved resolved records | %d |\n"), Audit.PhaseIIIELastMaterialPreservedRecordCount);
		Report += FString::Printf(TEXT("| mixed-material nearest material-preserved | %d |\n"), Audit.PhaseIIIELastMixedMaterialNearestMaterialPreservedCount);
		Report += FString::Printf(TEXT("| plate-component regularized samples | %d |\n"), Audit.PhaseIIIELastPlateComponentRegularizedSampleCount);
		Report += FString::Printf(TEXT("| nearest hit | %d |\n"), Audit.PhaseIIIELastNearestHitTieBreakCount);
		Report += FString::Printf(TEXT("| mixed-material nearest | %d |\n"), Audit.PhaseIIIELastNearestHitMixedMaterialCount);
		Report += FString::Printf(TEXT("| distance-tie fallback | %d |\n"), Audit.PhaseIIIELastDistanceTieFallbackCount);
		Report += FString::Printf(TEXT("| triple-junction splits | %d |\n\n"), Audit.PhaseIIIELastTripleJunctionSplitCount);

		Report += TEXT("## Images\n\n");
		Report += FString::Printf(TEXT("Contact sheet: `%s`\n\n"), *ContactSheetPath);
		Report += TEXT("| Image | Hash | Path |\n");
		Report += TEXT("|---|---|---|\n");
		for (const FDiagnosticImage& Image : Images)
		{
			Report += FString::Printf(TEXT("| `%s` | `%s` | `%s` |\n"), *Image.Name, *Image.Hash, *Image.Path);
		}
		Report += TEXT("\n");
		Report += TEXT("## Map Purposes\n\n");
		Report += TEXT("| Image | Why it is included |\n");
		Report += TEXT("|---|---|\n");
		for (const FDiagnosticImage& Image : Images)
		{
			Report += FString::Printf(TEXT("| `%s` | %s |\n"), *Image.Name, *ImagePurpose(Image.Name));
		}
		Report += TEXT("\n");

		Report += TEXT("## Interpretation Boundaries\n\n");
		Report += TEXT("- `FAIL_MATERIAL_COHERENCE` means the continental/material carrier changed beyond the diagnostic threshold, regardless of how the `PlateId` map looks.\n");
		Report += TEXT("- Material-preserved resolved records keep pre-remesh crustal fields when a non-divergent resolved hit would otherwise flip material class; they do not restore old `PlateId` ownership.\n");
		Report += TEXT("- Plate-component regularization absorbs detached post-selection ownership fragments into adjacent post-selection plates; it does not query or restore pre-remesh owners.\n");
		Report += TEXT("- `StatePlateId` maps render authoritative `State.Samples[*].PlateId`; `PlateProjectionMismatch` is green for agreement, orange for disagreement, and red for missing IDs.\n");
		Report += TEXT("- `ProjectionDiagnostics` consolidates the lower-level miss, overlap, and boundary maps so the contact sheet stays focused.\n");
		Report += TEXT("- Projection ray casts still drive diagnostic masks and projected continental fraction; viewport `PlateId` and bathymetric elevation should not perform a second ownership/elevation solve after remesh.\n");
		Report += TEXT("- `PASS_UNSAFE_REMESH_HELD` means the default live-remesh path detected that threshold before mutation and refused to apply the remesh.\n");
		Report += TEXT("- `FAIL_PLATE_COHERENCE` means material survived, but authoritative state `PlateId` connectivity/salt-pepper degraded beyond threshold.\n");
		Report += TEXT("- `FAIL_ELEVATION_COHERENCE` means plate/material coherence held but oceanic above-sea-level samples grew beyond the diagnostic threshold.\n");
		Report += TEXT("- `PASS_VISUAL_ARTIFACT_LIKELY` is only allowed when material and authoritative state plate metrics hold while projected `PlateId` degrades.\n");
		Report += TEXT("- Attribution counts are overlapping diagnostic tags, not an exclusive partition.\n\n");
		Report += FString::Printf(TEXT("Metrics: `%s`.\nOutput root: `%s`.\n"), *MetricsPath, *OutputRoot);
		return Report;
	}
}

UCarrierLabPhaseIIIE612CoherenceDiagnosisCommandlet::UCarrierLabPhaseIIIE612CoherenceDiagnosisCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIE612CoherenceDiagnosisCommandlet::Main(const FString& Params)
{
	const double StartSeconds = FPlatformTime::Seconds();
	const FString OutputRoot = GetOutputRoot(Params);
	const FString ReportPath = GetReportPath(Params);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("phase-iii-slice-iiie6-12-coherence-diagnosis.jsonl"));
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
		UE_LOG(LogTemp, Error, TEXT("CarrierLab IIIE.6.12 diagnosis failed: no commandlet world."));
		return 1;
	}

	ACarrierLabVisualizationActor* Actor = SpawnEditorDefaultActor(*World);
	if (Actor == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab IIIE.6.12 diagnosis failed: actor spawn failed."));
		return 1;
	}
	if (!Actor->InitializeCarrier())
	{
		Actor->Destroy();
		UE_LOG(LogTemp, Error, TEXT("CarrierLab IIIE.6.12 diagnosis failed: carrier initialization failed."));
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

	TArray<ECarrierLabVisualizationLayer> PostLayers;
	PostLayers.Add(ECarrierLabVisualizationLayer::StatePlateId);
	PostLayers.Add(ECarrierLabVisualizationLayer::PlateProjectionMismatch);
	PostLayers.Add(ECarrierLabVisualizationLayer::ContinentalFraction);
	PostLayers.Add(ECarrierLabVisualizationLayer::PhaseIIIERemeshSummary);
	PostLayers.Add(ECarrierLabVisualizationLayer::ProjectionDiagnostics);
	PostLayers.Add(ECarrierLabVisualizationLayer::BathymetricElevation);

	TArray<FDiagnosticImage> Images;
	bool bImagesOk = true;
	for (const ECarrierLabVisualizationLayer Layer : PreLayers)
	{
		bImagesOk = CaptureLayer(OutputRoot, *Actor, Layer, TEXT("Pre"), Images) && bImagesOk;
	}

	FCarrierLabPhaseIIIE612CoherenceDiagnosisAudit Audit;
	const bool bAuditOk = Actor->RunPhaseIIIE612CoherenceDiagnosisAudit(Audit);

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

	const bool bCommandletOk = bAuditOk && Audit.bRan && bImagesOk && bContactSheetOk && bMetricsOk && bReportOk;
	UE_LOG(
		LogTemp,
		Display,
		TEXT("CarrierLab IIIE.6.12 coherence diagnosis %s in %.3f s. Verdict=%s Metrics=%s Report=%s ContactSheet=%s"),
		bCommandletOk ? TEXT("completed") : TEXT("failed"),
		FPlatformTime::Seconds() - StartSeconds,
		*Audit.Verdict,
		*MetricsPath,
		*ReportPath,
		*ContactSheetPath);
	return bCommandletOk ? 0 : 1;
}
