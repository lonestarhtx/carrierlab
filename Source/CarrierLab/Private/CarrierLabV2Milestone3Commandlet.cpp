// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabV2Milestone3Commandlet.h"

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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("Milestone3"), Stamp);
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
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("docs"), TEXT("checkpoints"), TEXT("milestone-3-closeout-report.md"));
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
		return FixtureId.StartsWith(TEXT("M3-FX-"));
	}

	bool IsPinnedFixture(const FString& FixtureId)
	{
		return FixtureId.Contains(TEXT("PinnedM2Baseline"));
	}

	bool IsScaleFixture(const FString& FixtureId, const int32 SampleCount)
	{
		return FixtureId == FString::Printf(TEXT("SCALE-%dK-M3-FILTERS"), SampleCount / 1000);
	}
}

UCarrierLabV2Milestone3Commandlet::UCarrierLabV2Milestone3Commandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarrierLabV2Milestone3Commandlet::Main(const FString& Params)
{
	const bool bMicroOnly = Params.Contains(TEXT("-MicroOnly"), ESearchCase::IgnoreCase);
	const bool bSkip250k = Params.Contains(TEXT("-Skip250k"), ESearchCase::IgnoreCase);
	const bool bRun500k = Params.Contains(TEXT("-Run500k"), ESearchCase::IgnoreCase);

	const FString OutputRoot = GetOutputRoot(Params);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	const FString RuntimeReportPath = FPaths::Combine(OutputRoot, TEXT("milestone-3-closeout-report.md"));
	const FString CheckpointReportPath = ResolveReportPath(Params);

	CarrierLab::V2::FCarrierV2Milestone3SuiteResult Suite;
	Suite.OutputRoot = OutputRoot;
	Suite.MetricsPath = MetricsPath;
	Suite.ReportPath = CheckpointReportPath;
	Suite.NotAttempted500kReason = bRun500k ? TEXT("") : TEXT("not requested; optional characterization only");

	TArray<CarrierLab::V2::FCarrierV2Milestone3Config> Configs = CarrierLab::V2::FCarrierV2Milestone3::MakeMicroFixtureConfigs();
	if (!bMicroOnly)
	{
		Configs.Add(CarrierLab::V2::FCarrierV2Milestone3::MakeScaleConfig(50000, false));
		if (!bSkip250k)
		{
			Configs.Add(CarrierLab::V2::FCarrierV2Milestone3::MakeScaleConfig(250000, true));
		}
		if (bRun500k)
		{
			Configs.Add(CarrierLab::V2::FCarrierV2Milestone3::MakeScaleConfig(500000, true));
		}
	}

	FString MetricsJsonl;
	bool bAllMicroPassed = true;
	bool bPinnedPassed = false;
	bool bScale50kPassed = bMicroOnly;

	for (const CarrierLab::V2::FCarrierV2Milestone3Config& Config : Configs)
	{
		UE_LOG(LogTemp, Display, TEXT("CarrierLab M3: running %s (%d samples, %d plates, windows=%d, polarity=%s)."),
			*Config.FixtureId,
			Config.CarrierCycleConfig.MotionConfig.BaseConfig.SampleCount,
			Config.CarrierCycleConfig.MotionConfig.BaseConfig.PlateCount,
			Config.CarrierCycleConfig.LifecycleWindowCount,
			*Config.PolarityMode);

		CarrierLab::V2::FCarrierV2Milestone3FixtureResult Result;
		CarrierLab::V2::FCarrierV2Milestone3::RunFixtureWithReplay(Config, Result);
		Suite.Results.Add(Result);
		MetricsJsonl += CarrierLab::V2::FCarrierV2Milestone3::MetricsToJson(Result);
		MetricsJsonl += TEXT("\n");

		if (IsPinnedFixture(Config.FixtureId))
		{
			bPinnedPassed = Result.Metrics.bFixturePass;
		}
		if (IsMicroFixture(Config.FixtureId))
		{
			bAllMicroPassed = bAllMicroPassed && Result.Metrics.bFixturePass;
		}
		else if (IsScaleFixture(Config.FixtureId, 50000))
		{
			bScale50kPassed = Result.Metrics.bFixturePass;
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
			TEXT("CarrierLab M3: %s verdict=%s pass=%s replay=%s contacts=%d labels=%d filtered_single=%d exhausted=%d post_multi=%d q1q2_accept/reject=%d/%d hole_growth=%d prior_blocked_to_oceanic=%d unassigned=%d/%d process_ms=%.3f full_cycle_ms=%.3f total_ms=%.3f."),
			*Result.Metrics.FixtureId,
			*Result.Metrics.Verdict,
			Result.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Result.Metrics.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			Result.Metrics.ContactEvidenceCount,
			Result.Metrics.FilterActiveTriangleLabelCount,
			Result.Metrics.FilteredSingleSourceWriteCount,
			Result.Metrics.FilterExhaustedSampleCount,
			Result.Metrics.PostFilterUnresolvedMultihitCount,
			Result.Metrics.Q1Q2DivergentAcceptedCount,
			Result.Metrics.Q1Q2DivergenceRejectedCount,
			Result.Metrics.HoleCountGrowth,
			Result.Metrics.PreviouslyBlockedBecameQ1Q2OceanicCount,
			Result.Metrics.UnassignedTriangleCount,
			Result.Metrics.UnassignedTriangleBudget,
			Result.Metrics.ProcessLaneMs,
			Result.Metrics.FullCarrierCycleMs,
			Result.Metrics.TotalMs);
	}

	Suite.bPinnedM2BaselinePass = bPinnedPassed;
	Suite.bMicroGatePass = bAllMicroPassed;
	Suite.bScale50kPass = bScale50kPassed;
	Suite.bStageGatePass =
		Suite.bPinnedM2BaselinePass &&
		Suite.bMicroGatePass &&
		Suite.bScale50kPass &&
		Suite.bAttempted250k &&
		Suite.bScale250kPass;
	Suite.Verdict = Suite.bStageGatePass ? TEXT("MILESTONE_3_PASS") : TEXT("REVISE_MILESTONE_3");

	if (!SaveTextFile(MetricsPath, MetricsJsonl))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab M3: failed to write metrics JSONL: %s"), *MetricsPath);
		return 1;
	}

	const FString CommitSha = GetGitHead();
	const FString Report = CarrierLab::V2::FCarrierV2Milestone3::BuildCheckpointReport(Suite, Params, CommitSha);
	if (!SaveTextFile(RuntimeReportPath, Report))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab M3: failed to write runtime report: %s"), *RuntimeReportPath);
		return 1;
	}
	if (!SaveTextFile(CheckpointReportPath, Report))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab M3: failed to write checkpoint report: %s"), *CheckpointReportPath);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab M3 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab M3 runtime report: %s"), *RuntimeReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab M3 checkpoint report: %s"), *CheckpointReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab M3 final verdict: %s"), *Suite.Verdict);
	return Suite.bStageGatePass ? 0 : 2;
}
