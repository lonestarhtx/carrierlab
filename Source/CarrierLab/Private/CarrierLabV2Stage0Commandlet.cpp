// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabV2Stage0Commandlet.h"

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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("V2"), TEXT("Stage0"), Stamp);
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
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("docs"), TEXT("checkpoints"), TEXT("v2-stage-0-report.md"));
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

UCarrierLabV2Stage0Commandlet::UCarrierLabV2Stage0Commandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarrierLabV2Stage0Commandlet::Main(const FString& Params)
{
	const bool bMicroOnly = Params.Contains(TEXT("-MicroOnly"), ESearchCase::IgnoreCase);
	const bool bRun250k = Params.Contains(TEXT("-Run250k"), ESearchCase::IgnoreCase);
	int32 ScaleSamples = 50000;
	FParse::Value(*Params, TEXT("ScaleSamples="), ScaleSamples);
	ScaleSamples = FMath::Max(4, ScaleSamples);

	const FString OutputRoot = GetOutputRoot(Params);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	const FString RuntimeReportPath = FPaths::Combine(OutputRoot, TEXT("v2-stage-0-report.md"));
	const FString CheckpointReportPath = ResolveReportPath(Params);

	CarrierLab::V2::FCarrierV2Stage0SuiteResult Suite;
	Suite.OutputRoot = OutputRoot;
	Suite.MetricsPath = MetricsPath;
	Suite.ReportPath = CheckpointReportPath;

	TArray<CarrierLab::V2::FCarrierV2Stage0Config> Configs = CarrierLab::V2::FCarrierV2Stage0::MakeMicroFixtureConfigs();
	if (!bMicroOnly)
	{
		Configs.Add(CarrierLab::V2::FCarrierV2Stage0::MakeScaleConfig(ScaleSamples, false));
	}

	FString MetricsJsonl;
	bool bAllMicroPassed = true;
	bool bScale50kPassed = bMicroOnly;
	bool bShouldAttempt250k = false;

	for (const CarrierLab::V2::FCarrierV2Stage0Config& Config : Configs)
	{
		UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-0: running %s (%d samples, %d plates)."), *Config.FixtureId, Config.SampleCount, Config.PlateCount);
		CarrierLab::V2::FCarrierV2FixtureResult Result;
		CarrierLab::V2::FCarrierV2Stage0::RunFixtureWithReplay(Config, Result);
		Suite.Results.Add(Result);
		MetricsJsonl += CarrierLab::V2::FCarrierV2Stage0::MetricsToJson(Result);
		MetricsJsonl += TEXT("\n");

		if (IsMicroFixture(Config.FixtureId))
		{
			bAllMicroPassed = bAllMicroPassed && Result.Metrics.bFixturePass;
		}
		else if (Config.FixtureId == TEXT("SCALE-50K"))
		{
			bScale50kPassed = Result.Metrics.bFixturePass;
			bShouldAttempt250k = bScale50kPassed && bRun250k;
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT("CarrierLab V2-0: %s verdict=%s pass=%s projection_ms=%.3f total_ms=%.3f observed=%s."),
			*Config.FixtureId,
			*Result.Metrics.Verdict,
			Result.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Result.Metrics.ProjectionKernelMs,
			Result.Metrics.TotalMs,
			*Result.Metrics.ObservedFailureReason);
	}

	if (bShouldAttempt250k)
	{
		CarrierLab::V2::FCarrierV2Stage0Config Config250k = CarrierLab::V2::FCarrierV2Stage0::MakeScaleConfig(250000, true);
		UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-0: running %s (%d samples, %d plates)."), *Config250k.FixtureId, Config250k.SampleCount, Config250k.PlateCount);
		CarrierLab::V2::FCarrierV2FixtureResult Result250k;
		CarrierLab::V2::FCarrierV2Stage0::RunFixtureWithReplay(Config250k, Result250k);
		Suite.Results.Add(Result250k);
		Suite.bAttempted250k = true;
		Suite.bScale250kPass = Result250k.Metrics.bFixturePass;
		MetricsJsonl += CarrierLab::V2::FCarrierV2Stage0::MetricsToJson(Result250k);
		MetricsJsonl += TEXT("\n");

		UE_LOG(
			LogTemp,
			Display,
			TEXT("CarrierLab V2-0: %s verdict=%s pass=%s projection_ms=%.3f total_ms=%.3f observed=%s."),
			*Config250k.FixtureId,
			*Result250k.Metrics.Verdict,
			Result250k.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Result250k.Metrics.ProjectionKernelMs,
			Result250k.Metrics.TotalMs,
			*Result250k.Metrics.ObservedFailureReason);
	}

	Suite.bMicroGatePass = bAllMicroPassed;
	Suite.bScale50kPass = bScale50kPassed;
	Suite.bStageGatePass = Suite.bMicroGatePass && Suite.bScale50kPass && (!Suite.bAttempted250k || Suite.bScale250kPass);
	Suite.Verdict = Suite.bStageGatePass ? TEXT("GO_STAGE_1") : TEXT("REVISE_V2_0");

	if (!SaveTextFile(MetricsPath, MetricsJsonl))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab V2-0: failed to write metrics JSONL: %s"), *MetricsPath);
		return 1;
	}

	const FString CommitSha = GetGitHead();
	const FString Report = CarrierLab::V2::FCarrierV2Stage0::BuildCheckpointReport(Suite, Params, CommitSha);
	if (!SaveTextFile(RuntimeReportPath, Report))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab V2-0: failed to write runtime report: %s"), *RuntimeReportPath);
		return 1;
	}
	if (!SaveTextFile(CheckpointReportPath, Report))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab V2-0: failed to write checkpoint report: %s"), *CheckpointReportPath);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-0 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-0 runtime report: %s"), *RuntimeReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-0 checkpoint report: %s"), *CheckpointReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-0 final verdict: %s"), *Suite.Verdict);
	return Suite.bStageGatePass ? 0 : 2;
}
