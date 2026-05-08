// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIEVisualValidationCommandlet.h"

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
	constexpr int32 VisualSeed = 42;
	constexpr int32 ContactThumbWidth = 512;
	constexpr int32 ContactThumbHeight = 256;
	constexpr int32 CardWidth = 1024;
	constexpr int32 CardHeight = 512;
	constexpr double ContinentalOverwriteThreshold = 1.0e-4;

	void HashMix(uint64& Hash, const uint64 Value)
	{
		Hash ^= Value;
		Hash *= 1099511628211ull;
	}

	void HashMixString(uint64& Hash, const FString& Value)
	{
		for (const TCHAR Ch : Value)
		{
			HashMix(Hash, static_cast<uint64>(Ch));
		}
		HashMix(Hash, 0xffull);
	}

	FString HashToString(const uint64 Hash)
	{
		return FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Hash));
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

	FString PassFail(const bool bPass)
	{
		return bPass ? TEXT("pass") : TEXT("fail");
	}

	FString PassHoldFail(const bool bPass, const bool bHold)
	{
		return bHold ? TEXT("hold") : PassFail(bPass);
	}

	FVector3d UnitFromLonLat(const double LonDeg, const double LatDeg)
	{
		const double Lon = FMath::DegreesToRadians(LonDeg);
		const double Lat = FMath::DegreesToRadians(LatDeg);
		const double CosLat = FMath::Cos(Lat);
		return FVector3d(CosLat * FMath::Cos(Lon), CosLat * FMath::Sin(Lon), FMath::Sin(Lat));
	}

	FString SelectionClassName(const ECarrierLabPhaseIIIE3SelectionClass SelectionClass)
	{
		switch (SelectionClass)
		{
		case ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap:
			return TEXT("no-hit divergent gap");
		case ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit:
			return TEXT("resolved single hit");
		case ECarrierLabPhaseIIIE3SelectionClass::DivergentGapAfterFiltering:
			return TEXT("divergent gap after filtering");
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit:
			return TEXT("unresolved same-material multi-hit");
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedMixedMaterialMultiHit:
			return TEXT("unresolved mixed-material multi-hit");
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedThirdPlateMultiHit:
			return TEXT("unresolved third-plate multi-hit");
		default:
			return TEXT("unknown");
		}
	}

	FString LayerName(const ECarrierLabVisualizationLayer Layer)
	{
		switch (Layer)
		{
		case ECarrierLabVisualizationLayer::PlateId:
			return TEXT("PlateId");
		case ECarrierLabVisualizationLayer::ContinentalFraction:
			return TEXT("CrustType");
		case ECarrierLabVisualizationLayer::ElevationHeatmap:
			return TEXT("Elevation");
		case ECarrierLabVisualizationLayer::OceanicAgeHeatmap:
			return TEXT("OceanicAge");
		case ECarrierLabVisualizationLayer::RidgeDirection:
			return TEXT("RidgeDirection");
		case ECarrierLabVisualizationLayer::PhaseIIIERemeshSummary:
			return TEXT("PhaseIIIERemesh");
		default:
			return TEXT("Layer");
		}
	}

	FString LayerLabel(const ECarrierLabVisualizationLayer Layer)
	{
		switch (Layer)
		{
		case ECarrierLabVisualizationLayer::PlateId:
			return TEXT("PLATE ID");
		case ECarrierLabVisualizationLayer::ContinentalFraction:
			return TEXT("CRUST TYPE");
		case ECarrierLabVisualizationLayer::ElevationHeatmap:
			return TEXT("ELEVATION");
		case ECarrierLabVisualizationLayer::OceanicAgeHeatmap:
			return TEXT("OCEANIC AGE");
		case ECarrierLabVisualizationLayer::RidgeDirection:
			return TEXT("RIDGE DIRECTION");
		case ECarrierLabVisualizationLayer::PhaseIIIERemeshSummary:
			return TEXT("IIIE REMESH");
		default:
			return TEXT("LAYER");
		}
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

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			OutputRoot = FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("CarrierLab"),
				TEXT("PhaseIII"),
				TEXT("IIIEVisualValidation"),
				FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ")));
		}
		else if (FPaths::IsRelative(OutputRoot))
		{
			OutputRoot = FPaths::Combine(FPaths::ProjectDir(), OutputRoot);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	FString GetReportPath(const FString& Params)
	{
		FString ReportPath;
		if (!FParse::Value(*Params, TEXT("Report="), ReportPath))
		{
			ReportPath = FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("docs"),
				TEXT("checkpoints"),
				TEXT("phase-iii-iiie-visual-validation.md"));
		}
		else if (FPaths::IsRelative(ReportPath))
		{
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), ReportPath);
		}
		return FPaths::ConvertRelativePathToFull(ReportPath);
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

	void SetPixelSafe(TArray<FColor>& Pixels, const int32 Width, const int32 Height, const int32 X, const int32 Y, const FColor& Color)
	{
		if (X >= 0 && X < Width && Y >= 0 && Y < Height)
		{
			Pixels[Y * Width + X] = Color;
		}
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

	void BuildCard(const TArray<FString>& Lines, TArray<FColor>& OutPixels, int32& OutWidth, int32& OutHeight)
	{
		OutWidth = CardWidth;
		OutHeight = CardHeight;
		OutPixels.Init(FColor(6, 10, 14, 255), OutWidth * OutHeight);
		for (int32 Y = 0; Y < 72; ++Y)
		{
			for (int32 X = 0; X < OutWidth; ++X)
			{
				OutPixels[Y * OutWidth + X] = FColor(18, 25, 34, 255);
			}
		}
		const FColor Primary(232, 238, 242, 255);
		const FColor Secondary(160, 178, 190, 255);
		for (int32 Index = 0; Index < Lines.Num(); ++Index)
		{
			DrawTextSmall(OutPixels, OutWidth, OutHeight, 32, 24 + Index * 42, Lines[Index], Index == 0 ? Primary : Secondary, Index == 0 ? 3 : 2);
		}
	}

	struct FVisualImage
	{
		FString Name;
		FString Label;
		FString Path;
		FString Hash;
		int32 NonBackgroundPixelCount = 0;
		int32 Width = 0;
		int32 Height = 0;
		TArray<FColor> Pixels;
	};

	struct FVisualScenario
	{
		FString Name;
		FString Description;
		bool bPreExistingContinental = false;
		bool bUseFilteredGapRoute = false;
		bool bExpectRiftingPending = false;
	};

	struct FVisualReplay
	{
		FString ScenarioName;
		int32 Replay = 0;
		bool bRan = false;
		bool bPass = false;
		bool bHold = false;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIE3SelectionRecord SelectionRecord;
		FCarrierLabPhaseIIIE4OceanicGenerationRecord OceanicRecord;
		FCarrierLabPhaseIIIE5TopologyRebuildAudit TopologyAudit;
		int32 GeneratedRecordCount = 0;
		int32 NewOceanicCreationCount = 0;
		int32 OverwrittenByRidgeGenerationCount = 0;
		int32 RiftingPendingCount = 0;
		int32 GeophysicsDerivedZGammaCount = 0;
		int32 PaperFaithfulZGammaCount = 0;
		double OverwrittenContinentalFraction = 0.0;
		double RiftingPendingContinentalFraction = 0.0;
		bool bNoForbiddenPolicy = false;
		bool bMapsReadOnly = false;
		FString StateHashBeforeMaps;
		FString StateHashAfterMaps;
		FString CrustHashBeforeMaps;
		FString CrustHashAfterMaps;
		FString ContactSheetPath;
		TArray<FVisualImage> Images;
	};

	struct FVisualScenarioResult
	{
		FVisualScenario Scenario;
		FVisualReplay A;
		FVisualReplay B;
		bool bReplayHashesStable = false;
		bool bOverallPass = false;
	};

	TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe> MakeDivergentBoundaryEdges()
	{
		TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe> Edges;
		FCarrierLabPhaseIIIE2BoundaryEdgeProbe& Left = Edges.AddDefaulted_GetRef();
		Left.PlateId = 0;
		Left.StartUnitPosition = UnitFromLonLat(-20.0, 0.0);
		Left.EndUnitPosition = Left.StartUnitPosition;
		Left.StartElevation = -4.0;
		Left.EndElevation = -4.0;

		FCarrierLabPhaseIIIE2BoundaryEdgeProbe& Right = Edges.AddDefaulted_GetRef();
		Right.PlateId = 1;
		Right.StartUnitPosition = UnitFromLonLat(20.0, 0.0);
		Right.EndUnitPosition = Right.StartUnitPosition;
		Right.StartElevation = -2.0;
		Right.EndElevation = -2.0;
		return Edges;
	}

	TArray<FCarrierLabPhaseIIIE3CandidateProbe> MakeFilteredCandidateProbes()
	{
		TArray<FCarrierLabPhaseIIIE3CandidateProbe> Probes;
		FCarrierLabPhaseIIIE3CandidateProbe& SubductingProbe = Probes.AddDefaulted_GetRef();
		SubductingProbe.PlateId = 0;
		SubductingProbe.LocalTriangleId = 0;
		SubductingProbe.ContinentalFraction = 1.0;
		SubductingProbe.Elevation = 1.0;
		SubductingProbe.FilterReason = ECarrierLabPhaseIIIE3FilterReason::Subducting;

		FCarrierLabPhaseIIIE3CandidateProbe& CollisionProbe = Probes.AddDefaulted_GetRef();
		CollisionProbe.PlateId = 1;
		CollisionProbe.LocalTriangleId = 0;
		CollisionProbe.ContinentalFraction = 1.0;
		CollisionProbe.Elevation = 1.0;
		CollisionProbe.FilterReason = ECarrierLabPhaseIIIE3FilterReason::CollisionPending;
		return Probes;
	}

	ACarrierLabVisualizationActor* SpawnActor(UWorld& World, const FVisualScenario& Scenario)
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
		Actor->SampleCount = 96;
		Actor->PlateCount = 2;
		Actor->Seed = VisualSeed;
		Actor->ContinentalPlateFraction = Scenario.bPreExistingContinental ? 1.0 : 0.0;
		Actor->bEnableNaturalResamplingEvents = false;
		Actor->ConfigurePhaseIIICProcessLayer(false, false);
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	bool SaveImage(const FString& Dir, FVisualImage& Image)
	{
		Image.Path = FPaths::Combine(Dir, Image.Name + TEXT(".png"));
		Image.Hash = HashToString(HashPixels(Image.Pixels));
		Image.NonBackgroundPixelCount = CountNonBackgroundPixels(Image.Pixels);
		return SavePng(Image.Path, Image.Pixels, Image.Width, Image.Height);
	}

	bool CaptureActorLayer(
		const FString& Dir,
		const ACarrierLabVisualizationActor& Actor,
		const ECarrierLabVisualizationLayer Layer,
		const FString& Prefix,
		TArray<FVisualImage>& Images)
	{
		FVisualImage Image;
		Image.Name = Prefix + LayerName(Layer);
		Image.Label = Prefix.ToUpper() + LayerLabel(Layer);
		if (!Actor.BuildVisualizationLayerMap(Layer, Image.Pixels, Image.Width, Image.Height))
		{
			return false;
		}
		if (!SaveImage(Dir, Image))
		{
			return false;
		}
		Images.Add(MoveTemp(Image));
		return true;
	}

	bool AddCard(const FString& Dir, const FString& Name, const FString& Label, const TArray<FString>& Lines, TArray<FVisualImage>& Images)
	{
		FVisualImage Image;
		Image.Name = Name;
		Image.Label = Label;
		BuildCard(Lines, Image.Pixels, Image.Width, Image.Height);
		if (!SaveImage(Dir, Image))
		{
			return false;
		}
		Images.Add(MoveTemp(Image));
		return true;
	}

	bool WriteContactSheet(const FString& Dir, const TArray<FVisualImage>& Images, FString& OutPath)
	{
		const int32 Columns = 4;
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

		OutPath = FPaths::Combine(Dir, TEXT("ContactSheet.png"));
		return SavePng(OutPath, SheetPixels, SheetWidth, SheetHeight);
	}

	void CountLedgerLines(FVisualReplay& Replay, const bool bRiftingPendingRoute)
	{
		if (bRiftingPendingRoute)
		{
			Replay.RiftingPendingCount = 1;
			Replay.RiftingPendingContinentalFraction = 1.0;
			Replay.GeophysicsDerivedZGammaCount += Replay.OceanicRecord.bUsedZGammaGeophysicsDerivedProfile ? 1 : 0;
			Replay.PaperFaithfulZGammaCount += Replay.OceanicRecord.bPaperFaithfulZGammaProfile ? 1 : 0;
		}
		for (const FCarrierLabPhaseIIIE5RemeshVertexRecord& Record : Replay.TopologyAudit.VertexRecords)
		{
			if (!Record.bGeneratedOceanicCrust)
			{
				continue;
			}
			++Replay.GeneratedRecordCount;
			if (Record.bUsedZGammaGeophysicsDerivedProfile || Record.OceanicRecord.bUsedZGammaGeophysicsDerivedProfile)
			{
				++Replay.GeophysicsDerivedZGammaCount;
			}
			if (Record.bPaperFaithfulZGammaProfile || Record.OceanicRecord.bPaperFaithfulZGammaProfile)
			{
				++Replay.PaperFaithfulZGammaCount;
			}

			const double PreContinental = FMath::Clamp(Record.PreRemeshContinentalFraction, 0.0, 1.0);
			if (PreContinental > ContinentalOverwriteThreshold)
			{
				++Replay.OverwrittenByRidgeGenerationCount;
				Replay.OverwrittenContinentalFraction += PreContinental;
			}
			else
			{
				++Replay.NewOceanicCreationCount;
			}
		}
	}

	bool RunReplay(const FVisualScenario& Scenario, const int32 ReplayIndex, const FString& OutputRoot, FVisualReplay& OutReplay)
	{
		OutReplay = FVisualReplay();
		OutReplay.ScenarioName = Scenario.Name;
		OutReplay.Replay = ReplayIndex;
		const double StartSeconds = FPlatformTime::Seconds();

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}
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
		Actor->ConfigurePhaseIIMotionFixture(ECarrierLabPhaseIIMotionFixture::ForcedDivergence);
		for (int32 PlateId = 0; PlateId < 2; ++PlateId)
		{
			if (!Actor->SetPlateContinentalForTest(PlateId, Scenario.bPreExistingContinental))
			{
				Actor->Destroy();
				return false;
			}
		}

		const FString ReplayDir = FPaths::Combine(OutputRoot, Scenario.Name, FString::Printf(TEXT("replay_%d"), ReplayIndex));
		IFileManager::Get().MakeDirectory(*ReplayDir, true);
		if (!CaptureActorLayer(ReplayDir, *Actor, ECarrierLabVisualizationLayer::PlateId, TEXT("Pre"), OutReplay.Images) ||
			!CaptureActorLayer(ReplayDir, *Actor, ECarrierLabVisualizationLayer::ContinentalFraction, TEXT("Pre"), OutReplay.Images) ||
			!CaptureActorLayer(ReplayDir, *Actor, ECarrierLabVisualizationLayer::ElevationHeatmap, TEXT("Pre"), OutReplay.Images))
		{
			Actor->Destroy();
			return false;
		}

		const FVector3d SamplePosition = FVector3d::UnitX();
		const TArray<FCarrierLabPhaseIIIE3CandidateProbe> CandidateProbes =
			Scenario.bUseFilteredGapRoute ? MakeFilteredCandidateProbes() : TArray<FCarrierLabPhaseIIIE3CandidateProbe>();
		const bool bSelectionOk = Actor->QueryPhaseIIIE3FilteredRemeshSelectionForTest(
			SamplePosition,
			CandidateProbes,
			OutReplay.SelectionRecord);
		const bool bGenerationOk = bSelectionOk && Actor->QueryPhaseIIIE4DivergentOceanicFieldsForTest(
			SamplePosition,
			OutReplay.SelectionRecord.SelectionClass,
			MakeDivergentBoundaryEdges(),
			0.004,
			OutReplay.OceanicRecord);
		const bool bRiftingPendingRoute =
			Scenario.bExpectRiftingPending &&
			bGenerationOk &&
			OutReplay.OceanicRecord.bGeneratedOceanicCrust;

		if (!AddCard(
				ReplayDir,
				TEXT("SelectionAndGeneration"),
				TEXT("SELECTION"),
				{
					TEXT("IIIE SELECTION"),
					Scenario.bUseFilteredGapRoute ? TEXT("ROUTE FILTER EXHAUSTED") : TEXT("ROUTE NO HIT GAP"),
					FString::Printf(TEXT("RAW %d POST %d"), OutReplay.SelectionRecord.RawCandidateCount, OutReplay.SelectionRecord.PostFilterCandidateCount),
					FString::Printf(TEXT("GENERATED %d Q %d/%d"), OutReplay.OceanicRecord.bGeneratedOceanicCrust ? 1 : 0, OutReplay.OceanicRecord.Q1PlateId, OutReplay.OceanicRecord.Q2PlateId),
					FString::Printf(TEXT("ZGEO %d PAPER %d"), OutReplay.OceanicRecord.bUsedZGammaGeophysicsDerivedProfile ? 1 : 0, OutReplay.OceanicRecord.bPaperFaithfulZGammaProfile ? 1 : 0)
				},
				OutReplay.Images))
		{
			Actor->Destroy();
			return false;
		}

		FCarrierLabPhaseIIIE5RemeshInputFixture RebuildFixture;
		RebuildFixture.bInjectGeneratedOceanicRecord =
			OutReplay.OceanicRecord.bGeneratedOceanicCrust &&
			!bRiftingPendingRoute;
		RebuildFixture.OverrideTriangleId = 0;
		RebuildFixture.GeneratedOceanicRecord = OutReplay.OceanicRecord;
		const bool bRebuildOk = bGenerationOk && Actor->RunPhaseIIIE5TopologyRebuildFixtureForTest(RebuildFixture, OutReplay.TopologyAudit);
		if (bRebuildOk)
		{
			Actor->RefreshPhaseIIIMetricsForTest();
		}
		CountLedgerLines(OutReplay, bRiftingPendingRoute);

		if (!CaptureActorLayer(ReplayDir, *Actor, ECarrierLabVisualizationLayer::PlateId, TEXT("Post"), OutReplay.Images) ||
			!CaptureActorLayer(ReplayDir, *Actor, ECarrierLabVisualizationLayer::ContinentalFraction, TEXT("Post"), OutReplay.Images) ||
			!CaptureActorLayer(ReplayDir, *Actor, ECarrierLabVisualizationLayer::ElevationHeatmap, TEXT("Post"), OutReplay.Images) ||
			!CaptureActorLayer(ReplayDir, *Actor, ECarrierLabVisualizationLayer::OceanicAgeHeatmap, TEXT("Post"), OutReplay.Images) ||
			!CaptureActorLayer(ReplayDir, *Actor, ECarrierLabVisualizationLayer::RidgeDirection, TEXT("Post"), OutReplay.Images) ||
			!CaptureActorLayer(ReplayDir, *Actor, ECarrierLabVisualizationLayer::PhaseIIIERemeshSummary, TEXT("Post"), OutReplay.Images))
		{
			Actor->Destroy();
			return false;
		}

		if (!AddCard(
				ReplayDir,
				TEXT("LedgerAndHolds"),
				TEXT("LEDGER"),
				{
					TEXT("IIIE LEDGER"),
					FString::Printf(TEXT("NEW OCEAN %d"), OutReplay.NewOceanicCreationCount),
					FString::Printf(TEXT("RIFT PENDING %d"), OutReplay.RiftingPendingCount),
					FString::Printf(TEXT("OVERWRITTEN %d"), OutReplay.OverwrittenByRidgeGenerationCount),
					FString::Printf(TEXT("POLICY %d PRIOR %d PROJ %d"), OutReplay.TopologyAudit.PolicyWinnerCount, OutReplay.TopologyAudit.PriorOwnerFallbackCount, OutReplay.TopologyAudit.ProjectionOwnerFallbackCount),
					FString::Printf(TEXT("RESET %d/%d"), OutReplay.TopologyAudit.ResetAudit.ResetSerialBefore, OutReplay.TopologyAudit.ResetAudit.ResetSerialAfter)
				},
				OutReplay.Images))
		{
			Actor->Destroy();
			return false;
		}

		OutReplay.StateHashBeforeMaps = Actor->CurrentMetrics.StateHash;
		OutReplay.CrustHashBeforeMaps = Actor->CurrentMetrics.CrustStateHash;
		if (!WriteContactSheet(ReplayDir, OutReplay.Images, OutReplay.ContactSheetPath))
		{
			Actor->Destroy();
			return false;
		}
		OutReplay.StateHashAfterMaps = Actor->CurrentMetrics.StateHash;
		OutReplay.CrustHashAfterMaps = Actor->CurrentMetrics.CrustStateHash;
		OutReplay.bMapsReadOnly =
			OutReplay.StateHashBeforeMaps == OutReplay.StateHashAfterMaps &&
			OutReplay.CrustHashBeforeMaps == OutReplay.CrustHashAfterMaps;
		OutReplay.bNoForbiddenPolicy =
			OutReplay.SelectionRecord.bUsedPolicyWinner == false &&
			OutReplay.SelectionRecord.bUsedPriorOwnerFallback == false &&
			OutReplay.OceanicRecord.bUsedPolicyWinner == false &&
			OutReplay.OceanicRecord.bUsedPriorOwnerFallback == false &&
			OutReplay.TopologyAudit.PolicyWinnerCount == 0 &&
			OutReplay.TopologyAudit.PriorOwnerFallbackCount == 0 &&
			OutReplay.TopologyAudit.ProjectionOwnerFallbackCount == 0 &&
			OutReplay.TopologyAudit.UnresolvedMultiHitRoutedCount == 0;

		const bool bExpectedSelection = Scenario.bUseFilteredGapRoute
			? OutReplay.SelectionRecord.SelectionClass == ECarrierLabPhaseIIIE3SelectionClass::DivergentGapAfterFiltering
			: OutReplay.SelectionRecord.SelectionClass == ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
		const bool bExpectedLedger = Scenario.bExpectRiftingPending
			? (OutReplay.RiftingPendingCount > 0 &&
				OutReplay.NewOceanicCreationCount == 0 &&
				OutReplay.OverwrittenByRidgeGenerationCount == 0 &&
				OutReplay.GeneratedRecordCount == 0)
			: (OutReplay.NewOceanicCreationCount > 0 &&
				OutReplay.RiftingPendingCount == 0 &&
				OutReplay.OverwrittenByRidgeGenerationCount == 0);
		const int32 ExpectedProvenanceCount = OutReplay.GeneratedRecordCount + OutReplay.RiftingPendingCount;
		OutReplay.bPass =
			bSelectionOk &&
			bGenerationOk &&
			bRebuildOk &&
			bExpectedSelection &&
			OutReplay.OceanicRecord.bGeneratedOceanicCrust &&
			OutReplay.TopologyAudit.bRan &&
			OutReplay.TopologyAudit.bApplied &&
			ExpectedProvenanceCount > 0 &&
			bExpectedLedger &&
			OutReplay.GeophysicsDerivedZGammaCount == ExpectedProvenanceCount &&
			OutReplay.PaperFaithfulZGammaCount == 0 &&
			OutReplay.bNoForbiddenPolicy &&
			OutReplay.bMapsReadOnly;
		OutReplay.bRan = true;
		OutReplay.Seconds = FPlatformTime::Seconds() - StartSeconds;

		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return true;
	}

	bool ImageHashesMatch(const FVisualReplay& A, const FVisualReplay& B)
	{
		if (A.Images.Num() != B.Images.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Images.Num(); ++Index)
		{
			if (A.Images[Index].Name != B.Images[Index].Name || A.Images[Index].Hash != B.Images[Index].Hash)
			{
				return false;
			}
		}
		return true;
	}

	FString ReplayJson(const FVisualReplay& Replay)
	{
		TArray<FString> ImageJson;
		for (const FVisualImage& Image : Replay.Images)
		{
			ImageJson.Add(FString::Printf(
				TEXT("{\"name\":%s,\"hash\":%s,\"non_background_pixels\":%d,\"path\":%s}"),
				*JsonString(Image.Name),
				*JsonString(Image.Hash),
				Image.NonBackgroundPixelCount,
				*JsonString(Image.Path)));
		}
		return FString::Printf(
			TEXT("{\"scenario\":%s,\"replay\":%d,\"pass\":%s,\"hold\":%s,\"selection\":%s,\"raw_candidates\":%d,\"post_filter_candidates\":%d,\"applied_generated\":%d,\"new_oceanic\":%d,\"rifting_pending\":%d,\"overwritten_by_ridge\":%d,\"policy_prior_projection\":\"%d/%d/%d\",\"geophysics_zgamma\":%d,\"paper_zgamma\":%d,\"maps_read_only\":%s,\"topology_hash\":%s,\"contact_sheet\":%s,\"images\":[%s],\"seconds\":%.6f}"),
			*JsonString(Replay.ScenarioName),
			Replay.Replay,
			Replay.bPass ? TEXT("true") : TEXT("false"),
			Replay.bHold ? TEXT("true") : TEXT("false"),
			*JsonString(SelectionClassName(Replay.SelectionRecord.SelectionClass)),
			Replay.SelectionRecord.RawCandidateCount,
			Replay.SelectionRecord.PostFilterCandidateCount,
			Replay.GeneratedRecordCount,
			Replay.NewOceanicCreationCount,
			Replay.RiftingPendingCount,
			Replay.OverwrittenByRidgeGenerationCount,
			Replay.TopologyAudit.PolicyWinnerCount,
			Replay.TopologyAudit.PriorOwnerFallbackCount,
			Replay.TopologyAudit.ProjectionOwnerFallbackCount,
			Replay.GeophysicsDerivedZGammaCount,
			Replay.PaperFaithfulZGammaCount,
			Replay.bMapsReadOnly ? TEXT("true") : TEXT("false"),
			*JsonString(Replay.TopologyAudit.TopologyHash),
			*JsonString(Replay.ContactSheetPath),
			*FString::Join(ImageJson, TEXT(",")),
			Replay.Seconds);
	}

	FString BuildReport(const FString& OutputRoot, const FString& MetricsPath, const TArray<FVisualScenarioResult>& Results, const bool bOverallPass)
	{
		FString Report;
		Report += TEXT("# Phase IIIE Diagnostic Contact Sheets\n\n");
		Report += TEXT("Verdict: ");
		Report += bOverallPass ? TEXT("PASS / DIAGNOSTIC CONTACT SHEETS WRITTEN FOR PROMOTED IIIE.6 CHAIN") : TEXT("FAIL / HOLD DIAGNOSTIC CONTACT SHEETS");
		Report += TEXT(". This checkpoint exports dedicated diagnostic PNG contact sheets for the IIIE.3 -> IIIE.4 -> IIIE.5 -> IIIE.6 bounded chain as it exists today. It is a visual regression diagnostic for the promoted IIIE.6 cadence; it does not itself mutate the editor's live actor, relax unresolved multi-hit holds, or claim images are proof.\n\n");

		Report += TEXT("## Scope\n\n");
		Report += TEXT("- The export runs the same bounded IIIE.6 event cases: no-hit divergent ocean creation and filter-exhausted continental rifting-pending route.\n");
		Report += TEXT("- Each replay writes pre-state maps, a selection/generation card, post-rebuild maps, and a ledger card into one contact sheet.\n");
		Report += TEXT("- The commandlet is a read-only export diagnostic; live auto-remesh promotion is owned by the IIIE.6 commandlet/report, not by these images.\n");
		Report += TEXT("- The filter-exhausted continental case now records `rifting pending`: IIIE.4 provenance is visible, but no generated oceanic record is applied over continental material.\n\n");

		Report += TEXT("## Gates\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---:|---|\n");
		Report += FString::Printf(TEXT("| maps written | %s | output root `%s` |\n"), *PassFail(bOverallPass), *OutputRoot);
		for (const FVisualScenarioResult& Result : Results)
		{
			Report += FString::Printf(
				TEXT("| `%s` replay A/B | %s | replay A `%s`, replay B `%s`, map hashes stable `%s` |\n"),
				*Result.Scenario.Name,
				*PassFail(Result.bOverallPass),
				*PassHoldFail(Result.A.bPass, Result.A.bHold),
				*PassHoldFail(Result.B.bPass, Result.B.bHold),
				Result.bReplayHashesStable ? TEXT("yes") : TEXT("no"));
			Report += FString::Printf(
				TEXT("| `%s` forbidden policy counters | %s | policy/prior/projection `%d/%d/%d`, unresolved routed `%d` |\n"),
				*Result.Scenario.Name,
				*PassFail(Result.A.bNoForbiddenPolicy && Result.B.bNoForbiddenPolicy),
				Result.A.TopologyAudit.PolicyWinnerCount,
				Result.A.TopologyAudit.PriorOwnerFallbackCount,
				Result.A.TopologyAudit.ProjectionOwnerFallbackCount,
				Result.A.TopologyAudit.UnresolvedMultiHitRoutedCount);
		}
		Report += TEXT("\n");

		Report += TEXT("## Exported Contact Sheets\n\n");
		Report += TEXT("| Scenario | Replay | Selection | Applied generated | New ocean | Rifting pending | Overwritten | Topology hash | Contact sheet |\n");
		Report += TEXT("|---|---:|---|---:|---:|---:|---:|---|---|\n");
		for (const FVisualScenarioResult& Result : Results)
		{
			for (const FVisualReplay* Replay : { &Result.A, &Result.B })
			{
				Report += FString::Printf(
					TEXT("| `%s` | %d | `%s` | %d | %d | %d | %d | `%s` | `%s` |\n"),
					*Result.Scenario.Name,
					Replay->Replay,
					*SelectionClassName(Replay->SelectionRecord.SelectionClass),
					Replay->GeneratedRecordCount,
					Replay->NewOceanicCreationCount,
					Replay->RiftingPendingCount,
					Replay->OverwrittenByRidgeGenerationCount,
					*Replay->TopologyAudit.TopologyHash,
					*Replay->ContactSheetPath);
			}
		}
		Report += TEXT("\n");

		Report += TEXT("## Exported Layers\n\n");
		Report += TEXT("| Scenario | Replay | Layer | Hash | Non-background pixels | Path |\n");
		Report += TEXT("|---|---:|---|---|---:|---|\n");
		for (const FVisualScenarioResult& Result : Results)
		{
			for (const FVisualReplay* Replay : { &Result.A, &Result.B })
			{
				for (const FVisualImage& Image : Replay->Images)
				{
					Report += FString::Printf(
						TEXT("| `%s` | %d | `%s` | `%s` | %d | `%s` |\n"),
						*Result.Scenario.Name,
						Replay->Replay,
						*Image.Name,
						*Image.Hash,
						Image.NonBackgroundPixelCount,
						*Image.Path);
				}
			}
		}
		Report += TEXT("\n");

		Report += TEXT("## Interpretation\n\n");
		Report += TEXT("- `PrePlateId`, `PreCrustType`, and `PreElevation` show the bounded fixture before any IIIE mutation.\n");
		Report += TEXT("- `SelectionAndGeneration` is a card, not a map. It records the IIIE.3 route, raw/post-filter candidate counts, q1/q2 provenance, and zGamma authority flags.\n");
		Report += TEXT("- `PostPlateId`, `PostCrustType`, `PostElevation`, `PostOceanicAge`, `PostRidgeDirection`, and `PostPhaseIIIERemesh` show the state after the accepted IIIE.5 duplicate/re-index/re-compact helper.\n");
		Report += TEXT("- `LedgerAndHolds` is the audit card for the IIIE.6 ledger line. The continental case should show `RIFT PENDING 1` and `OVERWRITTEN 0`.\n");
		Report += TEXT("- The maps are human-spatial diagnostics only. The commandlet gates determinism, read-only export, no forbidden fallback counters, and expected ledger classification; it does not replace the numeric IIIE.2-6 commandlets.\n\n");

		Report += TEXT("## Next Required Work\n\n");
		Report += TEXT("1. Keep the contact-sheet export as a regression diagnostic after live promotion.\n");
		Report += TEXT("2. Use IIIE consolidation, not visual inspection alone, to disclose lab-policy choices and rerun the numeric gate chain.\n\n");
		Report += FString::Printf(TEXT("Metrics: `%s`.\n"), *MetricsPath);
		return Report;
	}
}

