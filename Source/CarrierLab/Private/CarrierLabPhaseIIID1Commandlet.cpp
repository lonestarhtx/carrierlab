// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIID1Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr int32 BaselineSamples = 60000;
	constexpr int32 BaselinePlates = 40;
	constexpr int32 BaselineSeed = 42;
	constexpr int32 BaselineSteps = 32;
	constexpr int32 MixedSignalSamples = 60000;
	constexpr int32 MixedSignalPlates = 2;
	constexpr int32 FixtureSamples = 10000;
	constexpr int32 FixturePlates = 2;
	constexpr int32 MaxSearchSteps = 80;
	constexpr double VelocityMmPerYear = 66.6666666667;
	constexpr double SlabPullSpeedMmPerYear = 8.0;
	constexpr double ReferenceVelocityMmPerYear = 100.0;
	constexpr TCHAR ExpectedSlice55StateHash[] = TEXT("3b4a85366dab80db");
	constexpr TCHAR ExpectedSlice55MaterialLedgerHash[] = TEXT("bc3077100ba291b4");
	constexpr TCHAR ExpectedIIIBIndependentSignature[] = TEXT("bf8818a26ed7b1dc");

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

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIID1"), Stamp);
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
				TEXT("phase-iii-slice-iiid1-report.md"));
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

	ACarrierLabVisualizationActor* SpawnActor(
		UWorld& World,
		const int32 SampleCount,
		const int32 PlateCount,
		const double ContinentalPlateFraction)
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
		Actor->Seed = BaselineSeed;
		Actor->ContinentalPlateFraction = ContinentalPlateFraction;
		Actor->VelocityMmPerYear = VelocityMmPerYear;
		Actor->MultiHitPolicy = ECarrierLabMultiHitPolicy::Centroid;
		Actor->VisualizationLayer = ECarrierLabVisualizationLayer::PlateId;
		Actor->PhaseIIICSlabPullSpeedMmPerYear = SlabPullSpeedMmPerYear;
		Actor->PhaseIIICReferenceVelocityMmPerYear = ReferenceVelocityMmPerYear;
		Actor->ConfigurePhaseIIICProcessLayer(false, false);
		Actor->bEnableNaturalResamplingEvents = false;
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	struct FSlice55BypassResult
	{
		int32 Replay = 0;
		bool bCompleted = false;
		double Seconds = 0.0;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterMetrics;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerMetrics;
		FCarrierLabPhaseIIIB7HashClosureAudit ClosureAudit;
	};

	struct FIIIBSignatureResult
	{
		int32 Replay = 0;
		FString FixtureName;
		int32 StepCount = 0;
		bool bCompleted = false;
		double Seconds = 0.0;
		double PairSignedConvergenceVelocity = 0.0;
		FCarrierLabPhaseIIIB1TrackingAudit TrackingAudit;
		FCarrierLabPhaseIIIB2DistanceAudit DistanceAudit;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit MatrixAudit;
		FCarrierLabPhaseIIIB4PolarityAudit PolarityAudit;
		FCarrierLabPhaseIIIB6NeighborPropagationAudit PropagationAudit;
		FCarrierLabPhaseIIIB7HashClosureAudit ClosureAudit;
		FString ActiveListComponentHash;
		FString DistanceComponentHash;
		FString MatrixEvidenceComponentHash;
		FString PolarityComponentHash;
		FString PropagationComponentHash;
		FString ClosureComponentHash;
		FString Slice55ComponentHash;
		FString IndependentSignatureHash;
	};

	struct FTerraneReplayResult
	{
		FString Fixture;
		int32 Replay = 0;
		bool bCompleted = false;
		int32 StepCount = 0;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit MatrixAudit;
		FCarrierLabPhaseIIIB4PolarityAudit PolarityAudit;
		FCarrierLabPhaseIIIB7HashClosureAudit ClosureAudit;
		FCarrierLabPhaseIIID1TerraneAudit TerraneAudit;
		FString ReplayHash;
	};

	bool RunSlice55Bypass(const int32 Replay, FSlice55BypassResult& OutResult)
	{
		OutResult = FSlice55BypassResult();
		OutResult.Replay = Replay;
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, BaselineSamples, BaselinePlates, 0.30);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier())
		{
			Actor->Destroy();
			return false;
		}
		Actor->ConfigurePhaseIIMotionFixture(ECarrierLabPhaseIIMotionFixture::Default);
		for (int32 Step = 0; Step < BaselineSteps; ++Step)
		{
			Actor->StepOnce();
		}

		TArray<FCarrierLabPhaseIIContactRecord> Contacts;
		TArray<FCarrierLabPhaseIITriangleLabelRecord> Labels;
		TArray<FCarrierLabPhaseIIFilterDecisionRecord> Decisions;
		FCarrierLabPhaseIITriangleLabelConfig LabelConfig;
		const bool bOk =
			Actor->DetectPhaseIIContacts(Contacts, OutResult.ContactMetrics) &&
			Actor->BuildPhaseIITriangleLabels(Contacts, LabelConfig, Labels, OutResult.LabelMetrics) &&
			Actor->ApplyPhaseIIResamplingFilterEvent(Labels, Decisions, OutResult.FilterMetrics, nullptr, &OutResult.LedgerMetrics) &&
			Actor->GetPhaseIIIB7HashClosureAudit(OutResult.ClosureAudit);

		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bCompleted = bOk;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return bOk;
	}

	void HashMixEvidence(uint64& Hash, const CarrierLab::FConvergenceSubductionMatrixEvidence& Evidence)
	{
		HashMix(Hash, static_cast<uint64>(Evidence.EvidenceId + 1));
		HashMix(Hash, static_cast<uint64>(Evidence.ContactId + 1));
		HashMix(Hash, Evidence.PairKey + 1ull);
		HashMix(Hash, static_cast<uint64>(Evidence.PlateId + 1));
		HashMix(Hash, static_cast<uint64>(Evidence.OtherPlateId + 1));
		HashMix(Hash, static_cast<uint64>(Evidence.LocalTriangleId + 1));
		HashMix(Hash, static_cast<uint64>(Evidence.OtherLocalTriangleId + 1));
		HashMixDouble(Hash, Evidence.SignedConvergenceVelocity);
		HashMix(Hash, Evidence.bAccepted ? 1ull : 0ull);
	}

	FString ComputeActiveListComponentHash(const FCarrierLabPhaseIIIB1TrackingAudit& Audit)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIIB-active-list-component-v1"));
		HashMix(Hash, static_cast<uint64>(Audit.Step + 1));
		HashMix(Hash, static_cast<uint64>(Audit.EventCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PlateCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.SourceBoundaryTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ActiveBoundaryTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.MissingBoundaryTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.NonBoundaryActiveTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.DuplicateActiveTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.InvalidActiveTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.EmptyActivePlateCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ResetSerial + 1));
		return HashToString(Hash);
	}

	FString ComputeDistanceComponentHash(const FCarrierLabPhaseIIIB2DistanceAudit& Audit)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIIB-distance-component-v1"));
		HashMix(Hash, static_cast<uint64>(Audit.Step + 1));
		HashMix(Hash, static_cast<uint64>(Audit.EventCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PlateCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.SourceBoundaryTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ActiveBoundaryTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.DistanceRecordCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.MissingDistanceRecordCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.NonFiniteDistanceCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.NegativeDistanceCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.OverThresholdActiveTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.DistanceCulledTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.EmptyActivePlateCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ResetSerial + 1));
		HashMixDouble(Hash, Audit.DistanceThresholdKm);
		HashMixDouble(Hash, Audit.MinDistanceKm);
		HashMixDouble(Hash, Audit.MeanDistanceKm);
		HashMixDouble(Hash, Audit.MaxDistanceKm);
		HashMix(Hash, static_cast<uint64>(Audit.ProbePlateId + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ProbeLocalTriangleId + 1));
		HashMixDouble(Hash, Audit.ProbeDistanceKm);
		HashMixDouble(Hash, Audit.ProbeStepDistanceKm);
		return HashToString(Hash);
	}

	FString ComputeMatrixEvidenceComponentHash(const FCarrierLabPhaseIIIB3SubductionMatrixAudit& Audit, const double PairSignedVelocity)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIIB-matrix-evidence-component-v1"));
		HashMix(Hash, static_cast<uint64>(Audit.Step + 1));
		HashMix(Hash, static_cast<uint64>(Audit.EventCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PlateCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ResetSerial + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ActiveBoundaryTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.DistanceRecordCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.MatrixPairCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.InvalidMatrixPairCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.SelfMatrixPairCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.RayTestCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.HitCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.BoundaryHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.NonConvergentHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.AcceptedLocalPositiveHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.RejectedLocalNonPositiveHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ProbePlateA + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ProbePlateB + 1));
		HashMixDouble(Hash, PairSignedVelocity);
		HashMixDouble(Hash, Audit.ProbeSignedConvergenceVelocity);
		HashMixString(Hash, Audit.MatrixEvidenceHash);
		HashMix(Hash, static_cast<uint64>(Audit.AcceptedEvidence.Num() + 1));
		for (const CarrierLab::FConvergenceSubductionMatrixEvidence& Evidence : Audit.AcceptedEvidence)
		{
			HashMixEvidence(Hash, Evidence);
		}
		HashMix(Hash, static_cast<uint64>(Audit.RejectedEvidence.Num() + 1));
		for (const CarrierLab::FConvergenceSubductionMatrixEvidence& Evidence : Audit.RejectedEvidence)
		{
			HashMixEvidence(Hash, Evidence);
		}
		return HashToString(Hash);
	}

	FString ComputePolarityComponentHash(const FCarrierLabPhaseIIIB4PolarityAudit& Audit)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIIB-polarity-component-v1"));
		HashMix(Hash, static_cast<uint64>(Audit.Step + 1));
		HashMix(Hash, static_cast<uint64>(Audit.EventCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PlateCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ResetSerial + 1));
		HashMix(Hash, static_cast<uint64>(Audit.MatrixPairCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.DecisionCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.OceanicUnderContinentalCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.CollisionCandidateCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.OceanOceanDeferredCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.OlderOceanicUnderYoungerOceanicCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.InvalidDecisionCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.MissingDecisionCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.SubductionPolarityCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ProbePlateA + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ProbePlateB + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ProbeUnderPlate + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ProbeOverPlate + 1));
		HashMixDouble(Hash, Audit.ProbePlateAContinentalFraction);
		HashMixDouble(Hash, Audit.ProbePlateBContinentalFraction);
		HashMixDouble(Hash, Audit.ProbePlateAOceanicAge);
		HashMixDouble(Hash, Audit.ProbePlateBOceanicAge);
		HashMix(Hash, static_cast<uint64>(Audit.ProbeDecisionClass) + 1ull);
		HashMix(Hash, static_cast<uint64>(Audit.Decisions.Num() + 1));
		for (const FCarrierLabPhaseIIIB4PolarityDecisionAudit& Decision : Audit.Decisions)
		{
			HashMix(Hash, Decision.PairKey + 1ull);
			HashMix(Hash, static_cast<uint64>(Decision.PlateA + 1));
			HashMix(Hash, static_cast<uint64>(Decision.PlateB + 1));
			HashMix(Hash, static_cast<uint64>(Decision.UnderPlate + 1));
			HashMix(Hash, static_cast<uint64>(Decision.OverPlate + 1));
			HashMixDouble(Hash, Decision.PlateAContinentalFraction);
			HashMixDouble(Hash, Decision.PlateBContinentalFraction);
			HashMixDouble(Hash, Decision.PlateAOceanicAge);
			HashMixDouble(Hash, Decision.PlateBOceanicAge);
			HashMix(Hash, static_cast<uint64>(Decision.DecisionClass) + 1ull);
		}
		return HashToString(Hash);
	}

	FString ComputePropagationComponentHash(const FCarrierLabPhaseIIIB6NeighborPropagationAudit& Audit)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIIB-propagation-component-v1"));
		HashMix(Hash, static_cast<uint64>(Audit.Step + 1));
		HashMix(Hash, static_cast<uint64>(Audit.EventCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PlateCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ResetSerial + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ActiveTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.TotalPlateLocalTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.DistanceRecordCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.NonBoundaryActiveTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.OverThresholdActiveTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PropagationSeedHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PropagationAddedCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PropagationDuplicateCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PropagationDistanceRejectedCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PropagationInvalidCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ActivePlateCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.MaxActiveTrianglesOnPlate + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ProbePlateId + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ProbeLocalTriangleId + 1));
		HashMix(Hash, static_cast<uint64>(Audit.SeedEvidenceId + 1));
		HashMix(Hash, static_cast<uint64>(Audit.SeedPlateId + 1));
		HashMix(Hash, static_cast<uint64>(Audit.SeedOtherPlateId + 1));
		HashMix(Hash, static_cast<uint64>(Audit.SeedLocalTriangleId + 1));
		HashMixDouble(Hash, Audit.SeedSignedConvergenceVelocity);
		HashMixDouble(Hash, Audit.ProbeDistanceKm);
		HashMixDouble(Hash, Audit.DistanceThresholdKm);
		HashMixDouble(Hash, Audit.MaxDistanceKm);
		return HashToString(Hash);
	}

	FString ComputeClosureComponentHash(const FCarrierLabPhaseIIIB7HashClosureAudit& Audit)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIIB-closure-component-v1"));
		HashMix(Hash, static_cast<uint64>(Audit.Step + 1));
		HashMix(Hash, static_cast<uint64>(Audit.EventCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.SampleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PlateCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ResetSerial + 1));
		HashMix(Hash, static_cast<uint64>(Audit.ActiveTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.DistanceRecordCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.MatrixPairCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.MatrixRayTestCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.MatrixHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.MatrixBoundaryHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.MatrixNonConvergentHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PolarityDecisionCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.TriangleHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PropagationSeedHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PropagationAddedCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PropagationDuplicateCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PropagationDistanceRejectedCount + 1));
		HashMix(Hash, static_cast<uint64>(Audit.PropagationInvalidCount + 1));
		HashMixString(Hash, Audit.ProjectionHash);
		HashMixString(Hash, Audit.StateHash);
		HashMixString(Hash, Audit.CrustStateHash);
		HashMixString(Hash, Audit.MetricsConvergenceTrackingHash);
		HashMixString(Hash, Audit.ComputedConvergenceTrackingHash);
		HashMix(Hash, Audit.bMetricsHashMatchesComputed ? 1ull : 0ull);
		return HashToString(Hash);
	}

	FString ComputeSlice55ComponentHash(const FSlice55BypassResult& A, const FSlice55BypassResult& B)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIIB-slice55-regression-component-v1"));
		const FSlice55BypassResult* Results[] = { &A, &B };
		for (const FSlice55BypassResult* Result : Results)
		{
			HashMix(Hash, static_cast<uint64>(Result->Replay + 1));
			HashMix(Hash, Result->bCompleted ? 1ull : 0ull);
			HashMixString(Hash, Result->ContactMetrics.ContactLogHash);
			HashMixString(Hash, Result->LabelMetrics.TriangleLabelHash);
			HashMixString(Hash, Result->FilterMetrics.FilterDecisionHash);
			HashMixString(Hash, Result->FilterMetrics.StateHashAfter);
			HashMixString(Hash, Result->LedgerMetrics.MaterialLedgerHash);
		}
		HashMixString(Hash, ExpectedSlice55StateHash);
		HashMixString(Hash, ExpectedSlice55MaterialLedgerHash);
		return HashToString(Hash);
	}

	void FinalizeIIIBIndependentSignature(
		const FSlice55BypassResult& BypassA,
		const FSlice55BypassResult& BypassB,
		FIIIBSignatureResult& Result)
	{
		Result.ActiveListComponentHash = ComputeActiveListComponentHash(Result.TrackingAudit);
		Result.DistanceComponentHash = ComputeDistanceComponentHash(Result.DistanceAudit);
		Result.MatrixEvidenceComponentHash = ComputeMatrixEvidenceComponentHash(Result.MatrixAudit, Result.PairSignedConvergenceVelocity);
		Result.PolarityComponentHash = ComputePolarityComponentHash(Result.PolarityAudit);
		Result.PropagationComponentHash = ComputePropagationComponentHash(Result.PropagationAudit);
		Result.ClosureComponentHash = ComputeClosureComponentHash(Result.ClosureAudit);
		Result.Slice55ComponentHash = ComputeSlice55ComponentHash(BypassA, BypassB);

		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIIB-independent-signature-v1"));
		HashMixString(Hash, Result.FixtureName);
		HashMix(Hash, static_cast<uint64>(Result.StepCount + 1));
		HashMixString(Hash, Result.Slice55ComponentHash);
		HashMixString(Hash, Result.ActiveListComponentHash);
		HashMixString(Hash, Result.DistanceComponentHash);
		HashMixString(Hash, Result.MatrixEvidenceComponentHash);
		HashMixString(Hash, Result.PolarityComponentHash);
		HashMixString(Hash, Result.PropagationComponentHash);
		HashMixString(Hash, Result.ClosureComponentHash);
		Result.IndependentSignatureHash = HashToString(Hash);
	}

	bool CaptureIIIBSignatureAudits(const ACarrierLabVisualizationActor& Actor, FIIIBSignatureResult& OutResult)
	{
		return Actor.GetPhaseIIIB1TrackingAudit(OutResult.TrackingAudit) &&
			Actor.GetPhaseIIIB2DistanceAudit(OutResult.DistanceAudit) &&
			Actor.GetPhaseIIIB3SubductionMatrixAudit(OutResult.MatrixAudit) &&
			Actor.GetPhaseIIIB4PolarityAudit(OutResult.PolarityAudit) &&
			Actor.GetPhaseIIIB6NeighborPropagationAudit(OutResult.PropagationAudit) &&
			Actor.GetPhaseIIIB7HashClosureAudit(OutResult.ClosureAudit);
	}

	bool RunIIIBLocalVsPairSignatureReplay(
		const int32 Replay,
		const FSlice55BypassResult& BypassA,
		const FSlice55BypassResult& BypassB,
		FIIIBSignatureResult& OutResult)
	{
		OutResult = FIIIBSignatureResult();
		OutResult.Replay = Replay;
		OutResult.FixtureName = TEXT("local_vs_pair_discriminator");
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, MixedSignalSamples, MixedSignalPlates, 0.50);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier())
		{
			Actor->Destroy();
			return false;
		}
		Actor->ConfigurePhaseIIMotionFixture(ECarrierLabPhaseIIMotionFixture::ForcedDivergence);
		if (!Actor->SetPlateContinentalForTest(0, true) ||
			!Actor->SetPlateContinentalForTest(1, false))
		{
			Actor->Destroy();
			return false;
		}

		for (int32 Step = 0; Step <= MaxSearchSteps; ++Step)
		{
			if (Step > 0)
			{
				Actor->StepOnce();
			}

			FIIIBSignatureResult Candidate;
			Candidate.Replay = Replay;
			Candidate.FixtureName = OutResult.FixtureName;
			Candidate.StepCount = Step;
			Candidate.PairSignedConvergenceVelocity = Actor->ComputePhaseIIPairSignedConvergenceVelocity(0, 1);
			if (!CaptureIIIBSignatureAudits(*Actor, Candidate))
			{
				Actor->Destroy();
				return false;
			}

			const bool bDiscriminates =
				Candidate.PairSignedConvergenceVelocity <= 0.0 &&
				Candidate.MatrixAudit.AcceptedLocalPositiveHitCount > 0 &&
				Candidate.MatrixAudit.RejectedLocalNonPositiveHitCount > 0 &&
				Candidate.MatrixAudit.MatrixPairCount == 1 &&
				Candidate.MatrixAudit.ProbePlateA == 0 &&
				Candidate.MatrixAudit.ProbePlateB == 1 &&
				Candidate.PolarityAudit.DecisionCount == 1 &&
				Candidate.PropagationAudit.PropagationSeedHitCount > 0 &&
				Candidate.PropagationAudit.PropagationAddedCount > 0 &&
				Candidate.ClosureAudit.bMetricsHashMatchesComputed;
			if (bDiscriminates)
			{
				FinalizeIIIBIndependentSignature(BypassA, BypassB, Candidate);
				OutResult = Candidate;
				OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
				OutResult.bCompleted = true;
				break;
			}
		}

		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bCompleted;
	}

	bool IIIBSignatureReplayStable(const FIIIBSignatureResult& A, const FIIIBSignatureResult& B)
	{
		return A.bCompleted && B.bCompleted &&
			A.FixtureName == B.FixtureName &&
			A.IndependentSignatureHash == B.IndependentSignatureHash &&
			A.ActiveListComponentHash == B.ActiveListComponentHash &&
			A.DistanceComponentHash == B.DistanceComponentHash &&
			A.MatrixEvidenceComponentHash == B.MatrixEvidenceComponentHash &&
			A.PolarityComponentHash == B.PolarityComponentHash &&
			A.PropagationComponentHash == B.PropagationComponentHash &&
			A.ClosureComponentHash == B.ClosureComponentHash &&
			A.Slice55ComponentHash == B.Slice55ComponentHash &&
			A.ClosureAudit.ComputedConvergenceTrackingHash == B.ClosureAudit.ComputedConvergenceTrackingHash;
	}

	bool IIIBIndependentSignaturePasses(const FIIIBSignatureResult& A, const FIIIBSignatureResult& B)
	{
		return IIIBSignatureReplayStable(A, B) &&
			A.IndependentSignatureHash == ExpectedIIIBIndependentSignature &&
			B.IndependentSignatureHash == ExpectedIIIBIndependentSignature &&
			A.PairSignedConvergenceVelocity <= 0.0 &&
			A.MatrixAudit.AcceptedLocalPositiveHitCount > 0 &&
			A.MatrixAudit.RejectedLocalNonPositiveHitCount > 0 &&
			A.MatrixAudit.MatrixPairCount == 1 &&
			A.PolarityAudit.DecisionCount == 1 &&
			A.PropagationAudit.PropagationSeedHitCount > 0 &&
			A.PropagationAudit.PropagationAddedCount > 0 &&
			A.ClosureAudit.bMetricsHashMatchesComputed &&
			B.ClosureAudit.bMetricsHashMatchesComputed;
	}

	FString ComputeTerraneReplayHash(const FTerraneReplayResult& Result)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIID1-terrane-replay-v1"));
		HashMixString(Hash, Result.Fixture);
		HashMix(Hash, Result.bCompleted ? 1ull : 0ull);
		HashMix(Hash, static_cast<uint64>(Result.StepCount + 1));
		HashMixString(Hash, Result.MatrixAudit.MatrixEvidenceHash);
		HashMixString(Hash, Result.PolarityAudit.PolarityHash);
		HashMixString(Hash, Result.ClosureAudit.ComputedConvergenceTrackingHash);
		HashMixString(Hash, Result.TerraneAudit.TerraneDetectionHash);
		HashMix(Hash, static_cast<uint64>(Result.TerraneAudit.CollisionCandidateHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.TerraneAudit.TerraneRecordCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.TerraneAudit.TotalTerraneTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.TerraneAudit.TotalInnerSeaTriangleCount + 1));
		return HashToString(Hash);
	}

	bool RunTerraneReplay(
		const int32 Replay,
		const FString& Fixture,
		const bool bContinentalPlate0,
		const bool bContinentalPlate1,
		const bool bExpectCollisionTerranes,
		FTerraneReplayResult& OutResult)
	{
		OutResult = FTerraneReplayResult();
		OutResult.Replay = Replay;
		OutResult.Fixture = Fixture;
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, FixtureSamples, FixturePlates, 0.50);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier())
		{
			Actor->Destroy();
			return false;
		}
		Actor->ConfigurePhaseIIMotionFixture(ECarrierLabPhaseIIMotionFixture::ForcedConvergence);
		if (!Actor->SetPlateContinentalForTest(0, bContinentalPlate0) ||
			!Actor->SetPlateContinentalForTest(1, bContinentalPlate1))
		{
			Actor->Destroy();
			return false;
		}

		for (int32 Step = 0; Step <= MaxSearchSteps; ++Step)
		{
			if (Step > 0)
			{
				Actor->StepOnce();
			}
			if (!Actor->GetPhaseIIIB3SubductionMatrixAudit(OutResult.MatrixAudit) ||
				!Actor->GetPhaseIIIB4PolarityAudit(OutResult.PolarityAudit) ||
				!Actor->GetPhaseIIIB7HashClosureAudit(OutResult.ClosureAudit) ||
				!Actor->DetectPhaseIIID1ConnectedTerranes(OutResult.TerraneAudit))
			{
				Actor->Destroy();
				return false;
			}

			const bool bFoundCollisionTerranes =
				OutResult.PolarityAudit.CollisionCandidateCount > 0 &&
				OutResult.TerraneAudit.TerraneRecordCount > 0;
			const bool bFoundNonCollisionEvidence =
				OutResult.MatrixAudit.HitCount > 0 &&
				OutResult.PolarityAudit.DecisionCount > 0 &&
				OutResult.TerraneAudit.TerraneRecordCount == 0;
			if ((bExpectCollisionTerranes && bFoundCollisionTerranes) ||
				(!bExpectCollisionTerranes && bFoundNonCollisionEvidence))
			{
				OutResult.StepCount = Step;
				OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
				OutResult.bCompleted = true;
				OutResult.ReplayHash = ComputeTerraneReplayHash(OutResult);
				break;
			}
		}

		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bCompleted;
	}

	bool TerraneReplayStable(const FTerraneReplayResult& A, const FTerraneReplayResult& B)
	{
		return A.bCompleted && B.bCompleted &&
			A.Fixture == B.Fixture &&
			A.StepCount == B.StepCount &&
			A.ReplayHash == B.ReplayHash &&
			A.MatrixAudit.MatrixEvidenceHash == B.MatrixAudit.MatrixEvidenceHash &&
			A.PolarityAudit.PolarityHash == B.PolarityAudit.PolarityHash &&
			A.ClosureAudit.ComputedConvergenceTrackingHash == B.ClosureAudit.ComputedConvergenceTrackingHash &&
			A.TerraneAudit.TerraneDetectionHash == B.TerraneAudit.TerraneDetectionHash &&
			A.TerraneAudit.TerraneRecordCount == B.TerraneAudit.TerraneRecordCount &&
			A.TerraneAudit.TotalTerraneTriangleCount == B.TerraneAudit.TotalTerraneTriangleCount;
	}

	bool CollisionTerranePasses(const FTerraneReplayResult& A, const FTerraneReplayResult& B)
	{
		if (!TerraneReplayStable(A, B) ||
			A.PolarityAudit.CollisionCandidateCount <= 0 ||
			A.TerraneAudit.CollisionCandidateHitCount <= 0 ||
			A.TerraneAudit.TerraneRecordCount <= 0 ||
			A.TerraneAudit.TerraneRecordCount != A.TerraneAudit.CollisionCandidateHitCount ||
			A.TerraneAudit.InvalidSeedCount != 0 ||
			A.TerraneAudit.NonCollisionDecisionHitCount != 0 ||
			A.TerraneAudit.NonContinentalSeedCount != 0 ||
			A.TerraneAudit.EmptyTerraneCount != 0 ||
			A.TerraneAudit.TotalInnerSeaTriangleCount != 0)
		{
			return false;
		}

		for (const FCarrierLabPhaseIIID1TerraneRecord& Record : A.TerraneAudit.Records)
		{
			if (Record.TriangleCount <= 0 ||
				Record.ContinentalTriangleCount != Record.TriangleCount ||
				Record.MeanContinentalFraction < 0.999999)
			{
				return false;
			}
		}
		return true;
	}

	bool PureOceanicNegativePasses(const FTerraneReplayResult& A, const FTerraneReplayResult& B)
	{
		return TerraneReplayStable(A, B) &&
			A.MatrixAudit.HitCount > 0 &&
			A.PolarityAudit.DecisionCount > 0 &&
			A.PolarityAudit.CollisionCandidateCount == 0 &&
			A.TerraneAudit.CollisionCandidateHitCount == 0 &&
			A.TerraneAudit.TerraneRecordCount == 0 &&
			A.TerraneAudit.InvalidSeedCount == 0 &&
			A.TerraneAudit.NonContinentalSeedCount == 0;
	}

	void AppendTerraneJson(const FTerraneReplayResult& Result, TArray<FString>& Lines)
	{
		Lines.Add(FString::Printf(
			TEXT("{\"kind\":\"terrane_replay\",\"fixture\":%s,\"replay\":%d,\"completed\":%s,\"step\":%d,\"matrix_hits\":%d,\"collision_candidates\":%d,\"terrane_records\":%d,\"total_triangles\":%d,\"inner_sea_triangles\":%d,\"invalid_seeds\":%d,\"non_collision_hits\":%d,\"hash\":%s,\"terrane_hash\":%s,\"seconds\":%.6f}"),
			*JsonString(Result.Fixture),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.StepCount,
			Result.MatrixAudit.HitCount,
			Result.PolarityAudit.CollisionCandidateCount,
			Result.TerraneAudit.TerraneRecordCount,
			Result.TerraneAudit.TotalTerraneTriangleCount,
			Result.TerraneAudit.TotalInnerSeaTriangleCount,
			Result.TerraneAudit.InvalidSeedCount,
			Result.TerraneAudit.NonCollisionDecisionHitCount,
			*JsonString(Result.ReplayHash),
			*JsonString(Result.TerraneAudit.TerraneDetectionHash),
			Result.Seconds));
	}

	FString FirstRecordSummary(const FCarrierLabPhaseIIID1TerraneAudit& Audit)
	{
		if (Audit.Records.IsEmpty())
		{
			return TEXT("none");
		}
		const FCarrierLabPhaseIIID1TerraneRecord& Record = Audit.Records[0];
		return FString::Printf(
			TEXT("plate %d seed %d triangles %d vertices %d mean_xC %.6f hash `%s`"),
			Record.SourcePlateId,
			Record.SeedLocalTriangleId,
			Record.TriangleCount,
			Record.VertexCount,
			Record.MeanContinentalFraction,
			*Record.TerraneHash);
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FSlice55BypassResult& BypassA,
		const FSlice55BypassResult& BypassB,
		const FIIIBSignatureResult& IIIBSignatureA,
		const FIIIBSignatureResult& IIIBSignatureB,
		const FTerraneReplayResult& CollisionA,
		const FTerraneReplayResult& CollisionB,
		const FTerraneReplayResult& OceanA,
		const FTerraneReplayResult& OceanB)
	{
		const bool bSlice55Pass =
			BypassA.bCompleted &&
			BypassB.bCompleted &&
			BypassA.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			BypassB.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			BypassA.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
			BypassB.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash;
		const bool bIIIBPass = IIIBIndependentSignaturePasses(IIIBSignatureA, IIIBSignatureB);
		const bool bCollisionPass = CollisionTerranePasses(CollisionA, CollisionB);
		const bool bOceanPass = PureOceanicNegativePasses(OceanA, OceanB);
		const bool bAllPass = bSlice55Pass && bIIIBPass && bCollisionPass && bOceanPass;

		FString Report;
		Report += TEXT("# Phase III Slice IIID.1 Report - Connected Continental Terrane Detection\n\n");
		Report += FString::Printf(TEXT("Generated: `%s` UTC\n\n"), *FDateTime::UtcNow().ToString(TEXT("%Y-%m-%d %H:%M:%S")));
		Report += TEXT("## Scope\n\n");
		Report += TEXT("IIID.1 adds read-only connected terrane detection from existing IIIB collision-candidate evidence. It does not detach terranes, mutate topology, apply sutures, change plate motion, resample, or alter carrier authority.\n\n");
		Report += FString::Printf(TEXT("Metrics: `%s`\n\n"), *FPaths::Combine(OutputRoot, TEXT("metrics.jsonl")));

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---:|---|\n");
		Report += FString::Printf(
			TEXT("| Slice 5.5 fixed-fixture regression | %s | state `%s` / `%s`, ledger `%s` / `%s` |\n"),
			*PassFail(bSlice55Pass),
			*BypassA.FilterMetrics.StateHashAfter,
			*BypassB.FilterMetrics.StateHashAfter,
			*BypassA.LedgerMetrics.MaterialLedgerHash,
			*BypassB.LedgerMetrics.MaterialLedgerHash);
		Report += FString::Printf(
			TEXT("| IIIB independent signature regression | %s | replay A `%s`, replay B `%s`, expected `%s` |\n"),
			*PassFail(bIIIBPass),
			*IIIBSignatureA.IndependentSignatureHash,
			*IIIBSignatureB.IndependentSignatureHash,
			ExpectedIIIBIndependentSignature);
		Report += FString::Printf(
			TEXT("| Forced collision terrane determinism | %s | hash A `%s`, hash B `%s`, records %d / %d |\n"),
			*PassFail(bCollisionPass),
			*CollisionA.TerraneAudit.TerraneDetectionHash,
			*CollisionB.TerraneAudit.TerraneDetectionHash,
			CollisionA.TerraneAudit.TerraneRecordCount,
			CollisionB.TerraneAudit.TerraneRecordCount);
		Report += FString::Printf(
			TEXT("| Pure-oceanic negative | %s | matrix hits %d, decisions %d, terrane records %d |\n\n"),
			*PassFail(bOceanPass),
			OceanA.MatrixAudit.HitCount,
			OceanA.PolarityAudit.DecisionCount,
			OceanA.TerraneAudit.TerraneRecordCount);

		Report += TEXT("## Fixture Results\n\n");
		Report += TEXT("| Fixture | Replay | Step | Matrix hits | Collision candidates | Terrane records | Total triangles | Inner sea triangles | Invalid seeds | Non-collision hits | Hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		const FTerraneReplayResult* Results[] = { &CollisionA, &CollisionB, &OceanA, &OceanB };
		for (const FTerraneReplayResult* Result : Results)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d | %d | %d | %d | %d | `%s` |\n"),
				*Result->Fixture,
				Result->Replay,
				Result->StepCount,
				Result->MatrixAudit.HitCount,
				Result->PolarityAudit.CollisionCandidateCount,
				Result->TerraneAudit.TerraneRecordCount,
				Result->TerraneAudit.TotalTerraneTriangleCount,
				Result->TerraneAudit.TotalInnerSeaTriangleCount,
				Result->TerraneAudit.InvalidSeedCount,
				Result->TerraneAudit.NonCollisionDecisionHitCount,
				*Result->TerraneAudit.TerraneDetectionHash);
		}

		Report += TEXT("\n## Representative Terrane\n\n");
		Report += FString::Printf(TEXT("- Forced collision replay A: %s\n"), *FirstRecordSummary(CollisionA.TerraneAudit));
		Report += FString::Printf(TEXT("- Forced collision replay B: %s\n\n"), *FirstRecordSummary(CollisionB.TerraneAudit));

		Report += TEXT("## Interpretation\n\n");
		Report += TEXT("The all-continental forced-collision fixture exercises the IIIB `CollisionCandidate` path and traverses the source plate's plate-local triangle connectivity from each candidate triangle. Since both fixture plates are entirely continental, every emitted terrane is expected to contain only continental triangles and no inner-sea additions. The pure-oceanic fixture proves that accepted convergence evidence alone does not fabricate terranes when polarity is not continental collision.\n\n");
		Report += TEXT("Inner-sea inclusion is implemented in the traversal: enclosed oceanic triangle regions that do not touch a plate boundary and whose continental neighbors all belong to the detected component are included in the terrane. This fixture has no inner seas, so the count is expected to remain zero.\n\n");
		Report += TEXT("## Verdict\n\n");
		Report += bAllPass
			? TEXT("PASS. IIID.1 connected terrane detection is deterministic and read-only for the exercised fixtures. IIID.2 may group collision candidates without treating this slice as topology mutation.\n")
			: TEXT("FAIL. Do not advance to IIID.2 until the failing gate is investigated.\n");
		return Report;
	}
}

