// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIC5Commandlet.h"

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
	constexpr double GateToleranceKm = 1.0e-8;
	constexpr TCHAR ExpectedSlice55StateHash[] = TEXT("3b4a85366dab80db");
	constexpr TCHAR ExpectedSlice55MaterialLedgerHash[] = TEXT("bc3077100ba291b4");
	constexpr TCHAR ExpectedIIIBIndependentSignature[] = TEXT("bf8818a26ed7b1dc");

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

	FString LedgerClassName(const ECarrierLabPhaseIIIC5ElevationLedgerClass LedgerClass)
	{
		switch (LedgerClass)
		{
		case ECarrierLabPhaseIIIC5ElevationLedgerClass::TrenchVisibleElevation:
			return TEXT("trench_visible_elevation_delta");
		case ECarrierLabPhaseIIIC5ElevationLedgerClass::OverridingUplift:
			return TEXT("overriding_uplift_delta");
		default:
			return TEXT("unknown");
		}
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIC5"), Stamp);
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
				TEXT("phase-iii-slice-iiic5-report.md"));
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
		const bool bEnableUplift,
		const bool bEnableSlabPull)
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
		Actor->bEnablePhaseIIICSubductingMarks = bEnableMarks;
		Actor->bEnablePhaseIIICVisibleHistoricalElevation = bEnableElevationSplit;
		Actor->bEnablePhaseIIICOverridingPlateUplift = bEnableUplift;
		Actor->bEnablePhaseIIICSlabPull = bEnableSlabPull;
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
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, BaselineSamples, BaselinePlates, 0.30, false, false, false, false);
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

	struct FElevationLedgerReplayResult
	{
		FString Fixture;
		int32 Replay = 0;
		bool bCompleted = false;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIC1SubductingMarkAudit MarkAudit;
		FCarrierLabPhaseIIIC2ElevationAudit ElevationAudit;
		FCarrierLabPhaseIIIC3UpliftAudit UpliftAudit;
		FCarrierLabPhaseIIIC4SlabPullAudit SlabPullAudit;
		FCarrierLabPhaseIIIC5ElevationLedgerAudit LedgerAudit;
		FCarrierLabPhaseIIIB7HashClosureAudit ClosureAudit;
		double OracleRecordDeltaKm = 0.0;
		double OracleTrenchDeltaKm = 0.0;
		double OracleUpliftDeltaKm = 0.0;
		double OracleLedgerResidualKm = 0.0;
		double OracleActualResidualKm = 0.0;
		double UpliftAuditResidualKm = 0.0;
	};

	void ComputeLedgerOracle(FElevationLedgerReplayResult& Result)
	{
		for (const FCarrierLabPhaseIIIC5ElevationLedgerRecord& Record : Result.LedgerAudit.Records)
		{
			Result.OracleRecordDeltaKm += Record.DeltaKm;
			switch (Record.LedgerClass)
			{
			case ECarrierLabPhaseIIIC5ElevationLedgerClass::TrenchVisibleElevation:
				Result.OracleTrenchDeltaKm += Record.DeltaKm;
				break;
			case ECarrierLabPhaseIIIC5ElevationLedgerClass::OverridingUplift:
				Result.OracleUpliftDeltaKm += Record.DeltaKm;
				break;
			default:
				break;
			}
		}
		Result.OracleLedgerResidualKm = Result.OracleRecordDeltaKm - Result.LedgerAudit.LedgerVisibleElevationDeltaKm;
		Result.OracleActualResidualKm = Result.LedgerAudit.ActualVisibleElevationDeltaKm - Result.OracleRecordDeltaKm;
		Result.UpliftAuditResidualKm = Result.UpliftAudit.TotalAppliedDeltaKm - Result.LedgerAudit.UpliftVisibleElevationDeltaKm;
	}

	bool RunLedgerReplay(
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
		const bool bEnableSlabPull,
		FElevationLedgerReplayResult& OutResult)
	{
		OutResult = FElevationLedgerReplayResult();
		OutResult.Fixture = Fixture;
		OutResult.Replay = Replay;
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(
			*World,
			SampleCount,
			PlateCount,
			ContinentalFraction,
			bEnableMarks,
			bEnableElevationSplit,
			bEnableUplift,
			bEnableSlabPull);
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
		const bool bAudits =
			Actor->GetPhaseIIIC1SubductingMarkAudit(OutResult.MarkAudit) &&
			Actor->GetPhaseIIIC2ElevationAudit(OutResult.ElevationAudit) &&
			Actor->GetPhaseIIIC3UpliftAudit(OutResult.UpliftAudit) &&
			Actor->GetPhaseIIIC4SlabPullAudit(OutResult.SlabPullAudit) &&
			Actor->GetPhaseIIIC5ElevationLedgerAudit(OutResult.LedgerAudit) &&
			Actor->GetPhaseIIIB7HashClosureAudit(OutResult.ClosureAudit);
		if (bAudits)
		{
			ComputeLedgerOracle(OutResult);
		}

		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bCompleted = bAudits;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return bAudits;
	}

	bool BypassPasses(const FSlice55BypassResult& Result)
	{
		return Result.bCompleted &&
			Result.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			Result.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
			Result.FilterMetrics.NoBoundaryPairMissCount == 0;
	}

	bool LedgerReconciles(const FElevationLedgerReplayResult& Result)
	{
		return Result.bCompleted &&
			FMath::Abs(Result.OracleLedgerResidualKm) <= GateToleranceKm &&
			FMath::Abs(Result.OracleActualResidualKm) <= GateToleranceKm &&
			FMath::Abs(Result.LedgerAudit.VisibleElevationResidualKm) <= GateToleranceKm &&
			Result.ClosureAudit.bMetricsHashMatchesComputed;
	}

	bool FullLedgerPasses(const FElevationLedgerReplayResult& Result)
	{
		return LedgerReconciles(Result) &&
			Result.MarkAudit.MarkCount > 0 &&
			Result.LedgerAudit.RecordCount > 0 &&
			Result.LedgerAudit.TrenchRecordCount > 0 &&
			Result.LedgerAudit.UpliftRecordCount > 0 &&
			Result.LedgerAudit.ActualVisibleElevationDeltaKm != 0.0 &&
			FMath::Abs(Result.UpliftAuditResidualKm) <= GateToleranceKm;
	}

	bool TrenchOnlyPasses(const FElevationLedgerReplayResult& Result)
	{
		return LedgerReconciles(Result) &&
			Result.MarkAudit.MarkCount > 0 &&
			Result.LedgerAudit.TrenchRecordCount > 0 &&
			Result.LedgerAudit.UpliftRecordCount == 0 &&
			Result.UpliftAudit.UpliftRecordCount == 0;
	}

	bool NoLedgerDeltaPasses(const FElevationLedgerReplayResult& Result)
	{
		return LedgerReconciles(Result) &&
			Result.LedgerAudit.RecordCount == 0 &&
			Result.LedgerAudit.TrenchRecordCount == 0 &&
			Result.LedgerAudit.UpliftRecordCount == 0 &&
			FMath::Abs(Result.LedgerAudit.ActualVisibleElevationDeltaKm) <= GateToleranceKm &&
			Result.ClosureAudit.bMetricsHashMatchesComputed;
	}

	FString BypassJson(const FSlice55BypassResult& Result)
	{
		return FString::Printf(
			TEXT("{\"kind\":\"bypass\",\"replay\":%d,\"completed\":%s,\"state_hash\":%s,\"material_ledger_hash\":%s,\"persistent_mark_inputs\":%d,\"no_boundary_pair_miss_count\":%d,\"seconds\":%.6f}"),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			*JsonString(Result.FilterMetrics.StateHashAfter),
			*JsonString(Result.LedgerMetrics.MaterialLedgerHash),
			Result.FilterMetrics.PersistentSubductingMarkInputCount,
			Result.FilterMetrics.NoBoundaryPairMissCount,
			Result.Seconds);
	}

	FString LedgerJson(const FElevationLedgerReplayResult& Result)
	{
		return FString::Printf(
			TEXT("{\"kind\":\"iiic5_elevation_ledger\",\"fixture\":%s,\"replay\":%d,\"completed\":%s,\"marks\":%d,\"records\":%d,\"trench_records\":%d,\"uplift_records\":%d,\"unique_vertices\":%d,\"actual_delta_km\":%.15f,\"ledger_delta_km\":%.15f,\"oracle_record_delta_km\":%.15f,\"trench_delta_km\":%.15f,\"uplift_delta_km\":%.15f,\"ledger_residual_km\":%.15e,\"actual_residual_km\":%.15e,\"uplift_audit_residual_km\":%.15e,\"ledger_hash\":%s,\"visible_hash\":%s,\"historical_hash\":%s,\"crust_hash\":%s,\"closure_hash\":%s,\"closure_matches\":%s,\"seconds\":%.6f}"),
			*JsonString(Result.Fixture),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.MarkAudit.MarkCount,
			Result.LedgerAudit.RecordCount,
			Result.LedgerAudit.TrenchRecordCount,
			Result.LedgerAudit.UpliftRecordCount,
			Result.LedgerAudit.UniqueVertexCount,
			Result.LedgerAudit.ActualVisibleElevationDeltaKm,
			Result.LedgerAudit.LedgerVisibleElevationDeltaKm,
			Result.OracleRecordDeltaKm,
			Result.LedgerAudit.TrenchVisibleElevationDeltaKm,
			Result.LedgerAudit.UpliftVisibleElevationDeltaKm,
			Result.OracleLedgerResidualKm,
			Result.OracleActualResidualKm,
			Result.UpliftAuditResidualKm,
			*JsonString(Result.LedgerAudit.ElevationLedgerHash),
			*JsonString(Result.LedgerAudit.VisibleElevationHash),
			*JsonString(Result.LedgerAudit.HistoricalElevationHash),
			*JsonString(Result.LedgerAudit.CrustStateHash),
			*JsonString(Result.ClosureAudit.ComputedConvergenceTrackingHash),
			Result.ClosureAudit.bMetricsHashMatchesComputed ? TEXT("true") : TEXT("false"),
			Result.Seconds);
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FSlice55BypassResult& BypassA,
		const FSlice55BypassResult& BypassB,
		const FElevationLedgerReplayResult& FullA,
		const FElevationLedgerReplayResult& FullB,
		const FElevationLedgerReplayResult& TrenchOnly,
		const FElevationLedgerReplayResult& Disabled,
		const FElevationLedgerReplayResult& SlabPullOnly,
		const FElevationLedgerReplayResult& ZeroMotion,
		const FElevationLedgerReplayResult& SinglePlate,
		const FElevationLedgerReplayResult& ForcedDivergenceNoSubduction)
	{
		const bool bBypassPass = BypassPasses(BypassA) && BypassPasses(BypassB);
		const bool bFullPass =
			FullLedgerPasses(FullA) &&
			FullLedgerPasses(FullB) &&
			FullA.LedgerAudit.ElevationLedgerHash == FullB.LedgerAudit.ElevationLedgerHash &&
			FullA.LedgerAudit.VisibleElevationHash == FullB.LedgerAudit.VisibleElevationHash &&
			FullA.LedgerAudit.CrustStateHash == FullB.LedgerAudit.CrustStateHash;
		const bool bTrenchOnlyPass = TrenchOnlyPasses(TrenchOnly);
		const bool bDisabledPass = NoLedgerDeltaPasses(Disabled) && Disabled.MarkAudit.MarkCount > 0;
		const bool bSlabPullOnlyPass = NoLedgerDeltaPasses(SlabPullOnly) && SlabPullOnly.MarkAudit.MarkCount > 0 && SlabPullOnly.SlabPullAudit.ContributionCount > 0;
		const bool bNegativePass =
			NoLedgerDeltaPasses(ZeroMotion) &&
			NoLedgerDeltaPasses(SinglePlate) &&
			NoLedgerDeltaPasses(ForcedDivergenceNoSubduction);
		const bool bIIIBClosureContinuity =
			FullA.ClosureAudit.bMetricsHashMatchesComputed &&
			Disabled.ClosureAudit.bMetricsHashMatchesComputed;
		const bool bAllPass = bBypassPass && bFullPass && bTrenchOnlyPass && bDisabledPass && bSlabPullOnlyPass && bNegativePass;

		FString Report;
		Report += TEXT("# Phase III Slice IIIC.5 Checkpoint\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("Status: IIIC elevation-ledger extension. This slice adds named audit lines for IIIC visible-elevation deltas, separate from the Phase II material ledger. It does not add new tectonic behavior, collision, rifting, erosion, terrain displacement, projection-derived ownership, or any new resampling mutation path.\n\n");
		Report += TEXT("Ledger equation: `actual plate-local visible elevation delta = trench_visible_elevation_delta + overriding_uplift_delta + residual`. The ledger sums plate-local carrier vertices, not projected global samples.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		Report += FString::Printf(
			TEXT("| Slice 5.5 bypass | %s | state `%s` / `%s`, material ledger `%s` / `%s` |\n"),
			*PassFail(bBypassPass),
			*BypassA.FilterMetrics.StateHashAfter,
			*BypassB.FilterMetrics.StateHashAfter,
			*BypassA.LedgerMetrics.MaterialLedgerHash,
			*BypassB.LedgerMetrics.MaterialLedgerHash);
		Report += FString::Printf(
			TEXT("| IIIB closure continuity (non-gating smoke) | %s | expected independent token `%s` is listed for continuity only; this standalone slice does not claim an IIIB signature gate |\n"),
			*PassFail(bIIIBClosureContinuity),
			ExpectedIIIBIndependentSignature);
		Report += FString::Printf(
			TEXT("| Full elevation ledger | %s | records %d / %d, trench %d / %d, uplift %d / %d, residual %.12e / %.12e km |\n"),
			*PassFail(bFullPass),
			FullA.LedgerAudit.RecordCount,
			FullB.LedgerAudit.RecordCount,
			FullA.LedgerAudit.TrenchRecordCount,
			FullB.LedgerAudit.TrenchRecordCount,
			FullA.LedgerAudit.UpliftRecordCount,
			FullB.LedgerAudit.UpliftRecordCount,
			FullA.LedgerAudit.VisibleElevationResidualKm,
			FullB.LedgerAudit.VisibleElevationResidualKm);
		Report += FString::Printf(
			TEXT("| Trench-only ledger line | %s | trench records %d, uplift records %d, residual %.12e km |\n"),
			*PassFail(bTrenchOnlyPass),
			TrenchOnly.LedgerAudit.TrenchRecordCount,
			TrenchOnly.LedgerAudit.UpliftRecordCount,
			TrenchOnly.LedgerAudit.VisibleElevationResidualKm);
		Report += FString::Printf(
			TEXT("| Disabled elevation mutations | %s | marks %d, records %d, actual delta %.12f km |\n"),
			*PassFail(bDisabledPass),
			Disabled.MarkAudit.MarkCount,
			Disabled.LedgerAudit.RecordCount,
			Disabled.LedgerAudit.ActualVisibleElevationDeltaKm);
		Report += FString::Printf(
			TEXT("| Slab pull excluded from elevation ledger | %s | marks %d, slab contributions %d, ledger records %d |\n"),
			*PassFail(bSlabPullOnlyPass),
			SlabPullOnly.MarkAudit.MarkCount,
			SlabPullOnly.SlabPullAudit.ContributionCount,
			SlabPullOnly.LedgerAudit.RecordCount);
		Report += FString::Printf(
			TEXT("| Negative controls | %s | zero %d, single %d, divergence-no-subduction %d records |\n"),
			*PassFail(bNegativePass),
			ZeroMotion.LedgerAudit.RecordCount,
			SinglePlate.LedgerAudit.RecordCount,
			ForcedDivergenceNoSubduction.LedgerAudit.RecordCount);

		Report += TEXT("\n## Primary Full IIIC Elevation Ledger\n\n");
		Report += TEXT("| Replay | Marks | Records | Unique vertices | Actual delta km | Ledger delta km | Trench delta km | Uplift delta km | Residual km | Uplift audit residual km | Ledger hash | Visible hash | Crust hash | Seconds |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|---:|\n");
		auto AddFullRow = [&Report](const FElevationLedgerReplayResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %d | %d | %d | %d | %.12f | %.12f | %.12f | %.12f | %.12e | %.12e | `%s` | `%s` | `%s` | %.3f |\n"),
				Result.Replay,
				Result.MarkAudit.MarkCount,
				Result.LedgerAudit.RecordCount,
				Result.LedgerAudit.UniqueVertexCount,
				Result.LedgerAudit.ActualVisibleElevationDeltaKm,
				Result.LedgerAudit.LedgerVisibleElevationDeltaKm,
				Result.LedgerAudit.TrenchVisibleElevationDeltaKm,
				Result.LedgerAudit.UpliftVisibleElevationDeltaKm,
				Result.LedgerAudit.VisibleElevationResidualKm,
				Result.UpliftAuditResidualKm,
				*Result.LedgerAudit.ElevationLedgerHash,
				*Result.LedgerAudit.VisibleElevationHash,
				*Result.LedgerAudit.CrustStateHash,
				Result.Seconds);
		};
		AddFullRow(FullA);
		AddFullRow(FullB);

		Report += TEXT("\n### Representative Elevation Ledger Records\n\n");
		Report += TEXT("| Record | Class | Mark | Plate | Other | Triangle | Vertex | Global sample | Previous km | New km | Delta km | Signed velocity |\n");
		Report += TEXT("|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		for (int32 Index = 0; Index < FMath::Min(20, FullA.LedgerAudit.Records.Num()); ++Index)
		{
			const FCarrierLabPhaseIIIC5ElevationLedgerRecord& Record = FullA.LedgerAudit.Records[Index];
			Report += FString::Printf(
				TEXT("| %d | %s | %d | %d | %d | %d | %d | %d | %.6f | %.6f | %.12f | %.12f |\n"),
				Record.RecordId,
				*LedgerClassName(Record.LedgerClass),
				Record.MarkId,
				Record.PlateId,
				Record.OtherPlateId,
				Record.LocalTriangleId,
				Record.LocalVertexId,
				Record.GlobalSampleId,
				Record.PreviousElevationKm,
				Record.NewElevationKm,
				Record.DeltaKm,
				Record.SignedConvergenceVelocity);
		}

		Report += TEXT("\n## Controls\n\n");
		Report += TEXT("| Fixture | Marks | Trench records | Uplift records | Ledger records | Actual delta km | Ledger delta km | Residual km | Closure matches | Result |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---|---|\n");
		auto AddControlRow = [&Report](const FElevationLedgerReplayResult& Result, const bool bPass)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %.12f | %.12f | %.12e | %s | %s |\n"),
				*Result.Fixture,
				Result.MarkAudit.MarkCount,
				Result.LedgerAudit.TrenchRecordCount,
				Result.LedgerAudit.UpliftRecordCount,
				Result.LedgerAudit.RecordCount,
				Result.LedgerAudit.ActualVisibleElevationDeltaKm,
				Result.LedgerAudit.LedgerVisibleElevationDeltaKm,
				Result.LedgerAudit.VisibleElevationResidualKm,
				Result.ClosureAudit.bMetricsHashMatchesComputed ? TEXT("yes") : TEXT("no"),
				*PassFail(bPass));
		};
		AddControlRow(TrenchOnly, bTrenchOnlyPass);
		AddControlRow(Disabled, bDisabledPass);
		AddControlRow(SlabPullOnly, bSlabPullOnlyPass);
		AddControlRow(ZeroMotion, NoLedgerDeltaPasses(ZeroMotion));
		AddControlRow(SinglePlate, NoLedgerDeltaPasses(SinglePlate));
		AddControlRow(ForcedDivergenceNoSubduction, NoLedgerDeltaPasses(ForcedDivergenceNoSubduction));

		Report += TEXT("\n## Scope Notes\n\n");
		Report += TEXT("- The Phase II material ledger categories and `MaterialLedgerHash` remain unchanged; IIIC.5 adds a separate elevation-ledger audit for process-state deltas.\n");
		Report += TEXT("- Slab pull is intentionally excluded from the elevation ledger because it mutates motion authority, not elevation or material.\n");
		Report += TEXT("- The ledger sums plate-local vertices, preserving carrier authority. It does not read projected global samples as authority.\n");
		Report += TEXT("- This checkpoint may claim only IIIC.5 elevation accounting. It does not claim Stage 1.5 carrier success, Slice 5.5 asymmetry resolution, collision, rifting, erosion, slab-pull correctness beyond its exclusion from the elevation ledger, or terrain morphology.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIC.5 passes. Pause for user review before IIIC consolidation.\n")
			: TEXT("IIIC.5 does not pass. Investigate the failed gate before IIIC consolidation.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIC5Commandlet::UCarrierLabPhaseIIIC5Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIC5Commandlet::Main(const FString& Params)
{
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FSlice55BypassResult BypassA;
	FSlice55BypassResult BypassB;
	const bool bBypassA = RunSlice55Bypass(0, BypassA);
	const bool bBypassB = RunSlice55Bypass(1, BypassB);

	FElevationLedgerReplayResult FullA;
	FElevationLedgerReplayResult FullB;
	const bool bFullA = RunLedgerReplay(
		TEXT("forced_convergence_full_elevation_ledger"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		true,
		true,
		false,
		FullA);
	const bool bFullB = RunLedgerReplay(
		TEXT("forced_convergence_full_elevation_ledger"),
		1,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		true,
		true,
		false,
		FullB);

	FElevationLedgerReplayResult TrenchOnly;
	const bool bTrenchOnly = RunLedgerReplay(
		TEXT("trench_only"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		true,
		false,
		false,
		TrenchOnly);
	FElevationLedgerReplayResult Disabled;
	const bool bDisabled = RunLedgerReplay(
		TEXT("elevation_mutations_disabled"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		false,
		false,
		false,
		Disabled);
	FElevationLedgerReplayResult SlabPullOnly;
	const bool bSlabPullOnly = RunLedgerReplay(
		TEXT("slab_pull_only"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		false,
		false,
		true,
		SlabPullOnly);

	FElevationLedgerReplayResult ZeroMotion;
	const bool bZero = RunLedgerReplay(
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
		false,
		ZeroMotion);
	FElevationLedgerReplayResult SinglePlate;
	const bool bSingle = RunLedgerReplay(
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
		false,
		SinglePlate);
	FElevationLedgerReplayResult ForcedDivergence;
	const bool bDivergence = RunLedgerReplay(
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
		false,
		ForcedDivergence);

	TArray<FString> JsonLines;
	JsonLines.Add(BypassJson(BypassA));
	JsonLines.Add(BypassJson(BypassB));
	JsonLines.Add(LedgerJson(FullA));
	JsonLines.Add(LedgerJson(FullB));
	JsonLines.Add(LedgerJson(TrenchOnly));
	JsonLines.Add(LedgerJson(Disabled));
	JsonLines.Add(LedgerJson(SlabPullOnly));
	JsonLines.Add(LedgerJson(ZeroMotion));
	JsonLines.Add(LedgerJson(SinglePlate));
	JsonLines.Add(LedgerJson(ForcedDivergence));

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(FString::Join(JsonLines, TEXT("\n")) + TEXT("\n"), *MetricsPath);

	const FString Report = BuildReport(OutputRoot, BypassA, BypassB, FullA, FullB, TrenchOnly, Disabled, SlabPullOnly, ZeroMotion, SinglePlate, ForcedDivergence);
	const FString ReportPath = ResolveReportPath(Params);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	const bool bBypassPass = bBypassA && bBypassB && BypassPasses(BypassA) && BypassPasses(BypassB);
	const bool bFullPass =
		bFullA &&
		bFullB &&
		FullLedgerPasses(FullA) &&
		FullLedgerPasses(FullB) &&
		FullA.LedgerAudit.ElevationLedgerHash == FullB.LedgerAudit.ElevationLedgerHash &&
		FullA.LedgerAudit.VisibleElevationHash == FullB.LedgerAudit.VisibleElevationHash &&
		FullA.LedgerAudit.CrustStateHash == FullB.LedgerAudit.CrustStateHash;
	const bool bTrenchOnlyPass = bTrenchOnly && TrenchOnlyPasses(TrenchOnly);
	const bool bDisabledPass = bDisabled && NoLedgerDeltaPasses(Disabled) && Disabled.MarkAudit.MarkCount > 0;
	const bool bSlabPullOnlyPass = bSlabPullOnly && NoLedgerDeltaPasses(SlabPullOnly) && SlabPullOnly.MarkAudit.MarkCount > 0 && SlabPullOnly.SlabPullAudit.ContributionCount > 0;
	const bool bNegativePass =
		bZero &&
		bSingle &&
		bDivergence &&
		NoLedgerDeltaPasses(ZeroMotion) &&
		NoLedgerDeltaPasses(SinglePlate) &&
		NoLedgerDeltaPasses(ForcedDivergence);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIIC.5 report: %s"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIIC.5 metrics: %s"), *MetricsPath);
	return (bBypassPass && bFullPass && bTrenchOnlyPass && bDisabledPass && bSlabPullOnlyPass && bNegativePass) ? 0 : 1;
}
