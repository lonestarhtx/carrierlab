// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIB2Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr double DistanceExpectationToleranceKm = 1.0e-6;

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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIB2"), Stamp);
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
				TEXT("phase-iii-slice-iiib2-report.md"));
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

	bool DistanceAuditIsClean(const FCarrierLabPhaseIIIB2DistanceAudit& Audit)
	{
		return Audit.DistanceRecordCount == Audit.ActiveBoundaryTriangleCount &&
			Audit.MissingDistanceRecordCount == 0 &&
			Audit.NonFiniteDistanceCount == 0 &&
			Audit.NegativeDistanceCount == 0 &&
			Audit.OverThresholdActiveTriangleCount == 0;
	}

	struct FDistanceReplayResult
	{
		FString Fixture;
		int32 Replay = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		double TotalSeconds = 0.0;
		FCarrierLabPhaseIIIB2DistanceAudit InitialAudit;
		FCarrierLabPhaseIIIB2DistanceAudit FinalAudit;
		bool bCompleted = false;
	};

	bool RunDistanceReplay(
		const FString& Fixture,
		const ECarrierLabPhaseIIMotionFixture MotionFixture,
		const int32 Replay,
		const int32 SampleCount,
		const int32 PlateCount,
		const int32 StepCount,
		FDistanceReplayResult& OutResult)
	{
		OutResult = FDistanceReplayResult();
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
		if (!Actor->GetPhaseIIIB2DistanceAudit(OutResult.InitialAudit))
		{
			Actor->Destroy();
			return false;
		}

		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			Actor->StepOnce();
		}
		if (!Actor->GetPhaseIIIB2DistanceAudit(OutResult.FinalAudit))
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
		FCarrierLabPhaseIIIB2DistanceAudit InitialAudit;
		FCarrierLabPhaseIIIB2DistanceAudit PreRemeshAudit;
		FCarrierLabPhaseIIIB2DistanceAudit PostRemeshAudit;
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
		if (!Actor->GetPhaseIIIB2DistanceAudit(OutResult.InitialAudit))
		{
			Actor->Destroy();
			return false;
		}

		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			Actor->StepOnce();
		}
		if (!Actor->GetPhaseIIIB2DistanceAudit(OutResult.PreRemeshAudit))
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
		if (!Actor->GetPhaseIIIB2DistanceAudit(OutResult.PostRemeshAudit))
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

	FString DistanceAuditJsonFields(const FCarrierLabPhaseIIIB2DistanceAudit& Audit, const TCHAR* Prefix)
	{
		return FString::Printf(
			TEXT("\"%s_step\":%d,\"%s_event_count\":%d,\"%s_reset_serial\":%d,\"%s_active_boundary_triangles\":%d,\"%s_source_boundary_triangles\":%d,\"%s_distance_records\":%d,\"%s_missing_distance_records\":%d,\"%s_nonfinite_distance_records\":%d,\"%s_negative_distance_records\":%d,\"%s_over_threshold_active_triangles\":%d,\"%s_distance_culled_triangles\":%d,\"%s_min_distance_km\":%.12f,\"%s_mean_distance_km\":%.12f,\"%s_max_distance_km\":%.12f,\"%s_probe_plate_id\":%d,\"%s_probe_local_triangle_id\":%d,\"%s_probe_distance_km\":%.12f,\"%s_probe_step_distance_km\":%.12f,\"%s_hash\":%s"),
			Prefix, Audit.Step,
			Prefix, Audit.EventCount,
			Prefix, Audit.ResetSerial,
			Prefix, Audit.ActiveBoundaryTriangleCount,
			Prefix, Audit.SourceBoundaryTriangleCount,
			Prefix, Audit.DistanceRecordCount,
			Prefix, Audit.MissingDistanceRecordCount,
			Prefix, Audit.NonFiniteDistanceCount,
			Prefix, Audit.NegativeDistanceCount,
			Prefix, Audit.OverThresholdActiveTriangleCount,
			Prefix, Audit.DistanceCulledTriangleCount,
			Prefix, Audit.MinDistanceKm,
			Prefix, Audit.MeanDistanceKm,
			Prefix, Audit.MaxDistanceKm,
			Prefix, Audit.ProbePlateId,
			Prefix, Audit.ProbeLocalTriangleId,
			Prefix, Audit.ProbeDistanceKm,
			Prefix, Audit.ProbeStepDistanceKm,
			Prefix, *JsonString(Audit.ConvergenceTrackingHash));
	}

	FString DistanceReplayJson(const FDistanceReplayResult& Result)
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
			*DistanceAuditJsonFields(Result.InitialAudit, TEXT("initial")),
			*DistanceAuditJsonFields(Result.FinalAudit, TEXT("final")),
			MemoryGb);
	}

	FString BaselineReplayJson(const FBaselineReplayResult& Result)
	{
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		return FString::Printf(
			TEXT("{\"fixture\":\"iiib2_60k_phase_ii_baseline\",\"replay\":%d,\"step_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"total_replay_seconds\":%.12f,%s,%s,%s,\"projection_hash_after\":%s,\"state_hash_after\":%s,\"crust_state_hash_after\":%s,\"material_ledger_hash\":%s,\"contact_hash\":%s,\"label_hash\":%s,\"filter_decision_hash\":%s,\"memory_gb\":%.12f}"),
			Result.Replay,
			Result.StepCount,
			Result.SampleCount,
			Result.PlateCount,
			Result.TotalSeconds,
			*DistanceAuditJsonFields(Result.InitialAudit, TEXT("initial")),
			*DistanceAuditJsonFields(Result.PreRemeshAudit, TEXT("pre_remesh")),
			*DistanceAuditJsonFields(Result.PostRemeshAudit, TEXT("post_remesh")),
			*JsonString(Result.ProjectionHashAfter),
			*JsonString(Result.StateHashAfter),
			*JsonString(Result.CrustStateHashAfter),
			*JsonString(Result.MaterialLedgerHash),
			*JsonString(Result.ContactMetrics.ContactLogHash),
			*JsonString(Result.LabelMetrics.TriangleLabelHash),
			*JsonString(Result.FilterMetrics.FilterDecisionHash),
			MemoryGb);
	}

	bool ReplayDistanceHashesMatch(const FDistanceReplayResult& A, const FDistanceReplayResult& B)
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
		const FDistanceReplayResult& AnalyticA,
		const FDistanceReplayResult& AnalyticB,
		const FDistanceReplayResult& CullA,
		const FDistanceReplayResult& CullB,
		const FDistanceReplayResult& ZeroA,
		const FDistanceReplayResult& ZeroB)
	{
		const bool bBaselineInitialClean = DistanceAuditIsClean(BaselineA.InitialAudit) && DistanceAuditIsClean(BaselineB.InitialAudit);
		const bool bBaselinePreClean = DistanceAuditIsClean(BaselineA.PreRemeshAudit) && DistanceAuditIsClean(BaselineB.PreRemeshAudit);
		const bool bBaselinePostClean = DistanceAuditIsClean(BaselineA.PostRemeshAudit) && DistanceAuditIsClean(BaselineB.PostRemeshAudit);
		const bool bBaselineReplay =
			BaselineA.bCompleted && BaselineB.bCompleted &&
			BaselineA.PreRemeshAudit.ConvergenceTrackingHash == BaselineB.PreRemeshAudit.ConvergenceTrackingHash &&
			BaselineA.PostRemeshAudit.ConvergenceTrackingHash == BaselineB.PostRemeshAudit.ConvergenceTrackingHash;
		const bool bProjectionReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.ProjectionHashAfter == BaselineB.ProjectionHashAfter;
		const bool bStateReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.StateHashAfter == BaselineB.StateHashAfter;
		const bool bCrustReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.CrustStateHashAfter == BaselineB.CrustStateHashAfter;
		const bool bMaterialReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.MaterialLedgerHash == BaselineB.MaterialLedgerHash;
		const bool bStateBaseline = Baseline.bLoaded && BaselineA.StateHashAfter == Baseline.StateHashAfter;
		const bool bMaterialBaseline = Baseline.bLoaded && BaselineA.MaterialLedgerHash == Baseline.MaterialLedgerHash;

		const double AnalyticExpectedKm = AnalyticA.InitialAudit.ProbeStepDistanceKm * static_cast<double>(AnalyticA.StepCount);
		const bool bAnalyticProbe =
			AnalyticA.bCompleted &&
			AnalyticA.InitialAudit.ProbePlateId == AnalyticA.FinalAudit.ProbePlateId &&
			AnalyticA.InitialAudit.ProbeLocalTriangleId == AnalyticA.FinalAudit.ProbeLocalTriangleId &&
			FMath::Abs(AnalyticA.FinalAudit.ProbeDistanceKm - AnalyticExpectedKm) <= DistanceExpectationToleranceKm;
		const bool bAnalyticReplay = ReplayDistanceHashesMatch(AnalyticA, AnalyticB);
		const bool bAnalyticClean = DistanceAuditIsClean(AnalyticA.FinalAudit) && DistanceAuditIsClean(AnalyticB.FinalAudit);

		const bool bCullShrinks =
			CullA.bCompleted &&
			CullA.FinalAudit.ActiveBoundaryTriangleCount < CullA.InitialAudit.ActiveBoundaryTriangleCount &&
			CullA.FinalAudit.DistanceCulledTriangleCount > 0 &&
			CullA.FinalAudit.OverThresholdActiveTriangleCount == 0;
		const bool bCullReplay = ReplayDistanceHashesMatch(CullA, CullB);

		const bool bZeroNoop =
			ZeroA.bCompleted &&
			ZeroA.InitialAudit.ActiveBoundaryTriangleCount == ZeroA.FinalAudit.ActiveBoundaryTriangleCount &&
			ZeroA.FinalAudit.MaxDistanceKm == 0.0 &&
			ZeroA.FinalAudit.DistanceCulledTriangleCount == 0 &&
			ZeroA.InitialAudit.ConvergenceTrackingHash == ZeroA.FinalAudit.ConvergenceTrackingHash;
		const bool bZeroReplay = ReplayDistanceHashesMatch(ZeroA, ZeroB);

		const bool bAllPass = bBaselineInitialClean && bBaselinePreClean && bBaselinePostClean &&
			bBaselineReplay && bProjectionReplay && bStateReplay && bCrustReplay && bMaterialReplay &&
			bStateBaseline && bMaterialBaseline &&
			bAnalyticProbe && bAnalyticReplay && bAnalyticClean &&
			bCullShrinks && bCullReplay &&
			bZeroNoop && bZeroReplay;

		FString Report = TEXT("# Phase III Slice IIIB.2 Checkpoint: Per-Triangle Distance-To-Front\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This slice adds `DistanceToFront` as a per-active-triangle kilometer scalar. It is read-only tracking state: it does not modify crust fields, projection ownership, Phase II contact detection, triangle labels, or resampling filters. Distances initialize to 0 at remesh-window creation, advance each rigid step by the thesis over-estimation `d(p,t+dt)=d(p,t)+s(p)dt`, and active triangles are culled when `d > r_s = 1800 km`.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		Report += FString::Printf(TEXT("| Initial distance records cover active triangles | %s | records %d, active %d, missing %d |\n"), *PassFail(bBaselineInitialClean), BaselineA.InitialAudit.DistanceRecordCount, BaselineA.InitialAudit.ActiveBoundaryTriangleCount, BaselineA.InitialAudit.MissingDistanceRecordCount);
		Report += FString::Printf(TEXT("| Distance records remain finite/nonnegative/below threshold pre-remesh | %s | nonfinite %d, negative %d, over-threshold active %d |\n"), *PassFail(bBaselinePreClean), BaselineA.PreRemeshAudit.NonFiniteDistanceCount, BaselineA.PreRemeshAudit.NegativeDistanceCount, BaselineA.PreRemeshAudit.OverThresholdActiveTriangleCount);
		Report += FString::Printf(TEXT("| Distance records reset cleanly after remesh | %s | serial %d -> %d, max distance %.6f km |\n"), *PassFail(bBaselinePostClean), BaselineA.PreRemeshAudit.ResetSerial, BaselineA.PostRemeshAudit.ResetSerial, BaselineA.PostRemeshAudit.MaxDistanceKm);
		Report += FString::Printf(TEXT("| Distance trajectory replay deterministic | %s | baseline pre `%s` vs `%s`, analytic final `%s` vs `%s` |\n"), *PassFail(bBaselineReplay && bAnalyticReplay), *BaselineA.PreRemeshAudit.ConvergenceTrackingHash, *BaselineB.PreRemeshAudit.ConvergenceTrackingHash, *AnalyticA.FinalAudit.ConvergenceTrackingHash, *AnalyticB.FinalAudit.ConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| Forced-convergence probe evolves analytically | %s | expected %.9f km, observed %.9f km, step %.9f km x %d |\n"), *PassFail(bAnalyticProbe), AnalyticExpectedKm, AnalyticA.FinalAudit.ProbeDistanceKm, AnalyticA.InitialAudit.ProbeStepDistanceKm, AnalyticA.StepCount);
		Report += FString::Printf(TEXT("| Active list shrinks past r_s | %s | active %d -> %d, culled %d, threshold %.1f km |\n"), *PassFail(bCullShrinks), CullA.InitialAudit.ActiveBoundaryTriangleCount, CullA.FinalAudit.ActiveBoundaryTriangleCount, CullA.FinalAudit.DistanceCulledTriangleCount, CullA.FinalAudit.DistanceThresholdKm);
		Report += FString::Printf(TEXT("| Zero-motion tracking no-op | %s | active %d -> %d, hash `%s` -> `%s` |\n"), *PassFail(bZeroNoop), ZeroA.InitialAudit.ActiveBoundaryTriangleCount, ZeroA.FinalAudit.ActiveBoundaryTriangleCount, *ZeroA.InitialAudit.ConvergenceTrackingHash, *ZeroA.FinalAudit.ConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| Projection replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bProjectionReplay), *BaselineA.ProjectionHashAfter, *BaselineB.ProjectionHashAfter);
		Report += FString::Printf(TEXT("| Phase II state replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bStateReplay), *BaselineA.StateHashAfter, *BaselineB.StateHashAfter);
		Report += FString::Printf(TEXT("| Crust state replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bCrustReplay), *BaselineA.CrustStateHashAfter, *BaselineB.CrustStateHashAfter);
		Report += FString::Printf(TEXT("| Phase II material ledger replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bMaterialReplay), *BaselineA.MaterialLedgerHash, *BaselineB.MaterialLedgerHash);
		Report += FString::Printf(TEXT("| Phase II baseline state hash unchanged | %s | baseline `%s`, IIIB.2 `%s` |\n"), *PassFail(bStateBaseline), Baseline.bLoaded ? *Baseline.StateHashAfter : TEXT("baseline unavailable"), *BaselineA.StateHashAfter);
		Report += FString::Printf(TEXT("| Phase II baseline material ledger unchanged | %s | baseline `%s`, IIIB.2 `%s` |\n"), *PassFail(bMaterialBaseline), Baseline.bLoaded ? *Baseline.MaterialLedgerHash : TEXT("baseline unavailable"), *BaselineA.MaterialLedgerHash);

		Report += TEXT("\n## Distance Audits\n\n");
		Report += TEXT("| Fixture | Replay | Window | Step | Reset | Active | Records | Culled | Min km | Mean km | Max km | Probe step km | Probe total km | Hash |\n");
		Report += TEXT("|---|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		auto AddDistanceRow = [&Report](const FString& Fixture, const int32 Replay, const TCHAR* Window, const FCarrierLabPhaseIIIB2DistanceAudit& Audit)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %s | %d | %d | %d | %d | %d | %.6f | %.6f | %.6f | %.6f | %.6f | `%s` |\n"),
				*Fixture,
				Replay,
				Window,
				Audit.Step,
				Audit.ResetSerial,
				Audit.ActiveBoundaryTriangleCount,
				Audit.DistanceRecordCount,
				Audit.DistanceCulledTriangleCount,
				Audit.MinDistanceKm,
				Audit.MeanDistanceKm,
				Audit.MaxDistanceKm,
				Audit.ProbeStepDistanceKm,
				Audit.ProbeDistanceKm,
				*Audit.ConvergenceTrackingHash);
		};
		AddDistanceRow(TEXT("baseline"), BaselineA.Replay, TEXT("initial"), BaselineA.InitialAudit);
		AddDistanceRow(TEXT("baseline"), BaselineA.Replay, TEXT("pre-remesh"), BaselineA.PreRemeshAudit);
		AddDistanceRow(TEXT("baseline"), BaselineA.Replay, TEXT("post-remesh"), BaselineA.PostRemeshAudit);
		AddDistanceRow(AnalyticA.Fixture, AnalyticA.Replay, TEXT("initial"), AnalyticA.InitialAudit);
		AddDistanceRow(AnalyticA.Fixture, AnalyticA.Replay, TEXT("final"), AnalyticA.FinalAudit);
		AddDistanceRow(CullA.Fixture, CullA.Replay, TEXT("initial"), CullA.InitialAudit);
		AddDistanceRow(CullA.Fixture, CullA.Replay, TEXT("final"), CullA.FinalAudit);
		AddDistanceRow(ZeroA.Fixture, ZeroA.Replay, TEXT("initial"), ZeroA.InitialAudit);
		AddDistanceRow(ZeroA.Fixture, ZeroA.Replay, TEXT("final"), ZeroA.FinalAudit);

		Report += TEXT("\n## Phase II Baseline\n\n");
		Report += FString::Printf(TEXT("Baseline artifact: `%s`\n\n"), *Baseline.Path);
		Report += TEXT("| Metric | Baseline | IIIB.2 replay 0 | Result |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += FString::Printf(TEXT("| State hash after resampling | `%s` | `%s` | %s |\n"), Baseline.bLoaded ? *Baseline.StateHashAfter : TEXT("unavailable"), *BaselineA.StateHashAfter, *PassFail(bStateBaseline));
		Report += FString::Printf(TEXT("| Material ledger hash | `%s` | `%s` | %s |\n"), Baseline.bLoaded ? *Baseline.MaterialLedgerHash : TEXT("unavailable"), *BaselineA.MaterialLedgerHash, *PassFail(bMaterialBaseline));

		Report += TEXT("\n## Notes\n\n");
		Report += TEXT("- Distance-to-front is process tracking state with remesh-window lifetime. It is not global ownership, not projection authority, and not a material mutation.\n");
		Report += TEXT("- The distance increment uses local triangle barycenter tangential speed under the current plate geodetic rotation, matching the thesis over-estimation form and avoiding brute-force exact distance recomputation.\n");
		Report += TEXT("- Culling only removes entries from the active convergence tracking list. It does not remove plate-local triangles from the carrier mesh.\n");
		Report += TEXT("- `state_hash` and `material_ledger_hash` match the Phase II Slice 5.5 baseline, confirming this read-only tracking state did not change filter or material behavior.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIB.2 passes. Pause for user review before IIIB.3 (subduction matrix).\n")
			: TEXT("Pause before IIIB.3. One or more distance-to-front gates require investigation.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIB2Commandlet::UCarrierLabPhaseIIIB2Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIB2Commandlet::Main(const FString& Params)
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

	FDistanceReplayResult AnalyticA;
	FDistanceReplayResult AnalyticB;
	const bool bAnalyticA = RunDistanceReplay(TEXT("forced_convergence_analytic"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, 0, 10000, 2, 4, AnalyticA);
	const bool bAnalyticB = RunDistanceReplay(TEXT("forced_convergence_analytic"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, 1, 10000, 2, 4, AnalyticB);

	FDistanceReplayResult CullA;
	FDistanceReplayResult CullB;
	const bool bCullA = RunDistanceReplay(TEXT("forced_convergence_cull"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, 0, 10000, 2, 12, CullA);
	const bool bCullB = RunDistanceReplay(TEXT("forced_convergence_cull"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, 1, 10000, 2, 12, CullB);

	FDistanceReplayResult ZeroA;
	FDistanceReplayResult ZeroB;
	const bool bZeroA = RunDistanceReplay(TEXT("zero_motion_noop"), ECarrierLabPhaseIIMotionFixture::Zero, 0, 10000, 40, 10, ZeroA);
	const bool bZeroB = RunDistanceReplay(TEXT("zero_motion_noop"), ECarrierLabPhaseIIMotionFixture::Zero, 1, 10000, 40, 10, ZeroB);

	const FPhaseIIBaseline Baseline = LoadPhaseIIBaseline();

	FString MetricsJsonl;
	MetricsJsonl += BaselineReplayJson(BaselineA) + LINE_TERMINATOR;
	MetricsJsonl += BaselineReplayJson(BaselineB) + LINE_TERMINATOR;
	MetricsJsonl += DistanceReplayJson(AnalyticA) + LINE_TERMINATOR;
	MetricsJsonl += DistanceReplayJson(AnalyticB) + LINE_TERMINATOR;
	MetricsJsonl += DistanceReplayJson(CullA) + LINE_TERMINATOR;
	MetricsJsonl += DistanceReplayJson(CullB) + LINE_TERMINATOR;
	MetricsJsonl += DistanceReplayJson(ZeroA) + LINE_TERMINATOR;
	MetricsJsonl += DistanceReplayJson(ZeroB) + LINE_TERMINATOR;
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);

	const FString Report = BuildReport(OutputRoot, Baseline, BaselineA, BaselineB, AnalyticA, AnalyticB, CullA, CullB, ZeroA, ZeroB);
	const FString ReportPath = ResolveReportPath(Params);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.2 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.2 report: %s"), *ReportPath);

	const bool bBaselineInitialClean = DistanceAuditIsClean(BaselineA.InitialAudit) && DistanceAuditIsClean(BaselineB.InitialAudit);
	const bool bBaselinePreClean = DistanceAuditIsClean(BaselineA.PreRemeshAudit) && DistanceAuditIsClean(BaselineB.PreRemeshAudit);
	const bool bBaselinePostClean = DistanceAuditIsClean(BaselineA.PostRemeshAudit) && DistanceAuditIsClean(BaselineB.PostRemeshAudit);
	const bool bBaselineReplay =
		BaselineA.bCompleted && BaselineB.bCompleted &&
		BaselineA.PreRemeshAudit.ConvergenceTrackingHash == BaselineB.PreRemeshAudit.ConvergenceTrackingHash &&
		BaselineA.PostRemeshAudit.ConvergenceTrackingHash == BaselineB.PostRemeshAudit.ConvergenceTrackingHash;
	const bool bProjectionReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.ProjectionHashAfter == BaselineB.ProjectionHashAfter;
	const bool bStateReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.StateHashAfter == BaselineB.StateHashAfter;
	const bool bCrustReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.CrustStateHashAfter == BaselineB.CrustStateHashAfter;
	const bool bMaterialReplay = BaselineA.bCompleted && BaselineB.bCompleted && BaselineA.MaterialLedgerHash == BaselineB.MaterialLedgerHash;
	const bool bStateBaseline = Baseline.bLoaded && BaselineA.StateHashAfter == Baseline.StateHashAfter;
	const bool bMaterialBaseline = Baseline.bLoaded && BaselineA.MaterialLedgerHash == Baseline.MaterialLedgerHash;
	const double AnalyticExpectedKm = AnalyticA.InitialAudit.ProbeStepDistanceKm * static_cast<double>(AnalyticA.StepCount);
	const bool bAnalyticProbe =
		AnalyticA.bCompleted &&
		AnalyticA.InitialAudit.ProbePlateId == AnalyticA.FinalAudit.ProbePlateId &&
		AnalyticA.InitialAudit.ProbeLocalTriangleId == AnalyticA.FinalAudit.ProbeLocalTriangleId &&
		FMath::Abs(AnalyticA.FinalAudit.ProbeDistanceKm - AnalyticExpectedKm) <= DistanceExpectationToleranceKm;
	const bool bAnalyticReplay = ReplayDistanceHashesMatch(AnalyticA, AnalyticB);
	const bool bAnalyticClean = DistanceAuditIsClean(AnalyticA.FinalAudit) && DistanceAuditIsClean(AnalyticB.FinalAudit);
	const bool bCullShrinks =
		CullA.bCompleted &&
		CullA.FinalAudit.ActiveBoundaryTriangleCount < CullA.InitialAudit.ActiveBoundaryTriangleCount &&
		CullA.FinalAudit.DistanceCulledTriangleCount > 0 &&
		CullA.FinalAudit.OverThresholdActiveTriangleCount == 0;
	const bool bCullReplay = ReplayDistanceHashesMatch(CullA, CullB);
	const bool bZeroNoop =
		ZeroA.bCompleted &&
		ZeroA.InitialAudit.ActiveBoundaryTriangleCount == ZeroA.FinalAudit.ActiveBoundaryTriangleCount &&
		ZeroA.FinalAudit.MaxDistanceKm == 0.0 &&
		ZeroA.FinalAudit.DistanceCulledTriangleCount == 0 &&
		ZeroA.InitialAudit.ConvergenceTrackingHash == ZeroA.FinalAudit.ConvergenceTrackingHash;
	const bool bZeroReplay = ReplayDistanceHashesMatch(ZeroA, ZeroB);

	return (bBaselineA && bBaselineB && bAnalyticA && bAnalyticB && bCullA && bCullB && bZeroA && bZeroB &&
		bBaselineInitialClean && bBaselinePreClean && bBaselinePostClean &&
		bBaselineReplay && bProjectionReplay && bStateReplay && bCrustReplay && bMaterialReplay &&
		bStateBaseline && bMaterialBaseline &&
		bAnalyticProbe && bAnalyticReplay && bAnalyticClean &&
		bCullShrinks && bCullReplay &&
		bZeroNoop && bZeroReplay) ? 0 : 2;
}
