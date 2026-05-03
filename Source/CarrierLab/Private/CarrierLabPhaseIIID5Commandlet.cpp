// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIID5Commandlet.h"

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
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIID5"), Stamp);
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
				TEXT("phase-iii-slice-iiid5-report.md"));
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

	struct FSutureReplayResult
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
		FCarrierLabPhaseIIID5SuturePlanAudit PlanAudit;
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

	FString ComputeSutureReplayHash(const FSutureReplayResult& Result)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIID5-suture-plan-replay-v1"));
		HashMixString(Hash, Result.Fixture);
		HashMix(Hash, Result.bCompleted ? 1ull : 0ull);
		HashMix(Hash, Result.bReadOnlyHashesStable ? 1ull : 0ull);
		HashMix(Hash, static_cast<uint64>(Result.StepCount + 1));
		HashMixString(Hash, Result.MatrixAudit.MatrixEvidenceHash);
		HashMixString(Hash, Result.PolarityAudit.PolarityHash);
		HashMixString(Hash, Result.ClosureBefore.ComputedConvergenceTrackingHash);
		HashMixString(Hash, Result.ClosureAfter.ComputedConvergenceTrackingHash);
		HashMixString(Hash, Result.PlanAudit.SourceSlabBreakPlanHash);
		HashMixString(Hash, Result.PlanAudit.SuturePlanHash);
		HashMix(Hash, static_cast<uint64>(Result.PolicyResolvedMultiHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PatchSeedTriangleId + 1));
		HashMix(Hash, static_cast<uint64>(Result.PatchTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PlanAudit.SlabBreakPlanCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PlanAudit.ValidSlabBreakPlanCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PlanAudit.SuturePlanCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PlanAudit.ValidSuturePlanCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PlanAudit.InvalidSuturePlanCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PlanAudit.BoundaryReinitializablePlanCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PlanAudit.TotalAddedTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PlanAudit.TotalAddedVertexCount + 1));
		HashMixDouble(Hash, Result.PlanAudit.TotalAddedAreaWeight);
		return HashToString(Hash);
	}

	bool RunSutureReplay(
		const int32 Replay,
		const FString& Fixture,
		const bool bContinentalPlate0,
		const bool bContinentalPlate1,
		const bool bSculptSourcePatch,
		const bool bExpectPlan,
		const bool bExpectNoPlan,
		FSutureReplayResult& OutResult)
	{
		OutResult = FSutureReplayResult();
		OutResult.Replay = Replay;
		OutResult.Fixture = Fixture;
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const int32 SearchLimit = bExpectPlan ? MaxThresholdSteps : MaxSearchSteps;
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

			if (bSculptSourcePatch)
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

			if (!Actor->GetPhaseIIIB3SubductionMatrixAudit(OutResult.MatrixAudit) ||
				!Actor->GetPhaseIIIB4PolarityAudit(OutResult.PolarityAudit) ||
				!Actor->GetPhaseIIIB7HashClosureAudit(OutResult.ClosureBefore) ||
				!Actor->PlanPhaseIIID5Suture(OutResult.PlanAudit, CollisionThresholdKm, 0.5) ||
				!Actor->GetPhaseIIIB7HashClosureAudit(OutResult.ClosureAfter))
			{
				Actor->Destroy();
				return false;
			}

			OutResult.PolicyResolvedMultiHitCount = Actor->CurrentMetrics.PolicyResolvedMultiHitCount;
			OutResult.bReadOnlyHashesStable = ClosureStateMatches(OutResult.ClosureBefore, OutResult.ClosureAfter);
			const bool bPlanReady =
				OutResult.PlanAudit.SuturePlanCount > 0 &&
				OutResult.PlanAudit.ValidSuturePlanCount == OutResult.PlanAudit.SuturePlanCount &&
				OutResult.PlanAudit.InvalidSuturePlanCount == 0;
			const bool bPureOceanicNegative =
				OutResult.MatrixAudit.HitCount > 0 &&
				OutResult.PolarityAudit.DecisionCount > 0 &&
				OutResult.PolarityAudit.CollisionCandidateCount == 0 &&
				OutResult.PlanAudit.SlabBreakPlanCount == 0 &&
				OutResult.PlanAudit.SuturePlanCount == 0;

			if ((bExpectPlan && bPlanReady) ||
				(bExpectNoPlan && bPureOceanicNegative))
			{
				OutResult.StepCount = Step;
				OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
				OutResult.bCompleted = true;
				OutResult.ReplayHash = ComputeSutureReplayHash(OutResult);
				break;
			}
		}

		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bCompleted;
	}

	bool SutureReplayStable(const FSutureReplayResult& A, const FSutureReplayResult& B)
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
			A.PlanAudit.SourceSlabBreakPlanHash == B.PlanAudit.SourceSlabBreakPlanHash &&
			A.PlanAudit.SuturePlanHash == B.PlanAudit.SuturePlanHash &&
			A.PlanAudit.SlabBreakPlanCount == B.PlanAudit.SlabBreakPlanCount &&
			A.PlanAudit.ValidSlabBreakPlanCount == B.PlanAudit.ValidSlabBreakPlanCount &&
			A.PlanAudit.SuturePlanCount == B.PlanAudit.SuturePlanCount &&
			A.PlanAudit.ValidSuturePlanCount == B.PlanAudit.ValidSuturePlanCount &&
			A.PlanAudit.InvalidSuturePlanCount == B.PlanAudit.InvalidSuturePlanCount &&
			A.PlanAudit.BoundaryReinitializablePlanCount == B.PlanAudit.BoundaryReinitializablePlanCount &&
			A.PlanAudit.TotalAddedTriangleCount == B.PlanAudit.TotalAddedTriangleCount &&
			A.PlanAudit.TotalAddedVertexCount == B.PlanAudit.TotalAddedVertexCount &&
			A.PatchSeedTriangleId == B.PatchSeedTriangleId &&
			A.PatchTriangleCount == B.PatchTriangleCount &&
			A.PolicyResolvedMultiHitCount == B.PolicyResolvedMultiHitCount &&
			FMath::IsNearlyEqual(A.PlanAudit.TotalAddedAreaWeight, B.PlanAudit.TotalAddedAreaWeight, 1.0e-9);
	}

	bool SuturePlanPasses(const FSutureReplayResult& A, const FSutureReplayResult& B)
	{
		return SutureReplayStable(A, B) &&
			A.StepCount > 0 &&
			A.StepCount <= MaxThresholdSteps &&
			A.PolarityAudit.CollisionCandidateCount > 0 &&
			A.PlanAudit.SlabBreakPlanCount > 0 &&
			A.PlanAudit.ValidSlabBreakPlanCount == A.PlanAudit.SlabBreakPlanCount &&
			A.PlanAudit.SuturePlanCount > 0 &&
			A.PlanAudit.ValidSuturePlanCount == A.PlanAudit.SuturePlanCount &&
			A.PlanAudit.InvalidSuturePlanCount == 0 &&
			A.PlanAudit.BoundaryReinitializablePlanCount == A.PlanAudit.SuturePlanCount &&
			A.PlanAudit.TotalAddedTriangleCount > 0 &&
			A.PlanAudit.TotalAddedVertexCount > 0 &&
			A.PlanAudit.TotalAddedAreaWeight > 0.0 &&
			A.PlanAudit.Plans.Num() == A.PlanAudit.SuturePlanCount &&
			A.PlanAudit.Plans[0].bAddsExactlyRemovedTerrane &&
			A.PlanAudit.Plans[0].bTopologyValid &&
			A.PatchTriangleCount > 0 &&
			A.PlanAudit.Plans[0].AddedTriangleCount >= A.PatchTriangleCount &&
			A.PlanAudit.Plans[0].PostSutureTriangleCount > 0 &&
			A.PlanAudit.Plans[0].PostNonManifoldEdgeCount == 0 &&
			A.PlanAudit.Plans[0].InvalidSourceTriangleCount == 0 &&
			A.PlanAudit.Plans[0].InvalidSourceVertexCount == 0 &&
			A.PlanAudit.Plans[0].DuplicateDestinationSourceTriangleCount == 0 &&
			A.PlanAudit.Plans[0].bBoundaryTrackingReinitializable &&
			A.PolicyResolvedMultiHitCount == 0;
	}

	bool PureOceanicNegativePasses(const FSutureReplayResult& A, const FSutureReplayResult& B)
	{
		return SutureReplayStable(A, B) &&
			A.MatrixAudit.HitCount > 0 &&
			A.PolarityAudit.DecisionCount > 0 &&
			A.PolarityAudit.CollisionCandidateCount == 0 &&
			A.PlanAudit.SlabBreakPlanCount == 0 &&
			A.PlanAudit.SuturePlanCount == 0 &&
			A.PolicyResolvedMultiHitCount == 0;
	}

	void AppendSutureJson(const FSutureReplayResult& Result, TArray<FString>& Lines)
	{
		Lines.Add(FString::Printf(
			TEXT("{\"kind\":\"iiid5_suture_plan_replay\",\"fixture\":%s,\"replay\":%d,\"completed\":%s,\"read_only_hashes_stable\":%s,\"step\":%d,\"matrix_hits\":%d,\"collision_candidates\":%d,\"slab_break_plans\":%d,\"plans\":%d,\"valid_plans\":%d,\"invalid_plans\":%d,\"boundary_reinitializable_plans\":%d,\"added_triangles\":%d,\"added_vertices\":%d,\"patch_seed_triangle\":%d,\"patch_triangle_count\":%d,\"added_area_weight\":%.9f,\"policy_resolved_multi_hits\":%d,\"slab_break_plan_hash\":%s,\"suture_plan_hash\":%s,\"replay_hash\":%s,\"seconds\":%.6f}"),
			*JsonString(Result.Fixture),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.bReadOnlyHashesStable ? TEXT("true") : TEXT("false"),
			Result.StepCount,
			Result.MatrixAudit.HitCount,
			Result.PolarityAudit.CollisionCandidateCount,
			Result.PlanAudit.SlabBreakPlanCount,
			Result.PlanAudit.SuturePlanCount,
			Result.PlanAudit.ValidSuturePlanCount,
			Result.PlanAudit.InvalidSuturePlanCount,
			Result.PlanAudit.BoundaryReinitializablePlanCount,
			Result.PlanAudit.TotalAddedTriangleCount,
			Result.PlanAudit.TotalAddedVertexCount,
			Result.PatchSeedTriangleId,
			Result.PatchTriangleCount,
			Result.PlanAudit.TotalAddedAreaWeight,
			Result.PolicyResolvedMultiHitCount,
			*JsonString(Result.PlanAudit.SourceSlabBreakPlanHash),
			*JsonString(Result.PlanAudit.SuturePlanHash),
			*JsonString(Result.ReplayHash),
			Result.Seconds));
	}

	FString PlanSummary(const FCarrierLabPhaseIIID5SuturePlanAudit& Audit)
	{
		if (Audit.Plans.IsEmpty())
		{
			return TEXT("none");
		}
		const FCarrierLabPhaseIIID5SuturePlanRecord& Plan = Audit.Plans[0];
		return FString::Printf(
			TEXT("pair `%llu`, source %d -> destination %d, added %d triangles / %d vertices, post-suture %d triangles, suture boundary edges %d, boundary reinit %s, valid %s, plan hash `%s`"),
			static_cast<unsigned long long>(Plan.PairKey),
			Plan.SourcePlateId,
			Plan.DestinationPlateId,
			Plan.AddedTriangleCount,
			Plan.AddedVertexCount,
			Plan.PostSutureTriangleCount,
			Plan.SutureBoundaryEdgeCount,
			Plan.bBoundaryTrackingReinitializable ? TEXT("yes") : TEXT("no"),
			Plan.bTopologyValid ? TEXT("yes") : TEXT("no"),
			*Plan.PlanHash);
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FSlice55BypassResult& BypassA,
		const FSlice55BypassResult& BypassB,
		const FIIIBSignatureResult& IIIBSignatureA,
		const FIIIBSignatureResult& IIIBSignatureB,
		const FSutureReplayResult& PlanA,
		const FSutureReplayResult& PlanB,
		const FSutureReplayResult& OceanA,
		const FSutureReplayResult& OceanB)
	{
		const bool bSlice55Pass =
			BypassA.bCompleted &&
			BypassB.bCompleted &&
			BypassA.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			BypassB.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			BypassA.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
			BypassB.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash;
		const bool bIIIBPass = IIIBIndependentSignaturePasses(IIIBSignatureA, IIIBSignatureB);
		const bool bPlanPass = SuturePlanPasses(PlanA, PlanB);
		const bool bOceanPass = PureOceanicNegativePasses(OceanA, OceanB);
		const bool bPolicyIndependent =
			PlanA.PolicyResolvedMultiHitCount == 0 &&
			PlanB.PolicyResolvedMultiHitCount == 0 &&
			OceanA.PolicyResolvedMultiHitCount == 0 &&
			OceanB.PolicyResolvedMultiHitCount == 0;
		const bool bAllPass = bSlice55Pass && bIIIBPass && bPlanPass && bOceanPass && bPolicyIndependent;

		FString Report;
		Report += TEXT("# Phase III Slice IIID.5 Report - Suture Topology Augmentation Dry Run\n\n");
		Report += FString::Printf(TEXT("Generated: `%s` UTC\n\n"), *FDateTime::UtcNow().ToString(TEXT("%Y-%m-%d %H:%M:%S")));
		Report += TEXT("## Scope\n\n");
		Report += TEXT("IIID.5 consumes the IIID.4 Slab Break dry-run output and produces a destination-plate suture augmentation plan. It emits added-terrane triangle sets, source-to-destination index maps, post-suture topology hashes, and boundary-reinitialization evidence only. It is read-only: no topology mutation, uplift, plate motion change, resampling, or carrier-authority mutation occurs in this slice.\n\n");
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
			TEXT("| Suture dry-run plan deterministic | %s | replay A `%s`, replay B `%s`, plans %d / %d |\n"),
			*PassFail(bPlanPass),
			*PlanA.PlanAudit.SuturePlanHash,
			*PlanB.PlanAudit.SuturePlanHash,
			PlanA.PlanAudit.SuturePlanCount,
			PlanB.PlanAudit.SuturePlanCount);
		Report += FString::Printf(
			TEXT("| Added terrane equals Slab Break removal and post-suture topology valid | %s | added triangles %d, added vertices %d, invalid plans %d, boundary-reinit plans %d |\n"),
			*PassFail(bPlanPass),
			PlanA.PlanAudit.TotalAddedTriangleCount,
			PlanA.PlanAudit.TotalAddedVertexCount,
			PlanA.PlanAudit.InvalidSuturePlanCount,
			PlanA.PlanAudit.BoundaryReinitializablePlanCount);
		Report += FString::Printf(
			TEXT("| Pure-oceanic negative emits no suture plans | %s | decisions %d, collision candidates %d, plans %d |\n"),
			*PassFail(bOceanPass),
			OceanA.PolarityAudit.DecisionCount,
			OceanA.PolarityAudit.CollisionCandidateCount,
			OceanA.PlanAudit.SuturePlanCount);
		Report += FString::Printf(
			TEXT("| No lab multi-hit policy influence | %s | policy-resolved multi-hit counts %d / %d / %d / %d |\n\n"),
			*PassFail(bPolicyIndependent),
			PlanA.PolicyResolvedMultiHitCount,
			PlanB.PolicyResolvedMultiHitCount,
			OceanA.PolicyResolvedMultiHitCount,
			OceanB.PolicyResolvedMultiHitCount);

		Report += TEXT("## Fixture Results\n\n");
		Report += TEXT("| Fixture | Replay | Step | Matrix hits | Collision candidates | Slab plans | Suture plans | Valid | Invalid | Boundary reinit | Added tris | Added verts | Patch tris | Policy multi-hits | Read-only stable | Hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		const FSutureReplayResult* Results[] = { &PlanA, &PlanB, &OceanA, &OceanB };
		for (const FSutureReplayResult* Result : Results)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %s | `%s` |\n"),
				*Result->Fixture,
				Result->Replay,
				Result->StepCount,
				Result->MatrixAudit.HitCount,
				Result->PolarityAudit.CollisionCandidateCount,
				Result->PlanAudit.SlabBreakPlanCount,
				Result->PlanAudit.SuturePlanCount,
				Result->PlanAudit.ValidSuturePlanCount,
				Result->PlanAudit.InvalidSuturePlanCount,
				Result->PlanAudit.BoundaryReinitializablePlanCount,
				Result->PlanAudit.TotalAddedTriangleCount,
				Result->PlanAudit.TotalAddedVertexCount,
				Result->PatchTriangleCount,
				Result->PolicyResolvedMultiHitCount,
				Result->bReadOnlyHashesStable ? TEXT("yes") : TEXT("no"),
				*Result->PlanAudit.SuturePlanHash);
		}

		Report += TEXT("\n## Representative Plan\n\n");
		Report += FString::Printf(TEXT("- Source-patch replay A: %s\n\n"), *PlanSummary(PlanA.PlanAudit));

		Report += TEXT("## Interpretation\n\n");
		Report += TEXT("The source-patch fixture reuses the IIID.4 Slab Break dry-run plan and maps that removed source terrane into the destination plate as an added topology component. The dry-run gate requires the added triangle set to equal the Slab Break removal set, preserve source triangle provenance, produce valid source-to-destination index maps, and leave enough post-suture boundary evidence for boundary tracking to be reinitialized in the later mutation slice.\n\n");
		Report += TEXT("All suture-plan calls are audited before and after against projection, state, crust, and convergence hashes. Stable before/after hashes show this slice plans from collision evidence without making topology, motion, or authority changes.\n\n");

		Report += TEXT("## Verdict\n\n");
		Report += bAllPass
			? TEXT("PASS. IIID.5 suture topology augmentation planning is deterministic and read-only for the exercised fixtures. IIID.6 may apply detach + suture mutations, but must consume these plans without introducing projection-derived authority or lab remesh policy.\n")
			: TEXT("FAIL. Do not advance to IIID.6 until the failing dry-run suture gate is investigated.\n");
		return Report;
	}
}

