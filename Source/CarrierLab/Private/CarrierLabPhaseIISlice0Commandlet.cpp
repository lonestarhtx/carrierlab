// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIISlice0Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	FString JsonString(const FString& Value)
	{
		FString Escaped;
		Escaped.Reserve(Value.Len() + 2);
		for (const TCHAR Ch : Value)
		{
			switch (Ch)
			{
			case TEXT('\\'):
				Escaped += TEXT("\\\\");
				break;
			case TEXT('"'):
				Escaped += TEXT("\\\"");
				break;
			case TEXT('\n'):
				Escaped += TEXT("\\n");
				break;
			case TEXT('\r'):
				Escaped += TEXT("\\r");
				break;
			case TEXT('\t'):
				Escaped += TEXT("\\t");
				break;
			default:
				Escaped.AppendChar(Ch);
				break;
			}
		}
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}

	TArray<int32> ParseResolutions(const FString& Params)
	{
		auto ExtractValue = [&Params](const FString& Key, FString& OutValue)
		{
			const FString Pattern = FString::Printf(TEXT("-%s="), *Key);
			int32 Start = Params.Find(Pattern, ESearchCase::IgnoreCase);
			int32 Offset = Pattern.Len();
			if (Start == INDEX_NONE)
			{
				const FString BarePattern = FString::Printf(TEXT("%s="), *Key);
				Start = Params.Find(BarePattern, ESearchCase::IgnoreCase);
				Offset = BarePattern.Len();
			}
			if (Start == INDEX_NONE)
			{
				return false;
			}

			Start += Offset;
			if (Start >= Params.Len())
			{
				OutValue.Reset();
				return true;
			}

			TCHAR Quote = 0;
			if (Params[Start] == TEXT('"') || Params[Start] == TEXT('\''))
			{
				Quote = Params[Start];
				++Start;
			}

			int32 End = Start;
			while (End < Params.Len())
			{
				const TCHAR Ch = Params[End];
				if ((Quote != 0 && Ch == Quote) || (Quote == 0 && FChar::IsWhitespace(Ch)))
				{
					break;
				}
				++End;
			}

			OutValue = Params.Mid(Start, End - Start);
			return true;
		};

		FString Value;
		if (!ExtractValue(TEXT("Resolutions"), Value))
		{
			return {60000, 100000, 250000, 500000};
		}

		TArray<int32> Resolutions;
		TArray<FString> Parts;
		Value.ReplaceInline(TEXT(";"), TEXT(","));
		Value.ReplaceInline(TEXT("+"), TEXT(","));
		Value.ReplaceInline(TEXT("|"), TEXT(","));
		Value.ReplaceInline(TEXT("_"), TEXT(","));
		Value.ParseIntoArray(Parts, TEXT(","), true);
		for (const FString& Part : Parts)
		{
			const int32 Resolution = FCString::Atoi(*Part);
			if (Resolution > 0)
			{
				Resolutions.Add(Resolution);
			}
		}
		return Resolutions;
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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseII"), TEXT("Slice0"), Stamp);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	struct FSlice0StepMetric
	{
		int32 Resolution = 0;
		int32 Replay = 0;
		int32 Step = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 Seed = 42;
		int32 MissCount = 0;
		int32 MultiHitCount = 0;
		int32 BoundaryHitCount = 0;
		int32 BoundaryVertexCount = 0;
		int32 NaNOrInfCount = 0;
		double AuthoritativeCAF = 0.0;
		double ProjectedCAF = 0.0;
		double DriftMeanKm = 0.0;
		double DriftP95Km = 0.0;
		double StepWallSeconds = 0.0;
		double ProjectionSeconds = 0.0;
		double BvhSeconds = 0.0;
		double QuerySeconds = 0.0;
		double DriftSeconds = 0.0;
		double BoundarySeconds = 0.0;
		double HashSeconds = 0.0;
		double RenderSeconds = 0.0;
		double MemoryGb = 0.0;
		FString ProjectionHash;
		FString StateHash;
	};

	struct FSlice0ResolutionSummary
	{
		int32 Resolution = 0;
		int32 StepCount = 0;
		bool bProjectionHashMatch = false;
		bool bStateHashMatch = false;
		bool bAllStepHashesMatch = false;
		double AvgKernelSeconds = 0.0;
		double AvgActorTotalSeconds = 0.0;
		double AvgBvhSeconds = 0.0;
		double AvgQuerySeconds = 0.0;
		double AvgDriftSeconds = 0.0;
		double AvgBoundarySeconds = 0.0;
		double AvgHashSeconds = 0.0;
		double AvgRenderSeconds = 0.0;
		double LastMissPercent = 0.0;
		double LastMultiHitPercent = 0.0;
		double LastAuthoritativeCAF = 0.0;
		double LastProjectedCAF = 0.0;
		double LastDriftP95Km = 0.0;
		double LastMemoryGb = 0.0;
		FString FinalProjectionHashA;
		FString FinalProjectionHashB;
		FString FinalStateHashA;
		FString FinalStateHashB;
	};

	FString MetricJson(const FSlice0StepMetric& Metric)
	{
		return FString::Printf(
			TEXT("{\"resolution\":%d,\"replay\":%d,\"step\":%d,\"sample_count\":%d,\"plate_count\":%d,\"seed\":%d,\"miss_count\":%d,\"multi_hit_count\":%d,\"boundary_hit_count\":%d,\"boundary_vertex_count\":%d,\"nan_or_inf_count\":%d,\"authoritative_caf\":%.12f,\"projected_caf\":%.12f,\"drift_mean_km\":%.12f,\"drift_p95_km\":%.12f,\"step_wall_seconds\":%.12f,\"projection_seconds\":%.12f,\"bvh_seconds\":%.12f,\"query_seconds\":%.12f,\"drift_seconds\":%.12f,\"boundary_seconds\":%.12f,\"hash_seconds\":%.12f,\"render_seconds\":%.12f,\"memory_gb\":%.12f,\"projection_hash\":%s,\"state_hash\":%s}"),
			Metric.Resolution,
			Metric.Replay,
			Metric.Step,
			Metric.SampleCount,
			Metric.PlateCount,
			Metric.Seed,
			Metric.MissCount,
			Metric.MultiHitCount,
			Metric.BoundaryHitCount,
			Metric.BoundaryVertexCount,
			Metric.NaNOrInfCount,
			Metric.AuthoritativeCAF,
			Metric.ProjectedCAF,
			Metric.DriftMeanKm,
			Metric.DriftP95Km,
			Metric.StepWallSeconds,
			Metric.ProjectionSeconds,
			Metric.BvhSeconds,
			Metric.QuerySeconds,
			Metric.DriftSeconds,
			Metric.BoundarySeconds,
			Metric.HashSeconds,
			Metric.RenderSeconds,
			Metric.MemoryGb,
			*JsonString(Metric.ProjectionHash),
			*JsonString(Metric.StateHash));
	}

	FSlice0StepMetric CaptureMetric(
		const ACarrierLabVisualizationActor& Actor,
		const int32 Resolution,
		const int32 Replay,
		const double StepWallSeconds)
	{
		const FCarrierLabVisualizationMetrics& Metrics = Actor.CurrentMetrics;
		FSlice0StepMetric Row;
		Row.Resolution = Resolution;
		Row.Replay = Replay;
		Row.Step = Metrics.Step;
		Row.SampleCount = Metrics.SampleCount;
		Row.PlateCount = Metrics.PlateCount;
		Row.Seed = Actor.Seed;
		Row.MissCount = Metrics.RawMissCount;
		Row.MultiHitCount = Metrics.RawMultiHitCount;
		Row.BoundaryHitCount = Metrics.BoundaryHitCount;
		Row.BoundaryVertexCount = Metrics.BoundaryVertexCount;
		Row.NaNOrInfCount = Metrics.NaNOrInfCount;
		Row.AuthoritativeCAF = Metrics.AuthoritativeCAF;
		Row.ProjectedCAF = Metrics.ProjectedCAF;
		Row.DriftMeanKm = Metrics.DriftErrorMeanKm;
		Row.DriftP95Km = Metrics.DriftErrorP95Km;
		Row.StepWallSeconds = StepWallSeconds;
		Row.ProjectionSeconds = Metrics.ProjectionSeconds;
		Row.BvhSeconds = Metrics.BvhBuildSeconds;
		Row.QuerySeconds = Metrics.ProjectionQuerySeconds;
		Row.DriftSeconds = Metrics.DriftMetricsSeconds;
		Row.BoundarySeconds = Metrics.BoundaryMaskSeconds;
		Row.HashSeconds = Metrics.HashSeconds;
		Row.RenderSeconds = Metrics.MeshUpdateSeconds;
		Row.MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		Row.ProjectionHash = Metrics.LastHash;
		Row.StateHash = Metrics.StateHash;
		return Row;
	}

	UWorld* GetBenchmarkWorld()
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

	ACarrierLabVisualizationActor* SpawnBenchmarkActor(UWorld& World, const int32 Resolution)
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
		Actor->SampleCount = Resolution;
		Actor->PlateCount = 40;
		Actor->Seed = 42;
		Actor->ContinentalPlateFraction = 0.30;
		Actor->VelocityMmPerYear = 66.6666666667;
		Actor->MultiHitPolicy = ECarrierLabMultiHitPolicy::Centroid;
		Actor->VisualizationLayer = ECarrierLabVisualizationLayer::PlateId;
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	bool RunActorReplay(const int32 Resolution, const int32 Replay, const int32 StepCount, TArray<FSlice0StepMetric>& OutRows)
	{
		UWorld* World = GetBenchmarkWorld();
		if (World == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 0 benchmark failed to create world for %d replay %d."), Resolution, Replay);
			return false;
		}

		ACarrierLabVisualizationActor* Actor = SpawnBenchmarkActor(*World, Resolution);
		if (Actor == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 0 benchmark failed to spawn actor for %d replay %d."), Resolution, Replay);
			return false;
		}

		const double InitStartSeconds = FPlatformTime::Seconds();
		if (!Actor->InitializeCarrier())
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 0 benchmark failed to initialize actor for %d replay %d."), Resolution, Replay);
			return false;
		}
		OutRows.Add(CaptureMetric(*Actor, Resolution, Replay, FPlatformTime::Seconds() - InitStartSeconds));

		for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
		{
			const double StepStartSeconds = FPlatformTime::Seconds();
			Actor->StepOnce();
			OutRows.Add(CaptureMetric(*Actor, Resolution, Replay, FPlatformTime::Seconds() - StepStartSeconds));
		}

		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return true;
	}

	FSlice0ResolutionSummary SummarizeResolution(const int32 Resolution, const int32 StepCount, const TArray<FSlice0StepMetric>& Rows)
	{
		FSlice0ResolutionSummary Summary;
		Summary.Resolution = Resolution;
		Summary.StepCount = StepCount;

		const FSlice0StepMetric* FinalA = nullptr;
		const FSlice0StepMetric* FinalB = nullptr;
		for (const FSlice0StepMetric& Row : Rows)
		{
			if (Row.Resolution != Resolution)
			{
				continue;
			}
			if (Row.Replay == 0 && Row.Step == StepCount)
			{
				FinalA = &Row;
			}
			else if (Row.Replay == 1 && Row.Step == StepCount)
			{
				FinalB = &Row;
			}
		}

		Summary.bAllStepHashesMatch = true;
		for (int32 Step = 0; Step <= StepCount; ++Step)
		{
			const FSlice0StepMetric* A = nullptr;
			const FSlice0StepMetric* B = nullptr;
			for (const FSlice0StepMetric& Row : Rows)
			{
				if (Row.Resolution == Resolution && Row.Step == Step)
				{
					if (Row.Replay == 0)
					{
						A = &Row;
					}
					else if (Row.Replay == 1)
					{
						B = &Row;
					}
				}
			}
			if (A == nullptr || B == nullptr || A->ProjectionHash != B->ProjectionHash || A->StateHash != B->StateHash)
			{
				Summary.bAllStepHashesMatch = false;
				break;
			}
		}

		if (FinalA != nullptr)
		{
			Summary.FinalProjectionHashA = FinalA->ProjectionHash;
			Summary.FinalStateHashA = FinalA->StateHash;
			Summary.LastMissPercent = FinalA->SampleCount > 0 ? static_cast<double>(FinalA->MissCount) * 100.0 / static_cast<double>(FinalA->SampleCount) : 0.0;
			Summary.LastMultiHitPercent = FinalA->SampleCount > 0 ? static_cast<double>(FinalA->MultiHitCount) * 100.0 / static_cast<double>(FinalA->SampleCount) : 0.0;
			Summary.LastAuthoritativeCAF = FinalA->AuthoritativeCAF;
			Summary.LastProjectedCAF = FinalA->ProjectedCAF;
			Summary.LastDriftP95Km = FinalA->DriftP95Km;
			Summary.LastMemoryGb = FinalA->MemoryGb;
		}
		if (FinalB != nullptr)
		{
			Summary.FinalProjectionHashB = FinalB->ProjectionHash;
			Summary.FinalStateHashB = FinalB->StateHash;
		}
		Summary.bProjectionHashMatch = FinalA != nullptr && FinalB != nullptr && FinalA->ProjectionHash == FinalB->ProjectionHash;
		Summary.bStateHashMatch = FinalA != nullptr && FinalB != nullptr && FinalA->StateHash == FinalB->StateHash;

		int32 TimedStepCount = 0;
		for (const FSlice0StepMetric& Row : Rows)
		{
			if (Row.Resolution != Resolution || Row.Replay != 0 || Row.Step <= 0)
			{
				continue;
			}
			++TimedStepCount;
			Summary.AvgKernelSeconds += Row.ProjectionSeconds;
			Summary.AvgActorTotalSeconds += Row.StepWallSeconds;
			Summary.AvgBvhSeconds += Row.BvhSeconds;
			Summary.AvgQuerySeconds += Row.QuerySeconds;
			Summary.AvgDriftSeconds += Row.DriftSeconds;
			Summary.AvgBoundarySeconds += Row.BoundarySeconds;
			Summary.AvgHashSeconds += Row.HashSeconds;
			Summary.AvgRenderSeconds += Row.RenderSeconds;
		}
		if (TimedStepCount > 0)
		{
			Summary.AvgKernelSeconds /= TimedStepCount;
			Summary.AvgActorTotalSeconds /= TimedStepCount;
			Summary.AvgBvhSeconds /= TimedStepCount;
			Summary.AvgQuerySeconds /= TimedStepCount;
			Summary.AvgDriftSeconds /= TimedStepCount;
			Summary.AvgBoundarySeconds /= TimedStepCount;
			Summary.AvgHashSeconds /= TimedStepCount;
			Summary.AvgRenderSeconds /= TimedStepCount;
		}
		return Summary;
	}

	FString BuildReport(
		const FString& OutputRoot,
		const int32 StepCount,
		const TArray<FSlice0ResolutionSummary>& Summaries,
		const bool bAllDeterministic)
	{
		FString Report = TEXT("# Phase II Slice 0 Checkpoint: Baseline Performance Validation\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This checkpoint validates the optimized actor path only: timing instrumentation, persistent render mesh/color updates, cached plate topology, combined projection BVH, and deterministic parallel per-sample ray queries. No subduction contacts, labels, filters, material mutation, or new resampling behavior is introduced.\n\n");
		Report += FString::Printf(TEXT("Benchmark configuration: 40 plates, seed 42, continental plate fraction 0.30, centroid multi-hit policy, velocity 66.6666666667 mm/y, no resampling, `%d` measured rigid steps per replay plus initialization. Each resolution ran two same-seed replays.\n\n"), StepCount);
		Report += TEXT("Important caveat: exact pre-optimization actor hashes/timings were not captured before Commit A added timing instrumentation. The `before` column therefore remains `not instrumented` except for the informal user-observed 250k actor cost of roughly 3 seconds/step. This run establishes the optimized baseline and same-seed replay gate for future Phase II work.\n\n");

		Report += TEXT("## Timing Summary\n\n");
		Report += TEXT("| Resolution | Before actor total | Optimized kernel s | Optimized actor total s | BVH s | Query s | Drift s | Boundary s | Hash s | Render s | Paper Table 2 target | Gate |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FSlice0ResolutionSummary& Summary : Summaries)
		{
			const FString Before = Summary.Resolution == 250000 ? TEXT("~3.000 informal") : TEXT("not instrumented");
			const double PaperTarget = Summary.Resolution == 60000 ? 0.19 : Summary.Resolution == 100000 ? 0.28 : Summary.Resolution == 250000 ? 1.24 : Summary.Resolution == 500000 ? 1.90 : 0.0;
			const bool bPassKernel = PaperTarget <= 0.0 || Summary.AvgKernelSeconds <= PaperTarget;
			Report += FString::Printf(
				TEXT("| %d | %s | %.6f | %.6f | %.6f | %.6f | %.6f | %.6f | %.6f | %.6f | %.2f | %s |\n"),
				Summary.Resolution,
				*Before,
				Summary.AvgKernelSeconds,
				Summary.AvgActorTotalSeconds,
				Summary.AvgBvhSeconds,
				Summary.AvgQuerySeconds,
				Summary.AvgDriftSeconds,
				Summary.AvgBoundarySeconds,
				Summary.AvgHashSeconds,
				Summary.AvgRenderSeconds,
				PaperTarget,
				bPassKernel ? TEXT("pass") : TEXT("investigate"));
		}

		Report += TEXT("\n## Determinism\n\n");
		Report += TEXT("| Resolution | Projection hash A | Projection hash B | State hash A | State hash B | All step hashes match | Gate |\n");
		Report += TEXT("|---:|---|---|---|---|---|---|\n");
		for (const FSlice0ResolutionSummary& Summary : Summaries)
		{
			const bool bPass = Summary.bProjectionHashMatch && Summary.bStateHashMatch && Summary.bAllStepHashesMatch;
			Report += FString::Printf(
				TEXT("| %d | `%s` | `%s` | `%s` | `%s` | %s | %s |\n"),
				Summary.Resolution,
				*Summary.FinalProjectionHashA,
				*Summary.FinalProjectionHashB,
				*Summary.FinalStateHashA,
				*Summary.FinalStateHashB,
				Summary.bAllStepHashesMatch ? TEXT("yes") : TEXT("no"),
				bPass ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Final-Step Metrics\n\n");
		Report += TEXT("| Resolution | Miss % | Multi-hit % | Auth CAF | Projected CAF | Drift p95 km | Memory GB |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FSlice0ResolutionSummary& Summary : Summaries)
		{
			Report += FString::Printf(
				TEXT("| %d | %.6f | %.6f | %.6f | %.6f | %.12f | %.3f |\n"),
				Summary.Resolution,
				Summary.LastMissPercent,
				Summary.LastMultiHitPercent,
				Summary.LastAuthoritativeCAF,
				Summary.LastProjectedCAF,
				Summary.LastDriftP95Km,
				Summary.LastMemoryGb);
		}

		Report += TEXT("\n## Interpretation\n\n");
		Report += TEXT("The actor path now has measurement points for the expensive surfaces that matter before Phase II contact detection: combined projection BVH build/refit, ray query, drift metrics, boundary mask, hash, and render/color update. This is still a no-mutation Slice 0 surface; global samples remain projection output, not process authority.\n\n");
		Report += TEXT("The before/after requirement is only partially closed because pre-optimization actor hashes were not recorded before the optimization series. The current optimized path is replay-deterministic within this run, which makes it a valid baseline for later Phase II slices. Treat the missing exact pre-optimization hash comparison as a documentation gap, not as permission to loosen future optimization gates.\n\n");

		Report += TEXT("## Recommendation\n\n");
		if (bAllDeterministic)
		{
			Report += TEXT("Conditional go for Phase II Slice 1 after user review: the optimized actor path is deterministic across same-seed replay. Any resolution marked `investigate` in the timing table should be profiled before adding contact detection at that resolution.\n");
		}
		else
		{
			Report += TEXT("No-go for Phase II Slice 1: same-seed replay hash determinism failed. Investigate the optimized actor path before adding contact detection.\n");
		}
		return Report;
	}
}

UCarrierLabPhaseIISlice0Commandlet::UCarrierLabPhaseIISlice0Commandlet()
{
	IsClient = false;
	IsEditor = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIISlice0Commandlet::Main(const FString& Params)
{
	const TArray<int32> Resolutions = ParseResolutions(Params);
	const int32 StepCount = FMath::Max(1, ParseIntParam(Params, TEXT("Steps="), 3));
	if (Resolutions.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("No valid Slice 0 resolutions provided."));
		return 1;
	}

	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	TArray<FSlice0StepMetric> Rows;
	bool bAllRunsCompleted = true;
	for (const int32 Resolution : Resolutions)
	{
		for (int32 Replay = 0; Replay < 2; ++Replay)
		{
			UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 0: resolution=%d replay=%d steps=%d"), Resolution, Replay, StepCount);
			bAllRunsCompleted &= RunActorReplay(Resolution, Replay, StepCount, Rows);
		}
	}

	FString MetricsJsonl;
	for (const FSlice0StepMetric& Row : Rows)
	{
		MetricsJsonl += MetricJson(Row);
		MetricsJsonl += LINE_TERMINATOR;
	}
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	if (!FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write Slice 0 metrics: %s"), *MetricsPath);
		return 1;
	}

	TArray<FSlice0ResolutionSummary> Summaries;
	bool bAllDeterministic = true;
	for (const int32 Resolution : Resolutions)
	{
		FSlice0ResolutionSummary Summary = SummarizeResolution(Resolution, StepCount, Rows);
		bAllDeterministic &= Summary.bProjectionHashMatch && Summary.bStateHashMatch && Summary.bAllStepHashesMatch;
		Summaries.Add(MoveTemp(Summary));
	}

	const FString Report = BuildReport(OutputRoot, StepCount, Summaries, bAllDeterministic);
	const FString ReportPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("docs"), TEXT("checkpoints"), TEXT("phase-ii-slice-0-report.md"));
	if (!FFileHelper::SaveStringToFile(Report, *ReportPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write Slice 0 report: %s"), *ReportPath);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 0 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 0 report: %s"), *ReportPath);
	return bAllRunsCompleted ? 0 : 2;
}
