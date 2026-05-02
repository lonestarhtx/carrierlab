// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIBConsolidationCommandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr int32 BaselineSamples = 60000;
	constexpr int32 BaselinePlates = 40;
	constexpr int32 BaselineSeed = 42;
	constexpr int32 BaselineSteps = 32;
	constexpr int32 MixedSignalSamples = 60000;
	constexpr int32 MixedSignalPlates = 2;
	constexpr int32 MixedSignalSteps = 40;
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

	void HashMix(uint64& Hash, const uint64 Value)
	{
		Hash ^= Value;
		Hash *= 1099511628211ull;
	}

	void HashMixString(uint64& Hash, const FString& Value)
	{
		HashMix(Hash, static_cast<uint64>(Value.Len() + 1));
		for (const TCHAR Ch : Value)
		{
			HashMix(Hash, static_cast<uint64>(Ch) + 1ull);
		}
	}

	FString HashToString(const uint64 Hash)
	{
		return FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Hash));
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIBConsolidation"), Stamp);
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
				TEXT("phase-iii-iiib-consolidated.md"));
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

	ACarrierLabVisualizationActor* SpawnActor(UWorld& World, const int32 SampleCount, const int32 PlateCount, const double ContinentalPlateFraction)
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
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	struct FSlice55BaselineResult
	{
		int32 Replay = 0;
		bool bCompleted = false;
		double TotalSeconds = 0.0;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterMetrics;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerMetrics;
	};

	bool RunSlice55Baseline(const int32 Replay, FSlice55BaselineResult& OutResult)
	{
		OutResult = FSlice55BaselineResult();
		OutResult.Replay = Replay;
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, BaselineSamples, BaselinePlates, 0.30);
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
		if (!Actor->DetectPhaseIIContacts(Contacts, OutResult.ContactMetrics))
		{
			Actor->Destroy();
			return false;
		}

		FCarrierLabPhaseIITriangleLabelConfig LabelConfig;
		TArray<FCarrierLabPhaseIITriangleLabelRecord> Labels;
		if (!Actor->BuildPhaseIITriangleLabels(Contacts, LabelConfig, Labels, OutResult.LabelMetrics))
		{
			Actor->Destroy();
			return false;
		}

		TArray<FCarrierLabPhaseIIFilterDecisionRecord> Decisions;
		if (!Actor->ApplyPhaseIIResamplingFilterEvent(Labels, Decisions, OutResult.FilterMetrics, nullptr, &OutResult.LedgerMetrics))
		{
			Actor->Destroy();
			return false;
		}

		OutResult.TotalSeconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bCompleted = true;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return true;
	}

	struct FIIIBSignatureResult
	{
		int32 Replay = 0;
		bool bCompleted = false;
		double TotalSeconds = 0.0;
		double PairSignedConvergenceVelocity = 0.0;
		FCarrierLabPhaseIIIB1TrackingAudit TrackingAudit;
		FCarrierLabPhaseIIIB2DistanceAudit DistanceAudit;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit MatrixAudit;
		FCarrierLabPhaseIIIB4PolarityAudit PolarityAudit;
		FCarrierLabPhaseIIIB6NeighborPropagationAudit PropagationAudit;
		FCarrierLabPhaseIIIB7HashClosureAudit ClosureAudit;
		FString RollupSignatureHash;
	};

	FString ComputeRollupSignatureHash(const FIIIBSignatureResult& Result)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIIB-signature-v1"));
		HashMix(Hash, static_cast<uint64>(MixedSignalSamples + 1));
		HashMix(Hash, static_cast<uint64>(MixedSignalPlates + 1));
		HashMix(Hash, static_cast<uint64>(MixedSignalSteps + 1));
		HashMix(Hash, static_cast<uint64>(BaselineSeed + 1));
		HashMixString(Hash, Result.TrackingAudit.ConvergenceTrackingHash);
		HashMixString(Hash, Result.DistanceAudit.ConvergenceTrackingHash);
		HashMixString(Hash, Result.MatrixAudit.ConvergenceTrackingHash);
		HashMixString(Hash, Result.PolarityAudit.PolarityHash);
		HashMixString(Hash, Result.PolarityAudit.ConvergenceTrackingHash);
		HashMixString(Hash, Result.PropagationAudit.ConvergenceTrackingHash);
		HashMixString(Hash, Result.ClosureAudit.ComputedConvergenceTrackingHash);
		HashMix(Hash, static_cast<uint64>(Result.MatrixAudit.MatrixPairCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.MatrixAudit.HitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.MatrixAudit.NonConvergentHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PropagationAudit.PropagationSeedHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PropagationAudit.PropagationAddedCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PolarityAudit.DecisionCount + 1));
		HashMix(Hash, Result.ClosureAudit.bMetricsHashMatchesComputed ? 1ull : 0ull);
		return HashToString(Hash);
	}

	bool RunSamePairMixedSignalReplay(const int32 Replay, FIIIBSignatureResult& OutResult)
	{
		OutResult = FIIIBSignatureResult();
		OutResult.Replay = Replay;
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, MixedSignalSamples, MixedSignalPlates, 0.50);
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
		if (!Actor->SetPlateContinentalForTest(0, true) ||
			!Actor->SetPlateContinentalForTest(1, false))
		{
			Actor->Destroy();
			return false;
		}
		for (int32 Step = 0; Step < MixedSignalSteps; ++Step)
		{
			Actor->StepOnce();
		}

		OutResult.PairSignedConvergenceVelocity = Actor->ComputePhaseIIPairSignedConvergenceVelocity(0, 1);
		const bool bAudits =
			Actor->GetPhaseIIIB1TrackingAudit(OutResult.TrackingAudit) &&
			Actor->GetPhaseIIIB2DistanceAudit(OutResult.DistanceAudit) &&
			Actor->GetPhaseIIIB3SubductionMatrixAudit(OutResult.MatrixAudit) &&
			Actor->GetPhaseIIIB4PolarityAudit(OutResult.PolarityAudit) &&
			Actor->GetPhaseIIIB6NeighborPropagationAudit(OutResult.PropagationAudit) &&
			Actor->GetPhaseIIIB7HashClosureAudit(OutResult.ClosureAudit);
		if (!bAudits)
		{
			Actor->Destroy();
			return false;
		}

		OutResult.RollupSignatureHash = ComputeRollupSignatureHash(OutResult);
		OutResult.TotalSeconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bCompleted = true;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return true;
	}

	bool BaselineMatchesExpected(const FSlice55BaselineResult& Result)
	{
		return Result.bCompleted &&
			Result.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			Result.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash;
	}

	bool BaselineReplayStable(const FSlice55BaselineResult& A, const FSlice55BaselineResult& B)
	{
		return A.bCompleted && B.bCompleted &&
			A.ContactMetrics.ContactLogHash == B.ContactMetrics.ContactLogHash &&
			A.LabelMetrics.TriangleLabelHash == B.LabelMetrics.TriangleLabelHash &&
			A.FilterMetrics.FilterDecisionHash == B.FilterMetrics.FilterDecisionHash &&
			A.FilterMetrics.StateHashAfter == B.FilterMetrics.StateHashAfter &&
			A.LedgerMetrics.MaterialLedgerHash == B.LedgerMetrics.MaterialLedgerHash;
	}

	bool SamePairMixedSignalPasses(const FIIIBSignatureResult& Result)
	{
		return Result.bCompleted &&
			Result.MatrixAudit.MatrixPairCount == 1 &&
			Result.MatrixAudit.ProbePlateA == 0 &&
			Result.MatrixAudit.ProbePlateB == 1 &&
			Result.MatrixAudit.HitCount > 0 &&
			Result.MatrixAudit.NonConvergentHitCount > 0 &&
			Result.MatrixAudit.ProbeSignedConvergenceVelocity > 0.0 &&
			Result.PolarityAudit.DecisionCount == 1 &&
			Result.PropagationAudit.PropagationSeedHitCount > 0 &&
			Result.PropagationAudit.PropagationAddedCount > 0 &&
			Result.ClosureAudit.bMetricsHashMatchesComputed;
	}

	bool SignatureReplayStable(const FIIIBSignatureResult& A, const FIIIBSignatureResult& B)
	{
		return A.bCompleted && B.bCompleted &&
			A.RollupSignatureHash == B.RollupSignatureHash &&
			A.TrackingAudit.ConvergenceTrackingHash == B.TrackingAudit.ConvergenceTrackingHash &&
			A.DistanceAudit.ConvergenceTrackingHash == B.DistanceAudit.ConvergenceTrackingHash &&
			A.MatrixAudit.ConvergenceTrackingHash == B.MatrixAudit.ConvergenceTrackingHash &&
			A.PolarityAudit.PolarityHash == B.PolarityAudit.PolarityHash &&
			A.PolarityAudit.ConvergenceTrackingHash == B.PolarityAudit.ConvergenceTrackingHash &&
			A.PropagationAudit.ConvergenceTrackingHash == B.PropagationAudit.ConvergenceTrackingHash &&
			A.ClosureAudit.ComputedConvergenceTrackingHash == B.ClosureAudit.ComputedConvergenceTrackingHash;
	}

	FString BaselineJson(const FSlice55BaselineResult& Result)
	{
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		return FString::Printf(
			TEXT("{\"fixture\":\"slice55_baseline_replay\",\"replay\":%d,\"samples\":%d,\"plates\":%d,\"steps\":%d,\"completed\":%s,\"seconds\":%.6f,\"contact_hash\":%s,\"label_hash\":%s,\"filter_decision_hash\":%s,\"state_hash_after\":%s,\"material_ledger_hash\":%s,\"expected_state_hash\":%s,\"expected_material_ledger_hash\":%s,\"state_hash_matches_expected\":%s,\"material_ledger_matches_expected\":%s,\"memory_gb\":%.12f}"),
			Result.Replay,
			BaselineSamples,
			BaselinePlates,
			BaselineSteps,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.TotalSeconds,
			*JsonString(Result.ContactMetrics.ContactLogHash),
			*JsonString(Result.LabelMetrics.TriangleLabelHash),
			*JsonString(Result.FilterMetrics.FilterDecisionHash),
			*JsonString(Result.FilterMetrics.StateHashAfter),
			*JsonString(Result.LedgerMetrics.MaterialLedgerHash),
			*JsonString(ExpectedSlice55StateHash),
			*JsonString(ExpectedSlice55MaterialLedgerHash),
			Result.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash ? TEXT("true") : TEXT("false"),
			Result.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash ? TEXT("true") : TEXT("false"),
			MemoryGb);
	}

	FString SignatureJson(const FIIIBSignatureResult& Result)
	{
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		return FString::Printf(
			TEXT("{\"fixture\":\"same_pair_mixed_signal\",\"replay\":%d,\"samples\":%d,\"plates\":%d,\"steps\":%d,\"completed\":%s,\"seconds\":%.6f,\"pair_signed_convergence_velocity\":%.12f,\"probe_local_signed_convergence_velocity\":%.12f,\"active_triangles\":%d,\"distance_records\":%d,\"matrix_pairs\":%d,\"matrix_hits\":%d,\"matrix_nonconvergent_hits\":%d,\"polarity_decisions\":%d,\"polarity_hash\":%s,\"propagation_seed_hits\":%d,\"propagation_added\":%d,\"closure_metrics_hash\":%s,\"closure_computed_hash\":%s,\"closure_matches\":%s,\"iiib_rollup_signature_hash\":%s,\"memory_gb\":%.12f}"),
			Result.Replay,
			MixedSignalSamples,
			MixedSignalPlates,
			MixedSignalSteps,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.TotalSeconds,
			Result.PairSignedConvergenceVelocity,
			Result.MatrixAudit.ProbeSignedConvergenceVelocity,
			Result.TrackingAudit.ActiveBoundaryTriangleCount,
			Result.DistanceAudit.DistanceRecordCount,
			Result.MatrixAudit.MatrixPairCount,
			Result.MatrixAudit.HitCount,
			Result.MatrixAudit.NonConvergentHitCount,
			Result.PolarityAudit.DecisionCount,
			*JsonString(Result.PolarityAudit.PolarityHash),
			Result.PropagationAudit.PropagationSeedHitCount,
			Result.PropagationAudit.PropagationAddedCount,
			*JsonString(Result.ClosureAudit.MetricsConvergenceTrackingHash),
			*JsonString(Result.ClosureAudit.ComputedConvergenceTrackingHash),
			Result.ClosureAudit.bMetricsHashMatchesComputed ? TEXT("true") : TEXT("false"),
			*JsonString(Result.RollupSignatureHash),
			MemoryGb);
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FSlice55BaselineResult& BaselineA,
		const FSlice55BaselineResult& BaselineB,
		const FIIIBSignatureResult& MixedA,
		const FIIIBSignatureResult& MixedB)
	{
		const bool bBaselineReplay = BaselineReplayStable(BaselineA, BaselineB);
		const bool bBaselineExpected = BaselineMatchesExpected(BaselineA) && BaselineMatchesExpected(BaselineB);
		const bool bSamePairMixed = SamePairMixedSignalPasses(MixedA) && SamePairMixedSignalPasses(MixedB);
		const bool bRollupReplay = SignatureReplayStable(MixedA, MixedB);
		const bool bAllPass = bBaselineReplay && bBaselineExpected && bSamePairMixed && bRollupReplay;

		FString Report = TEXT("# Phase III Sub-phase IIIB Consolidation Checkpoint\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This checkpoint closes IIIB.1-IIIB.7 as a consolidated read-only convergence-tracking sub-phase. It adds no subduction mutation, no triangle consumption, no material transfer, and no new global ownership authority. The gates below exist to catch exactly the lingering review threads: Phase II Slice 5.5 regression, local mixed-signal convergence evidence, and a single rollup signature for future Phase III regression checks.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n|---|---|---|\n");
		Report += FString::Printf(
			TEXT("| Slice 5.5 baseline replay regression | %s | replay hashes stable: %s; state `%s`, material ledger `%s` |\n"),
			*PassFail(bBaselineReplay && bBaselineExpected),
			*PassFail(bBaselineReplay),
			*BaselineA.FilterMetrics.StateHashAfter,
			*BaselineA.LedgerMetrics.MaterialLedgerHash);
		Report += FString::Printf(
			TEXT("| Same-pair mixed-signal local gate | %s | pair signed %.12f, local probe %.12f, hits %d, non-convergent %d, propagated %d |\n"),
			*PassFail(bSamePairMixed),
			MixedA.PairSignedConvergenceVelocity,
			MixedA.MatrixAudit.ProbeSignedConvergenceVelocity,
			MixedA.MatrixAudit.HitCount,
			MixedA.MatrixAudit.NonConvergentHitCount,
			MixedA.PropagationAudit.PropagationAddedCount);
		Report += FString::Printf(
			TEXT("| IIIB rollup signature replay | %s | `%s` vs `%s` |\n"),
			*PassFail(bRollupReplay),
			*MixedA.RollupSignatureHash,
			*MixedB.RollupSignatureHash);

		Report += TEXT("\n## Slice 5.5 Baseline Regression\n\n");
		Report += TEXT("| Replay | State hash | Material ledger hash | Contact hash | Label hash | Filter hash | Expected state | Expected ledger | Seconds |\n");
		Report += TEXT("|---:|---|---|---|---|---|---|---|---:|\n");
		auto AddBaselineRow = [&Report](const FSlice55BaselineResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %d | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | %.3f |\n"),
				Result.Replay,
				*Result.FilterMetrics.StateHashAfter,
				*Result.LedgerMetrics.MaterialLedgerHash,
				*Result.ContactMetrics.ContactLogHash,
				*Result.LabelMetrics.TriangleLabelHash,
				*Result.FilterMetrics.FilterDecisionHash,
				ExpectedSlice55StateHash,
				ExpectedSlice55MaterialLedgerHash,
				Result.TotalSeconds);
		};
		AddBaselineRow(BaselineA);
		AddBaselineRow(BaselineB);

		Report += TEXT("\n## Same-Pair Mixed-Signal Fixture\n\n");
		Report += TEXT("Fixture: two plates, forced-divergence motion, 40 steps. The closed sphere produces both locally convergent and locally divergent/non-convergent evidence on the same plate pair. The matrix and propagation gates therefore prove local hit location controls IIIB state, not a blanket pair-wide classification.\n\n");
		Report += TEXT("| Replay | Pair sign | Probe local sign | Matrix pairs | Hits | Non-convergent | Decisions | Seed hits | Added | Closure hash | Rollup signature |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|\n");
		auto AddSignatureRow = [&Report](const FIIIBSignatureResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %d | %.12f | %.12f | %d | %d | %d | %d | %d | %d | `%s` | `%s` |\n"),
				Result.Replay,
				Result.PairSignedConvergenceVelocity,
				Result.MatrixAudit.ProbeSignedConvergenceVelocity,
				Result.MatrixAudit.MatrixPairCount,
				Result.MatrixAudit.HitCount,
				Result.MatrixAudit.NonConvergentHitCount,
				Result.PolarityAudit.DecisionCount,
				Result.PropagationAudit.PropagationSeedHitCount,
				Result.PropagationAudit.PropagationAddedCount,
				*Result.ClosureAudit.ComputedConvergenceTrackingHash,
				*Result.RollupSignatureHash);
		};
		AddSignatureRow(MixedA);
		AddSignatureRow(MixedB);

		Report += TEXT("\n## IIIB Rollup Signature Components\n\n");
		Report += TEXT("| Component | Replay 0 hash | Replay 1 hash |\n|---|---|---|\n");
		Report += FString::Printf(TEXT("| IIIB.1 active list | `%s` | `%s` |\n"), *MixedA.TrackingAudit.ConvergenceTrackingHash, *MixedB.TrackingAudit.ConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| IIIB.2 distance-to-front | `%s` | `%s` |\n"), *MixedA.DistanceAudit.ConvergenceTrackingHash, *MixedB.DistanceAudit.ConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| IIIB.3 matrix | `%s` | `%s` |\n"), *MixedA.MatrixAudit.ConvergenceTrackingHash, *MixedB.MatrixAudit.ConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| IIIB.4 polarity | `%s` | `%s` |\n"), *MixedA.PolarityAudit.PolarityHash, *MixedB.PolarityAudit.PolarityHash);
		Report += FString::Printf(TEXT("| IIIB.6 propagation | `%s` | `%s` |\n"), *MixedA.PropagationAudit.ConvergenceTrackingHash, *MixedB.PropagationAudit.ConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| IIIB.7 closure | `%s` | `%s` |\n"), *MixedA.ClosureAudit.ComputedConvergenceTrackingHash, *MixedB.ClosureAudit.ComputedConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| Consolidated IIIB signature | `%s` | `%s` |\n"), *MixedA.RollupSignatureHash, *MixedB.RollupSignatureHash);

		Report += TEXT("\n## Notes\n\n");
		Report += TEXT("- This checkpoint intentionally corrects IIIB.3's earlier pair-level interpretation: matrix admission is now based on local signed convergence at the active triangle barycenter. Pair keys remain canonical metadata, not the convergence oracle.\n");
		Report += TEXT("- The same-pair fixture requires accepted local hits and rejected non-convergent hits on the same plate pair. The pair-level sign is reported for context only; it is not the matrix admission oracle.\n");
		Report += TEXT("- The consolidated signature is the Phase IIIB regression token future Phase III sub-phases should quote when they need to prove convergence tracking did not drift.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIB consolidation passes. Pause for user review before Phase IIIC planning or implementation.\n")
			: TEXT("IIIB consolidation does not pass. Investigate before Phase IIIC.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIBConsolidationCommandlet::UCarrierLabPhaseIIIBConsolidationCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIBConsolidationCommandlet::Main(const FString& Params)
{
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FSlice55BaselineResult BaselineA;
	FSlice55BaselineResult BaselineB;
	const bool bBaselineA = RunSlice55Baseline(0, BaselineA);
	const bool bBaselineB = RunSlice55Baseline(1, BaselineB);

	FIIIBSignatureResult MixedA;
	FIIIBSignatureResult MixedB;
	const bool bMixedA = RunSamePairMixedSignalReplay(0, MixedA);
	const bool bMixedB = RunSamePairMixedSignalReplay(1, MixedB);

	FString MetricsJsonl;
	MetricsJsonl += BaselineJson(BaselineA) + LINE_TERMINATOR;
	MetricsJsonl += BaselineJson(BaselineB) + LINE_TERMINATOR;
	MetricsJsonl += SignatureJson(MixedA) + LINE_TERMINATOR;
	MetricsJsonl += SignatureJson(MixedB) + LINE_TERMINATOR;
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);

	const FString Report = BuildReport(OutputRoot, BaselineA, BaselineB, MixedA, MixedB);
	const FString ReportPath = ResolveReportPath(Params);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB consolidation metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB consolidation report: %s"), *ReportPath);

	const bool bBaseline = bBaselineA && bBaselineB &&
		BaselineReplayStable(BaselineA, BaselineB) &&
		BaselineMatchesExpected(BaselineA) &&
		BaselineMatchesExpected(BaselineB);
	const bool bMixedSignal = bMixedA && bMixedB &&
		SamePairMixedSignalPasses(MixedA) &&
		SamePairMixedSignalPasses(MixedB) &&
		SignatureReplayStable(MixedA, MixedB);

	if (!(bBaseline && bMixedSignal))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab Phase IIIB consolidation gates failed."));
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB consolidation gates passed."));
	return 0;
}
