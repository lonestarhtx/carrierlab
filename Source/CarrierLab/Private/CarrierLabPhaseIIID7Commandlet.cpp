// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIID7Commandlet.h"

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
	constexpr double SlabPullSpeedMmPerYear = 8.0;
	constexpr double ReferenceVelocityMmPerYear = 100.0;
	constexpr double CollisionThresholdKm = 300.0;
	constexpr double DestinationMassThresholdRatio = 0.5;
	constexpr double GateToleranceKm = 1.0e-9;
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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIID7"), Stamp);
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
				TEXT("phase-iii-slice-iiid7-report.md"));
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

	struct FUpliftReplayResult
	{
		FString Fixture;
		int32 Replay = 0;
		bool bCompleted = false;
		bool bBaselineHashesStable = false;
		bool bMutationChangedHashes = false;
		int32 StepCount = 0;
		double Seconds = 0.0;
		double OracleDeltaSumKm = 0.0;
		double OracleMaxRecordResidualKm = 0.0;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit MatrixAudit;
		FCarrierLabPhaseIIIB4PolarityAudit PolarityAudit;
		FCarrierLabPhaseIIIB7HashClosureAudit ClosureBefore;
		FCarrierLabPhaseIIIB7HashClosureAudit ClosureAfter;
		FCarrierLabPhaseIIID7InputPipelineEquivalenceAudit InputPipelineEquivalenceAudit;
		FCarrierLabPhaseIIID7CollisionUpliftAudit PlanAudit;
		FCarrierLabPhaseIIID7CollisionUpliftAudit UpliftAudit;
		int32 PolicyResolvedMultiHitCount = 0;
		int32 PatchSeedTriangleId = INDEX_NONE;
		int32 PatchTriangleCount = 0;
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

	bool BypassPasses(const FSlice55BypassResult& A, const FSlice55BypassResult& B)
	{
		return A.bCompleted && B.bCompleted &&
			A.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			B.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			A.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
			B.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash;
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

	FString ComputeUpliftReplayHash(const FUpliftReplayResult& Result)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIID7-collision-uplift-replay-v1"));
		HashMixString(Hash, Result.Fixture);
		HashMix(Hash, Result.bCompleted ? 1ull : 0ull);
		HashMix(Hash, static_cast<uint64>(Result.StepCount + 1));
		HashMixString(Hash, Result.MatrixAudit.MatrixEvidenceHash);
		HashMixString(Hash, Result.PolarityAudit.PolarityHash);
		HashMixString(Hash, Result.ClosureBefore.ComputedConvergenceTrackingHash);
		HashMixString(Hash, Result.ClosureAfter.ComputedConvergenceTrackingHash);
		HashMix(Hash, Result.InputPipelineEquivalenceAudit.bPassed ? 1ull : 0ull);
		HashMixString(Hash, Result.InputPipelineEquivalenceAudit.CachedDestinationMassHash);
		HashMixString(Hash, Result.InputPipelineEquivalenceAudit.UncachedDestinationMassHash);
		HashMixString(Hash, Result.InputPipelineEquivalenceAudit.CachedSlabBreakPlanHash);
		HashMixString(Hash, Result.InputPipelineEquivalenceAudit.UncachedSlabBreakPlanHash);
		HashMixString(Hash, Result.InputPipelineEquivalenceAudit.CachedSuturePlanHash);
		HashMixString(Hash, Result.InputPipelineEquivalenceAudit.UncachedSuturePlanHash);
		HashMixString(Hash, Result.PlanAudit.UpliftHash);
		HashMixString(Hash, Result.UpliftAudit.SourceTopologyMutationHash);
		HashMixString(Hash, Result.UpliftAudit.UpliftHash);
		HashMixDouble(Hash, Result.OracleDeltaSumKm);
		HashMixDouble(Hash, Result.OracleMaxRecordResidualKm);
		HashMix(Hash, static_cast<uint64>(Result.PolicyResolvedMultiHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PatchSeedTriangleId + 1));
		HashMix(Hash, static_cast<uint64>(Result.PatchTriangleCount + 1));
		return HashToString(Hash);
	}

	void ComputeOracleResiduals(FUpliftReplayResult& Result)
	{
		Result.OracleDeltaSumKm = 0.0;
		Result.OracleMaxRecordResidualKm = 0.0;
		const int32 Count = FMath::Min(Result.PlanAudit.Records.Num(), Result.UpliftAudit.Records.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const FCarrierLabPhaseIIID7CollisionUpliftRecord& Expected = Result.PlanAudit.Records[Index];
			const FCarrierLabPhaseIIID7CollisionUpliftRecord& Actual = Result.UpliftAudit.Records[Index];
			Result.OracleDeltaSumKm += Expected.AppliedDeltaKm;
			Result.OracleMaxRecordResidualKm = FMath::Max(
				Result.OracleMaxRecordResidualKm,
				FMath::Abs(Expected.AppliedDeltaKm - Actual.AppliedDeltaKm));
			Result.OracleMaxRecordResidualKm = FMath::Max(
				Result.OracleMaxRecordResidualKm,
				FMath::Abs(Expected.DistanceToTerraneKm - Actual.DistanceToTerraneKm));
			Result.OracleMaxRecordResidualKm = FMath::Max(
				Result.OracleMaxRecordResidualKm,
				FMath::Abs(Expected.ExpectedFoldDirection.X - Actual.ExpectedFoldDirection.X));
			Result.OracleMaxRecordResidualKm = FMath::Max(
				Result.OracleMaxRecordResidualKm,
				FMath::Abs(Expected.ExpectedFoldDirection.Y - Actual.ExpectedFoldDirection.Y));
			Result.OracleMaxRecordResidualKm = FMath::Max(
				Result.OracleMaxRecordResidualKm,
				FMath::Abs(Expected.ExpectedFoldDirection.Z - Actual.ExpectedFoldDirection.Z));
		}
		if (Result.PlanAudit.Records.Num() != Result.UpliftAudit.Records.Num())
		{
			Result.OracleMaxRecordResidualKm = TNumericLimits<double>::Max();
		}
	}

	bool RunUpliftReplay(
		const int32 Replay,
		const FString& Fixture,
		const bool bContinentalPlate0,
		const bool bContinentalPlate1,
		const bool bSculptSourcePatch,
		const bool bExpectUplift,
		const bool bExpectNoUplift,
		FUpliftReplayResult& OutResult)
	{
		OutResult = FUpliftReplayResult();
		OutResult.Replay = Replay;
		OutResult.Fixture = Fixture;
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const int32 SearchLimit = bExpectUplift ? MaxThresholdSteps : MaxSearchSteps;
		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, FixtureSamples, FixturePlates, 0.50, DefaultVelocityMmPerYear);
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

			if (bSculptSourcePatch && OutResult.PatchTriangleCount == 0)
			{
				FCarrierLabPhaseIIID2CollisionGroupingAudit ProbeGrouping;
				if (!Actor->DetectPhaseIIID2CollisionGroups(ProbeGrouping, CollisionThresholdKm))
				{
					Actor->Destroy();
					return false;
				}
				if (ProbeGrouping.AcceptedGroupCount <= 0)
				{
					continue;
				}
				if (!Actor->SetPhaseIIID3DestinationPatchForTest(0, 1, 0, OutResult.PatchSeedTriangleId, OutResult.PatchTriangleCount))
				{
					Actor->Destroy();
					return false;
				}
			}

			const bool bInputEquivalenceOk = !bExpectUplift ||
				Actor->VerifyPhaseIIID7InputPipelineEquivalence(
					OutResult.InputPipelineEquivalenceAudit,
					CollisionThresholdKm,
					DestinationMassThresholdRatio);

			if (!bInputEquivalenceOk ||
				!Actor->GetPhaseIIIB3SubductionMatrixAudit(OutResult.MatrixAudit) ||
				!Actor->GetPhaseIIIB4PolarityAudit(OutResult.PolarityAudit) ||
				!Actor->GetPhaseIIIB7HashClosureAudit(OutResult.ClosureBefore) ||
				!Actor->PlanPhaseIIID7CollisionUplift(OutResult.PlanAudit, CollisionThresholdKm, DestinationMassThresholdRatio) ||
				!Actor->ApplyPhaseIIID7CollisionUplift(OutResult.UpliftAudit, CollisionThresholdKm, DestinationMassThresholdRatio) ||
				!Actor->GetPhaseIIIB7HashClosureAudit(OutResult.ClosureAfter))
			{
				Actor->Destroy();
				return false;
			}

			OutResult.PolicyResolvedMultiHitCount = Actor->CurrentMetrics.PolicyResolvedMultiHitCount;
			OutResult.bBaselineHashesStable = ClosureStateMatches(OutResult.ClosureBefore, OutResult.ClosureAfter);
			OutResult.bMutationChangedHashes =
				OutResult.ClosureBefore.StateHash != OutResult.ClosureAfter.StateHash ||
				OutResult.ClosureBefore.CrustStateHash != OutResult.ClosureAfter.CrustStateHash ||
				OutResult.ClosureBefore.ComputedConvergenceTrackingHash != OutResult.ClosureAfter.ComputedConvergenceTrackingHash;
			ComputeOracleResiduals(OutResult);

			const bool bUpliftReady =
				OutResult.UpliftAudit.bTopologyMutationApplied &&
				OutResult.UpliftAudit.bUpliftApplied &&
				OutResult.UpliftAudit.UpliftRecordCount > 0;
			const bool bPureOceanicNegative =
				OutResult.MatrixAudit.HitCount > 0 &&
				OutResult.PolarityAudit.DecisionCount > 0 &&
				OutResult.PolarityAudit.CollisionCandidateCount == 0 &&
				OutResult.UpliftAudit.TopologyAudit.SlabBreakPlanCount == 0 &&
				OutResult.UpliftAudit.TopologyAudit.SuturePlanCount == 0 &&
				OutResult.UpliftAudit.TopologyAudit.AppliedMutationCount == 0 &&
				OutResult.UpliftAudit.bNoUpliftAvailable;

			if ((bExpectUplift && bUpliftReady) ||
				(bExpectNoUplift && bPureOceanicNegative))
			{
				OutResult.StepCount = Step;
				OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
				OutResult.bCompleted = true;
				OutResult.ReplayHash = ComputeUpliftReplayHash(OutResult);
				break;
			}
		}

		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bCompleted;
	}

	bool UpliftReplayStable(const FUpliftReplayResult& A, const FUpliftReplayResult& B)
	{
		return A.bCompleted && B.bCompleted &&
			A.Fixture == B.Fixture &&
			A.StepCount == B.StepCount &&
			A.ReplayHash == B.ReplayHash &&
			A.MatrixAudit.MatrixEvidenceHash == B.MatrixAudit.MatrixEvidenceHash &&
			A.PolarityAudit.PolarityHash == B.PolarityAudit.PolarityHash &&
			A.InputPipelineEquivalenceAudit.bPassed == B.InputPipelineEquivalenceAudit.bPassed &&
			A.InputPipelineEquivalenceAudit.CachedDestinationMassHash == B.InputPipelineEquivalenceAudit.CachedDestinationMassHash &&
			A.InputPipelineEquivalenceAudit.UncachedDestinationMassHash == B.InputPipelineEquivalenceAudit.UncachedDestinationMassHash &&
			A.InputPipelineEquivalenceAudit.CachedSlabBreakPlanHash == B.InputPipelineEquivalenceAudit.CachedSlabBreakPlanHash &&
			A.InputPipelineEquivalenceAudit.UncachedSlabBreakPlanHash == B.InputPipelineEquivalenceAudit.UncachedSlabBreakPlanHash &&
			A.InputPipelineEquivalenceAudit.CachedSuturePlanHash == B.InputPipelineEquivalenceAudit.CachedSuturePlanHash &&
			A.InputPipelineEquivalenceAudit.UncachedSuturePlanHash == B.InputPipelineEquivalenceAudit.UncachedSuturePlanHash &&
			A.PlanAudit.UpliftHash == B.PlanAudit.UpliftHash &&
			A.UpliftAudit.SourceTopologyMutationHash == B.UpliftAudit.SourceTopologyMutationHash &&
			A.UpliftAudit.UpliftHash == B.UpliftAudit.UpliftHash &&
			A.PolicyResolvedMultiHitCount == B.PolicyResolvedMultiHitCount &&
			A.PatchSeedTriangleId == B.PatchSeedTriangleId &&
			A.PatchTriangleCount == B.PatchTriangleCount;
	}

	bool CollisionUpliftPasses(const FUpliftReplayResult& A, const FUpliftReplayResult& B)
	{
		if (!UpliftReplayStable(A, B))
		{
			return false;
		}
		return
			A.StepCount > 0 &&
			A.StepCount <= MaxThresholdSteps &&
			A.PolarityAudit.CollisionCandidateCount > 0 &&
			A.PlanAudit.UpliftRecordCount > 0 &&
			A.PlanAudit.CenterExpectedDeltaKm > 0.0 &&
			A.PlanAudit.CenterAppliedDeltaKm > 0.0 &&
			FMath::Abs(A.PlanAudit.CenterExpectedDeltaKm - A.PlanAudit.CenterAppliedDeltaKm) <= GateToleranceKm &&
			A.UpliftAudit.bTopologyMutationApplied &&
			A.UpliftAudit.bUpliftApplied &&
			A.UpliftAudit.UpliftRecordCount == A.PlanAudit.UpliftRecordCount &&
			A.UpliftAudit.UniqueUpliftedVertexCount == A.PlanAudit.UniqueUpliftedVertexCount &&
			A.UpliftAudit.TotalAppliedDeltaKm > 0.0 &&
			FMath::Abs(A.OracleDeltaSumKm - A.UpliftAudit.TotalAppliedDeltaKm) <= GateToleranceKm &&
			A.OracleMaxRecordResidualKm <= GateToleranceKm &&
			A.UpliftAudit.FormulaResidualKm <= GateToleranceKm &&
			A.UpliftAudit.MaxOutsideRadiusDeltaKm <= GateToleranceKm &&
			A.InputPipelineEquivalenceAudit.bPassed &&
			A.UpliftAudit.TopologyAudit.bOneCollisionOnly &&
			A.UpliftAudit.TopologyAudit.AppliedMutationCount == 1 &&
			A.PolicyResolvedMultiHitCount == 0;
	}

	bool PureOceanicNegativePasses(const FUpliftReplayResult& A, const FUpliftReplayResult& B)
	{
		return UpliftReplayStable(A, B) &&
			A.MatrixAudit.HitCount > 0 &&
			A.PolarityAudit.DecisionCount > 0 &&
			A.PolarityAudit.CollisionCandidateCount == 0 &&
			A.UpliftAudit.TopologyAudit.SlabBreakPlanCount == 0 &&
			A.UpliftAudit.TopologyAudit.SuturePlanCount == 0 &&
			A.UpliftAudit.TopologyAudit.AppliedMutationCount == 0 &&
			A.UpliftAudit.bNoUpliftAvailable &&
			A.bBaselineHashesStable &&
			!A.bMutationChangedHashes &&
			A.PolicyResolvedMultiHitCount == 0;
	}

	void AppendUpliftJson(const FUpliftReplayResult& Result, TArray<FString>& Lines)
	{
		Lines.Add(FString::Printf(
			TEXT("{\"kind\":\"iiid7_collision_uplift_replay\",\"fixture\":%s,\"replay\":%d,\"completed\":%s,\"step\":%d,\"event_before\":%d,\"event_after\":%d,\"matrix_hits\":%d,\"collision_candidates\":%d,\"topology_applied\":%s,\"uplift_records\":%d,\"unique_vertices\":%d,\"terrane_area_km2\":%.12f,\"influence_radius_km\":%.12f,\"center_expected_delta_km\":%.12f,\"center_applied_delta_km\":%.12f,\"total_delta_km\":%.12f,\"oracle_delta_km\":%.12f,\"oracle_residual_km\":%.15f,\"policy_resolved_multi_hits\":%d,\"plan_hash\":%s,\"uplift_hash\":%s,\"replay_hash\":%s,\"seconds\":%.6f}"),
			*JsonString(Result.Fixture),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.StepCount,
			Result.UpliftAudit.EventCountBefore,
			Result.UpliftAudit.EventCountAfter,
			Result.MatrixAudit.HitCount,
			Result.PolarityAudit.CollisionCandidateCount,
			Result.UpliftAudit.bTopologyMutationApplied ? TEXT("true") : TEXT("false"),
			Result.UpliftAudit.UpliftRecordCount,
			Result.UpliftAudit.UniqueUpliftedVertexCount,
			Result.UpliftAudit.TerraneAreaKm2,
			Result.UpliftAudit.InfluenceRadiusKm,
			Result.UpliftAudit.CenterExpectedDeltaKm,
			Result.UpliftAudit.CenterAppliedDeltaKm,
			Result.UpliftAudit.TotalAppliedDeltaKm,
			Result.OracleDeltaSumKm,
			Result.OracleMaxRecordResidualKm,
			Result.PolicyResolvedMultiHitCount,
			*JsonString(Result.PlanAudit.UpliftHash),
			*JsonString(Result.UpliftAudit.UpliftHash),
			*JsonString(Result.ReplayHash),
			Result.Seconds));
		Lines.Add(FString::Printf(
			TEXT("{\"kind\":\"iiid7_input_pipeline_equivalence\",\"fixture\":%s,\"replay\":%d,\"passed\":%s,\"cached_seconds\":%.6f,\"uncached_seconds\":%.6f,\"cached_destination_hash\":%s,\"uncached_destination_hash\":%s,\"cached_slab_hash\":%s,\"uncached_slab_hash\":%s,\"cached_suture_hash\":%s,\"uncached_suture_hash\":%s}"),
			*JsonString(Result.Fixture),
			Result.Replay,
			Result.InputPipelineEquivalenceAudit.bPassed ? TEXT("true") : TEXT("false"),
			Result.InputPipelineEquivalenceAudit.CachedPipelineSeconds,
			Result.InputPipelineEquivalenceAudit.UncachedPipelineSeconds,
			*JsonString(Result.InputPipelineEquivalenceAudit.CachedDestinationMassHash),
			*JsonString(Result.InputPipelineEquivalenceAudit.UncachedDestinationMassHash),
			*JsonString(Result.InputPipelineEquivalenceAudit.CachedSlabBreakPlanHash),
			*JsonString(Result.InputPipelineEquivalenceAudit.UncachedSlabBreakPlanHash),
			*JsonString(Result.InputPipelineEquivalenceAudit.CachedSuturePlanHash),
			*JsonString(Result.InputPipelineEquivalenceAudit.UncachedSuturePlanHash)));
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FSlice55BypassResult& BypassA,
		const FSlice55BypassResult& BypassB,
		const FIIIBSignatureResult& IIIBA,
		const FIIIBSignatureResult& IIIBB,
		const FUpliftReplayResult& CollisionA,
		const FUpliftReplayResult& CollisionB,
		const FUpliftReplayResult& OceanA,
		const FUpliftReplayResult& OceanB)
	{
		const bool bSlice55Pass = BypassPasses(BypassA, BypassB);
		const bool bIIIBPass = IIIBIndependentSignaturePasses(IIIBA, IIIBB);
		const bool bCollisionPass = CollisionUpliftPasses(CollisionA, CollisionB);
		const bool bOceanPass = PureOceanicNegativePasses(OceanA, OceanB);
		const bool bAllPass = bSlice55Pass && bIIIBPass && bCollisionPass && bOceanPass;

		FString Report;
		Report += TEXT("# Phase III Slice IIID.7 Report - Collision Uplift Propagation\n\n");
		Report += FString::Printf(TEXT("Status: %s. This slice applies the thesis page-60 collision uplift formula after IIID.6 detach+suture topology mutation. It does not add remeshing, rifting, erosion, terrain displacement, ownership recovery, or projection repair.\n\n"), bAllPass ? TEXT("PASS") : TEXT("FAIL"));
		Report += FString::Printf(TEXT("Output root: `%s`\n\n"), *OutputRoot);

		Report += TEXT("## Source Check\n\n");
		Report += TEXT("- Formula source: `docs/Synthèse de terrain à léchelle planétaire/cc5c6807-071.png` (thesis page 60, Figure 38 text).\n");
		Report += TEXT("- Radius: `r = r_c * sqrt(v(q)/v_0 * A/A_0)`.\n");
		Report += TEXT("- Uplift: `dz(p) = Delta_c * A * (1 - (d(p,R)/r)^2)^2` inside the influence region.\n");
		Report += TEXT("- Fold direction: `(n x normalize(p - q)) x n`, with `q` as terrane centroid. CarrierLab applies this as a scalar field only; vertices remain on the unit sphere.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---:|---|\n");
		Report += FString::Printf(TEXT("| Slice 5.5 fixed-fixture bypass | %s | state `%s` / `%s`, ledger `%s` / `%s` |\n"),
			*PassFail(bSlice55Pass),
			*BypassA.FilterMetrics.StateHashAfter,
			*BypassB.FilterMetrics.StateHashAfter,
			*BypassA.LedgerMetrics.MaterialLedgerHash,
			*BypassB.LedgerMetrics.MaterialLedgerHash);
		Report += FString::Printf(TEXT("| IIIB independent signature | %s | computed `%s` / `%s`, expected `%s` |\n"),
			*PassFail(bIIIBPass),
			*IIIBA.IndependentSignatureHash,
			*IIIBB.IndependentSignatureHash,
			ExpectedIIIBIndependentSignature);
		Report += FString::Printf(TEXT("| Collision uplift deterministic | %s | replay `%s` / `%s`, records %d / %d |\n"),
			*PassFail(bCollisionPass),
			*CollisionA.ReplayHash,
			*CollisionB.ReplayHash,
			CollisionA.UpliftAudit.UpliftRecordCount,
			CollisionB.UpliftAudit.UpliftRecordCount);
		Report += FString::Printf(TEXT("| Formula oracle | %s | delta residual %.15f km, record residual %.15f km |\n"),
			*PassFail(bCollisionPass),
			FMath::Abs(CollisionA.OracleDeltaSumKm - CollisionA.UpliftAudit.TotalAppliedDeltaKm),
			CollisionA.OracleMaxRecordResidualKm);
		Report += FString::Printf(TEXT("| D7 input pipeline equivalence | %s | cached/uncached destination `%s` / `%s`, slab `%s` / `%s`, suture `%s` / `%s` |\n"),
			*PassFail(CollisionA.InputPipelineEquivalenceAudit.bPassed && CollisionB.InputPipelineEquivalenceAudit.bPassed),
			*CollisionA.InputPipelineEquivalenceAudit.CachedDestinationMassHash,
			*CollisionA.InputPipelineEquivalenceAudit.UncachedDestinationMassHash,
			*CollisionA.InputPipelineEquivalenceAudit.CachedSlabBreakPlanHash,
			*CollisionA.InputPipelineEquivalenceAudit.UncachedSlabBreakPlanHash,
			*CollisionA.InputPipelineEquivalenceAudit.CachedSuturePlanHash,
			*CollisionA.InputPipelineEquivalenceAudit.UncachedSuturePlanHash);
		Report += FString::Printf(TEXT("| Pure oceanic negative | %s | collision candidates %d, uplift records %d |\n"),
			*PassFail(bOceanPass),
			OceanA.PolarityAudit.CollisionCandidateCount,
			OceanA.UpliftAudit.UpliftRecordCount);
		Report += FString::Printf(TEXT("| Lab multi-hit policy dormant | %s | policy-resolved counts %d / %d / %d / %d |\n\n"),
			*PassFail(
				CollisionA.PolicyResolvedMultiHitCount == 0 &&
				CollisionB.PolicyResolvedMultiHitCount == 0 &&
				OceanA.PolicyResolvedMultiHitCount == 0 &&
				OceanB.PolicyResolvedMultiHitCount == 0),
			CollisionA.PolicyResolvedMultiHitCount,
			CollisionB.PolicyResolvedMultiHitCount,
			OceanA.PolicyResolvedMultiHitCount,
			OceanB.PolicyResolvedMultiHitCount);

		Report += TEXT("## Uplift Replays\n\n");
		Report += TEXT("| Fixture | Replay | Step | Topology | Records | Unique vertices | Terrane area km2 | Radius km | Center expected | Center applied | Total delta | Oracle residual | Policy multi-hits | Plan hash | Uplift hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|\n");
		auto AddUpliftRow = [&Report](const FUpliftReplayResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %s | %d | %d | %.6f | %.6f | %.12f | %.12f | %.12f | %.15f | %d | `%s` | `%s` |\n"),
				*Result.Fixture,
				Result.Replay,
				Result.StepCount,
				Result.UpliftAudit.bTopologyMutationApplied ? TEXT("applied") : TEXT("none"),
				Result.UpliftAudit.UpliftRecordCount,
				Result.UpliftAudit.UniqueUpliftedVertexCount,
				Result.UpliftAudit.TerraneAreaKm2,
				Result.UpliftAudit.InfluenceRadiusKm,
				Result.UpliftAudit.CenterExpectedDeltaKm,
				Result.UpliftAudit.CenterAppliedDeltaKm,
				Result.UpliftAudit.TotalAppliedDeltaKm,
				Result.OracleMaxRecordResidualKm,
				Result.PolicyResolvedMultiHitCount,
				*Result.PlanAudit.UpliftHash,
				*Result.UpliftAudit.UpliftHash);
		};
		AddUpliftRow(CollisionA);
		AddUpliftRow(CollisionB);
		AddUpliftRow(OceanA);
		AddUpliftRow(OceanB);

		Report += TEXT("\n## D7 Input Pipeline Equivalence\n\n");
		Report += TEXT("| Fixture | Replay | Pass | Cached seconds | Uncached seconds | Destination hash | Slab-break hash | Suture hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---|---|---|\n");
		auto AddEquivalenceRow = [&Report](const FUpliftReplayResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %s | %.6f | %.6f | `%s` / `%s` | `%s` / `%s` | `%s` / `%s` |\n"),
				*Result.Fixture,
				Result.Replay,
				Result.InputPipelineEquivalenceAudit.bPassed ? TEXT("PASS") : TEXT("SKIP"),
				Result.InputPipelineEquivalenceAudit.CachedPipelineSeconds,
				Result.InputPipelineEquivalenceAudit.UncachedPipelineSeconds,
				*Result.InputPipelineEquivalenceAudit.CachedDestinationMassHash,
				*Result.InputPipelineEquivalenceAudit.UncachedDestinationMassHash,
				*Result.InputPipelineEquivalenceAudit.CachedSlabBreakPlanHash,
				*Result.InputPipelineEquivalenceAudit.UncachedSlabBreakPlanHash,
				*Result.InputPipelineEquivalenceAudit.CachedSuturePlanHash,
				*Result.InputPipelineEquivalenceAudit.UncachedSuturePlanHash);
		};
		AddEquivalenceRow(CollisionA);
		AddEquivalenceRow(CollisionB);
		AddEquivalenceRow(OceanA);
		AddEquivalenceRow(OceanB);

		Report += TEXT("\n## Representative Records\n\n");
		Report += TEXT("| Vertex | Distance km | Transfer | Previous z | Delta z | New z | Fold magnitude |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|\n");
		for (int32 Index = 0; Index < FMath::Min(12, CollisionA.UpliftAudit.Records.Num()); ++Index)
		{
			const FCarrierLabPhaseIIID7CollisionUpliftRecord& Record = CollisionA.UpliftAudit.Records[Index];
			Report += FString::Printf(
				TEXT("| %d | %.12f | %.12f | %.12f | %.12f | %.12f | %.12f |\n"),
				Record.DestinationLocalVertexId,
				Record.DistanceToTerraneKm,
				Record.DistanceTransfer,
				Record.PreviousElevationKm,
				Record.AppliedDeltaKm,
				Record.NewElevationKm,
				Record.NewFoldMagnitude);
		}

		Report += TEXT("\n## Scope Notes\n\n");
		Report += TEXT("- IIID.7 consumes the IIID.6 dry-run/application chain and then mutates destination-plate elevation/fold scalar fields only.\n");
		Report += TEXT("- The distance-to-terrane implementation is a discrete carrier approximation: minimum geodesic distance to transferred terrane vertices and triangle barycenters. Continuous distance to the terrane polygon remains a consolidation/IIIE review item if needed.\n");
		Report += TEXT("- The pure-oceanic fixture demonstrates that accepted convergence evidence alone does not fabricate collision uplift.\n");
		Report += TEXT("- Stage 1.5 remains foundation characterization; this slice does not claim standalone remesh paper faithfulness.\n\n");
		Report += FString::Printf(TEXT("Decision: %s. %s\n"),
			bAllPass ? TEXT("PASS") : TEXT("FAIL"),
			bAllPass
				? TEXT("IIID.7 collision uplift is accepted for the exercised fixtures. Proceed to IIID.8.")
				: TEXT("Do not advance to IIID.8 until failed gates are resolved."));
		return Report;
	}
}

