// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabStage0Commandlet.h"

#include "CarrierLabCarrier.h"
#include "HAL/PlatformMemory.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"

namespace
{
	constexpr int32 MapWidth = 2048;
	constexpr int32 MapHeight = 1024;
	constexpr int32 ContactThumbWidth = 512;
	constexpr int32 ContactThumbHeight = 256;

	FString JsonString(const FString& Value)
	{
		FString Escaped;
		Escaped.Reserve(Value.Len() + 2);
		for (const TCHAR Ch : Value)
		{
			switch (Ch)
			{
			case TEXT('\\'):
				Escaped += TEXT("\\\\");
				break;
			case TEXT('"'):
				Escaped += TEXT("\\\"");
				break;
			case TEXT('\n'):
				Escaped += TEXT("\\n");
				break;
			case TEXT('\r'):
				Escaped += TEXT("\\r");
				break;
			case TEXT('\t'):
				Escaped += TEXT("\\t");
				break;
			default:
				Escaped.AppendChar(Ch);
				break;
			}
		}
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}

	TArray<int32> ParseResolutions(const FString& Params)
	{
		auto ExtractValue = [&Params](const FString& Key, FString& OutValue)
		{
			const FString Pattern = FString::Printf(TEXT("-%s="), *Key);
			int32 Start = Params.Find(Pattern, ESearchCase::IgnoreCase);
			int32 Offset = Pattern.Len();
			if (Start == INDEX_NONE)
			{
				const FString BarePattern = FString::Printf(TEXT("%s="), *Key);
				Start = Params.Find(BarePattern, ESearchCase::IgnoreCase);
				Offset = BarePattern.Len();
			}
			if (Start == INDEX_NONE)
			{
				return false;
			}

			Start += Offset;
			if (Start >= Params.Len())
			{
				OutValue.Reset();
				return true;
			}

			TCHAR Quote = 0;
			if (Params[Start] == TEXT('"') || Params[Start] == TEXT('\''))
			{
				Quote = Params[Start];
				++Start;
			}

			int32 End = Start;
			while (End < Params.Len())
			{
				const TCHAR Ch = Params[End];
				if ((Quote != 0 && Ch == Quote) || (Quote == 0 && FChar::IsWhitespace(Ch)))
				{
					break;
				}
				++End;
			}

			OutValue = Params.Mid(Start, End - Start);
			return true;
		};

		FString ResolutionParam;
		if (!ExtractValue(TEXT("Resolutions"), ResolutionParam))
		{
			ExtractValue(TEXT("Stage0Resolutions"), ResolutionParam);
		}

		TArray<int32> Resolutions;
		if (ResolutionParam.IsEmpty())
		{
			Resolutions = {60000, 100000, 250000, 500000};
			return Resolutions;
		}

		TArray<FString> Parts;
		ResolutionParam.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& Part : Parts)
		{
			const int32 Value = FCString::Atoi(*Part);
			if (Value > 0)
			{
				Resolutions.Add(Value);
			}
		}
		return Resolutions;
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("Stage0"), Stamp);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	FColor PlateColor(const int32 PlateId)
	{
		uint32 X = static_cast<uint32>(PlateId + 1) * 2654435761u;
		X ^= X >> 16;
		const uint8 R = static_cast<uint8>(80 + (X & 127));
		const uint8 G = static_cast<uint8>(80 + ((X >> 8) & 127));
		const uint8 B = static_cast<uint8>(80 + ((X >> 16) & 127));
		return FColor(R, G, B, 255);
	}

	FColor MissColor(const uint8 MissClass)
	{
		switch (static_cast<CarrierLab::EStage0MissClass>(MissClass))
		{
		case CarrierLab::EStage0MissClass::NumericMiss:
			return FColor(255, 40, 40, 255);
		case CarrierLab::EStage0MissClass::TopologyHole:
			return FColor(255, 220, 20, 255);
		case CarrierLab::EStage0MissClass::OutOfDomain:
			return FColor(255, 0, 255, 255);
		default:
			return FColor(5, 8, 12, 255);
		}
	}

	FColor OverlapColor(const uint8 OverlapClass)
	{
		switch (static_cast<CarrierLab::EStage0OverlapClass>(OverlapClass))
		{
		case CarrierLab::EStage0OverlapClass::BoundaryDegenerate:
			return FColor(30, 220, 255, 255);
		case CarrierLab::EStage0OverlapClass::NonDegenerate:
			return FColor(255, 35, 35, 255);
		case CarrierLab::EStage0OverlapClass::ThirdPlateBoundary:
			return FColor(255, 155, 20, 255);
		case CarrierLab::EStage0OverlapClass::ThirdPlateNonDegenerate:
			return FColor(255, 0, 220, 255);
		default:
			return FColor(5, 8, 12, 255);
		}
	}

	FIntPoint SamplePixel(const FVector3d& UnitPosition)
	{
		const double Longitude = FMath::Atan2(UnitPosition.Y, UnitPosition.X);
		const double Latitude = FMath::Asin(FMath::Clamp(UnitPosition.Z, -1.0, 1.0));
		const int32 X = FMath::Clamp(
			static_cast<int32>(FMath::FloorToDouble(((Longitude + UE_DOUBLE_PI) / (2.0 * UE_DOUBLE_PI)) * MapWidth)),
			0,
			MapWidth - 1);
		const int32 Y = FMath::Clamp(
			static_cast<int32>(FMath::FloorToDouble(((UE_DOUBLE_PI * 0.5 - Latitude) / UE_DOUBLE_PI) * MapHeight)),
			0,
			MapHeight - 1);
		return FIntPoint(X, Y);
	}

	void PaintSample(TArray<FColor>& Pixels, const FIntPoint Pixel, const FColor Color, const int32 Radius)
	{
		for (int32 DY = -Radius; DY <= Radius; ++DY)
		{
			const int32 Y = Pixel.Y + DY;
			if (Y < 0 || Y >= MapHeight)
			{
				continue;
			}
			for (int32 DX = -Radius; DX <= Radius; ++DX)
			{
				const int32 X = Pixel.X + DX;
				if (X < 0 || X >= MapWidth)
				{
					continue;
				}
				Pixels[Y * MapWidth + X] = Color;
			}
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

	void MakeMap(
		const CarrierLab::FCarrierState& State,
		const TFunctionRef<FColor(int32)> ColorFunc,
		TArray<FColor>& OutPixels)
	{
		OutPixels.Init(FColor(5, 8, 12, 255), MapWidth * MapHeight);
		const int32 Radius = State.Samples.Num() <= 100000 ? 1 : 0;
		for (const CarrierLab::FSphereSample& Sample : State.Samples)
		{
			PaintSample(OutPixels, SamplePixel(Sample.UnitPosition), ColorFunc(Sample.Id), Radius);
		}
	}

	void BlitThumbnail(
		const TArray<FColor>& Source,
		TArray<FColor>& Dest,
		const int32 DestWidth,
		const int32 DestX,
		const int32 DestY)
	{
		for (int32 Y = 0; Y < ContactThumbHeight; ++Y)
		{
			const int32 SourceY = FMath::Clamp((Y * MapHeight) / ContactThumbHeight, 0, MapHeight - 1);
			for (int32 X = 0; X < ContactThumbWidth; ++X)
			{
				const int32 SourceX = FMath::Clamp((X * MapWidth) / ContactThumbWidth, 0, MapWidth - 1);
				Dest[(DestY + Y) * DestWidth + (DestX + X)] = Source[SourceY * MapWidth + SourceX];
			}
		}
	}

	bool WriteExports(
		const FString& ResolutionDir,
		const CarrierLab::FCarrierState& State,
		const CarrierLab::FStage0ProjectionBuffers& ProjectionBuffers,
		TArray<FString>& OutExportPaths)
	{
		IFileManager::Get().MakeDirectory(*ResolutionDir, true);

		struct FExportImage
		{
			FString Name;
			TArray<FColor> Pixels;
		};

		TArray<FExportImage> Images;
		Images.SetNum(6);
		Images[0].Name = TEXT("PlateId.png");
		MakeMap(State, [&ProjectionBuffers](int32 SampleId)
		{
			const int32 PlateId = ProjectionBuffers.ResolvedPlateIds.IsValidIndex(SampleId) ? ProjectionBuffers.ResolvedPlateIds[SampleId] : INDEX_NONE;
			return PlateId == INDEX_NONE ? FColor(0, 0, 0, 255) : PlateColor(PlateId);
		}, Images[0].Pixels);

		Images[1].Name = TEXT("MissMask.png");
		MakeMap(State, [&ProjectionBuffers](int32 SampleId)
		{
			const uint8 MissClass = ProjectionBuffers.MissClasses.IsValidIndex(SampleId) ? ProjectionBuffers.MissClasses[SampleId] : 0;
			return MissColor(MissClass);
		}, Images[1].Pixels);

		Images[2].Name = TEXT("OverlapMask.png");
		MakeMap(State, [&ProjectionBuffers](int32 SampleId)
		{
			const uint8 OverlapClass = ProjectionBuffers.OverlapClasses.IsValidIndex(SampleId) ? ProjectionBuffers.OverlapClasses[SampleId] : 0;
			return OverlapColor(OverlapClass);
		}, Images[2].Pixels);

		Images[3].Name = TEXT("BoundaryMask.png");
		MakeMap(State, [&ProjectionBuffers](int32 SampleId)
		{
			const bool bBoundary = ProjectionBuffers.BoundaryMask.IsValidIndex(SampleId) && ProjectionBuffers.BoundaryMask[SampleId] != 0;
			return bBoundary ? FColor(245, 245, 245, 255) : FColor(5, 8, 12, 255);
		}, Images[3].Pixels);

		Images[4].Name = TEXT("ContinentalFraction.png");
		MakeMap(State, [&ProjectionBuffers](int32 SampleId)
		{
			const bool bContinental = ProjectionBuffers.ContinentalMask.IsValidIndex(SampleId) && ProjectionBuffers.ContinentalMask[SampleId] != 0;
			return bContinental ? FColor(74, 165, 82, 255) : FColor(32, 78, 142, 255);
		}, Images[4].Pixels);

		Images[5].Name = TEXT("ThirdPlateIntrusion.png");
		MakeMap(State, [&ProjectionBuffers](int32 SampleId)
		{
			if (!ProjectionBuffers.OverlapClasses.IsValidIndex(SampleId))
			{
				return FColor(5, 8, 12, 255);
			}
			const CarrierLab::EStage0OverlapClass OverlapClass =
				static_cast<CarrierLab::EStage0OverlapClass>(ProjectionBuffers.OverlapClasses[SampleId]);
			return (OverlapClass == CarrierLab::EStage0OverlapClass::ThirdPlateBoundary ||
				OverlapClass == CarrierLab::EStage0OverlapClass::ThirdPlateNonDegenerate)
				? OverlapColor(ProjectionBuffers.OverlapClasses[SampleId])
				: FColor(5, 8, 12, 255);
		}, Images[5].Pixels);

		for (const FExportImage& Image : Images)
		{
			const FString Path = FPaths::Combine(ResolutionDir, Image.Name);
			if (!SavePng(Path, Image.Pixels, MapWidth, MapHeight))
			{
				return false;
			}
			OutExportPaths.Add(Path);
		}

		const int32 ContactWidth = ContactThumbWidth * 3;
		const int32 ContactHeight = ContactThumbHeight * 2;
		TArray<FColor> ContactPixels;
		ContactPixels.Init(FColor(0, 0, 0, 255), ContactWidth * ContactHeight);
		for (int32 Index = 0; Index < Images.Num(); ++Index)
		{
			const int32 Column = Index % 3;
			const int32 Row = Index / 3;
			BlitThumbnail(
				Images[Index].Pixels,
				ContactPixels,
				ContactWidth,
				Column * ContactThumbWidth,
				Row * ContactThumbHeight);
		}

		const FString ContactPath = FPaths::Combine(ResolutionDir, TEXT("ContactSheet.png"));
		if (!SavePng(ContactPath, ContactPixels, ContactWidth, ContactHeight))
		{
			return false;
		}
		OutExportPaths.Add(ContactPath);
		return true;
	}

	FString ComparisonRowsJson(const CarrierLab::FStage0Metrics& Metrics)
	{
		const double MissRate = Metrics.SampleCount > 0 ? static_cast<double>(Metrics.RawMissCount) / static_cast<double>(Metrics.SampleCount) : 0.0;
		const double MultiHitRate = Metrics.SampleCount > 0 ? static_cast<double>(Metrics.RawMultiHitCount) / static_cast<double>(Metrics.SampleCount) : 0.0;
		const FString NoBaseline = TEXT("no baseline available");
		const FString Note = TEXT("No Aurous Stage 0 cold-start baseline at this resolution; Aurous published failure-memo baselines are later-step motion/projection failures and are not parameter-matched.");
		return FString::Printf(
			TEXT("[")
			TEXT("{\"metric\":\"stage0_raw_miss_rate\",\"lab_value\":%.12f,\"aurous_value\":null,\"aurous_run_id\":%s,\"parameters_match\":false,\"note\":%s},")
			TEXT("{\"metric\":\"stage0_raw_multi_hit_rate\",\"lab_value\":%.12f,\"aurous_value\":null,\"aurous_run_id\":%s,\"parameters_match\":false,\"note\":%s},")
			TEXT("{\"metric\":\"stage0_projected_caf\",\"lab_value\":%.12f,\"aurous_value\":null,\"aurous_run_id\":%s,\"parameters_match\":false,\"note\":%s}")
			TEXT("]"),
			MissRate,
			*JsonString(NoBaseline),
			*JsonString(Note),
			MultiHitRate,
			*JsonString(NoBaseline),
			*JsonString(Note),
			Metrics.ProjectedContinentalAreaFraction,
			*JsonString(NoBaseline),
			*JsonString(Note));
	}

	FString MetricsRowJson(
		const CarrierLab::FStage0Metrics& Metrics,
		const FString& HashA,
		const FString& HashB,
		const bool bDeterministic,
		const TArray<FString>& ExportPaths)
	{
		TArray<FString> JsonPaths;
		for (const FString& Path : ExportPaths)
		{
			JsonPaths.Add(JsonString(Path));
		}

		return FString::Printf(
			TEXT("{")
			TEXT("\"stage\":\"stage0\",")
			TEXT("\"resolution\":%d,")
			TEXT("\"sample_count\":%d,")
			TEXT("\"plate_count\":%d,")
			TEXT("\"seed\":42,")
			TEXT("\"triangle_count\":%d,")
			TEXT("\"plate_local_vertex_count\":%d,")
			TEXT("\"plate_local_triangle_count\":%d,")
			TEXT("\"edge_count\":%d,")
			TEXT("\"euler_characteristic\":%d,")
			TEXT("\"boundary_triangle_count\":%d,")
			TEXT("\"raw_hit_count\":%d,")
			TEXT("\"raw_miss_count\":%d,")
			TEXT("\"raw_multi_hit_count\":%d,")
			TEXT("\"boundary_degenerate_overlap_count\":%d,")
			TEXT("\"non_degenerate_miss_count\":%d,")
			TEXT("\"non_degenerate_overlap_count\":%d,")
			TEXT("\"third_plate_intrusion_count\":%d,")
			TEXT("\"third_plate_boundary_degenerate_count\":%d,")
			TEXT("\"third_plate_non_degenerate_count\":%d,")
			TEXT("\"nan_or_inf_count\":%d,")
			TEXT("\"ray_candidate_count\":%lld,")
			TEXT("\"ray_triangle_test_count\":%lld,")
			TEXT("\"authoritative_caf\":%.12f,")
			TEXT("\"projected_caf\":%.12f,")
			TEXT("\"total_material_area\":%.12f,")
			TEXT("\"build_wall_clock_seconds\":%.6f,")
			TEXT("\"step_kernel_seconds\":%.6f,")
			TEXT("\"total_wall_clock_seconds\":%.6f,")
			TEXT("\"memory_used_bytes\":%llu,")
			TEXT("\"determinism_hash_a\":%s,")
			TEXT("\"determinism_hash_b\":%s,")
			TEXT("\"deterministic\":%s,")
			TEXT("\"exports\":[%s],")
			TEXT("\"aurous_comparisons\":%s")
			TEXT("}"),
			Metrics.SampleCount,
			Metrics.SampleCount,
			Metrics.PlateCount,
			Metrics.TriangleCount,
			Metrics.PlateLocalVertexCount,
			Metrics.PlateLocalTriangleCount,
			Metrics.EdgeCount,
			Metrics.EulerCharacteristic,
			Metrics.BoundaryTriangleCount,
			Metrics.RawHitCount,
			Metrics.RawMissCount,
			Metrics.RawMultiHitCount,
			Metrics.BoundaryDegenerateOverlapCount,
			Metrics.NonDegenerateMissCount,
			Metrics.NonDegenerateOverlapCount,
			Metrics.ThirdPlateIntrusionCount,
			Metrics.ThirdPlateBoundaryDegenerateCount,
			Metrics.ThirdPlateNonDegenerateCount,
			Metrics.NaNOrInfCount,
			Metrics.RayCandidateCount,
			Metrics.RayTriangleTestCount,
			Metrics.AuthoritativeContinentalAreaFraction,
			Metrics.ProjectedContinentalAreaFraction,
			Metrics.TotalMaterialArea,
			Metrics.BuildWallClockSeconds,
			Metrics.WallClockSeconds,
			Metrics.TotalWallClockSeconds,
			static_cast<unsigned long long>(Metrics.MemoryUsedBytes),
			*JsonString(HashA),
			*JsonString(HashB),
			bDeterministic ? TEXT("true") : TEXT("false"),
			*FString::Join(JsonPaths, TEXT(",")),
			*ComparisonRowsJson(Metrics));
	}
}

UCarrierLabStage0Commandlet::UCarrierLabStage0Commandlet()
{
	IsClient = false;
	IsEditor = false;
	LogToConsole = true;
}

int32 UCarrierLabStage0Commandlet::Main(const FString& Params)
{
	const TArray<int32> Resolutions = ParseResolutions(Params);
	if (Resolutions.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("No valid Stage 0 resolutions provided."));
		return 1;
	}

	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FString MetricsJsonl;
	bool bAllPassed = true;
	for (const int32 Resolution : Resolutions)
	{
		UE_LOG(LogTemp, Display, TEXT("CarrierLab Stage 0: running %d samples."), Resolution);

		CarrierLab::FStage0Config Config;
		Config.SampleCount = Resolution;
		Config.PlateCount = 40;
		Config.Seed = 42;
		Config.ContinentalPlateFraction = 0.30;

		const double TotalStartSeconds = FPlatformTime::Seconds();
		const double BuildStartSeconds = FPlatformTime::Seconds();
		CarrierLab::FCarrierState StateA;
		FString ErrorA;
		if (!CarrierLab::FCarrierLabStage0::BuildColdStartCarrier(Config, StateA, ErrorA))
		{
			UE_LOG(LogTemp, Error, TEXT("Stage 0 build failed at %d: %s"), Resolution, *ErrorA);
			bAllPassed = false;
			continue;
		}
		const double BuildSeconds = FPlatformTime::Seconds() - BuildStartSeconds;

		CarrierLab::FStage0ProjectionBuffers ProjectionBuffers;
		CarrierLab::FStage0Metrics MetricsA = CarrierLab::FCarrierLabStage0::ProjectColdStart(StateA, &ProjectionBuffers);
		MetricsA.BuildWallClockSeconds = BuildSeconds;
		MetricsA.TotalWallClockSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
		MetricsA.MemoryUsedBytes = FPlatformMemory::GetStats().UsedPhysical;
		const FString HashA = MetricsA.DeterminismHash;

		const double ReplayBuildStartSeconds = FPlatformTime::Seconds();
		CarrierLab::FCarrierState StateB;
		FString ErrorB;
		if (!CarrierLab::FCarrierLabStage0::BuildColdStartCarrier(Config, StateB, ErrorB))
		{
			UE_LOG(LogTemp, Error, TEXT("Stage 0 replay build failed at %d: %s"), Resolution, *ErrorB);
			bAllPassed = false;
			continue;
		}
		CarrierLab::FStage0Metrics MetricsB = CarrierLab::FCarrierLabStage0::ProjectColdStart(StateB, nullptr);
		MetricsB.BuildWallClockSeconds = FPlatformTime::Seconds() - ReplayBuildStartSeconds;
		const FString HashB = MetricsB.DeterminismHash;
		const bool bDeterministic = HashA == HashB;
		if (!bDeterministic)
		{
			UE_LOG(LogTemp, Error, TEXT("Stage 0 determinism failed at %d: %s vs %s"), Resolution, *HashA, *HashB);
			bAllPassed = false;
		}

		const FString ResolutionDir = FPaths::Combine(OutputRoot, FString::Printf(TEXT("%dk"), Resolution / 1000));
		TArray<FString> ExportPaths;
		if (!WriteExports(ResolutionDir, StateA, ProjectionBuffers, ExportPaths))
		{
			UE_LOG(LogTemp, Error, TEXT("Stage 0 export failed at %d."), Resolution);
			bAllPassed = false;
		}

		MetricsJsonl += MetricsRowJson(MetricsA, HashA, HashB, bDeterministic, ExportPaths);
		MetricsJsonl += LINE_TERMINATOR;

		UE_LOG(
			LogTemp,
			Display,
			TEXT("CarrierLabStage0 resolution=%d tris=%d hit=%d miss=%d multi=%d boundary2=%d third=%d caf=%.6f kernel=%.3fs total=%.3fs hash=%s"),
			Resolution,
			MetricsA.TriangleCount,
			MetricsA.RawHitCount,
			MetricsA.RawMissCount,
			MetricsA.RawMultiHitCount,
			MetricsA.BoundaryDegenerateOverlapCount,
			MetricsA.ThirdPlateIntrusionCount,
			MetricsA.ProjectedContinentalAreaFraction,
			MetricsA.WallClockSeconds,
			MetricsA.TotalWallClockSeconds,
			*HashA);
	}

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	if (!FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write metrics: %s"), *MetricsPath);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Stage 0 metrics: %s"), *MetricsPath);
	return bAllPassed ? 0 : 2;
}
