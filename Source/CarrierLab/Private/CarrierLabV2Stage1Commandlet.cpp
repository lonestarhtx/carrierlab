// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabV2Stage1Commandlet.h"

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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("V2"), TEXT("Stage1"), Stamp);
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
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("docs"), TEXT("checkpoints"), TEXT("milestone-1-closeout-report.md"));
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
		return FixtureId.StartsWith(TEXT("FX-"));
	}
}

UCarrierLabV2Stage1Commandlet::UCarrierLabV2Stage1Commandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarrierLabV2Stage1Commandlet::Main(const FString& Params)
{
	const bool bMicroOnly = Params.Contains(TEXT("-MicroOnly"), ESearchCase::IgnoreCase);
	const bool bRun100k = !bMicroOnly && Params.Contains(TEXT("-Run100k"), ESearchCase::IgnoreCase);
	const bool bRun250k = !bMicroOnly && !Params.Contains(TEXT("-Skip250k"), ESearchCase::IgnoreCase);
	const bool bRun500k = !bMicroOnly && Params.Contains(TEXT("-Run500k"), ESearchCase::IgnoreCase);
	int32 ScaleSamples = 50000;
	FParse::Value(*Params, TEXT("ScaleSamples="), ScaleSamples);
	ScaleSamples = FMath::Max(4, ScaleSamples);

	const FString OutputRoot = GetOutputRoot(Params);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	const FString RuntimeReportPath = FPaths::Combine(OutputRoot, TEXT("v2-stage-1-report.md"));
	const FString CheckpointReportPath = ResolveReportPath(Params);

	CarrierLab::V2::FCarrierV2Stage1SuiteResult Suite;
	Suite.OutputRoot = OutputRoot;
	Suite.MetricsPath = MetricsPath;
	Suite.ReportPath = CheckpointReportPath;

	TArray<CarrierLab::V2::FCarrierV2Stage1Config> Configs = CarrierLab::V2::FCarrierV2Stage1::MakeMicroFixtureConfigs();
	if (!bMicroOnly)
	{
		Configs.Add(CarrierLab::V2::FCarrierV2Stage1::MakeScaleConfig(ScaleSamples, false));
	}

	FString MetricsJsonl;
	bool bAllMicroPassed = true;
	bool bScale50kPassed = bMicroOnly;
	bool bScale100kPassed = false;
	bool bShouldAttempt250k = false;

	for (const CarrierLab::V2::FCarrierV2Stage1Config& Config : Configs)
	{
		UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-1: running %s (%d samples, %d plates, policy=%s)."),
			*Config.FixtureId,
			Config.BaseConfig.SampleCount,
			Config.BaseConfig.PlateCount,
			*Config.ProjectionCandidatePolicyId);

		CarrierLab::V2::FCarrierV2Stage1FixtureResult Result;
		CarrierLab::V2::FCarrierV2Stage1::RunFixtureWithReplay(Config, Result);
		Suite.Results.Add(Result);
		MetricsJsonl += CarrierLab::V2::FCarrierV2Stage1::MetricsToJson(Result);
		MetricsJsonl += TEXT("\n");

		if (IsMicroFixture(Config.FixtureId))
		{
			bAllMicroPassed = bAllMicroPassed && Result.Metrics.bFixturePass;
		}
		else if (Config.FixtureId == TEXT("SCALE-50K-MOTION"))
		{
			bScale50kPassed = Result.Metrics.bFixturePass;
			bShouldAttempt250k = bScale50kPassed && bRun250k;
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT("CarrierLab V2-1: %s verdict=%s pass=%s max_error_km=%.12g raw_miss=%d raw_overlap=%d motion_ms=%.3f projection_ms=%.3f total_ms=%.3f."),
			*Config.FixtureId,
			*Result.Metrics.Verdict,
			Result.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Result.Metrics.AnalyticMotionMaxErrorKm,
			Result.Metrics.RawMotionMissCount,
			Result.Metrics.RawMotionOverlapCount,
			Result.Metrics.MotionApplyMs,
			Result.Metrics.ProjectionKernelMs,
			Result.Metrics.TotalMs);
	}

	if (bRun100k && bScale50kPassed)
	{
		CarrierLab::V2::FCarrierV2Stage1Config Config100k = CarrierLab::V2::FCarrierV2Stage1::MakeScaleConfig(100000, true);
		UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-1: running %s (%d samples, %d plates, policy=%s)."),
			*Config100k.FixtureId,
			Config100k.BaseConfig.SampleCount,
			Config100k.BaseConfig.PlateCount,
			*Config100k.ProjectionCandidatePolicyId);

		CarrierLab::V2::FCarrierV2Stage1FixtureResult Result100k;
		CarrierLab::V2::FCarrierV2Stage1::RunFixtureWithReplay(Config100k, Result100k);
		Suite.Results.Add(Result100k);
		Suite.bAttempted100k = true;
		Suite.bScale100kPass = Result100k.Metrics.bFixturePass;
		bScale100kPassed = Result100k.Metrics.bFixturePass;
		MetricsJsonl += CarrierLab::V2::FCarrierV2Stage1::MetricsToJson(Result100k);
		MetricsJsonl += TEXT("\n");

		UE_LOG(
			LogTemp,
			Display,
			TEXT("CarrierLab V2-1: %s verdict=%s pass=%s max_error_km=%.12g raw_miss=%d raw_overlap=%d motion_ms=%.3f projection_ms=%.3f total_ms=%.3f."),
			*Config100k.FixtureId,
			*Result100k.Metrics.Verdict,
			Result100k.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Result100k.Metrics.AnalyticMotionMaxErrorKm,
			Result100k.Metrics.RawMotionMissCount,
			Result100k.Metrics.RawMotionOverlapCount,
			Result100k.Metrics.MotionApplyMs,
			Result100k.Metrics.ProjectionKernelMs,
			Result100k.Metrics.TotalMs);
	}

	if (bShouldAttempt250k)
	{
		CarrierLab::V2::FCarrierV2Stage1Config Config250k = CarrierLab::V2::FCarrierV2Stage1::MakeScaleConfig(250000, true);
		UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-1: running %s (%d samples, %d plates, policy=%s)."),
			*Config250k.FixtureId,
			Config250k.BaseConfig.SampleCount,
			Config250k.BaseConfig.PlateCount,
			*Config250k.ProjectionCandidatePolicyId);

		CarrierLab::V2::FCarrierV2Stage1FixtureResult Result250k;
		CarrierLab::V2::FCarrierV2Stage1::RunFixtureWithReplay(Config250k, Result250k);
		Suite.Results.Add(Result250k);
		Suite.bAttempted250k = true;
		Suite.bScale250kPass = Result250k.Metrics.bFixturePass;
		MetricsJsonl += CarrierLab::V2::FCarrierV2Stage1::MetricsToJson(Result250k);
		MetricsJsonl += TEXT("\n");

		UE_LOG(
			LogTemp,
			Display,
			TEXT("CarrierLab V2-1: %s verdict=%s pass=%s max_error_km=%.12g raw_miss=%d raw_overlap=%d motion_ms=%.3f projection_ms=%.3f total_ms=%.3f."),
			*Config250k.FixtureId,
			*Result250k.Metrics.Verdict,
			Result250k.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Result250k.Metrics.AnalyticMotionMaxErrorKm,
			Result250k.Metrics.RawMotionMissCount,
			Result250k.Metrics.RawMotionOverlapCount,
			Result250k.Metrics.MotionApplyMs,
			Result250k.Metrics.ProjectionKernelMs,
			Result250k.Metrics.TotalMs);
	}

	if (bRun500k && Suite.bAttempted250k && Suite.bScale250kPass)
	{
		CarrierLab::V2::FCarrierV2Stage1Config Config500k = CarrierLab::V2::FCarrierV2Stage1::MakeScaleConfig(500000, true);
		Config500k.BaseConfig.FixtureId = TEXT("SCALE-500K-MOTION");
		Config500k.BaseConfig.FixtureName = TEXT("Scale500kRigidMotionStretch");
		Config500k.FixtureId = Config500k.BaseConfig.FixtureId;
		Config500k.FixtureName = Config500k.BaseConfig.FixtureName;
		UE_LOG(LogTemp, Display, TEXT("CarrierLab Milestone 1: running %s (%d samples, %d plates, policy=%s)."),
			*Config500k.FixtureId,
			Config500k.BaseConfig.SampleCount,
			Config500k.BaseConfig.PlateCount,
			*Config500k.ProjectionCandidatePolicyId);

		CarrierLab::V2::FCarrierV2Stage1FixtureResult Result500k;
		CarrierLab::V2::FCarrierV2Stage1::RunFixtureWithReplay(Config500k, Result500k);
		Suite.Results.Add(Result500k);
		Suite.bAttempted500k = true;
		Suite.bScale500kPass = Result500k.Metrics.bFixturePass;
		MetricsJsonl += CarrierLab::V2::FCarrierV2Stage1::MetricsToJson(Result500k);
		MetricsJsonl += TEXT("\n");
	}

	Suite.bMicroGatePass = bAllMicroPassed;
	Suite.bScale50kPass = bScale50kPassed;
	Suite.bScale100kPass = Suite.bAttempted100k && bScale100kPassed;
	Suite.bStageGatePass = Suite.bMicroGatePass && Suite.bScale50kPass && Suite.bAttempted250k && Suite.bScale250kPass;
	Suite.Verdict = Suite.bStageGatePass ? TEXT("MILESTONE_1_PASS") : TEXT("MILESTONE_1_REVISE");

	if (!SaveTextFile(MetricsPath, MetricsJsonl))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab V2-1: failed to write metrics JSONL: %s"), *MetricsPath);
		return 1;
	}

	const FString CommitSha = GetGitHead();
	const FString Report = CarrierLab::V2::FCarrierV2Stage1::BuildCheckpointReport(Suite, Params, CommitSha);
	if (!SaveTextFile(RuntimeReportPath, Report))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab V2-1: failed to write runtime report: %s"), *RuntimeReportPath);
		return 1;
	}
	if (!SaveTextFile(CheckpointReportPath, Report))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab V2-1: failed to write checkpoint report: %s"), *CheckpointReportPath);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-1 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-1 runtime report: %s"), *RuntimeReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-1 checkpoint report: %s"), *CheckpointReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-1 final verdict: %s"), *Suite.Verdict);
	return Suite.bStageGatePass ? 0 : 2;
}
