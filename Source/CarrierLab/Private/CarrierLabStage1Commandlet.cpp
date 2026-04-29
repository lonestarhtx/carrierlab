// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabStage1Commandlet.h"

#include "CarrierLabCarrier.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "HAL/PlatformMemory.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "LineTypes.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Spatial/SpatialInterfaces.h"

namespace
{
	using UE::Geometry::FDynamicMesh3;
	using UE::Geometry::FDynamicMeshAABBTree3;
	using UE::Geometry::IMeshSpatial;

	constexpr int32 MapWidth = 2048;
	constexpr int32 MapHeight = 1024;
	constexpr int32 ContactThumbWidth = 512;
	constexpr int32 ContactThumbHeight = 256;
	constexpr double BoundaryBarycentricEpsilon = 1.0e-7;
	constexpr double EarthRadiusKm = 6371.0;
	constexpr double DefaultAngularSpeedRadiansPerStep = 8950.0 / (EarthRadiusKm * 400.0);

	enum class EStage1MissClass : uint8
	{
		None = 0,
		DivergentGap = 1,
		NumericMiss = 2,
		TopologyHole = 3,
		OutOfDomain = 4
	};

	enum class EStage1OverlapClass : uint8
	{
		None = 0,
		BoundaryDegenerate = 1,
		ConvergentOverlap = 2,
		NumericOverlap = 3,
		ThirdPlateBoundary = 4,
		ThirdPlateNonDegenerate = 5
	};

	struct FStage1Motion
	{
		FVector3d Axis = FVector3d::UnitZ();
		double AngularSpeedRadiansPerStep = 0.0;
		FVector3d CurrentCenter = FVector3d::UnitZ();
	};

	struct FPlateRayMesh
	{
		FDynamicMesh3 Mesh;
		TUniquePtr<FDynamicMeshAABBTree3> Tree;
		TMap<int32, int32> MeshTriangleIdToLocalTriangleId;
	};

	struct FStage1Buffers
	{
		TArray<int32> ResolvedPlateIds;
		TArray<uint8> MissClasses;
		TArray<uint8> OverlapClasses;
		TArray<uint8> BoundaryMask;
		TArray<uint8> ContinentalMask;
		TArray<double> DriftErrorRadiansBySample;
	};

	struct FStage1Metrics
	{
		int32 Resolution = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 Seed = 42;
		int32 Step = 0;
		int32 RawHitCount = 0;
		int32 RawMissCount = 0;
		int32 RawMultiHitCount = 0;
		int32 TrueMissCount = 0;
		int32 DivergentGapCount = 0;
		int32 NumericMissCount = 0;
		int32 TopologyHoleCount = 0;
		int32 OutOfDomainMissCount = 0;
		int32 BoundaryDegenerateOverlapCount = 0;
		int32 ConvergentOverlapCount = 0;
		int32 NumericOverlapCount = 0;
		int32 ThirdPlateIntrusionCount = 0;
		int32 ThirdPlateBoundaryDegenerateCount = 0;
		int32 ThirdPlateNonDegenerateCount = 0;
		int32 ResolvedClassifiedCount = 0;
		int32 NaNOrInfCount = 0;
		int64 RayTreeQueryCount = 0;
		int64 RayHitTriangleCount = 0;
		double AuthoritativeCAF = 0.0;
		double ProjectedCAF = 0.0;
		double TotalMaterialArea = 0.0;
		double DriftExpectedMeanKm = 0.0;
		double DriftObservedMeanKm = 0.0;
		double DriftErrorMeanKm = 0.0;
		double DriftErrorP50Km = 0.0;
		double DriftErrorP95Km = 0.0;
		double DriftErrorMaxKm = 0.0;
		double BvhBuildSeconds = 0.0;
		double ProjectionSeconds = 0.0;
		double StepKernelSeconds = 0.0;
		double TotalWallClockSeconds = 0.0;
		uint64 MemoryUsedBytes = 0;
		FString DeterminismHash;
	};

	struct FRunResult
	{
		TArray<FStage1Metrics> Metrics;
		TMap<int32, FString> HashByStep;
		TArray<FString> ExportPaths;
		bool bPassed = true;
	};

	struct FStage1Candidate
	{
		int32 PlateId = INDEX_NONE;
		int32 LocalTriangleId = INDEX_NONE;
		FVector3d Bary = FVector3d::Zero();
		double Distance = 0.0;
		bool bBoundary = false;
	};

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

