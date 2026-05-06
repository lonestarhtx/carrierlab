// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPreIIIE8Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr int32 FixtureSamples = 10000;
	constexpr int32 FixturePlates = 2;
	constexpr int32 FixtureSeed = 42;
	constexpr int32 MaxSearchSteps = 4;
	constexpr double FixtureVelocityMmPerYear = 66.6666666667;
	constexpr double CollisionThresholdKm = 300.0;
	constexpr double DestinationMassThresholdRatio = 0.5;

	void HashMix(uint64& Hash, const uint64 Value)
	{
		Hash ^= Value;
		Hash *= 1099511628211ull;
	}

	void HashMixDouble(uint64& Hash, const double Value)
	{
		HashMix(Hash, static_cast<uint64>(FMath::RoundToInt64(Value * 1000000000.0)));
	}

	void HashMixString(uint64& Hash, const FString& Value)
	{
		HashMix(Hash, static_cast<uint64>(Value.Len() + 1));
		for (const TCHAR Ch : Value)
		{
			HashMix(Hash, static_cast<uint64>(Ch) + 1ull);
		}
	}

	FString HashToString(const uint64 Hash)
	{
		return FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Hash));
	}

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

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			OutputRoot = FPaths::Combine(
				FPaths::ProjectSavedDir(),
				TEXT("CarrierLab"),
				TEXT("PhaseIII"),
				TEXT("PreIIIE8"));
		}
		else if (FPaths::IsRelative(OutputRoot))
		{
			OutputRoot = FPaths::Combine(FPaths::ProjectDir(), OutputRoot);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	FString GetReportPath(const FString& Params)
	{
		FString ReportPath;
		if (!FParse::Value(*Params, TEXT("Report="), ReportPath))
		{
			ReportPath = FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("docs"),
				TEXT("checkpoints"),
				TEXT("phase-iii-pre-iiie8-obduction-bridge.md"));
		}
		else if (FPaths::IsRelative(ReportPath))
		{
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), ReportPath);
		}
		return FPaths::ConvertRelativePathToFull(ReportPath);
	}

	ACarrierLabVisualizationActor* SpawnActor(
		UWorld& World,
		const bool bEnableBridge,
		const ECarrierLabPhaseIIMotionFixture MotionFixture)
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
		Actor->SampleCount = FixtureSamples;
		Actor->PlateCount = FixturePlates;
		Actor->Seed = FixtureSeed;
		Actor->ContinentalPlateFraction = 0.5;
		Actor->VelocityMmPerYear = FixtureVelocityMmPerYear;
		Actor->MultiHitPolicy = ECarrierLabMultiHitPolicy::Centroid;
		Actor->bEnableNaturalResamplingEvents = false;
		Actor->ConfigurePhaseIIICProcessLayer(bEnableBridge, false);
		Actor->FinishSpawning(FTransform::Identity);
		if (!Actor->InitializeCarrier())
		{
			Actor->Destroy();
			return nullptr;
		}
		Actor->ConfigurePhaseIIMotionFixture(MotionFixture);
		return Actor;
	}

	int32 CountCarrierTriangles(const ACarrierLabVisualizationActor& Actor)
	{
		return Actor.GetCarrierLocalTriangleCountForTest();
	}

	struct FObductionReplayResult
	{
		int32 Replay = 0;
		FString Fixture;
		bool bCompleted = false;
		double Seconds = 0.0;
		int32 StepCount = 0;
		int32 EventCountBefore = 0;
		int32 EventCountAfter = 0;
		int32 TriangleCountBefore = 0;
		int32 TriangleCountAfter = 0;
		int32 PolicyResolvedMultiHitCount = 0;
		FCarrierLabPhaseIIIB4PolarityAudit PolarityAudit;
		FCarrierLabPhaseIIID2CollisionGroupingAudit GroupingAudit;
		FCarrierLabPhaseIIICObductionUpliftAudit ObductionAudit;
		FString StateHashBefore;
		FString CrustHashBefore;
		FString StateHashAfter;
		FString CrustHashAfter;
		FString ReplayHash;
	};

	FString ComputeReplayHash(const FObductionReplayResult& Result)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, Result.Fixture);
		HashMix(Hash, static_cast<uint64>(Result.StepCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.EventCountBefore + 1));
		HashMix(Hash, static_cast<uint64>(Result.EventCountAfter + 1));
		HashMix(Hash, static_cast<uint64>(Result.TriangleCountBefore + 1));
		HashMix(Hash, static_cast<uint64>(Result.TriangleCountAfter + 1));
		HashMix(Hash, static_cast<uint64>(Result.PolicyResolvedMultiHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PolarityAudit.CollisionCandidateCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.GroupingAudit.AcceptedGroupCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.ObductionAudit.CollisionCandidateHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.ObductionAudit.MarkCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.ObductionAudit.UpliftRecordCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.ObductionAudit.UniqueUpliftedVertexCount + 1));
		HashMixDouble(Hash, Result.ObductionAudit.TotalAppliedDeltaKm);
		HashMixDouble(Hash, Result.ObductionAudit.MaxAppliedDeltaKm);
		HashMixString(Hash, Result.ObductionAudit.ObductionMarkHash);
		HashMixString(Hash, Result.ObductionAudit.ObductionUpliftHash);
		HashMixString(Hash, Result.StateHashBefore);
		HashMixString(Hash, Result.StateHashAfter);
		HashMixString(Hash, Result.CrustHashBefore);
		HashMixString(Hash, Result.CrustHashAfter);
		return HashToString(Hash);
	}

	bool RunReplay(
		const int32 Replay,
		const FString& Fixture,
		const ECarrierLabPhaseIIMotionFixture MotionFixture,
		const bool bPlate0Continental,
		const bool bPlate1Continental,
		const bool bExpectBridge,
		FObductionReplayResult& OutResult)
	{
		OutResult = FObductionReplayResult();
		OutResult.Replay = Replay;
		OutResult.Fixture = Fixture;
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, true, MotionFixture);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->SetPlateContinentalForTest(0, bPlate0Continental) ||
			!Actor->SetPlateContinentalForTest(1, bPlate1Continental))
		{
			Actor->Destroy();
			return false;
		}
		OutResult.EventCountBefore = Actor->CurrentMetrics.EventCount;
		OutResult.TriangleCountBefore = CountCarrierTriangles(*Actor);
		OutResult.StateHashBefore = Actor->CurrentMetrics.StateHash;
		OutResult.CrustHashBefore = Actor->CurrentMetrics.CrustStateHash;

		for (int32 Step = 1; Step <= MaxSearchSteps; ++Step)
		{
			Actor->StepOnce();
			if (!Actor->GetPhaseIIIB4PolarityAudit(OutResult.PolarityAudit) ||
				!Actor->DetectPhaseIIID2CollisionGroups(OutResult.GroupingAudit, CollisionThresholdKm) ||
				!Actor->GetPhaseIIICObductionUpliftAudit(OutResult.ObductionAudit))
			{
				Actor->Destroy();
				return false;
			}

			const bool bBridgeReady =
				OutResult.PolarityAudit.CollisionCandidateCount > 0 &&
				OutResult.GroupingAudit.AcceptedGroupCount == 0 &&
				OutResult.ObductionAudit.CollisionCandidateHitCount > 0 &&
				OutResult.ObductionAudit.MarkCount > 0 &&
				OutResult.ObductionAudit.UpliftRecordCount > 0 &&
				OutResult.ObductionAudit.TotalAppliedDeltaKm > 0.0;
			const bool bNegativeReady =
				OutResult.PolarityAudit.CollisionCandidateCount == 0 &&
				OutResult.GroupingAudit.AcceptedGroupCount == 0 &&
				OutResult.ObductionAudit.MarkCount == 0 &&
				OutResult.ObductionAudit.UpliftRecordCount == 0;

			if ((bExpectBridge && bBridgeReady) ||
				(!bExpectBridge && bNegativeReady))
			{
				OutResult.StepCount = Step;
				OutResult.EventCountAfter = Actor->CurrentMetrics.EventCount;
				OutResult.TriangleCountAfter = CountCarrierTriangles(*Actor);
				OutResult.PolicyResolvedMultiHitCount = Actor->CurrentMetrics.PolicyResolvedMultiHitCount;
				OutResult.StateHashAfter = Actor->CurrentMetrics.StateHash;
				OutResult.CrustHashAfter = Actor->CurrentMetrics.CrustStateHash;
				OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
				OutResult.bCompleted = true;
				OutResult.ReplayHash = ComputeReplayHash(OutResult);
				break;
			}
		}

		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bCompleted;
	}

	bool BridgePasses(const FObductionReplayResult& A, const FObductionReplayResult& B)
	{
		return A.bCompleted &&
			B.bCompleted &&
			A.ReplayHash == B.ReplayHash &&
			A.PolarityAudit.CollisionCandidateCount > 0 &&
			A.GroupingAudit.AcceptedGroupCount == 0 &&
			A.ObductionAudit.CollisionCandidateHitCount > 0 &&
			A.ObductionAudit.MarkCount > 0 &&
			A.ObductionAudit.DuplicateMarkCount == 0 &&
			A.ObductionAudit.InvalidMarkCount == 0 &&
			A.ObductionAudit.UpliftRecordCount > 0 &&
			A.ObductionAudit.TotalAppliedDeltaKm > 0.0 &&
			A.PolicyResolvedMultiHitCount == 0 &&
			A.EventCountBefore == A.EventCountAfter &&
			A.TriangleCountBefore == A.TriangleCountAfter &&
			A.CrustHashBefore != A.CrustHashAfter;
	}

	bool NegativePasses(const FObductionReplayResult& A, const FObductionReplayResult& B)
	{
		return A.bCompleted &&
			B.bCompleted &&
			A.ReplayHash == B.ReplayHash &&
			A.PolarityAudit.CollisionCandidateCount == 0 &&
			A.GroupingAudit.AcceptedGroupCount == 0 &&
			A.ObductionAudit.MarkCount == 0 &&
			A.ObductionAudit.UpliftRecordCount == 0 &&
			A.PolicyResolvedMultiHitCount == 0 &&
			A.EventCountBefore == A.EventCountAfter &&
			A.TriangleCountBefore == A.TriangleCountAfter;
	}

	FString BuildJsonLine(const FObductionReplayResult& Result)
	{
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"completed\":%s,\"seconds\":%.6f,\"steps\":%d,\"events_before\":%d,\"events_after\":%d,\"triangles_before\":%d,\"triangles_after\":%d,\"collision_candidate_hits\":%d,\"accepted_collision_groups\":%d,\"obduction_marks\":%d,\"uplift_records\":%d,\"total_delta_km\":%.12f,\"state_before\":%s,\"state_after\":%s,\"crust_before\":%s,\"crust_after\":%s,\"replay_hash\":%s}"),
			*JsonString(Result.Fixture),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.Seconds,
			Result.StepCount,
			Result.EventCountBefore,
			Result.EventCountAfter,
			Result.TriangleCountBefore,
			Result.TriangleCountAfter,
			Result.ObductionAudit.CollisionCandidateHitCount,
			Result.GroupingAudit.AcceptedGroupCount,
			Result.ObductionAudit.MarkCount,
			Result.ObductionAudit.UpliftRecordCount,
			Result.ObductionAudit.TotalAppliedDeltaKm,
			*JsonString(Result.StateHashBefore),
			*JsonString(Result.StateHashAfter),
			*JsonString(Result.CrustHashBefore),
			*JsonString(Result.CrustHashAfter),
			*JsonString(Result.ReplayHash));
	}

	FString BuildReport(
		const FObductionReplayResult& BridgeA,
		const FObductionReplayResult& BridgeB,
		const FObductionReplayResult& NegativeA,
		const FObductionReplayResult& NegativeB,
		const FString& MetricsPath)
	{
		const bool bBridgePass = BridgePasses(BridgeA, BridgeB);
		const bool bNegativePass = NegativePasses(NegativeA, NegativeB);
		const bool bPass = bBridgePass && bNegativePass;
		FString Report;
		Report += TEXT("# Phase III Pre-IIIE.8 Obduction Continuous Uplift Bridge\n\n");
		Report += TEXT("Verdict: ");
		Report += bPass ? TEXT("pass") : TEXT("fail");
		Report += TEXT(". This checkpoint adds the missing cont-cont obduction uplift bridge before IIIE.1. It does not start IIIE remesh implementation and does not mutate topology.\n\n");
		Report += TEXT("## Gates\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---:|---|\n");
		Report += FString::Printf(
			TEXT("| Pre-threshold cont-cont obduction uplift | %s | Replay hashes `%s` / `%s`; collision candidate hits `%d`; accepted collision groups `%d`; marks `%d`; uplift records `%d`; total uplift `%.12f km`; events `%d` -> `%d`; triangles `%d` -> `%d`; crust hash `%s` -> `%s`. |\n"),
			*PassFail(bBridgePass),
			*BridgeA.ReplayHash,
			*BridgeB.ReplayHash,
			BridgeA.ObductionAudit.CollisionCandidateHitCount,
			BridgeA.GroupingAudit.AcceptedGroupCount,
			BridgeA.ObductionAudit.MarkCount,
			BridgeA.ObductionAudit.UpliftRecordCount,
			BridgeA.ObductionAudit.TotalAppliedDeltaKm,
			BridgeA.EventCountBefore,
			BridgeA.EventCountAfter,
			BridgeA.TriangleCountBefore,
			BridgeA.TriangleCountAfter,
			*BridgeA.CrustHashBefore,
			*BridgeA.CrustHashAfter);
		Report += FString::Printf(
			TEXT("| Zero-motion negative produces no obduction uplift | %s | Replay hashes `%s` / `%s`; collision candidates `%d`; marks `%d`; uplift records `%d`; events `%d` -> `%d`; triangles `%d` -> `%d`. |\n"),
			*PassFail(bNegativePass),
			*NegativeA.ReplayHash,
			*NegativeB.ReplayHash,
			NegativeA.PolarityAudit.CollisionCandidateCount,
			NegativeA.ObductionAudit.MarkCount,
			NegativeA.ObductionAudit.UpliftRecordCount,
			NegativeA.EventCountBefore,
			NegativeA.EventCountAfter,
			NegativeA.TriangleCountBefore,
			NegativeA.TriangleCountAfter);
		Report += TEXT("\n## Scope Notes\n\n");
		Report += TEXT("- The bridge consumes IIIB.4 `CollisionCandidate` decisions and produces diagnostics-only obduction marks; it does not add persistent global ownership, recovery, repair, backfill, anchoring, or projection-derived authority.\n");
		Report += TEXT("- The pre-threshold gate requires `AcceptedGroupCount == 0`, so uplift is proven before the IIID collision/suture mutation threshold fires.\n");
		Report += TEXT("- Event count and total plate-local triangle count are used as the no-topology-mutation guard; `CrustHashBefore != CrustHashAfter` is expected because visible elevation/fold state changes.\n");
		Report += TEXT("- `PolicyResolvedMultiHitCount == 0` keeps centroid/random lab multi-hit policy dormant in this evidence path.\n");
		Report += TEXT("- Finding 33 is closed for the local cont-cont obduction bridge. Full remesh filtering remains IIIE-owned.\n\n");
		Report += TEXT("## Metrics\n\n");
		Report += FString::Printf(TEXT("JSONL metrics: `%s`\n"), *MetricsPath);
		return Report;
	}
}