UCarrierLabPhaseIIID5Commandlet::UCarrierLabPhaseIIID5Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIID5Commandlet::Main(const FString& Params)
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

	FSutureReplayResult PlanA;
	FSutureReplayResult PlanB;
	FSutureReplayResult OceanA;
	FSutureReplayResult OceanB;
	const bool bPlanA = RunSutureReplay(0, TEXT("source_patch_suture_plan"), true, true, true, true, false, PlanA);
	const bool bPlanB = RunSutureReplay(1, TEXT("source_patch_suture_plan"), true, true, true, true, false, PlanB);
	const bool bOceanA = RunSutureReplay(0, TEXT("pure_oceanic_negative"), false, false, false, false, true, OceanA);
	const bool bOceanB = RunSutureReplay(1, TEXT("pure_oceanic_negative"), false, false, false, false, true, OceanB);

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
	AppendSutureJson(PlanA, JsonLines);
	AppendSutureJson(PlanB, JsonLines);
	AppendSutureJson(OceanA, JsonLines);
	AppendSutureJson(OceanB, JsonLines);

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(FString::Join(JsonLines, TEXT("\n")) + TEXT("\n"), *MetricsPath);

	const FString Report = BuildReport(
		OutputRoot,
		BypassA,
		BypassB,
		IIIBSignatureA,
		IIIBSignatureB,
		PlanA,
		PlanB,
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
		bPlanA &&
		bPlanB &&
		bOceanA &&
		bOceanB &&
		BypassA.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
		BypassB.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
		BypassA.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
		BypassB.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
		IIIBIndependentSignaturePasses(IIIBSignatureA, IIIBSignatureB) &&
		SuturePlanPasses(PlanA, PlanB) &&
		PureOceanicNegativePasses(OceanA, OceanB) &&
		PlanA.PolicyResolvedMultiHitCount == 0 &&
		PlanB.PolicyResolvedMultiHitCount == 0 &&
		OceanA.PolicyResolvedMultiHitCount == 0 &&
		OceanB.PolicyResolvedMultiHitCount == 0;

	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIID.5 report: %s"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIID.5 metrics: %s"), *MetricsPath);
	return bAllPass ? 0 : 1;
}