UCarrierLabPhaseIIIEVisualValidationCommandlet::UCarrierLabPhaseIIIEVisualValidationCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIEVisualValidationCommandlet::Main(const FString& Params)
{
	const FString OutputRoot = GetOutputRoot(Params);
	const FString ReportPath = GetReportPath(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);

	TArray<FVisualScenario> Scenarios;
	FVisualScenario NoHit;
	NoHit.Name = TEXT("no_hit_oceanic_creation");
	NoHit.Description = TEXT("No raw ray candidates route to IIIE.4 q1/q2/qGamma generation and IIIE.5 topology rebuild.");
	NoHit.bPreExistingContinental = false;
	NoHit.bUseFilteredGapRoute = false;
	Scenarios.Add(NoHit);

	FVisualScenario RiftingPending;
	RiftingPending.Name = TEXT("filter_exhausted_continental_rifting_pending");
	RiftingPending.Description = TEXT("Filtered process candidates compute divergent provenance, while pre-existing continental material routes to rifting-pending instead of oceanic overwrite.");
	RiftingPending.bPreExistingContinental = true;
	RiftingPending.bUseFilteredGapRoute = true;
	RiftingPending.bExpectRiftingPending = true;
	Scenarios.Add(RiftingPending);

	TArray<FVisualScenarioResult> Results;
	bool bOverallPass = true;
	for (const FVisualScenario& Scenario : Scenarios)
	{
		FVisualScenarioResult Result;
		Result.Scenario = Scenario;
		const bool bRanA = RunReplay(Scenario, 0, OutputRoot, Result.A);
		const bool bRanB = RunReplay(Scenario, 1, OutputRoot, Result.B);
		Result.bReplayHashesStable = bRanA && bRanB && ImageHashesMatch(Result.A, Result.B);
		Result.bOverallPass = bRanA && bRanB && Result.A.bPass && Result.B.bPass && Result.bReplayHashesStable;
		bOverallPass = bOverallPass && Result.bOverallPass;
		Results.Add(MoveTemp(Result));
	}

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FString MetricsJsonl;
	for (const FVisualScenarioResult& Result : Results)
	{
		MetricsJsonl += ReplayJson(Result.A);
		MetricsJsonl += LINE_TERMINATOR;
		MetricsJsonl += ReplayJson(Result.B);
		MetricsJsonl += LINE_TERMINATOR;
	}
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);

	const FString Report = BuildReport(OutputRoot, MetricsPath, Results, bOverallPass);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(
		LogTemp,
		Display,
		TEXT("CarrierLab Phase IIIE visual validation %s. Metrics: %s Report: %s"),
		bOverallPass ? TEXT("passed") : TEXT("failed"),
		*MetricsPath,
		*ReportPath);
	return bOverallPass ? 0 : 1;
}