UCarrierLabPreIIIE8Commandlet::UCarrierLabPreIIIE8Commandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarrierLabPreIIIE8Commandlet::Main(const FString& Params)
{
	FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("pre-iiie8-obduction-bridge.jsonl"));
	const FString ReportPath = GetReportPath(Params);

	FObductionReplayResult BridgeA;
	FObductionReplayResult BridgeB;
	FObductionReplayResult NegativeA;
	FObductionReplayResult NegativeB;
	const bool bRunsCompleted =
		RunReplay(0, TEXT("forced_continent_continent_prethreshold"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, true, true, true, BridgeA) &&
		RunReplay(1, TEXT("forced_continent_continent_prethreshold"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, true, true, true, BridgeB) &&
		RunReplay(0, TEXT("zero_motion_negative"), ECarrierLabPhaseIIMotionFixture::Zero, true, true, false, NegativeA) &&
		RunReplay(1, TEXT("zero_motion_negative"), ECarrierLabPhaseIIMotionFixture::Zero, true, true, false, NegativeB);

	TArray<FString> Lines;
	Lines.Add(BuildJsonLine(BridgeA));
	Lines.Add(BuildJsonLine(BridgeB));
	Lines.Add(BuildJsonLine(NegativeA));
	Lines.Add(BuildJsonLine(NegativeB));
	FFileHelper::SaveStringArrayToFile(Lines, *MetricsPath);

	const FString Report = BuildReport(BridgeA, BridgeB, NegativeA, NegativeB, MetricsPath);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	const bool bPass = bRunsCompleted && BridgePasses(BridgeA, BridgeB) && NegativePasses(NegativeA, NegativeB);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Pre-IIIE.8 obduction bridge report: %s"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Pre-IIIE.8 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Pre-IIIE.8 verdict: %s"), bPass ? TEXT("pass") : TEXT("fail"));
	return bPass ? 0 : 1;
}
