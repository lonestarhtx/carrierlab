// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIA2Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr double CrustFieldTolerance = 1.0e-12;

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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIA2"), Stamp);
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
				TEXT("phase-iii-slice-iiia2-report.md"));
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

	struct FPhaseIIIA2Audit
	{
		int32 SampleCount = 0;
		int32 PlateVertexCount = 0;
		double MaxAbsSampleElevation = 0.0;
		double MaxAbsPlateVertexElevation = 0.0;
		double MaxAbsSampleOceanicAge = 0.0;
		double MaxAbsPlateVertexOceanicAge = 0.0;
	};

	struct FPhaseIIIA2ReplayResult
	{
		int32 Replay = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		double TotalSeconds = 0.0;
		double StepProjectionSecondsTotal = 0.0;
		double StepWallSecondsTotal = 0.0;
		FString ProjectionHashBefore;
		FString ProjectionHashAfter;
		FString StateHashBefore;
		FString StateHashAfter;
		FString CrustStateHashBefore;
		FString CrustStateHashAfter;
		FString MaterialLedgerHash;
		FPhaseIIIA2Audit AuditBefore;
		FPhaseIIIA2Audit AuditAfter;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterMetrics;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerMetrics;
		bool bCompleted = false;
	};

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

	FPhaseIIIA2Audit ReadAudit(const ACarrierLabVisualizationActor& Actor)
	{
		FPhaseIIIA2Audit Audit;
		Actor.GetPhaseIIIA2CrustFieldAudit(
			Audit.SampleCount,
			Audit.PlateVertexCount,
			Audit.MaxAbsSampleElevation,
			Audit.MaxAbsPlateVertexElevation,
			Audit.MaxAbsSampleOceanicAge,
			Audit.MaxAbsPlateVertexOceanicAge);
		return Audit;
	}

	bool RunReplay(const int32 Replay, const int32 SampleCount, const int32 PlateCount, const int32 StepCount, FPhaseIIIA2ReplayResult& OutResult)
	{
		OutResult = FPhaseIIIA2ReplayResult();
		OutResult.Replay = Replay;
		OutResult.SampleCount = SampleCount;
		OutResult.PlateCount = PlateCount;
		OutResult.StepCount = StepCount;

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("CarrierLab Phase IIIA.2 could not find a commandlet world."));
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

		OutResult.ProjectionHashBefore = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashBefore = Actor->CurrentMetrics.StateHash;
		OutResult.CrustStateHashBefore = Actor->CurrentMetrics.CrustStateHash;
		OutResult.AuditBefore = ReadAudit(*Actor);

		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			const double StepStartSeconds = FPlatformTime::Seconds();
			Actor->StepOnce();
			OutResult.StepWallSecondsTotal += FPlatformTime::Seconds() - StepStartSeconds;
			OutResult.StepProjectionSecondsTotal += Actor->CurrentMetrics.ProjectionSeconds;
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

		OutResult.ProjectionHashAfter = OutResult.FilterMetrics.ProjectionHashAfter;
		OutResult.StateHashAfter = OutResult.FilterMetrics.StateHashAfter;
		OutResult.CrustStateHashAfter = Actor->CurrentMetrics.CrustStateHash;
		OutResult.MaterialLedgerHash = OutResult.LedgerMetrics.MaterialLedgerHash;
		OutResult.AuditAfter = ReadAudit(*Actor);
		OutResult.TotalSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
		OutResult.bCompleted = true;

		Actor->Destroy();
		return true;
	}

	bool AuditIsZero(const FPhaseIIIA2Audit& Audit)
	{
		return Audit.MaxAbsSampleElevation <= CrustFieldTolerance &&
			Audit.MaxAbsPlateVertexElevation <= CrustFieldTolerance &&
			Audit.MaxAbsSampleOceanicAge <= CrustFieldTolerance &&
			Audit.MaxAbsPlateVertexOceanicAge <= CrustFieldTolerance;
	}

	FString ReplayJson(const FPhaseIIIA2ReplayResult& Result)
	{
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		const double AvgStepProjection = Result.StepCount > 0 ? Result.StepProjectionSecondsTotal / static_cast<double>(Result.StepCount) : 0.0;
		return FString::Printf(
			TEXT("{\"fixture\":\"iiia2_60k_primary\",\"replay\":%d,\"step_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"avg_step_projection_seconds\":%.12f,\"total_replay_seconds\":%.12f,\"projection_hash_before\":%s,\"projection_hash_after\":%s,\"state_hash_before\":%s,\"state_hash_after\":%s,\"crust_state_hash_before\":%s,\"crust_state_hash_after\":%s,\"material_ledger_hash\":%s,\"contact_hash\":%s,\"label_hash\":%s,\"filter_decision_hash\":%s,\"max_abs_sample_elevation_before\":%.15f,\"max_abs_plate_vertex_elevation_before\":%.15f,\"max_abs_sample_elevation_after\":%.15f,\"max_abs_plate_vertex_elevation_after\":%.15f,\"max_abs_sample_oceanic_age_before\":%.15f,\"max_abs_plate_vertex_oceanic_age_before\":%.15f,\"max_abs_sample_oceanic_age_after\":%.15f,\"max_abs_plate_vertex_oceanic_age_after\":%.15f,\"sample_audit_count\":%d,\"plate_vertex_audit_count\":%d,\"material_record_count\":%d,\"changed_record_count\":%d,\"memory_gb\":%.12f}"),
			Result.Replay,
			Result.StepCount,
			Result.SampleCount,
			Result.PlateCount,
			AvgStepProjection,
			Result.TotalSeconds,
			*JsonString(Result.ProjectionHashBefore),
			*JsonString(Result.ProjectionHashAfter),
			*JsonString(Result.StateHashBefore),
			*JsonString(Result.StateHashAfter),
			*JsonString(Result.CrustStateHashBefore),
			*JsonString(Result.CrustStateHashAfter),
			*JsonString(Result.MaterialLedgerHash),
			*JsonString(Result.ContactMetrics.ContactLogHash),
			*JsonString(Result.LabelMetrics.TriangleLabelHash),
			*JsonString(Result.FilterMetrics.FilterDecisionHash),
			Result.AuditBefore.MaxAbsSampleElevation,
			Result.AuditBefore.MaxAbsPlateVertexElevation,
			Result.AuditAfter.MaxAbsSampleElevation,
			Result.AuditAfter.MaxAbsPlateVertexElevation,
			Result.AuditBefore.MaxAbsSampleOceanicAge,
			Result.AuditBefore.MaxAbsPlateVertexOceanicAge,
			Result.AuditAfter.MaxAbsSampleOceanicAge,
			Result.AuditAfter.MaxAbsPlateVertexOceanicAge,
			Result.AuditAfter.SampleCount,
			Result.AuditAfter.PlateVertexCount,
			Result.LedgerMetrics.RecordCount,
			Result.LedgerMetrics.ChangedRecordCount,
			MemoryGb);
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FPhaseIIBaseline& Baseline,
		const FPhaseIIIA2ReplayResult& A,
		const FPhaseIIIA2ReplayResult& B)
	{
		const bool bProjectionReplay = A.bCompleted && B.bCompleted && A.ProjectionHashAfter == B.ProjectionHashAfter;
		const bool bStateReplay = A.bCompleted && B.bCompleted && A.StateHashAfter == B.StateHashAfter;
		const bool bMaterialReplay = A.bCompleted && B.bCompleted && A.MaterialLedgerHash == B.MaterialLedgerHash;
		const bool bCrustReplay = A.bCompleted && B.bCompleted && A.CrustStateHashAfter == B.CrustStateHashAfter;
		const bool bFieldsZero = AuditIsZero(A.AuditBefore) && AuditIsZero(B.AuditBefore) && AuditIsZero(A.AuditAfter) && AuditIsZero(B.AuditAfter);
		const bool bStateBaseline = Baseline.bLoaded && A.StateHashAfter == Baseline.StateHashAfter;
		const bool bMaterialBaseline = Baseline.bLoaded && A.MaterialLedgerHash == Baseline.MaterialLedgerHash;
		const bool bAllPass = bProjectionReplay && bStateReplay && bMaterialReplay && bCrustReplay && bFieldsZero && bStateBaseline && bMaterialBaseline;

		FString Report = TEXT("# Phase III Slice IIIA.2 Checkpoint: Inert Oceanic Age\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This slice adds an inert `OceanicAge` scalar, in Ma, to global samples and plate-local carrier vertices. It does not generate ridge crust, increment age, subduct, collide, rift, erode, uplift, or amplify terrain. The existing Phase II `projection_hash`, `state_hash`, and material ledger stay unchanged; `crust_state_hash` is the additive Phase III field-aware hash and now includes both `Elevation` and `OceanicAge`.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		Report += FString::Printf(TEXT("| Projection replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bProjectionReplay), *A.ProjectionHashAfter, *B.ProjectionHashAfter);
		Report += FString::Printf(TEXT("| Phase II state replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bStateReplay), *A.StateHashAfter, *B.StateHashAfter);
		Report += FString::Printf(TEXT("| Phase II material ledger replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bMaterialReplay), *A.MaterialLedgerHash, *B.MaterialLedgerHash);
		Report += FString::Printf(TEXT("| Crust state replay hash includes `OceanicAge` | %s | `%s` vs `%s` |\n"), *PassFail(bCrustReplay), *A.CrustStateHashAfter, *B.CrustStateHashAfter);
		Report += FString::Printf(TEXT("| `Elevation` and `OceanicAge` remain zero before/after resampling | %s | max sample elevation %.3e km, max sample age %.3e Ma, max plate vertex age %.3e Ma |\n"), *PassFail(bFieldsZero), A.AuditAfter.MaxAbsSampleElevation, A.AuditAfter.MaxAbsSampleOceanicAge, A.AuditAfter.MaxAbsPlateVertexOceanicAge);
		Report += FString::Printf(TEXT("| Phase II baseline state hash unchanged | %s | baseline `%s`, IIIA.2 `%s` |\n"), *PassFail(bStateBaseline), Baseline.bLoaded ? *Baseline.StateHashAfter : TEXT("baseline unavailable"), *A.StateHashAfter);
		Report += FString::Printf(TEXT("| Phase II baseline material ledger unchanged | %s | baseline `%s`, IIIA.2 `%s` |\n"), *PassFail(bMaterialBaseline), Baseline.bLoaded ? *Baseline.MaterialLedgerHash : TEXT("baseline unavailable"), *A.MaterialLedgerHash);
		Report += TEXT("| Phase II baseline projection hash | no baseline available | Slice 5.5 artifact did not record `projection_hash_after`; replay match above covers determinism for this slice. |\n\n");

		Report += TEXT("## Hashes\n\n");
		Report += TEXT("| Replay | Projection before | Projection after | State before | State after | Crust before | Crust after | Material ledger |\n");
		Report += TEXT("|---:|---|---|---|---|---|---|---|\n");
		for (const FPhaseIIIA2ReplayResult* Result : {&A, &B})
		{
			Report += FString::Printf(
				TEXT("| %d | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` |\n"),
				Result->Replay,
				*Result->ProjectionHashBefore,
				*Result->ProjectionHashAfter,
				*Result->StateHashBefore,
				*Result->StateHashAfter,
				*Result->CrustStateHashBefore,
				*Result->CrustStateHashAfter,
				*Result->MaterialLedgerHash);
		}

		Report += TEXT("\n## Crust Field Audit\n\n");
		Report += TEXT("| Replay | Samples | Plate-local vertices | Max sample elevation before km | Max sample elevation after km | Max sample age before Ma | Max sample age after Ma | Max plate vertex age after Ma |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FPhaseIIIA2ReplayResult* Result : {&A, &B})
		{
			Report += FString::Printf(
				TEXT("| %d | %d | %d | %.3e | %.3e | %.3e | %.3e | %.3e |\n"),
				Result->Replay,
				Result->AuditAfter.SampleCount,
				Result->AuditAfter.PlateVertexCount,
				Result->AuditBefore.MaxAbsSampleElevation,
				Result->AuditAfter.MaxAbsSampleElevation,
				Result->AuditBefore.MaxAbsSampleOceanicAge,
				Result->AuditAfter.MaxAbsSampleOceanicAge,
				Result->AuditAfter.MaxAbsPlateVertexOceanicAge);
		}

		Report += TEXT("\n## Phase II Baseline\n\n");
		Report += FString::Printf(TEXT("Baseline artifact: `%s`\n\n"), *Baseline.Path);
		Report += TEXT("| Metric | Baseline | IIIA.2 replay 0 | Result |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += FString::Printf(TEXT("| State hash after resampling | `%s` | `%s` | %s |\n"), Baseline.bLoaded ? *Baseline.StateHashAfter : TEXT("unavailable"), *A.StateHashAfter, *PassFail(bStateBaseline));
		Report += FString::Printf(TEXT("| Material ledger hash | `%s` | `%s` | %s |\n"), Baseline.bLoaded ? *Baseline.MaterialLedgerHash : TEXT("unavailable"), *A.MaterialLedgerHash, *PassFail(bMaterialBaseline));

		Report += TEXT("\n## Notes\n\n");
		Report += TEXT("- `OceanicAge` is initialized to `0.0` on `FSphereSample` and copied into `FCarrierVertex` during plate-local triangulation rebuilds.\n");
		Report += TEXT("- Age generation at ridges belongs to IIIE; this slice only stores the field and proves it stays inert through the accepted Phase II filtered resampling path.\n");
		Report += TEXT("- `state_hash` deliberately excludes Phase III crust fields so prior Phase II checkpoints stay comparable. `crust_state_hash` now includes `Elevation` plus `OceanicAge`.\n");
		Report += TEXT("- The resampling event used here is the accepted Phase II filtered resampling path after 32 rigid-motion steps at 60k/40/seed 42.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIA.2 passes. Pause for user review before IIIA.3 (inert vector fields with per-step rotation).\n")
			: TEXT("Pause before IIIA.3. One or more inert-field or baseline-preservation gates require investigation.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIA2Commandlet::UCarrierLabPhaseIIIA2Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIA2Commandlet::Main(const FString& Params)
{
	const int32 SampleCount = FMath::Max(12, ParseIntParam(Params, TEXT("Samples="), 60000));
	const int32 PlateCount = FMath::Clamp(ParseIntParam(Params, TEXT("Plates="), 40), 1, 63);
	const int32 StepCount = FMath::Max(1, ParseIntParam(Params, TEXT("Steps="), 32));
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FPhaseIIIA2ReplayResult A;
	FPhaseIIIA2ReplayResult B;
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

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIA.2 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIA.2 report: %s"), *ReportPath);

	const bool bProjectionReplay = A.bCompleted && B.bCompleted && A.ProjectionHashAfter == B.ProjectionHashAfter;
	const bool bStateReplay = A.bCompleted && B.bCompleted && A.StateHashAfter == B.StateHashAfter;
	const bool bMaterialReplay = A.bCompleted && B.bCompleted && A.MaterialLedgerHash == B.MaterialLedgerHash;
	const bool bCrustReplay = A.bCompleted && B.bCompleted && A.CrustStateHashAfter == B.CrustStateHashAfter;
	const bool bFieldsZero = AuditIsZero(A.AuditBefore) && AuditIsZero(B.AuditBefore) && AuditIsZero(A.AuditAfter) && AuditIsZero(B.AuditAfter);
	const bool bStateBaseline = Baseline.bLoaded && A.StateHashAfter == Baseline.StateHashAfter;
	const bool bMaterialBaseline = Baseline.bLoaded && A.MaterialLedgerHash == Baseline.MaterialLedgerHash;
	return (bRunA && bRunB && bProjectionReplay && bStateReplay && bMaterialReplay && bCrustReplay && bFieldsZero && bStateBaseline && bMaterialBaseline) ? 0 : 2;
}
