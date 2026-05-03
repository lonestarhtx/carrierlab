// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIID2Commandlet.h"

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
	constexpr int32 MaxThresholdSteps = 12;
	constexpr double DefaultVelocityMmPerYear = 66.6666666667;
	constexpr double SlowFixtureVelocityMmPerYear = 20.0;
	constexpr double SlabPullSpeedMmPerYear = 8.0;
	constexpr double ReferenceVelocityMmPerYear = 100.0;
	constexpr double CollisionThresholdKm = 300.0;
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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIID2"), Stamp);
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
				TEXT("phase-iii-slice-iiid2-report.md"));
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
		const double ContinentalPlateFraction,
		const double VelocityMmPerYear)
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

	struct FGroupingReplayResult
	{
		FString Fixture;
		int32 Replay = 0;
		bool bCompleted = false;
		bool bReadOnlyHashesStable = false;
		int32 StepCount = 0;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit MatrixAudit;
		FCarrierLabPhaseIIIB4PolarityAudit PolarityAudit;
		FCarrierLabPhaseIIIB7HashClosureAudit ClosureBefore;
		FCarrierLabPhaseIIIB7HashClosureAudit ClosureAfter;
		FCarrierLabPhaseIIID2CollisionGroupingAudit GroupingAudit;
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
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, BaselineSamples, BaselinePlates, 0.30, DefaultVelocityMmPerYear);
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
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, MixedSignalSamples, MixedSignalPlates, 0.50, DefaultVelocityMmPerYear);
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

	bool ClosureStateMatches(const FCarrierLabPhaseIIIB7HashClosureAudit& A, const FCarrierLabPhaseIIIB7HashClosureAudit& B)
	{
		return A.ProjectionHash == B.ProjectionHash &&
			A.StateHash == B.StateHash &&
			A.CrustStateHash == B.CrustStateHash &&
			A.ComputedConvergenceTrackingHash == B.ComputedConvergenceTrackingHash &&
			A.bMetricsHashMatchesComputed &&
			B.bMetricsHashMatchesComputed;
	}

	FString ComputeGroupingReplayHash(const FGroupingReplayResult& Result)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIID2-grouping-replay-v1"));
		HashMixString(Hash, Result.Fixture);
		HashMix(Hash, Result.bCompleted ? 1ull : 0ull);
		HashMix(Hash, Result.bReadOnlyHashesStable ? 1ull : 0ull);
		HashMix(Hash, static_cast<uint64>(Result.StepCount + 1));
		HashMixString(Hash, Result.MatrixAudit.MatrixEvidenceHash);
		HashMixString(Hash, Result.PolarityAudit.PolarityHash);
		HashMixString(Hash, Result.ClosureBefore.ComputedConvergenceTrackingHash);
		HashMixString(Hash, Result.ClosureAfter.ComputedConvergenceTrackingHash);
		HashMixString(Hash, Result.GroupingAudit.GroupingHash);
		HashMix(Hash, static_cast<uint64>(Result.GroupingAudit.GroupCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.GroupingAudit.AcceptedGroupCount + 1));
		HashMixDouble(Hash, Result.GroupingAudit.MaxInterpenetrationKm);
		return HashToString(Hash);
	}

	bool RunGroupingReplay(
		const int32 Replay,
		const FString& Fixture,
		const bool bContinentalPlate0,
		const bool bContinentalPlate1,
		const bool bExpectAcceptedThreshold,
		const bool bExpectSubThreshold,
		FGroupingReplayResult& OutResult)
	{
		OutResult = FGroupingReplayResult();
		OutResult.Replay = Replay;
		OutResult.Fixture = Fixture;
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double FixtureVelocity = bExpectSubThreshold ? SlowFixtureVelocityMmPerYear : DefaultVelocityMmPerYear;
		const int32 SearchLimit = bExpectAcceptedThreshold ? MaxThresholdSteps : MaxSearchSteps;
		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, FixtureSamples, FixturePlates, 0.50, FixtureVelocity);
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

		for (int32 Step = 0; Step <= SearchLimit; ++Step)
		{
			if (Step > 0)
			{
				Actor->StepOnce();
			}

			if (!Actor->GetPhaseIIIB3SubductionMatrixAudit(OutResult.MatrixAudit) ||
				!Actor->GetPhaseIIIB4PolarityAudit(OutResult.PolarityAudit) ||
				!Actor->GetPhaseIIIB7HashClosureAudit(OutResult.ClosureBefore) ||
				!Actor->DetectPhaseIIID2CollisionGroups(OutResult.GroupingAudit, CollisionThresholdKm) ||
				!Actor->GetPhaseIIIB7HashClosureAudit(OutResult.ClosureAfter))
			{
				Actor->Destroy();
				return false;
			}

			OutResult.bReadOnlyHashesStable = ClosureStateMatches(OutResult.ClosureBefore, OutResult.ClosureAfter);
			const bool bAccepted = OutResult.GroupingAudit.AcceptedGroupCount > 0;
			const bool bSubThreshold =
				OutResult.GroupingAudit.GroupCount > 0 &&
				OutResult.GroupingAudit.AcceptedGroupCount == 0 &&
				OutResult.GroupingAudit.MaxInterpenetrationKm < CollisionThresholdKm;
			const bool bPureOceanicNegative =
				OutResult.MatrixAudit.HitCount > 0 &&
				OutResult.PolarityAudit.DecisionCount > 0 &&
				OutResult.PolarityAudit.CollisionCandidateCount == 0 &&
				OutResult.GroupingAudit.GroupCount == 0;

			if ((bExpectAcceptedThreshold && bAccepted) ||
				(bExpectSubThreshold && bSubThreshold) ||
				(!bExpectAcceptedThreshold && !bExpectSubThreshold && bPureOceanicNegative))
			{
				OutResult.StepCount = Step;
				OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
				OutResult.bCompleted = true;
				OutResult.ReplayHash = ComputeGroupingReplayHash(OutResult);
				break;
			}
		}

		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bCompleted;
	}

	bool GroupingReplayStable(const FGroupingReplayResult& A, const FGroupingReplayResult& B)
	{
		return A.bCompleted && B.bCompleted &&
			A.Fixture == B.Fixture &&
			A.StepCount == B.StepCount &&
			A.ReplayHash == B.ReplayHash &&
			A.bReadOnlyHashesStable &&
			B.bReadOnlyHashesStable &&
			A.MatrixAudit.MatrixEvidenceHash == B.MatrixAudit.MatrixEvidenceHash &&
			A.PolarityAudit.PolarityHash == B.PolarityAudit.PolarityHash &&
			A.ClosureBefore.ComputedConvergenceTrackingHash == B.ClosureBefore.ComputedConvergenceTrackingHash &&
			A.GroupingAudit.GroupingHash == B.GroupingAudit.GroupingHash &&
			A.GroupingAudit.GroupCount == B.GroupingAudit.GroupCount &&
			A.GroupingAudit.AcceptedGroupCount == B.GroupingAudit.AcceptedGroupCount &&
			FMath::IsNearlyEqual(A.GroupingAudit.MaxInterpenetrationKm, B.GroupingAudit.MaxInterpenetrationKm, 1.0e-9);
	}

	bool SubThresholdPasses(const FGroupingReplayResult& A, const FGroupingReplayResult& B)
	{
		return GroupingReplayStable(A, B) &&
			A.GroupingAudit.GroupCount > 0 &&
			A.GroupingAudit.AcceptedGroupCount == 0 &&
			A.GroupingAudit.SubThresholdGroupCount == A.GroupingAudit.GroupCount &&
			A.GroupingAudit.MaxInterpenetrationKm < CollisionThresholdKm;
	}

	bool ThresholdPasses(const FGroupingReplayResult& A, const FGroupingReplayResult& B)
	{
		return GroupingReplayStable(A, B) &&
			A.StepCount > 0 &&
			A.StepCount <= MaxThresholdSteps &&
			A.PolarityAudit.CollisionCandidateCount > 0 &&
			A.GroupingAudit.GroupCount == 1 &&
			A.GroupingAudit.AcceptedGroupCount == 1 &&
			A.GroupingAudit.Groups.Num() == 1 &&
			A.GroupingAudit.Groups[0].CandidateRecordCount > 0 &&
			A.GroupingAudit.Groups[0].MaxInterpenetrationKm >= CollisionThresholdKm &&
			A.GroupingAudit.Groups[0].ValidDistanceCount > 0 &&
			A.GroupingAudit.Groups[0].InvalidDistanceCount == 0;
	}

	bool PureOceanicNegativePasses(const FGroupingReplayResult& A, const FGroupingReplayResult& B)
	{
		return GroupingReplayStable(A, B) &&
			A.MatrixAudit.HitCount > 0 &&
			A.PolarityAudit.DecisionCount > 0 &&
			A.PolarityAudit.CollisionCandidateCount == 0 &&
			A.GroupingAudit.GroupCount == 0 &&
			A.GroupingAudit.AcceptedGroupCount == 0;
	}

	void AppendGroupingJson(const FGroupingReplayResult& Result, TArray<FString>& Lines)
	{
		Lines.Add(FString::Printf(
			TEXT("{\"kind\":\"iiid2_grouping_replay\",\"fixture\":%s,\"replay\":%d,\"completed\":%s,\"read_only_hashes_stable\":%s,\"step\":%d,\"matrix_hits\":%d,\"collision_candidates\":%d,\"terrane_records\":%d,\"groups\":%d,\"accepted_groups\":%d,\"subthreshold_groups\":%d,\"max_interpenetration_km\":%.9f,\"threshold_km\":%.3f,\"grouping_hash\":%s,\"replay_hash\":%s,\"seconds\":%.6f}"),
			*JsonString(Result.Fixture),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.bReadOnlyHashesStable ? TEXT("true") : TEXT("false"),
			Result.StepCount,
			Result.MatrixAudit.HitCount,
			Result.PolarityAudit.CollisionCandidateCount,
			Result.GroupingAudit.TerraneRecordCount,
			Result.GroupingAudit.GroupCount,
			Result.GroupingAudit.AcceptedGroupCount,
			Result.GroupingAudit.SubThresholdGroupCount,
			Result.GroupingAudit.MaxInterpenetrationKm,
			Result.GroupingAudit.ThresholdKm,
			*JsonString(Result.GroupingAudit.GroupingHash),
			*JsonString(Result.ReplayHash),
			Result.Seconds));
	}

	FString GroupSummary(const FCarrierLabPhaseIIID2CollisionGroupingAudit& Audit)
	{
		if (Audit.Groups.IsEmpty())
		{
			return TEXT("none");
		}
		const FCarrierLabPhaseIIID2CollisionGroupRecord& Group = Audit.Groups[0];
		return FString::Printf(
			TEXT("pair `%llu`, records %d, valid distances %d, max d %.3f km, accepted %s, hash `%s`"),
			static_cast<unsigned long long>(Group.PairKey),
			Group.CandidateRecordCount,
			Group.ValidDistanceCount,
			Group.MaxInterpenetrationKm,
			Group.bAccepted ? TEXT("yes") : TEXT("no"),
			*Group.GroupHash);
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FSlice55BypassResult& BypassA,
		const FSlice55BypassResult& BypassB,
		const FIIIBSignatureResult& IIIBSignatureA,
		const FIIIBSignatureResult& IIIBSignatureB,
		const FGroupingReplayResult& SubThresholdA,
		const FGroupingReplayResult& SubThresholdB,
		const FGroupingReplayResult& ThresholdA,
		const FGroupingReplayResult& ThresholdB,
		const FGroupingReplayResult& OceanA,
		const FGroupingReplayResult& OceanB)
	{
		const bool bSlice55Pass =
			BypassA.bCompleted &&
			BypassB.bCompleted &&
			BypassA.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			BypassB.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			BypassA.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
			BypassB.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash;
		const bool bIIIBPass = IIIBIndependentSignaturePasses(IIIBSignatureA, IIIBSignatureB);
		const bool bSubThresholdPass = SubThresholdPasses(SubThresholdA, SubThresholdB);
		const bool bThresholdPass = ThresholdPasses(ThresholdA, ThresholdB);
		const bool bOceanPass = PureOceanicNegativePasses(OceanA, OceanB);
		const bool bAllPass = bSlice55Pass && bIIIBPass && bSubThresholdPass && bThresholdPass && bOceanPass;

		FString Report;
		Report += TEXT("# Phase III Slice IIID.2 Report - Collision Candidate Grouping And Interpenetration Gate\n\n");
		Report += FString::Printf(TEXT("Generated: `%s` UTC\n\n"), *FDateTime::UtcNow().ToString(TEXT("%Y-%m-%d %H:%M:%S")));
		Report += TEXT("## Scope\n\n");
		Report += TEXT("IIID.2 groups IIID.1 continental terrane evidence by opposing plate pair and applies the paper's 300 km interpenetration threshold as a read-only gate. It does not detach terranes, mutate topology, apply sutures, alter plate motion, resample, or change carrier authority.\n\n");
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
			TEXT("| Sub-threshold candidate group rejected | %s | max d %.3f km, accepted groups %d |\n"),
			*PassFail(bSubThresholdPass),
			SubThresholdA.GroupingAudit.MaxInterpenetrationKm,
			SubThresholdA.GroupingAudit.AcceptedGroupCount);
		Report += FString::Printf(
			TEXT("| Forced collision reaches 300 km threshold | %s | step %d, max d %.3f km, accepted groups %d |\n"),
			*PassFail(bThresholdPass),
			ThresholdA.StepCount,
			ThresholdA.GroupingAudit.MaxInterpenetrationKm,
			ThresholdA.GroupingAudit.AcceptedGroupCount);
		Report += FString::Printf(
			TEXT("| Pure-oceanic negative emits no collision groups | %s | decisions %d, collision candidates %d, groups %d |\n\n"),
			*PassFail(bOceanPass),
			OceanA.PolarityAudit.DecisionCount,
			OceanA.PolarityAudit.CollisionCandidateCount,
			OceanA.GroupingAudit.GroupCount);

		Report += TEXT("## Fixture Results\n\n");
		Report += TEXT("| Fixture | Replay | Step | Matrix hits | Collision candidates | Terrane records | Groups | Accepted | Subthreshold | Max d km | Read-only stable | Hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		const FGroupingReplayResult* Results[] = { &SubThresholdA, &SubThresholdB, &ThresholdA, &ThresholdB, &OceanA, &OceanB };
		for (const FGroupingReplayResult* Result : Results)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d | %d | %d | %d | %.3f | %s | `%s` |\n"),
				*Result->Fixture,
				Result->Replay,
				Result->StepCount,
				Result->MatrixAudit.HitCount,
				Result->PolarityAudit.CollisionCandidateCount,
				Result->GroupingAudit.TerraneRecordCount,
				Result->GroupingAudit.GroupCount,
				Result->GroupingAudit.AcceptedGroupCount,
				Result->GroupingAudit.SubThresholdGroupCount,
				Result->GroupingAudit.MaxInterpenetrationKm,
				Result->bReadOnlyHashesStable ? TEXT("yes") : TEXT("no"),
				*Result->GroupingAudit.GroupingHash);
		}

		Report += TEXT("\n## Representative Groups\n\n");
		Report += FString::Printf(TEXT("- Sub-threshold replay A: %s\n"), *GroupSummary(SubThresholdA.GroupingAudit));
		Report += FString::Printf(TEXT("- Threshold replay A: %s\n\n"), *GroupSummary(ThresholdA.GroupingAudit));

		Report += TEXT("## Interpretation\n\n");
		Report += TEXT("The grouping key is the canonical opposing plate pair from IIIB collision-candidate evidence. The interpenetration distance is the maximum IIIB distance-to-front value over candidate terrane records in that pair, which matches the slice contract's `max(d(p))` gate. The low-speed fixture demonstrates that collision candidates are not accepted merely because contacts and terranes exist; the default-speed fixture demonstrates that the same group crosses the 300 km threshold within the bounded search window.\n\n");
		Report += TEXT("All grouping calls are audited before and after against projection, state, crust, and convergence hashes. Stable before/after hashes show this slice observes IIID.1 evidence without making topology, motion, or authority changes.\n\n");

		Report += TEXT("## Verdict\n\n");
		Report += bAllPass
			? TEXT("PASS. IIID.2 collision grouping and interpenetration gating are deterministic and read-only. IIID.3 may implement slab-break detachment using the accepted group output; it must not reinterpret this slice as already mutating topology.\n")
			: TEXT("FAIL. Do not advance to IIID.3 until the failing grouping or threshold gate is investigated.\n");
		return Report;
	}
}

