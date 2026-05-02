// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIB5Commandlet.h"

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
		AllContinental,
		AllOceanic
	};

	struct FAgeFixture
	{
		bool bApply = false;
		double Plate0AgeMa = 0.0;
		double Plate1AgeMa = 0.0;
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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIB5"), Stamp);
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
				TEXT("phase-iii-slice-iiib5-report.md"));
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

	bool ApplyAgeFixture(ACarrierLabVisualizationActor& Actor, const FAgeFixture& Fixture)
	{
		if (!Fixture.bApply)
		{
			return true;
		}
		return Actor.SetPlateOceanicAgeForTest(0, Fixture.Plate0AgeMa) &&
			Actor.SetPlateOceanicAgeForTest(1, Fixture.Plate1AgeMa);
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
		case CarrierLab::EConvergenceSubductionPolarityClass::OlderOceanicUnderYoungerOceanic:
			return TEXT("older_oceanic_under_younger_oceanic");
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

	struct FAgePolarityReplayResult
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

	bool RunAgePolarityReplay(
		const FString& Fixture,
		const ECarrierLabPhaseIIMotionFixture MotionFixture,
		const EMaterialFixture MaterialFixture,
		const FAgeFixture AgeFixture,
		const int32 Replay,
		const int32 SampleCount,
		const int32 PlateCount,
		const int32 StepCount,
		FAgePolarityReplayResult& OutResult)
	{
		OutResult = FAgePolarityReplayResult();
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
		if (!ApplyMaterialFixture(*Actor, MaterialFixture) ||
			!ApplyAgeFixture(*Actor, AgeFixture))
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
			TEXT(",\"%s_step\":%d,\"%s_matrix_pairs\":%d,\"%s_decisions\":%d,\"%s_oceanic_under_continental\":%d,\"%s_older_oceanic_under_younger\":%d,\"%s_collision_candidate\":%d,\"%s_ocean_ocean_deferred\":%d,\"%s_invalid\":%d,\"%s_missing\":%d,\"%s_subduction_polarity\":%d,\"%s_probe_a\":%d,\"%s_probe_b\":%d,\"%s_probe_under\":%d,\"%s_probe_over\":%d,\"%s_probe_a_continental_fraction\":%.12f,\"%s_probe_b_continental_fraction\":%.12f,\"%s_probe_a_oceanic_age\":%.12f,\"%s_probe_b_oceanic_age\":%.12f,\"%s_probe_class\":%s,\"%s_polarity_hash\":%s,\"%s_convergence_hash\":%s"),
			Prefix, Audit.Step,
			Prefix, Audit.MatrixPairCount,
			Prefix, Audit.DecisionCount,
			Prefix, Audit.OceanicUnderContinentalCount,
			Prefix, Audit.OlderOceanicUnderYoungerOceanicCount,
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
			Prefix, Audit.ProbePlateAOceanicAge,
			Prefix, Audit.ProbePlateBOceanicAge,
			Prefix, *JsonString(PolarityClassName(Audit.ProbeDecisionClass)),
			Prefix, *JsonString(Audit.PolarityHash),
			Prefix, *JsonString(Audit.ConvergenceTrackingHash));
	}

	FString ReplayJson(const FAgePolarityReplayResult& Result)
	{
		FString Json = FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"completed\":%s,\"samples\":%d,\"plates\":%d,\"steps\":%d,\"seconds\":%.6f,\"projection_hash_after\":%s,\"state_hash_after\":%s,\"crust_state_hash_after\":%s"),
			*JsonString(Result.Fixture),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.SampleCount,
			Result.PlateCount,
			Result.StepCount,
			Result.TotalSeconds,
			*JsonString(Result.ProjectionHashAfter),
			*JsonString(Result.StateHashAfter),
			*JsonString(Result.CrustStateHashAfter));
		Json += AuditJsonFields(Result.PolarityInitial, TEXT("initial"));
		Json += AuditJsonFields(Result.PolarityFinal, TEXT("final"));
		Json += TEXT("}");
		return Json;
	}

	bool IsAgePolarityResult(
		const FAgePolarityReplayResult& Result,
		const int32 ExpectedUnder,
		const int32 ExpectedOver,
		const double ExpectedPlateAAge,
		const double ExpectedPlateBAge)
	{
		const FCarrierLabPhaseIIIB4PolarityDecisionAudit* Decision = FindPairDecision(Result.PolarityFinal, 0, 1);
		return Result.bCompleted &&
			Result.MatrixInitial.MatrixPairCount == 0 &&
			Result.PolarityInitial.DecisionCount == 0 &&
			Result.MatrixFinal.MatrixPairCount == 1 &&
			Result.PolarityFinal.DecisionCount == 1 &&
			Result.PolarityFinal.OlderOceanicUnderYoungerOceanicCount == 1 &&
			Result.PolarityFinal.SubductionPolarityCount == 1 &&
			Result.PolarityFinal.OceanicUnderContinentalCount == 0 &&
			Result.PolarityFinal.CollisionCandidateCount == 0 &&
			Result.PolarityFinal.OceanOceanDeferredCount == 0 &&
			Result.PolarityFinal.InvalidDecisionCount == 0 &&
			Result.PolarityFinal.MissingDecisionCount == 0 &&
			Decision != nullptr &&
			Decision->DecisionClass == CarrierLab::EConvergenceSubductionPolarityClass::OlderOceanicUnderYoungerOceanic &&
			Decision->UnderPlate == ExpectedUnder &&
			Decision->OverPlate == ExpectedOver &&
			FMath::IsNearlyEqual(Decision->PlateAOceanicAge, ExpectedPlateAAge, 1.0e-6) &&
			FMath::IsNearlyEqual(Decision->PlateBOceanicAge, ExpectedPlateBAge, 1.0e-6);
	}

	bool IsEqualAgeDeferredResult(const FAgePolarityReplayResult& Result)
	{
		const FCarrierLabPhaseIIIB4PolarityDecisionAudit* Decision = FindPairDecision(Result.PolarityFinal, 0, 1);
		return Result.bCompleted &&
			Result.MatrixFinal.MatrixPairCount == 1 &&
			Result.PolarityFinal.DecisionCount == 1 &&
			Result.PolarityFinal.OceanOceanDeferredCount == 1 &&
			Result.PolarityFinal.SubductionPolarityCount == 0 &&
			Result.PolarityFinal.OlderOceanicUnderYoungerOceanicCount == 0 &&
			Result.PolarityFinal.InvalidDecisionCount == 0 &&
			Result.PolarityFinal.MissingDecisionCount == 0 &&
			Decision != nullptr &&
			Decision->DecisionClass == CarrierLab::EConvergenceSubductionPolarityClass::OceanOceanDeferred &&
			Decision->UnderPlate == INDEX_NONE &&
			Decision->OverPlate == INDEX_NONE;
	}

	bool IsMixedRegressionResult(const FAgePolarityReplayResult& Result)
	{
		const FCarrierLabPhaseIIIB4PolarityDecisionAudit* Decision = FindPairDecision(Result.PolarityFinal, 0, 1);
		return Result.bCompleted &&
			Result.MatrixFinal.MatrixPairCount == 1 &&
			Result.PolarityFinal.DecisionCount == 1 &&
			Result.PolarityFinal.OceanicUnderContinentalCount == 1 &&
			Result.PolarityFinal.OlderOceanicUnderYoungerOceanicCount == 0 &&
			Result.PolarityFinal.SubductionPolarityCount == 1 &&
			Decision != nullptr &&
			Decision->DecisionClass == CarrierLab::EConvergenceSubductionPolarityClass::OceanicUnderContinental &&
			Decision->UnderPlate == 1 &&
			Decision->OverPlate == 0;
	}

	bool IsCollisionRegressionResult(const FAgePolarityReplayResult& Result)
	{
		const FCarrierLabPhaseIIIB4PolarityDecisionAudit* Decision = FindPairDecision(Result.PolarityFinal, 0, 1);
		return Result.bCompleted &&
			Result.MatrixFinal.MatrixPairCount == 1 &&
			Result.PolarityFinal.DecisionCount == 1 &&
			Result.PolarityFinal.CollisionCandidateCount == 1 &&
			Result.PolarityFinal.SubductionPolarityCount == 0 &&
			Result.PolarityFinal.OlderOceanicUnderYoungerOceanicCount == 0 &&
			Decision != nullptr &&
			Decision->DecisionClass == CarrierLab::EConvergenceSubductionPolarityClass::CollisionCandidate &&
			Decision->UnderPlate == INDEX_NONE &&
			Decision->OverPlate == INDEX_NONE;
	}

	bool IsEmptyResult(const FAgePolarityReplayResult& Result)
	{
		return Result.bCompleted &&
			Result.MatrixFinal.MatrixPairCount == 0 &&
			Result.PolarityFinal.DecisionCount == 0 &&
			Result.PolarityFinal.SubductionPolarityCount == 0 &&
			Result.PolarityFinal.CollisionCandidateCount == 0 &&
			Result.PolarityFinal.OceanOceanDeferredCount == 0 &&
			Result.PolarityFinal.OlderOceanicUnderYoungerOceanicCount == 0 &&
			Result.PolarityFinal.InvalidDecisionCount == 0 &&
			Result.PolarityFinal.MissingDecisionCount == 0;
	}

	bool ReplayHashesMatch(const FAgePolarityReplayResult& A, const FAgePolarityReplayResult& B)
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
		const FAgePolarityReplayResult& Older0A,
		const FAgePolarityReplayResult& Older0B,
		const FAgePolarityReplayResult& Older1A,
		const FAgePolarityReplayResult& Older1B,
		const FAgePolarityReplayResult& EqualA,
		const FAgePolarityReplayResult& EqualB,
		const FAgePolarityReplayResult& MixedA,
		const FAgePolarityReplayResult& MixedB,
		const FAgePolarityReplayResult& CollisionA,
		const FAgePolarityReplayResult& CollisionB,
		const FAgePolarityReplayResult& DivergenceA,
		const FAgePolarityReplayResult& DivergenceB,
		const FAgePolarityReplayResult& ZeroA,
		const FAgePolarityReplayResult& ZeroB)
	{
		const bool bOlder0 = IsAgePolarityResult(Older0A, 0, 1, 120.0, 20.0) && ReplayHashesMatch(Older0A, Older0B);
		const bool bOlder1 = IsAgePolarityResult(Older1A, 1, 0, 20.0, 120.0) && ReplayHashesMatch(Older1A, Older1B);
		const bool bAgeFlip = Older0A.PolarityFinal.PolarityHash != Older1A.PolarityFinal.PolarityHash &&
			Older0A.PolarityFinal.ProbeUnderPlate == 0 &&
			Older1A.PolarityFinal.ProbeUnderPlate == 1;
		const bool bEqual = IsEqualAgeDeferredResult(EqualA) && ReplayHashesMatch(EqualA, EqualB);
		const bool bMixed = IsMixedRegressionResult(MixedA) && ReplayHashesMatch(MixedA, MixedB);
		const bool bCollision = IsCollisionRegressionResult(CollisionA) && ReplayHashesMatch(CollisionA, CollisionB);
		const bool bDivergence = IsEmptyResult(DivergenceA) && ReplayHashesMatch(DivergenceA, DivergenceB);
		const bool bZero = IsEmptyResult(ZeroA) && ReplayHashesMatch(ZeroA, ZeroB) &&
			ZeroA.PolarityInitial.PolarityHash == ZeroA.PolarityFinal.PolarityHash;
		const bool bAllPass = bOlder0 && bOlder1 && bAgeFlip && bEqual && bMixed && bCollision && bDivergence && bZero;

		FString Report;
		Report += TEXT("# Phase III Slice IIIB.5 Checkpoint: Ocean-Ocean Age Polarity Rule\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This slice completes the read-only polarity decision layer for oceanic-oceanic convergence. It uses plate-local `OceanicAge` as inert crust authority and records older-oceanic-under-younger decisions without marking triangles, filtering projection candidates, resampling material, or mutating process state.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n|---|---|---|\n");
		Report += FString::Printf(TEXT("| Older plate 0 subducts under younger plate 1 | %s | ages %.1f / %.1f Ma, under %d, hash `%s` |\n"),
			*PassFail(bOlder0), Older0A.PolarityFinal.ProbePlateAOceanicAge, Older0A.PolarityFinal.ProbePlateBOceanicAge, Older0A.PolarityFinal.ProbeUnderPlate, *Older0A.PolarityFinal.PolarityHash);
		Report += FString::Printf(TEXT("| Reversing ages reverses polarity | %s | older0 under %d, older1 under %d, reversed hash `%s` |\n"),
			*PassFail(bOlder1 && bAgeFlip), Older0A.PolarityFinal.ProbeUnderPlate, Older1A.PolarityFinal.ProbeUnderPlate, *Older1A.PolarityFinal.PolarityHash);
		Report += FString::Printf(TEXT("| Equal-age ocean-ocean remains deferred | %s | deferred %d, age-polarity %d |\n"),
			*PassFail(bEqual), EqualA.PolarityFinal.OceanOceanDeferredCount, EqualA.PolarityFinal.OlderOceanicUnderYoungerOceanicCount);
		Report += FString::Printf(TEXT("| Mixed-material IIIB.4 regression unchanged | %s | oceanic-under %d, age-polarity %d |\n"),
			*PassFail(bMixed), MixedA.PolarityFinal.OceanicUnderContinentalCount, MixedA.PolarityFinal.OlderOceanicUnderYoungerOceanicCount);
		Report += FString::Printf(TEXT("| Continental collision regression unchanged | %s | collision %d, subduction polarity %d |\n"),
			*PassFail(bCollision), CollisionA.PolarityFinal.CollisionCandidateCount, CollisionA.PolarityFinal.SubductionPolarityCount);
		Report += FString::Printf(TEXT("| Forced divergence remains empty | %s | matrix pairs %d, decisions %d |\n"),
			*PassFail(bDivergence), DivergenceA.PolarityFinal.MatrixPairCount, DivergenceA.PolarityFinal.DecisionCount);
		Report += FString::Printf(TEXT("| Zero-motion remains empty and stable | %s | initial `%s`, final `%s` |\n"),
			*PassFail(bZero), *ZeroA.PolarityInitial.PolarityHash, *ZeroA.PolarityFinal.PolarityHash);
		Report += TEXT("\n");

		Report += TEXT("## Polarity Audits\n\n");
		Report += TEXT("| Fixture | Replay | Step | Matrix pairs | Decisions | Oceanic-under | Age-polarity | Collision | Ocean-ocean deferred | Under | Over | A/B oceanic age Ma | Polarity hash | Convergence hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|\n");
		auto AddRow = [&Report](const FAgePolarityReplayResult& Result)
		{
			const FCarrierLabPhaseIIIB4PolarityAudit& Audit = Result.PolarityFinal;
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %.3f / %.3f | `%s` | `%s` |\n"),
				*Result.Fixture,
				Result.Replay,
				Audit.Step,
				Audit.MatrixPairCount,
				Audit.DecisionCount,
				Audit.OceanicUnderContinentalCount,
				Audit.OlderOceanicUnderYoungerOceanicCount,
				Audit.CollisionCandidateCount,
				Audit.OceanOceanDeferredCount,
				Audit.ProbeUnderPlate,
				Audit.ProbeOverPlate,
				Audit.ProbePlateAOceanicAge,
				Audit.ProbePlateBOceanicAge,
				*Audit.PolarityHash,
				*Audit.ConvergenceTrackingHash);
		};
		AddRow(Older0A);
		AddRow(Older0B);
		AddRow(Older1A);
		AddRow(Older1B);
		AddRow(EqualA);
		AddRow(EqualB);
		AddRow(MixedA);
		AddRow(MixedB);
		AddRow(CollisionA);
		AddRow(CollisionB);
		AddRow(DivergenceA);
		AddRow(DivergenceB);
		AddRow(ZeroA);
		AddRow(ZeroB);
		Report += TEXT("\n");

		Report += TEXT("## Notes\n\n");
		Report += TEXT("- Oceanic age is averaged from plate-local vertices using area weights. Global samples are updated only by the explicit test seeding helper so fixtures survive projection/replay checks.\n");
		Report += TEXT("- Equal oceanic ages still defer, preserving the IIIB.4 no-invented-policy discipline when age evidence cannot distinguish polarity.\n");
		Report += TEXT("- The age rule is a decision record only. IIIB.6 may propagate from active decisions, but this slice does not label neighbors, filter resampling, or mutate crust.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIB.5 passes. Pause for user review before IIIB.6 (neighbor propagation).\n")
			: TEXT("IIIB.5 does not pass. Investigate before IIIB.6.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIB5Commandlet::UCarrierLabPhaseIIIB5Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIB5Commandlet::Main(const FString& Params)
{
	const int32 FixtureSamples = FMath::Max(12, ParseIntParam(Params, TEXT("FixtureSamples="), 10000));
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	const FAgeFixture OlderPlate0{ true, 120.0, 20.0 };
	const FAgeFixture OlderPlate1{ true, 20.0, 120.0 };
	const FAgeFixture EqualAges{ true, 64.0, 64.0 };
	const FAgeFixture NoAges{ false, 0.0, 0.0 };

	FAgePolarityReplayResult Older0A;
	FAgePolarityReplayResult Older0B;
	const bool bOlder0A = RunAgePolarityReplay(TEXT("ocean_ocean_plate0_older"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllOceanic, OlderPlate0, 0, FixtureSamples, 2, 4, Older0A);
	const bool bOlder0B = RunAgePolarityReplay(TEXT("ocean_ocean_plate0_older"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllOceanic, OlderPlate0, 1, FixtureSamples, 2, 4, Older0B);

	FAgePolarityReplayResult Older1A;
	FAgePolarityReplayResult Older1B;
	const bool bOlder1A = RunAgePolarityReplay(TEXT("ocean_ocean_plate1_older"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllOceanic, OlderPlate1, 0, FixtureSamples, 2, 4, Older1A);
	const bool bOlder1B = RunAgePolarityReplay(TEXT("ocean_ocean_plate1_older"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllOceanic, OlderPlate1, 1, FixtureSamples, 2, 4, Older1B);

	FAgePolarityReplayResult EqualA;
	FAgePolarityReplayResult EqualB;
	const bool bEqualA = RunAgePolarityReplay(TEXT("ocean_ocean_equal_age_deferred"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllOceanic, EqualAges, 0, FixtureSamples, 2, 4, EqualA);
	const bool bEqualB = RunAgePolarityReplay(TEXT("ocean_ocean_equal_age_deferred"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllOceanic, EqualAges, 1, FixtureSamples, 2, 4, EqualB);

	FAgePolarityReplayResult MixedA;
	FAgePolarityReplayResult MixedB;
	const bool bMixedA = RunAgePolarityReplay(TEXT("mixed_material_regression"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::MixedPlate0Continental, OlderPlate1, 0, FixtureSamples, 2, 4, MixedA);
	const bool bMixedB = RunAgePolarityReplay(TEXT("mixed_material_regression"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::MixedPlate0Continental, OlderPlate1, 1, FixtureSamples, 2, 4, MixedB);

	FAgePolarityReplayResult CollisionA;
	FAgePolarityReplayResult CollisionB;
	const bool bCollisionA = RunAgePolarityReplay(TEXT("continental_collision_regression"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllContinental, OlderPlate0, 0, FixtureSamples, 2, 4, CollisionA);
	const bool bCollisionB = RunAgePolarityReplay(TEXT("continental_collision_regression"), ECarrierLabPhaseIIMotionFixture::ForcedConvergence, EMaterialFixture::AllContinental, OlderPlate0, 1, FixtureSamples, 2, 4, CollisionB);

	FAgePolarityReplayResult DivergenceA;
	FAgePolarityReplayResult DivergenceB;
	const bool bDivergenceA = RunAgePolarityReplay(TEXT("forced_divergence_empty"), ECarrierLabPhaseIIMotionFixture::ForcedDivergence, EMaterialFixture::AllOceanic, OlderPlate0, 0, FixtureSamples, 2, 1, DivergenceA);
	const bool bDivergenceB = RunAgePolarityReplay(TEXT("forced_divergence_empty"), ECarrierLabPhaseIIMotionFixture::ForcedDivergence, EMaterialFixture::AllOceanic, OlderPlate0, 1, FixtureSamples, 2, 1, DivergenceB);

	FAgePolarityReplayResult ZeroA;
	FAgePolarityReplayResult ZeroB;
	const bool bZeroA = RunAgePolarityReplay(TEXT("zero_motion_empty"), ECarrierLabPhaseIIMotionFixture::Zero, EMaterialFixture::Default, NoAges, 0, FixtureSamples, 40, 10, ZeroA);
	const bool bZeroB = RunAgePolarityReplay(TEXT("zero_motion_empty"), ECarrierLabPhaseIIMotionFixture::Zero, EMaterialFixture::Default, NoAges, 1, FixtureSamples, 40, 10, ZeroB);

	FString MetricsJsonl;
	MetricsJsonl += ReplayJson(Older0A) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(Older0B) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(Older1A) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(Older1B) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(EqualA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(EqualB) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(MixedA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(MixedB) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(CollisionA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(CollisionB) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(DivergenceA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(DivergenceB) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(ZeroA) + LINE_TERMINATOR;
	MetricsJsonl += ReplayJson(ZeroB) + LINE_TERMINATOR;
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);

	const FString Report = BuildReport(OutputRoot, Older0A, Older0B, Older1A, Older1B, EqualA, EqualB, MixedA, MixedB, CollisionA, CollisionB, DivergenceA, DivergenceB, ZeroA, ZeroB);
	const FString ReportPath = ResolveReportPath(Params);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.5 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.5 report: %s"), *ReportPath);

	const bool bOlder0 = bOlder0A && bOlder0B && IsAgePolarityResult(Older0A, 0, 1, 120.0, 20.0) && ReplayHashesMatch(Older0A, Older0B);
	const bool bOlder1 = bOlder1A && bOlder1B && IsAgePolarityResult(Older1A, 1, 0, 20.0, 120.0) && ReplayHashesMatch(Older1A, Older1B);
	const bool bAgeFlip = Older0A.PolarityFinal.PolarityHash != Older1A.PolarityFinal.PolarityHash &&
		Older0A.PolarityFinal.ProbeUnderPlate == 0 &&
		Older1A.PolarityFinal.ProbeUnderPlate == 1;
	const bool bEqual = bEqualA && bEqualB && IsEqualAgeDeferredResult(EqualA) && ReplayHashesMatch(EqualA, EqualB);
	const bool bMixed = bMixedA && bMixedB && IsMixedRegressionResult(MixedA) && ReplayHashesMatch(MixedA, MixedB);
	const bool bCollision = bCollisionA && bCollisionB && IsCollisionRegressionResult(CollisionA) && ReplayHashesMatch(CollisionA, CollisionB);
	const bool bDivergence = bDivergenceA && bDivergenceB && IsEmptyResult(DivergenceA) && ReplayHashesMatch(DivergenceA, DivergenceB);
	const bool bZero = bZeroA && bZeroB && IsEmptyResult(ZeroA) && ReplayHashesMatch(ZeroA, ZeroB) &&
		ZeroA.PolarityInitial.PolarityHash == ZeroA.PolarityFinal.PolarityHash;

	if (!(bOlder0 && bOlder1 && bAgeFlip && bEqual && bMixed && bCollision && bDivergence && bZero))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab Phase IIIB.5 gates failed."));
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB.5 gates passed."));
	return 0;
}
