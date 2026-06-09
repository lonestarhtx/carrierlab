// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabV2Milestone2Commandlet.h"

#include "CarrierLabV2Core.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("Milestone2"), Stamp);
		}
		else if (FPaths::IsRelative(OutputRoot))
		{
			OutputRoot = FPaths::Combine(FPaths::ProjectDir(), OutputRoot);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	FString ResolveReportPath(const FString& Params)
	{
		FString ReportPath;
		if (!FParse::Value(*Params, TEXT("Report="), ReportPath))
		{
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("docs"), TEXT("checkpoints"), TEXT("milestone-2-closeout-report.md"));
		}
		else if (FPaths::IsRelative(ReportPath))
		{
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), ReportPath);
		}
		return FPaths::ConvertRelativePathToFull(ReportPath);
	}

	FString GetGitHead()
	{
		FString StdOut;
		FString StdErr;
		int32 ReturnCode = 0;
		const FString Args = FString::Printf(TEXT("-C \"%s\" rev-parse HEAD"), *FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
		if (FPlatformProcess::ExecProcess(TEXT("git"), *Args, &ReturnCode, &StdOut, &StdErr) && ReturnCode == 0)
		{
			StdOut.TrimStartAndEndInline();
			return StdOut;
		}
		return TEXT("unknown");
	}

	bool SaveTextFile(const FString& Path, const FString& Contents)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
		return FFileHelper::SaveStringToFile(Contents, *Path);
	}

	bool IsMicroFixture(const FString& FixtureId)
	{
		return FixtureId.StartsWith(TEXT("M2-FX-"));
	}

	bool IsScaleFixture(const FString& FixtureId, const int32 SampleCount)
	{
		return FixtureId == FString::Printf(TEXT("SCALE-%dK-M2"), SampleCount / 1000);
	}
}

