// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIC4Commandlet.h"

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
	constexpr double SlabPullSpeedMmPerYear = 8.0;
	constexpr double ReferenceVelocityMmPerYear = 100.0;
	constexpr double EarthRadiusKm = 6371.0;
	constexpr double DeltaTimeMa = 2.0;
	constexpr double GateTolerance = 1.0e-10;
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

	FVector3d NormalizeOrFallback(const FVector3d& Vector, const FVector3d& Fallback)
	{
		const double Size = Vector.Size();
		return Size > UE_DOUBLE_SMALL_NUMBER ? Vector / Size : Fallback;
	}

	double AngularSpeedRadiansPerStep(const double VelocityMmPerYearValue)
	{
		return VelocityMmPerYearValue * DeltaTimeMa / EarthRadiusKm;
	}

	double VelocityMmPerYearFromAngularSpeed(const double AngularSpeed)
	{
		return FMath::Abs(AngularSpeed) * EarthRadiusKm / DeltaTimeMa;
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIC4"), Stamp);
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
				TEXT("phase-iii-slice-iiic4-report.md"));
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
		Actor->PhaseIIICSlabPullSpeedMmPerYear = SlabPullSpeedMmPerYear;
		Actor->PhaseIIICReferenceVelocityMmPerYear = ReferenceVelocityMmPerYear;
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	bool ConfigureMixedFixture(ACarrierLabVisualizationActor& Actor)
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

	struct FSlabPullReplayResult
	{
		FString Fixture;
		int32 Replay = 0;
		bool bCompleted = false;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIC1SubductingMarkAudit MarkAudit;
		FCarrierLabPhaseIIIC4SlabPullAudit SlabPullAudit;
		FCarrierLabPhaseIIIC4SlabPullAudit ExpectedSlabPullAudit;
		FCarrierLabPhaseIIIB7HashClosureAudit ClosureAudit;
		bool bIndependentOracleCaptured = false;
		bool bIndependentOracleCompared = false;
		bool bIndependentOracleHashesMatch = false;
		double OracleMaxAxisResidual = 0.0;
		double OracleMaxAngularResidual = 0.0;
		double OracleMaxContributionResidual = 0.0;
	};

	void CompareSlabPullOracle(const FCarrierLabPhaseIIIC4SlabPullAudit& Expected, FSlabPullReplayResult& Result)
	{
		Result.OracleMaxAxisResidual = 0.0;
		Result.OracleMaxAngularResidual = 0.0;
		Result.OracleMaxContributionResidual = 0.0;
		Result.bIndependentOracleCompared = Result.SlabPullAudit.bSlabPullEnabled && !Expected.SlabPullHash.IsEmpty();
		Result.bIndependentOracleHashesMatch =
			Result.bIndependentOracleCompared &&
			Expected.MotionHashBefore == Result.SlabPullAudit.MotionHashBefore &&
			Expected.MotionHashAfter == Result.SlabPullAudit.MotionHashAfter &&
			Expected.SlabPullHash == Result.SlabPullAudit.SlabPullHash;
		if (!Result.bIndependentOracleCompared)
		{
			return;
		}

		if (Expected.ContributionCount != Result.SlabPullAudit.ContributionCount ||
			Expected.AffectedPlateCount != Result.SlabPullAudit.AffectedPlateCount ||
			Expected.InvalidInputCount != Result.SlabPullAudit.InvalidInputCount)
		{
			Result.OracleMaxContributionResidual = FMath::Max(Result.OracleMaxContributionResidual, 1.0);
		}

		const int32 ContributionPairs = FMath::Min(Expected.Contributions.Num(), Result.SlabPullAudit.Contributions.Num());
		for (int32 Index = 0; Index < ContributionPairs; ++Index)
		{
			const FCarrierLabPhaseIIIC4SlabPullContributionRecord& A = Expected.Contributions[Index];
			const FCarrierLabPhaseIIIC4SlabPullContributionRecord& B = Result.SlabPullAudit.Contributions[Index];
			const bool bSameSource =
				A.MarkId == B.MarkId &&
				A.PlateId == B.PlateId &&
				A.OtherPlateId == B.OtherPlateId &&
				A.LocalTriangleId == B.LocalTriangleId;
			if (!bSameSource)
			{
				Result.OracleMaxContributionResidual = FMath::Max(Result.OracleMaxContributionResidual, 1.0);
			}
			Result.OracleMaxContributionResidual = FMath::Max(Result.OracleMaxContributionResidual, (A.PlateCenter - B.PlateCenter).Size());
			Result.OracleMaxContributionResidual = FMath::Max(Result.OracleMaxContributionResidual, (A.FrontBarycenter - B.FrontBarycenter).Size());
			Result.OracleMaxContributionResidual = FMath::Max(Result.OracleMaxContributionResidual, (A.ContributionUnit - B.ContributionUnit).Size());
			Result.OracleMaxContributionResidual = FMath::Max(Result.OracleMaxContributionResidual, FMath::Abs(A.SignedConvergenceVelocity - B.SignedConvergenceVelocity));
		}
		if (Expected.Contributions.Num() != Result.SlabPullAudit.Contributions.Num())
		{
			Result.OracleMaxContributionResidual = FMath::Max(Result.OracleMaxContributionResidual, 1.0);
		}

		TMap<int32, const FCarrierLabPhaseIIIC4SlabPullPlateRecord*> ActualByPlate;
		for (const FCarrierLabPhaseIIIC4SlabPullPlateRecord& Record : Result.SlabPullAudit.PlateRecords)
		{
			ActualByPlate.Add(Record.PlateId, &Record);
		}
		for (const FCarrierLabPhaseIIIC4SlabPullPlateRecord& ExpectedRecord : Expected.PlateRecords)
		{
			const FCarrierLabPhaseIIIC4SlabPullPlateRecord* ActualRecord = ActualByPlate.FindRef(ExpectedRecord.PlateId);
			if (ActualRecord == nullptr)
			{
				Result.OracleMaxAxisResidual = FMath::Max(Result.OracleMaxAxisResidual, 1.0);
				continue;
			}
			Result.OracleMaxContributionResidual = FMath::Max(Result.OracleMaxContributionResidual, (ExpectedRecord.ContributionSum - ActualRecord->ContributionSum).Size());
			Result.OracleMaxAxisResidual = FMath::Max(Result.OracleMaxAxisResidual, (ExpectedRecord.OldAxis - ActualRecord->OldAxis).Size());
			Result.OracleMaxAxisResidual = FMath::Max(Result.OracleMaxAxisResidual, (ExpectedRecord.NewAxis - ActualRecord->NewAxis).Size());
			Result.OracleMaxAngularResidual = FMath::Max(Result.OracleMaxAngularResidual, FMath::Abs(ExpectedRecord.OldAngularSpeedRadiansPerStep - ActualRecord->OldAngularSpeedRadiansPerStep));
			Result.OracleMaxAngularResidual = FMath::Max(Result.OracleMaxAngularResidual, FMath::Abs(ExpectedRecord.RawAngularSpeedRadiansPerStep - ActualRecord->RawAngularSpeedRadiansPerStep));
			Result.OracleMaxAngularResidual = FMath::Max(Result.OracleMaxAngularResidual, FMath::Abs(ExpectedRecord.NewAngularSpeedRadiansPerStep - ActualRecord->NewAngularSpeedRadiansPerStep));
			Result.OracleMaxAngularResidual = FMath::Max(Result.OracleMaxAngularResidual, FMath::Abs(ExpectedRecord.NewVelocityMmPerYear - ActualRecord->NewVelocityMmPerYear));
			if (ExpectedRecord.ContributionCount != ActualRecord->ContributionCount ||
				ExpectedRecord.bClampedToReferenceSpeed != ActualRecord->bClampedToReferenceSpeed)
			{
				Result.OracleMaxContributionResidual = FMath::Max(Result.OracleMaxContributionResidual, 1.0);
			}
		}
		if (Expected.PlateRecords.Num() != Result.SlabPullAudit.PlateRecords.Num())
		{
			Result.OracleMaxAxisResidual = FMath::Max(Result.OracleMaxAxisResidual, 1.0);
		}
	}

	bool RunSlabPullReplay(
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
		FSlabPullReplayResult& OutResult)
	{
		OutResult = FSlabPullReplayResult();
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
			Actor->GetPhaseIIIC4SlabPullAudit(OutResult.SlabPullAudit) &&
			Actor->GetPhaseIIIB7HashClosureAudit(OutResult.ClosureAudit);
		if (bAudits && bEnableMarks && !bEnableSlabPull)
		{
			OutResult.bIndependentOracleCaptured = Actor->BuildPhaseIIIC4SlabPullOracleFromCurrentState(OutResult.ExpectedSlabPullAudit);
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
			Result.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash;
	}

	bool SlabPullPrimaryPasses(const FSlabPullReplayResult& Result)
	{
		return Result.bCompleted &&
			Result.MarkAudit.MarkCount > 0 &&
			Result.SlabPullAudit.bSlabPullEnabled &&
			Result.SlabPullAudit.ContributionCount > 0 &&
			Result.SlabPullAudit.AffectedPlateCount > 0 &&
			Result.SlabPullAudit.MotionHashBefore != Result.SlabPullAudit.MotionHashAfter &&
			Result.SlabPullAudit.MaxVelocityMmPerYear <= ReferenceVelocityMmPerYear + 1.0e-6 &&
			Result.bIndependentOracleCompared &&
			Result.bIndependentOracleHashesMatch &&
			Result.OracleMaxAxisResidual <= GateTolerance &&
			Result.OracleMaxAngularResidual <= GateTolerance &&
			Result.OracleMaxContributionResidual <= GateTolerance &&
			Result.ClosureAudit.bMetricsHashMatchesComputed;
	}

	bool NegativePasses(const FSlabPullReplayResult& Result)
	{
		return Result.bCompleted &&
			Result.SlabPullAudit.ContributionCount == 0 &&
			Result.SlabPullAudit.AffectedPlateCount == 0 &&
			Result.SlabPullAudit.MotionHashBefore == Result.SlabPullAudit.MotionHashAfter &&
			Result.ClosureAudit.bMetricsHashMatchesComputed;
	}

	FString BypassJson(const FSlice55BypassResult& Result)
	{
		return FString::Printf(
			TEXT("{\"kind\":\"bypass\",\"replay\":%d,\"completed\":%s,\"state_hash\":%s,\"material_ledger_hash\":%s,\"persistent_mark_inputs\":%d,\"seconds\":%.6f}"),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			*JsonString(Result.FilterMetrics.StateHashAfter),
			*JsonString(Result.LedgerMetrics.MaterialLedgerHash),
			Result.FilterMetrics.PersistentSubductingMarkInputCount,
			Result.Seconds);
	}

	FString SlabPullJson(const FSlabPullReplayResult& Result)
	{
		return FString::Printf(
			TEXT("{\"kind\":\"slab_pull\",\"fixture\":%s,\"replay\":%d,\"completed\":%s,\"marks\":%d,\"contributions\":%d,\"affected_plates\":%d,\"invalid_inputs\":%d,\"max_velocity_mm_per_year\":%.12f,\"oracle_captured\":%s,\"oracle_compared\":%s,\"oracle_hashes_match\":%s,\"oracle_axis_residual\":%.15e,\"oracle_angular_residual\":%.15e,\"oracle_contribution_residual\":%.15e,\"motion_hash_before\":%s,\"motion_hash_after\":%s,\"slab_pull_hash\":%s,\"closure_hash\":%s,\"closure_matches\":%s,\"seconds\":%.6f}"),
			*JsonString(Result.Fixture),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.MarkAudit.MarkCount,
			Result.SlabPullAudit.ContributionCount,
			Result.SlabPullAudit.AffectedPlateCount,
			Result.SlabPullAudit.InvalidInputCount,
			Result.SlabPullAudit.MaxVelocityMmPerYear,
			Result.bIndependentOracleCaptured ? TEXT("true") : TEXT("false"),
			Result.bIndependentOracleCompared ? TEXT("true") : TEXT("false"),
			Result.bIndependentOracleHashesMatch ? TEXT("true") : TEXT("false"),
			Result.OracleMaxAxisResidual,
			Result.OracleMaxAngularResidual,
			Result.OracleMaxContributionResidual,
			*JsonString(Result.SlabPullAudit.MotionHashBefore),
			*JsonString(Result.SlabPullAudit.MotionHashAfter),
			*JsonString(Result.SlabPullAudit.SlabPullHash),
			*JsonString(Result.ClosureAudit.ComputedConvergenceTrackingHash),
			Result.ClosureAudit.bMetricsHashMatchesComputed ? TEXT("true") : TEXT("false"),
			Result.Seconds);
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FSlice55BypassResult& BypassA,
		const FSlice55BypassResult& BypassB,
		const FSlabPullReplayResult& PullA,
		const FSlabPullReplayResult& PullB,
		const FSlabPullReplayResult& Disabled,
		const FSlabPullReplayResult& ZeroMotion,
		const FSlabPullReplayResult& SinglePlate,
		const FSlabPullReplayResult& ForcedDivergenceNoSubduction)
	{
		const bool bBypassPass = BypassPasses(BypassA) && BypassPasses(BypassB);
		const bool bPrimaryPass =
			SlabPullPrimaryPasses(PullA) &&
			SlabPullPrimaryPasses(PullB) &&
			PullA.SlabPullAudit.SlabPullHash == PullB.SlabPullAudit.SlabPullHash &&
			PullA.SlabPullAudit.MotionHashAfter == PullB.SlabPullAudit.MotionHashAfter;
		const bool bDisabledPass =
			NegativePasses(Disabled) &&
			Disabled.MarkAudit.MarkCount > 0;
		const bool bOffOnDifferential =
			bDisabledPass &&
			PullA.SlabPullAudit.MotionHashBefore == Disabled.SlabPullAudit.MotionHashBefore &&
			PullA.SlabPullAudit.MotionHashAfter != Disabled.SlabPullAudit.MotionHashAfter;
		const bool bNegativePass =
			NegativePasses(ZeroMotion) &&
			NegativePasses(SinglePlate) &&
			NegativePasses(ForcedDivergenceNoSubduction);
		const bool bIIIBClosureContinuity =
			PullA.ClosureAudit.bMetricsHashMatchesComputed &&
			Disabled.ClosureAudit.bMetricsHashMatchesComputed &&
			ZeroMotion.ClosureAudit.bMetricsHashMatchesComputed;
		const bool bAllPass = bBypassPass && bPrimaryPass && bDisabledPass && bOffOnDifferential && bNegativePass;

		FString Report;
		Report += TEXT("# Phase III Slice IIIC.4 Checkpoint\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("Status: opt-in slab pull feedback. This slice mutates only plate motion authority from IIIC subducting-triangle marks. It does not add collision, rifting, erosion, terrain displacement, projection-derived ownership, repair, recovery, backfill, or any new resampling mutation path.\n\n");
		Report += TEXT("Formula in code units: `omega' = clamp_v0(omega + angular(vs) * Sum(normalize(c_i x q_k)))`, where `q_k` is each subducting-front triangle barycenter, `c_i` is the current plate center, `vs=8 mm/yr`, `v0=100 mm/yr`, and `angular(v)=v*dt/R` for `dt=2 Ma`, `R=6371 km`.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		Report += FString::Printf(
			TEXT("| Slice 5.5 bypass | %s | state `%s` / `%s`, ledger `%s` / `%s` |\n"),
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
			TEXT("| Slab pull opt-in on | %s | contributions %d / %d, affected plates %d / %d, motion `%s` -> `%s` |\n"),
			*PassFail(bPrimaryPass),
			PullA.SlabPullAudit.ContributionCount,
			PullB.SlabPullAudit.ContributionCount,
			PullA.SlabPullAudit.AffectedPlateCount,
			PullB.SlabPullAudit.AffectedPlateCount,
			*PullA.SlabPullAudit.MotionHashBefore,
			*PullA.SlabPullAudit.MotionHashAfter);
		Report += FString::Printf(
			TEXT("| Independent slab-pull oracle | %s | disabled mirror captured %s; hashes %s / %s; axis residual %.12e / %.12e, angular residual %.12e / %.12e, contribution residual %.12e / %.12e |\n"),
			*PassFail(bPrimaryPass),
			Disabled.bIndependentOracleCaptured ? TEXT("yes") : TEXT("no"),
			PullA.bIndependentOracleHashesMatch ? TEXT("match") : TEXT("mismatch"),
			PullB.bIndependentOracleHashesMatch ? TEXT("match") : TEXT("mismatch"),
			PullA.OracleMaxAxisResidual,
			PullB.OracleMaxAxisResidual,
			PullA.OracleMaxAngularResidual,
			PullB.OracleMaxAngularResidual,
			PullA.OracleMaxContributionResidual,
			PullB.OracleMaxContributionResidual);
		Report += FString::Printf(
			TEXT("| Bounded omega | %s | max velocity %.6f / %.6f mm/yr, v0 %.6f |\n"),
			*PassFail(bPrimaryPass),
			PullA.SlabPullAudit.MaxVelocityMmPerYear,
			PullB.SlabPullAudit.MaxVelocityMmPerYear,
			ReferenceVelocityMmPerYear);
		Report += FString::Printf(
			TEXT("| Off/on differential | %s | disabled `%s` -> `%s`, enabled after `%s` |\n"),
			*PassFail(bOffOnDifferential),
			*Disabled.SlabPullAudit.MotionHashBefore,
			*Disabled.SlabPullAudit.MotionHashAfter,
			*PullA.SlabPullAudit.MotionHashAfter);
		Report += FString::Printf(
			TEXT("| Negative controls | %s | zero %d, single %d, divergence-no-subduction %d contributions |\n"),
			*PassFail(bNegativePass),
			ZeroMotion.SlabPullAudit.ContributionCount,
			SinglePlate.SlabPullAudit.ContributionCount,
			ForcedDivergenceNoSubduction.SlabPullAudit.ContributionCount);

		Report += TEXT("\n## Primary Forced-Convergence Replay\n\n");
		Report += TEXT("| Replay | Marks | Contributions | Affected plates | Max velocity mm/yr | Axis residual | Angular residual | Contribution residual | Motion before | Motion after | Slab hash | Seconds |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|---:|\n");
		auto AddPullRow = [&Report](const FSlabPullReplayResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %d | %d | %d | %d | %.6f | %.12e | %.12e | %.12e | `%s` | `%s` | `%s` | %.3f |\n"),
				Result.Replay,
				Result.MarkAudit.MarkCount,
				Result.SlabPullAudit.ContributionCount,
				Result.SlabPullAudit.AffectedPlateCount,
				Result.SlabPullAudit.MaxVelocityMmPerYear,
				Result.OracleMaxAxisResidual,
				Result.OracleMaxAngularResidual,
				Result.OracleMaxContributionResidual,
				*Result.SlabPullAudit.MotionHashBefore,
				*Result.SlabPullAudit.MotionHashAfter,
				*Result.SlabPullAudit.SlabPullHash,
				Result.Seconds);
		};
		AddPullRow(PullA);
		AddPullRow(PullB);

		Report += TEXT("\n### Affected Plate Records\n\n");
		Report += TEXT("| Plate | Contributions | Old angular | Raw angular | New angular | New velocity | Clamped | Old axis | New axis | Contribution sum |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---|---|---|---|\n");
		for (const FCarrierLabPhaseIIIC4SlabPullPlateRecord& Record : PullA.SlabPullAudit.PlateRecords)
		{
			Report += FString::Printf(
				TEXT("| %d | %d | %.12e | %.12e | %.12e | %.6f | %s | `(%.6f, %.6f, %.6f)` | `(%.6f, %.6f, %.6f)` | `(%.6f, %.6f, %.6f)` |\n"),
				Record.PlateId,
				Record.ContributionCount,
				Record.OldAngularSpeedRadiansPerStep,
				Record.RawAngularSpeedRadiansPerStep,
				Record.NewAngularSpeedRadiansPerStep,
				Record.NewVelocityMmPerYear,
				Record.bClampedToReferenceSpeed ? TEXT("yes") : TEXT("no"),
				Record.OldAxis.X,
				Record.OldAxis.Y,
				Record.OldAxis.Z,
				Record.NewAxis.X,
				Record.NewAxis.Y,
				Record.NewAxis.Z,
				Record.ContributionSum.X,
				Record.ContributionSum.Y,
				Record.ContributionSum.Z);
		}

		Report += TEXT("\n### Representative Slab-Pull Contributions\n\n");
		Report += TEXT("| Mark | Plate | Other | Triangle | Signed velocity | Plate center | Front barycenter | Contribution unit |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---|---|---|\n");
		for (int32 Index = 0; Index < FMath::Min(16, PullA.SlabPullAudit.Contributions.Num()); ++Index)
		{
			const FCarrierLabPhaseIIIC4SlabPullContributionRecord& Record = PullA.SlabPullAudit.Contributions[Index];
			Report += FString::Printf(
				TEXT("| %d | %d | %d | %d | %.12f | `(%.6f, %.6f, %.6f)` | `(%.6f, %.6f, %.6f)` | `(%.6f, %.6f, %.6f)` |\n"),
				Record.MarkId,
				Record.PlateId,
				Record.OtherPlateId,
				Record.LocalTriangleId,
				Record.SignedConvergenceVelocity,
				Record.PlateCenter.X,
				Record.PlateCenter.Y,
				Record.PlateCenter.Z,
				Record.FrontBarycenter.X,
				Record.FrontBarycenter.Y,
				Record.FrontBarycenter.Z,
				Record.ContributionUnit.X,
				Record.ContributionUnit.Y,
				Record.ContributionUnit.Z);
		}

		Report += TEXT("\n## Negative And Off-State Controls\n\n");
		Report += TEXT("| Fixture | Marks | Contributions | Motion before | Motion after | Closure matches | Result |\n");
		Report += TEXT("|---|---:|---:|---|---|---|---|\n");
		auto AddNegativeRow = [&Report](const FSlabPullReplayResult& Result, const bool bPass)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | `%s` | `%s` | %s | %s |\n"),
				*Result.Fixture,
				Result.MarkAudit.MarkCount,
				Result.SlabPullAudit.ContributionCount,
				*Result.SlabPullAudit.MotionHashBefore,
				*Result.SlabPullAudit.MotionHashAfter,
				Result.ClosureAudit.bMetricsHashMatchesComputed ? TEXT("yes") : TEXT("no"),
				*PassFail(bPass));
		};
		AddNegativeRow(Disabled, bDisabledPass);
		AddNegativeRow(ZeroMotion, NegativePasses(ZeroMotion));
		AddNegativeRow(SinglePlate, NegativePasses(SinglePlate));
		AddNegativeRow(ForcedDivergenceNoSubduction, NegativePasses(ForcedDivergenceNoSubduction));

		Report += TEXT("\n## Scope Notes\n\n");
		Report += TEXT("- Slab pull is opt-in and defaults off on the actor. The commandlet enables it only for the primary on-state fixture.\n");
		Report += TEXT("- The off-state fixture has IIIC marks present but slab pull disabled, so marks alone cannot mutate motion authority.\n");
		Report += TEXT("- The independent oracle is computed from a slab-pull-disabled mirror run after the same marks are produced, using the mirror plate vertices, motion state, marks, and configured constants. It does not recompute expected values from the enabled run's slab-pull contribution records.\n");
		Report += TEXT("- Direct thesis inspection for this hardening tranche found that the source says slab pull updates both axis and speed of `G_i`; this slice therefore keeps the existing axis+speed omega update instead of changing it to an axis-only variant.\n");
		Report += TEXT("- This checkpoint may claim only IIIC.4 slab-pull feedback behavior. It does not claim Stage 1.5 carrier success, Slice 5.5 asymmetry resolution, collision, rifting, erosion, or terrain morphology.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIC.4 passes. Pause for user review before IIIC consolidation or IIID planning.\n")
			: TEXT("IIIC.4 does not pass. Investigate the failed gate before any follow-on Phase III work.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIC4Commandlet::UCarrierLabPhaseIIIC4Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIC4Commandlet::Main(const FString& Params)
{
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FSlice55BypassResult BypassA;
	FSlice55BypassResult BypassB;
	const bool bBypassA = RunSlice55Bypass(0, BypassA);
	const bool bBypassB = RunSlice55Bypass(1, BypassB);

	FSlabPullReplayResult PullA;
	FSlabPullReplayResult PullB;
	const bool bPullA = RunSlabPullReplay(
		TEXT("forced_convergence_slab_pull"),
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
		PullA);
	const bool bPullB = RunSlabPullReplay(
		TEXT("forced_convergence_slab_pull"),
		1,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		false,
		false,
		true,
		PullB);

	FSlabPullReplayResult Disabled;
	const bool bDisabled = RunSlabPullReplay(
		TEXT("slab_pull_disabled"),
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

	FSlabPullReplayResult ZeroMotion;
	const bool bZero = RunSlabPullReplay(
		TEXT("zero_motion"),
		0,
		ECarrierLabPhaseIIMotionFixture::Zero,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		false,
		false,
		true,
		ZeroMotion);
	FSlabPullReplayResult SinglePlate;
	const bool bSingle = RunSlabPullReplay(
		TEXT("single_plate"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		1,
		1.0,
		false,
		true,
		false,
		false,
		true,
		SinglePlate);
	FSlabPullReplayResult ForcedDivergence;
	const bool bDivergence = RunSlabPullReplay(
		TEXT("forced_divergence_no_subduction"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedDivergence,
		FixtureSamples,
		FixturePlates,
		1.0,
		false,
		true,
		false,
		false,
		true,
		ForcedDivergence);

	if (Disabled.bIndependentOracleCaptured)
	{
		CompareSlabPullOracle(Disabled.ExpectedSlabPullAudit, PullA);
		CompareSlabPullOracle(Disabled.ExpectedSlabPullAudit, PullB);
	}

	TArray<FString> JsonLines;
	JsonLines.Add(BypassJson(BypassA));
	JsonLines.Add(BypassJson(BypassB));
	JsonLines.Add(SlabPullJson(PullA));
	JsonLines.Add(SlabPullJson(PullB));
	JsonLines.Add(SlabPullJson(Disabled));
	JsonLines.Add(SlabPullJson(ZeroMotion));
	JsonLines.Add(SlabPullJson(SinglePlate));
	JsonLines.Add(SlabPullJson(ForcedDivergence));

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(FString::Join(JsonLines, TEXT("\n")) + TEXT("\n"), *MetricsPath);

	const FString Report = BuildReport(OutputRoot, BypassA, BypassB, PullA, PullB, Disabled, ZeroMotion, SinglePlate, ForcedDivergence);
	const FString ReportPath = ResolveReportPath(Params);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	const bool bBypassPass = bBypassA && bBypassB && BypassPasses(BypassA) && BypassPasses(BypassB);
	const bool bPrimaryPass =
		bPullA &&
		bPullB &&
		SlabPullPrimaryPasses(PullA) &&
		SlabPullPrimaryPasses(PullB) &&
		PullA.SlabPullAudit.SlabPullHash == PullB.SlabPullAudit.SlabPullHash &&
		PullA.SlabPullAudit.MotionHashAfter == PullB.SlabPullAudit.MotionHashAfter;
	const bool bDisabledPass = bDisabled && NegativePasses(Disabled) && Disabled.MarkAudit.MarkCount > 0;
	const bool bOffOnDifferential =
		bDisabledPass &&
		PullA.SlabPullAudit.MotionHashBefore == Disabled.SlabPullAudit.MotionHashBefore &&
		PullA.SlabPullAudit.MotionHashAfter != Disabled.SlabPullAudit.MotionHashAfter;
	const bool bNegativePass =
		bZero &&
		bSingle &&
		bDivergence &&
		NegativePasses(ZeroMotion) &&
		NegativePasses(SinglePlate) &&
		NegativePasses(ForcedDivergence);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIIC.4 report: %s"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIIC.4 metrics: %s"), *MetricsPath);
	return (bBypassPass && bPrimaryPass && bDisabledPass && bOffOnDifferential && bNegativePass) ? 0 : 1;
}