UCarrierLabPhaseIIID2Commandlet::UCarrierLabPhaseIIID2Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIID2Commandlet::Main(const FString& Params)
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

	FGroupingReplayResult SubThresholdA;
	FGroupingReplayResult SubThresholdB;
	FGroupingReplayResult ThresholdA;
	FGroupingReplayResult ThresholdB;
	FGroupingReplayResult OceanA;
	FGroupingReplayResult OceanB;
	const bool bSubThresholdA = RunGroupingReplay(0, TEXT("subthreshold_all_continental"), true, true, false, true, SubThresholdA);
	const bool bSubThresholdB = RunGroupingReplay(1, TEXT("subthreshold_all_continental"), true, true, false, true, SubThresholdB);
	const bool bThresholdA = RunGroupingReplay(0, TEXT("forced_collision_threshold"), true, true, true, false, ThresholdA);
	const bool bThresholdB = RunGroupingReplay(1, TEXT("forced_collision_threshold"), true, true, true, false, ThresholdB);
	const bool bOceanA = RunGroupingReplay(0, TEXT("pure_oceanic_negative"), false, false, false, false, OceanA);
	const bool bOceanB = RunGroupingReplay(1, TEXT("pure_oceanic_negative"), false, false, false, false, OceanB);

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
	AppendGroupingJson(SubThresholdA, JsonLines);
	AppendGroupingJson(SubThresholdB, JsonLines);
	AppendGroupingJson(ThresholdA, JsonLines);
	AppendGroupingJson(ThresholdB, JsonLines);
	AppendGroupingJson(OceanA, JsonLines);
	AppendGroupingJson(OceanB, JsonLines);

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(FString::Join(JsonLines, TEXT("\n")) + TEXT("\n"), *MetricsPath);

	const FString Report = BuildReport(
		OutputRoot,
		BypassA,
		BypassB,
		IIIBSignatureA,
		IIIBSignatureB,
		SubThresholdA,
		SubThresholdB,
		ThresholdA,
		ThresholdB,
		OceanA,
		OceanB);
	const FString ReportPath = ResolveReportPath(Params);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	const bool bAllPass =
		bBypassA &&
		bBypassB &&
		bIIIBSignatureA &&
		bIIIBSignatureB &&
		bSubThresholdA &&
		bSubThresholdB &&
		bThresholdA &&
		bThresholdB &&
		bOceanA &&
		bOceanB &&
		BypassA.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
		BypassB.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
		BypassA.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
		BypassB.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
		IIIBIndependentSignaturePasses(IIIBSignatureA, IIIBSignatureB) &&
		SubThresholdPasses(SubThresholdA, SubThresholdB) &&
		ThresholdPasses(ThresholdA, ThresholdB) &&
		PureOceanicNegativePasses(OceanA, OceanB);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIID.2 report: %s"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIID.2 metrics: %s"), *MetricsPath);
	return bAllPass ? 0 : 1;
}
