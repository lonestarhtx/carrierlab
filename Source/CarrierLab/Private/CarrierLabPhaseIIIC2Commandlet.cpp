// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIC2Commandlet.h"

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
	constexpr int32 MaxSearchSteps = 80;
	constexpr double VelocityMmPerYear = 66.6666666667;
	constexpr double SeedElevationKm = 2.5;
	constexpr double TrenchDepthKm = -10.0;
	constexpr double ElevationToleranceKm = 1.0e-9;
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

	bool NearlyEqual(const double A, const double B)
	{
		return FMath::Abs(A - B) <= ElevationToleranceKm;
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIC2"), Stamp);
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
				TEXT("phase-iii-slice-iiic2-report.md"));
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
		const bool bEnableIIICMarks,
		const bool bEnableIIIC2ElevationSplit)
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
		Actor->bEnablePhaseIIICSubductingMarks = bEnableIIICMarks;
		Actor->bEnablePhaseIIICVisibleHistoricalElevation = bEnableIIIC2ElevationSplit;
		Actor->PhaseIIICTrenchDepthKm = TrenchDepthKm;
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	bool ConfigureMixedTwoPlateFixture(ACarrierLabVisualizationActor& Actor)
	{
		return Actor.SetPlateContinentalForTest(0, true) &&
			Actor.SetPlateContinentalForTest(1, false);
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
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, BaselineSamples, BaselinePlates, 0.30, false, false);
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

	struct FElevationReplayResult
	{
		int32 Replay = 0;
		bool bCompleted = false;
		bool bElevationEnabled = false;
		int32 StepFound = 0;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIC1SubductingMarkAudit MarkAudit;
		FCarrierLabPhaseIIIC2ElevationAudit ElevationAudit;
		FCarrierLabPhaseIIIC2ElevationAudit SecondStepElevationAudit;
		FCarrierLabVisualizationMetrics Metrics;
	};

	bool RunElevationReplay(const int32 Replay, const bool bEnableElevationSplit, FElevationReplayResult& OutResult)
	{
		OutResult = FElevationReplayResult();
		OutResult.Replay = Replay;
		OutResult.bElevationEnabled = bEnableElevationSplit;
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, FixtureSamples, FixturePlates, 0.50, true, bEnableElevationSplit);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier() ||
			!ConfigureMixedTwoPlateFixture(*Actor) ||
			!Actor->SetPlateElevationForTest(1, SeedElevationKm))
		{
			Actor->Destroy();
			return false;
		}
		Actor->ConfigurePhaseIIMotionFixture(ECarrierLabPhaseIIMotionFixture::ForcedConvergence);

		for (int32 Step = 0; Step < MaxSearchSteps; ++Step)
		{
			Actor->StepOnce();
			Actor->GetPhaseIIIC1SubductingMarkAudit(OutResult.MarkAudit);
			Actor->GetPhaseIIIC2ElevationAudit(OutResult.ElevationAudit);
			if (OutResult.MarkAudit.MarkCount > 0)
			{
				break;
			}
		}

		OutResult.StepFound = OutResult.ElevationAudit.Step;
		Actor->StepOnce();
		Actor->GetPhaseIIIC2ElevationAudit(OutResult.SecondStepElevationAudit);
		OutResult.Metrics = Actor->CurrentMetrics;
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bCompleted = true;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return true;
	}

	struct FResetReplayResult
	{
		bool bCompleted = false;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIC2ElevationAudit BeforeAudit;
		FCarrierLabPhaseIIIC2ElevationAudit AfterAudit;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterMetrics;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerMetrics;
	};

	bool RunResetReplay(FResetReplayResult& OutResult)
	{
		OutResult = FResetReplayResult();
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, FixtureSamples, FixturePlates, 0.50, true, true);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier() ||
			!ConfigureMixedTwoPlateFixture(*Actor) ||
			!Actor->SetPlateElevationForTest(1, SeedElevationKm))
		{
			Actor->Destroy();
			return false;
		}
		Actor->ConfigurePhaseIIMotionFixture(ECarrierLabPhaseIIMotionFixture::ForcedConvergence);
		for (int32 Step = 0; Step < MaxSearchSteps; ++Step)
		{
			Actor->StepOnce();
			Actor->GetPhaseIIIC2ElevationAudit(OutResult.BeforeAudit);
			if (OutResult.BeforeAudit.SnapshotMarkCount > 0)
			{
				break;
			}
		}

		TArray<FCarrierLabPhaseIITriangleLabelRecord> EmptyLabels;
		TArray<FCarrierLabPhaseIIFilterDecisionRecord> Decisions;
		const bool bFilterOk = Actor->ApplyPhaseIIResamplingFilterEvent(
			EmptyLabels,
			Decisions,
			OutResult.FilterMetrics,
			nullptr,
			&OutResult.LedgerMetrics);
		Actor->GetPhaseIIIC2ElevationAudit(OutResult.AfterAudit);

		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bCompleted = bFilterOk;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return bFilterOk;
	}

	bool ElevationAuditHasExpectedTrench(const FCarrierLabPhaseIIIC2ElevationAudit& Audit)
	{
		return Audit.SnapshotMarkCount > 0 &&
			Audit.MissingSnapshotCount == 0 &&
			Audit.InvalidSnapshotCount == 0 &&
			Audit.StateInvalidSnapshotCount == 0 &&
			Audit.DuplicateSnapshotCount == 0 &&
			Audit.StateSnapshotCount == Audit.SnapshotMarkCount &&
			Audit.StateSnapshotVertexCount == Audit.SnapshotVertexCount &&
			NearlyEqual(Audit.VisibleElevationMin, TrenchDepthKm) &&
			NearlyEqual(Audit.VisibleElevationMax, TrenchDepthKm) &&
			NearlyEqual(Audit.HistoricalElevationMin, SeedElevationKm) &&
			NearlyEqual(Audit.HistoricalElevationMax, SeedElevationKm) &&
			Audit.VisibleElevationHash != Audit.HistoricalElevationHash;
	}

	FString ElevationRecordJson(const FCarrierLabPhaseIIIC2ElevationAuditRecord& Record)
	{
		return FString::Printf(
			TEXT("{\"mark_id\":%d,\"plate\":%d,\"other_plate\":%d,\"local_triangle\":%d,\"snapshot_vertices\":%d,\"historical_min\":%.12f,\"historical_max\":%.12f,\"visible_min\":%.12f,\"visible_max\":%.12f,\"trench_depth\":%.12f}"),
			Record.MarkId,
			Record.PlateId,
			Record.OtherPlateId,
			Record.LocalTriangleId,
			Record.SnapshotVertexCount,
			Record.HistoricalElevationMin,
			Record.HistoricalElevationMax,
			Record.VisibleElevationMin,
			Record.VisibleElevationMax,
			Record.AppliedTrenchDepthKm);
	}

	void AppendElevationReplayJson(const FString& Kind, const FElevationReplayResult& Result, TArray<FString>& Lines)
	{
		FString RecordsJson = TEXT("[");
		for (int32 Index = 0; Index < Result.ElevationAudit.Records.Num(); ++Index)
		{
			if (Index > 0)
			{
				RecordsJson += TEXT(",");
			}
			RecordsJson += ElevationRecordJson(Result.ElevationAudit.Records[Index]);
		}
		RecordsJson += TEXT("]");

		Lines.Add(FString::Printf(
			TEXT("{\"kind\":%s,\"replay\":%d,\"completed\":%s,\"enabled\":%s,\"step\":%d,\"marks\":%d,\"snapshots\":%d,\"missing_snapshots\":%d,\"duplicate_snapshots\":%d,\"invalid_snapshots\":%d,\"snapshot_vertices\":%d,\"visible_min\":%.12f,\"visible_max\":%.12f,\"historical_min\":%.12f,\"historical_max\":%.12f,\"visible_hash\":%s,\"historical_hash\":%s,\"crust_hash\":%s,\"records\":%s}"),
			*JsonString(Kind),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.bElevationEnabled ? TEXT("true") : TEXT("false"),
			Result.StepFound,
			Result.ElevationAudit.MarkCount,
			Result.ElevationAudit.SnapshotMarkCount,
			Result.ElevationAudit.MissingSnapshotCount,
			Result.ElevationAudit.DuplicateSnapshotCount,
			Result.ElevationAudit.InvalidSnapshotCount + Result.ElevationAudit.StateInvalidSnapshotCount,
			Result.ElevationAudit.SnapshotVertexCount,
			Result.ElevationAudit.VisibleElevationMin,
			Result.ElevationAudit.VisibleElevationMax,
			Result.ElevationAudit.HistoricalElevationMin,
			Result.ElevationAudit.HistoricalElevationMax,
			*JsonString(Result.ElevationAudit.VisibleElevationHash),
			*JsonString(Result.ElevationAudit.HistoricalElevationHash),
			*JsonString(Result.ElevationAudit.CrustStateHash),
			*RecordsJson));
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FSlice55BypassResult& BypassA,
		const FSlice55BypassResult& BypassB,
		const FElevationReplayResult& ElevationA,
		const FElevationReplayResult& ElevationB,
		const FElevationReplayResult& DisabledElevation,
		const FResetReplayResult& Reset)
	{
		const bool bBypassPass =
			BypassA.bCompleted &&
			BypassB.bCompleted &&
			BypassA.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			BypassB.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			BypassA.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
			BypassB.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash;
		const bool bElevationPass =
			ElevationA.bCompleted &&
			ElevationB.bCompleted &&
			ElevationAuditHasExpectedTrench(ElevationA.ElevationAudit) &&
			ElevationAuditHasExpectedTrench(ElevationB.ElevationAudit) &&
			ElevationAuditHasExpectedTrench(ElevationA.SecondStepElevationAudit) &&
			ElevationAuditHasExpectedTrench(ElevationB.SecondStepElevationAudit) &&
			ElevationA.ElevationAudit.SnapshotMarkCount == ElevationB.ElevationAudit.SnapshotMarkCount &&
			ElevationA.ElevationAudit.VisibleElevationHash == ElevationB.ElevationAudit.VisibleElevationHash &&
			ElevationA.ElevationAudit.HistoricalElevationHash == ElevationB.ElevationAudit.HistoricalElevationHash &&
			ElevationA.ElevationAudit.CrustStateHash == ElevationB.ElevationAudit.CrustStateHash;
		const bool bOptInPass =
			DisabledElevation.bCompleted &&
			DisabledElevation.MarkAudit.MarkCount > 0 &&
			DisabledElevation.ElevationAudit.SnapshotMarkCount == 0 &&
			DisabledElevation.ElevationAudit.MissingSnapshotCount == DisabledElevation.ElevationAudit.MarkCount &&
			DisabledElevation.ElevationAudit.StateSnapshotCount == 0;
		const bool bResetPass =
			Reset.bCompleted &&
			Reset.BeforeAudit.SnapshotMarkCount > 0 &&
			Reset.FilterMetrics.PersistentSubductingMarkInputCount > 0 &&
			Reset.AfterAudit.MarkCount == 0 &&
			Reset.AfterAudit.SnapshotMarkCount == 0 &&
			Reset.AfterAudit.ResetSerial > Reset.BeforeAudit.ResetSerial;
		const bool bAllPass = bBypassPass && bElevationPass && bOptInPass && bResetPass;

		FString Report;
		Report += TEXT("# Phase III Slice IIIC.2 Checkpoint\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("Status: opt-in visible/historical elevation split for IIIC subducting triangles. On first marking, the under-plate triangle snapshots visible `Elevation` into `HistoricalElevation`, then visible `Elevation` is set to paper Table 3.2 trench depth `z_t = -10 km`. This slice does not add uplift, slab pull, collision, rifting, erosion, terrain displacement, global ownership, or any new resampling mutation path.\n\n");

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
			TEXT("| Snapshot and trench depth | %s | snapshots %d / %d, visible %.3f..%.3f km, historical %.3f..%.3f km |\n"),
			*PassFail(bElevationPass),
			ElevationA.ElevationAudit.SnapshotMarkCount,
			ElevationB.ElevationAudit.SnapshotMarkCount,
			ElevationA.ElevationAudit.VisibleElevationMin,
			ElevationA.ElevationAudit.VisibleElevationMax,
			ElevationA.ElevationAudit.HistoricalElevationMin,
			ElevationA.ElevationAudit.HistoricalElevationMax);
		Report += FString::Printf(
			TEXT("| Independent elevation hashes | %s | visible `%s` / `%s`, historical `%s` / `%s` |\n"),
			*PassFail(bElevationPass),
			*ElevationA.ElevationAudit.VisibleElevationHash,
			*ElevationB.ElevationAudit.VisibleElevationHash,
			*ElevationA.ElevationAudit.HistoricalElevationHash,
			*ElevationB.ElevationAudit.HistoricalElevationHash);
		Report += FString::Printf(
			TEXT("| Opt-in disabled | %s | marks %d, snapshots %d, missing snapshots %d |\n"),
			*PassFail(bOptInPass),
			DisabledElevation.MarkAudit.MarkCount,
			DisabledElevation.ElevationAudit.SnapshotMarkCount,
			DisabledElevation.ElevationAudit.MissingSnapshotCount);
		Report += FString::Printf(
			TEXT("| Remesh reset | %s | before marks %d/snapshots %d, persistent mark inputs %d, after marks %d/snapshots %d, reset serial %d -> %d |\n"),
			*PassFail(bResetPass),
			Reset.BeforeAudit.MarkCount,
			Reset.BeforeAudit.SnapshotMarkCount,
			Reset.FilterMetrics.PersistentSubductingMarkInputCount,
			Reset.AfterAudit.MarkCount,
			Reset.AfterAudit.SnapshotMarkCount,
			Reset.BeforeAudit.ResetSerial,
			Reset.AfterAudit.ResetSerial);

		Report += TEXT("\n## Bypass Gate\n\n");
		Report += TEXT("With both IIIC marks and the IIIC.2 elevation split disabled, the fixed Slice 5.5 replay must continue to hit the accepted baseline hashes. This is a fixed-fixture regression, not a global no-mutation proof.\n\n");
		Report += TEXT("| Replay | State hash | Expected state | Material ledger hash | Expected ledger | Persistent mark inputs | Seconds |\n");
		Report += TEXT("|---:|---|---|---|---|---:|---:|\n");
		auto AddBypassRow = [&Report](const FSlice55BypassResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %d | `%s` | `%s` | `%s` | `%s` | %d | %.3f |\n"),
				Result.Replay,
				*Result.FilterMetrics.StateHashAfter,
				ExpectedSlice55StateHash,
				*Result.LedgerMetrics.MaterialLedgerHash,
				ExpectedSlice55MaterialLedgerHash,
				Result.FilterMetrics.PersistentSubductingMarkInputCount,
				Result.Seconds);
		};
		AddBypassRow(BypassA);
		AddBypassRow(BypassB);

		Report += TEXT("\n## Elevation Split Replay\n\n");
		Report += FString::Printf(TEXT("Fixture: two plates under forced convergence, plate 0 continental, plate 1 oceanic. Plate 1 visible elevation is seeded to %.3f km before marking. The expected first snapshot is %.3f km historical elevation and %.3f km visible trench elevation.\n\n"), SeedElevationKm, SeedElevationKm, TrenchDepthKm);
		Report += TEXT("| Replay | Step | Marks | Snapshots | Missing | Duplicate | Invalid | Snapshot vertices | Visible min/max km | Historical min/max km | Visible hash | Historical hash | Crust hash |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|---|---|\n");
		auto AddElevationRow = [&Report](const FElevationReplayResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %d | %d | %d | %d | %d | %d | %d | %d | %.3f / %.3f | %.3f / %.3f | `%s` | `%s` | `%s` |\n"),
				Result.Replay,
				Result.StepFound,
				Result.ElevationAudit.MarkCount,
				Result.ElevationAudit.SnapshotMarkCount,
				Result.ElevationAudit.MissingSnapshotCount,
				Result.ElevationAudit.DuplicateSnapshotCount,
				Result.ElevationAudit.InvalidSnapshotCount + Result.ElevationAudit.StateInvalidSnapshotCount,
				Result.ElevationAudit.SnapshotVertexCount,
				Result.ElevationAudit.VisibleElevationMin,
				Result.ElevationAudit.VisibleElevationMax,
				Result.ElevationAudit.HistoricalElevationMin,
				Result.ElevationAudit.HistoricalElevationMax,
				*Result.ElevationAudit.VisibleElevationHash,
				*Result.ElevationAudit.HistoricalElevationHash,
				*Result.ElevationAudit.CrustStateHash);
		};
		AddElevationRow(ElevationA);
		AddElevationRow(ElevationB);

		Report += TEXT("\n### Representative Elevation Records\n\n");
		Report += TEXT("| Mark | Plate | Other | Local triangle | Snapshot vertices | Historical min/max km | Visible min/max km | Applied trench km |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---|---|---:|\n");
		for (const FCarrierLabPhaseIIIC2ElevationAuditRecord& Record : ElevationA.ElevationAudit.Records)
		{
			Report += FString::Printf(
				TEXT("| %d | %d | %d | %d | %d | %.3f / %.3f | %.3f / %.3f | %.3f |\n"),
				Record.MarkId,
				Record.PlateId,
				Record.OtherPlateId,
				Record.LocalTriangleId,
				Record.SnapshotVertexCount,
				Record.HistoricalElevationMin,
				Record.HistoricalElevationMax,
				Record.VisibleElevationMin,
				Record.VisibleElevationMax,
				Record.AppliedTrenchDepthKm);
		}

		Report += TEXT("\n## Negative And Reset Controls\n\n");
		Report += TEXT("| Control | Marks | Snapshots before | Snapshots after | Missing | Persistent mark inputs | Reset serial | Result |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---|---|\n");
		Report += FString::Printf(
			TEXT("| elevation split disabled | %d | %d | n/a | %d | 0 | %d | %s |\n"),
			DisabledElevation.MarkAudit.MarkCount,
			DisabledElevation.ElevationAudit.SnapshotMarkCount,
			DisabledElevation.ElevationAudit.MissingSnapshotCount,
			DisabledElevation.ElevationAudit.ResetSerial,
			*PassFail(bOptInPass));
		Report += FString::Printf(
			TEXT("| filtered resample reset | %d | %d | %d | %d | %d | %d -> %d | %s |\n"),
			Reset.BeforeAudit.MarkCount,
			Reset.BeforeAudit.SnapshotMarkCount,
			Reset.AfterAudit.SnapshotMarkCount,
			Reset.AfterAudit.MissingSnapshotCount,
			Reset.FilterMetrics.PersistentSubductingMarkInputCount,
			Reset.BeforeAudit.ResetSerial,
			Reset.AfterAudit.ResetSerial,
			*PassFail(bResetPass));

		Report += TEXT("\n## Scope Notes\n\n");
		Report += TEXT("- The historical snapshot is plate-local/per-window process state attached to IIIC subducting triangle marks. Duplicate mark evidence does not re-snapshot an existing triangle.\n");
		Report += TEXT("- `VisibleElevationHash` and `HistoricalElevationHash` are independent field hashes. The former sees the trench depth; the latter sees the pre-trench value that IIIC.3 uplift will read.\n");
		Report += TEXT("- The actor elevation heatmap now reads projected plate-local visible elevation for display, without making projected output authoritative.\n");
		Report += TEXT("- This checkpoint may claim only IIIC.2 visible/historical elevation behavior. It does not claim uplift, slab-pull, collision, rifting, erosion, Stage 1.5 carrier success, or Slice 5.5 asymmetry resolution.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIC.2 passes. Pause for user review before IIIC.3 overriding-plate uplift work.\n")
			: TEXT("IIIC.2 does not pass. Investigate the failed gate before any IIIC.3 work.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIC2Commandlet::UCarrierLabPhaseIIIC2Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIC2Commandlet::Main(const FString& Params)
{
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FSlice55BypassResult BypassA;
	FSlice55BypassResult BypassB;
	const bool bBypassA = RunSlice55Bypass(0, BypassA);
	const bool bBypassB = RunSlice55Bypass(1, BypassB);

	FElevationReplayResult ElevationA;
	FElevationReplayResult ElevationB;
	const bool bElevationA = RunElevationReplay(0, true, ElevationA);
	const bool bElevationB = RunElevationReplay(1, true, ElevationB);

	FElevationReplayResult DisabledElevation;
	const bool bDisabled = RunElevationReplay(0, false, DisabledElevation);

	FResetReplayResult Reset;
	const bool bReset = RunResetReplay(Reset);

	TArray<FString> JsonLines;
	JsonLines.Add(FString::Printf(
		TEXT("{\"kind\":\"bypass\",\"replay\":0,\"completed\":%s,\"state_hash\":%s,\"material_ledger_hash\":%s,\"persistent_mark_inputs\":%d,\"seconds\":%.6f}"),
		bBypassA ? TEXT("true") : TEXT("false"),
		*JsonString(BypassA.FilterMetrics.StateHashAfter),
		*JsonString(BypassA.LedgerMetrics.MaterialLedgerHash),
		BypassA.FilterMetrics.PersistentSubductingMarkInputCount,
		BypassA.Seconds));
	JsonLines.Add(FString::Printf(
		TEXT("{\"kind\":\"bypass\",\"replay\":1,\"completed\":%s,\"state_hash\":%s,\"material_ledger_hash\":%s,\"persistent_mark_inputs\":%d,\"seconds\":%.6f}"),
		bBypassB ? TEXT("true") : TEXT("false"),
		*JsonString(BypassB.FilterMetrics.StateHashAfter),
		*JsonString(BypassB.LedgerMetrics.MaterialLedgerHash),
		BypassB.FilterMetrics.PersistentSubductingMarkInputCount,
		BypassB.Seconds));
	AppendElevationReplayJson(TEXT("elevation_replay"), ElevationA, JsonLines);
	AppendElevationReplayJson(TEXT("elevation_replay"), ElevationB, JsonLines);
	AppendElevationReplayJson(TEXT("elevation_disabled"), DisabledElevation, JsonLines);
	JsonLines.Add(FString::Printf(
		TEXT("{\"kind\":\"reset\",\"completed\":%s,\"marks_before\":%d,\"snapshots_before\":%d,\"persistent_mark_inputs\":%d,\"filtered_candidates\":%d,\"marks_after\":%d,\"snapshots_after\":%d,\"reset_serial_before\":%d,\"reset_serial_after\":%d,\"visible_hash_before\":%s,\"historical_hash_before\":%s,\"material_ledger_hash\":%s,\"seconds\":%.6f}"),
		bReset ? TEXT("true") : TEXT("false"),
		Reset.BeforeAudit.MarkCount,
		Reset.BeforeAudit.SnapshotMarkCount,
		Reset.FilterMetrics.PersistentSubductingMarkInputCount,
		Reset.FilterMetrics.FilteredCandidateCount,
		Reset.AfterAudit.MarkCount,
		Reset.AfterAudit.SnapshotMarkCount,
		Reset.BeforeAudit.ResetSerial,
		Reset.AfterAudit.ResetSerial,
		*JsonString(Reset.BeforeAudit.VisibleElevationHash),
		*JsonString(Reset.BeforeAudit.HistoricalElevationHash),
		*JsonString(Reset.LedgerMetrics.MaterialLedgerHash),
		Reset.Seconds));

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(FString::Join(JsonLines, TEXT("\n")) + TEXT("\n"), *MetricsPath);

	const FString Report = BuildReport(OutputRoot, BypassA, BypassB, ElevationA, ElevationB, DisabledElevation, Reset);
	const FString ReportPath = ResolveReportPath(Params);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	const bool bBypassPass =
		bBypassA &&
		bBypassB &&
		BypassA.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
		BypassB.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
		BypassA.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
		BypassB.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash;
	const bool bElevationPass =
		bElevationA &&
		bElevationB &&
		ElevationAuditHasExpectedTrench(ElevationA.ElevationAudit) &&
		ElevationAuditHasExpectedTrench(ElevationB.ElevationAudit) &&
		ElevationAuditHasExpectedTrench(ElevationA.SecondStepElevationAudit) &&
		ElevationAuditHasExpectedTrench(ElevationB.SecondStepElevationAudit) &&
		ElevationA.ElevationAudit.SnapshotMarkCount == ElevationB.ElevationAudit.SnapshotMarkCount &&
		ElevationA.ElevationAudit.VisibleElevationHash == ElevationB.ElevationAudit.VisibleElevationHash &&
		ElevationA.ElevationAudit.HistoricalElevationHash == ElevationB.ElevationAudit.HistoricalElevationHash &&
		ElevationA.ElevationAudit.CrustStateHash == ElevationB.ElevationAudit.CrustStateHash;
	const bool bOptInPass =
		bDisabled &&
		DisabledElevation.MarkAudit.MarkCount > 0 &&
		DisabledElevation.ElevationAudit.SnapshotMarkCount == 0 &&
		DisabledElevation.ElevationAudit.MissingSnapshotCount == DisabledElevation.ElevationAudit.MarkCount &&
		DisabledElevation.ElevationAudit.StateSnapshotCount == 0;
	const bool bResetPass =
		bReset &&
		Reset.BeforeAudit.SnapshotMarkCount > 0 &&
		Reset.FilterMetrics.PersistentSubductingMarkInputCount > 0 &&
		Reset.AfterAudit.MarkCount == 0 &&
		Reset.AfterAudit.SnapshotMarkCount == 0 &&
		Reset.AfterAudit.ResetSerial > Reset.BeforeAudit.ResetSerial;

	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIIC.2 report: %s"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIIC.2 metrics: %s"), *MetricsPath);
	return (bBypassPass && bElevationPass && bOptInPass && bResetPass) ? 0 : 1;
}
