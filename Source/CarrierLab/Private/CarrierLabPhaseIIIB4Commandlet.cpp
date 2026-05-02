// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIB4Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	enum class EMaterialFixture : uint8
	{
		Default,
		MixedPlate0Continental,
		MixedPlate1Continental,
		AllContinental,
		AllOceanic
	};

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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIB4"), Stamp);
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
				TEXT("phase-iii-slice-iiib4-report.md"));
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

	bool ApplyMaterialFixture(ACarrierLabVisualizationActor& Actor, const EMaterialFixture Fixture)
	{
		switch (Fixture)
		{
		case EMaterialFixture::MixedPlate0Continental:
			return Actor.SetPlateContinentalForTest(0, true) &&
				Actor.SetPlateContinentalForTest(1, false);
		case EMaterialFixture::MixedPlate1Continental:
			return Actor.SetPlateContinentalForTest(0, false) &&
				Actor.SetPlateContinentalForTest(1, true);
		case EMaterialFixture::AllContinental:
			return Actor.SetPlateContinentalForTest(0, true) &&
				Actor.SetPlateContinentalForTest(1, true);
		case EMaterialFixture::AllOceanic:
			return Actor.SetPlateContinentalForTest(0, false) &&
				Actor.SetPlateContinentalForTest(1, false);
		case EMaterialFixture::Default:
		default:
			return true;
		}
	}

	FString PolarityClassName(const CarrierLab::EConvergenceSubductionPolarityClass DecisionClass)
	{
		switch (DecisionClass)
		{
		case CarrierLab::EConvergenceSubductionPolarityClass::OceanicUnderContinental:
			return TEXT("oceanic_under_continental");
		case CarrierLab::EConvergenceSubductionPolarityClass::CollisionCandidate:
			return TEXT("collision_candidate");
		case CarrierLab::EConvergenceSubductionPolarityClass::OceanOceanDeferred:
			return TEXT("ocean_ocean_deferred");
		case CarrierLab::EConvergenceSubductionPolarityClass::Invalid:
			return TEXT("invalid");
		case CarrierLab::EConvergenceSubductionPolarityClass::None:
		default:
			return TEXT("none");
		}
	}

	const FCarrierLabPhaseIIIB4PolarityDecisionAudit* FindPairDecision(
		const FCarrierLabPhaseIIIB4PolarityAudit& Audit,
		const int32 PlateA,
		const int32 PlateB)
	{
		const int32 MinPlate = FMath::Min(PlateA, PlateB);
		const int32 MaxPlate = FMath::Max(PlateA, PlateB);
		for (const FCarrierLabPhaseIIIB4PolarityDecisionAudit& Decision : Audit.Decisions)
		{
			if (Decision.PlateA == MinPlate && Decision.PlateB == MaxPlate)
			{
				return &Decision;
			}
		}
		return nullptr;
	}

	struct FPolarityReplayResult
	{
		FString Fixture;
		int32 Replay = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		double TotalSeconds = 0.0;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit MatrixInitial;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit MatrixFinal;
		FCarrierLabPhaseIIIB4PolarityAudit PolarityInitial;
		FCarrierLabPhaseIIIB4PolarityAudit PolarityFinal;
		FString ProjectionHashAfter;
		FString StateHashAfter;
		FString CrustStateHashAfter;
		bool bCompleted = false;
	};

	bool RunPolarityReplay(
		const FString& Fixture,
		const ECarrierLabPhaseIIMotionFixture MotionFixture,
		const EMaterialFixture MaterialFixture,
		const int32 Replay,
		const int32 SampleCount,
		const int32 PlateCount,
		const int32 StepCount,
		FPolarityReplayResult& OutResult)
	{
		OutResult = FPolarityReplayResult();
		OutResult.Fixture = Fixture;
		OutResult.Replay = Replay;
		OutResult.SampleCount = SampleCount;
		OutResult.PlateCount = PlateCount;
		OutResult.StepCount = StepCount;

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
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
		Actor->ConfigurePhaseIIMotionFixture(MotionFixture);
		if (!ApplyMaterialFixture(*Actor, MaterialFixture))
		{
			Actor->Destroy();
			return false;
		}
		if (!Actor->GetPhaseIIIB3SubductionMatrixAudit(OutResult.MatrixInitial) ||
			!Actor->GetPhaseIIIB4PolarityAudit(OutResult.PolarityInitial))
		{
			Actor->Destroy();
			return false;
		}

		for (int32 Step = 0; Step < StepCount; ++Step)
		{
			Actor->StepOnce();
		}
		if (!Actor->GetPhaseIIIB3SubductionMatrixAudit(OutResult.MatrixFinal) ||
			!Actor->GetPhaseIIIB4PolarityAudit(OutResult.PolarityFinal))
		{
			Actor->Destroy();
			return false;
		}

		OutResult.ProjectionHashAfter = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashAfter = Actor->CurrentMetrics.StateHash;
		OutResult.CrustStateHashAfter = Actor->CurrentMetrics.CrustStateHash;
		OutResult.TotalSeconds = FPlatformTime::Seconds() - TotalStartSeconds;
		OutResult.bCompleted = true;
		Actor->Destroy();
		return true;
	}

	FString AuditJsonFields(const FCarrierLabPhaseIIIB4PolarityAudit& Audit, const TCHAR* Prefix)
	{
		return FString::Printf(
			TEXT(",\"%s_step\":%d,\"%s_reset\":%d,\"%s_matrix_pairs\":%d,\"%s_decisions\":%d,\"%s_oceanic_under_continental\":%d,\"%s_collision_candidate\":%d,\"%s_ocean_ocean_deferred\":%d,\"%s_invalid\":%d,\"%s_missing\":%d,\"%s_subduction_polarity\":%d,\"%s_probe_a\":%d,\"%s_probe_b\":%d,\"%s_probe_under\":%d,\"%s_probe_over\":%d,\"%s_probe_a_continental_fraction\":%.12f,\"%s_probe_b_continental_fraction\":%.12f,\"%s_probe_class\":%s,\"%s_polarity_hash\":%s,\"%s_convergence_hash\":%s"),
			Prefix, Audit.Step,
			Prefix, Audit.ResetSerial,
			Prefix, Audit.MatrixPairCount,
			Prefix, Audit.DecisionCount,
			Prefix, Audit.OceanicUnderContinentalCount,
			Prefix, Audit.CollisionCandidateCount,
			Prefix, Audit.OceanOceanDeferredCount,
			Prefix, Audit.InvalidDecisionCount,
			Prefix, Audit.MissingDecisionCount,
			Prefix, Audit.SubductionPolarityCount,
			Prefix, Audit.ProbePlateA,
			Prefix, Audit.ProbePlateB,
			Prefix, Audit.ProbeUnderPlate,
			Prefix, Audit.ProbeOverPlate,
			Prefix, Audit.ProbePlateAContinentalFraction,
			Prefix, Audit.ProbePlateBContinentalFraction,
			Prefix, *JsonString(PolarityClassName(Audit.ProbeDecisionClass)),
			Prefix, *JsonString(Audit.PolarityHash),
			Prefix, *JsonString(Audit.ConvergenceTrackingHash));
	}

	FString ReplayJson(const FPolarityReplayResult& Result)
	{
		FString Json = FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"completed\":%s,\"samples\":%d,\"plates\":%d,\"steps\":%d,\"seconds\":%.6f,\"projection_hash_after\":%s,\"state_hash_after\":%s,\"crust_state_hash_after\":%s,\"initial_matrix_pairs\":%d,\"final_matrix_pairs\":%d"),
			*JsonString(Result.Fixture),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.SampleCount,
			Result.PlateCount,
			Result.StepCount,
			Result.TotalSeconds,
			*JsonString(Result.ProjectionHashAfter),
			*JsonString(Result.StateHashAfter),
			*JsonString(Result.CrustStateHashAfter),
			Result.MatrixInitial.MatrixPairCount,
			Result.MatrixFinal.MatrixPairCount);
		Json += AuditJsonFields(Result.PolarityInitial, TEXT("initial"));
		Json += AuditJsonFields(Result.PolarityFinal, TEXT("final"));
		Json += TEXT("}");
		return Json;
	}

	bool IsMixedPolarityResult(const FPolarityReplayResult& Result, const int32 ExpectedUnder, const int32 ExpectedOver)
	{
		const FCarrierLabPhaseIIIB4PolarityDecisionAudit* Decision = FindPairDecision(Result.PolarityFinal, 0, 1);
		return Result.bCompleted &&
			Result.MatrixInitial.MatrixPairCount == 0 &&
			Result.PolarityInitial.DecisionCount == 0 &&
			Result.MatrixFinal.MatrixPairCount == 1 &&
			Result.PolarityFinal.DecisionCount == 1 &&
			Result.PolarityFinal.OceanicUnderContinentalCount == 1 &&
			Result.PolarityFinal.SubductionPolarityCount == 1 &&
			Result.PolarityFinal.CollisionCandidateCount == 0 &&
			Result.PolarityFinal.OceanOceanDeferredCount == 0 &&
			Result.PolarityFinal.InvalidDecisionCount == 0 &&
			Result.PolarityFinal.MissingDecisionCount == 0 &&
			Decision != nullptr &&
			Decision->DecisionClass == CarrierLab::EConvergenceSubductionPolarityClass::OceanicUnderContinental &&
			Decision->UnderPlate == ExpectedUnder &&
			Decision->OverPlate == ExpectedOver;
	}

	bool IsCollisionCandidateResult(const FPolarityReplayResult& Result)
	{
		const FCarrierLabPhaseIIIB4PolarityDecisionAudit* Decision = FindPairDecision(Result.PolarityFinal, 0, 1);
		return Result.bCompleted &&
			Result.MatrixFinal.MatrixPairCount == 1 &&
			Result.PolarityFinal.DecisionCount == 1 &&
			Result.PolarityFinal.CollisionCandidateCount == 1 &&
			Result.PolarityFinal.SubductionPolarityCount == 0 &&
			Result.PolarityFinal.OceanicUnderContinentalCount == 0 &&
			Result.PolarityFinal.InvalidDecisionCount == 0 &&
			Result.PolarityFinal.MissingDecisionCount == 0 &&
			Decision != nullptr &&
			Decision->DecisionClass == CarrierLab::EConvergenceSubductionPolarityClass::CollisionCandidate &&
			Decision->UnderPlate == INDEX_NONE &&
			Decision->OverPlate == INDEX_NONE;
	}

	bool IsOceanOceanDeferredResult(const FPolarityReplayResult& Result)
	{
		const FCarrierLabPhaseIIIB4PolarityDecisionAudit* Decision = FindPairDecision(Result.PolarityFinal, 0, 1);
		return Result.bCompleted &&
			Result.MatrixFinal.MatrixPairCount == 1 &&
			Result.PolarityFinal.DecisionCount == 1 &&
			Result.PolarityFinal.OceanOceanDeferredCount == 1 &&
			Result.PolarityFinal.SubductionPolarityCount == 0 &&
			Result.PolarityFinal.InvalidDecisionCount == 0 &&
			Result.PolarityFinal.MissingDecisionCount == 0 &&
			Decision != nullptr &&
			Decision->DecisionClass == CarrierLab::EConvergenceSubductionPolarityClass::OceanOceanDeferred &&
			Decision->UnderPlate == INDEX_NONE &&
			Decision->OverPlate == INDEX_NONE;
	}

	bool IsEmptyResult(const FPolarityReplayResult& Result)
	{
		return Result.bCompleted &&
			Result.MatrixFinal.MatrixPairCount == 0 &&
			Result.PolarityFinal.DecisionCount == 0 &&
			Result.PolarityFinal.SubductionPolarityCount == 0 &&
			Result.PolarityFinal.CollisionCandidateCount == 0 &&
			Result.PolarityFinal.OceanOceanDeferredCount == 0 &&
			Result.PolarityFinal.InvalidDecisionCount == 0 &&
			Result.PolarityFinal.MissingDecisionCount == 0;
	}

	bool ReplayHashesMatch(const FPolarityReplayResult& A, const FPolarityReplayResult& B)
	{
		return A.bCompleted && B.bCompleted &&
			A.PolarityFinal.PolarityHash == B.PolarityFinal.PolarityHash &&
			A.PolarityFinal.ConvergenceTrackingHash == B.PolarityFinal.ConvergenceTrackingHash &&
			A.ProjectionHashAfter == B.ProjectionHashAfter &&
			A.StateHashAfter == B.StateHashAfter &&
			A.CrustStateHashAfter == B.CrustStateHashAfter;
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FPolarityReplayResult& MixedA,
		const FPolarityReplayResult& MixedB,
		const FPolarityReplayResult& SwappedA,
		const FPolarityReplayResult& SwappedB,
		const FPolarityReplayResult& CollisionA,
		const FPolarityReplayResult& CollisionB,
		const FPolarityReplayResult& OceanA,
		const FPolarityReplayResult& OceanB,
		const FPolarityReplayResult& DivergenceA,
		const FPolarityReplayResult& DivergenceB,
		const FPolarityReplayResult& ZeroA,
		const FPolarityReplayResult& ZeroB)
	{
		const bool bMixed = IsMixedPolarityResult(MixedA, 1, 0) && ReplayHashesMatch(MixedA, MixedB);
		const bool bSwapped = IsMixedPolarityResult(SwappedA, 0, 1) && ReplayHashesMatch(SwappedA, SwappedB);
		const bool bSwapFlips = MixedA.PolarityFinal.PolarityHash != SwappedA.PolarityFinal.PolarityHash &&
			MixedA.PolarityFinal.ProbeUnderPlate == 1 &&
			SwappedA.PolarityFinal.ProbeUnderPlate == 0;
		const bool bCollision = IsCollisionCandidateResult(CollisionA) && ReplayHashesMatch(CollisionA, CollisionB);
		const bool bOcean = IsOceanOceanDeferredResult(OceanA) && ReplayHashesMatch(OceanA, OceanB);
		const bool bDivergence = IsEmptyResult(DivergenceA) && ReplayHashesMatch(DivergenceA, DivergenceB);
		const bool bZero = IsEmptyResult(ZeroA) && ReplayHashesMatch(ZeroA, ZeroB) &&
			ZeroA.PolarityInitial.PolarityHash == ZeroA.PolarityFinal.PolarityHash;

		FString Report;
		Report += TEXT("# Phase III Slice IIIB.4 Checkpoint: Oceanic-Under-Continental Polarity Rule\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This slice evaluates polarity for existing `SubductionMatrix` plate-pair entries. It is read-only: it does not mark triangles, filter projection candidates, resample material, or advance any IIIC mutation path.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n|---|---|---|\n");
		Report += FString::Printf(TEXT("| Mixed oceanic/continental polarity | %s | plate 1 under plate 0, hash `%s` |\n"),
			*PassFail(bMixed), *MixedA.PolarityFinal.PolarityHash);
		Report += FString::Printf(TEXT("| Polarity-swap fixture flips under/over | %s | mixed under %d, swapped under %d, swapped hash `%s` |\n"),
			*PassFail(bSwapped && bSwapFlips), MixedA.PolarityFinal.ProbeUnderPlate, SwappedA.PolarityFinal.ProbeUnderPlate, *SwappedA.PolarityFinal.PolarityHash);
		Report += FString::Printf(TEXT("| Continental-continental emits collision-candidate only | %s | collision %d, subduction polarity %d |\n"),
			*PassFail(bCollision), CollisionA.PolarityFinal.CollisionCandidateCount, CollisionA.PolarityFinal.SubductionPolarityCount);
		Report += FString::Printf(TEXT("| Ocean-ocean defers to IIIB.5 | %s | deferred %d, subduction polarity %d |\n"),
			*PassFail(bOcean), OceanA.PolarityFinal.OceanOceanDeferredCount, OceanA.PolarityFinal.SubductionPolarityCount);
		Report += FString::Printf(TEXT("| Forced divergence remains empty | %s | matrix pairs %d, decisions %d |\n"),
			*PassFail(bDivergence), DivergenceA.PolarityFinal.MatrixPairCount, DivergenceA.PolarityFinal.DecisionCount);
		Report += FString::Printf(TEXT("| Zero-motion remains empty and stable | %s | initial `%s`, final `%s` |\n"),
			*PassFail(bZero), *ZeroA.PolarityInitial.PolarityHash, *ZeroA.PolarityFinal.PolarityHash);
		Report += TEXT("\n");

		Report += TEXT("## Polarity Audits\n\n");
		Report += TEXT("| Fixture | Replay | Step | Matrix pairs | Decisions | Oceanic-under | Collision | Ocean-ocean deferred | Under | Over | A/B continental fraction | Polarity hash | Convergence hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|\n");
		auto AddRow = [&Report](const FPolarityReplayResult& Result)
		{
			const FCarrierLabPhaseIIIB4PolarityAudit& Audit = Result.PolarityFinal;
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d | %d | %d | %d | %d | %.3f / %.3f | `%s` | `%s` |\n"),
				*Result.Fixture,
				Result.Replay,
				Audit.Step,
				Audit.MatrixPairCount,
				Audit.DecisionCount,
				Audit.OceanicUnderContinentalCount,
				Audit.CollisionCandidateCount,
				Audit.OceanOceanDeferredCount,
				Audit.ProbeUnderPlate,
				Audit.ProbeOverPlate,
				Audit.ProbePlateAContinentalFraction,
				Audit.ProbePlateBContinentalFraction,
				*Audit.PolarityHash,
				*Audit.ConvergenceTrackingHash);
		};
		AddRow(MixedA);
		AddRow(MixedB);
		AddRow(SwappedA);
		AddRow(SwappedB);
		AddRow(CollisionA);
		AddRow(CollisionB);
		AddRow(OceanA);
		AddRow(OceanB);
		AddRow(DivergenceA);
		AddRow(DivergenceB);
		AddRow(ZeroA);
		AddRow(ZeroB);
		Report += TEXT("\n");

		Report += TEXT("## Notes\n\n");
		Report += TEXT("- Dominant material is computed from plate-local vertex continental fraction, not from persistent global ownership.\n");
		Report += TEXT("- `OceanicUnderContinental` records an under/over plate pair but does not mark any triangle as subducting; IIIC owns triangle marking and filter integration.\n");
		Report += TEXT("- Continental-continental entries are logged as collision candidates for IIID. Ocean-ocean entries are deferred to IIIB.5 for age polarity.\n");
		Report += TEXT("- Empty matrix controls produce no polarity decisions, preserving IIIB.3's forced-divergence and zero-motion gates.\n\n");

		const bool bAllPass = bMixed && bSwapped && bSwapFlips && bCollision && bOcean && bDivergence && bZero;
		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIB.4 passes. Pause for user review before IIIB.5 (ocean-ocean age polarity rule).\n")
			: TEXT("IIIB.4 does not pass. Investigate before IIIB.5.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIB4Commandlet::UCarrierLabPhaseIIIB4Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIB4Commandlet::Main(const FString& Params)
{
	const int32 FixtureSamples = FMath::Max(12, ParseIntParam(Params, TEXT("FixtureSamples="), 10000));
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FPolarityReplayResult MixedA;
	FPolarityReplayResult MixedB;
	const bool bMixedA = RunPolarityReplay(TEXT("mixed_oceanic_under_continental"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::MixedPlate0Continental, 0, FixtureSamples, 2, 4, MixedA);
	const bool bMixedB = RunPolarityReplay(TEXT("mixed_oceanic_under_continental"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::MixedPlate0Continental, 1, FixtureSamples, 2, 4, MixedB);

	FPolarityReplayResult SwappedA;
	FPolarityReplayResult SwappedB;
	const bool bSwappedA = RunPolarityReplay(TEXT("mixed_polarity_swap"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::MixedPlate1Continental, 0, FixtureSamples, 2, 4, SwappedA);
	const bool bSwappedB = RunPolarityReplay(TEXT("mixed_polarity_swap"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::MixedPlate1Continental, 1, FixtureSamples, 2, 4, SwappedB);

	FPolarityReplayResult CollisionA;
	FPolarityReplayResult CollisionB;
	const bool bCollisionA = RunPolarityReplay(TEXT("continental_continental_collision_candidate"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllContinental, 0, FixtureSamples, 2, 4, CollisionA);
	const bool bCollisionB = RunPolarityReplay(TEXT("continental_continental_collision_candidate"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllContinental, 1, FixtureSamples, 2, 4, CollisionB);

	FPolarityReplayResult OceanA;
	FPolarityReplayResult OceanB;
	const bool bOceanA = RunPolarityReplay(TEXT("ocean_ocean_deferred"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllOceanic, 0, FixtureSamples, 2, 4, OceanA);
	const bool bOceanB = RunPolarityReplay(TEXT("ocean_ocean_deferred"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllOceanic, 1, FixtureSamples, 2, 4, OceanB);

	FPolarityReplayResult DivergenceA;
	FPolarityReplayResult DivergenceB;
	const bool bDivergenceA = RunPolarityReplay(TEXT("forced_divergence_empty"), ECarrierLabPhaseIIMotionFixture::ForcedDivergence, EMaterialFixture::MixedPlate0Continental, 0, FixtureSamples, 2, 1, DivergenceA);
	const bool bDivergenceB = RunPolarityReplay(TEXT("forced_divergence_empty"), ECarrierLabPhaseIIMotionFixture::ForcedDivergence, EMaterialFixture::MixedPlate0Continental, 1, FixtureSamples, 2, 1, DivergenceB);

	FPolarityReplayResult ZeroA;
	FPolarityReplayResult ZeroB;
	const bool bZeroA = RunPolarityReplay(TEXT("zero_motion_empty"), ECarrierLabPhaseIIMotionFixture::Zero, EMaterialFixture::Default, 0, FixtureSamples, 40, 10, ZeroA);
	const bool bZeroB = RunPolarityReplay(TEXT("zero_motion_empty"), ECarrierLabPhaseIIMotionFixture::Zero, EMaterialFixture::Default, 1, FixtureSamples, 40, 10, ZeroB);

	FString MetricsJsonl;
	MetricsJsonl += ReplayJson(MixedA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(MixedB) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(SwappedA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(SwappedB) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(CollisionA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(CollisionB) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(OceanA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(OceanB) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(DivergenceA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(DivergenceB) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(ZeroA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(ZeroB) + LINE_TERMINATOR;
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);

	const FString Report = BuildReport(OutputRoot, MixedA, MixedB, SwappedA, SwappedB, CollisionA, CollisionB, OceanA, OceanB, DivergenceA, DivergenceB, ZeroA, ZeroB);
	const FString ReportPath = ResolveReportPath(Params);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.4 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.4 report: %s"), *ReportPath);

	const bool bMixed = bMixedA && bMixedB && IsMixedPolarityResult(MixedA, 1, 0) && ReplayHashesMatch(MixedA, MixedB);
	const bool bSwapped = bSwappedA && bSwappedB && IsMixedPolarityResult(SwappedA, 0, 1) && ReplayHashesMatch(SwappedA, SwappedB);
	const bool bSwapFlips = MixedA.PolarityFinal.PolarityHash != SwappedA.PolarityFinal.PolarityHash &&
		MixedA.PolarityFinal.ProbeUnderPlate == 1 &&
		SwappedA.PolarityFinal.ProbeUnderPlate == 0;
	const bool bCollision = bCollisionA && bCollisionB && IsCollisionCandidateResult(CollisionA) && ReplayHashesMatch(CollisionA, CollisionB);
	const bool bOcean = bOceanA && bOceanB && IsOceanOceanDeferredResult(OceanA) && ReplayHashesMatch(OceanA, OceanB);
	const bool bDivergence = bDivergenceA && bDivergenceB && IsEmptyResult(DivergenceA) && ReplayHashesMatch(DivergenceA, DivergenceB);
	const bool bZero = bZeroA && bZeroB && IsEmptyResult(ZeroA) && ReplayHashesMatch(ZeroA, ZeroB) &&
		ZeroA.PolarityInitial.PolarityHash == ZeroA.PolarityFinal.PolarityHash;

	if (!(bMixed && bSwapped && bSwapFlips && bCollision && bOcean && bDivergence && bZero))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab Phase IIIB.4 gates failed."));
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.4 gates passed."));
	return 0;
}
