// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIA4Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr double ZeroFieldTolerance = 1.0e-12;
	constexpr double VectorTangencyTolerance = 1.0e-8;

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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIA4"), Stamp);
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
				TEXT("phase-iii-slice-iiia4-report.md"));
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

	struct FPhaseIIIA4PrimaryResult
	{
		int32 Replay = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		double TotalSeconds = 0.0;
		FString ProjectionHashBefore;
		FString ProjectionHashAfter;
		FString StateHashBefore;
		FString StateHashAfter;
		FString CrustStateHashBefore;
		FString CrustStateHashAfter;
		FString MaterialLedgerHash;
		FCarrierLabPhaseIIIA4FieldAudit FieldAuditBefore;
		FCarrierLabPhaseIIIA4FieldAudit FieldAuditAfter;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterMetrics;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerMetrics;
		bool bCompleted = false;
	};

	struct FPhaseIIIA4SmearResult
	{
		int32 Replay = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		double TotalSeconds = 0.0;
		FString CrustStateHashBeforeSeed;
		FString CrustStateHashAfterSeed;
		FString CrustStateHashAfter;
		FString StateHashAfter;
		FString MaterialLedgerHash;
		FCarrierLabPhaseIIIA4SeedMetrics SeedMetrics;
		FCarrierLabPhaseIIIA4FieldAudit FieldAuditAfter;
		FCarrierLabPhaseIIIA4FieldAudit GapFillAuditAfter;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterMetrics;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerMetrics;
		int32 GapFillRecordCount = 0;
		bool bCompleted = false;
	};

	bool FieldAuditIsZero(const FCarrierLabPhaseIIIA4FieldAudit& Audit)
	{
		return Audit.MaxAbsSampleElevation <= ZeroFieldTolerance &&
			Audit.MaxAbsSampleOceanicAge <= ZeroFieldTolerance &&
			Audit.MaxSampleVectorMagnitude <= ZeroFieldTolerance &&
			Audit.MaxAbsPlateVertexElevation <= ZeroFieldTolerance &&
			Audit.MaxAbsPlateVertexOceanicAge <= ZeroFieldTolerance &&
			Audit.MaxPlateVertexVectorMagnitude <= ZeroFieldTolerance;
	}

	bool RunPrimaryReplay(
		const int32 Replay,
		const int32 SampleCount,
		const int32 PlateCount,
		const int32 StepCount,
		FPhaseIIIA4PrimaryResult& OutResult)
	{
		OutResult = FPhaseIIIA4PrimaryResult();
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

		OutResult.ProjectionHashBefore = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashBefore = Actor->CurrentMetrics.StateHash;
		OutResult.CrustStateHashBefore = Actor->CurrentMetrics.CrustStateHash;
		Actor->GetPhaseIIIA4FieldAudit(10.0, OutResult.FieldAuditBefore);

		for (int32 Step = 0; Step < StepCount; ++Step)
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
		Actor->GetPhaseIIIA4FieldAudit(10.0, OutResult.FieldAuditAfter);
		OutResult.TotalSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
		OutResult.bCompleted = true;

		Actor->Destroy();
		return true;
	}

	bool RunSmearReplay(
		const int32 Replay,
		const int32 SampleCount,
		const int32 PlateCount,
		const int32 StepCount,
		FPhaseIIIA4SmearResult& OutResult)
	{
		OutResult = FPhaseIIIA4SmearResult();
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

		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			Actor->StepOnce();
		}

		OutResult.CrustStateHashBeforeSeed = Actor->CurrentMetrics.CrustStateHash;
		if (!Actor->SeedPhaseIIIA4BoundarySmearProbe(INDEX_NONE, OutResult.SeedMetrics))
		{
			Actor->Destroy();
			return false;
		}
		OutResult.CrustStateHashAfterSeed = Actor->CurrentMetrics.CrustStateHash;

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

		TArray<int32> GapFillSampleIds;
		for (const FCarrierLabPhaseIIMaterialRecord& Record : MaterialRecords)
		{
			if (Record.EventClass == ECarrierLabPhaseIIMaterialEventClass::OverwrittenByGapFill)
			{
				GapFillSampleIds.Add(Record.SampleId);
			}
		}
		OutResult.GapFillRecordCount = GapFillSampleIds.Num();

		OutResult.StateHashAfter = OutResult.FilterMetrics.StateHashAfter;
		OutResult.CrustStateHashAfter = Actor->CurrentMetrics.CrustStateHash;
		OutResult.MaterialLedgerHash = OutResult.LedgerMetrics.MaterialLedgerHash;
		Actor->GetPhaseIIIA4FieldAudit(OutResult.SeedMetrics.SeedElevation, OutResult.FieldAuditAfter);
		Actor->GetPhaseIIIA4FieldAuditForSamples(GapFillSampleIds, OutResult.SeedMetrics.SeedElevation, OutResult.GapFillAuditAfter);
		OutResult.TotalSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
		OutResult.bCompleted = true;

		Actor->Destroy();
		return true;
	}

	FString PrimaryJson(const FPhaseIIIA4PrimaryResult& Result)
	{
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		return FString::Printf(
			TEXT("{\"fixture\":\"iiia4_60k_primary_zero_fields\",\"replay\":%d,\"step_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"total_replay_seconds\":%.12f,\"projection_hash_before\":%s,\"projection_hash_after\":%s,\"state_hash_before\":%s,\"state_hash_after\":%s,\"crust_state_hash_before\":%s,\"crust_state_hash_after\":%s,\"material_ledger_hash\":%s,\"nonzero_sample_fields_after\":%d,\"nonzero_plate_vertex_fields_after\":%d,\"max_sample_vector_radial_dot\":%.15f,\"material_record_count\":%d,\"changed_record_count\":%d,\"memory_gb\":%.12f}"),
			Result.Replay,
			Result.StepCount,
			Result.SampleCount,
			Result.PlateCount,
			Result.TotalSeconds,
			*JsonString(Result.ProjectionHashBefore),
			*JsonString(Result.ProjectionHashAfter),
			*JsonString(Result.StateHashBefore),
			*JsonString(Result.StateHashAfter),
			*JsonString(Result.CrustStateHashBefore),
			*JsonString(Result.CrustStateHashAfter),
			*JsonString(Result.MaterialLedgerHash),
			Result.FieldAuditAfter.NonZeroSampleFieldCount,
			Result.FieldAuditAfter.NonZeroPlateVertexFieldCount,
			Result.FieldAuditAfter.MaxSampleVectorRadialDot,
			Result.LedgerMetrics.RecordCount,
			Result.LedgerMetrics.ChangedRecordCount,
			MemoryGb);
	}

	FString SmearJson(const FPhaseIIIA4SmearResult& Result)
	{
		return FString::Printf(
			TEXT("{\"fixture\":\"iiia4_boundary_smear\",\"replay\":%d,\"step_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"total_replay_seconds\":%.12f,\"seed_plate_id\":%d,\"seeded_vertices\":%d,\"zeroed_vertices\":%d,\"boundary_triangles\":%d,\"crust_state_hash_before_seed\":%s,\"crust_state_hash_after_seed\":%s,\"crust_state_hash_after\":%s,\"state_hash_after\":%s,\"material_ledger_hash\":%s,\"nonzero_sample_fields_after\":%d,\"smeared_sample_count\":%d,\"min_positive_elevation\":%.15f,\"max_positive_elevation\":%.15f,\"mean_positive_elevation\":%.15f,\"max_sample_vector_radial_dot\":%.15f,\"gap_fill_records\":%d,\"gap_fill_nonzero_fields\":%d,\"gap_fill_max_abs_elevation\":%.15f,\"gap_fill_max_vector_magnitude\":%.15f}"),
			Result.Replay,
			Result.StepCount,
			Result.SampleCount,
			Result.PlateCount,
			Result.TotalSeconds,
			Result.SeedMetrics.PlateId,
			Result.SeedMetrics.SeededVertexCount,
			Result.SeedMetrics.ZeroedVertexCount,
			Result.SeedMetrics.BoundaryTriangleCount,
			*JsonString(Result.CrustStateHashBeforeSeed),
			*JsonString(Result.CrustStateHashAfterSeed),
			*JsonString(Result.CrustStateHashAfter),
			*JsonString(Result.StateHashAfter),
			*JsonString(Result.MaterialLedgerHash),
			Result.FieldAuditAfter.NonZeroSampleFieldCount,
			Result.FieldAuditAfter.SmearedSampleCount,
			Result.FieldAuditAfter.MinPositiveSampleElevation,
			Result.FieldAuditAfter.MaxPositiveSampleElevation,
			Result.FieldAuditAfter.MeanPositiveSampleElevation,
			Result.FieldAuditAfter.MaxSampleVectorRadialDot,
			Result.GapFillRecordCount,
			Result.GapFillAuditAfter.NonZeroSampleFieldCount,
			Result.GapFillAuditAfter.MaxAbsSampleElevation,
			Result.GapFillAuditAfter.MaxSampleVectorMagnitude);
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FPhaseIIBaseline& Baseline,
		const FPhaseIIIA4PrimaryResult& A,
		const FPhaseIIIA4PrimaryResult& B,
		const FPhaseIIIA4SmearResult& SmearA,
		const FPhaseIIIA4SmearResult& SmearB)
	{
		const bool bProjectionReplay = A.bCompleted && B.bCompleted && A.ProjectionHashAfter == B.ProjectionHashAfter;
		const bool bStateReplay = A.bCompleted && B.bCompleted && A.StateHashAfter == B.StateHashAfter;
		const bool bMaterialReplay = A.bCompleted && B.bCompleted && A.MaterialLedgerHash == B.MaterialLedgerHash;
		const bool bCrustReplay = A.bCompleted && B.bCompleted && A.CrustStateHashAfter == B.CrustStateHashAfter;
		const bool bPrimaryFieldsZero = FieldAuditIsZero(A.FieldAuditBefore) && FieldAuditIsZero(B.FieldAuditBefore) &&
			FieldAuditIsZero(A.FieldAuditAfter) && FieldAuditIsZero(B.FieldAuditAfter);
		const bool bStateBaseline = Baseline.bLoaded && A.StateHashAfter == Baseline.StateHashAfter;
		const bool bMaterialBaseline = Baseline.bLoaded && A.MaterialLedgerHash == Baseline.MaterialLedgerHash;
		const bool bSmearReplay = SmearA.bCompleted && SmearB.bCompleted &&
			SmearA.CrustStateHashAfter == SmearB.CrustStateHashAfter &&
			SmearA.StateHashAfter == SmearB.StateHashAfter &&
			SmearA.MaterialLedgerHash == SmearB.MaterialLedgerHash;
		const bool bSmearObserved = SmearA.FieldAuditAfter.SmearedSampleCount > 0 &&
			SmearA.FieldAuditAfter.NonZeroSampleFieldCount > 0 &&
			SmearA.FieldAuditAfter.MaxPositiveSampleElevation <= SmearA.SeedMetrics.SeedElevation + 1.0e-9;
		const bool bGapFillZero = SmearA.GapFillRecordCount > 0 &&
			SmearB.GapFillRecordCount == SmearA.GapFillRecordCount &&
			SmearA.GapFillAuditAfter.NonZeroSampleFieldCount == 0 &&
			SmearB.GapFillAuditAfter.NonZeroSampleFieldCount == 0;
		const bool bTangency = SmearA.FieldAuditAfter.MaxSampleVectorRadialDot <= VectorTangencyTolerance &&
			SmearB.FieldAuditAfter.MaxSampleVectorRadialDot <= VectorTangencyTolerance;
		const bool bAllPass = bProjectionReplay && bStateReplay && bMaterialReplay && bCrustReplay &&
			bPrimaryFieldsZero && bStateBaseline && bMaterialBaseline && bSmearReplay &&
			bSmearObserved && bGapFillZero && bTangency;

		FString Report = TEXT("# Phase III Slice IIIA.4 Checkpoint: Field Interpolation Through Resampling\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This slice extends the accepted resampling paths to carry `Elevation`, `OceanicAge`, `RidgeDirection`, and `FoldDirection` through barycentric interpolation. Vector fields are interpolated component-wise, projected back into the target sample tangent plane, and normalized when non-zero. Gap-fill samples still receive zero Phase III fields; ridge generation populates them later in IIIE.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		Report += FString::Printf(TEXT("| Projection replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bProjectionReplay), *A.ProjectionHashAfter, *B.ProjectionHashAfter);
		Report += FString::Printf(TEXT("| Phase II state replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bStateReplay), *A.StateHashAfter, *B.StateHashAfter);
		Report += FString::Printf(TEXT("| Phase II material ledger replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bMaterialReplay), *A.MaterialLedgerHash, *B.MaterialLedgerHash);
		Report += FString::Printf(TEXT("| Crust state replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bCrustReplay), *A.CrustStateHashAfter, *B.CrustStateHashAfter);
		Report += FString::Printf(TEXT("| Zero-field production run remains zero | %s | non-zero samples %d, non-zero plate vertices %d |\n"), *PassFail(bPrimaryFieldsZero), A.FieldAuditAfter.NonZeroSampleFieldCount, A.FieldAuditAfter.NonZeroPlateVertexFieldCount);
		Report += FString::Printf(TEXT("| Boundary smear observed | %s | %d smeared samples, elevation range %.6f..%.6f km |\n"), *PassFail(bSmearObserved), SmearA.FieldAuditAfter.SmearedSampleCount, SmearA.FieldAuditAfter.MinPositiveSampleElevation, SmearA.FieldAuditAfter.MaxPositiveSampleElevation);
		Report += FString::Printf(TEXT("| Boundary smear replay hash | %s | crust `%s` vs `%s` |\n"), *PassFail(bSmearReplay), *SmearA.CrustStateHashAfter, *SmearB.CrustStateHashAfter);
		Report += FString::Printf(TEXT("| Gap-fill fields remain zero | %s | %d gap-fill samples, max gap elevation %.3e km, max vector %.3e |\n"), *PassFail(bGapFillZero), SmearA.GapFillRecordCount, SmearA.GapFillAuditAfter.MaxAbsSampleElevation, SmearA.GapFillAuditAfter.MaxSampleVectorMagnitude);
		Report += FString::Printf(TEXT("| Interpolated vectors remain tangent | %s | max radial dot %.3e |\n"), *PassFail(bTangency), FMath::Max(SmearA.FieldAuditAfter.MaxSampleVectorRadialDot, SmearB.FieldAuditAfter.MaxSampleVectorRadialDot));
		Report += FString::Printf(TEXT("| Phase II baseline state hash unchanged | %s | baseline `%s`, IIIA.4 `%s` |\n"), *PassFail(bStateBaseline), Baseline.bLoaded ? *Baseline.StateHashAfter : TEXT("baseline unavailable"), *A.StateHashAfter);
		Report += FString::Printf(TEXT("| Phase II baseline material ledger unchanged | %s | baseline `%s`, IIIA.4 `%s` |\n"), *PassFail(bMaterialBaseline), Baseline.bLoaded ? *Baseline.MaterialLedgerHash : TEXT("baseline unavailable"), *A.MaterialLedgerHash);

		Report += TEXT("\n## Primary Zero-Field Replay\n\n");
		Report += TEXT("| Replay | Projection after | State after | Crust after | Material ledger | Non-zero samples | Non-zero plate vertices |\n");
		Report += TEXT("|---:|---|---|---|---|---:|---:|\n");
		for (const FPhaseIIIA4PrimaryResult* Result : {&A, &B})
		{
			Report += FString::Printf(
				TEXT("| %d | `%s` | `%s` | `%s` | `%s` | %d | %d |\n"),
				Result->Replay,
				*Result->ProjectionHashAfter,
				*Result->StateHashAfter,
				*Result->CrustStateHashAfter,
				*Result->MaterialLedgerHash,
				Result->FieldAuditAfter.NonZeroSampleFieldCount,
				Result->FieldAuditAfter.NonZeroPlateVertexFieldCount);
		}

		Report += TEXT("\n## Boundary Smear Probe\n\n");
		Report += TEXT("| Replay | Seed plate | Seeded vertices | Zeroed vertices | Boundary tris | Smeared samples | Non-zero samples | Elevation min/mean/max km | Gap fills | Gap non-zero fields | Max vector radial dot | Crust after |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---|---:|---:|---:|---|\n");
		for (const FPhaseIIIA4SmearResult* Result : {&SmearA, &SmearB})
		{
			Report += FString::Printf(
				TEXT("| %d | %d | %d | %d | %d | %d | %d | %.6f / %.6f / %.6f | %d | %d | %.3e | `%s` |\n"),
				Result->Replay,
				Result->SeedMetrics.PlateId,
				Result->SeedMetrics.SeededVertexCount,
				Result->SeedMetrics.ZeroedVertexCount,
				Result->SeedMetrics.BoundaryTriangleCount,
				Result->FieldAuditAfter.SmearedSampleCount,
				Result->FieldAuditAfter.NonZeroSampleFieldCount,
				Result->FieldAuditAfter.MinPositiveSampleElevation,
				Result->FieldAuditAfter.MeanPositiveSampleElevation,
				Result->FieldAuditAfter.MaxPositiveSampleElevation,
				Result->GapFillRecordCount,
				Result->GapFillAuditAfter.NonZeroSampleFieldCount,
				Result->FieldAuditAfter.MaxSampleVectorRadialDot,
				*Result->CrustStateHashAfter);
		}

		Report += TEXT("\n## Phase II Baseline\n\n");
		Report += FString::Printf(TEXT("Baseline artifact: `%s`\n\n"), *Baseline.Path);
		Report += TEXT("| Metric | Baseline | IIIA.4 replay 0 | Result |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += FString::Printf(TEXT("| State hash after resampling | `%s` | `%s` | %s |\n"), Baseline.bLoaded ? *Baseline.StateHashAfter : TEXT("unavailable"), *A.StateHashAfter, *PassFail(bStateBaseline));
		Report += FString::Printf(TEXT("| Material ledger hash | `%s` | `%s` | %s |\n"), Baseline.bLoaded ? *Baseline.MaterialLedgerHash : TEXT("unavailable"), *A.MaterialLedgerHash, *PassFail(bMaterialBaseline));

		Report += TEXT("\n## Notes\n\n");
		Report += TEXT("- Valid single-hit resampling now copies all Phase IIIA crust fields from the hit plate-local triangle by barycentric interpolation.\n");
		Report += TEXT("- The synthetic boundary-smear fixture seeds one side of a boundary triangle on one plate-local mesh and leaves the opposite-side boundary vertices at zero, so fractional post-resample values are expected evidence of interpolation rather than value passthrough.\n");
		Report += TEXT("- Gap-fill samples are explicitly zeroed for Phase III fields. IIIE will populate ridge direction, oceanic age, and ridge elevation as the paper's divergent-zone process.\n");
		Report += TEXT("- Phase II `state_hash` and material ledger remain comparable because they intentionally exclude Phase IIIA crust fields.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIA.4 passes. Pause for user review before IIIA consolidation.\n")
			: TEXT("Pause before IIIA consolidation. One or more field-interpolation or baseline-preservation gates require investigation.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIA4Commandlet::UCarrierLabPhaseIIIA4Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIA4Commandlet::Main(const FString& Params)
{
	const int32 SampleCount = FMath::Max(12, ParseIntParam(Params, TEXT("Samples="), 60000));
	const int32 PlateCount = FMath::Clamp(ParseIntParam(Params, TEXT("Plates="), 40), 1, 63);
	const int32 StepCount = FMath::Max(1, ParseIntParam(Params, TEXT("Steps="), 32));
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FPhaseIIIA4PrimaryResult A;
	FPhaseIIIA4PrimaryResult B;
	FPhaseIIIA4SmearResult SmearA;
	FPhaseIIIA4SmearResult SmearB;
	const bool bRunA = RunPrimaryReplay(0, SampleCount, PlateCount, StepCount, A);
	const bool bRunB = RunPrimaryReplay(1, SampleCount, PlateCount, StepCount, B);
	const bool bRunSmearA = RunSmearReplay(0, SampleCount, PlateCount, StepCount, SmearA);
	const bool bRunSmearB = RunSmearReplay(1, SampleCount, PlateCount, StepCount, SmearB);
	const FPhaseIIBaseline Baseline = LoadPhaseIIBaseline();

	FString MetricsJsonl;
	MetricsJsonl += PrimaryJson(A) + LINE_TERMINATOR;
	MetricsJsonl += PrimaryJson(B) + LINE_TERMINATOR;
	MetricsJsonl += SmearJson(SmearA) + LINE_TERMINATOR;
	MetricsJsonl += SmearJson(SmearB) + LINE_TERMINATOR;
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);

	const FString Report = BuildReport(OutputRoot, Baseline, A, B, SmearA, SmearB);
	const FString ReportPath = ResolveReportPath(Params);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIA.4 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIA.4 report: %s"), *ReportPath);

	const bool bProjectionReplay = A.bCompleted && B.bCompleted && A.ProjectionHashAfter == B.ProjectionHashAfter;
	const bool bStateReplay = A.bCompleted && B.bCompleted && A.StateHashAfter == B.StateHashAfter;
	const bool bMaterialReplay = A.bCompleted && B.bCompleted && A.MaterialLedgerHash == B.MaterialLedgerHash;
	const bool bCrustReplay = A.bCompleted && B.bCompleted && A.CrustStateHashAfter == B.CrustStateHashAfter;
	const bool bPrimaryFieldsZero = FieldAuditIsZero(A.FieldAuditBefore) && FieldAuditIsZero(B.FieldAuditBefore) &&
		FieldAuditIsZero(A.FieldAuditAfter) && FieldAuditIsZero(B.FieldAuditAfter);
	const bool bStateBaseline = Baseline.bLoaded && A.StateHashAfter == Baseline.StateHashAfter;
	const bool bMaterialBaseline = Baseline.bLoaded && A.MaterialLedgerHash == Baseline.MaterialLedgerHash;
	const bool bSmearReplay = SmearA.bCompleted && SmearB.bCompleted &&
		SmearA.CrustStateHashAfter == SmearB.CrustStateHashAfter &&
		SmearA.StateHashAfter == SmearB.StateHashAfter &&
		SmearA.MaterialLedgerHash == SmearB.MaterialLedgerHash;
	const bool bSmearObserved = SmearA.FieldAuditAfter.SmearedSampleCount > 0 &&
		SmearA.FieldAuditAfter.NonZeroSampleFieldCount > 0 &&
		SmearA.FieldAuditAfter.MaxPositiveSampleElevation <= SmearA.SeedMetrics.SeedElevation + 1.0e-9;
	const bool bGapFillZero = SmearA.GapFillRecordCount > 0 &&
		SmearB.GapFillRecordCount == SmearA.GapFillRecordCount &&
		SmearA.GapFillAuditAfter.NonZeroSampleFieldCount == 0 &&
		SmearB.GapFillAuditAfter.NonZeroSampleFieldCount == 0;
	const bool bTangency = SmearA.FieldAuditAfter.MaxSampleVectorRadialDot <= VectorTangencyTolerance &&
		SmearB.FieldAuditAfter.MaxSampleVectorRadialDot <= VectorTangencyTolerance;

	return (bRunA && bRunB && bRunSmearA && bRunSmearB &&
		bProjectionReplay && bStateReplay && bMaterialReplay && bCrustReplay &&
		bPrimaryFieldsZero && bStateBaseline && bMaterialBaseline && bSmearReplay &&
		bSmearObserved && bGapFillZero && bTangency) ? 0 : 2;
}
