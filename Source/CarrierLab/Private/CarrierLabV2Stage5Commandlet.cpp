// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabV2Stage5Commandlet.h"

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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("V2"), TEXT("Stage5"), Stamp);
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
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("docs"), TEXT("checkpoints"), TEXT("v2-stage-5-report.md"));
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

UCarrierLabV2Stage5Commandlet::UCarrierLabV2Stage5Commandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarrierLabV2Stage5Commandlet::Main(const FString& Params)
{
	const bool bMicroOnly = Params.Contains(TEXT("-MicroOnly"), ESearchCase::IgnoreCase);
	const bool bRun250k = Params.Contains(TEXT("-Run250k"), ESearchCase::IgnoreCase);
	int32 ScaleSamples = 50000;
	FParse::Value(*Params, TEXT("ScaleSamples="), ScaleSamples);
	ScaleSamples = FMath::Max(4, ScaleSamples);

	const FString OutputRoot = GetOutputRoot(Params);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	const FString RuntimeReportPath = FPaths::Combine(OutputRoot, TEXT("v2-stage-5-report.md"));
	const FString CheckpointReportPath = ResolveReportPath(Params);

	CarrierLab::V2::FCarrierV2Stage5SuiteResult Suite;
	Suite.OutputRoot = OutputRoot;
	Suite.MetricsPath = MetricsPath;
	Suite.ReportPath = CheckpointReportPath;

	TArray<CarrierLab::V2::FCarrierV2Stage5Config> Configs = CarrierLab::V2::FCarrierV2Stage5::MakeMicroFixtureConfigs();
	if (!bMicroOnly)
	{
		Configs.Add(CarrierLab::V2::FCarrierV2Stage5::MakeScaleConfig(ScaleSamples, false));
	}

	FString MetricsJsonl;
	bool bAllMicroPassed = true;
	bool bScale50kPassed = bMicroOnly;
	bool bShouldAttempt250k = false;

	for (const CarrierLab::V2::FCarrierV2Stage5Config& Config : Configs)
	{
		UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-5: running %s (%d samples, %d plates, sampling_policy=%s, partition_policy=%s)."),
			*Config.FixtureId,
			Config.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig.BaseConfig.SampleCount,
			Config.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig.BaseConfig.PlateCount,
			*Config.RemeshSamplingPolicyId,
			*Config.TrianglePartitionPolicyId);

		CarrierLab::V2::FCarrierV2Stage5FixtureResult Result;
		CarrierLab::V2::FCarrierV2Stage5::RunFixtureWithReplay(Config, Result);
		Suite.Results.Add(Result);
		MetricsJsonl += CarrierLab::V2::FCarrierV2Stage5::MetricsToJson(Result);
		MetricsJsonl += TEXT("\n");

		if (IsMicroFixture(Config.FixtureId))
		{
			bAllMicroPassed = bAllMicroPassed && Result.Metrics.bFixturePass;
		}
		else if (Config.FixtureId == TEXT("SCALE-50K-Q1Q2-REBUILD"))
		{
			bScale50kPassed = Result.Metrics.bFixturePass;
			bShouldAttempt250k = bScale50kPassed && bRun250k;
		}

		UE_LOG(
			LogTemp,
			Display,
			TEXT("CarrierLab V2-5: %s verdict=%s pass=%s raw_hits=%lld filtered=(sub:%d coll:%d) valid=%d zero=%d q1q2_pairs=%d qgamma=%d generated=%d no_pair=%d rebuilt_triangles=%d/%d reset=%d post_marks=%d forbidden=(owner:%d centroid:%d random:%d nearest:%d repair:%d retain:%d q1q2_prior:%d q1q2_discrete:%d material_without_gap:%d terrain:%d) sampling_ms=%.3f rebuild_ms=%.3f total_ms=%.3f."),
			*Config.FixtureId,
			*Result.Metrics.Verdict,
			Result.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Result.Metrics.RawHitCountTotal,
			Result.Metrics.FilteredSubductingHitCount,
			Result.Metrics.FilteredCollidingHitCount,
			Result.Metrics.ValidHitAfterFilterCount,
			Result.Metrics.ZeroValidHitCount,
			Result.Metrics.Q1Q2BoundaryPairCount,
			Result.Metrics.QGammaComputedCount,
			Result.Metrics.GeneratedOceanicCount,
			Result.Metrics.GapFillNoBoundaryPairCount,
			Result.Metrics.RebuiltTriangleAssignmentCount,
			Result.Metrics.GlobalTriangleCount,
			Result.Metrics.ProcessStateResetCount,
			Result.Metrics.PostResetTriangleMarkCount,
			Result.Metrics.PriorOwnerFallbackCount,
			Result.Metrics.CentroidPrimaryResolutionCount,
			Result.Metrics.RandomPrimaryResolutionCount,
			Result.Metrics.NearestPrimaryResolutionCount,
			Result.Metrics.OwnershipRepairDuringRemeshCount,
			Result.Metrics.RetentionHysteresisAnchorCount,
			Result.Metrics.Q1Q2PriorOwnerLookupCount,
			Result.Metrics.Q1Q2DiscreteApproxCount,
			Result.Metrics.MaterialCreatedWithoutGapFillCount,
			Result.Metrics.TerrainBeautyMutationCount,
			Result.Metrics.GlobalSamplingMs,
			Result.Metrics.TopologyRebuildMs,
			Result.Metrics.TotalMs);
	}

	if (bShouldAttempt250k)
	{
		CarrierLab::V2::FCarrierV2Stage5Config Config250k = CarrierLab::V2::FCarrierV2Stage5::MakeScaleConfig(250000, true);
		UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-5: running %s (%d samples, %d plates, sampling_policy=%s, partition_policy=%s)."),
			*Config250k.FixtureId,
			Config250k.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig.BaseConfig.SampleCount,
			Config250k.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig.BaseConfig.PlateCount,
			*Config250k.RemeshSamplingPolicyId,
			*Config250k.TrianglePartitionPolicyId);

		CarrierLab::V2::FCarrierV2Stage5FixtureResult Result250k;
		CarrierLab::V2::FCarrierV2Stage5::RunFixtureWithReplay(Config250k, Result250k);
		Suite.Results.Add(Result250k);
		Suite.bAttempted250k = true;
		Suite.bScale250kPass = Result250k.Metrics.bFixturePass;
		MetricsJsonl += CarrierLab::V2::FCarrierV2Stage5::MetricsToJson(Result250k);
		MetricsJsonl += TEXT("\n");

		UE_LOG(
			LogTemp,
			Display,
			TEXT("CarrierLab V2-5: %s verdict=%s pass=%s raw_hits=%lld filtered=(sub:%d coll:%d) valid=%d zero=%d q1q2_pairs=%d qgamma=%d generated=%d rebuilt_triangles=%d/%d reset=%d sampling_ms=%.3f rebuild_ms=%.3f total_ms=%.3f."),
			*Config250k.FixtureId,
			*Result250k.Metrics.Verdict,
			Result250k.Metrics.bFixturePass ? TEXT("true") : TEXT("false"),
			Result250k.Metrics.RawHitCountTotal,
			Result250k.Metrics.FilteredSubductingHitCount,
			Result250k.Metrics.FilteredCollidingHitCount,
			Result250k.Metrics.ValidHitAfterFilterCount,
			Result250k.Metrics.ZeroValidHitCount,
			Result250k.Metrics.Q1Q2BoundaryPairCount,
			Result250k.Metrics.QGammaComputedCount,
			Result250k.Metrics.GeneratedOceanicCount,
			Result250k.Metrics.RebuiltTriangleAssignmentCount,
			Result250k.Metrics.GlobalTriangleCount,
			Result250k.Metrics.ProcessStateResetCount,
			Result250k.Metrics.GlobalSamplingMs,
			Result250k.Metrics.TopologyRebuildMs,
			Result250k.Metrics.TotalMs);
	}

	Suite.bMicroGatePass = bAllMicroPassed;
	Suite.bScale50kPass = bScale50kPassed;
	Suite.bStageGatePass = Suite.bMicroGatePass && Suite.bScale50kPass && (!Suite.bAttempted250k || Suite.bScale250kPass);
	Suite.Verdict = Suite.bStageGatePass ? TEXT("GO_V2_6") : TEXT("REVISE_V2_5");

	if (!SaveTextFile(MetricsPath, MetricsJsonl))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab V2-5: failed to write metrics JSONL: %s"), *MetricsPath);
		return 1;
	}

	const FString CommitSha = GetGitHead();
	const FString Report = CarrierLab::V2::FCarrierV2Stage5::BuildCheckpointReport(Suite, Params, CommitSha);
	if (!SaveTextFile(RuntimeReportPath, Report))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab V2-5: failed to write runtime report: %s"), *RuntimeReportPath);
		return 1;
	}
	if (!SaveTextFile(CheckpointReportPath, Report))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab V2-5: failed to write checkpoint report: %s"), *CheckpointReportPath);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-5 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-5 runtime report: %s"), *RuntimeReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-5 checkpoint report: %s"), *CheckpointReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab V2-5 final verdict: %s"), *Suite.Verdict);
	return Suite.bStageGatePass ? 0 : 2;
}
