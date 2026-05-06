// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIC3Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr int32 BaselineSamples = 60000;
	constexpr int32 BaselinePlates = 40;
	constexpr int32 BaselineSeed = 42;
	constexpr int32 BaselineSteps = 32;
	constexpr int32 FixtureSamples = 10000;
	constexpr int32 FixturePlates = 2;
	constexpr double VelocityMmPerYear = 66.6666666667;
	constexpr double SeedElevationKm = 2.5;
	constexpr double TrenchDepthKm = -10.0;
	constexpr double ContinentalMaxElevationKm = 10.0;
	constexpr double EffectRadiusKm = 1800.0;
	constexpr double UpliftRateMmPerYear = 0.6;
	constexpr double ReferenceVelocityMmPerYear = 100.0;
	constexpr double EarthRadiusKm = 6371.0;
	constexpr double DeltaTimeMa = 2.0;
	constexpr double GateToleranceKm = 1.0e-8;
	constexpr TCHAR ExpectedSlice55StateHash[] = TEXT("3b4a85366dab80db");
	constexpr TCHAR ExpectedSlice55MaterialLedgerHash[] = TEXT("bc3077100ba291b4");

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

	double OracleDistanceTransfer(const double DistanceKm)
	{
		if (!FMath::IsFinite(DistanceKm) || DistanceKm < 0.0 || DistanceKm > EffectRadiusKm)
		{
			return 0.0;
		}
		const double D = DistanceKm / EffectRadiusKm;
		return FMath::Exp(3.0 * D) * FMath::Exp(-9.0 * D * D);
	}

	double OracleSpeedTransfer(const double SignedConvergenceVelocity)
	{
		if (!FMath::IsFinite(SignedConvergenceVelocity) || SignedConvergenceVelocity <= 0.0)
		{
			return 0.0;
		}
		const double LocalVelocityMmPerYear = SignedConvergenceVelocity * EarthRadiusKm / DeltaTimeMa;
		return FMath::Clamp(LocalVelocityMmPerYear / ReferenceVelocityMmPerYear, 0.0, 1.0);
	}

	double OracleReliefTransfer(const double HistoricalElevationKm)
	{
		const double NormalizedElevation = FMath::Clamp(
			(HistoricalElevationKm - TrenchDepthKm) / (ContinentalMaxElevationKm - TrenchDepthKm),
			0.0,
			1.0);
		return NormalizedElevation * NormalizedElevation;
	}

	double OracleDeltaKm(const FCarrierLabPhaseIIIC3UpliftAuditRecord& Record)
	{
		return UpliftRateMmPerYear *
			DeltaTimeMa *
			OracleDistanceTransfer(Record.DistanceKm) *
			OracleSpeedTransfer(Record.SignedConvergenceVelocity) *
			OracleReliefTransfer(Record.HistoricalElevationKm);
	}

	FVector3d RetangentAndNormalize(const FVector3d& Vector, const FVector3d& UnitPosition)
	{
		const FVector3d Tangent = Vector - UnitPosition * FVector3d::DotProduct(Vector, UnitPosition);
		const double TangentSize = Tangent.Size();
		return TangentSize > UE_DOUBLE_SMALL_NUMBER ? Tangent / TangentSize : FVector3d::ZeroVector;
	}

	FVector3d OracleFoldDirection(const FCarrierLabPhaseIIIC3UpliftAuditRecord& Record)
	{
		if (Record.RelativeFoldStep.SquaredLength() <= UE_DOUBLE_SMALL_NUMBER)
		{
			return Record.PreviousFoldDirection;
		}
		return RetangentAndNormalize(
			Record.PreviousFoldDirection + Record.RelativeFoldStep * Record.FoldInfluenceBeta,
			Record.OverUnitPosition);
	}

	double OracleFoldResidual(const FCarrierLabPhaseIIIC3UpliftAuditRecord& Record)
	{
		const FVector3d Expected = OracleFoldDirection(Record);
		const double ExpectedResidual = FMath::Max(
			FMath::Abs(Expected.X - Record.ExpectedFoldDirection.X),
			FMath::Max(
				FMath::Abs(Expected.Y - Record.ExpectedFoldDirection.Y),
				FMath::Abs(Expected.Z - Record.ExpectedFoldDirection.Z)));
		const double AppliedResidual = FMath::Max(
			FMath::Abs(Expected.X - Record.NewFoldDirection.X),
			FMath::Max(
				FMath::Abs(Expected.Y - Record.NewFoldDirection.Y),
				FMath::Abs(Expected.Z - Record.NewFoldDirection.Z)));
		return FMath::Max(ExpectedResidual, AppliedResidual);
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIC3"), Stamp);
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
				TEXT("phase-iii-slice-iiic3-report.md"));
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

	ACarrierLabVisualizationActor* SpawnActor(
		UWorld& World,
		const int32 SampleCount,
		const int32 PlateCount,
		const double ContinentalPlateFraction,
		const bool bEnableMarks,
		const bool bEnableElevationSplit,
		const bool bEnableUplift)
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
		Actor->SampleCount = SampleCount;
		Actor->PlateCount = PlateCount;
		Actor->Seed = BaselineSeed;
		Actor->ContinentalPlateFraction = ContinentalPlateFraction;
		Actor->VelocityMmPerYear = VelocityMmPerYear;
		Actor->MultiHitPolicy = ECarrierLabMultiHitPolicy::Centroid;
		Actor->VisualizationLayer = ECarrierLabVisualizationLayer::ElevationHeatmap;
		Actor->bEnablePhaseIIICSubductingMarks = bEnableMarks;
		Actor->bEnablePhaseIIICVisibleHistoricalElevation = bEnableElevationSplit;
		Actor->bEnablePhaseIIICOverridingPlateUplift = bEnableUplift;
		Actor->PhaseIIICTrenchDepthKm = TrenchDepthKm;
		Actor->PhaseIIICMaxContinentalElevationKm = ContinentalMaxElevationKm;
		Actor->PhaseIIICSubductionEffectRadiusKm = EffectRadiusKm;
		Actor->PhaseIIICSubductionUpliftMmPerYear = UpliftRateMmPerYear;
		Actor->PhaseIIICReferenceVelocityMmPerYear = ReferenceVelocityMmPerYear;
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	bool ConfigureMixedFixture(ACarrierLabVisualizationActor& Actor)
	{
		return Actor.SetPlateContinentalForTest(0, true) &&
			Actor.SetPlateContinentalForTest(1, false) &&
			Actor.SetPlateElevationForTest(1, SeedElevationKm);
	}

	struct FSlice55BypassResult
	{
		int32 Replay = 0;
		bool bCompleted = false;
		double Seconds = 0.0;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterMetrics;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerMetrics;
	};

	bool RunSlice55Bypass(const int32 Replay, FSlice55BypassResult& OutResult)
	{
		OutResult = FSlice55BypassResult();
		OutResult.Replay = Replay;
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, BaselineSamples, BaselinePlates, 0.30, false, false, false);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier())
		{
			Actor->Destroy();
			return false;
		}
		Actor->ConfigurePhaseIIMotionFixture(ECarrierLabPhaseIIMotionFixture::Default);
		for (int32 Step = 0; Step < BaselineSteps; ++Step)
		{
			Actor->StepOnce();
		}

		TArray<FCarrierLabPhaseIIContactRecord> Contacts;
		TArray<FCarrierLabPhaseIITriangleLabelRecord> Labels;
		TArray<FCarrierLabPhaseIIFilterDecisionRecord> Decisions;
		FCarrierLabPhaseIITriangleLabelConfig LabelConfig;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
		const bool bOk =
			Actor->DetectPhaseIIContacts(Contacts, ContactMetrics) &&
			Actor->BuildPhaseIITriangleLabels(Contacts, LabelConfig, Labels, LabelMetrics) &&
			Actor->ApplyPhaseIIResamplingFilterEvent(Labels, Decisions, OutResult.FilterMetrics, nullptr, &OutResult.LedgerMetrics);

		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bCompleted = bOk;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return bOk;
	}

	struct FUpliftReplayResult
	{
		FString Fixture;
		int32 Replay = 0;
		bool bCompleted = false;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIC1SubductingMarkAudit MarkAudit;
		FCarrierLabPhaseIIIC2ElevationAudit ElevationAudit;
		FCarrierLabPhaseIIIC3UpliftAudit UpliftAudit;
		double OracleDeltaSumKm = 0.0;
		double OracleMaxRecordResidualKm = 0.0;
		double FoldOracleMaxRecordResidual = 0.0;
		double MinFoldMagnitude = 0.0;
	};

	bool RunUpliftReplay(
		const FString& Fixture,
		const int32 Replay,
		const ECarrierLabPhaseIIMotionFixture MotionFixture,
		const int32 SampleCount,
		const int32 PlateCount,
		const double ContinentalFraction,
		const bool bConfigureMixed,
		const bool bEnableMarks,
		const bool bEnableElevationSplit,
		const bool bEnableUplift,
		FUpliftReplayResult& OutResult)
	{
		OutResult = FUpliftReplayResult();
		OutResult.Fixture = Fixture;
		OutResult.Replay = Replay;
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, SampleCount, PlateCount, ContinentalFraction, bEnableMarks, bEnableElevationSplit, bEnableUplift);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier() ||
			(bConfigureMixed && !ConfigureMixedFixture(*Actor)))
		{
			Actor->Destroy();
			return false;
		}

		Actor->ConfigurePhaseIIMotionFixture(MotionFixture);
		Actor->StepOnce();
		Actor->GetPhaseIIIC1SubductingMarkAudit(OutResult.MarkAudit);
		Actor->GetPhaseIIIC2ElevationAudit(OutResult.ElevationAudit);
		Actor->GetPhaseIIIC3UpliftAudit(OutResult.UpliftAudit);

		bool bInitializedFold = false;
		for (const FCarrierLabPhaseIIIC3UpliftAuditRecord& Record : OutResult.UpliftAudit.Records)
		{
			const double Expected = OracleDeltaKm(Record);
			OutResult.OracleDeltaSumKm += Expected;
			OutResult.OracleMaxRecordResidualKm = FMath::Max(
				OutResult.OracleMaxRecordResidualKm,
				FMath::Abs(Expected - Record.AppliedDeltaKm));
			OutResult.FoldOracleMaxRecordResidual = FMath::Max(
				OutResult.FoldOracleMaxRecordResidual,
				OracleFoldResidual(Record));
			if (!bInitializedFold)
			{
				OutResult.MinFoldMagnitude = Record.FoldDirectionMagnitude;
				bInitializedFold = true;
			}
			else
			{
				OutResult.MinFoldMagnitude = FMath::Min(OutResult.MinFoldMagnitude, Record.FoldDirectionMagnitude);
			}
		}

		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bCompleted = true;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return true;
	}

	bool UpliftReplayPasses(const FUpliftReplayResult& Result)
	{
		return Result.bCompleted &&
			Result.MarkAudit.MarkCount > 0 &&
			Result.ElevationAudit.SnapshotMarkCount > 0 &&
			Result.UpliftAudit.UpliftRecordCount > 0 &&
			Result.UpliftAudit.UniqueUpliftedVertexCount > 0 &&
			Result.UpliftAudit.TotalAppliedDeltaKm > 0.0 &&
			FMath::Abs(Result.OracleDeltaSumKm - Result.UpliftAudit.TotalAppliedDeltaKm) <= GateToleranceKm &&
			Result.OracleMaxRecordResidualKm <= GateToleranceKm &&
			Result.FoldOracleMaxRecordResidual <= GateToleranceKm &&
			Result.MinFoldMagnitude > 0.0;
	}

	bool NegativeReplayPasses(const FUpliftReplayResult& Result)
	{
		return Result.bCompleted &&
			Result.UpliftAudit.UpliftRecordCount == 0 &&
			Result.UpliftAudit.UniqueUpliftedVertexCount == 0 &&
			FMath::Abs(Result.UpliftAudit.TotalAppliedDeltaKm) <= GateToleranceKm;
	}

	FString UpliftJson(const FUpliftReplayResult& Result)
	{
		return FString::Printf(
			TEXT("{\"kind\":\"uplift\",\"fixture\":%s,\"replay\":%d,\"completed\":%s,\"marks\":%d,\"snapshots\":%d,\"uplift_records\":%d,\"unique_vertices\":%d,\"skipped_non_continental\":%d,\"skipped_outside_radius\":%d,\"invalid_inputs\":%d,\"total_delta_km\":%.15f,\"oracle_delta_sum_km\":%.15f,\"oracle_residual_km\":%.15f,\"fold_oracle_residual\":%.15f,\"fold_beta\":%.12f,\"max_delta_km\":%.15f,\"min_fold_magnitude\":%.15f,\"uplift_hash\":%s,\"visible_hash\":%s,\"historical_hash\":%s,\"crust_hash\":%s,\"seconds\":%.6f}"),
			*JsonString(Result.Fixture),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.MarkAudit.MarkCount,
			Result.ElevationAudit.SnapshotMarkCount,
			Result.UpliftAudit.UpliftRecordCount,
			Result.UpliftAudit.UniqueUpliftedVertexCount,
			Result.UpliftAudit.SkippedNonContinentalVertexCount,
			Result.UpliftAudit.SkippedOutsideRadiusCount,
			Result.UpliftAudit.InvalidInputCount,
			Result.UpliftAudit.TotalAppliedDeltaKm,
			Result.OracleDeltaSumKm,
			FMath::Abs(Result.OracleDeltaSumKm - Result.UpliftAudit.TotalAppliedDeltaKm),
			Result.FoldOracleMaxRecordResidual,
			Result.UpliftAudit.FoldInfluenceBeta,
			Result.UpliftAudit.MaxAppliedDeltaKm,
			Result.MinFoldMagnitude,
			*JsonString(Result.UpliftAudit.UpliftHash),
			*JsonString(Result.UpliftAudit.VisibleElevationHash),
			*JsonString(Result.UpliftAudit.HistoricalElevationHash),
			*JsonString(Result.UpliftAudit.CrustStateHash),
			Result.Seconds);
	}

	FString BypassJson(const FSlice55BypassResult& Result)
	{
		return FString::Printf(
			TEXT("{\"kind\":\"bypass\",\"replay\":%d,\"completed\":%s,\"state_hash\":%s,\"material_ledger_hash\":%s,\"persistent_mark_inputs\":%d,\"seconds\":%.6f}"),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			*JsonString(Result.FilterMetrics.StateHashAfter),
			*JsonString(Result.LedgerMetrics.MaterialLedgerHash),
			Result.FilterMetrics.PersistentSubductingMarkInputCount,
			Result.Seconds);
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FSlice55BypassResult& BypassA,
		const FSlice55BypassResult& BypassB,
		const FUpliftReplayResult& UpliftA,
		const FUpliftReplayResult& UpliftB,
		const FUpliftReplayResult& Disabled,
		const FUpliftReplayResult& ZeroMotion,
		const FUpliftReplayResult& SinglePlate,
		const FUpliftReplayResult& ForcedDivergenceNoSubduction,
		const FUpliftReplayResult& ForcedDivergenceMixedDiagnostic)
	{
		const bool bBypassPass =
			BypassA.bCompleted &&
			BypassB.bCompleted &&
			BypassA.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			BypassB.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			BypassA.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
			BypassB.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash;
		const bool bUpliftPass =
			UpliftReplayPasses(UpliftA) &&
			UpliftReplayPasses(UpliftB) &&
			UpliftA.UpliftAudit.UpliftHash == UpliftB.UpliftAudit.UpliftHash &&
			UpliftA.UpliftAudit.VisibleElevationHash == UpliftB.UpliftAudit.VisibleElevationHash &&
			UpliftA.UpliftAudit.CrustStateHash == UpliftB.UpliftAudit.CrustStateHash;
		const bool bDisabledPass = NegativeReplayPasses(Disabled) && Disabled.MarkAudit.MarkCount > 0;
		const bool bNegativePass =
			NegativeReplayPasses(ZeroMotion) &&
			NegativeReplayPasses(SinglePlate) &&
			NegativeReplayPasses(ForcedDivergenceNoSubduction);
		const bool bAllPass = bBypassPass && bUpliftPass && bDisabledPass && bNegativePass;

		FString Report;
		Report += TEXT("# Phase III Slice IIIC.3 Checkpoint\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("Status: opt-in overriding-plate uplift. This slice applies thesis section 3.3.1.3 uplift to continental over-plate vertices near IIIC subducting marks, using IIIC.2 `HistoricalElevation` on the under-plate triangle. It does not add slab pull, collision, rifting, erosion, terrain displacement, global ownership, or any new resampling mutation path.\n\n");
		Report += TEXT("Formula: `Delta z = u0 * dt * exp(3d/rs) * exp(-9d^2/rs^2) * clamp(v/v0,0,1) * ztilde^2`, with `u0=0.6 mm/yr`, `dt=2 Ma`, `rs=1800 km`, `v0=100 mm/yr`, `zt=-10 km`, `zc=10 km`, and `ztilde=(HistoricalElevation-zt)/(zc-zt)`.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		Report += FString::Printf(
			TEXT("| Bypass disabled | %s | Slice 5.5 fixed fixture state `%s` / `%s`, ledger `%s` / `%s` |\n"),
			*PassFail(bBypassPass),
			*BypassA.FilterMetrics.StateHashAfter,
			*BypassB.FilterMetrics.StateHashAfter,
			*BypassA.LedgerMetrics.MaterialLedgerHash,
			*BypassB.LedgerMetrics.MaterialLedgerHash);
		Report += FString::Printf(
			TEXT("| Forced convergence uplift | %s | records %d / %d, unique vertices %d / %d, total delta %.12f / %.12f km |\n"),
			*PassFail(bUpliftPass),
			UpliftA.UpliftAudit.UpliftRecordCount,
			UpliftB.UpliftAudit.UpliftRecordCount,
			UpliftA.UpliftAudit.UniqueUpliftedVertexCount,
			UpliftB.UpliftAudit.UniqueUpliftedVertexCount,
			UpliftA.UpliftAudit.TotalAppliedDeltaKm,
			UpliftB.UpliftAudit.TotalAppliedDeltaKm);
		Report += FString::Printf(
			TEXT("| Independent formula oracle | %s | oracle residual %.12e / %.12e km, max record residual %.12e / %.12e km |\n"),
			*PassFail(bUpliftPass),
			FMath::Abs(UpliftA.OracleDeltaSumKm - UpliftA.UpliftAudit.TotalAppliedDeltaKm),
			FMath::Abs(UpliftB.OracleDeltaSumKm - UpliftB.UpliftAudit.TotalAppliedDeltaKm),
			UpliftA.OracleMaxRecordResidualKm,
			UpliftB.OracleMaxRecordResidualKm);
		Report += FString::Printf(
			TEXT("| Fold-direction oracle | %s | beta %.6f / %.6f, max vector residual %.12e / %.12e |\n"),
			*PassFail(bUpliftPass),
			UpliftA.UpliftAudit.FoldInfluenceBeta,
			UpliftB.UpliftAudit.FoldInfluenceBeta,
			UpliftA.FoldOracleMaxRecordResidual,
			UpliftB.FoldOracleMaxRecordResidual);
		Report += FString::Printf(
			TEXT("| Same-seed replay | %s | uplift hash `%s` / `%s`, visible hash `%s` / `%s` |\n"),
			*PassFail(bUpliftPass),
			*UpliftA.UpliftAudit.UpliftHash,
			*UpliftB.UpliftAudit.UpliftHash,
			*UpliftA.UpliftAudit.VisibleElevationHash,
			*UpliftB.UpliftAudit.VisibleElevationHash);
		Report += FString::Printf(
			TEXT("| Uplift opt-in disabled | %s | marks %d, uplift records %d, total delta %.12f km |\n"),
			*PassFail(bDisabledPass),
			Disabled.MarkAudit.MarkCount,
			Disabled.UpliftAudit.UpliftRecordCount,
			Disabled.UpliftAudit.TotalAppliedDeltaKm);
		Report += FString::Printf(
			TEXT("| Negative controls | %s | zero %d, single %d, divergence-no-subduction %d uplift records |\n"),
			*PassFail(bNegativePass),
			ZeroMotion.UpliftAudit.UpliftRecordCount,
			SinglePlate.UpliftAudit.UpliftRecordCount,
			ForcedDivergenceNoSubduction.UpliftAudit.UpliftRecordCount);

		Report += TEXT("\n## Primary Forced-Convergence Replay\n\n");
		Report += TEXT("| Replay | Marks | Snapshots | Uplift records | Unique vertices | Total delta km | Oracle delta km | Max delta km | Min fold | Fold residual | Uplift hash | Visible hash | Crust hash | Seconds |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|---:|\n");
		auto AddUpliftRow = [&Report](const FUpliftReplayResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %d | %d | %d | %d | %d | %.12f | %.12f | %.12f | %.12f | %.12e | `%s` | `%s` | `%s` | %.3f |\n"),
				Result.Replay,
				Result.MarkAudit.MarkCount,
				Result.ElevationAudit.SnapshotMarkCount,
				Result.UpliftAudit.UpliftRecordCount,
				Result.UpliftAudit.UniqueUpliftedVertexCount,
				Result.UpliftAudit.TotalAppliedDeltaKm,
				Result.OracleDeltaSumKm,
				Result.UpliftAudit.MaxAppliedDeltaKm,
				Result.MinFoldMagnitude,
				Result.FoldOracleMaxRecordResidual,
				*Result.UpliftAudit.UpliftHash,
				*Result.UpliftAudit.VisibleElevationHash,
				*Result.UpliftAudit.CrustStateHash,
				Result.Seconds);
		};
		AddUpliftRow(UpliftA);
		AddUpliftRow(UpliftB);

		Report += TEXT("\n## Representative Uplift Records\n\n");
		Report += TEXT("| Mark | Under | Over | Under tri | Over vertex | Distance km | Signed velocity | Historical km | Delta km | Distance f | Speed g | Relief h | Fold beta | Fold residual | Fold |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		for (int32 Index = 0; Index < FMath::Min(16, UpliftA.UpliftAudit.Records.Num()); ++Index)
		{
			const FCarrierLabPhaseIIIC3UpliftAuditRecord& Record = UpliftA.UpliftAudit.Records[Index];
			Report += FString::Printf(
				TEXT("| %d | %d | %d | %d | %d | %.3f | %.12f | %.3f | %.12f | %.6f | %.6f | %.6f | %.6f | %.12e | %.6f |\n"),
				Record.MarkId,
				Record.UnderPlateId,
				Record.OverPlateId,
				Record.UnderLocalTriangleId,
				Record.OverLocalVertexId,
				Record.DistanceKm,
				Record.SignedConvergenceVelocity,
				Record.HistoricalElevationKm,
				Record.AppliedDeltaKm,
				Record.DistanceTransfer,
				Record.SpeedTransfer,
				Record.ReliefTransfer,
				Record.FoldInfluenceBeta,
				OracleFoldResidual(Record),
				Record.FoldDirectionMagnitude);
		}

		Report += TEXT("\n## Negative Controls\n\n");
		Report += TEXT("| Fixture | Marks | Snapshots | Uplift records | Total delta km | Uplift hash | Visible hash | Result |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---|---|---|\n");
		auto AddNegativeRow = [&Report](const FUpliftReplayResult& Result, const bool bPass)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %.12f | `%s` | `%s` | %s |\n"),
				*Result.Fixture,
				Result.MarkAudit.MarkCount,
				Result.ElevationAudit.SnapshotMarkCount,
				Result.UpliftAudit.UpliftRecordCount,
				Result.UpliftAudit.TotalAppliedDeltaKm,
				*Result.UpliftAudit.UpliftHash,
				*Result.UpliftAudit.VisibleElevationHash,
				*PassFail(bPass));
		};
		AddNegativeRow(Disabled, bDisabledPass);
		AddNegativeRow(ZeroMotion, NegativeReplayPasses(ZeroMotion));
		AddNegativeRow(SinglePlate, NegativeReplayPasses(SinglePlate));
		AddNegativeRow(ForcedDivergenceNoSubduction, NegativeReplayPasses(ForcedDivergenceNoSubduction));

		Report += TEXT("\n## Non-Gate Closed-Sphere Divergence Diagnostic\n\n");
		Report += TEXT("A two-plate forced-divergence mixed-material fixture can still produce backside local convergence after motion on a closed sphere. IIIC.3 keeps that evidence visible, but does not use it as the no-uplift negative gate because the IIIB mark path is correctly local, not pair-global.\n\n");
		Report += TEXT("| Fixture | Marks | Snapshots | Uplift records | Total delta km | Uplift hash | Visible hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---|---|\n");
		Report += FString::Printf(
			TEXT("| %s | %d | %d | %d | %.12f | `%s` | `%s` |\n"),
			*ForcedDivergenceMixedDiagnostic.Fixture,
			ForcedDivergenceMixedDiagnostic.MarkAudit.MarkCount,
			ForcedDivergenceMixedDiagnostic.ElevationAudit.SnapshotMarkCount,
			ForcedDivergenceMixedDiagnostic.UpliftAudit.UpliftRecordCount,
			ForcedDivergenceMixedDiagnostic.UpliftAudit.TotalAppliedDeltaKm,
			*ForcedDivergenceMixedDiagnostic.UpliftAudit.UpliftHash,
			*ForcedDivergenceMixedDiagnostic.UpliftAudit.VisibleElevationHash);

		Report += TEXT("\n## Scope Notes\n\n");
		Report += TEXT("- Uplift mutates plate-local over-plate vertices only; global TDS samples remain projection/resampling targets, not persistent authority.\n");
		Report += TEXT("- The independent oracle recomputes the uplift formula from raw record distance, signed convergence velocity, and historical elevation fields.\n");
		Report += TEXT("- The fold-direction oracle recomputes thesis page 59's `f_j(t+dt)=f_j(t)+beta*(s_i-s_j)*dt` update from the previous fold vector, raw tangent relative step vector, and configured beta; it is not scaled by uplift delta.\n");
		Report += TEXT("- The speed transfer is clamped to `[0,1]` because thesis Table 3.2 defines `v0` as the maximum authorized plate speed and Figure 37 normalizes `g(v)` at `v0`.\n");
		Report += TEXT("- This checkpoint may claim only IIIC.3 overriding-plate uplift behavior. It does not claim slab pull, collision, rifting, erosion, Stage 1.5 carrier success, or Slice 5.5 asymmetry resolution.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIC.3 passes. Pause for user review before IIIC.4 slab-pull work.\n")
			: TEXT("IIIC.3 does not pass. Investigate the failed gate before any IIIC.4 work.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIC3Commandlet::UCarrierLabPhaseIIIC3Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIC3Commandlet::Main(const FString& Params)
{
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FSlice55BypassResult BypassA;
	FSlice55BypassResult BypassB;
	const bool bBypassA = RunSlice55Bypass(0, BypassA);
	const bool bBypassB = RunSlice55Bypass(1, BypassB);

	FUpliftReplayResult UpliftA;
	FUpliftReplayResult UpliftB;
	const bool bUpliftA = RunUpliftReplay(
		TEXT("forced_convergence_uplift"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		true,
		true,
		UpliftA);
	const bool bUpliftB = RunUpliftReplay(
		TEXT("forced_convergence_uplift"),
		1,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		true,
		true,
		UpliftB);

	FUpliftReplayResult Disabled;
	const bool bDisabled = RunUpliftReplay(
		TEXT("uplift_disabled"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		true,
		false,
		Disabled);

	FUpliftReplayResult ZeroMotion;
	const bool bZero = RunUpliftReplay(
		TEXT("zero_motion"),
		0,
		ECarrierLabPhaseIIMotionFixture::Zero,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		true,
		true,
		ZeroMotion);
	FUpliftReplayResult SinglePlate;
	const bool bSingle = RunUpliftReplay(
		TEXT("single_plate"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		1,
		1.0,
		false,
		true,
		true,
		true,
		SinglePlate);
	FUpliftReplayResult ForcedDivergence;
	const bool bDivergence = RunUpliftReplay(
		TEXT("forced_divergence_no_subduction"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedDivergence,
		FixtureSamples,
		FixturePlates,
		1.0,
		false,
		true,
		true,
		true,
		ForcedDivergence);
	FUpliftReplayResult ForcedDivergenceMixedDiagnostic;
	const bool bDivergenceDiagnostic = RunUpliftReplay(
		TEXT("forced_divergence_mixed_backside_diagnostic"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedDivergence,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		true,
		true,
		ForcedDivergenceMixedDiagnostic);

	TArray<FString> JsonLines;
	JsonLines.Add(BypassJson(BypassA));
	JsonLines.Add(BypassJson(BypassB));
	JsonLines.Add(UpliftJson(UpliftA));
	JsonLines.Add(UpliftJson(UpliftB));
	JsonLines.Add(UpliftJson(Disabled));
	JsonLines.Add(UpliftJson(ZeroMotion));
	JsonLines.Add(UpliftJson(SinglePlate));
	JsonLines.Add(UpliftJson(ForcedDivergence));
	JsonLines.Add(UpliftJson(ForcedDivergenceMixedDiagnostic));

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(FString::Join(JsonLines, TEXT("\n")) + TEXT("\n"), *MetricsPath);

	const FString Report = BuildReport(OutputRoot, BypassA, BypassB, UpliftA, UpliftB, Disabled, ZeroMotion, SinglePlate, ForcedDivergence, ForcedDivergenceMixedDiagnostic);
	const FString ReportPath = ResolveReportPath(Params);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	FFileHelper::SaveStringToFile(Report, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	const bool bBypassPass =
		bBypassA &&
		bBypassB &&
		BypassA.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
		BypassB.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
		BypassA.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
		BypassB.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash;
	const bool bUpliftPass =
		bUpliftA &&
		bUpliftB &&
		UpliftReplayPasses(UpliftA) &&
		UpliftReplayPasses(UpliftB) &&
		UpliftA.UpliftAudit.UpliftHash == UpliftB.UpliftAudit.UpliftHash &&
		UpliftA.UpliftAudit.VisibleElevationHash == UpliftB.UpliftAudit.VisibleElevationHash &&
		UpliftA.UpliftAudit.CrustStateHash == UpliftB.UpliftAudit.CrustStateHash;
	const bool bDisabledPass = bDisabled && NegativeReplayPasses(Disabled) && Disabled.MarkAudit.MarkCount > 0;
	const bool bNegativePass =
		bZero &&
		bSingle &&
		bDivergence &&
		bDivergenceDiagnostic &&
		NegativeReplayPasses(ZeroMotion) &&
		NegativeReplayPasses(SinglePlate) &&
		NegativeReplayPasses(ForcedDivergence);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIIC.3 report: %s"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIIC.3 metrics: %s"), *MetricsPath);
	return (bBypassPass && bUpliftPass && bDisabledPass && bNegativePass) ? 0 : 1;
}
