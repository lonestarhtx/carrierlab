// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIBConsolidationCommandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
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
	constexpr int32 MixedSignalSteps = 40;
	constexpr int32 LocalDiscriminatorMaxSteps = 80;
	constexpr double VelocityMmPerYear = 66.6666666667;
	constexpr TCHAR ExpectedSlice55StateHash[] = TEXT("3b4a85366dab80db");
	constexpr TCHAR ExpectedSlice55MaterialLedgerHash[] = TEXT("bc3077100ba291b4");
	constexpr TCHAR HistoricalIIIBSmokeToken[] = TEXT("df36a5bc9e8f175e");

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

	void HashMix(uint64& Hash, const uint64 Value)
	{
		Hash ^= Value;
		Hash *= 1099511628211ull;
	}

	void HashMixString(uint64& Hash, const FString& Value)
	{
		HashMix(Hash, static_cast<uint64>(Value.Len() + 1));
		for (const TCHAR Ch : Value)
		{
			HashMix(Hash, static_cast<uint64>(Ch) + 1ull);
		}
	}

	void HashMixDouble(uint64& Hash, const double Value)
	{
		HashMix(Hash, static_cast<uint64>(FMath::RoundToInt64(Value * 1000000000.0)));
	}

	FString HashToString(const uint64 Hash)
	{
		return FString::Printf(TEXT("%016llx"), static_cast<unsigned long long>(Hash));
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIIBConsolidation"), Stamp);
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
				TEXT("phase-iii-iiib-consolidated.md"));
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

	ACarrierLabVisualizationActor* SpawnActor(UWorld& World, const int32 SampleCount, const int32 PlateCount, const double ContinentalPlateFraction)
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
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	struct FSlice55BaselineResult
	{
		int32 Replay = 0;
		bool bCompleted = false;
		double TotalSeconds = 0.0;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterMetrics;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerMetrics;
	};

	bool RunSlice55Baseline(const int32 Replay, FSlice55BaselineResult& OutResult)
	{
		OutResult = FSlice55BaselineResult();
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
		if (!Actor->DetectPhaseIIContacts(Contacts, OutResult.ContactMetrics))
		{
			Actor->Destroy();
			return false;
		}

		FCarrierLabPhaseIITriangleLabelConfig LabelConfig;
		TArray<FCarrierLabPhaseIITriangleLabelRecord> Labels;
		if (!Actor->BuildPhaseIITriangleLabels(Contacts, LabelConfig, Labels, OutResult.LabelMetrics))
		{
			Actor->Destroy();
			return false;
		}

		TArray<FCarrierLabPhaseIIFilterDecisionRecord> Decisions;
		if (!Actor->ApplyPhaseIIResamplingFilterEvent(Labels, Decisions, OutResult.FilterMetrics, nullptr, &OutResult.LedgerMetrics))
		{
			Actor->Destroy();
			return false;
		}

		OutResult.TotalSeconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bCompleted = true;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return true;
	}

	struct FIIIBSignatureResult
	{
		int32 Replay = 0;
		FString FixtureName;
		int32 StepCount = 0;
		bool bCompleted = false;
		double TotalSeconds = 0.0;
		double PairSignedConvergenceVelocity = 0.0;
		FCarrierLabPhaseIIIB1TrackingAudit TrackingAudit;
		FCarrierLabPhaseIIIB2DistanceAudit DistanceAudit;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit MatrixAudit;
		FCarrierLabPhaseIIIB4PolarityAudit PolarityAudit;
		FCarrierLabPhaseIIIB6NeighborPropagationAudit PropagationAudit;
		FCarrierLabPhaseIIIB7HashClosureAudit ClosureAudit;
		FString RollupSignatureHash;
		FString ActiveListComponentHash;
		FString DistanceComponentHash;
		FString MatrixEvidenceComponentHash;
		FString PolarityComponentHash;
		FString PropagationComponentHash;
		FString ClosureComponentHash;
		FString Slice55ComponentHash;
		FString IndependentSignatureHash;
	};

	FString ComputeRollupSignatureHash(const FIIIBSignatureResult& Result)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIIB-signature-v1"));
		HashMix(Hash, static_cast<uint64>(MixedSignalSamples + 1));
		HashMix(Hash, static_cast<uint64>(MixedSignalPlates + 1));
		HashMix(Hash, static_cast<uint64>(MixedSignalSteps + 1));
		HashMix(Hash, static_cast<uint64>(BaselineSeed + 1));
		HashMixString(Hash, Result.TrackingAudit.ConvergenceTrackingHash);
		HashMixString(Hash, Result.DistanceAudit.ConvergenceTrackingHash);
		HashMixString(Hash, Result.MatrixAudit.ConvergenceTrackingHash);
		HashMixString(Hash, Result.PolarityAudit.PolarityHash);
		HashMixString(Hash, Result.PolarityAudit.ConvergenceTrackingHash);
		HashMixString(Hash, Result.PropagationAudit.ConvergenceTrackingHash);
		HashMixString(Hash, Result.ClosureAudit.ComputedConvergenceTrackingHash);
		HashMix(Hash, static_cast<uint64>(Result.MatrixAudit.MatrixPairCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.MatrixAudit.HitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.MatrixAudit.NonConvergentHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PropagationAudit.PropagationSeedHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PropagationAudit.PropagationAddedCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PolarityAudit.DecisionCount + 1));
		HashMix(Hash, Result.ClosureAudit.bMetricsHashMatchesComputed ? 1ull : 0ull);
		return HashToString(Hash);
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

	FString ComputeSlice55ComponentHash(const FSlice55BaselineResult& A, const FSlice55BaselineResult& B)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIIB-slice55-regression-component-v1"));
		const FSlice55BaselineResult* Results[] = { &A, &B };
		for (const FSlice55BaselineResult* Result : Results)
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

	void FinalizeIndependentSignature(const FSlice55BaselineResult& BaselineA, const FSlice55BaselineResult& BaselineB, FIIIBSignatureResult& Result)
	{
		Result.ActiveListComponentHash = ComputeActiveListComponentHash(Result.TrackingAudit);
		Result.DistanceComponentHash = ComputeDistanceComponentHash(Result.DistanceAudit);
		Result.MatrixEvidenceComponentHash = ComputeMatrixEvidenceComponentHash(Result.MatrixAudit, Result.PairSignedConvergenceVelocity);
		Result.PolarityComponentHash = ComputePolarityComponentHash(Result.PolarityAudit);
		Result.PropagationComponentHash = ComputePropagationComponentHash(Result.PropagationAudit);
		Result.ClosureComponentHash = ComputeClosureComponentHash(Result.ClosureAudit);
		Result.Slice55ComponentHash = ComputeSlice55ComponentHash(BaselineA, BaselineB);

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
		const bool bAudits =
			Actor.GetPhaseIIIB1TrackingAudit(OutResult.TrackingAudit) &&
			Actor.GetPhaseIIIB2DistanceAudit(OutResult.DistanceAudit) &&
			Actor.GetPhaseIIIB3SubductionMatrixAudit(OutResult.MatrixAudit) &&
			Actor.GetPhaseIIIB4PolarityAudit(OutResult.PolarityAudit) &&
			Actor.GetPhaseIIIB6NeighborPropagationAudit(OutResult.PropagationAudit) &&
			Actor.GetPhaseIIIB7HashClosureAudit(OutResult.ClosureAudit);
		if (!bAudits)
		{
			return false;
		}

		OutResult.RollupSignatureHash = ComputeRollupSignatureHash(OutResult);
		return true;
	}

	bool RunLocalVsPairDiscriminatorReplay(const int32 Replay, FIIIBSignatureResult& OutResult)
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
		for (int32 Step = 0; Step <= LocalDiscriminatorMaxSteps; ++Step)
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
				OutResult = Candidate;
				OutResult.TotalSeconds = FPlatformTime::Seconds() - StartSeconds;
				OutResult.bCompleted = true;
				break;
			}
		}
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bCompleted;
	}

	bool RunNoAdmissibleConvergenceNegativeReplay(const int32 Replay, FIIIBSignatureResult& OutResult)
	{
		OutResult = FIIIBSignatureResult();
		OutResult.Replay = Replay;
		OutResult.FixtureName = TEXT("no_admissible_convergence_negative");
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
		Actor->ConfigurePhaseIIMotionFixture(ECarrierLabPhaseIIMotionFixture::Zero);
		if (!Actor->SetPlateContinentalForTest(0, true) ||
			!Actor->SetPlateContinentalForTest(1, false))
		{
			Actor->Destroy();
			return false;
		}
		Actor->StepOnce();

		FCarrierLabPhaseIIIB3SubductionMatrixAudit SeedAudit;
		if (!Actor->SeedPhaseIIIB3NonConvergentEvidenceForTest(SeedAudit))
		{
			Actor->Destroy();
			return false;
		}

		OutResult.StepCount = SeedAudit.Step;
		OutResult.PairSignedConvergenceVelocity = Actor->ComputePhaseIIPairSignedConvergenceVelocity(0, 1);
		if (!CaptureIIIBSignatureAudits(*Actor, OutResult))
		{
			Actor->Destroy();
			return false;
		}
		OutResult.TotalSeconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bCompleted =
			OutResult.PairSignedConvergenceVelocity <= 0.0 &&
			OutResult.MatrixAudit.AcceptedLocalPositiveHitCount == 0 &&
			OutResult.MatrixAudit.RejectedLocalNonPositiveHitCount > 0 &&
			OutResult.MatrixAudit.HitCount == 0 &&
			OutResult.MatrixAudit.MatrixPairCount == 0 &&
			OutResult.PolarityAudit.DecisionCount == 0 &&
			OutResult.PropagationAudit.PropagationSeedHitCount == 0 &&
			OutResult.PropagationAudit.PropagationAddedCount == 0 &&
			OutResult.ClosureAudit.bMetricsHashMatchesComputed;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bCompleted;
	}

	bool BaselineMatchesExpected(const FSlice55BaselineResult& Result)
	{
		return Result.bCompleted &&
			Result.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			Result.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash;
	}

	bool BaselineReplayStable(const FSlice55BaselineResult& A, const FSlice55BaselineResult& B)
	{
		return A.bCompleted && B.bCompleted &&
			A.ContactMetrics.ContactLogHash == B.ContactMetrics.ContactLogHash &&
			A.LabelMetrics.TriangleLabelHash == B.LabelMetrics.TriangleLabelHash &&
			A.FilterMetrics.FilterDecisionHash == B.FilterMetrics.FilterDecisionHash &&
			A.FilterMetrics.StateHashAfter == B.FilterMetrics.StateHashAfter &&
			A.LedgerMetrics.MaterialLedgerHash == B.LedgerMetrics.MaterialLedgerHash;
	}

	bool LocalVsPairDiscriminatorPasses(const FIIIBSignatureResult& Result)
	{
		return Result.bCompleted &&
			Result.FixtureName == TEXT("local_vs_pair_discriminator") &&
			Result.PairSignedConvergenceVelocity <= 0.0 &&
			Result.MatrixAudit.MatrixPairCount == 1 &&
			Result.MatrixAudit.ProbePlateA == 0 &&
			Result.MatrixAudit.ProbePlateB == 1 &&
			Result.MatrixAudit.AcceptedLocalPositiveHitCount > 0 &&
			Result.MatrixAudit.RejectedLocalNonPositiveHitCount > 0 &&
			Result.MatrixAudit.ProbeSignedConvergenceVelocity > 0.0 &&
			Result.PolarityAudit.DecisionCount == 1 &&
			Result.PropagationAudit.PropagationSeedHitCount > 0 &&
			Result.PropagationAudit.PropagationAddedCount > 0 &&
			Result.ClosureAudit.bMetricsHashMatchesComputed;
	}

	bool NoAdmissibleConvergenceNegativePasses(const FIIIBSignatureResult& Result)
	{
		return Result.bCompleted &&
			Result.FixtureName == TEXT("no_admissible_convergence_negative") &&
			Result.PairSignedConvergenceVelocity <= 0.0 &&
			Result.MatrixAudit.AcceptedLocalPositiveHitCount == 0 &&
			Result.MatrixAudit.RejectedLocalNonPositiveHitCount > 0 &&
			Result.MatrixAudit.HitCount == 0 &&
			Result.MatrixAudit.MatrixPairCount == 0 &&
			Result.PolarityAudit.DecisionCount == 0 &&
			Result.PropagationAudit.PropagationSeedHitCount == 0 &&
			Result.PropagationAudit.PropagationAddedCount == 0 &&
			Result.ClosureAudit.bMetricsHashMatchesComputed;
	}

	bool SignatureReplayStable(const FIIIBSignatureResult& A, const FIIIBSignatureResult& B)
	{
		return A.bCompleted && B.bCompleted &&
			A.FixtureName == B.FixtureName &&
			A.RollupSignatureHash == B.RollupSignatureHash &&
			A.IndependentSignatureHash == B.IndependentSignatureHash &&
			A.ActiveListComponentHash == B.ActiveListComponentHash &&
			A.DistanceComponentHash == B.DistanceComponentHash &&
			A.MatrixEvidenceComponentHash == B.MatrixEvidenceComponentHash &&
			A.PolarityComponentHash == B.PolarityComponentHash &&
			A.PropagationComponentHash == B.PropagationComponentHash &&
			A.ClosureComponentHash == B.ClosureComponentHash &&
			A.Slice55ComponentHash == B.Slice55ComponentHash &&
			A.TrackingAudit.ConvergenceTrackingHash == B.TrackingAudit.ConvergenceTrackingHash &&
			A.DistanceAudit.ConvergenceTrackingHash == B.DistanceAudit.ConvergenceTrackingHash &&
			A.MatrixAudit.ConvergenceTrackingHash == B.MatrixAudit.ConvergenceTrackingHash &&
			A.MatrixAudit.MatrixEvidenceHash == B.MatrixAudit.MatrixEvidenceHash &&
			A.PolarityAudit.PolarityHash == B.PolarityAudit.PolarityHash &&
			A.PolarityAudit.ConvergenceTrackingHash == B.PolarityAudit.ConvergenceTrackingHash &&
			A.PropagationAudit.ConvergenceTrackingHash == B.PropagationAudit.ConvergenceTrackingHash &&
			A.ClosureAudit.ComputedConvergenceTrackingHash == B.ClosureAudit.ComputedConvergenceTrackingHash;
	}

	FString BaselineJson(const FSlice55BaselineResult& Result)
	{
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		return FString::Printf(
			TEXT("{\"fixture\":\"slice55_baseline_replay\",\"replay\":%d,\"samples\":%d,\"plates\":%d,\"steps\":%d,\"completed\":%s,\"seconds\":%.6f,\"contact_hash\":%s,\"label_hash\":%s,\"filter_decision_hash\":%s,\"state_hash_after\":%s,\"material_ledger_hash\":%s,\"expected_state_hash\":%s,\"expected_material_ledger_hash\":%s,\"state_hash_matches_expected\":%s,\"material_ledger_matches_expected\":%s,\"memory_gb\":%.12f}"),
			Result.Replay,
			BaselineSamples,
			BaselinePlates,
			BaselineSteps,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.TotalSeconds,
			*JsonString(Result.ContactMetrics.ContactLogHash),
			*JsonString(Result.LabelMetrics.TriangleLabelHash),
			*JsonString(Result.FilterMetrics.FilterDecisionHash),
			*JsonString(Result.FilterMetrics.StateHashAfter),
			*JsonString(Result.LedgerMetrics.MaterialLedgerHash),
			*JsonString(ExpectedSlice55StateHash),
			*JsonString(ExpectedSlice55MaterialLedgerHash),
			Result.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash ? TEXT("true") : TEXT("false"),
			Result.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash ? TEXT("true") : TEXT("false"),
			MemoryGb);
	}

	FString EvidenceJson(const CarrierLab::FConvergenceSubductionMatrixEvidence* Evidence)
	{
		if (Evidence == nullptr)
		{
			return TEXT("null");
		}
		return FString::Printf(
			TEXT("{\"evidence_id\":%d,\"contact_id\":%d,\"pair_key\":\"%llu\",\"plate_id\":%d,\"other_plate_id\":%d,\"local_triangle_id\":%d,\"other_local_triangle_id\":%d,\"signed_local_velocity\":%.12f,\"accepted\":%s}"),
			Evidence->EvidenceId,
			Evidence->ContactId,
			static_cast<unsigned long long>(Evidence->PairKey),
			Evidence->PlateId,
			Evidence->OtherPlateId,
			Evidence->LocalTriangleId,
			Evidence->OtherLocalTriangleId,
			Evidence->SignedConvergenceVelocity,
			Evidence->bAccepted ? TEXT("true") : TEXT("false"));
	}

	FString SignatureJson(const FIIIBSignatureResult& Result)
	{
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		const CarrierLab::FConvergenceSubductionMatrixEvidence* Accepted = Result.MatrixAudit.AcceptedEvidence.IsEmpty() ? nullptr : &Result.MatrixAudit.AcceptedEvidence[0];
		const CarrierLab::FConvergenceSubductionMatrixEvidence* Rejected = Result.MatrixAudit.RejectedEvidence.IsEmpty() ? nullptr : &Result.MatrixAudit.RejectedEvidence[0];
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"samples\":%d,\"plates\":%d,\"steps\":%d,\"completed\":%s,\"seconds\":%.6f,\"pair_signed_convergence_velocity\":%.12f,\"probe_local_signed_convergence_velocity\":%.12f,\"accepted_local_positive_hits\":%d,\"rejected_local_non_positive_hits\":%d,\"representative_accepted\":%s,\"representative_rejected\":%s,\"active_triangles\":%d,\"distance_records\":%d,\"matrix_pairs\":%d,\"matrix_hits\":%d,\"matrix_nonconvergent_hits\":%d,\"matrix_evidence_hash\":%s,\"polarity_decisions\":%d,\"polarity_hash\":%s,\"propagation_seed_hits\":%d,\"propagation_added\":%d,\"seed_evidence_id\":%d,\"seed_plate_id\":%d,\"seed_other_plate_id\":%d,\"seed_local_triangle_id\":%d,\"seed_signed_convergence_velocity\":%.12f,\"closure_metrics_hash\":%s,\"closure_computed_hash\":%s,\"closure_matches\":%s,\"legacy_iiib_smoke_token\":%s,\"iiib_rollup_signature_hash\":%s,\"iiib_independent_signature_hash\":%s,\"active_component_hash\":%s,\"distance_component_hash\":%s,\"matrix_evidence_component_hash\":%s,\"polarity_component_hash\":%s,\"propagation_component_hash\":%s,\"closure_component_hash\":%s,\"slice55_component_hash\":%s,\"memory_gb\":%.12f}"),
			*JsonString(Result.FixtureName),
			Result.Replay,
			MixedSignalSamples,
			MixedSignalPlates,
			Result.StepCount,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.TotalSeconds,
			Result.PairSignedConvergenceVelocity,
			Result.MatrixAudit.ProbeSignedConvergenceVelocity,
			Result.MatrixAudit.AcceptedLocalPositiveHitCount,
			Result.MatrixAudit.RejectedLocalNonPositiveHitCount,
			*EvidenceJson(Accepted),
			*EvidenceJson(Rejected),
			Result.TrackingAudit.ActiveBoundaryTriangleCount,
			Result.DistanceAudit.DistanceRecordCount,
			Result.MatrixAudit.MatrixPairCount,
			Result.MatrixAudit.HitCount,
			Result.MatrixAudit.NonConvergentHitCount,
			*JsonString(Result.MatrixAudit.MatrixEvidenceHash),
			Result.PolarityAudit.DecisionCount,
			*JsonString(Result.PolarityAudit.PolarityHash),
			Result.PropagationAudit.PropagationSeedHitCount,
			Result.PropagationAudit.PropagationAddedCount,
			Result.PropagationAudit.SeedEvidenceId,
			Result.PropagationAudit.SeedPlateId,
			Result.PropagationAudit.SeedOtherPlateId,
			Result.PropagationAudit.SeedLocalTriangleId,
			Result.PropagationAudit.SeedSignedConvergenceVelocity,
			*JsonString(Result.ClosureAudit.MetricsConvergenceTrackingHash),
			*JsonString(Result.ClosureAudit.ComputedConvergenceTrackingHash),
			Result.ClosureAudit.bMetricsHashMatchesComputed ? TEXT("true") : TEXT("false"),
			*JsonString(HistoricalIIIBSmokeToken),
			*JsonString(Result.RollupSignatureHash),
			*JsonString(Result.IndependentSignatureHash),
			*JsonString(Result.ActiveListComponentHash),
			*JsonString(Result.DistanceComponentHash),
			*JsonString(Result.MatrixEvidenceComponentHash),
			*JsonString(Result.PolarityComponentHash),
			*JsonString(Result.PropagationComponentHash),
			*JsonString(Result.ClosureComponentHash),
			*JsonString(Result.Slice55ComponentHash),
			MemoryGb);
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FSlice55BaselineResult& BaselineA,
		const FSlice55BaselineResult& BaselineB,
		const FIIIBSignatureResult& DiscriminatorA,
		const FIIIBSignatureResult& DiscriminatorB,
		const FIIIBSignatureResult& NegativeA,
		const FIIIBSignatureResult& NegativeB)
	{
		const bool bBaselineReplay = BaselineReplayStable(BaselineA, BaselineB);
		const bool bBaselineExpected = BaselineMatchesExpected(BaselineA) && BaselineMatchesExpected(BaselineB);
		const bool bLocalDiscriminator = LocalVsPairDiscriminatorPasses(DiscriminatorA) && LocalVsPairDiscriminatorPasses(DiscriminatorB);
		const bool bNegative = NoAdmissibleConvergenceNegativePasses(NegativeA) && NoAdmissibleConvergenceNegativePasses(NegativeB);
		const bool bDiscriminatorReplay = SignatureReplayStable(DiscriminatorA, DiscriminatorB);
		const bool bNegativeReplay = SignatureReplayStable(NegativeA, NegativeB);
		const bool bAllPass = bBaselineReplay && bBaselineExpected && bLocalDiscriminator && bNegative && bDiscriminatorReplay && bNegativeReplay;

		FString Report = TEXT("# Phase III Sub-phase IIIB Consolidation Checkpoint\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This hardening checkpoint keeps IIIB.1-IIIB.7 read-only and addresses the GPT-5.5 Pro pause-pending-investigation review. It adds no subduction mutation, no triangle consumption, no material transfer, and no new global ownership authority. The gates below demonstrate deterministic local-vs-pair discriminator evidence, a true no-admissible-convergence negative, and an independent component signature for future Phase III regression checks.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n|---|---|---|\n");
		Report += FString::Printf(
			TEXT("| Slice 5.5 baseline replay regression | %s | replay hashes stable: %s; state `%s`, material ledger `%s` |\n"),
			*PassFail(bBaselineReplay && bBaselineExpected),
			*PassFail(bBaselineReplay),
			*BaselineA.FilterMetrics.StateHashAfter,
			*BaselineA.LedgerMetrics.MaterialLedgerHash);
		Report += FString::Printf(
			TEXT("| local_vs_pair_discriminator | %s | pair signed %.12f, accepted local positives %d, rejected local non-positives %d, propagated %d |\n"),
			*PassFail(bLocalDiscriminator),
			DiscriminatorA.PairSignedConvergenceVelocity,
			DiscriminatorA.MatrixAudit.AcceptedLocalPositiveHitCount,
			DiscriminatorA.MatrixAudit.RejectedLocalNonPositiveHitCount,
			DiscriminatorA.PropagationAudit.PropagationAddedCount);
		Report += FString::Printf(
			TEXT("| no_admissible_convergence_negative | %s | pair signed %.12f, matrix pairs %d, decisions %d, seed hits %d, added %d |\n"),
			*PassFail(bNegative),
			NegativeA.PairSignedConvergenceVelocity,
			NegativeA.MatrixAudit.MatrixPairCount,
			NegativeA.PolarityAudit.DecisionCount,
			NegativeA.PropagationAudit.PropagationSeedHitCount,
			NegativeA.PropagationAudit.PropagationAddedCount);
		Report += FString::Printf(
			TEXT("| IIIB independent signature replay | %s | discriminator `%s` vs `%s`; negative `%s` vs `%s` |\n"),
			*PassFail(bDiscriminatorReplay && bNegativeReplay),
			*DiscriminatorA.IndependentSignatureHash,
			*DiscriminatorB.IndependentSignatureHash,
			*NegativeA.IndependentSignatureHash,
			*NegativeB.IndependentSignatureHash);

		Report += TEXT("\n## Slice 5.5 Baseline Regression\n\n");
		Report += TEXT("This is a fixed-fixture regression against Phase II Slice 5.5, not a global no-mutation proof. It protects the known 60k/40/seed-42 filtered-resampling baseline while the IIIB tracking data is active.\n\n");
		Report += TEXT("| Replay | State hash | Material ledger hash | Contact hash | Label hash | Filter hash | Expected state | Expected ledger | Seconds |\n");
		Report += TEXT("|---:|---|---|---|---|---|---|---|---:|\n");
		auto AddBaselineRow = [&Report](const FSlice55BaselineResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %d | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | %.3f |\n"),
				Result.Replay,
				*Result.FilterMetrics.StateHashAfter,
				*Result.LedgerMetrics.MaterialLedgerHash,
				*Result.ContactMetrics.ContactLogHash,
				*Result.LabelMetrics.TriangleLabelHash,
				*Result.FilterMetrics.FilterDecisionHash,
				ExpectedSlice55StateHash,
				ExpectedSlice55MaterialLedgerHash,
				Result.TotalSeconds);
		};
		AddBaselineRow(BaselineA);
		AddBaselineRow(BaselineB);

		Report += TEXT("\n## Local-Vs-Pair Discriminator Fixture\n\n");
		Report += TEXT("Fixture: two plates, forced-divergence motion, searched up to 80 steps. The gate requires pair-level signed convergence `<= 0` while the same pair still has both accepted local positive evidence and rejected local non-positive evidence. This demonstrates deterministic local-vs-pair discriminator evidence; it does not overclaim a global proof of all possible contact geometries.\n\n");
		Report += TEXT("| Replay | Step | Pair sign | Probe local sign | Accepted local + | Rejected local <=0 | Matrix pairs | Decisions | Seed evidence | Seed triangle | Seed velocity | Seed hits | Added | Closure hash | Legacy smoke token | Independent signature |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|\n");
		auto AddSignatureRow = [&Report](const FIIIBSignatureResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %d | %d | %.12f | %.12f | %d | %d | %d | %d | %d | %d | %.12f | %d | %d | `%s` | `%s` | `%s` |\n"),
				Result.Replay,
				Result.StepCount,
				Result.PairSignedConvergenceVelocity,
				Result.MatrixAudit.ProbeSignedConvergenceVelocity,
				Result.MatrixAudit.AcceptedLocalPositiveHitCount,
				Result.MatrixAudit.RejectedLocalNonPositiveHitCount,
				Result.MatrixAudit.MatrixPairCount,
				Result.PolarityAudit.DecisionCount,
				Result.PropagationAudit.SeedEvidenceId,
				Result.PropagationAudit.SeedLocalTriangleId,
				Result.PropagationAudit.SeedSignedConvergenceVelocity,
				Result.PropagationAudit.PropagationSeedHitCount,
				Result.PropagationAudit.PropagationAddedCount,
				*Result.ClosureAudit.ComputedConvergenceTrackingHash,
				HistoricalIIIBSmokeToken,
				*Result.IndependentSignatureHash);
		};
		AddSignatureRow(DiscriminatorA);
		AddSignatureRow(DiscriminatorB);

		auto AddEvidenceRows = [&Report](const TCHAR* Label, const TArray<CarrierLab::FConvergenceSubductionMatrixEvidence>& EvidenceRows)
		{
			for (const CarrierLab::FConvergenceSubductionMatrixEvidence& Evidence : EvidenceRows)
			{
				Report += FString::Printf(
					TEXT("| %s | %d | %d | %d | %d | %d | %d | %.12f | %s |\n"),
					Label,
					Evidence.EvidenceId,
					Evidence.PlateId,
					Evidence.OtherPlateId,
					Evidence.LocalTriangleId,
					Evidence.OtherLocalTriangleId,
					Evidence.ContactId,
					Evidence.SignedConvergenceVelocity,
					Evidence.bAccepted ? TEXT("accepted") : TEXT("rejected"));
			}
		};

		Report += TEXT("\n### Representative Matrix Evidence\n\n");
		Report += TEXT("| Bucket | Evidence id | Plate | Other plate | Local triangle | Other triangle | Contact id | Signed local velocity | Disposition |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---|\n");
		AddEvidenceRows(TEXT("accepted local positive"), DiscriminatorA.MatrixAudit.AcceptedEvidence);
		AddEvidenceRows(TEXT("rejected local non-positive"), DiscriminatorA.MatrixAudit.RejectedEvidence);

		Report += TEXT("\n## No-Admissible-Convergence Negative\n\n");
		Report += TEXT("Fixture: two plates, zero-motion plus one synthetic rejected local-evidence record. The gate requires real rejected local non-positive evidence while matrix admission, polarity decisions, and propagation remain empty; this is a no-admission downstream diagnostic, not a natural ray-query geometry claim.\n\n");
		Report += TEXT("| Replay | Step | Pair sign | Accepted local + | Rejected local <=0 | Matrix pairs | Decisions | Seed hits | Added | Matrix evidence hash | Independent signature |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|\n");
		auto AddNegativeRow = [&Report](const FIIIBSignatureResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %d | %d | %.12f | %d | %d | %d | %d | %d | %d | `%s` | `%s` |\n"),
				Result.Replay,
				Result.StepCount,
				Result.PairSignedConvergenceVelocity,
				Result.MatrixAudit.AcceptedLocalPositiveHitCount,
				Result.MatrixAudit.RejectedLocalNonPositiveHitCount,
				Result.MatrixAudit.MatrixPairCount,
				Result.PolarityAudit.DecisionCount,
				Result.PropagationAudit.PropagationSeedHitCount,
				Result.PropagationAudit.PropagationAddedCount,
				*Result.MatrixAudit.MatrixEvidenceHash,
				*Result.IndependentSignatureHash);
		};
		AddNegativeRow(NegativeA);
		AddNegativeRow(NegativeB);

		Report += TEXT("\n## IIIB Independent Signature Components\n\n");
		Report += TEXT("| Component | Discriminator replay 0 | Discriminator replay 1 | Negative replay 0 | Negative replay 1 |\n|---|---|---|---|---|\n");
		auto AddComponentRow = [&Report](const TCHAR* Name, const FString& A0, const FString& A1, const FString& B0, const FString& B1)
		{
			Report += FString::Printf(TEXT("| %s | `%s` | `%s` | `%s` | `%s` |\n"), Name, *A0, *A1, *B0, *B1);
		};
		AddComponentRow(TEXT("Slice 5.5 baseline"), DiscriminatorA.Slice55ComponentHash, DiscriminatorB.Slice55ComponentHash, NegativeA.Slice55ComponentHash, NegativeB.Slice55ComponentHash);
		AddComponentRow(TEXT("IIIB.1 active list"), DiscriminatorA.ActiveListComponentHash, DiscriminatorB.ActiveListComponentHash, NegativeA.ActiveListComponentHash, NegativeB.ActiveListComponentHash);
		AddComponentRow(TEXT("IIIB.2 distance-to-front"), DiscriminatorA.DistanceComponentHash, DiscriminatorB.DistanceComponentHash, NegativeA.DistanceComponentHash, NegativeB.DistanceComponentHash);
		AddComponentRow(TEXT("IIIB.3 matrix evidence"), DiscriminatorA.MatrixEvidenceComponentHash, DiscriminatorB.MatrixEvidenceComponentHash, NegativeA.MatrixEvidenceComponentHash, NegativeB.MatrixEvidenceComponentHash);
		AddComponentRow(TEXT("IIIB.4 polarity"), DiscriminatorA.PolarityComponentHash, DiscriminatorB.PolarityComponentHash, NegativeA.PolarityComponentHash, NegativeB.PolarityComponentHash);
		AddComponentRow(TEXT("IIIB.6 propagation"), DiscriminatorA.PropagationComponentHash, DiscriminatorB.PropagationComponentHash, NegativeA.PropagationComponentHash, NegativeB.PropagationComponentHash);
		AddComponentRow(TEXT("IIIB.7 closure"), DiscriminatorA.ClosureComponentHash, DiscriminatorB.ClosureComponentHash, NegativeA.ClosureComponentHash, NegativeB.ClosureComponentHash);
		AddComponentRow(TEXT("Independent IIIB signature"), DiscriminatorA.IndependentSignatureHash, DiscriminatorB.IndependentSignatureHash, NegativeA.IndependentSignatureHash, NegativeB.IndependentSignatureHash);

		Report += TEXT("\n## Historical Smoke Token\n\n");
		Report += FString::Printf(TEXT("The previous consolidated IIIB smoke token `%s` is retained only as historical comparison. The new gate is the independent component signature above; it does not reuse the same aggregate convergence hash as every component.\n\n"), HistoricalIIIBSmokeToken);

		Report += TEXT("\n## Notes\n\n");
		Report += TEXT("- Matrix admission is based on local signed convergence at the active triangle barycenter. Pair keys remain canonical metadata, not the convergence oracle.\n");
		Report += TEXT("- Propagation seed provenance is now reported by evidence id, plate ids, triangle id, and signed local velocity.\n");
		Report += TEXT("- Phase IIIC remains paused until this hardening checkpoint has explicit user review.\n\n");

		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIB hardening gates pass. Pause for user review before Phase IIIC planning or implementation.\n")
			: TEXT("IIIB consolidation does not pass. Investigate before Phase IIIC.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIBConsolidationCommandlet::UCarrierLabPhaseIIIBConsolidationCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIBConsolidationCommandlet::Main(const FString& Params)
{
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	FSlice55BaselineResult BaselineA;
	FSlice55BaselineResult BaselineB;
	const bool bBaselineA = RunSlice55Baseline(0, BaselineA);
	const bool bBaselineB = RunSlice55Baseline(1, BaselineB);

	FIIIBSignatureResult DiscriminatorA;
	FIIIBSignatureResult DiscriminatorB;
	const bool bDiscriminatorA = RunLocalVsPairDiscriminatorReplay(0, DiscriminatorA);
	const bool bDiscriminatorB = RunLocalVsPairDiscriminatorReplay(1, DiscriminatorB);

	FIIIBSignatureResult NegativeA;
	FIIIBSignatureResult NegativeB;
	const bool bNegativeA = RunNoAdmissibleConvergenceNegativeReplay(0, NegativeA);
	const bool bNegativeB = RunNoAdmissibleConvergenceNegativeReplay(1, NegativeB);

	FinalizeIndependentSignature(BaselineA, BaselineB, DiscriminatorA);
	FinalizeIndependentSignature(BaselineA, BaselineB, DiscriminatorB);
	FinalizeIndependentSignature(BaselineA, BaselineB, NegativeA);
	FinalizeIndependentSignature(BaselineA, BaselineB, NegativeB);

	FString MetricsJsonl;
	MetricsJsonl += BaselineJson(BaselineA) + LINE_TERMINATOR;
	MetricsJsonl += BaselineJson(BaselineB) + LINE_TERMINATOR;
	MetricsJsonl += SignatureJson(DiscriminatorA) + LINE_TERMINATOR;
	MetricsJsonl += SignatureJson(DiscriminatorB) + LINE_TERMINATOR;
	MetricsJsonl += SignatureJson(NegativeA) + LINE_TERMINATOR;
	MetricsJsonl += SignatureJson(NegativeB) + LINE_TERMINATOR;
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath);

	const FString Report = BuildReport(OutputRoot, BaselineA, BaselineB, DiscriminatorA, DiscriminatorB, NegativeA, NegativeB);
	const FString ReportPath = ResolveReportPath(Params);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB consolidation metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB consolidation report: %s"), *ReportPath);

	const bool bBaseline = bBaselineA && bBaselineB &&
		BaselineReplayStable(BaselineA, BaselineB) &&
		BaselineMatchesExpected(BaselineA) &&
		BaselineMatchesExpected(BaselineB);
	const bool bDiscriminator = bDiscriminatorA && bDiscriminatorB &&
		LocalVsPairDiscriminatorPasses(DiscriminatorA) &&
		LocalVsPairDiscriminatorPasses(DiscriminatorB) &&
		SignatureReplayStable(DiscriminatorA, DiscriminatorB);
	const bool bNegative = bNegativeA && bNegativeB &&
		NoAdmissibleConvergenceNegativePasses(NegativeA) &&
		NoAdmissibleConvergenceNegativePasses(NegativeB) &&
		SignatureReplayStable(NegativeA, NegativeB);

	if (!(bBaseline && bDiscriminator && bNegative))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab Phase IIIB consolidation gates failed."));
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIB consolidation gates passed."));
	return 0;
}