UCarrierLabPhaseIIID1Commandlet::UCarrierLabPhaseIIID1Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIID1Commandlet::Main(const FString& Params)
{
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FSlice55BypassResult BypassA;
	FSlice55BypassResult BypassB;
	const bool bBypassA = RunSlice55Bypass(0, BypassA);
	const bool bBypassB = RunSlice55Bypass(1, BypassB);

	FIIIBSignatureResult IIIBSignatureA;
	FIIIBSignatureResult IIIBSignatureB;
	const bool bIIIBSignatureA = RunIIIBLocalVsPairSignatureReplay(0, BypassA, BypassB, IIIBSignatureA);
	const bool bIIIBSignatureB = RunIIIBLocalVsPairSignatureReplay(1, BypassA, BypassB, IIIBSignatureB);

	FTerraneReplayResult CollisionA;
	FTerraneReplayResult CollisionB;
	FTerraneReplayResult OceanA;
	FTerraneReplayResult OceanB;
	const bool bCollisionA = RunTerraneReplay(0, TEXT("forced_collision_all_continental"), true, true, true, CollisionA);
	const bool bCollisionB = RunTerraneReplay(1, TEXT("forced_collision_all_continental"), true, true, true, CollisionB);
	const bool bOceanA = RunTerraneReplay(0, TEXT("pure_oceanic_negative"), false, false, false, OceanA);
	const bool bOceanB = RunTerraneReplay(1, TEXT("pure_oceanic_negative"), false, false, false, OceanB);

	TArray<FString> JsonLines;
	JsonLines.Add(FString::Printf(
		TEXT("{\"kind\":\"slice55_bypass\",\"replay\":0,\"completed\":%s,\"contact_hash\":%s,\"label_hash\":%s,\"filter_decision_hash\":%s,\"projection_hash\":%s,\"state_hash\":%s,\"crust_hash\":%s,\"material_ledger_hash\":%s,\"convergence_hash\":%s,\"no_boundary_pair_miss_count\":%d,\"seconds\":%.6f}"),
		bBypassA ? TEXT("true") : TEXT("false"),
		*JsonString(BypassA.ContactMetrics.ContactLogHash),
		*JsonString(BypassA.LabelMetrics.TriangleLabelHash),
		*JsonString(BypassA.FilterMetrics.FilterDecisionHash),
		*JsonString(BypassA.FilterMetrics.ProjectionHashAfter),
		*JsonString(BypassA.FilterMetrics.StateHashAfter),
		*JsonString(BypassA.ClosureAudit.CrustStateHash),
		*JsonString(BypassA.LedgerMetrics.MaterialLedgerHash),
		*JsonString(BypassA.ClosureAudit.ComputedConvergenceTrackingHash),
		BypassA.FilterMetrics.NoBoundaryPairMissCount,
		BypassA.Seconds));
	JsonLines.Add(FString::Printf(
		TEXT("{\"kind\":\"slice55_bypass\",\"replay\":1,\"completed\":%s,\"contact_hash\":%s,\"label_hash\":%s,\"filter_decision_hash\":%s,\"projection_hash\":%s,\"state_hash\":%s,\"crust_hash\":%s,\"material_ledger_hash\":%s,\"convergence_hash\":%s,\"no_boundary_pair_miss_count\":%d,\"seconds\":%.6f}"),
		bBypassB ? TEXT("true") : TEXT("false"),
		*JsonString(BypassB.ContactMetrics.ContactLogHash),
		*JsonString(BypassB.LabelMetrics.TriangleLabelHash),
		*JsonString(BypassB.FilterMetrics.FilterDecisionHash),
		*JsonString(BypassB.FilterMetrics.ProjectionHashAfter),
		*JsonString(BypassB.FilterMetrics.StateHashAfter),
		*JsonString(BypassB.ClosureAudit.CrustStateHash),
		*JsonString(BypassB.LedgerMetrics.MaterialLedgerHash),
		*JsonString(BypassB.ClosureAudit.ComputedConvergenceTrackingHash),
		BypassB.FilterMetrics.NoBoundaryPairMissCount,
		BypassB.Seconds));
	JsonLines.Add(FString::Printf(
		TEXT("{\"kind\":\"iiib_signature\",\"replay\":0,\"completed\":%s,\"expected\":%s,\"actual\":%s,\"step\":%d,\"slice55_component\":%s,\"closure_component\":%s,\"matrix_component\":%s,\"seconds\":%.6f}"),
		bIIIBSignatureA ? TEXT("true") : TEXT("false"),
		*JsonString(ExpectedIIIBIndependentSignature),
		*JsonString(IIIBSignatureA.IndependentSignatureHash),
		IIIBSignatureA.StepCount,
		*JsonString(IIIBSignatureA.Slice55ComponentHash),
		*JsonString(IIIBSignatureA.ClosureComponentHash),
		*JsonString(IIIBSignatureA.MatrixEvidenceComponentHash),
		IIIBSignatureA.Seconds));
	JsonLines.Add(FString::Printf(
		TEXT("{\"kind\":\"iiib_signature\",\"replay\":1,\"completed\":%s,\"expected\":%s,\"actual\":%s,\"step\":%d,\"slice55_component\":%s,\"closure_component\":%s,\"matrix_component\":%s,\"seconds\":%.6f}"),
		bIIIBSignatureB ? TEXT("true") : TEXT("false"),
		*JsonString(ExpectedIIIBIndependentSignature),
		*JsonString(IIIBSignatureB.IndependentSignatureHash),
		IIIBSignatureB.StepCount,
		*JsonString(IIIBSignatureB.Slice55ComponentHash),
		*JsonString(IIIBSignatureB.ClosureComponentHash),
		*JsonString(IIIBSignatureB.MatrixEvidenceComponentHash),
		IIIBSignatureB.Seconds));
	AppendTerraneJson(CollisionA, JsonLines);
	AppendTerraneJson(CollisionB, JsonLines);
	AppendTerraneJson(OceanA, JsonLines);
	AppendTerraneJson(OceanB, JsonLines);

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(FString::Join(JsonLines, TEXT("\n")) + TEXT("\n"), *MetricsPath);

	const FString Report = BuildReport(OutputRoot, BypassA, BypassB, IIIBSignatureA, IIIBSignatureB, CollisionA, CollisionB, OceanA, OceanB);
	const FString ReportPath = ResolveReportPath(Params);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	const bool bAllPass =
		bBypassA &&
		bBypassB &&
		bIIIBSignatureA &&
		bIIIBSignatureB &&
		bCollisionA &&
		bCollisionB &&
		bOceanA &&
		bOceanB &&
		BypassA.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
		BypassB.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
		BypassA.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
		BypassB.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
		IIIBIndependentSignaturePasses(IIIBSignatureA, IIIBSignatureB) &&
		CollisionTerranePasses(CollisionA, CollisionB) &&
		PureOceanicNegativePasses(OceanA, OceanB);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIID.1 report: %s"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIID.1 metrics: %s"), *MetricsPath);
	return bAllPass ? 0 : 1;
}
