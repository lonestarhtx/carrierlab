// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIA3Commandlet.h"

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
	constexpr double VectorRotationTolerance = 1.0e-8;

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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIA3"), Stamp);
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
				TEXT("phase-iii-slice-iiia3-report.md"));
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

	FVector3d RotateWithQuat(const FVector3d& Vector, const FVector3d& Axis, const double AngleRadians)
	{
		if (Vector.SquaredLength() <= UE_DOUBLE_SMALL_NUMBER)
		{
			return FVector3d::ZeroVector;
		}
		const FQuat4d Rotation(Axis.GetSafeNormal(), AngleRadians);
		return Rotation.RotateVector(Vector);
	}

	double AngularErrorRadians(const FVector3d& Expected, const FVector3d& Actual)
	{
		if (Expected.SquaredLength() <= UE_DOUBLE_SMALL_NUMBER || Actual.SquaredLength() <= UE_DOUBLE_SMALL_NUMBER)
		{
			return TNumericLimits<double>::Max();
		}
		const double ChordLength = (Expected.GetSafeNormal() - Actual.GetSafeNormal()).Size();
		return 2.0 * FMath::Asin(FMath::Clamp(0.5 * ChordLength, 0.0, 1.0));
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

	struct FPhaseIIIA3VectorAudit
	{
		int32 SampleCount = 0;
		int32 PlateVertexCount = 0;
		double MaxSampleVectorMagnitude = 0.0;
		double MaxPlateVertexVectorMagnitude = 0.0;
		double MaxPlateVertexRadialDot = 0.0;
	};

	struct FPhaseIIIA3PrimaryResult
	{
		int32 Replay = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		double TotalSeconds = 0.0;
		double StepProjectionSecondsTotal = 0.0;
		FString ProjectionHashBefore;
		FString ProjectionHashAfter;
		FString StateHashBefore;
		FString StateHashAfter;
		FString CrustStateHashBefore;
		FString CrustStateHashAfter;
		FString MaterialLedgerHash;
		FPhaseIIIA3VectorAudit VectorAuditBefore;
		FPhaseIIIA3VectorAudit VectorAuditAfter;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterMetrics;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerMetrics;
		bool bCompleted = false;
	};

	struct FPhaseIIIA3ForcedVectorResult
	{
		int32 Replay = 0;
		int32 StepCount = 0;
		int32 PlateId = INDEX_NONE;
		int32 LocalVertexId = INDEX_NONE;
		double RidgeAngularErrorRadians = 0.0;
		double FoldAngularErrorRadians = 0.0;
		double PositionAngularErrorRadians = 0.0;
		double RidgeMagnitudeError = 0.0;
		double FoldMagnitudeError = 0.0;
		double RidgeRadialDot = 0.0;
		double FoldRadialDot = 0.0;
		FString StateHashBefore;
		FString StateHashAfter;
		FString CrustStateHashBefore;
		FString CrustStateHashAfter;
		bool bCompleted = false;
	};

	FPhaseIIIA3VectorAudit ReadVectorAudit(const ACarrierLabVisualizationActor& Actor)
	{
		FPhaseIIIA3VectorAudit Audit;
		Actor.GetPhaseIIIA3VectorFieldAudit(
			Audit.SampleCount,
			Audit.PlateVertexCount,
			Audit.MaxSampleVectorMagnitude,
			Audit.MaxPlateVertexVectorMagnitude,
			Audit.MaxPlateVertexRadialDot);
		return Audit;
	}

	bool RunPrimaryReplay(const int32 Replay, const int32 SampleCount, const int32 PlateCount, const int32 StepCount, FPhaseIIIA3PrimaryResult& OutResult)
	{
		OutResult = FPhaseIIIA3PrimaryResult();
		OutResult.Replay = Replay;
		OutResult.SampleCount = SampleCount;
		OutResult.PlateCount = PlateCount;
		OutResult.StepCount = StepCount;

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("CarrierLab Phase IIIA.3 could not find a commandlet world."));
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
		OutResult.VectorAuditBefore = ReadVectorAudit(*Actor);

		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			Actor->StepOnce();
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
		OutResult.VectorAuditAfter = ReadVectorAudit(*Actor);
		OutResult.TotalSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
		OutResult.bCompleted = true;

		Actor->Destroy();
		return true;
	}

	bool RunForcedVectorReplay(const int32 Replay, const int32 StepCount, FPhaseIIIA3ForcedVectorResult& OutResult)
	{
		OutResult = FPhaseIIIA3ForcedVectorResult();
		OutResult.Replay = Replay;
		OutResult.StepCount = StepCount;

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, 10000, 40);
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

		FVector3d InitialPosition;
		FVector3d InitialRidge;
		FVector3d InitialFold;
		FVector3d RotationAxis;
		double AngularSpeed = 0.0;
		if (!Actor->SeedPhaseIIIA3VectorAuditProbe(
			OutResult.PlateId,
			OutResult.LocalVertexId,
			InitialPosition,
			InitialRidge,
			InitialFold,
			RotationAxis,
			AngularSpeed))
		{
			Actor->Destroy();
			return false;
		}

		OutResult.StateHashBefore = Actor->CurrentMetrics.StateHash;
		OutResult.CrustStateHashBefore = Actor->CurrentMetrics.CrustStateHash;

		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			Actor->StepOnce();
		}

		FVector3d ActualPosition;
		FVector3d ActualRidge;
		FVector3d ActualFold;
		if (!Actor->GetPhaseIIIA3VectorAuditProbe(
			OutResult.PlateId,
			OutResult.LocalVertexId,
			ActualPosition,
			ActualRidge,
			ActualFold))
		{
			Actor->Destroy();
			return false;
		}

		const double Angle = AngularSpeed * static_cast<double>(StepCount);
		const FVector3d ExpectedPosition = RotateWithQuat(InitialPosition, RotationAxis, Angle).GetSafeNormal();
		const FVector3d ExpectedRidge = RotateWithQuat(InitialRidge, RotationAxis, Angle);
		const FVector3d ExpectedFold = RotateWithQuat(InitialFold, RotationAxis, Angle);
		OutResult.PositionAngularErrorRadians = AngularErrorRadians(ExpectedPosition, ActualPosition);
		OutResult.RidgeAngularErrorRadians = AngularErrorRadians(ExpectedRidge, ActualRidge);
		OutResult.FoldAngularErrorRadians = AngularErrorRadians(ExpectedFold, ActualFold);
		OutResult.RidgeMagnitudeError = FMath::Abs(ActualRidge.Size() - InitialRidge.Size());
		OutResult.FoldMagnitudeError = FMath::Abs(ActualFold.Size() - InitialFold.Size());
		OutResult.RidgeRadialDot = FMath::Abs(FVector3d::DotProduct(ActualPosition.GetSafeNormal(), ActualRidge));
		OutResult.FoldRadialDot = FMath::Abs(FVector3d::DotProduct(ActualPosition.GetSafeNormal(), ActualFold));
		OutResult.StateHashAfter = Actor->CurrentMetrics.StateHash;
		OutResult.CrustStateHashAfter = Actor->CurrentMetrics.CrustStateHash;
		OutResult.bCompleted = true;

		Actor->Destroy();
		return true;
	}

	bool VectorAuditIsZero(const FPhaseIIIA3VectorAudit& Audit)
	{
		return Audit.MaxSampleVectorMagnitude <= ZeroFieldTolerance &&
			Audit.MaxPlateVertexVectorMagnitude <= ZeroFieldTolerance &&
			Audit.MaxPlateVertexRadialDot <= ZeroFieldTolerance;
	}

	bool ForcedVectorPasses(const FPhaseIIIA3ForcedVectorResult& Result)
	{
		return Result.bCompleted &&
			Result.PositionAngularErrorRadians <= VectorRotationTolerance &&
			Result.RidgeAngularErrorRadians <= VectorRotationTolerance &&
			Result.FoldAngularErrorRadians <= VectorRotationTolerance &&
			Result.RidgeMagnitudeError <= VectorRotationTolerance &&
			Result.FoldMagnitudeError <= VectorRotationTolerance &&
			Result.RidgeRadialDot <= VectorRotationTolerance &&
			Result.FoldRadialDot <= VectorRotationTolerance;
	}

	FString PrimaryReplayJson(const FPhaseIIIA3PrimaryResult& Result)
	{
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		const double AvgStepProjection = Result.StepCount > 0 ? Result.StepProjectionSecondsTotal / static_cast<double>(Result.StepCount) : 0.0;
		return FString::Printf(
			TEXT("{\"fixture\":\"iiia3_60k_primary\",\"replay\":%d,\"step_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"avg_step_projection_seconds\":%.12f,\"total_replay_seconds\":%.12f,\"projection_hash_before\":%s,\"projection_hash_after\":%s,\"state_hash_before\":%s,\"state_hash_after\":%s,\"crust_state_hash_before\":%s,\"crust_state_hash_after\":%s,\"material_ledger_hash\":%s,\"contact_hash\":%s,\"label_hash\":%s,\"filter_decision_hash\":%s,\"max_sample_vector_before\":%.15f,\"max_plate_vector_before\":%.15f,\"max_plate_radial_dot_before\":%.15f,\"max_sample_vector_after\":%.15f,\"max_plate_vector_after\":%.15f,\"max_plate_radial_dot_after\":%.15f,\"sample_audit_count\":%d,\"plate_vertex_audit_count\":%d,\"material_record_count\":%d,\"changed_record_count\":%d,\"memory_gb\":%.12f}"),
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
			Result.VectorAuditBefore.MaxSampleVectorMagnitude,
			Result.VectorAuditBefore.MaxPlateVertexVectorMagnitude,
			Result.VectorAuditBefore.MaxPlateVertexRadialDot,
			Result.VectorAuditAfter.MaxSampleVectorMagnitude,
			Result.VectorAuditAfter.MaxPlateVertexVectorMagnitude,
			Result.VectorAuditAfter.MaxPlateVertexRadialDot,
			Result.VectorAuditAfter.SampleCount,
			Result.VectorAuditAfter.PlateVertexCount,
			Result.LedgerMetrics.RecordCount,
			Result.LedgerMetrics.ChangedRecordCount,
			MemoryGb);
	}

	FString ForcedReplayJson(const FPhaseIIIA3ForcedVectorResult& Result)
	{
		return FString::Printf(
			TEXT("{\"fixture\":\"iiia3_forced_vector_rotation\",\"replay\":%d,\"step_count\":%d,\"plate_id\":%d,\"local_vertex_id\":%d,\"state_hash_before\":%s,\"state_hash_after\":%s,\"crust_state_hash_before\":%s,\"crust_state_hash_after\":%s,\"position_angular_error_radians\":%.15f,\"ridge_angular_error_radians\":%.15f,\"fold_angular_error_radians\":%.15f,\"ridge_magnitude_error\":%.15f,\"fold_magnitude_error\":%.15f,\"ridge_radial_dot\":%.15f,\"fold_radial_dot\":%.15f}"),
			Result.Replay,
			Result.StepCount,
			Result.PlateId,
			Result.LocalVertexId,
			*JsonString(Result.StateHashBefore),
			*JsonString(Result.StateHashAfter),
			*JsonString(Result.CrustStateHashBefore),
			*JsonString(Result.CrustStateHashAfter),
			Result.PositionAngularErrorRadians,
			Result.RidgeAngularErrorRadians,
			Result.FoldAngularErrorRadians,
			Result.RidgeMagnitudeError,
			Result.FoldMagnitudeError,
			Result.RidgeRadialDot,
			Result.FoldRadialDot);
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FPhaseIIBaseline& Baseline,
		const FPhaseIIIA3PrimaryResult& A,
		const FPhaseIIIA3PrimaryResult& B,
		const FPhaseIIIA3ForcedVectorResult& ForcedA,
		const FPhaseIIIA3ForcedVectorResult& ForcedB)
	{
		const bool bProjectionReplay = A.bCompleted && B.bCompleted && A.ProjectionHashAfter == B.ProjectionHashAfter;
		const bool bStateReplay = A.bCompleted && B.bCompleted && A.StateHashAfter == B.StateHashAfter;
		const bool bMaterialReplay = A.bCompleted && B.bCompleted && A.MaterialLedgerHash == B.MaterialLedgerHash;
		const bool bCrustReplay = A.bCompleted && B.bCompleted && A.CrustStateHashAfter == B.CrustStateHashAfter;
		const bool bVectorsZero = VectorAuditIsZero(A.VectorAuditBefore) && VectorAuditIsZero(B.VectorAuditBefore) && VectorAuditIsZero(A.VectorAuditAfter) && VectorAuditIsZero(B.VectorAuditAfter);
		const bool bStateBaseline = Baseline.bLoaded && A.StateHashAfter == Baseline.StateHashAfter;
		const bool bMaterialBaseline = Baseline.bLoaded && A.MaterialLedgerHash == Baseline.MaterialLedgerHash;
		const bool bForcedA = ForcedVectorPasses(ForcedA);
		const bool bForcedB = ForcedVectorPasses(ForcedB);
		const bool bForcedReplay = ForcedA.bCompleted && ForcedB.bCompleted &&
			ForcedA.CrustStateHashAfter == ForcedB.CrustStateHashAfter &&
			ForcedA.StateHashAfter == ForcedB.StateHashAfter;
		const bool bAllPass = bProjectionReplay && bStateReplay && bMaterialReplay && bCrustReplay && bVectorsZero && bStateBaseline && bMaterialBaseline && bForcedA && bForcedB && bForcedReplay;

		FString Report = TEXT("# Phase III Slice IIIA.3 Checkpoint: Inert Vector Fields With Per-Step Rotation\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This slice adds inert tangent vector storage for `RidgeDirection` and `FoldDirection` on global samples and plate-local vertices. Production initialization remains zero. The only behavior added is that non-zero plate-local vectors rotate with the same per-step geodetic transform as their vertex positions; no ridge generation, folding, collision, rifting, uplift, erosion, amplification, or material mutation is added.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		Report += FString::Printf(TEXT("| Projection replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bProjectionReplay), *A.ProjectionHashAfter, *B.ProjectionHashAfter);
		Report += FString::Printf(TEXT("| Phase II state replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bStateReplay), *A.StateHashAfter, *B.StateHashAfter);
		Report += FString::Printf(TEXT("| Phase II material ledger replay hash | %s | `%s` vs `%s` |\n"), *PassFail(bMaterialReplay), *A.MaterialLedgerHash, *B.MaterialLedgerHash);
		Report += FString::Printf(TEXT("| Crust state replay hash includes vector fields | %s | `%s` vs `%s` |\n"), *PassFail(bCrustReplay), *A.CrustStateHashAfter, *B.CrustStateHashAfter);
		Report += FString::Printf(TEXT("| Production vector fields remain zero through Phase II resampling | %s | max sample %.3e, max plate vertex %.3e |\n"), *PassFail(bVectorsZero), A.VectorAuditAfter.MaxSampleVectorMagnitude, A.VectorAuditAfter.MaxPlateVertexVectorMagnitude);
		Report += FString::Printf(TEXT("| Forced vector rotation oracle | %s | ridge %.3e rad / fold %.3e rad / position %.3e rad |\n"), *PassFail(bForcedA && bForcedB), FMath::Max(ForcedA.RidgeAngularErrorRadians, ForcedB.RidgeAngularErrorRadians), FMath::Max(ForcedA.FoldAngularErrorRadians, ForcedB.FoldAngularErrorRadians), FMath::Max(ForcedA.PositionAngularErrorRadians, ForcedB.PositionAngularErrorRadians));
		Report += FString::Printf(TEXT("| Forced vector replay hash | %s | crust `%s` vs `%s` |\n"), *PassFail(bForcedReplay), *ForcedA.CrustStateHashAfter, *ForcedB.CrustStateHashAfter);
		Report += FString::Printf(TEXT("| Phase II baseline state hash unchanged | %s | baseline `%s`, IIIA.3 `%s` |\n"), *PassFail(bStateBaseline), Baseline.bLoaded ? *Baseline.StateHashAfter : TEXT("baseline unavailable"), *A.StateHashAfter);
		Report += FString::Printf(TEXT("| Phase II baseline material ledger unchanged | %s | baseline `%s`, IIIA.3 `%s` |\n"), *PassFail(bMaterialBaseline), Baseline.bLoaded ? *Baseline.MaterialLedgerHash : TEXT("baseline unavailable"), *A.MaterialLedgerHash);

		Report += TEXT("\n## Primary Hashes\n\n");
		Report += TEXT("| Replay | Projection before | Projection after | State before | State after | Crust before | Crust after | Material ledger |\n");
		Report += TEXT("|---:|---|---|---|---|---|---|---|\n");
		for (const FPhaseIIIA3PrimaryResult* Result : {&A, &B})
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

		Report += TEXT("\n## Zero-Vector Audit\n\n");
		Report += TEXT("| Replay | Samples | Plate-local vertices | Max sample vector before | Max sample vector after | Max plate vector before | Max plate vector after | Max plate radial dot after |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FPhaseIIIA3PrimaryResult* Result : {&A, &B})
		{
			Report += FString::Printf(
				TEXT("| %d | %d | %d | %.3e | %.3e | %.3e | %.3e | %.3e |\n"),
				Result->Replay,
				Result->VectorAuditAfter.SampleCount,
				Result->VectorAuditAfter.PlateVertexCount,
				Result->VectorAuditBefore.MaxSampleVectorMagnitude,
				Result->VectorAuditAfter.MaxSampleVectorMagnitude,
				Result->VectorAuditBefore.MaxPlateVertexVectorMagnitude,
				Result->VectorAuditAfter.MaxPlateVertexVectorMagnitude,
				Result->VectorAuditAfter.MaxPlateVertexRadialDot);
		}

		Report += TEXT("\n## Forced-Rotation Probe\n\n");
		Report += TEXT("| Replay | Plate | Local vertex | Steps | Position err rad | Ridge err rad | Fold err rad | Ridge mag err | Fold mag err | Ridge radial dot | Fold radial dot | Crust after |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FPhaseIIIA3ForcedVectorResult* Result : {&ForcedA, &ForcedB})
		{
			Report += FString::Printf(
				TEXT("| %d | %d | %d | %d | %.3e | %.3e | %.3e | %.3e | %.3e | %.3e | %.3e | `%s` |\n"),
				Result->Replay,
				Result->PlateId,
				Result->LocalVertexId,
				Result->StepCount,
				Result->PositionAngularErrorRadians,
				Result->RidgeAngularErrorRadians,
				Result->FoldAngularErrorRadians,
				Result->RidgeMagnitudeError,
				Result->FoldMagnitudeError,
				Result->RidgeRadialDot,
				Result->FoldRadialDot,
				*Result->CrustStateHashAfter);
		}

		Report += TEXT("\n## Phase II Baseline\n\n");
		Report += FString::Printf(TEXT("Baseline artifact: `%s`\n\n"), *Baseline.Path);
		Report += TEXT("| Metric | Baseline | IIIA.3 replay 0 | Result |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += FString::Printf(TEXT("| State hash after resampling | `%s` | `%s` | %s |\n"), Baseline.bLoaded ? *Baseline.StateHashAfter : TEXT("unavailable"), *A.StateHashAfter, *PassFail(bStateBaseline));
		Report += FString::Printf(TEXT("| Material ledger hash | `%s` | `%s` | %s |\n"), Baseline.bLoaded ? *Baseline.MaterialLedgerHash : TEXT("unavailable"), *A.MaterialLedgerHash, *PassFail(bMaterialBaseline));

		Report += TEXT("\n## Notes\n\n");
		Report += TEXT("- `RidgeDirection` and `FoldDirection` are initialized to zero on `FSphereSample` and copied into `FCarrierVertex` during plate-local triangulation rebuilds.\n");
		Report += TEXT("- Zero vectors are left zero during per-step rotation; non-zero vector fields rotate with the same axis and angular speed as their plate-local vertex positions.\n");
		Report += TEXT("- The forced-rotation oracle uses `FQuat4d` recomputation from the initial vector, axis, and elapsed angle rather than reading the implementation's rotated value as expected truth.\n");
		Report += TEXT("- `state_hash` deliberately excludes Phase III crust fields so prior Phase II checkpoints stay comparable. `crust_state_hash` now includes `Elevation`, `OceanicAge`, `RidgeDirection`, and `FoldDirection`.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIA.3 passes. Pause for user review before IIIA.4 (field interpolation through resampling).\n")
			: TEXT("Pause before IIIA.4. One or more vector-field or baseline-preservation gates require investigation.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIA3Commandlet::UCarrierLabPhaseIIIA3Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIA3Commandlet::Main(const FString& Params)
{
	const int32 SampleCount = FMath::Max(12, ParseIntParam(Params, TEXT("Samples="), 60000));
	const int32 PlateCount = FMath::Clamp(ParseIntParam(Params, TEXT("Plates="), 40), 1, 63);
	const int32 StepCount = FMath::Max(1, ParseIntParam(Params, TEXT("Steps="), 32));
	const int32 ForcedStepCount = FMath::Max(1, ParseIntParam(Params, TEXT("ForcedSteps="), 5));
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FPhaseIIIA3PrimaryResult A;
	FPhaseIIIA3PrimaryResult B;
	FPhaseIIIA3ForcedVectorResult ForcedA;
	FPhaseIIIA3ForcedVectorResult ForcedB;
	const bool bRunA = RunPrimaryReplay(0, SampleCount, PlateCount, StepCount, A);
	const bool bRunB = RunPrimaryReplay(1, SampleCount, PlateCount, StepCount, B);
	const bool bRunForcedA = RunForcedVectorReplay(0, ForcedStepCount, ForcedA);
	const bool bRunForcedB = RunForcedVectorReplay(1, ForcedStepCount, ForcedB);
	const FPhaseIIBaseline Baseline = LoadPhaseIIBaseline();

	FString MetricsJsonl;
	MetricsJsonl += PrimaryReplayJson(A) + LINE_TERMINATOR;
	MetricsJsonl += PrimaryReplayJson(B) + LINE_TERMINATOR;
	MetricsJsonl += ForcedReplayJson(ForcedA) + LINE_TERMINATOR;
	MetricsJsonl += ForcedReplayJson(ForcedB) + LINE_TERMINATOR;
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);

	const FString Report = BuildReport(OutputRoot, Baseline, A, B, ForcedA, ForcedB);
	const FString ReportPath = ResolveReportPath(Params);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIA.3 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIA.3 report: %s"), *ReportPath);

	const bool bProjectionReplay = A.bCompleted && B.bCompleted && A.ProjectionHashAfter == B.ProjectionHashAfter;
	const bool bStateReplay = A.bCompleted && B.bCompleted && A.StateHashAfter == B.StateHashAfter;
	const bool bMaterialReplay = A.bCompleted && B.bCompleted && A.MaterialLedgerHash == B.MaterialLedgerHash;
	const bool bCrustReplay = A.bCompleted && B.bCompleted && A.CrustStateHashAfter == B.CrustStateHashAfter;
	const bool bVectorsZero = VectorAuditIsZero(A.VectorAuditBefore) && VectorAuditIsZero(B.VectorAuditBefore) && VectorAuditIsZero(A.VectorAuditAfter) && VectorAuditIsZero(B.VectorAuditAfter);
	const bool bStateBaseline = Baseline.bLoaded && A.StateHashAfter == Baseline.StateHashAfter;
	const bool bMaterialBaseline = Baseline.bLoaded && A.MaterialLedgerHash == Baseline.MaterialLedgerHash;
	const bool bForcedA = ForcedVectorPasses(ForcedA);
	const bool bForcedB = ForcedVectorPasses(ForcedB);
	const bool bForcedReplay = ForcedA.bCompleted && ForcedB.bCompleted &&
		ForcedA.CrustStateHashAfter == ForcedB.CrustStateHashAfter &&
		ForcedA.StateHashAfter == ForcedB.StateHashAfter;
	return (bRunA && bRunB && bRunForcedA && bRunForcedB && bProjectionReplay && bStateReplay && bMaterialReplay && bCrustReplay && bVectorsZero && bStateBaseline && bMaterialBaseline && bForcedA && bForcedB && bForcedReplay) ? 0 : 2;
}
