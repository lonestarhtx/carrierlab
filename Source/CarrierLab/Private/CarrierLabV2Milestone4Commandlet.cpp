// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabV2Milestone4Commandlet.h"

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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("Milestone4"), Stamp);
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
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("docs"), TEXT("checkpoints"), TEXT("milestone-4-closeout-report.md"));
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

	bool IsPinnedFixture(const FString& FixtureId)
	{
		return FixtureId == TEXT("M4-FX-001");
	}

	bool IsPaperRegimeFixture(const FString& FixtureId)
	{
		return FixtureId == TEXT("M4-FX-014");
	}

	bool IsMicroFixture(const FString& FixtureId)
	{
		return FixtureId.StartsWith(TEXT("M4-FX-")) && !IsPaperRegimeFixture(FixtureId);
	}

	bool IsScaleFixture(const FString& FixtureId, const int32 SampleCount)
	{
		return FixtureId == FString::Printf(TEXT("SCALE-%dK-M4-FIELDS"), SampleCount / 1000);
	}
}

UCarrierLabV2Milestone4Commandlet::UCarrierLabV2Milestone4Commandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarrierLabV2Milestone4Commandlet::Main(const FString& Params)
{
	const bool bMicroOnly = Params.Contains(TEXT("-MicroOnly"), ESearchCase::IgnoreCase);
	const bool bSkipPaperRegime = Params.Contains(TEXT("-SkipPaperRegime"), ESearchCase::IgnoreCase);
	const bool bSkip250k = Params.Contains(TEXT("-Skip250k"), ESearchCase::IgnoreCase);
	const bool bRun100k = Params.Contains(TEXT("-Run100k"), ESearchCase::IgnoreCase);
	const bool bRun500k = Params.Contains(TEXT("-Run500k"), ESearchCase::IgnoreCase);

	const FString OutputRoot = GetOutputRoot(Params);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	const FString RuntimeReportPath = FPaths::Combine(OutputRoot, TEXT("milestone-4-closeout-report.md"));
	const FString CheckpointReportPath = ResolveReportPath(Params);

	CarrierLab::V2::FCarrierV2Milestone4SuiteResult Suite;
	Suite.OutputRoot = OutputRoot;
	Suite.MetricsPath = MetricsPath;
	Suite.ReportPath = CheckpointReportPath;
	Suite.NotAttempted100kReason = bRun100k ? TEXT("") : TEXT("not requested; optional interpolation scale");
	Suite.NotAttempted500kReason = bRun500k ? TEXT("") : TEXT("not requested; optional stretch characterization only");

	TArray<CarrierLab::V2::FCarrierV2Milestone4Config> Configs = CarrierLab::V2::FCarrierV2Milestone4::MakeMicroFixtureConfigs();
	if (!bMicroOnly)
	{
		if (!bSkipPaperRegime)
		{
			Configs.Add(CarrierLab::V2::FCarrierV2Milestone4::MakePaperRegimeCharacterizationConfig());
		}
		Configs.Add(CarrierLab::V2::FCarrierV2Milestone4::MakeScaleConfig(50000, 2, false));
		if (bRun100k)
		{
			Configs.Add(CarrierLab::V2::FCarrierV2Milestone4::MakeScaleConfig(100000, 1, false));
		}
		if (!bSkip250k)
		{
			Configs.Add(CarrierLab::V2::FCarrierV2Milestone4::MakeScaleConfig(250000, 1, true));
		}
		if (bRun500k)
		{
			Configs.Add(CarrierLab::V2::FCarrierV2Milestone4::MakeScaleConfig(500000, 1, true));
		}
	}

	FString MetricsJsonl;
	bool bAllMicroPassed = true;
	bool bPinnedPassed = false;
	bool bPaperRegimePassed = bMicroOnly;
	bool bScale50kPassed = bMicroOnly;

	for (const CarrierLab::V2::FCarrierV2Milestone4Config& Config : Configs)
	{
		UE_LOG(LogTemp, Display, TEXT("CarrierLab M4: running %s (%d samples, %d plates, windows=%d, steps=%d, fields=%s)."),
			*Config.FixtureId,
			Config.ProcessConfig.CarrierCycleConfig.MotionConfig.BaseConfig.SampleCount,
			Config.ProcessConfig.CarrierCycleConfig.MotionConfig.BaseConfig.PlateCount,
			Config.ProcessConfig.CarrierCycleConfig.LifecycleWindowCount,
			Config.ProcessConfig.CarrierCycleConfig.MotionConfig.MotionStepCount,
			Config.bEnableFieldStorage ? TEXT("true") : TEXT("false"));

		CarrierLab::V2::FCarrierV2Milestone4FixtureResult Result;
		CarrierLab::V2::FCarrierV2Milestone4::RunFixtureWithReplay(Config, Result);
		Suite.Results.Add(Result);
		MetricsJsonl += CarrierLab::V2::FCarrierV2Milestone4::MetricsToJson(Result);
		MetricsJsonl += TEXT("\n");
		if (!Result.Error.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("CarrierLab M4: %s diagnostics: %s"), *Config.FixtureId, *Result.Error);
		}

		if (IsPinnedFixture(Config.FixtureId))
		{
			bPinnedPassed = Result.Metrics.bFixturePass;
		}
		if (IsMicroFixture(Config.FixtureId))
		{
			bAllMicroPassed = bAllMicroPassed && Result.Metrics.bFixturePass;
		}
		else if (IsPaperRegimeFixture(Config.FixtureId))
		{
			bPaperRegimePassed = Result.Metrics.bFixturePass;
		}
		else if (IsScaleFixture(Config.FixtureId, 50000))
		{
			bScale50kPassed = Result.Metrics.bFixturePass;
		}
		else if (IsScaleFixture(Config.FixtureId, 100000))
		{
			Suite.bAttempted100k = true;
			Suite.bScale100kPass = Result.Metrics.bFixturePass;
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
			TEXT("CarrierLab M4: %s verdict=%s pass=%s replay=%s fields=%d single/q1q2/defer=%d/%d/%d o/o older/younger/equal=%d/%d/%d contacts=%d/%d fronts=%d continuity=%d/%d dangerous=%d hole_growth=%d unassigned=%d/%d cycle_ms=%.3f total_ms=%.3f."),
			*Result.Metrics.FixtureId,
			*Result.Metrics.Verdict,
			Result.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Result.Metrics.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			Result.Metrics.FieldVertexCount,
			Result.Metrics.FieldTransferSingleSourceCount,
			Result.Metrics.FieldTransferQ1Q2Count,
			Result.Metrics.FieldTransferDeferredCount,
			Result.Metrics.OceanOceanOlderSubductingLabelCount,
			Result.Metrics.OceanOceanYoungerSubductingLabelCount,
			Result.Metrics.OceanOceanEqualAgeDeferralCount,
			Result.Metrics.ConvergentContactCount,
			Result.Metrics.DivergentContactCount,
			Result.Metrics.ActiveFrontCount,
			Result.Metrics.FrontContinuityMatchedCount,
			Result.Metrics.FrontContinuityCandidateCount,
			Result.Metrics.DangerousNonOpeningQ1Q2OceanicCount,
			Result.Metrics.HoleCountGrowth,
			Result.Metrics.UnassignedTriangleCount,
			Result.Metrics.UnassignedTriangleBudget,
			Result.Metrics.FullCarrierCycleMs,
			Result.Metrics.TotalMs);
	}

	Suite.bPinnedM3BaselinePass = bPinnedPassed;
	Suite.bMicroGatePass = bAllMicroPassed;
	Suite.bPaperRegimeCharacterizationPass = bPaperRegimePassed;
	Suite.bScale50kPass = bScale50kPassed;
	Suite.bStageGatePass =
		Suite.bPinnedM3BaselinePass &&
		Suite.bMicroGatePass &&
		Suite.bPaperRegimeCharacterizationPass &&
		Suite.bScale50kPass &&
		Suite.bAttempted250k &&
		Suite.bScale250kPass;
	if (bMicroOnly)
	{
		Suite.bStageGatePass = Suite.bPinnedM3BaselinePass && Suite.bMicroGatePass;
	}
	Suite.Verdict = Suite.bStageGatePass ? TEXT("MILESTONE_4_PASS") : TEXT("REVISE_MILESTONE_4");

	if (!SaveTextFile(MetricsPath, MetricsJsonl))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab M4: failed to write metrics JSONL: %s"), *MetricsPath);
		return 1;
	}

	const FString CommitSha = GetGitHead();
	const FString Report = CarrierLab::V2::FCarrierV2Milestone4::BuildCheckpointReport(Suite, Params, CommitSha);
	if (!SaveTextFile(RuntimeReportPath, Report))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab M4: failed to write runtime report: %s"), *RuntimeReportPath);
		return 1;
	}
	if (!SaveTextFile(CheckpointReportPath, Report))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab M4: failed to write checkpoint report: %s"), *CheckpointReportPath);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab M4 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab M4 runtime report: %s"), *RuntimeReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab M4 checkpoint report: %s"), *CheckpointReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab M4 final verdict: %s"), *Suite.Verdict);
	return Suite.bStageGatePass ? 0 : 2;
}