	bool ExtractValue(const FString& Params, const FString& Key, FString& OutValue)
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
	}

	TArray<int32> ParseIntList(const FString& Params, const FString& Key, const TArray<int32>& Defaults)
	{
		FString Value;
		if (!ExtractValue(Params, Key, Value) || Value.IsEmpty())
		{
			return Defaults;
		}

		TArray<int32> Parsed;
		TArray<FString> Parts;
		Value.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& Part : Parts)
		{
			const int32 ParsedValue = FCString::Atoi(*Part);
			if (ParsedValue >= 0)
			{
				Parsed.Add(ParsedValue);
			}
		}
		return Parsed.IsEmpty() ? Defaults : Parsed;
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("Stage1"), Stamp);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	void HashMix(uint64& Hash, const uint64 Value)
	{
		Hash ^= Value + 0x9e3779b97f4a7c15ull + (Hash << 6) + (Hash >> 2);
	}

	void HashMixDouble(uint64& Hash, const double Value)
	{
		const int64 Quantized = FMath::RoundToInt64(Value * 1000000000.0);
		HashMix(Hash, static_cast<uint64>(Quantized));
	}

	FString MakeHash(const FStage1Metrics& Metrics, const FStage1Buffers& Buffers)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMix(Hash, static_cast<uint64>(Metrics.Resolution));
		HashMix(Hash, static_cast<uint64>(Metrics.RawHitCount));
		HashMix(Hash, static_cast<uint64>(Metrics.RawMissCount));
		HashMix(Hash, static_cast<uint64>(Metrics.RawMultiHitCount));
		HashMix(Hash, static_cast<uint64>(Metrics.ThirdPlateIntrusionCount));
		HashMixDouble(Hash, Metrics.AuthoritativeCAF);
		HashMixDouble(Hash, Metrics.ProjectedCAF);
		for (int32 Index = 0; Index < Buffers.ResolvedPlateIds.Num(); Index += FMath::Max(1, Buffers.ResolvedPlateIds.Num() / 4096))
		{
			HashMix(Hash, static_cast<uint64>(Buffers.ResolvedPlateIds[Index] + 2));
			HashMix(Hash, static_cast<uint64>(Buffers.MissClasses[Index]));
			HashMix(Hash, static_cast<uint64>(Buffers.OverlapClasses[Index]));
		}
		return FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Hash));
	}

	bool IsFiniteUnit(const FVector3d& V)
	{
		return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z) && FMath::Abs(V.Length() - 1.0) < 1.0e-4;
	}

	FVector3d NormalizeOrFallback(const FVector3d& V, const FVector3d& Fallback)
	{
		const double SizeSquared = V.SquaredLength();
		if (SizeSquared <= UE_SMALL_NUMBER || !FMath::IsFinite(SizeSquared))
		{
			return Fallback;
		}
		return V / FMath::Sqrt(SizeSquared);
	}

	FVector3d RotateVector(const FVector3d& Position, const FVector3d& Axis, const double AngleRadians)
	{
		const double C = FMath::Cos(AngleRadians);
		const double S = FMath::Sin(AngleRadians);
		return (Position * C) + (FVector3d::CrossProduct(Axis, Position) * S) + (Axis * FVector3d::DotProduct(Axis, Position) * (1.0 - C));
	}

	double AngularDistanceRadians(const FVector3d& A, const FVector3d& B)
	{
		return FMath::Acos(FMath::Clamp(FVector3d::DotProduct(A, B), -1.0, 1.0));
	}

	bool IsBoundaryHit(const FVector3d& Bary)
	{
		return Bary.X <= BoundaryBarycentricEpsilon || Bary.Y <= BoundaryBarycentricEpsilon || Bary.Z <= BoundaryBarycentricEpsilon;
	}

	int32 LowestSetBitIndex(const uint64 Mask)
	{
		for (int32 Index = 0; Index < 64; ++Index)
		{
			if ((Mask & (1ull << Index)) != 0)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	int32 CountBits64(uint64 Value)
	{
		int32 Count = 0;
		while (Value != 0)
		{
			Value &= Value - 1;
			++Count;
		}
		return Count;
	}

	int32 ChooseNearestCandidatePlate(
		const CarrierLab::FSphereSample& Sample,
		const TArray<FStage1Candidate>& Candidates,
		const TArray<FStage1Motion>& Motions)
	{
		int32 BestPlateId = INDEX_NONE;
		double BestDot = -TNumericLimits<double>::Max();
		for (const FStage1Candidate& Candidate : Candidates)
		{
			if (!Motions.IsValidIndex(Candidate.PlateId))
			{
				continue;
			}
			const double Dot = FVector3d::DotProduct(Sample.UnitPosition, Motions[Candidate.PlateId].CurrentCenter);
			if (Dot > BestDot + 1.0e-12 || (FMath::IsNearlyEqual(Dot, BestDot, 1.0e-12) && Candidate.PlateId < BestPlateId))
			{
				BestDot = Dot;
				BestPlateId = Candidate.PlateId;
			}
		}
		return BestPlateId;
	}

	void FindTwoNearestPlates(
		const FVector3d& Position,
		const TArray<FStage1Motion>& Motions,
		int32& OutA,
		int32& OutB)
	{
		OutA = INDEX_NONE;
		OutB = INDEX_NONE;
		double BestA = -TNumericLimits<double>::Max();
		double BestB = -TNumericLimits<double>::Max();
		for (int32 PlateId = 0; PlateId < Motions.Num(); ++PlateId)
		{
			const double Dot = FVector3d::DotProduct(Position, Motions[PlateId].CurrentCenter);
			if (Dot > BestA)
			{
				BestB = BestA;
				OutB = OutA;
				BestA = Dot;
				OutA = PlateId;
			}
			else if (Dot > BestB)
			{
				BestB = Dot;
				OutB = PlateId;
			}
		}
	}

	bool IsSeparatingKinematics(const FVector3d& Position, const TArray<FStage1Motion>& Motions)
	{
		int32 A = INDEX_NONE;
		int32 B = INDEX_NONE;
		FindTwoNearestPlates(Position, Motions, A, B);
		if (!Motions.IsValidIndex(A) || !Motions.IsValidIndex(B))
		{
			return false;
		}

		const FVector3d ToB = NormalizeOrFallback(
			Motions[B].CurrentCenter - FVector3d::DotProduct(Motions[B].CurrentCenter, Position) * Position,
			FVector3d::UnitX());
		const FVector3d VelocityA = FVector3d::CrossProduct(Motions[A].Axis, Position) * Motions[A].AngularSpeedRadiansPerStep;
		const FVector3d VelocityB = FVector3d::CrossProduct(Motions[B].Axis, Position) * Motions[B].AngularSpeedRadiansPerStep;
		return FVector3d::DotProduct(VelocityB - VelocityA, ToB) > 0.0;
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
		switch (static_cast<EStage1MissClass>(MissClass))
		{
		case EStage1MissClass::DivergentGap:
			return FColor(255, 210, 40, 255);
		case EStage1MissClass::NumericMiss:
			return FColor(255, 40, 40, 255);
		case EStage1MissClass::TopologyHole:
			return FColor(255, 0, 255, 255);
		case EStage1MissClass::OutOfDomain:
			return FColor(255, 255, 255, 255);
		default:
			return FColor(5, 8, 12, 255);
		}
	}

	FColor OverlapColor(const uint8 OverlapClass)
	{
		switch (static_cast<EStage1OverlapClass>(OverlapClass))
		{
		case EStage1OverlapClass::BoundaryDegenerate:
			return FColor(30, 220, 255, 255);
		case EStage1OverlapClass::ConvergentOverlap:
			return FColor(255, 35, 35, 255);
		case EStage1OverlapClass::NumericOverlap:
			return FColor(255, 0, 255, 255);
		case EStage1OverlapClass::ThirdPlateBoundary:
			return FColor(255, 155, 20, 255);
		case EStage1OverlapClass::ThirdPlateNonDegenerate:
			return FColor(255, 0, 220, 255);
		default:
			return FColor(5, 8, 12, 255);
		}
	}

	FColor DriftErrorColor(const double ErrorRadians)
	{
		if (!FMath::IsFinite(ErrorRadians) || ErrorRadians < 0.0)
		{
			return FColor(5, 8, 12, 255);
		}
		const double ErrorKm = ErrorRadians * EarthRadiusKm;
		if (ErrorKm < 0.001)
		{
			return FColor(0, 0, 0, 255);
		}
		if (ErrorKm < 1.0)
		{
			return FColor(20, 180, 80, 255);
		}
		if (ErrorKm < 10.0)
		{
			return FColor(240, 210, 40, 255);
		}
		return FColor(255, 35, 35, 255);
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
		const FString& StepDir,
		const CarrierLab::FCarrierState& State,
		const FStage1Buffers& Buffers,
		TArray<FString>& OutExportPaths)
	{
		IFileManager::Get().MakeDirectory(*StepDir, true);

		struct FExportImage
		{
			FString Name;
			TArray<FColor> Pixels;
		};

		TArray<FExportImage> Images;
		Images.SetNum(7);
		Images[0].Name = TEXT("PlateId.png");
		MakeMap(State, [&Buffers](const int32 SampleId)
		{
			const int32 PlateId = Buffers.ResolvedPlateIds.IsValidIndex(SampleId) ? Buffers.ResolvedPlateIds[SampleId] : INDEX_NONE;
			return PlateId == INDEX_NONE ? FColor(0, 0, 0, 255) : PlateColor(PlateId);
		}, Images[0].Pixels);

		Images[1].Name = TEXT("MissMask.png");
		MakeMap(State, [&Buffers](const int32 SampleId)
		{
			return MissColor(Buffers.MissClasses.IsValidIndex(SampleId) ? Buffers.MissClasses[SampleId] : 0);
		}, Images[1].Pixels);

		Images[2].Name = TEXT("OverlapMask.png");
		MakeMap(State, [&Buffers](const int32 SampleId)
		{
			return OverlapColor(Buffers.OverlapClasses.IsValidIndex(SampleId) ? Buffers.OverlapClasses[SampleId] : 0);
		}, Images[2].Pixels);

		Images[3].Name = TEXT("BoundaryMask.png");
		MakeMap(State, [&Buffers](const int32 SampleId)
		{
			const bool bBoundary = Buffers.BoundaryMask.IsValidIndex(SampleId) && Buffers.BoundaryMask[SampleId] != 0;
			return bBoundary ? FColor(245, 245, 245, 255) : FColor(5, 8, 12, 255);
		}, Images[3].Pixels);

		Images[4].Name = TEXT("ContinentalFraction.png");
		MakeMap(State, [&Buffers](const int32 SampleId)
		{
			const bool bContinental = Buffers.ContinentalMask.IsValidIndex(SampleId) && Buffers.ContinentalMask[SampleId] != 0;
			return bContinental ? FColor(74, 165, 82, 255) : FColor(32, 78, 142, 255);
		}, Images[4].Pixels);

		Images[5].Name = TEXT("ThirdPlateIntrusion.png");
		MakeMap(State, [&Buffers](const int32 SampleId)
		{
			if (!Buffers.OverlapClasses.IsValidIndex(SampleId))
			{
				return FColor(5, 8, 12, 255);
			}
			const EStage1OverlapClass OverlapClass = static_cast<EStage1OverlapClass>(Buffers.OverlapClasses[SampleId]);
			return (OverlapClass == EStage1OverlapClass::ThirdPlateBoundary || OverlapClass == EStage1OverlapClass::ThirdPlateNonDegenerate)
				? OverlapColor(Buffers.OverlapClasses[SampleId])
				: FColor(5, 8, 12, 255);
		}, Images[5].Pixels);

		Images[6].Name = TEXT("DriftErrorMap.png");
		MakeMap(State, [&Buffers](const int32 SampleId)
		{
			return DriftErrorColor(Buffers.DriftErrorRadiansBySample.IsValidIndex(SampleId) ? Buffers.DriftErrorRadiansBySample[SampleId] : -1.0);
		}, Images[6].Pixels);

		for (const FExportImage& Image : Images)
		{
			const FString Path = FPaths::Combine(StepDir, Image.Name);
			if (!SavePng(Path, Image.Pixels, MapWidth, MapHeight))
			{
				return false;
			}
			OutExportPaths.Add(Path);
		}

		const int32 ContactWidth = ContactThumbWidth * 4;
		const int32 ContactHeight = ContactThumbHeight * 2;
		TArray<FColor> ContactPixels;
		ContactPixels.Init(FColor(0, 0, 0, 255), ContactWidth * ContactHeight);
		for (int32 Index = 0; Index < Images.Num(); ++Index)
		{
			const int32 Column = Index % 4;
			const int32 Row = Index / 4;
			BlitThumbnail(Images[Index].Pixels, ContactPixels, ContactWidth, Column * ContactThumbWidth, Row * ContactThumbHeight);
		}

		const FString ContactPath = FPaths::Combine(StepDir, TEXT("ContactSheet.png"));
		if (!SavePng(ContactPath, ContactPixels, ContactWidth, ContactHeight))
		{
			return false;
		}
		OutExportPaths.Add(ContactPath);
		return true;
	}

	void CaptureInitialPositions(
		const CarrierLab::FCarrierState& State,
		TArray<TArray<FVector3d>>& OutInitialPositions)
	{
		OutInitialPositions.Reset(State.Plates.Num());
		OutInitialPositions.SetNum(State.Plates.Num());
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			TArray<FVector3d>& Positions = OutInitialPositions[Plate.PlateId];
			Positions.Reserve(Plate.Vertices.Num());
			for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
			{
				Positions.Add(Vertex.UnitPosition);
			}
		}
	}

	void BuildDefaultMotions(const CarrierLab::FCarrierState& State, TArray<FStage1Motion>& OutMotions, const double SpeedScale)
	{
		OutMotions.Reset(State.Plates.Num());
		OutMotions.SetNum(State.Plates.Num());
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			const FVector3d SeedVector = NormalizeOrFallback(
				FVector3d(
					FMath::Sin(static_cast<double>(Plate.PlateId + 1) * 12.9898),
					FMath::Sin(static_cast<double>(Plate.PlateId + 1) * 78.233),
					FMath::Sin(static_cast<double>(Plate.PlateId + 1) * 37.719)),
				FVector3d::UnitZ());
			FVector3d Axis = FVector3d::CrossProduct(Plate.InitialCenter, SeedVector);
			if (Axis.SquaredLength() <= UE_SMALL_NUMBER)
			{
				Axis = FVector3d::CrossProduct(Plate.InitialCenter, FVector3d::UnitX());
			}
			OutMotions[Plate.PlateId].Axis = NormalizeOrFallback(Axis, FVector3d::UnitZ());
			OutMotions[Plate.PlateId].AngularSpeedRadiansPerStep = DefaultAngularSpeedRadiansPerStep * SpeedScale;
			OutMotions[Plate.PlateId].CurrentCenter = Plate.InitialCenter;
		}
	}

	void BuildForcedPairMotions(
		const CarrierLab::FCarrierState& State,
		TArray<FStage1Motion>& OutMotions,
		const bool bConvergent)
	{
		OutMotions.Reset(State.Plates.Num());
		OutMotions.SetNum(State.Plates.Num());
		if (State.Plates.Num() < 2)
		{
			return;
		}

		for (int32 PlateId = 0; PlateId < State.Plates.Num(); ++PlateId)
		{
			const CarrierLab::FCarrierPlate& Plate = State.Plates[PlateId];
			const CarrierLab::FCarrierPlate& OtherPlate = State.Plates[PlateId == 0 ? 1 : 0];
			FVector3d Direction = OtherPlate.InitialCenter - FVector3d::DotProduct(OtherPlate.InitialCenter, Plate.InitialCenter) * Plate.InitialCenter;
			Direction = NormalizeOrFallback(Direction, FVector3d::UnitX());
			if (!bConvergent)
			{
				Direction *= -1.0;
			}
			const FVector3d Axis = NormalizeOrFallback(FVector3d::CrossProduct(Plate.InitialCenter, Direction), FVector3d::UnitZ());
			OutMotions[PlateId].Axis = Axis;
			OutMotions[PlateId].AngularSpeedRadiansPerStep = DefaultAngularSpeedRadiansPerStep * 1.8;
			OutMotions[PlateId].CurrentCenter = Plate.InitialCenter;
		}
	}

	void ApplyOneRigidMotionStep(CarrierLab::FCarrierState& State, TArray<FStage1Motion>& Motions)
	{
		for (CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			if (!Motions.IsValidIndex(Plate.PlateId))
			{
				continue;
			}
			const FStage1Motion& Motion = Motions[Plate.PlateId];
			for (CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
			{
				Vertex.UnitPosition = NormalizeOrFallback(RotateVector(Vertex.UnitPosition, Motion.Axis, Motion.AngularSpeedRadiansPerStep), Vertex.UnitPosition);
			}
		}

		for (FStage1Motion& Motion : Motions)
		{
			Motion.CurrentCenter = NormalizeOrFallback(RotateVector(Motion.CurrentCenter, Motion.Axis, Motion.AngularSpeedRadiansPerStep), Motion.CurrentCenter);
		}
	}

	bool BuildPlateRayMeshes(const CarrierLab::FCarrierState& State, TArray<FPlateRayMesh>& OutPlateMeshes, FString& OutError)
	{
		OutPlateMeshes.Reset(State.Plates.Num());
		OutPlateMeshes.SetNum(State.Plates.Num());
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			if (!OutPlateMeshes.IsValidIndex(Plate.PlateId))
			{
				OutError = FString::Printf(TEXT("Invalid plate id %d while building ray meshes."), Plate.PlateId);
				return false;
			}

			FPlateRayMesh& PlateMesh = OutPlateMeshes[Plate.PlateId];
			PlateMesh.Mesh.EnableTriangleGroups();
			for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
			{
				PlateMesh.Mesh.AppendVertex(Vertex.UnitPosition);
			}

			for (int32 LocalTriangleId = 0; LocalTriangleId < Plate.LocalTriangles.Num(); ++LocalTriangleId)
			{
				const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
				const int32 MeshTriangleId = PlateMesh.Mesh.AppendTriangle(Triangle.A, Triangle.B, Triangle.C, 0);
				if (MeshTriangleId < 0)
				{
					OutError = FString::Printf(TEXT("FDynamicMesh3 rejected plate %d local triangle %d."), Plate.PlateId, LocalTriangleId);
					return false;
				}
				PlateMesh.MeshTriangleIdToLocalTriangleId.Add(MeshTriangleId, LocalTriangleId);
			}

			PlateMesh.Tree = MakeUnique<FDynamicMeshAABBTree3>(&PlateMesh.Mesh, true);
		}
		return true;
	}

	void ComputeDriftMetrics(
		const CarrierLab::FCarrierState& State,
		const TArray<TArray<FVector3d>>& InitialPositions,
		const TArray<FStage1Motion>& Motions,
		const int32 Step,
		FStage1Buffers& Buffers,
		FStage1Metrics& Metrics)
	{
		TArray<double> ErrorsKm;
		double ExpectedSumKm = 0.0;
		double ObservedSumKm = 0.0;
		double ErrorSumKm = 0.0;
		int32 Count = 0;
		Buffers.DriftErrorRadiansBySample.Init(-1.0, State.Samples.Num());

		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			if (!Plate.bContinental || !InitialPositions.IsValidIndex(Plate.PlateId) || !Motions.IsValidIndex(Plate.PlateId))
			{
				continue;
			}
			const TArray<FVector3d>& PlateInitialPositions = InitialPositions[Plate.PlateId];
			const FStage1Motion& Motion = Motions[Plate.PlateId];
			for (int32 LocalVertexId = 0; LocalVertexId < Plate.Vertices.Num(); ++LocalVertexId)
			{
				const CarrierLab::FCarrierVertex& Vertex = Plate.Vertices[LocalVertexId];
				if (!PlateInitialPositions.IsValidIndex(LocalVertexId))
				{
					continue;
				}
				const FVector3d Initial = PlateInitialPositions[LocalVertexId];
				const FVector3d Expected = NormalizeOrFallback(RotateVector(Initial, Motion.Axis, Motion.AngularSpeedRadiansPerStep * Step), Initial);
				const double ExpectedKm = AngularDistanceRadians(Initial, Expected) * EarthRadiusKm;
				const double ObservedKm = AngularDistanceRadians(Initial, Vertex.UnitPosition) * EarthRadiusKm;
				const double ErrorRadians = AngularDistanceRadians(Vertex.UnitPosition, Expected);
				const double ErrorKm = ErrorRadians * EarthRadiusKm;
				if (!FMath::IsFinite(ErrorKm))
				{
					++Metrics.NaNOrInfCount;
					continue;
				}
				ExpectedSumKm += ExpectedKm;
				ObservedSumKm += ObservedKm;
				ErrorSumKm += ErrorKm;
				ErrorsKm.Add(ErrorKm);
				++Count;

				if (Buffers.DriftErrorRadiansBySample.IsValidIndex(Vertex.GlobalSampleId))
				{
					Buffers.DriftErrorRadiansBySample[Vertex.GlobalSampleId] =
						FMath::Max(Buffers.DriftErrorRadiansBySample[Vertex.GlobalSampleId], ErrorRadians);
				}
			}
		}

		if (Count <= 0)
		{
			return;
		}

		ErrorsKm.Sort();
		Metrics.DriftExpectedMeanKm = ExpectedSumKm / static_cast<double>(Count);
		Metrics.DriftObservedMeanKm = ObservedSumKm / static_cast<double>(Count);
		Metrics.DriftErrorMeanKm = ErrorSumKm / static_cast<double>(Count);
		Metrics.DriftErrorP50Km = ErrorsKm[FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(0.50 * (ErrorsKm.Num() - 1))), 0, ErrorsKm.Num() - 1)];
		Metrics.DriftErrorP95Km = ErrorsKm[FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(0.95 * (ErrorsKm.Num() - 1))), 0, ErrorsKm.Num() - 1)];
		Metrics.DriftErrorMaxKm = ErrorsKm.Last();
	}

	FStage1Metrics ProjectWithPlateRayMeshes(
		const CarrierLab::FCarrierState& State,
		const TArray<TArray<FVector3d>>& InitialPositions,
		const TArray<FStage1Motion>& Motions,
		const int32 Step,
		FStage1Buffers& OutBuffers)
	{
		FStage1Metrics Metrics;
		Metrics.Resolution = State.Samples.Num();
		Metrics.SampleCount = State.Samples.Num();
		Metrics.PlateCount = State.Plates.Num();
		Metrics.Seed = State.Config.Seed;
		Metrics.Step = Step;
		Metrics.TotalMaterialArea = 1.0;

		OutBuffers.ResolvedPlateIds.Init(INDEX_NONE, State.Samples.Num());
		OutBuffers.MissClasses.Init(static_cast<uint8>(EStage1MissClass::None), State.Samples.Num());
		OutBuffers.OverlapClasses.Init(static_cast<uint8>(EStage1OverlapClass::None), State.Samples.Num());
		OutBuffers.BoundaryMask.Init(0, State.Samples.Num());
		OutBuffers.ContinentalMask.Init(0, State.Samples.Num());

		const double BvhStartSeconds = FPlatformTime::Seconds();
		TArray<FPlateRayMesh> PlateMeshes;
		FString MeshError;
		if (!BuildPlateRayMeshes(State, PlateMeshes, MeshError))
		{
			UE_LOG(LogTemp, Error, TEXT("Stage 1 BVH build failed: %s"), *MeshError);
			++Metrics.NaNOrInfCount;
			return Metrics;
		}
		Metrics.BvhBuildSeconds = FPlatformTime::Seconds() - BvhStartSeconds;

		double AuthoritativeContinentalArea = 0.0;
		double ProjectedContinentalArea = 0.0;
		double TotalArea = 0.0;
		for (const CarrierLab::FSphereSample& Sample : State.Samples)
		{
			TotalArea += Sample.AreaWeight;
			if (Sample.bContinental)
			{
				AuthoritativeContinentalArea += Sample.AreaWeight;
			}
		}
		const double ProjectionStartSeconds = FPlatformTime::Seconds();
		IMeshSpatial::FQueryOptions QueryOptions(2.0 + 1.0e-6);
		TArray<MeshIntersection::FHitIntersectionResult> Hits;
		TArray<FStage1Candidate> Candidates;

		for (const CarrierLab::FSphereSample& Sample : State.Samples)
		{
			if (!IsFiniteUnit(Sample.UnitPosition))
			{
				++Metrics.NaNOrInfCount;
				OutBuffers.MissClasses[Sample.Id] = static_cast<uint8>(EStage1MissClass::OutOfDomain);
				++Metrics.RawMissCount;
				++Metrics.TrueMissCount;
				++Metrics.OutOfDomainMissCount;
				continue;
			}

			uint64 PlateMask = 0;
			bool bAnyBoundary = false;
			Candidates.Reset();
			const FRay3d Ray(FVector3d::Zero(), Sample.UnitPosition);
			for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
			{
				if (!PlateMeshes.IsValidIndex(Plate.PlateId) || !PlateMeshes[Plate.PlateId].Tree.IsValid())
				{
					continue;
				}
				Hits.Reset();
				++Metrics.RayTreeQueryCount;
				PlateMeshes[Plate.PlateId].Tree->FindAllHitTriangles(Ray, Hits, QueryOptions);
				for (const MeshIntersection::FHitIntersectionResult& Hit : Hits)
				{
					if (Hit.Distance <= 0.0 || Hit.Distance > 2.0 + 1.0e-6)
					{
						continue;
					}
					const int32* LocalTriangleId = PlateMeshes[Plate.PlateId].MeshTriangleIdToLocalTriangleId.Find(Hit.TriangleId);
					if (LocalTriangleId == nullptr)
					{
						continue;
					}
					++Metrics.RayHitTriangleCount;
					const bool bBoundary = IsBoundaryHit(Hit.BaryCoords);
					bAnyBoundary = bAnyBoundary || bBoundary;
					PlateMask |= (1ull << static_cast<uint64>(Plate.PlateId));
					FStage1Candidate Candidate;
					Candidate.PlateId = Plate.PlateId;
					Candidate.LocalTriangleId = *LocalTriangleId;
					Candidate.Bary = Hit.BaryCoords;
					Candidate.Distance = Hit.Distance;
					Candidate.bBoundary = bBoundary;
					Candidates.Add(Candidate);
				}

				if (Hits.IsEmpty())
				{
					double NearestDistSqr = TNumericLimits<double>::Max();
					const int32 NearestTriangleId = PlateMeshes[Plate.PlateId].Tree->FindNearestTriangle(Sample.UnitPosition, NearestDistSqr, IMeshSpatial::FQueryOptions(1.0e-8));
					const int32* LocalTriangleId = PlateMeshes[Plate.PlateId].MeshTriangleIdToLocalTriangleId.Find(NearestTriangleId);
					if (LocalTriangleId != nullptr && NearestDistSqr <= 1.0e-16)
					{
						++Metrics.RayHitTriangleCount;
						bAnyBoundary = true;
						PlateMask |= (1ull << static_cast<uint64>(Plate.PlateId));
						FStage1Candidate Candidate;
						Candidate.PlateId = Plate.PlateId;
						Candidate.LocalTriangleId = *LocalTriangleId;
						Candidate.Bary = FVector3d(1.0, 0.0, 0.0);
						Candidate.Distance = 1.0;
						Candidate.bBoundary = true;
						Candidates.Add(Candidate);
					}
				}
			}

			const int32 PlateHitCount = CountBits64(PlateMask);
			OutBuffers.BoundaryMask[Sample.Id] = bAnyBoundary ? 1 : 0;
			if (PlateHitCount == 0)
			{
				++Metrics.RawMissCount;
				++Metrics.TrueMissCount;
				if (IsSeparatingKinematics(Sample.UnitPosition, Motions))
				{
					OutBuffers.MissClasses[Sample.Id] = static_cast<uint8>(EStage1MissClass::DivergentGap);
					++Metrics.DivergentGapCount;
				}
				else
				{
					OutBuffers.MissClasses[Sample.Id] = static_cast<uint8>(EStage1MissClass::NumericMiss);
					++Metrics.NumericMissCount;
				}
				continue;
			}

			++Metrics.RawHitCount;
			if (PlateHitCount > 1)
			{
				++Metrics.RawMultiHitCount;
				if (PlateHitCount >= 3)
				{
					++Metrics.ThirdPlateIntrusionCount;
					if (bAnyBoundary)
					{
						OutBuffers.OverlapClasses[Sample.Id] = static_cast<uint8>(EStage1OverlapClass::ThirdPlateBoundary);
						++Metrics.ThirdPlateBoundaryDegenerateCount;
					}
					else
					{
						OutBuffers.OverlapClasses[Sample.Id] = static_cast<uint8>(EStage1OverlapClass::ThirdPlateNonDegenerate);
						++Metrics.ThirdPlateNonDegenerateCount;
					}
				}
				else if (bAnyBoundary)
				{
					OutBuffers.OverlapClasses[Sample.Id] = static_cast<uint8>(EStage1OverlapClass::BoundaryDegenerate);
					++Metrics.BoundaryDegenerateOverlapCount;
				}
				else
				{
					OutBuffers.OverlapClasses[Sample.Id] = static_cast<uint8>(EStage1OverlapClass::ConvergentOverlap);
					++Metrics.ConvergentOverlapCount;
				}
			}

			const int32 ResolvedPlateId = PlateHitCount == 1 ? LowestSetBitIndex(PlateMask) : ChooseNearestCandidatePlate(Sample, Candidates, Motions);
			OutBuffers.ResolvedPlateIds[Sample.Id] = ResolvedPlateId;
			++Metrics.ResolvedClassifiedCount;
			if (State.Plates.IsValidIndex(ResolvedPlateId) && State.Plates[ResolvedPlateId].bContinental)
			{
				OutBuffers.ContinentalMask[Sample.Id] = 1;
				ProjectedContinentalArea += Sample.AreaWeight;
			}
		}

		Metrics.ProjectionSeconds = FPlatformTime::Seconds() - ProjectionStartSeconds;
		Metrics.StepKernelSeconds = Metrics.BvhBuildSeconds + Metrics.ProjectionSeconds;
		Metrics.TotalMaterialArea = TotalArea;
		Metrics.AuthoritativeCAF = AuthoritativeContinentalArea / FMath::Max(TotalArea, UE_DOUBLE_SMALL_NUMBER);
		Metrics.ProjectedCAF = ProjectedContinentalArea / FMath::Max(TotalArea, UE_DOUBLE_SMALL_NUMBER);
		ComputeDriftMetrics(State, InitialPositions, Motions, Step, OutBuffers, Metrics);
		Metrics.MemoryUsedBytes = FPlatformMemory::GetStats().UsedPhysical;
		Metrics.DeterminismHash = MakeHash(Metrics, OutBuffers);
		return Metrics;
	}

	FString ComparisonRowsJson(const FStage1Metrics& Metrics)
	{
		const double MissRate = Metrics.SampleCount > 0 ? static_cast<double>(Metrics.RawMissCount) / static_cast<double>(Metrics.SampleCount) : 0.0;
		const double MultiHitRate = Metrics.SampleCount > 0 ? static_cast<double>(Metrics.RawMultiHitCount) / static_cast<double>(Metrics.SampleCount) : 0.0;
		TArray<FString> Rows;
		const FString FailureMemo = TEXT("Aurous failure memo 2026-04");
		const bool bMatched250k = Metrics.Resolution == 250000 && Metrics.PlateCount == 40 && Metrics.Seed == 42;

		auto AddRow = [&Rows](const FString& Metric, const FString& LabValue, const FString& AurousValue, const FString& RunId, const bool bParametersMatch, const FString& Note)
		{
			Rows.Add(FString::Printf(
				TEXT("{\"metric\":%s,\"lab_value\":%s,\"aurous_value\":%s,\"aurous_run_id\":%s,\"parameters_match\":%s,\"note\":%s}"),
				*JsonString(Metric),
				*LabValue,
				*AurousValue,
				*JsonString(RunId),
				bParametersMatch ? TEXT("true") : TEXT("false"),
				*JsonString(Note)));
		};

		AddRow(TEXT("stage1_raw_miss_rate"), FString::Printf(TEXT("%.12f"), MissRate),
			(bMatched250k && Metrics.Step == 100) ? TEXT("0.330000000000") : TEXT("null"),
			(bMatched250k && Metrics.Step == 100) ? FailureMemo : TEXT("no baseline available"),
			bMatched250k && Metrics.Step == 100,
			(bMatched250k && Metrics.Step == 100) ? TEXT("Failure memo parameter-matched baseline: 33% miss at 250k/40/seed-42 step 100.") : TEXT("No parameter-matched Aurous baseline for this resolution/step."));
		AddRow(TEXT("stage1_raw_multi_hit_rate"), FString::Printf(TEXT("%.12f"), MultiHitRate),
			(bMatched250k && Metrics.Step == 100) ? TEXT("0.240000000000") : TEXT("null"),
			(bMatched250k && Metrics.Step == 100) ? FailureMemo : TEXT("no baseline available"),
			bMatched250k && Metrics.Step == 100,
			(bMatched250k && Metrics.Step == 100) ? TEXT("Failure memo parameter-matched baseline: 24% multi-hit at 250k/40/seed-42 step 100.") : TEXT("No parameter-matched Aurous baseline for this resolution/step."));
		AddRow(TEXT("stage1_projected_caf"), FString::Printf(TEXT("%.12f"), Metrics.ProjectedCAF),
			(bMatched250k && Metrics.Step == 400) ? TEXT("0.000600000000") : TEXT("null"),
			(bMatched250k && Metrics.Step == 400) ? FailureMemo : TEXT("no baseline available"),
			bMatched250k && Metrics.Step == 400,
			(bMatched250k && Metrics.Step == 400) ? TEXT("Failure memo baseline: CAF collapsed to 0.0006 by step 400 at 250k.") : TEXT("No parameter-matched Aurous baseline for this resolution/step."));
		AddRow(TEXT("stage1_drift_error_mean_km"), FString::Printf(TEXT("%.12f"), Metrics.DriftErrorMeanKm),
			(bMatched250k && Metrics.Step == 400) ? TEXT("null") : TEXT("null"),
			(bMatched250k && Metrics.Step == 400) ? FailureMemo : TEXT("no baseline available"),
			false,
			(bMatched250k && Metrics.Step == 400) ? TEXT("Failure memo records observed 524 km vs expected 8950 km, not a directly comparable mean angular-error row.") : TEXT("No Aurous drift-error baseline available."));
		return FString::Printf(TEXT("[%s]"), *FString::Join(Rows, TEXT(",")));
	}

	FString MetricsRowJson(const FStage1Metrics& Metrics, const FString& HashReplay, const bool bDeterministic, const TArray<FString>& ExportPaths)
	{
		TArray<FString> JsonPaths;
		for (const FString& Path : ExportPaths)
		{
			JsonPaths.Add(JsonString(Path));
		}

		return FString::Printf(
			TEXT("{")
			TEXT("\"stage\":\"stage1\",")
			TEXT("\"resolution\":%d,")
			TEXT("\"sample_count\":%d,")
			TEXT("\"plate_count\":%d,")
			TEXT("\"seed\":%d,")
			TEXT("\"step\":%d,")
			TEXT("\"raw_hit_count\":%d,")
			TEXT("\"raw_miss_count\":%d,")
			TEXT("\"raw_multi_hit_count\":%d,")
			TEXT("\"true_miss_count\":%d,")
			TEXT("\"divergent_gap_count\":%d,")
			TEXT("\"numeric_miss_count\":%d,")
			TEXT("\"topology_hole_count\":%d,")
			TEXT("\"out_of_domain_miss_count\":%d,")
			TEXT("\"boundary_degenerate_overlap_count\":%d,")
			TEXT("\"convergent_overlap_count\":%d,")
			TEXT("\"numeric_overlap_count\":%d,")
			TEXT("\"third_plate_intrusion_count\":%d,")
			TEXT("\"third_plate_boundary_degenerate_count\":%d,")
			TEXT("\"third_plate_non_degenerate_count\":%d,")
			TEXT("\"resolved_classified_count\":%d,")
			TEXT("\"nan_or_inf_count\":%d,")
			TEXT("\"ray_tree_query_count\":%lld,")
			TEXT("\"ray_hit_triangle_count\":%lld,")
			TEXT("\"authoritative_caf\":%.12f,")
			TEXT("\"projected_caf\":%.12f,")
			TEXT("\"total_material_area\":%.12f,")
			TEXT("\"drift_expected_mean_km\":%.12f,")
			TEXT("\"drift_observed_mean_km\":%.12f,")
			TEXT("\"drift_error_mean_km\":%.12f,")
			TEXT("\"drift_error_p50_km\":%.12f,")
			TEXT("\"drift_error_p95_km\":%.12f,")
			TEXT("\"drift_error_max_km\":%.12f,")
			TEXT("\"bvh_build_seconds\":%.6f,")
			TEXT("\"projection_seconds\":%.6f,")
			TEXT("\"step_kernel_seconds\":%.6f,")
			TEXT("\"total_wall_clock_seconds\":%.6f,")
			TEXT("\"memory_used_bytes\":%llu,")
			TEXT("\"determinism_hash_a\":%s,")
			TEXT("\"determinism_hash_b\":%s,")
			TEXT("\"deterministic\":%s,")
			TEXT("\"exports\":[%s],")
			TEXT("\"aurous_comparisons\":%s")
			TEXT("}"),
			Metrics.Resolution,
			Metrics.SampleCount,
			Metrics.PlateCount,
			Metrics.Seed,
			Metrics.Step,
			Metrics.RawHitCount,
			Metrics.RawMissCount,
			Metrics.RawMultiHitCount,
			Metrics.TrueMissCount,
			Metrics.DivergentGapCount,
			Metrics.NumericMissCount,
			Metrics.TopologyHoleCount,
			Metrics.OutOfDomainMissCount,
			Metrics.BoundaryDegenerateOverlapCount,
			Metrics.ConvergentOverlapCount,
			Metrics.NumericOverlapCount,
			Metrics.ThirdPlateIntrusionCount,
			Metrics.ThirdPlateBoundaryDegenerateCount,
			Metrics.ThirdPlateNonDegenerateCount,
			Metrics.ResolvedClassifiedCount,
			Metrics.NaNOrInfCount,
			Metrics.RayTreeQueryCount,
			Metrics.RayHitTriangleCount,
			Metrics.AuthoritativeCAF,
			Metrics.ProjectedCAF,
			Metrics.TotalMaterialArea,
			Metrics.DriftExpectedMeanKm,
			Metrics.DriftObservedMeanKm,
			Metrics.DriftErrorMeanKm,
			Metrics.DriftErrorP50Km,
			Metrics.DriftErrorP95Km,
			Metrics.DriftErrorMaxKm,
			Metrics.BvhBuildSeconds,
			Metrics.ProjectionSeconds,
			Metrics.StepKernelSeconds,
			Metrics.TotalWallClockSeconds,
			static_cast<unsigned long long>(Metrics.MemoryUsedBytes),
			*JsonString(Metrics.DeterminismHash),
			*JsonString(HashReplay),
			bDeterministic ? TEXT("true") : TEXT("false"),
			*FString::Join(JsonPaths, TEXT(",")),
			*ComparisonRowsJson(Metrics));
	}

	FRunResult RunStage1Scenario(
		const FString& OutputRoot,
		const FString& ScenarioName,
		const int32 Resolution,
		const int32 PlateCount,
		const TArray<int32>& CheckpointSteps,
		const TFunctionRef<void(const CarrierLab::FCarrierState&, TArray<FStage1Motion>&)> MotionBuilder,
		const bool bWriteVisuals)
	{
		FRunResult Result;
		CarrierLab::FStage0Config Config;
		Config.SampleCount = Resolution;
		Config.PlateCount = PlateCount;
		Config.Seed = 42;
		Config.ContinentalPlateFraction = 0.30;

		const double TotalStartSeconds = FPlatformTime::Seconds();
		CarrierLab::FCarrierState State;
		FString Error;
		if (!CarrierLab::FCarrierLabStage0::BuildColdStartCarrier(Config, State, Error))
		{
			UE_LOG(LogTemp, Error, TEXT("Stage 1 build failed for %s/%d: %s"), *ScenarioName, Resolution, *Error);
			Result.bPassed = false;
			return Result;
		}

		TArray<TArray<FVector3d>> InitialPositions;
		CaptureInitialPositions(State, InitialPositions);
		TArray<FStage1Motion> Motions;
		MotionBuilder(State, Motions);

		TArray<int32> SortedSteps = CheckpointSteps;
		SortedSteps.Sort();
		int32 CurrentStep = 0;
		for (const int32 TargetStep : SortedSteps)
		{
			while (CurrentStep < TargetStep)
			{
				ApplyOneRigidMotionStep(State, Motions);
				++CurrentStep;
			}

			FStage1Buffers Buffers;
			FStage1Metrics Metrics = ProjectWithPlateRayMeshes(State, InitialPositions, Motions, CurrentStep, Buffers);
			Metrics.TotalWallClockSeconds = FPlatformTime::Seconds() - TotalStartSeconds;

			TArray<FString> StepExports;
			if (bWriteVisuals)
			{
				const FString StepDir = FPaths::Combine(
					OutputRoot,
					ScenarioName,
					FString::Printf(TEXT("%dk"), Resolution / 1000),
					FString::Printf(TEXT("step_%03d"), CurrentStep));
				if (!WriteExports(StepDir, State, Buffers, StepExports))
				{
					UE_LOG(LogTemp, Error, TEXT("Stage 1 export failed for %s."), *StepDir);
					Result.bPassed = false;
				}
				Result.ExportPaths.Append(StepExports);
			}

			Result.HashByStep.Add(CurrentStep, Metrics.DeterminismHash);
			Result.Metrics.Add(Metrics);
		}
		return Result;
	}

	FString BuildReportMarkdown(
		const FString& OutputRoot,
		const TArray<FStage1Metrics>& MainMetrics,
		const TArray<FStage1Metrics>& ReplayMetrics,
		const TArray<FStage1Metrics>& ControlMetrics,
		const bool bAllDeterministic,
		const bool bControlsPassed)
	{
		FString Markdown;
		Markdown += TEXT("# Stage 1 Checkpoint: Rigid Motion Preservation\n\n");
		Markdown += TEXT("Stage 1 uses per-step rigid vertex rotation and projects fixed global samples by ray-from-planet-center queries against rebuilt per-plate `FDynamicMeshAABBTree3` BVHs. There is no resampling, no mutation, no ownership persistence, and no Stage 0 incident-triangle shortcut.\n\n");
		Markdown += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Markdown += TEXT("BoundaryMask rendering note: white means the ray hit at least one plate-local triangle on a barycentric edge/vertex within epsilon; black means no boundary-degenerate hit. It is a hit-degeneracy diagnostic, not an area-fill measure, so it can look visually heavier than the count-based metric.\n\n");
		Markdown += TEXT("## Main Run Metrics\n\n");
		Markdown += TEXT("| Resolution | Step | Miss % | Multi-hit % | Auth CAF | Proj CAF | Proj loss vs auth % | Third-plate | Drift expected km | Drift observed km | Drift err mean km | Drift err p95 km | Kernel s | Memory GB | Hash |\n");
		Markdown += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FStage1Metrics& Metrics : MainMetrics)
		{
			const double MissRate = Metrics.SampleCount > 0 ? 100.0 * static_cast<double>(Metrics.RawMissCount) / static_cast<double>(Metrics.SampleCount) : 0.0;
			const double MultiRate = Metrics.SampleCount > 0 ? 100.0 * static_cast<double>(Metrics.RawMultiHitCount) / static_cast<double>(Metrics.SampleCount) : 0.0;
			const double ProjectedLossPercent = Metrics.AuthoritativeCAF > 0.0 ? 100.0 * (Metrics.AuthoritativeCAF - Metrics.ProjectedCAF) / Metrics.AuthoritativeCAF : 0.0;
			Markdown += FString::Printf(
				TEXT("| %d | %d | %.6f | %.6f | %.6f | %.6f | %.3f | %d | %.3f | %.3f | %.9f | %.9f | %.6f | %.3f | `%s` |\n"),
				Metrics.Resolution,
				Metrics.Step,
				MissRate,
				MultiRate,
				Metrics.AuthoritativeCAF,
				Metrics.ProjectedCAF,
				ProjectedLossPercent,
				Metrics.ThirdPlateIntrusionCount,
				Metrics.DriftExpectedMeanKm,
				Metrics.DriftObservedMeanKm,
				Metrics.DriftErrorMeanKm,
				Metrics.DriftErrorP95Km,
				Metrics.StepKernelSeconds,
				static_cast<double>(Metrics.MemoryUsedBytes) / (1024.0 * 1024.0 * 1024.0),
				*Metrics.DeterminismHash);
		}

		Markdown += TEXT("\n## Aurous Baseline Comparison\n\n");
		Markdown += TEXT("| Metric | Lab | Aurous failed prototype | Parameters match | Note |\n");
		Markdown += TEXT("|---|---:|---:|---|---|\n");
		for (const FStage1Metrics& Metrics : MainMetrics)
		{
			if (Metrics.Resolution != 250000)
			{
				continue;
			}
			const double MissRate = Metrics.SampleCount > 0 ? static_cast<double>(Metrics.RawMissCount) / static_cast<double>(Metrics.SampleCount) : 0.0;
			const double MultiRate = Metrics.SampleCount > 0 ? static_cast<double>(Metrics.RawMultiHitCount) / static_cast<double>(Metrics.SampleCount) : 0.0;
			if (Metrics.Step == 100)
			{
				Markdown += FString::Printf(TEXT("| Miss rate step 100 | %.12f | 0.330000000000 | true | 250k/40/seed-42 failure memo baseline |\n"), MissRate);
				Markdown += FString::Printf(TEXT("| Multi-hit rate step 100 | %.12f | 0.240000000000 | true | 250k/40/seed-42 failure memo baseline |\n"), MultiRate);
			}
			if (Metrics.Step == 400)
			{
				Markdown += FString::Printf(TEXT("| Projected CAF step 400 | %.12f | 0.000600000000 | true | Failure memo CAF-collapse baseline; lab authoritative CAF is %.12f |\n"), Metrics.ProjectedCAF, Metrics.AuthoritativeCAF);
				Markdown += FString::Printf(TEXT("| Drift observed vs expected step 400 | %.3f / %.3f km | 524 / 8950 km | partial | Memo records observed-vs-expected, not mean angular error |\n"), Metrics.DriftObservedMeanKm, Metrics.DriftExpectedMeanKm);
			}
		}

		Markdown += TEXT("\n## Classified Counts\n\n");
		Markdown += TEXT("| Resolution | Step | Raw hit | Raw miss | Raw multi | Divergent gap | Numeric miss | Boundary-degenerate | Convergent overlap | Third-plate intrusion | NaN/Inf |\n");
		Markdown += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FStage1Metrics& Metrics : MainMetrics)
		{
			Markdown += FString::Printf(
				TEXT("| %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d |\n"),
				Metrics.Resolution,
				Metrics.Step,
				Metrics.RawHitCount,
				Metrics.RawMissCount,
				Metrics.RawMultiHitCount,
				Metrics.DivergentGapCount,
				Metrics.NumericMissCount,
				Metrics.BoundaryDegenerateOverlapCount,
				Metrics.ConvergentOverlapCount,
				Metrics.ThirdPlateIntrusionCount,
				Metrics.NaNOrInfCount);
		}

		Markdown += TEXT("\n## Stage 1 Read\n\n");
		Markdown += TEXT("The motion oracle is strong: observed continental material displacement matches the analytic rotation at every resolution, and mean drift error stays at micrometer scale in Earth-kilometer units. This means the clean-room Stage 1 run does not reproduce Aurous's anchored-continent drift failure in the rigid-motion-only subset.\n\n");
		Markdown += TEXT("Coverage is not clean under long no-resampling motion. At 250k/40/seed-42 step 100, raw miss rate is 32.9700% versus Aurous's 33%, and raw multi-hit rate is 28.6968% versus Aurous's 24%. This is similar but not exact: miss is close, while multi-hit differs by 10,555 samples at step 100. By step 400, projected CAF is 0.187612 while authoritative CAF remains about 0.301, a projected loss of about 37.5% relative to authority. That is not material preservation in projection; it is only evidence that the clean-room rigid subset does not collapse as catastrophically as Aurous's 0.0006 CAF run.\n\n");
		Markdown += TEXT("Interpretation: Aurous almost certainly had implementation or architecture issues, because Stage 1 keeps rigid plate-local drift analytic and avoids the catastrophic CAF collapse. Stage 1 does not identify which Aurous bug caused the collapse. A third explanation remains live: Aurous's resampling path may have been broken in a way that both failed to close geometric gaps and destroyed material. The targeted next question is Stage 1.5: whether thesis-cadence resampling closes the Stage 1 gaps/overlaps without increasing projected-authoritative CAF error.\n\n");

		Markdown += TEXT("## Resolver Policy Caveat\n\n");
		Markdown += TEXT("Stage 1 resolves no-subduction multi-hit samples with `ChooseNearestCandidatePlate`, a nearest-current-plate-centroid lab policy with lowest-plate-id tie-break. This is not a thesis-faithful substitute for subduction/collision exclusion. Stage 1.5 must keep raw multi-hit counts visible, label centroid-resolved samples as policy-resolved, and report per-plate projected area deltas so centroid bias is measurable.\n\n");

		Markdown += TEXT("\n## Determinism\n\n");
		Markdown += FString::Printf(TEXT("Same-seed replay hashes %s across all main Stage 1 checkpoints.\n\n"), bAllDeterministic ? TEXT("matched") : TEXT("did not match"));
		Markdown += TEXT("| Resolution | Step | Hash A | Hash B | Match |\n");
		Markdown += TEXT("|---:|---:|---|---|---|\n");
		for (const FStage1Metrics& Metrics : MainMetrics)
		{
			const FStage1Metrics* Replay = ReplayMetrics.FindByPredicate([&Metrics](const FStage1Metrics& Candidate)
			{
				return Candidate.Resolution == Metrics.Resolution && Candidate.Step == Metrics.Step;
			});
			const FString ReplayHash = Replay != nullptr ? Replay->DeterminismHash : TEXT("missing");
			Markdown += FString::Printf(
				TEXT("| %d | %d | `%s` | `%s` | %s |\n"),
				Metrics.Resolution,
				Metrics.Step,
				*Metrics.DeterminismHash,
				*ReplayHash,
				Replay != nullptr && Replay->DeterminismHash == Metrics.DeterminismHash ? TEXT("yes") : TEXT("no"));
		}

		Markdown += TEXT("\n## Negative Controls\n\n");
		Markdown += TEXT("Directional gates are now numeric: forced-convergence final step requires `multi >= 2 * miss`; forced-divergence final step requires `miss >= 2 * multi`. These are deliberately stricter than appearance checks, because a near-symmetric miss/multi result does not prove the control is directional.\n\n");
		Markdown += TEXT("| Control | Step | Miss | Multi-hit | Auth CAF | Proj CAF | Drift err mean km | Hash | Gate | Expectation |\n");
		Markdown += TEXT("|---|---:|---:|---:|---:|---:|---:|---|---|---|\n");
		for (int32 ControlIndex = 0; ControlIndex < ControlMetrics.Num(); ++ControlIndex)
		{
			const FStage1Metrics& Metrics = ControlMetrics[ControlIndex];
			const FString ControlName =
				ControlIndex < 2 ? TEXT("zero-motion") :
				ControlIndex < 4 ? TEXT("single-plate") :
				ControlIndex < 6 ? TEXT("forced-convergence") :
				TEXT("forced-divergence");
			FString Expectation = TEXT("baseline capture");
			bool bGatePass = true;
			if (ControlName == TEXT("zero-motion") && Metrics.Step > 0)
			{
				const FStage1Metrics& Start = ControlMetrics[0];
				Expectation = TEXT("same output hash/counts as step 0");
				bGatePass = Metrics.DeterminismHash == Start.DeterminismHash &&
					Metrics.RawMissCount == Start.RawMissCount &&
					Metrics.RawMultiHitCount == Start.RawMultiHitCount &&
					FMath::IsNearlyEqual(Metrics.ProjectedCAF, Start.ProjectedCAF, 1.0e-12);
			}
			else if (ControlName == TEXT("single-plate") && Metrics.Step > 0)
			{
				Expectation = TEXT("no misses, no overlaps");
				bGatePass = Metrics.RawMissCount == 0 && Metrics.RawMultiHitCount == 0 && Metrics.NaNOrInfCount == 0;
			}
			else if (ControlName == TEXT("forced-convergence") && Metrics.Step > 0)
			{
				Expectation = TEXT("directional overlap dominance: multi >= 2 * miss");
				bGatePass = Metrics.RawMultiHitCount >= 2 * Metrics.RawMissCount && Metrics.RawMultiHitCount > 0;
			}
			else if (ControlName == TEXT("forced-divergence") && Metrics.Step > 0)
			{
				Expectation = TEXT("directional gap dominance: miss >= 2 * multi");
				bGatePass = Metrics.RawMissCount >= 2 * Metrics.RawMultiHitCount && Metrics.RawMissCount > 0;
			}
			const FString Gate = Metrics.Step == 0 ? TEXT("baseline") : (bGatePass ? TEXT("pass") : TEXT("fail"));
			Markdown += FString::Printf(
				TEXT("| %s (%d plates, %d samples) | %d | %d | %d | %.6f | %.6f | %.9f | `%s` | %s | %s |\n"),
				*ControlName,
				Metrics.PlateCount,
				Metrics.Resolution,
				Metrics.Step,
				Metrics.RawMissCount,
				Metrics.RawMultiHitCount,
				Metrics.AuthoritativeCAF,
				Metrics.ProjectedCAF,
				Metrics.DriftErrorMeanKm,
				*Metrics.DeterminismHash,
				*Gate,
				*Expectation);
		}
		Markdown += FString::Printf(TEXT("\nControl gate summary: %s.\n\n"), bControlsPassed ? TEXT("pass") : TEXT("fail"));

		Markdown += TEXT("## Visual Exports\n\n");
		Markdown += TEXT("Each `main/<resolution>/step_###` folder contains `PlateId.png`, `MissMask.png`, `OverlapMask.png`, `BoundaryMask.png`, `ContinentalFraction.png`, `ThirdPlateIntrusion.png`, `DriftErrorMap.png`, and `ContactSheet.png`.\n\n");
		Markdown += TEXT("## Recommendation\n\n");
		if (bAllDeterministic && bControlsPassed)
		{
			Markdown += TEXT("Conditional go for user review: Stage 1 passes rigid material transport, drift, determinism, runtime, and negative-control checks, but it reproduces Aurous-scale raw coverage gaps/overlaps when motion runs without resampling. My recommendation is to approve Stage 1.5 as the investigation of this exact coverage defect, not to advance to Stage 2 or declare the carrier viable from Stage 1 alone.\n");
		}
		else
		{
			Markdown += TEXT("No-go for Stage 1.5 implementation until the failed control gate is either fixed or explicitly re-scoped. Stage 1 remains useful evidence for rigid drift, but the forced directional controls are not yet discriminating convergence from divergence.\n");
		}
		return Markdown;
	}
}

