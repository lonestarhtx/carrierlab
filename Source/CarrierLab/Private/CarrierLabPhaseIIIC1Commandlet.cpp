// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIC1Commandlet.h"

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
	constexpr int32 MaxMarkSearchSteps = 80;
	constexpr double VelocityMmPerYear = 66.6666666667;
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

	FString PolarityClassName(const CarrierLab::EConvergenceSubductionPolarityClass DecisionClass)
	{
		switch (DecisionClass)
		{
		case CarrierLab::EConvergenceSubductionPolarityClass::OceanicUnderContinental:
			return TEXT("oceanic_under_continental");
		case CarrierLab::EConvergenceSubductionPolarityClass::OlderOceanicUnderYoungerOceanic:
			return TEXT("older_oceanic_under_younger_oceanic");
		case CarrierLab::EConvergenceSubductionPolarityClass::CollisionCandidate:
			return TEXT("collision_candidate");
		case CarrierLab::EConvergenceSubductionPolarityClass::OceanOceanDeferred:
			return TEXT("ocean_ocean_deferred");
		case CarrierLab::EConvergenceSubductionPolarityClass::Invalid:
			return TEXT("invalid");
		case CarrierLab::EConvergenceSubductionPolarityClass::None:
		default:
			return TEXT("none");
		}
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIC1"), Stamp);
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
				TEXT("phase-iii-slice-iiic1-report.md"));
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
		const bool bEnableIIICMarks)
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
		Actor->VisualizationLayer = ECarrierLabVisualizationLayer::PlateId;
		Actor->bEnablePhaseIIICSubductingMarks = bEnableIIICMarks;
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	struct FSlice55BypassResult
	{
		int32 Replay = 0;
		bool bCompleted = false;
		double Seconds = 0.0;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
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
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, BaselineSamples, BaselinePlates, 0.30, false);
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
		const bool bOk =
			Actor->DetectPhaseIIContacts(Contacts, OutResult.ContactMetrics) &&
			Actor->BuildPhaseIITriangleLabels(Contacts, LabelConfig, Labels, OutResult.LabelMetrics) &&
			Actor->ApplyPhaseIIResamplingFilterEvent(Labels, Decisions, OutResult.FilterMetrics, nullptr, &OutResult.LedgerMetrics);

		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bCompleted = bOk;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return bOk;
	}

	struct FMarkingReplayResult
	{
		int32 Replay = 0;
		FString FixtureName;
		bool bCompleted = false;
		int32 StepFound = 0;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit MatrixAudit;
		FCarrierLabPhaseIIIB4PolarityAudit PolarityAudit;
		FCarrierLabPhaseIIIC1SubductingMarkAudit MarkAudit;
		FCarrierLabVisualizationMetrics Metrics;
	};

	bool ConfigureMixedTwoPlateFixture(ACarrierLabVisualizationActor& Actor)
	{
		return Actor.SetPlateContinentalForTest(0, true) &&
			Actor.SetPlateContinentalForTest(1, false);
	}

	bool RunMarkingReplay(
		const int32 Replay,
		const FString& FixtureName,
		const ECarrierLabPhaseIIMotionFixture MotionFixture,
		const bool bEnableMarks,
		FMarkingReplayResult& OutResult)
	{
		OutResult = FMarkingReplayResult();
		OutResult.Replay = Replay;
		OutResult.FixtureName = FixtureName;
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, FixtureSamples, FixturePlates, 0.50, bEnableMarks);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier() || !ConfigureMixedTwoPlateFixture(*Actor))
		{
			Actor->Destroy();
			return false;
		}
		Actor->ConfigurePhaseIIMotionFixture(MotionFixture);

		for (int32 Step = 0; Step < MaxMarkSearchSteps; ++Step)
		{
			Actor->StepOnce();
			Actor->GetPhaseIIIB3SubductionMatrixAudit(OutResult.MatrixAudit);
			Actor->GetPhaseIIIB4PolarityAudit(OutResult.PolarityAudit);
			Actor->GetPhaseIIIC1SubductingMarkAudit(OutResult.MarkAudit);
			if (!bEnableMarks || OutResult.MarkAudit.MarkCount > 0 || MotionFixture == ECarrierLabPhaseIIMotionFixture::Zero)
			{
				break;
			}
		}

		OutResult.StepFound = OutResult.MarkAudit.Step;
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
		FCarrierLabPhaseIIIC1SubductingMarkAudit BeforeAudit;
		FCarrierLabPhaseIIIC1SubductingMarkAudit AfterAudit;
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
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, FixtureSamples, FixturePlates, 0.50, true);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier() || !ConfigureMixedTwoPlateFixture(*Actor))
		{
			Actor->Destroy();
			return false;
		}
		Actor->ConfigurePhaseIIMotionFixture(ECarrierLabPhaseIIMotionFixture::ForcedConvergence);
		for (int32 Step = 0; Step < MaxMarkSearchSteps; ++Step)
		{
			Actor->StepOnce();
			Actor->GetPhaseIIIC1SubductingMarkAudit(OutResult.BeforeAudit);
			if (OutResult.BeforeAudit.MarkCount > 0)
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
		Actor->GetPhaseIIIC1SubductingMarkAudit(OutResult.AfterAudit);

		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bCompleted = bFilterOk;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return bFilterOk;
	}

	FString MarkRecordJson(const FCarrierLabPhaseIIIC1SubductingMarkAuditRecord& Record)
	{
		return FString::Printf(
			TEXT("{\"mark_id\":%d,\"plate\":%d,\"other_plate\":%d,\"local_triangle\":%d,\"evidence_id\":%d,\"signed_local_velocity\":%.12f,\"decision\":%s}"),
			Record.MarkId,
			Record.PlateId,
			Record.OtherPlateId,
			Record.LocalTriangleId,
			Record.EvidenceId,
			Record.SignedConvergenceVelocity,
			*JsonString(PolarityClassName(Record.DecisionClass)));
	}

	void AppendMarkReplayJson(const FMarkingReplayResult& Result, TArray<FString>& Lines)
	{
		FString RecordsJson = TEXT("[");
		for (int32 Index = 0; Index < Result.MarkAudit.Records.Num(); ++Index)
		{
			if (Index > 0)
			{
				RecordsJson += TEXT(",");
			}
			RecordsJson += MarkRecordJson(Result.MarkAudit.Records[Index]);
		}
		RecordsJson += TEXT("]");

		Lines.Add(FString::Printf(
			TEXT("{\"kind\":\"marking_replay\",\"fixture\":%s,\"replay\":%d,\"completed\":%s,\"step\":%d,\"enabled\":%s,\"matrix_pairs\":%d,\"polarity_decisions\":%d,\"marks\":%d,\"invalid_marks\":%d,\"under_mismatch\":%d,\"non_subduction_decisions\":%d,\"mark_hash\":%s,\"records\":%s}"),
			*JsonString(Result.FixtureName),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.StepFound,
			Result.MarkAudit.bEnabled ? TEXT("true") : TEXT("false"),
			Result.MatrixAudit.MatrixPairCount,
			Result.PolarityAudit.DecisionCount,
			Result.MarkAudit.MarkCount,
			Result.MarkAudit.InvalidMarkCount,
			Result.MarkAudit.UnderPlateMismatchCount,
			Result.MarkAudit.NonSubductionDecisionCount,
			*JsonString(Result.MarkAudit.SubductingMarkHash),
			*RecordsJson));
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FSlice55BypassResult& BypassA,
		const FSlice55BypassResult& BypassB,
		const FMarkingReplayResult& MarksA,
		const FMarkingReplayResult& MarksB,
		const FMarkingReplayResult& ZeroMarks,
		const FResetReplayResult& Reset)
	{
		const bool bBypassPass =
			BypassA.bCompleted &&
			BypassB.bCompleted &&
			BypassA.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			BypassB.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			BypassA.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
			BypassB.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash;
		const bool bMarkingPass =
			MarksA.bCompleted &&
			MarksB.bCompleted &&
			MarksA.MarkAudit.MarkCount > 0 &&
			MarksB.MarkAudit.MarkCount == MarksA.MarkAudit.MarkCount &&
			MarksA.MarkAudit.SubductingMarkHash == MarksB.MarkAudit.SubductingMarkHash &&
			MarksA.MarkAudit.InvalidMarkCount == 0 &&
			MarksB.MarkAudit.InvalidMarkCount == 0 &&
			MarksA.MarkAudit.UnderPlateMismatchCount == 0 &&
			MarksB.MarkAudit.UnderPlateMismatchCount == 0 &&
			MarksA.MarkAudit.NonSubductionDecisionCount == 0 &&
			MarksB.MarkAudit.NonSubductionDecisionCount == 0;
		const bool bNegativePass =
			ZeroMarks.bCompleted &&
			ZeroMarks.MarkAudit.MarkCount == 0 &&
			ZeroMarks.MatrixAudit.MatrixPairCount == 0 &&
			ZeroMarks.PolarityAudit.DecisionCount == 0;
		const bool bResetPass =
			Reset.bCompleted &&
			Reset.BeforeAudit.MarkCount > 0 &&
			Reset.FilterMetrics.PersistentSubductingMarkInputCount > 0 &&
			Reset.FilterMetrics.FilteredCandidateCount > 0 &&
			Reset.AfterAudit.MarkCount == 0 &&
			Reset.AfterAudit.ResetSerial > Reset.BeforeAudit.ResetSerial;
		const bool bAllPass = bBypassPass && bMarkingPass && bNegativePass && bResetPass;

		FString Report;
		Report += TEXT("# Phase III Slice IIIC.1 Checkpoint\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("Status: first Phase IIIC mutation slice. This slice adds opt-in plate-local/per-window subducting triangle marks derived from accepted IIIB polarity evidence. It does not add trench elevation, uplift, slab pull, collision, rifting, erosion, terrain displacement, global ownership, or projection-history authority.\n\n");

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
			TEXT("| Deterministic subducting marks | %s | mark count %d / %d, hash `%s` / `%s` |\n"),
			*PassFail(bMarkingPass),
			MarksA.MarkAudit.MarkCount,
			MarksB.MarkAudit.MarkCount,
			*MarksA.MarkAudit.SubductingMarkHash,
			*MarksB.MarkAudit.SubductingMarkHash);
		Report += FString::Printf(
			TEXT("| No-admissible negative | %s | zero-motion matrix pairs %d, decisions %d, marks %d |\n"),
			*PassFail(bNegativePass),
			ZeroMarks.MatrixAudit.MatrixPairCount,
			ZeroMarks.PolarityAudit.DecisionCount,
			ZeroMarks.MarkAudit.MarkCount);
		Report += FString::Printf(
			TEXT("| Remesh reset and filter consumption | %s | before marks %d, persistent mark inputs %d, filtered candidates %d, after marks %d, reset serial %d -> %d |\n"),
			*PassFail(bResetPass),
			Reset.BeforeAudit.MarkCount,
			Reset.FilterMetrics.PersistentSubductingMarkInputCount,
			Reset.FilterMetrics.FilteredCandidateCount,
			Reset.AfterAudit.MarkCount,
			Reset.BeforeAudit.ResetSerial,
			Reset.AfterAudit.ResetSerial);

		Report += TEXT("\n## Bypass Gate\n\n");
		Report += TEXT("With `bEnablePhaseIIICSubductingMarks=false`, the fixed Slice 5.5 replay must continue to hit the accepted baseline hashes. This is a fixed-fixture regression, not a global proof that every run is unchanged.\n\n");
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

		Report += TEXT("\n## Marking Replay\n\n");
		Report += TEXT("The enabled fixture is two plates under forced convergence with plate 0 continental and plate 1 oceanic. Marks are emitted only for the under-plate triangle from accepted local IIIB evidence.\n\n");
		Report += TEXT("| Replay | Step | Matrix pairs | Decisions | Marks | Invalid | Under mismatch | Non-subduction decisions | Mark hash | Seconds |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|\n");
		auto AddMarkRow = [&Report](const FMarkingReplayResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %d | %d | %d | %d | %d | %d | %d | %d | `%s` | %.3f |\n"),
				Result.Replay,
				Result.StepFound,
				Result.MatrixAudit.MatrixPairCount,
				Result.PolarityAudit.DecisionCount,
				Result.MarkAudit.MarkCount,
				Result.MarkAudit.InvalidMarkCount,
				Result.MarkAudit.UnderPlateMismatchCount,
				Result.MarkAudit.NonSubductionDecisionCount,
				*Result.MarkAudit.SubductingMarkHash,
				Result.Seconds);
		};
		AddMarkRow(MarksA);
		AddMarkRow(MarksB);

		Report += TEXT("\n### Representative Marks\n\n");
		Report += TEXT("| Replay | Mark | Plate | Other | Local triangle | Evidence | Signed local velocity | Decision |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FCarrierLabPhaseIIIC1SubductingMarkAuditRecord& Record : MarksA.MarkAudit.Records)
		{
			Report += FString::Printf(
				TEXT("| 0 | %d | %d | %d | %d | %d | %.12f | %s |\n"),
				Record.MarkId,
				Record.PlateId,
				Record.OtherPlateId,
				Record.LocalTriangleId,
				Record.EvidenceId,
				Record.SignedConvergenceVelocity,
				*PolarityClassName(Record.DecisionClass));
		}

		Report += TEXT("\n## Negative And Reset Controls\n\n");
		Report += TEXT("| Control | Marks before | Persistent mark inputs | Filtered candidates | Marks after | Reset serial | Result |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---|---|\n");
		Report += FString::Printf(
			TEXT("| zero-motion no-admissible | %d | 0 | 0 | %d | %d | %s |\n"),
			ZeroMarks.MarkAudit.MarkCount,
			ZeroMarks.MarkAudit.MarkCount,
			ZeroMarks.MarkAudit.ResetSerial,
			*PassFail(bNegativePass));
		Report += FString::Printf(
			TEXT("| filtered resample reset | %d | %d | %d | %d | %d -> %d | %s |\n"),
			Reset.BeforeAudit.MarkCount,
			Reset.FilterMetrics.PersistentSubductingMarkInputCount,
			Reset.FilterMetrics.FilteredCandidateCount,
			Reset.AfterAudit.MarkCount,
			Reset.BeforeAudit.ResetSerial,
			Reset.AfterAudit.ResetSerial,
			*PassFail(bResetPass));

		Report += TEXT("\n## Scope Notes\n\n");
		Report += TEXT("- The new marks are plate-local/per-window process state. They are reset by the existing plate-local rebuild at remesh.\n");
		Report += TEXT("- The normal Phase II label filter path remains the source when labels are supplied. Persistent IIIC marks are additional filter inputs only when `bEnablePhaseIIICSubductingMarks=true`.\n");
		Report += TEXT("- This checkpoint may claim deterministic IIIC.1 marking and filter consumption only. It does not claim Stage 1.5 carrier success, Slice 5.5 asymmetry resolution, elevation behavior, or slab-pull correctness.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIC.1 passes. Pause for user review before IIIC.2 visible/historical elevation work.\n")
			: TEXT("IIIC.1 does not pass. Investigate the failed gate before any IIIC.2 work.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIC1Commandlet::UCarrierLabPhaseIIIC1Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIC1Commandlet::Main(const FString& Params)
{
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FSlice55BypassResult BypassA;
	FSlice55BypassResult BypassB;
	const bool bBypassA = RunSlice55Bypass(0, BypassA);
	const bool bBypassB = RunSlice55Bypass(1, BypassB);

	FMarkingReplayResult MarksA;
	FMarkingReplayResult MarksB;
	const bool bMarksA = RunMarkingReplay(0, TEXT("forced_convergence_mixed_enabled"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, true, MarksA);
	const bool bMarksB = RunMarkingReplay(1, TEXT("forced_convergence_mixed_enabled"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, true, MarksB);

	FMarkingReplayResult ZeroMarks;
	const bool bZero = RunMarkingReplay(0, TEXT("zero_motion_enabled_negative"), ECarrierLabPhaseIIMotionFixture::Zero, true, ZeroMarks);

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
	AppendMarkReplayJson(MarksA, JsonLines);
	AppendMarkReplayJson(MarksB, JsonLines);
	AppendMarkReplayJson(ZeroMarks, JsonLines);
	JsonLines.Add(FString::Printf(
		TEXT("{\"kind\":\"reset\",\"completed\":%s,\"marks_before\":%d,\"persistent_mark_inputs\":%d,\"filtered_candidates\":%d,\"marks_after\":%d,\"reset_serial_before\":%d,\"reset_serial_after\":%d,\"filter_hash\":%s,\"material_ledger_hash\":%s,\"seconds\":%.6f}"),
		bReset ? TEXT("true") : TEXT("false"),
		Reset.BeforeAudit.MarkCount,
		Reset.FilterMetrics.PersistentSubductingMarkInputCount,
		Reset.FilterMetrics.FilteredCandidateCount,
		Reset.AfterAudit.MarkCount,
		Reset.BeforeAudit.ResetSerial,
		Reset.AfterAudit.ResetSerial,
		*JsonString(Reset.FilterMetrics.FilterDecisionHash),
		*JsonString(Reset.LedgerMetrics.MaterialLedgerHash),
		Reset.Seconds));

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(FString::Join(JsonLines, TEXT("\n")) + TEXT("\n"), *MetricsPath);

	const FString Report = BuildReport(OutputRoot, BypassA, BypassB, MarksA, MarksB, ZeroMarks, Reset);
	const FString ReportPath = ResolveReportPath(Params);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	const bool bAllPass =
		bBypassA &&
		bBypassB &&
		bMarksA &&
		bMarksB &&
		bZero &&
		bReset &&
		BypassA.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
		BypassB.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
		BypassA.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
		BypassB.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
		MarksA.MarkAudit.MarkCount > 0 &&
		MarksB.MarkAudit.MarkCount == MarksA.MarkAudit.MarkCount &&
		MarksA.MarkAudit.SubductingMarkHash == MarksB.MarkAudit.SubductingMarkHash &&
		MarksA.MarkAudit.InvalidMarkCount == 0 &&
		MarksB.MarkAudit.InvalidMarkCount == 0 &&
		MarksA.MarkAudit.UnderPlateMismatchCount == 0 &&
		MarksB.MarkAudit.UnderPlateMismatchCount == 0 &&
		MarksA.MarkAudit.NonSubductionDecisionCount == 0 &&
		MarksB.MarkAudit.NonSubductionDecisionCount == 0 &&
		ZeroMarks.MarkAudit.MarkCount == 0 &&
		ZeroMarks.MatrixAudit.MatrixPairCount == 0 &&
		ZeroMarks.PolarityAudit.DecisionCount == 0 &&
		Reset.BeforeAudit.MarkCount > 0 &&
		Reset.FilterMetrics.PersistentSubductingMarkInputCount > 0 &&
		Reset.FilterMetrics.FilteredCandidateCount > 0 &&
		Reset.AfterAudit.MarkCount == 0 &&
		Reset.AfterAudit.ResetSerial > Reset.BeforeAudit.ResetSerial;

	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIIC.1 report: %s"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIIC.1 metrics: %s"), *MetricsPath);
	return bAllPass ? 0 : 1;
}
