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
	constexpr int32 ProfileWidth = 1024;
	constexpr int32 ProfileHeight = 512;

	enum class EObservabilityMaterialFixture : uint8
	{
		Default,
		MixedPlate0Continental
	};

	enum class EObservabilityExpectedState : uint8
	{
		Informational,
		NoProcessSignals,
		ProcessSignalsRequired
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
		EObservabilityExpectedState ExpectedState = EObservabilityExpectedState::Informational;
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
		case ECarrierLabVisualizationLayer::PhaseIIISummary:
			return TEXT("PhaseIIISummary");
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

	FString ExpectedStateName(const EObservabilityExpectedState ExpectedState)
	{
		switch (ExpectedState)
		{
		case EObservabilityExpectedState::NoProcessSignals:
			return TEXT("no_process_signals");
		case EObservabilityExpectedState::ProcessSignalsRequired:
			return TEXT("process_signals_required");
		case EObservabilityExpectedState::Informational:
		default:
			return TEXT("informational");
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
			FObservabilityScenario ZeroMotion;
			ZeroMotion.Name = TEXT("zero_motion_control");
			ZeroMotion.Description = TEXT("Zero-motion control with IIIC process layer enabled. It should remain spatially blank for subduction, trench, and uplift signals.");
			ZeroMotion.SampleCount = 10000;
			ZeroMotion.PlateCount = 2;
			ZeroMotion.StepCount = 1;
			ZeroMotion.ContinentalPlateFraction = 0.50;
			ZeroMotion.MotionFixture = ECarrierLabPhaseIIMotionFixture::Zero;
			ZeroMotion.MaterialFixture = EObservabilityMaterialFixture::MixedPlate0Continental;
			ZeroMotion.bSeedMixedElevation = true;
			ZeroMotion.ExpectedState = EObservabilityExpectedState::NoProcessSignals;
			ZeroMotion.bEnableIIICProcessLayer = true;
			ZeroMotion.bEnableIIICSlabPull = false;
			Scenarios.Add(ZeroMotion);

			FObservabilityScenario SinglePlate;
			SinglePlate.Name = TEXT("single_plate_control");
			SinglePlate.Description = TEXT("Single-plate control with IIIC process layer enabled. No plate-pair contacts exist, so process overlays should stay blank.");
			SinglePlate.SampleCount = 10000;
			SinglePlate.PlateCount = 1;
			SinglePlate.StepCount = 1;
			SinglePlate.ContinentalPlateFraction = 1.0;
			SinglePlate.MotionFixture = ECarrierLabPhaseIIMotionFixture::Zero;
			SinglePlate.MaterialFixture = EObservabilityMaterialFixture::Default;
			SinglePlate.ExpectedState = EObservabilityExpectedState::NoProcessSignals;
			SinglePlate.bEnableIIICProcessLayer = true;
			SinglePlate.bEnableIIICSlabPull = false;
			Scenarios.Add(SinglePlate);

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
			ProcessLayer.ExpectedState = EObservabilityExpectedState::ProcessSignalsRequired;
			ProcessLayer.bEnableIIICProcessLayer = true;
			ProcessLayer.bEnableIIICSlabPull = false;
			Scenarios.Add(ProcessLayer);

			FObservabilityScenario DefaultProcess;
			DefaultProcess.Name = TEXT("default_40_plate_process");
			DefaultProcess.Description = TEXT("Default 40-plate spatial sanity run with the consolidated IIIC process layer enabled and slab pull off. This is a human-inspection map, not a hard morphology gate.");
			DefaultProcess.SampleCount = 60000;
			DefaultProcess.PlateCount = 40;
			DefaultProcess.StepCount = 40;
			DefaultProcess.ContinentalPlateFraction = 0.30;
			DefaultProcess.MotionFixture = ECarrierLabPhaseIIMotionFixture::Default;
			DefaultProcess.MaterialFixture = EObservabilityMaterialFixture::Default;
			DefaultProcess.ExpectedState = EObservabilityExpectedState::Informational;
			DefaultProcess.bEnableIIICProcessLayer = true;
			DefaultProcess.bEnableIIICSlabPull = false;
			Scenarios.Add(DefaultProcess);
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

	void SetPixelSafe(TArray<FColor>& Pixels, const int32 Width, const int32 Height, const int32 X, const int32 Y, const FColor& Color)
	{
		if (X >= 0 && X < Width && Y >= 0 && Y < Height)
		{
			Pixels[Y * Width + X] = Color;
		}
	}

	void DrawDisk(TArray<FColor>& Pixels, const int32 Width, const int32 Height, const int32 CenterX, const int32 CenterY, const int32 Radius, const FColor& Color)
	{
		for (int32 Y = -Radius; Y <= Radius; ++Y)
		{
			for (int32 X = -Radius; X <= Radius; ++X)
			{
				if (X * X + Y * Y <= Radius * Radius)
				{
					SetPixelSafe(Pixels, Width, Height, CenterX + X, CenterY + Y, Color);
				}
			}
		}
	}

	void DrawLine(TArray<FColor>& Pixels, const int32 Width, const int32 Height, int32 X0, int32 Y0, const int32 X1, const int32 Y1, const FColor& Color)
	{
		const int32 Dx = FMath::Abs(X1 - X0);
		const int32 Sx = X0 < X1 ? 1 : -1;
		const int32 Dy = -FMath::Abs(Y1 - Y0);
		const int32 Sy = Y0 < Y1 ? 1 : -1;
		int32 Error = Dx + Dy;

		for (;;)
		{
			SetPixelSafe(Pixels, Width, Height, X0, Y0, Color);
			if (X0 == X1 && Y0 == Y1)
			{
				break;
			}
			const int32 E2 = 2 * Error;
			if (E2 >= Dy)
			{
				Error += Dy;
				X0 += Sx;
			}
			if (E2 <= Dx)
			{
				Error += Dx;
				Y0 += Sy;
			}
		}
	}

	bool ForwardMollweidePixel(const FVector3d& UnitPosition, const int32 Width, const int32 Height, int32& OutX, int32& OutY)
	{
		if (!FMath::IsFinite(UnitPosition.X) || !FMath::IsFinite(UnitPosition.Y) || !FMath::IsFinite(UnitPosition.Z))
		{
			return false;
		}

		const FVector3d P = UnitPosition.GetSafeNormal();
		const double Lon = FMath::Atan2(P.Y, P.X);
		const double Lat = FMath::Asin(FMath::Clamp(P.Z, -1.0, 1.0));
		double Theta = Lat;
		for (int32 Iteration = 0; Iteration < 10; ++Iteration)
		{
			const double Sin2Theta = FMath::Sin(2.0 * Theta);
			const double Cos2Theta = FMath::Cos(2.0 * Theta);
			const double F = 2.0 * Theta + Sin2Theta - UE_DOUBLE_PI * FMath::Sin(Lat);
			const double Df = 2.0 + 2.0 * Cos2Theta;
			if (FMath::Abs(Df) <= 1.0e-12)
			{
				break;
			}
			Theta -= F / Df;
		}

		const double Sqrt2 = FMath::Sqrt(2.0);
		const double ProjectedX = 2.0 * Sqrt2 / UE_DOUBLE_PI * Lon * FMath::Cos(Theta);
		const double ProjectedY = Sqrt2 * FMath::Sin(Theta);
		const double U = (ProjectedX + 2.0 * Sqrt2) / (4.0 * Sqrt2);
		const double V = (Sqrt2 - ProjectedY) / (2.0 * Sqrt2);
		OutX = FMath::RoundToInt(FMath::Clamp(U, 0.0, 1.0) * static_cast<double>(Width - 1));
		OutY = FMath::RoundToInt(FMath::Clamp(V, 0.0, 1.0) * static_cast<double>(Height - 1));
		return true;
	}

	void DrawArrow(TArray<FColor>& Pixels, const int32 Width, const int32 Height, const FVector3d& Start, const FVector3d& End, const FColor& Color)
	{
		int32 X0 = 0;
		int32 Y0 = 0;
		int32 X1 = 0;
		int32 Y1 = 0;
		if (!ForwardMollweidePixel(Start, Width, Height, X0, Y0) || !ForwardMollweidePixel(End, Width, Height, X1, Y1))
		{
			return;
		}

		DrawLine(Pixels, Width, Height, X0, Y0, X1, Y1, Color);
		DrawDisk(Pixels, Width, Height, X0, Y0, 2, Color);
		const FVector2D Direction(static_cast<double>(X1 - X0), static_cast<double>(Y1 - Y0));
		if (Direction.SizeSquared() <= 1.0)
		{
			return;
		}
		const FVector2D Unit = Direction.GetSafeNormal();
		const FVector2D Perp(-Unit.Y, Unit.X);
		const FVector2D Tip(static_cast<double>(X1), static_cast<double>(Y1));
		const FVector2D Left = Tip - Unit * 10.0 + Perp * 5.0;
		const FVector2D Right = Tip - Unit * 10.0 - Perp * 5.0;
		DrawLine(Pixels, Width, Height, X1, Y1, FMath::RoundToInt(Left.X), FMath::RoundToInt(Left.Y), Color);
		DrawLine(Pixels, Width, Height, X1, Y1, FMath::RoundToInt(Right.X), FMath::RoundToInt(Right.Y), Color);
	}

	void DrawMotionArrows(const ACarrierLabVisualizationActor& Actor, TArray<FColor>& Pixels, const int32 Width, const int32 Height)
	{
		constexpr double ArrowAngularLength = 0.13;
		const FColor ArrowColor(255, 36, 36, 255);
		for (int32 PlateId = 0; PlateId < Actor.CurrentMetrics.PlateCount; ++PlateId)
		{
			FCarrierLabVisualizationMotion Motion;
			if (!Actor.GetPhaseIIMotion(PlateId, Motion) ||
				FMath::Abs(Motion.AngularSpeedRadiansPerStep) <= UE_DOUBLE_SMALL_NUMBER)
			{
				continue;
			}
			const FVector3d Tangent = FVector3d::CrossProduct(Motion.Axis, Motion.CurrentCenter);
			if (Tangent.SquaredLength() <= UE_DOUBLE_SMALL_NUMBER)
			{
				continue;
			}
			const FVector3d End = (Motion.CurrentCenter + Tangent.GetSafeNormal() * ArrowAngularLength).GetSafeNormal();
			DrawArrow(Pixels, Width, Height, Motion.CurrentCenter, End, ArrowColor);
		}
	}

	bool BuildElevationProfile(
		const FCarrierLabPhaseIIIC3UpliftAudit& Audit,
		TArray<FColor>& OutPixels,
		int32& OutWidth,
		int32& OutHeight)
	{
		OutWidth = ProfileWidth;
		OutHeight = ProfileHeight;
		OutPixels.Init(FColor(5, 8, 12, 255), OutWidth * OutHeight);

		const int32 Left = 72;
		const int32 Right = OutWidth - 32;
		const int32 Top = 32;
		const int32 Bottom = OutHeight - 56;
		const FColor AxisColor(190, 205, 220, 255);
		DrawLine(OutPixels, OutWidth, OutHeight, Left, Bottom, Right, Bottom, AxisColor);
		DrawLine(OutPixels, OutWidth, OutHeight, Left, Bottom, Left, Top, AxisColor);

		const double MaxDistanceKm = FMath::Max(1.0, Audit.EffectRadiusKm);
		double MaxDeltaKm = 0.0;
		for (const FCarrierLabPhaseIIIC3UpliftAuditRecord& Record : Audit.Records)
		{
			MaxDeltaKm = FMath::Max(MaxDeltaKm, FMath::Abs(Record.AppliedDeltaKm));
		}
		MaxDeltaKm = FMath::Max(MaxDeltaKm, 1.0e-9);

		const FColor CurveColor(110, 190, 255, 255);
		int32 PreviousX = INDEX_NONE;
		int32 PreviousY = INDEX_NONE;
		for (int32 Index = 0; Index <= 240; ++Index)
		{
			const double DistanceKm = MaxDistanceKm * static_cast<double>(Index) / 240.0;
			const double RadiusAlpha = DistanceKm / MaxDistanceKm;
			const double Transfer = FMath::Exp(3.0 * RadiusAlpha) * FMath::Exp(-9.0 * RadiusAlpha * RadiusAlpha);
			const int32 X = Left + FMath::RoundToInt((Right - Left) * DistanceKm / MaxDistanceKm);
			const int32 Y = Bottom - FMath::RoundToInt((Bottom - Top) * FMath::Clamp(Transfer / 1.3, 0.0, 1.0));
			if (PreviousX != INDEX_NONE)
			{
				DrawLine(OutPixels, OutWidth, OutHeight, PreviousX, PreviousY, X, Y, CurveColor);
			}
			PreviousX = X;
			PreviousY = Y;
		}

		const FColor PointColor(255, 190, 65, 255);
		for (const FCarrierLabPhaseIIIC3UpliftAuditRecord& Record : Audit.Records)
		{
			const int32 X = Left + FMath::RoundToInt((Right - Left) * FMath::Clamp(Record.DistanceKm / MaxDistanceKm, 0.0, 1.0));
			const int32 Y = Bottom - FMath::RoundToInt((Bottom - Top) * FMath::Clamp(FMath::Abs(Record.AppliedDeltaKm) / MaxDeltaKm, 0.0, 1.0));
			DrawDisk(OutPixels, OutWidth, OutHeight, X, Y, 2, PointColor);
		}
		return true;
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
		FString ExpectedState;
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
		bool bExpectedSignalsPass = false;
		bool bOverallPass = false;
	};

	bool ReplayMatchesExpectedSignals(const FObservabilityScenario& Scenario, const FObservabilityReplay& Replay)
	{
		switch (Scenario.ExpectedState)
		{
		case EObservabilityExpectedState::NoProcessSignals:
			return Replay.IIICMarkAudit.MarkCount == 0 &&
				Replay.IIICLedgerAudit.RecordCount == 0 &&
				Replay.IIICLedgerAudit.TrenchRecordCount == 0 &&
				Replay.IIICLedgerAudit.UpliftRecordCount == 0;
		case EObservabilityExpectedState::ProcessSignalsRequired:
			return Replay.IIICMarkAudit.MarkCount > 0 &&
				Replay.IIICLedgerAudit.TrenchRecordCount > 0 &&
				Replay.IIICLedgerAudit.UpliftRecordCount > 0 &&
				FMath::Abs(Replay.IIICLedgerAudit.VisibleElevationResidualKm) <= 1.0e-8;
		case EObservabilityExpectedState::Informational:
		default:
			return true;
		}
	}

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
		const FCarrierLabPhaseIIIC3UpliftAudit& UpliftAudit,
		TArray<FLayerExport>& OutLayerExports,
		FString& OutContactSheetPath)
	{
		IFileManager::Get().MakeDirectory(*ReplayDir, true);
		const ECarrierLabVisualizationLayer Layers[] =
		{
			ECarrierLabVisualizationLayer::PhaseIIISummary,
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
			if (Layer == ECarrierLabVisualizationLayer::PhaseIIISummary)
			{
				DrawMotionArrows(Actor, Image.Pixels, Image.Width, Image.Height);
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

		FLayerPixels ProfileImage;
		ProfileImage.Export.Layer = ECarrierLabVisualizationLayer::PhaseIIISummary;
		ProfileImage.Export.Name = TEXT("ElevationProfile");
		if (!BuildElevationProfile(UpliftAudit, ProfileImage.Pixels, ProfileImage.Width, ProfileImage.Height))
		{
			return false;
		}
		ProfileImage.Export.Path = FPaths::Combine(ReplayDir, ProfileImage.Export.Name + TEXT(".png"));
		ProfileImage.Export.Hash = HashToString(HashPixels(ProfileImage.Pixels));
		ProfileImage.Export.NonBackgroundPixelCount = CountNonBackgroundPixels(ProfileImage.Pixels);
		if (!SavePng(ProfileImage.Export.Path, ProfileImage.Pixels, ProfileImage.Width, ProfileImage.Height))
		{
			return false;
		}
		Images.Add(MoveTemp(ProfileImage));

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
		OutResult.ExpectedState = ExpectedStateName(Scenario.ExpectedState);
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
		if (!WriteMaps(ReplayDir, *Actor, OutResult.IIICUpliftAudit, OutResult.LayerExports, OutResult.ContactSheetPath))
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
			TEXT("{\"scenario\":%s,\"expected_state\":%s,\"replay\":%d,\"completed\":%s,\"total_seconds\":%.6f,")
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
			*JsonString(Result.ExpectedState),
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
				TEXT("- `%s`: %s (`%dk / %d plates / seed %d / %d rigid steps / %s motion / %s material / IIIC process %s / slab pull %s / expected %s / centroid policy`).\n"),
				*Result.Scenario.Name,
				*Result.Scenario.Description,
				Result.Scenario.SampleCount / 1000,
				Result.Scenario.PlateCount,
				ObservabilitySeed,
				Result.Scenario.StepCount,
				*MotionFixtureName(Result.Scenario.MotionFixture),
				*MaterialFixtureName(Result.Scenario.MaterialFixture),
				Result.Scenario.bEnableIIICProcessLayer ? TEXT("on") : TEXT("off"),
				Result.Scenario.bEnableIIICSlabPull ? TEXT("on") : TEXT("off"),
				*ExpectedStateName(Result.Scenario.ExpectedState));
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
			Report += FString::Printf(
				TEXT("| `%s` expected spatial signal | %s | expected `%s` |\n"),
				*Result.Scenario.Name,
				*PassFail(Result.bExpectedSignalsPass),
				*ExpectedStateName(Result.Scenario.ExpectedState));
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
		Report += TEXT("- `PhaseIIISummary` is the human-inspection layer: filled crust type, plate-boundary emphasis, velocity arrows in PNG exports, IIIB distance context, IIIC subduction roles, and IIIC elevation overlays in one map. It is still a color diagnostic on a unit sphere, not terrain displacement.\n");
		Report += bIIICMode
			? TEXT("- `ElevationHeatmap` should show the IIIC.2 trench split and IIIC.3 overriding uplift as scalar-field color overlays on the filled continental/oceanic base map.\n")
			: TEXT("- `ElevationHeatmap` uses the filled continental/oceanic base map when elevation is still zero, then overlays positive/negative elevation once IIIC.2/IIIC.3 mutate the scalar field.\n");
		Report += bIIICMode
			? TEXT("- `SubductionMask` should show the consolidated IIIC subducting/overriding roles produced from persistent plate-local marks, not only the earlier pre-mutation IIIB inspection overlay.\n")
			: TEXT("- `SubductionMask` uses the filled base map plus IIIB polarity-derived role overlays; the forced-convergence fixture is the human-inspection map, not persistent IIIC subducting-triangle authority.\n");
		Report += bIIICMode
			? TEXT("- `DistanceToFrontHeatmap` remains the front-distance spatial context for the same fixture; it is diagnostic context, not a source of authority.\n\n")
			: TEXT("- `DistanceToFrontHeatmap` uses the filled base map plus active boundary distance overlays; the default baseline may be sparse, while the forced fixture intentionally exercises propagated front state.\n\n");
		Report += TEXT("- `ElevationProfile` plots uplift delta against distance-to-front and includes the expected thesis distance-transfer curve as a visual shape reference. It is paired with the IIIC.3 numeric oracle; the plot alone is not a gate.\n\n");
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
		Result.bExpectedSignalsPass = Result.bRanA && Result.bRanB &&
			ReplayMatchesExpectedSignals(Scenario, Result.A) &&
			ReplayMatchesExpectedSignals(Scenario, Result.B);
		Result.bOverallPass = Result.bRanA && Result.bRanB && Result.A.bExportReadOnly && Result.B.bExportReadOnly && Result.bLayerHashesStable && Result.bExpectedSignalsPass;
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
