// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIB1Commandlet.h"

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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIB1"), Stamp);
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
				TEXT("phase-iii-slice-iiib1-report.md"));
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

	bool TrackingAuditIsValid(const FCarrierLabPhaseIIIB1TrackingAudit& Audit)
	{
		return Audit.ActiveBoundaryTriangleCount == Audit.SourceBoundaryTriangleCount &&
			Audit.ActiveBoundaryTriangleCount > 0 &&
			Audit.MissingBoundaryTriangleCount == 0 &&
			Audit.NonBoundaryActiveTriangleCount == 0 &&
			Audit.DuplicateActiveTriangleCount == 0 &&
			Audit.InvalidActiveTriangleCount == 0;
	}

	struct FPhaseIIIB1ReplayResult
	{
		int32 Replay = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		double TotalSeconds = 0.0;
		FCarrierLabPhaseIIIB1TrackingAudit InitialAudit;
		FCarrierLabPhaseIIIB1TrackingAudit PreRemeshAudit;
		FCarrierLabPhaseIIIB1TrackingAudit PostRemeshAudit;
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

	bool RunReplay(
		const int32 Replay,
		const int32 SampleCount,
		const int32 PlateCount,
		const int32 StepCount,
		FPhaseIIIB1ReplayResult& OutResult)
	{
		OutResult = FPhaseIIIB1ReplayResult();
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
		if (!Actor->GetPhaseIIIB1TrackingAudit(OutResult.InitialAudit))
		{
			Actor->Destroy();
			return false;
		}

		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			Actor->StepOnce();
		}
		if (!Actor->GetPhaseIIIB1TrackingAudit(OutResult.PreRemeshAudit))
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
		if (!Actor->GetPhaseIIIB1TrackingAudit(OutResult.PostRemeshAudit))
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

	FString AuditJsonFields(const FCarrierLabPhaseIIIB1TrackingAudit& Audit, const TCHAR* Prefix)
	{
		return FString::Printf(
			TEXT("\"%s_step\":%d,\"%s_event_count\":%d,\"%s_reset_serial\":%d,\"%s_active_boundary_triangles\":%d,\"%s_source_boundary_triangles\":%d,\"%s_missing_boundary_triangles\":%d,\"%s_nonboundary_active_triangles\":%d,\"%s_duplicate_active_triangles\":%d,\"%s_invalid_active_triangles\":%d,\"%s_empty_active_plates\":%d,\"%s_hash\":%s"),
			Prefix, Audit.Step,
			Prefix, Audit.EventCount,
			Prefix, Audit.ResetSerial,
			Prefix, Audit.ActiveBoundaryTriangleCount,
			Prefix, Audit.SourceBoundaryTriangleCount,
			Prefix, Audit.MissingBoundaryTriangleCount,
			Prefix, Audit.NonBoundaryActiveTriangleCount,
			Prefix, Audit.DuplicateActiveTriangleCount,
			Prefix, Audit.InvalidActiveTriangleCount,
			Prefix, Audit.EmptyActivePlateCount,
			Prefix, *JsonString(Audit.ConvergenceTrackingHash));
	}

	FString ReplayJson(const FPhaseIIIB1ReplayResult& Result)
	{
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		return FString::Printf(
			TEXT("{\"fixture\":\"iiib1_60k_boundary_active_list\",\"replay\":%d,\"step_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"total_replay_seconds\":%.12f,%s,%s,%s,\"projection_hash_after\":%s,\"state_hash_after\":%s,\"crust_state_hash_after\":%s,\"material_ledger_hash\":%s,\"contact_hash\":%s,\"label_hash\":%s,\"filter_decision_hash\":%s,\"memory_gb\":%.12f}"),
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

	FString BuildReport(
		const FString& OutputRoot,
		const FPhaseIIBaseline& Baseline,
		const FPhaseIIIB1ReplayResult& A,
		const FPhaseIIIB1ReplayResult& B)
	{
		const bool bInitialValid = TrackingAuditIsValid(A.InitialAudit) && TrackingAuditIsValid(B.InitialAudit);
		const bool bPreRemeshValid = TrackingAuditIsValid(A.PreRemeshAudit) && TrackingAuditIsValid(B.PreRemeshAudit);
		const bool bPostRemeshValid = TrackingAuditIsValid(A.PostRemeshAudit) && TrackingAuditIsValid(B.PostRemeshAudit);
		const bool bActiveListStableWithinWindow = A.InitialAudit.ConvergenceTrackingHash == A.PreRemeshAudit.ConvergenceTrackingHash &&
			B.InitialAudit.ConvergenceTrackingHash == B.PreRemeshAudit.ConvergenceTrackingHash;
		const bool bTrackingReplay =
			A.InitialAudit.ConvergenceTrackingHash == B.InitialAudit.ConvergenceTrackingHash &&
			A.PreRemeshAudit.ConvergenceTrackingHash == B.PreRemeshAudit.ConvergenceTrackingHash &&
			A.PostRemeshAudit.ConvergenceTrackingHash == B.PostRemeshAudit.ConvergenceTrackingHash;
		const bool bRemeshReset = A.PostRemeshAudit.ResetSerial == A.PreRemeshAudit.ResetSerial + 1 &&
			B.PostRemeshAudit.ResetSerial == B.PreRemeshAudit.ResetSerial + 1 &&
			A.PostRemeshAudit.ConvergenceTrackingHash != A.PreRemeshAudit.ConvergenceTrackingHash &&
			B.PostRemeshAudit.ConvergenceTrackingHash != B.PreRemeshAudit.ConvergenceTrackingHash;
		const bool bProjectionReplay = A.bCompleted && B.bCompleted && A.ProjectionHashAfter == B.ProjectionHashAfter;
		const bool bStateReplay = A.bCompleted && B.bCompleted && A.StateHashAfter == B.StateHashAfter;
		const bool bCrustReplay = A.bCompleted && B.bCompleted && A.CrustStateHashAfter == B.CrustStateHashAfter;
		const bool bMaterialReplay = A.bCompleted && B.bCompleted && A.MaterialLedgerHash == B.MaterialLedgerHash;
		const bool bStateBaseline = Baseline.bLoaded && A.StateHashAfter == Baseline.StateHashAfter;
		const bool bMaterialBaseline = Baseline.bLoaded && A.MaterialLedgerHash == Baseline.MaterialLedgerHash;
		const bool bAllPass = bInitialValid && bPreRemeshValid && bPostRemeshValid &&
			bActiveListStableWithinWindow && bTrackingReplay && bRemeshReset &&
			bProjectionReplay && bStateReplay && bCrustReplay && bMaterialReplay &&
			bStateBaseline && bMaterialBaseline;

		FString Report = TEXT("# Phase III Slice IIIB.1 Checkpoint: Boundary Active List Scaffold\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This slice adds read-only convergence tracking scaffold state: each plate owns `ActiveBoundaryTriangles`, initialized from that plate's local boundary triangles at the start of a remesh window. The list is reset when plate-local topology is rebuilt. No distance-to-front, subduction matrix, polarity rule, filter behavior, projection behavior, or crust mutation was introduced.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		Report += FString::Printf(TEXT("| Initial active lists equal source boundary triangles | %s | active %d, source %d, missing %d, non-boundary %d |\n"), *PassFail(bInitialValid), A.InitialAudit.ActiveBoundaryTriangleCount, A.InitialAudit.SourceBoundaryTriangleCount, A.InitialAudit.MissingBoundaryTriangleCount, A.InitialAudit.NonBoundaryActiveTriangleCount);
		Report += FString::Printf(TEXT("| Active list stable across rigid steps before remesh | %s | `%s` -> `%s` |\n"), *PassFail(bActiveListStableWithinWindow), *A.InitialAudit.ConvergenceTrackingHash, *A.PreRemeshAudit.ConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| Active list resets at remesh | %s | serial %d -> %d, hash `%s` -> `%s` |\n"), *PassFail(bRemeshReset), A.PreRemeshAudit.ResetSerial, A.PostRemeshAudit.ResetSerial, *A.PreRemeshAudit.ConvergenceTrackingHash, *A.PostRemeshAudit.ConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| Post-remesh active lists equal rebuilt boundary triangles | %s | active %d, source %d, missing %d, non-boundary %d |\n"), *PassFail(bPostRemeshValid), A.PostRemeshAudit.ActiveBoundaryTriangleCount, A.PostRemeshAudit.SourceBoundaryTriangleCount, A.PostRemeshAudit.MissingBoundaryTriangleCount, A.PostRemeshAudit.NonBoundaryActiveTriangleCount);
		Report += FString::Printf(TEXT("| Convergence tracking replay deterministic | %s | initial `%s`, pre `%s`, post `%s` |\n"), *PassFail(bTrackingReplay), *A.InitialAudit.ConvergenceTrackingHash, *A.PreRemeshAudit.ConvergenceTrackingHash, *A.PostRemeshAudit.ConvergenceTrackingHash);
		Report += FString::Printf(TEXT("| Projection replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bProjectionReplay), *A.ProjectionHashAfter, *B.ProjectionHashAfter);
		Report += FString::Printf(TEXT("| Phase II state replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bStateReplay), *A.StateHashAfter, *B.StateHashAfter);
		Report += FString::Printf(TEXT("| Crust state replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bCrustReplay), *A.CrustStateHashAfter, *B.CrustStateHashAfter);
		Report += FString::Printf(TEXT("| Phase II material ledger replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bMaterialReplay), *A.MaterialLedgerHash, *B.MaterialLedgerHash);
		Report += FString::Printf(TEXT("| Phase II baseline state hash unchanged | %s | baseline `%s`, IIIB.1 `%s` |\n"), *PassFail(bStateBaseline), Baseline.bLoaded ? *Baseline.StateHashAfter : TEXT("baseline unavailable"), *A.StateHashAfter);
		Report += FString::Printf(TEXT("| Phase II baseline material ledger unchanged | %s | baseline `%s`, IIIB.1 `%s` |\n"), *PassFail(bMaterialBaseline), Baseline.bLoaded ? *Baseline.MaterialLedgerHash : TEXT("baseline unavailable"), *A.MaterialLedgerHash);

		Report += TEXT("\n## Tracking Audits\n\n");
		Report += TEXT("| Replay | Window | Step | Event | Reset serial | Active | Source boundary | Missing | Non-boundary active | Duplicate | Invalid | Empty plates | Hash |\n");
		Report += TEXT("|---:|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		auto AddAuditRow = [&Report](const int32 Replay, const TCHAR* Window, const FCarrierLabPhaseIIIB1TrackingAudit& Audit)
		{
			Report += FString::Printf(
				TEXT("| %d | %s | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | `%s` |\n"),
				Replay,
				Window,
				Audit.Step,
				Audit.EventCount,
				Audit.ResetSerial,
				Audit.ActiveBoundaryTriangleCount,
				Audit.SourceBoundaryTriangleCount,
				Audit.MissingBoundaryTriangleCount,
				Audit.NonBoundaryActiveTriangleCount,
				Audit.DuplicateActiveTriangleCount,
				Audit.InvalidActiveTriangleCount,
				Audit.EmptyActivePlateCount,
				*Audit.ConvergenceTrackingHash);
		};
		AddAuditRow(A.Replay, TEXT("initial"), A.InitialAudit);
		AddAuditRow(A.Replay, TEXT("pre-remesh"), A.PreRemeshAudit);
		AddAuditRow(A.Replay, TEXT("post-remesh"), A.PostRemeshAudit);
		AddAuditRow(B.Replay, TEXT("initial"), B.InitialAudit);
		AddAuditRow(B.Replay, TEXT("pre-remesh"), B.PreRemeshAudit);
		AddAuditRow(B.Replay, TEXT("post-remesh"), B.PostRemeshAudit);

		Report += TEXT("\n## Phase II Baseline\n\n");
		Report += FString::Printf(TEXT("Baseline artifact: `%s`\n\n"), *Baseline.Path);
		Report += TEXT("| Metric | Baseline | IIIB.1 replay 0 | Result |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += FString::Printf(TEXT("| State hash after resampling | `%s` | `%s` | %s |\n"), Baseline.bLoaded ? *Baseline.StateHashAfter : TEXT("unavailable"), *A.StateHashAfter, *PassFail(bStateBaseline));
		Report += FString::Printf(TEXT("| Material ledger hash | `%s` | `%s` | %s |\n"), Baseline.bLoaded ? *Baseline.MaterialLedgerHash : TEXT("unavailable"), *A.MaterialLedgerHash, *PassFail(bMaterialBaseline));

		Report += TEXT("\n## Notes\n\n");
		Report += TEXT("- `ActiveBoundaryTriangles` is plate-local process tracking state, not projection authority and not global sample ownership.\n");
		Report += TEXT("- `state_hash` and `crust_state_hash` intentionally remain independent of `convergence_tracking_hash` so Phase II and IIIA evidence stays comparable.\n");
		Report += TEXT("- The hash changes at remesh because the remesh-window reset serial is included; this makes reset observable even in a degenerate case where a rebuilt active list has identical membership.\n");
		Report += TEXT("- IIIB.1 does not add distance-to-front, subduction matrix entries, polarity decisions, or any filter behavior.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIB.1 passes. Pause for user review before IIIB.2 (per-triangle distance-to-front).\n")
			: TEXT("Pause before IIIB.2. One or more active-list scaffold gates require investigation.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIB1Commandlet::UCarrierLabPhaseIIIB1Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIB1Commandlet::Main(const FString& Params)
{
	const int32 SampleCount = FMath::Max(12, ParseIntParam(Params, TEXT("Samples="), 60000));
	const int32 PlateCount = FMath::Clamp(ParseIntParam(Params, TEXT("Plates="), 40), 1, 63);
	const int32 StepCount = FMath::Max(1, ParseIntParam(Params, TEXT("Steps="), 32));
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FPhaseIIIB1ReplayResult A;
	FPhaseIIIB1ReplayResult B;
	const bool bRunA = RunReplay(0, SampleCount, PlateCount, StepCount, A);
	const bool bRunB = RunReplay(1, SampleCount, PlateCount, StepCount, B);
	const FPhaseIIBaseline Baseline = LoadPhaseIIBaseline();

	FString MetricsJsonl;
	MetricsJsonl += ReplayJson(A) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(B) + LINE_TERMINATOR;
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);

	const FString Report = BuildReport(OutputRoot, Baseline, A, B);
	const FString ReportPath = ResolveReportPath(Params);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.1 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.1 report: %s"), *ReportPath);

	const bool bInitialValid = TrackingAuditIsValid(A.InitialAudit) && TrackingAuditIsValid(B.InitialAudit);
	const bool bPreRemeshValid = TrackingAuditIsValid(A.PreRemeshAudit) && TrackingAuditIsValid(B.PreRemeshAudit);
	const bool bPostRemeshValid = TrackingAuditIsValid(A.PostRemeshAudit) && TrackingAuditIsValid(B.PostRemeshAudit);
	const bool bActiveListStableWithinWindow = A.InitialAudit.ConvergenceTrackingHash == A.PreRemeshAudit.ConvergenceTrackingHash &&
		B.InitialAudit.ConvergenceTrackingHash == B.PreRemeshAudit.ConvergenceTrackingHash;
	const bool bTrackingReplay =
		A.InitialAudit.ConvergenceTrackingHash == B.InitialAudit.ConvergenceTrackingHash &&
		A.PreRemeshAudit.ConvergenceTrackingHash == B.PreRemeshAudit.ConvergenceTrackingHash &&
		A.PostRemeshAudit.ConvergenceTrackingHash == B.PostRemeshAudit.ConvergenceTrackingHash;
	const bool bRemeshReset = A.PostRemeshAudit.ResetSerial == A.PreRemeshAudit.ResetSerial + 1 &&
		B.PostRemeshAudit.ResetSerial == B.PreRemeshAudit.ResetSerial + 1 &&
		A.PostRemeshAudit.ConvergenceTrackingHash != A.PreRemeshAudit.ConvergenceTrackingHash &&
		B.PostRemeshAudit.ConvergenceTrackingHash != B.PreRemeshAudit.ConvergenceTrackingHash;
	const bool bProjectionReplay = A.bCompleted && B.bCompleted && A.ProjectionHashAfter == B.ProjectionHashAfter;
	const bool bStateReplay = A.bCompleted && B.bCompleted && A.StateHashAfter == B.StateHashAfter;
	const bool bCrustReplay = A.bCompleted && B.bCompleted && A.CrustStateHashAfter == B.CrustStateHashAfter;
	const bool bMaterialReplay = A.bCompleted && B.bCompleted && A.MaterialLedgerHash == B.MaterialLedgerHash;
	const bool bStateBaseline = Baseline.bLoaded && A.StateHashAfter == Baseline.StateHashAfter;
	const bool bMaterialBaseline = Baseline.bLoaded && A.MaterialLedgerHash == Baseline.MaterialLedgerHash;

	return (bRunA && bRunB &&
		bInitialValid && bPreRemeshValid && bPostRemeshValid &&
		bActiveListStableWithinWindow && bTrackingReplay && bRemeshReset &&
		bProjectionReplay && bStateReplay && bCrustReplay && bMaterialReplay &&
		bStateBaseline && bMaterialBaseline) ? 0 : 2;
}
