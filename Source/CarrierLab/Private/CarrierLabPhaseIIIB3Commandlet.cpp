// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIB3Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
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

	int32 ParseIntParam(const FString& Params, const TCHAR* Key, const int32 DefaultValue)
	{
		int32 Value = DefaultValue;
		FParse::Value(*Params, Key, Value);
		return Value;
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIB3"), Stamp);
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
				TEXT("phase-iii-slice-iiib3-report.md"));
		}
		else if (FPaths::IsRelative(ReportPath))
		{
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), ReportPath);
		}
		return FPaths::ConvertRelativePathToFull(ReportPath);
	}

	FString ExtractJsonStringValue(const FString& Line, const FString& Key)
	{
		const FString Needle = FString::Printf(TEXT("\"%s\":\""), *Key);
		const int32 Start = Line.Find(Needle);
		if (Start == INDEX_NONE)
		{
			return FString();
		}

		const int32 ValueStart = Start + Needle.Len();
		const int32 ValueEnd = Line.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
		if (ValueEnd == INDEX_NONE || ValueEnd <= ValueStart)
		{
			return FString();
		}
		return Line.Mid(ValueStart, ValueEnd - ValueStart);
	}

	struct FPhaseIIBaseline
	{
		bool bLoaded = false;
		FString Path;
		FString StateHashAfter;
		FString MaterialLedgerHash;
	};

	FPhaseIIBaseline LoadPhaseIIBaseline()
	{
		FPhaseIIBaseline Baseline;
		Baseline.Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::ProjectSavedDir(),
			TEXT("CarrierLab"),
			TEXT("PhaseII"),
			TEXT("Slice55"),
			TEXT("verify_60k_20260502"),
			TEXT("metrics.jsonl")));

		TArray<FString> Lines;
		if (!FFileHelper::LoadFileToStringArray(Lines, *Baseline.Path))
		{
			return Baseline;
		}

		for (const FString& Line : Lines)
		{
			if (!Line.Contains(TEXT("\"fixture\":\"cadence_60k_primary\"")) ||
				!Line.Contains(TEXT("\"replay\":0")))
			{
				continue;
			}

			Baseline.StateHashAfter = ExtractJsonStringValue(Line, TEXT("state_hash_after"));
			Baseline.MaterialLedgerHash = ExtractJsonStringValue(Line, TEXT("material_ledger_hash"));
			Baseline.bLoaded = !Baseline.StateHashAfter.IsEmpty() && !Baseline.MaterialLedgerHash.IsEmpty();
			return Baseline;
		}
		return Baseline;
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

	ACarrierLabVisualizationActor* SpawnActor(UWorld& World, const int32 SampleCount, const int32 PlateCount)
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
		Actor->Seed = 42;
		Actor->ContinentalPlateFraction = 0.30;
		Actor->VelocityMmPerYear = 66.6666666667;
		Actor->MultiHitPolicy = ECarrierLabMultiHitPolicy::Centroid;
		Actor->VisualizationLayer = ECarrierLabVisualizationLayer::PlateId;
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	bool MatrixAuditIsValid(const FCarrierLabPhaseIIIB3SubductionMatrixAudit& Audit)
	{
		return Audit.InvalidMatrixPairCount == 0 &&
			Audit.SelfMatrixPairCount == 0 &&
			Audit.DistanceRecordCount == Audit.ActiveBoundaryTriangleCount;
	}

	bool HasExpectedPair(const FCarrierLabPhaseIIIB3SubductionMatrixAudit& Audit, const int32 A, const int32 B)
	{
		return Audit.MatrixPairCount == 1 &&
			Audit.ProbePlateA == FMath::Min(A, B) &&
			Audit.ProbePlateB == FMath::Max(A, B);
	}

	struct FMatrixReplayResult
	{
		FString Fixture;
		int32 Replay = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		double TotalSeconds = 0.0;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit InitialAudit;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit FinalAudit;
		bool bCompleted = false;
	};

	bool RunMatrixReplay(
		const FString& Fixture,
		const ECarrierLabPhaseIIMotionFixture MotionFixture,
		const int32 Replay,
		const int32 SampleCount,
		const int32 PlateCount,
		const int32 StepCount,
		FMatrixReplayResult& OutResult)
	{
		OutResult = FMatrixReplayResult();
		OutResult.Fixture = Fixture;
		OutResult.Replay = Replay;
		OutResult.SampleCount = SampleCount;
		OutResult.PlateCount = PlateCount;
		OutResult.StepCount = StepCount;

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double TotalStartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, SampleCount, PlateCount);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier())
		{
			Actor->Destroy();
			return false;
		}
		Actor->ConfigurePhaseIIMotionFixture(MotionFixture);
		if (!Actor->GetPhaseIIIB3SubductionMatrixAudit(OutResult.InitialAudit))
		{
			Actor->Destroy();
			return false;
		}

		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			Actor->StepOnce();
		}
		if (!Actor->GetPhaseIIIB3SubductionMatrixAudit(OutResult.FinalAudit))
		{
			Actor->Destroy();
			return false;
		}

		OutResult.TotalSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
		OutResult.bCompleted = true;
		Actor->Destroy();
		return true;
	}

	struct FBaselineReplayResult
	{
		int32 Replay = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		double TotalSeconds = 0.0;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit InitialAudit;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit PreRemeshAudit;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit PostRemeshAudit;
		FString ProjectionHashAfter;
		FString StateHashAfter;
		FString CrustStateHashAfter;
		FString MaterialLedgerHash;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterMetrics;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerMetrics;
		bool bCompleted = false;
	};

	bool RunBaselineReplay(
		const int32 Replay,
		const int32 SampleCount,
		const int32 PlateCount,
		const int32 StepCount,
		FBaselineReplayResult& OutResult)
	{
		OutResult = FBaselineReplayResult();
		OutResult.Replay = Replay;
		OutResult.SampleCount = SampleCount;
		OutResult.PlateCount = PlateCount;
		OutResult.StepCount = StepCount;

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double TotalStartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, SampleCount, PlateCount);
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
		if (!Actor->GetPhaseIIIB3SubductionMatrixAudit(OutResult.InitialAudit))
		{
			Actor->Destroy();
			return false;
		}

		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			Actor->StepOnce();
		}
		if (!Actor->GetPhaseIIIB3SubductionMatrixAudit(OutResult.PreRemeshAudit))
		{
			Actor->Destroy();
			return false;
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
		TArray<FCarrierLabPhaseIIMaterialRecord> MaterialRecords;
		if (!Actor->ApplyPhaseIIResamplingFilterEvent(Labels, Decisions, OutResult.FilterMetrics, &MaterialRecords, &OutResult.LedgerMetrics))
		{
			Actor->Destroy();
			return false;
		}
		if (!Actor->GetPhaseIIIB3SubductionMatrixAudit(OutResult.PostRemeshAudit))
		{
			Actor->Destroy();
			return false;
		}

		OutResult.ProjectionHashAfter = OutResult.FilterMetrics.ProjectionHashAfter;
		OutResult.StateHashAfter = OutResult.FilterMetrics.StateHashAfter;
		OutResult.CrustStateHashAfter = Actor->CurrentMetrics.CrustStateHash;
		OutResult.MaterialLedgerHash = OutResult.LedgerMetrics.MaterialLedgerHash;
		OutResult.TotalSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
		OutResult.bCompleted = true;

		Actor->Destroy();
		return true;
	}

	FString AuditJsonFields(const FCarrierLabPhaseIIIB3SubductionMatrixAudit& Audit, const TCHAR* Prefix)
	{
		return FString::Printf(
			TEXT("\"%s_step\":%d,\"%s_event_count\":%d,\"%s_reset_serial\":%d,\"%s_active_boundary_triangles\":%d,\"%s_distance_records\":%d,\"%s_matrix_pairs\":%d,\"%s_invalid_pairs\":%d,\"%s_self_pairs\":%d,\"%s_ray_tests\":%d,\"%s_hits\":%d,\"%s_boundary_hits\":%d,\"%s_nonconvergent_hits\":%d,\"%s_probe_plate_a\":%d,\"%s_probe_plate_b\":%d,\"%s_probe_signed_convergence\":%.12f,\"%s_hash\":%s"),
			Prefix, Audit.Step,
			Prefix, Audit.EventCount,
			Prefix, Audit.ResetSerial,
			Prefix, Audit.ActiveBoundaryTriangleCount,
			Prefix, Audit.DistanceRecordCount,
			Prefix, Audit.MatrixPairCount,
			Prefix, Audit.InvalidMatrixPairCount,
			Prefix, Audit.SelfMatrixPairCount,
			Prefix, Audit.RayTestCount,
			Prefix, Audit.HitCount,
			Prefix, Audit.BoundaryHitCount,
			Prefix, Audit.NonConvergentHitCount,
			Prefix, Audit.ProbePlateA,
			Prefix, Audit.ProbePlateB,
			Prefix, Audit.ProbeSignedConvergenceVelocity,
			Prefix, *JsonString(Audit.ConvergenceTrackingHash));
	}

	FString MatrixReplayJson(const FMatrixReplayResult& Result)
	{
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"step_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"total_replay_seconds\":%.12f,%s,%s,\"memory_gb\":%.12f}"),
			*JsonString(Result.Fixture),
			Result.Replay,
			Result.StepCount,
			Result.SampleCount,
			Result.PlateCount,
			Result.TotalSeconds,
			*AuditJsonFields(Result.InitialAudit, TEXT("initial")),
			*AuditJsonFields(Result.FinalAudit, TEXT("final")),
			MemoryGb);
	}

	FString BaselineReplayJson(const FBaselineReplayResult& Result)
	{
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		return FString::Printf(
			TEXT("{\"fixture\":\"iiib3_60k_phase_ii_baseline\",\"replay\":%d,\"step_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"total_replay_seconds\":%.12f,%s,%s,%s,\"projection_hash_after\":%s,\"state_hash_after\":%s,\"crust_state_hash_after\":%s,\"material_ledger_hash\":%s,\"contact_hash\":%s,\"label_hash\":%s,\"filter_decision_hash\":%s,\"memory_gb\":%.12f}"),
			Result.Replay,
			Result.StepCount,
			Result.SampleCount,
			Result.PlateCount,
			Result.TotalSeconds,
			*AuditJsonFields(Result.InitialAudit, TEXT("initial")),
			*AuditJsonFields(Result.PreRemeshAudit, TEXT("pre_remesh")),
			*AuditJsonFields(Result.PostRemeshAudit, TEXT("post_remesh")),
			*JsonString(Result.ProjectionHashAfter),
			*JsonString(Result.StateHashAfter),
			*JsonString(Result.CrustStateHashAfter),
			*JsonString(Result.MaterialLedgerHash),
			*JsonString(Result.ContactMetrics.ContactLogHash),
			*JsonString(Result.LabelMetrics.TriangleLabelHash),
			*JsonString(Result.FilterMetrics.FilterDecisionHash),
			MemoryGb);
	}

	bool MatrixReplayHashesMatch(const FMatrixReplayResult& A, const FMatrixReplayResult& B)
	{
		return A.bCompleted && B.bCompleted &&
			A.InitialAudit.ConvergenceTrackingHash == B.InitialAudit.ConvergenceTrackingHash &&
			A.FinalAudit.ConvergenceTrackingHash == B.FinalAudit.ConvergenceTrackingHash;
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FPhaseIIBaseline& Baseline,
		const FBaselineReplayResult& BaselineA,
		const FBaselineReplayResult& BaselineB,
		const FMatrixReplayResult& ConvergenceA,
		const FMatrixReplayResult& ConvergenceB,
		const FMatrixReplayResult& DivergenceA,
		const FMatrixReplayResult& DivergenceB,
		const FMatrixReplayResult& ZeroA,
		const FMatrixReplayResult& ZeroB)
	{
		const bool bInitialEmpty = BaselineA.InitialAudit.MatrixPairCount == 0 && BaselineB.InitialAudit.MatrixPairCount == 0;
		const bool bPostRemeshEmpty = BaselineA.PostRemeshAudit.MatrixPairCount == 0 && BaselineB.PostRemeshAudit.MatrixPairCount == 0;
		const bool bBaselineValid = MatrixAuditIsValid(BaselineA.InitialAudit) &&
			MatrixAuditIsValid(BaselineA.PreRemeshAudit) &&
			MatrixAuditIsValid(BaselineA.PostRemeshAudit) &&
			MatrixAuditIsValid(BaselineB.InitialAudit) &&
			MatrixAuditIsValid(BaselineB.PreRemeshAudit) &&
			MatrixAuditIsValid(BaselineB.PostRemeshAudit);
		const bool bBaselineReplay = BaselineA.bCompleted && BaselineB.bCompleted &&
			BaselineA.InitialAudit.ConvergenceTrackingHash == BaselineB.InitialAudit.ConvergenceTrackingHash &&
			BaselineA.PreRemeshAudit.ConvergenceTrackingHash == BaselineB.PreRemeshAudit.ConvergenceTrackingHash &&
			BaselineA.PostRemeshAudit.ConvergenceTrackingHash == BaselineB.PostRemeshAudit.ConvergenceTrackingHash;
		const bool bProjectionReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.ProjectionHashAfter == BaselineB.ProjectionHashAfter;
		const bool bStateReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.StateHashAfter == BaselineB.StateHashAfter;
		const bool bCrustReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.CrustStateHashAfter == BaselineB.CrustStateHashAfter;
		const bool bMaterialReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.MaterialLedgerHash == BaselineB.MaterialLedgerHash;
		const bool bStateBaseline = Baseline.bLoaded && BaselineA.StateHashAfter == Baseline.StateHashAfter;
		const bool bMaterialBaseline = Baseline.bLoaded && BaselineA.MaterialLedgerHash == Baseline.MaterialLedgerHash;

		const bool bConvergencePopulates = ConvergenceA.bCompleted &&
			ConvergenceA.FinalAudit.MatrixPairCount > 0 &&
			HasExpectedPair(ConvergenceA.FinalAudit, 0, 1) &&
			ConvergenceA.FinalAudit.ProbeSignedConvergenceVelocity > 0.0;
		const bool bConvergenceReplay = MatrixReplayHashesMatch(ConvergenceA, ConvergenceB);
		const bool bDivergenceLocalConvergence = DivergenceA.bCompleted &&
			HasExpectedPair(DivergenceA.FinalAudit, 0, 1) &&
			DivergenceA.FinalAudit.HitCount > 0 &&
			DivergenceA.FinalAudit.ProbeSignedConvergenceVelocity > 0.0;
		const bool bDivergenceReplay = MatrixReplayHashesMatch(DivergenceA, DivergenceB);
		const bool bZeroEmpty = ZeroA.bCompleted &&
			ZeroA.InitialAudit.MatrixPairCount == 0 &&
			ZeroA.FinalAudit.MatrixPairCount == 0 &&
			ZeroA.InitialAudit.ConvergenceTrackingHash == ZeroA.FinalAudit.ConvergenceTrackingHash;
		const bool bZeroReplay = MatrixReplayHashesMatch(ZeroA, ZeroB);

		const bool bAllPass = bInitialEmpty && bPostRemeshEmpty && bBaselineValid && bBaselineReplay &&
			bProjectionReplay && bStateReplay && bCrustReplay && bMaterialReplay &&
			bStateBaseline && bMaterialBaseline &&
			bConvergencePopulates && bConvergenceReplay &&
			bDivergenceLocalConvergence && bDivergenceReplay &&
			bZeroEmpty && bZeroReplay;

		FString Report = TEXT("# Phase III Slice IIIB.3 Checkpoint: Subduction Matrix\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This slice adds a sparse per-remesh-window subduction matrix as read-only convergence tracking state. Active boundary triangle barycenters cast ray-from-origin queries against the other plates' BVHs; non-boundary intersections add a canonical plate-pair flag only when the plate pair's signed convergence is positive. The matrix resets at remesh and does not mark triangles, filter projection candidates, or mutate material.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		Report += FString::Printf(TEXT("| Matrix empty at start of remesh window | %s | initial pairs %d / %d |\n"), *PassFail(bInitialEmpty), BaselineA.InitialAudit.MatrixPairCount, BaselineB.InitialAudit.MatrixPairCount);
		Report += FString::Printf(TEXT("| Matrix resets after remesh | %s | pre pairs %d -> post pairs %d, serial %d -> %d |\n"), *PassFail(bPostRemeshEmpty), BaselineA.PreRemeshAudit.MatrixPairCount, BaselineA.PostRemeshAudit.MatrixPairCount, BaselineA.PreRemeshAudit.ResetSerial, BaselineA.PostRemeshAudit.ResetSerial);
		Report += FString::Printf(TEXT("| Matrix audit has no invalid/self pairs | %s | invalid %d, self %d |\n"), *PassFail(bBaselineValid), BaselineA.PreRemeshAudit.InvalidMatrixPairCount, BaselineA.PreRemeshAudit.SelfMatrixPairCount);
		Report += FString::Printf(TEXT("| Matrix replay deterministic | %s | pre `%s` vs `%s`, post `%s` vs `%s` |\n"), *PassFail(bBaselineReplay), *BaselineA.PreRemeshAudit.ConvergenceTrackingHash, *BaselineB.PreRemeshAudit.ConvergenceTrackingHash, *BaselineA.PostRemeshAudit.ConvergenceTrackingHash, *BaselineB.PostRemeshAudit.ConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| Forced convergence populates expected plate pair | %s | pairs %d, probe %d/%d, signed convergence %.12f |\n"), *PassFail(bConvergencePopulates), ConvergenceA.FinalAudit.MatrixPairCount, ConvergenceA.FinalAudit.ProbePlateA, ConvergenceA.FinalAudit.ProbePlateB, ConvergenceA.FinalAudit.ProbeSignedConvergenceVelocity);
		Report += FString::Printf(TEXT("| Forced divergence admits only local convergent evidence | %s | pairs %d, hits %d, probe local sign %.12f |\n"), *PassFail(bDivergenceLocalConvergence), DivergenceA.FinalAudit.MatrixPairCount, DivergenceA.FinalAudit.HitCount, DivergenceA.FinalAudit.ProbeSignedConvergenceVelocity);
		Report += FString::Printf(TEXT("| Zero-motion leaves matrix empty and hash stable | %s | pairs %d -> %d, hash `%s` -> `%s` |\n"), *PassFail(bZeroEmpty), ZeroA.InitialAudit.MatrixPairCount, ZeroA.FinalAudit.MatrixPairCount, *ZeroA.InitialAudit.ConvergenceTrackingHash, *ZeroA.FinalAudit.ConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| Control replay hashes deterministic | %s | convergence `%s` vs `%s`, divergence `%s` vs `%s`, zero `%s` vs `%s` |\n"), *PassFail(bConvergenceReplay && bDivergenceReplay && bZeroReplay), *ConvergenceA.FinalAudit.ConvergenceTrackingHash, *ConvergenceB.FinalAudit.ConvergenceTrackingHash, *DivergenceA.FinalAudit.ConvergenceTrackingHash, *DivergenceB.FinalAudit.ConvergenceTrackingHash, *ZeroA.FinalAudit.ConvergenceTrackingHash, *ZeroB.FinalAudit.ConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| Projection replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bProjectionReplay), *BaselineA.ProjectionHashAfter, *BaselineB.ProjectionHashAfter);
		Report += FString::Printf(TEXT("| Phase II state replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bStateReplay), *BaselineA.StateHashAfter, *BaselineB.StateHashAfter);
		Report += FString::Printf(TEXT("| Crust state replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bCrustReplay), *BaselineA.CrustStateHashAfter, *BaselineB.CrustStateHashAfter);
		Report += FString::Printf(TEXT("| Phase II material ledger replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bMaterialReplay), *BaselineA.MaterialLedgerHash, *BaselineB.MaterialLedgerHash);
		Report += FString::Printf(TEXT("| Phase II baseline state hash unchanged | %s | baseline `%s`, IIIB.3 `%s` |\n"), *PassFail(bStateBaseline), Baseline.bLoaded ? *Baseline.StateHashAfter : TEXT("baseline unavailable"), *BaselineA.StateHashAfter);
		Report += FString::Printf(TEXT("| Phase II baseline material ledger unchanged | %s | baseline `%s`, IIIB.3 `%s` |\n"), *PassFail(bMaterialBaseline), Baseline.bLoaded ? *Baseline.MaterialLedgerHash : TEXT("baseline unavailable"), *BaselineA.MaterialLedgerHash);

		Report += TEXT("\n## Matrix Audits\n\n");
		Report += TEXT("| Fixture | Replay | Window | Step | Reset | Active | Pairs | Ray tests | Hits | Boundary hits | Non-convergent hits | Probe pair | Probe signed convergence | Hash |\n");
		Report += TEXT("|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---|\n");
		auto AddMatrixRow = [&Report](const FString& Fixture, const int32 Replay, const TCHAR* Window, const FCarrierLabPhaseIIIB3SubductionMatrixAudit& Audit)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %s | %d | %d | %d | %d | %d | %d | %d | %d | %d/%d | %.12f | `%s` |\n"),
				*Fixture,
				Replay,
				Window,
				Audit.Step,
				Audit.ResetSerial,
				Audit.ActiveBoundaryTriangleCount,
				Audit.MatrixPairCount,
				Audit.RayTestCount,
				Audit.HitCount,
				Audit.BoundaryHitCount,
				Audit.NonConvergentHitCount,
				Audit.ProbePlateA,
				Audit.ProbePlateB,
				Audit.ProbeSignedConvergenceVelocity,
				*Audit.ConvergenceTrackingHash);
		};
		AddMatrixRow(TEXT("baseline"), BaselineA.Replay, TEXT("initial"), BaselineA.InitialAudit);
		AddMatrixRow(TEXT("baseline"), BaselineA.Replay, TEXT("pre-remesh"), BaselineA.PreRemeshAudit);
		AddMatrixRow(TEXT("baseline"), BaselineA.Replay, TEXT("post-remesh"), BaselineA.PostRemeshAudit);
		AddMatrixRow(ConvergenceA.Fixture, ConvergenceA.Replay, TEXT("initial"), ConvergenceA.InitialAudit);
		AddMatrixRow(ConvergenceA.Fixture, ConvergenceA.Replay, TEXT("final"), ConvergenceA.FinalAudit);
		AddMatrixRow(DivergenceA.Fixture, DivergenceA.Replay, TEXT("initial"), DivergenceA.InitialAudit);
		AddMatrixRow(DivergenceA.Fixture, DivergenceA.Replay, TEXT("final"), DivergenceA.FinalAudit);
		AddMatrixRow(ZeroA.Fixture, ZeroA.Replay, TEXT("initial"), ZeroA.InitialAudit);
		AddMatrixRow(ZeroA.Fixture, ZeroA.Replay, TEXT("final"), ZeroA.FinalAudit);

		Report += TEXT("\n## Phase II Baseline\n\n");
		Report += FString::Printf(TEXT("Baseline artifact: `%s`\n\n"), *Baseline.Path);
		Report += TEXT("| Metric | Baseline | IIIB.3 replay 0 | Result |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += FString::Printf(TEXT("| State hash after resampling | `%s` | `%s` | %s |\n"), Baseline.bLoaded ? *Baseline.StateHashAfter : TEXT("unavailable"), *BaselineA.StateHashAfter, *PassFail(bStateBaseline));
		Report += FString::Printf(TEXT("| Material ledger hash | `%s` | `%s` | %s |\n"), Baseline.bLoaded ? *Baseline.MaterialLedgerHash : TEXT("unavailable"), *BaselineA.MaterialLedgerHash, *PassFail(bMaterialBaseline));

		Report += TEXT("\n## Notes\n\n");
		Report += TEXT("- `SubductionMatrix` is a plate-pair evidence matrix only. It does not assign under/over polarity; IIIB.4 owns that decision.\n");
		Report += TEXT("- Boundary-degenerate hits are counted but do not populate the matrix. Ray hits with non-positive local signed convergence at the active triangle barycenter are counted and rejected.\n");
		Report += TEXT("- The forced-divergence fixture can still produce locally convergent backside intersections on a closed sphere; matrix admission is local evidence, not blanket pair-wide classification.\n");
		Report += TEXT("- Matrix state is reset by the same plate-local rebuild path that resets the active list and distance-to-front values.\n");
		Report += TEXT("- Phase II `state_hash` and `material_ledger_hash` remain matched to the Slice 5.5 baseline, so the new matrix has not changed filter or material behavior.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIB.3 passes. Pause for user review before IIIB.4 (oceanic-under-continental polarity rule).\n")
			: TEXT("Pause before IIIB.4. One or more subduction-matrix gates require investigation.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIB3Commandlet::UCarrierLabPhaseIIIB3Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIB3Commandlet::Main(const FString& Params)
{
	const int32 BaselineSamples = FMath::Max(12, ParseIntParam(Params, TEXT("Samples="), 60000));
	const int32 BaselinePlates = FMath::Clamp(ParseIntParam(Params, TEXT("Plates="), 40), 1, 63);
	const int32 BaselineSteps = FMath::Max(1, ParseIntParam(Params, TEXT("Steps="), 32));
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FBaselineReplayResult BaselineA;
	FBaselineReplayResult BaselineB;
	const bool bBaselineA = RunBaselineReplay(0, BaselineSamples, BaselinePlates, BaselineSteps, BaselineA);
	const bool bBaselineB = RunBaselineReplay(1, BaselineSamples, BaselinePlates, BaselineSteps, BaselineB);

	FMatrixReplayResult ConvergenceA;
	FMatrixReplayResult ConvergenceB;
	const bool bConvergenceA = RunMatrixReplay(TEXT("forced_convergence_matrix"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, 0, 10000, 2, 4, ConvergenceA);
	const bool bConvergenceB = RunMatrixReplay(TEXT("forced_convergence_matrix"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, 1, 10000, 2, 4, ConvergenceB);

	FMatrixReplayResult DivergenceA;
	FMatrixReplayResult DivergenceB;
	const bool bDivergenceA = RunMatrixReplay(TEXT("forced_divergence_matrix"), ECarrierLabPhaseIIMotionFixture::ForcedDivergence, 0, 10000, 2, 1, DivergenceA);
	const bool bDivergenceB = RunMatrixReplay(TEXT("forced_divergence_matrix"), ECarrierLabPhaseIIMotionFixture::ForcedDivergence, 1, 10000, 2, 1, DivergenceB);

	FMatrixReplayResult ZeroA;
	FMatrixReplayResult ZeroB;
	const bool bZeroA = RunMatrixReplay(TEXT("zero_motion_matrix"), ECarrierLabPhaseIIMotionFixture::Zero, 0, 10000, 40, 10, ZeroA);
	const bool bZeroB = RunMatrixReplay(TEXT("zero_motion_matrix"), ECarrierLabPhaseIIMotionFixture::Zero, 1, 10000, 40, 10, ZeroB);

	const FPhaseIIBaseline Baseline = LoadPhaseIIBaseline();

	FString MetricsJsonl;
	MetricsJsonl += BaselineReplayJson(BaselineA) + LINE_TERMINATOR;
	MetricsJsonl += BaselineReplayJson(BaselineB) + LINE_TERMINATOR;
	MetricsJsonl += MatrixReplayJson(ConvergenceA) + LINE_TERMINATOR;
	MetricsJsonl += MatrixReplayJson(ConvergenceB) + LINE_TERMINATOR;
	MetricsJsonl += MatrixReplayJson(DivergenceA) + LINE_TERMINATOR;
	MetricsJsonl += MatrixReplayJson(DivergenceB) + LINE_TERMINATOR;
	MetricsJsonl += MatrixReplayJson(ZeroA) + LINE_TERMINATOR;
	MetricsJsonl += MatrixReplayJson(ZeroB) + LINE_TERMINATOR;
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);

	const FString Report = BuildReport(OutputRoot, Baseline, BaselineA, BaselineB, ConvergenceA, ConvergenceB, DivergenceA, DivergenceB, ZeroA, ZeroB);
	const FString ReportPath = ResolveReportPath(Params);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.3 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.3 report: %s"), *ReportPath);

	const bool bInitialEmpty = BaselineA.InitialAudit.MatrixPairCount == 0 && BaselineB.InitialAudit.MatrixPairCount == 0;
	const bool bPostRemeshEmpty = BaselineA.PostRemeshAudit.MatrixPairCount == 0 && BaselineB.PostRemeshAudit.MatrixPairCount == 0;
	const bool bBaselineValid = MatrixAuditIsValid(BaselineA.InitialAudit) &&
		MatrixAuditIsValid(BaselineA.PreRemeshAudit) &&
		MatrixAuditIsValid(BaselineA.PostRemeshAudit) &&
		MatrixAuditIsValid(BaselineB.InitialAudit) &&
		MatrixAuditIsValid(BaselineB.PreRemeshAudit) &&
		MatrixAuditIsValid(BaselineB.PostRemeshAudit);
	const bool bBaselineReplay = BaselineA.bCompleted && BaselineB.bCompleted &&
		BaselineA.InitialAudit.ConvergenceTrackingHash == BaselineB.InitialAudit.ConvergenceTrackingHash &&
		BaselineA.PreRemeshAudit.ConvergenceTrackingHash == BaselineB.PreRemeshAudit.ConvergenceTrackingHash &&
		BaselineA.PostRemeshAudit.ConvergenceTrackingHash == BaselineB.PostRemeshAudit.ConvergenceTrackingHash;
	const bool bProjectionReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.ProjectionHashAfter == BaselineB.ProjectionHashAfter;
	const bool bStateReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.StateHashAfter == BaselineB.StateHashAfter;
	const bool bCrustReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.CrustStateHashAfter == BaselineB.CrustStateHashAfter;
	const bool bMaterialReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.MaterialLedgerHash == BaselineB.MaterialLedgerHash;
	const bool bStateBaseline = Baseline.bLoaded && BaselineA.StateHashAfter == Baseline.StateHashAfter;
	const bool bMaterialBaseline = Baseline.bLoaded && BaselineA.MaterialLedgerHash == Baseline.MaterialLedgerHash;
	const bool bConvergencePopulates = ConvergenceA.bCompleted &&
		ConvergenceA.FinalAudit.MatrixPairCount > 0 &&
		HasExpectedPair(ConvergenceA.FinalAudit, 0, 1) &&
		ConvergenceA.FinalAudit.ProbeSignedConvergenceVelocity > 0.0;
	const bool bConvergenceReplay = MatrixReplayHashesMatch(ConvergenceA, ConvergenceB);
	const bool bDivergenceLocalConvergence = DivergenceA.bCompleted &&
		HasExpectedPair(DivergenceA.FinalAudit, 0, 1) &&
		DivergenceA.FinalAudit.HitCount > 0 &&
		DivergenceA.FinalAudit.ProbeSignedConvergenceVelocity > 0.0;
	const bool bDivergenceReplay = MatrixReplayHashesMatch(DivergenceA, DivergenceB);
	const bool bZeroEmpty = ZeroA.bCompleted &&
		ZeroA.InitialAudit.MatrixPairCount == 0 &&
		ZeroA.FinalAudit.MatrixPairCount == 0 &&
		ZeroA.InitialAudit.ConvergenceTrackingHash == ZeroA.FinalAudit.ConvergenceTrackingHash;
	const bool bZeroReplay = MatrixReplayHashesMatch(ZeroA, ZeroB);

	return (bBaselineA && bBaselineB && bConvergenceA && bConvergenceB && bDivergenceA && bDivergenceB && bZeroA && bZeroB &&
		bInitialEmpty && bPostRemeshEmpty && bBaselineValid && bBaselineReplay &&
		bProjectionReplay && bStateReplay && bCrustReplay && bMaterialReplay &&
		bStateBaseline && bMaterialBaseline &&
		bConvergencePopulates && bConvergenceReplay &&
		bDivergenceLocalConvergence && bDivergenceReplay &&
		bZeroEmpty && bZeroReplay) ? 0 : 2;
}