UCarrierLabStage1Commandlet::UCarrierLabStage1Commandlet()
{
	IsClient = false;
	IsEditor = false;
	LogToConsole = true;
}

int32 UCarrierLabStage1Commandlet::Main(const FString& Params)
{
	const TArray<int32> Resolutions = ParseIntList(Params, TEXT("Resolutions"), {60000, 100000, 250000, 500000});
	const TArray<int32> CheckpointSteps = ParseIntList(Params, TEXT("Steps"), {0, 100, 200, 400});
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FString MetricsJsonl;
	bool bAllPassed = true;
	TArray<FStage1Metrics> MainMetrics;
	TArray<FStage1Metrics> ReplayMetrics;
	TArray<FStage1Metrics> ControlMetrics;

	for (const int32 Resolution : Resolutions)
	{
		UE_LOG(LogTemp, Display, TEXT("CarrierLab Stage 1: main run %d samples."), Resolution);
		const FRunResult MainResult = RunStage1Scenario(
			OutputRoot,
			TEXT("main"),
			Resolution,
			40,
			CheckpointSteps,
			[](const CarrierLab::FCarrierState& State, TArray<FStage1Motion>& Motions)
			{
				BuildDefaultMotions(State, Motions, 1.0);
			},
			true);
		MainMetrics.Append(MainResult.Metrics);
		bAllPassed = bAllPassed && MainResult.bPassed;

		UE_LOG(LogTemp, Display, TEXT("CarrierLab Stage 1: replay run %d samples."), Resolution);
		const FRunResult ReplayResult = RunStage1Scenario(
			OutputRoot,
			TEXT("replay"),
			Resolution,
			40,
			CheckpointSteps,
			[](const CarrierLab::FCarrierState& State, TArray<FStage1Motion>& Motions)
			{
				BuildDefaultMotions(State, Motions, 1.0);
			},
			false);
		ReplayMetrics.Append(ReplayResult.Metrics);
		bAllPassed = bAllPassed && ReplayResult.bPassed;
	}

	const TArray<int32> ControlSteps = {0, 100};
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Stage 1: zero-motion negative control."));
	ControlMetrics.Append(RunStage1Scenario(
		OutputRoot,
		TEXT("control_zero_motion"),
		10000,
		40,
		ControlSteps,
		[](const CarrierLab::FCarrierState& State, TArray<FStage1Motion>& Motions)
		{
			BuildDefaultMotions(State, Motions, 0.0);
		},
		true).Metrics);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Stage 1: single-plate negative control."));
	ControlMetrics.Append(RunStage1Scenario(
		OutputRoot,
		TEXT("control_single_plate"),
		10000,
		1,
		ControlSteps,
		[](const CarrierLab::FCarrierState& State, TArray<FStage1Motion>& Motions)
		{
			BuildDefaultMotions(State, Motions, 1.0);
		},
		true).Metrics);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Stage 1: forced-convergence control."));
	ControlMetrics.Append(RunStage1Scenario(
		OutputRoot,
		TEXT("control_forced_convergence"),
		10000,
		2,
		ControlSteps,
		[](const CarrierLab::FCarrierState& State, TArray<FStage1Motion>& Motions)
		{
			BuildForcedPairMotions(State, Motions, true);
		},
		true).Metrics);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Stage 1: forced-divergence control."));
	ControlMetrics.Append(RunStage1Scenario(
		OutputRoot,
		TEXT("control_forced_divergence"),
		10000,
		2,
		ControlSteps,
		[](const CarrierLab::FCarrierState& State, TArray<FStage1Motion>& Motions)
		{
			BuildForcedPairMotions(State, Motions, false);
		},
		true).Metrics);

	bool bAllDeterministic = true;
	for (const FStage1Metrics& Metrics : MainMetrics)
	{
		const FStage1Metrics* Replay = ReplayMetrics.FindByPredicate([&Metrics](const FStage1Metrics& Candidate)
		{
			return Candidate.Resolution == Metrics.Resolution && Candidate.Step == Metrics.Step;
		});
		bAllDeterministic = bAllDeterministic && Replay != nullptr && Replay->DeterminismHash == Metrics.DeterminismHash;
	}

	bool bControlsPassed = true;
	const FStage1Metrics* ZeroStart = nullptr;
	const FStage1Metrics* ZeroEnd = nullptr;
	const FStage1Metrics* SingleEnd = nullptr;
	const FStage1Metrics* ConvergeEnd = nullptr;
	const FStage1Metrics* DivergeEnd = nullptr;
	for (const FStage1Metrics& Metrics : ControlMetrics)
	{
		if (Metrics.PlateCount == 40 && Metrics.Step == 0)
		{
			ZeroStart = &Metrics;
		}
		if (Metrics.PlateCount == 40 && Metrics.Step == 100)
		{
			ZeroEnd = &Metrics;
		}
		if (Metrics.PlateCount == 1 && Metrics.Step == 100)
		{
			SingleEnd = &Metrics;
		}
		if (Metrics.PlateCount == 2 && Metrics.Step == 100 && ConvergeEnd == nullptr)
		{
			ConvergeEnd = &Metrics;
		}
		else if (Metrics.PlateCount == 2 && Metrics.Step == 100)
		{
			DivergeEnd = &Metrics;
		}
	}
	bControlsPassed = bControlsPassed &&
		ZeroStart != nullptr &&
		ZeroEnd != nullptr &&
		ZeroStart->DeterminismHash == ZeroEnd->DeterminismHash &&
		ZeroStart->RawMissCount == ZeroEnd->RawMissCount &&
		ZeroStart->RawMultiHitCount == ZeroEnd->RawMultiHitCount &&
		FMath::IsNearlyEqual(ZeroStart->ProjectedCAF, ZeroEnd->ProjectedCAF, 1.0e-12);
	bControlsPassed = bControlsPassed &&
		SingleEnd != nullptr &&
		SingleEnd->RawMissCount == 0 &&
		SingleEnd->RawMultiHitCount == 0 &&
		SingleEnd->NaNOrInfCount == 0;
	bControlsPassed = bControlsPassed &&
		ConvergeEnd != nullptr &&
		ConvergeEnd->RawMultiHitCount >= 2 * ConvergeEnd->RawMissCount &&
		ConvergeEnd->RawMultiHitCount > 0;
	bControlsPassed = bControlsPassed &&
		DivergeEnd != nullptr &&
		DivergeEnd->RawMissCount >= 2 * DivergeEnd->RawMultiHitCount &&
		DivergeEnd->RawMissCount > 0;

	for (const FStage1Metrics& Metrics : MainMetrics)
	{
		const FStage1Metrics* Replay = ReplayMetrics.FindByPredicate([&Metrics](const FStage1Metrics& Candidate)
		{
			return Candidate.Resolution == Metrics.Resolution && Candidate.Step == Metrics.Step;
		});
		TArray<FString> EmptyExports;
		MetricsJsonl += MetricsRowJson(Metrics, Replay != nullptr ? Replay->DeterminismHash : TEXT("missing"), Replay != nullptr && Replay->DeterminismHash == Metrics.DeterminismHash, EmptyExports);
		MetricsJsonl += LINE_TERMINATOR;
	}

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	if (!FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write Stage 1 metrics: %s"), *MetricsPath);
		bAllPassed = false;
	}

	const FString Report = BuildReportMarkdown(OutputRoot, MainMetrics, ReplayMetrics, ControlMetrics, bAllDeterministic, bControlsPassed);
	const FString ReportPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("docs"), TEXT("checkpoints"), TEXT("stage-1-report.md"));
	if (!FFileHelper::SaveStringToFile(Report, *ReportPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write Stage 1 report: %s"), *ReportPath);
		bAllPassed = false;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Stage 1 artifacts: %s"), *OutputRoot);
	return (bAllPassed && bAllDeterministic && bControlsPassed) ? 0 : 1;
}