UCarrierLabV2Milestone2Commandlet::UCarrierLabV2Milestone2Commandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarrierLabV2Milestone2Commandlet::Main(const FString& Params)
{
	const bool bMicroOnly = Params.Contains(TEXT("-MicroOnly"), ESearchCase::IgnoreCase);
	const bool bSkip100k = Params.Contains(TEXT("-Skip100k"), ESearchCase::IgnoreCase);
	const bool bSkip250k = Params.Contains(TEXT("-Skip250k"), ESearchCase::IgnoreCase);
	const bool bRun500k = Params.Contains(TEXT("-Run500k"), ESearchCase::IgnoreCase);

	const FString OutputRoot = GetOutputRoot(Params);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	const FString RuntimeReportPath = FPaths::Combine(OutputRoot, TEXT("milestone-2-closeout-report.md"));
	const FString CheckpointReportPath = ResolveReportPath(Params);

	CarrierLab::V2::FCarrierV2Milestone2SuiteResult Suite;
	Suite.OutputRoot = OutputRoot;
	Suite.MetricsPath = MetricsPath;
	Suite.ReportPath = CheckpointReportPath;
	Suite.NotAttempted500kReason = bRun500k ? TEXT("") : TEXT("not requested; optional characterization only");

	TArray<CarrierLab::V2::FCarrierV2Milestone2Config> Configs = CarrierLab::V2::FCarrierV2Milestone2::MakeMicroFixtureConfigs();
	if (!bMicroOnly)
	{
		Configs.Add(CarrierLab::V2::FCarrierV2Milestone2::MakeScaleConfig(50000, false));
		if (!bSkip100k)
		{
			Configs.Add(CarrierLab::V2::FCarrierV2Milestone2::MakeScaleConfig(100000, true));
		}
		if (!bSkip250k)
		{
			Configs.Add(CarrierLab::V2::FCarrierV2Milestone2::MakeScaleConfig(250000, true));
		}
		if (bRun500k)
		{
			Configs.Add(CarrierLab::V2::FCarrierV2Milestone2::MakeScaleConfig(500000, true));
		}
	}

	FString MetricsJsonl;
	bool bAllMicroPassed = true;
	bool bScale50kPassed = bMicroOnly;
	bool bScale100kReported = bMicroOnly;

	for (const CarrierLab::V2::FCarrierV2Milestone2Config& Config : Configs)
	{
		UE_LOG(LogTemp, Display, TEXT("CarrierLab M2: running %s (%d samples, %d plates, windows=%d, policy=%s)."),
			*Config.FixtureId,
			Config.MotionConfig.BaseConfig.SampleCount,
			Config.MotionConfig.BaseConfig.PlateCount,
			Config.LifecycleWindowCount,
			*Config.ResamplePolicyId);

		CarrierLab::V2::FCarrierV2Milestone2FixtureResult Result;
		CarrierLab::V2::FCarrierV2Milestone2::RunFixtureWithReplay(Config, Result);
		Suite.Results.Add(Result);
		MetricsJsonl += CarrierLab::V2::FCarrierV2Milestone2::MetricsToJson(Result);
		MetricsJsonl += TEXT("\n");

		if (IsMicroFixture(Config.FixtureId))
		{
			bAllMicroPassed = bAllMicroPassed && Result.Metrics.bFixturePass;
		}
		else if (IsScaleFixture(Config.FixtureId, 50000))
		{
			bScale50kPassed = Result.Metrics.bFixturePass;
		}
		else if (IsScaleFixture(Config.FixtureId, 100000))
		{
			bScale100kReported = true;
		}
		else if (IsScaleFixture(Config.FixtureId, 250000))
		{
			Suite.bAttempted250k = true;
			Suite.bScale250kPass = Result.Metrics.bFixturePass;
		}
		else if (IsScaleFixture(Config.FixtureId, 500000))
		{
			Suite.bAttempted500k = true;
			Suite.bScale500kPass = Result.Metrics.bFixturePass;
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT("CarrierLab M2: %s verdict=%s pass=%s replay=%s single=%d gaps=%d q1q2=%d overlap_blocked=%d unsupported_overlap_writes=%d owner_reads=%d resolvers(c/r/n)=%d/%d/%d rebuilt=%d/%d material_delta=%.12f tv_delta=%.12f broadphase_mismatch=%d step_ms=%.3f total_ms=%.3f."),
			*Result.Metrics.FixtureId,
			*Result.Metrics.Verdict,
			Result.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Result.Metrics.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			Result.Metrics.ValidSingleHitWriteCount,
			Result.Metrics.DivergentZeroHitCount,
			Result.Metrics.Q1Q2BoundaryPairCount,
			Result.Metrics.NondegenerateOverlapBlockedCount,
			Result.Metrics.UnsupportedOverlapWriteAttemptCount,
			Result.Metrics.PriorOwnerReadCount + Result.Metrics.GlobalOwnerReadCount,
			Result.Metrics.CentroidPrimaryResolutionCount,
			Result.Metrics.RandomPrimaryResolutionCount,
			Result.Metrics.NearestPrimaryResolutionCount,
			Result.Metrics.RebuiltTriangleAssignmentCount,
			Result.Metrics.GlobalTriangleCount,
			Result.Metrics.MaterialConservationDelta,
			Result.Metrics.TotalVariationDelta,
			Result.Metrics.BroadphaseEquivalenceMismatchCount,
			Result.Metrics.StepKernelMs,
			Result.Metrics.TotalMs);
	}

	Suite.bMicroGatePass = bAllMicroPassed;
	Suite.bScale50kPass = bScale50kPassed;
	Suite.bScale100kReported = bScale100kReported;
	Suite.bStageGatePass =
		Suite.bMicroGatePass &&
		Suite.bScale50kPass &&
		Suite.bScale100kReported &&
		Suite.bAttempted250k &&
		Suite.bScale250kPass;
	Suite.Verdict = Suite.bStageGatePass ? TEXT("MILESTONE_2_PASS") : TEXT("REVISE_MILESTONE_2");

	if (!SaveTextFile(MetricsPath, MetricsJsonl))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab M2: failed to write metrics JSONL: %s"), *MetricsPath);
		return 1;
	}

	const FString CommitSha = GetGitHead();
	const FString Report = CarrierLab::V2::FCarrierV2Milestone2::BuildCheckpointReport(Suite, Params, CommitSha);
	if (!SaveTextFile(RuntimeReportPath, Report))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab M2: failed to write runtime report: %s"), *RuntimeReportPath);
		return 1;
	}
	if (!SaveTextFile(CheckpointReportPath, Report))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab M2: failed to write checkpoint report: %s"), *CheckpointReportPath);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab M2 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab M2 runtime report: %s"), *RuntimeReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab M2 checkpoint report: %s"), *CheckpointReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab M2 final verdict: %s"), *Suite.Verdict);
	return Suite.bStageGatePass ? 0 : 2;
}