UCarrierLabPhaseIIID7Commandlet::UCarrierLabPhaseIIID7Commandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIID7Commandlet::Main(const FString& Params)
{
	const FString OutputRoot = GetOutputRoot(Params);
	const FString ReportPath = ResolveReportPath(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);

	FSlice55BypassResult BypassA;
	FSlice55BypassResult BypassB;
	FIIIBSignatureResult IIIBA;
	FIIIBSignatureResult IIIBB;
	FUpliftReplayResult CollisionA;
	FUpliftReplayResult CollisionB;
	FUpliftReplayResult OceanA;
	FUpliftReplayResult OceanB;

	const bool bBypassA = RunSlice55Bypass(0, BypassA);
	const bool bBypassB = RunSlice55Bypass(1, BypassB);
	const bool bIIIBA = RunIIIBLocalVsPairSignatureReplay(0, BypassA, BypassB, IIIBA);
	const bool bIIIBB = RunIIIBLocalVsPairSignatureReplay(1, BypassA, BypassB, IIIBB);
	const bool bCollisionA = RunUpliftReplay(0, TEXT("forced_collision_all_continental"), true, true, true, true, false, CollisionA);
	const bool bCollisionB = RunUpliftReplay(1, TEXT("forced_collision_all_continental"), true, true, true, true, false, CollisionB);
	const bool bOceanA = RunUpliftReplay(0, TEXT("pure_oceanic_negative"), false, false, false, false, true, OceanA);
	const bool bOceanB = RunUpliftReplay(1, TEXT("pure_oceanic_negative"), false, false, false, false, true, OceanB);

	TArray<FString> JsonLines;
	AppendUpliftJson(CollisionA, JsonLines);
	AppendUpliftJson(CollisionB, JsonLines);
	AppendUpliftJson(OceanA, JsonLines);
	AppendUpliftJson(OceanB, JsonLines);
	FFileHelper::SaveStringToFile(
		FString::Join(JsonLines, LINE_TERMINATOR) + LINE_TERMINATOR,
		*FPaths::Combine(OutputRoot, TEXT("metrics.jsonl")));

	const FString Report = BuildReport(OutputRoot, BypassA, BypassB, IIIBA, IIIBB, CollisionA, CollisionB, OceanA, OceanB);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	const bool bPass =
		bBypassA &&
		bBypassB &&
		bIIIBA &&
		bIIIBB &&
		bCollisionA &&
		bCollisionB &&
		bOceanA &&
		bOceanB &&
		BypassPasses(BypassA, BypassB) &&
		IIIBIndependentSignaturePasses(IIIBA, IIIBB) &&
		CollisionUpliftPasses(CollisionA, CollisionB) &&
		PureOceanicNegativePasses(OceanA, OceanB);

	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIID7 report: %s"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIID7 output: %s"), *OutputRoot);
	return bPass ? 0 : 1;
}
