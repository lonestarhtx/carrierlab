// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIID8Commandlet.h"

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
	constexpr int32 MaxSearchSteps = 80;
	constexpr double DefaultVelocityMmPerYear = 66.6666666667;
	constexpr double SlabPullSpeedMmPerYear = 8.0;
	constexpr double ReferenceVelocityMmPerYear = 100.0;
	constexpr double CollisionThresholdKm = 300.0;
	constexpr double DestinationMassThresholdRatio = 0.5;
	constexpr double RequiredReductionFraction = 0.80;
	constexpr double RequiredCollisionAttributionFraction = 0.50;
	constexpr double PaperTable2TotalTectonicsSecondsPerStep60k40 = 0.19;
	constexpr double SoftPaperRatioTarget = 10.0;
	constexpr TCHAR ExpectedSlice55StateHash[] = TEXT("3b4a85366dab80db");
	constexpr TCHAR ExpectedSlice55MaterialLedgerHash[] = TEXT("bc3077100ba291b4");
	constexpr TCHAR ExpectedIIIBIndependentSignature[] = TEXT("bf8818a26ed7b1dc");

	enum class EValidationTier : uint8
	{
		Tiny,
		Slice,
		Integrated,
		Benchmark
	};

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

	double NetDelta(const double Gain, const double Loss)
	{
		return Gain - Loss;
	}

	double NetLossMagnitude(const double Net)
	{
		return FMath::Max(0.0, -Net);
	}

	double SafeRatio(const double Numerator, const double Denominator)
	{
		return FMath::Abs(Denominator) > UE_SMALL_NUMBER ? Numerator / Denominator : 0.0;
	}

	FString TierName(const EValidationTier Tier)
	{
		switch (Tier)
		{
		case EValidationTier::Tiny:
			return TEXT("Tiny");
		case EValidationTier::Integrated:
			return TEXT("Integrated");
		case EValidationTier::Benchmark:
			return TEXT("Benchmark");
		case EValidationTier::Slice:
		default:
			return TEXT("Slice");
		}
	}

	EValidationTier ParseValidationTier(const FString& Params)
	{
		FString TierValue;
		if (!FParse::Value(*Params, TEXT("ValidationTier="), TierValue))
		{
			return EValidationTier::Slice;
		}
		if (TierValue.Equals(TEXT("Tiny"), ESearchCase::IgnoreCase))
		{
			return EValidationTier::Tiny;
		}
		if (TierValue.Equals(TEXT("Integrated"), ESearchCase::IgnoreCase))
		{
			return EValidationTier::Integrated;
		}
		if (TierValue.Equals(TEXT("Benchmark"), ESearchCase::IgnoreCase))
		{
			return EValidationTier::Benchmark;
		}
		return EValidationTier::Slice;
	}

	bool IsIntegratedLikeTier(const EValidationTier Tier)
	{
		return Tier == EValidationTier::Integrated || Tier == EValidationTier::Benchmark;
	}

	FString RatioString(const double Ratio)
	{
		return FString::Printf(TEXT("%.2fx"), Ratio);
	}

	void AddDiagnosticCallCounts(
		FCarrierLabPhaseIIIDiagnosticCallCounts& Accumulator,
		const FCarrierLabPhaseIIIDiagnosticCallCounts& Value)
	{
		Accumulator.DetectTerranes += Value.DetectTerranes;
		Accumulator.GroupCollisions += Value.GroupCollisions;
		Accumulator.DestinationMass += Value.DestinationMass;
		Accumulator.SlabBreakPlan += Value.SlabBreakPlan;
		Accumulator.SuturePlan += Value.SuturePlan;
		Accumulator.TopologyMutation += Value.TopologyMutation;
		Accumulator.UpliftPlan += Value.UpliftPlan;
		Accumulator.UpliftApply += Value.UpliftApply;
		Accumulator.D1SortedHitCount += Value.D1SortedHitCount;
		Accumulator.D1CollisionCandidateHitCount += Value.D1CollisionCandidateHitCount;
		Accumulator.D1ComponentBuildCount += Value.D1ComponentBuildCount;
		Accumulator.D1ComponentCacheHitCount += Value.D1ComponentCacheHitCount;
		Accumulator.D1ExpandedContinentalTriangleCount += Value.D1ExpandedContinentalTriangleCount;
		Accumulator.D1ScannedOceanicTriangleCount += Value.D1ScannedOceanicTriangleCount;
		Accumulator.D1InnerSeaTriangleCount += Value.D1InnerSeaTriangleCount;
		Accumulator.D1RecordCount += Value.D1RecordCount;
		Accumulator.D7PlannedRecordCount += Value.D7PlannedRecordCount;
		Accumulator.D7AppliedRecordCount += Value.D7AppliedRecordCount;
		Accumulator.D7TopologyMutationAppliedCount += Value.D7TopologyMutationAppliedCount;
		Accumulator.D7NoUpliftAvailableCount += Value.D7NoUpliftAvailableCount;
		Accumulator.D3DestinationComponentBuildCount += Value.D3DestinationComponentBuildCount;
		Accumulator.D3DestinationComponentCacheHitCount += Value.D3DestinationComponentCacheHitCount;
		Accumulator.D1DecisionIndexSeconds += Value.D1DecisionIndexSeconds;
		Accumulator.D1HitSortSeconds += Value.D1HitSortSeconds;
		Accumulator.D1HitClassificationSeconds += Value.D1HitClassificationSeconds;
		Accumulator.D1ComponentExpansionSeconds += Value.D1ComponentExpansionSeconds;
		Accumulator.D1InnerSeaScanSeconds += Value.D1InnerSeaScanSeconds;
		Accumulator.D1RecordConstructionSeconds += Value.D1RecordConstructionSeconds;
		Accumulator.D1AuditHashSeconds += Value.D1AuditHashSeconds;
		Accumulator.D7ApplyTotalSeconds += Value.D7ApplyTotalSeconds;
		Accumulator.D7InputPipelineSeconds += Value.D7InputPipelineSeconds;
		Accumulator.D7D2GroupingSeconds += Value.D7D2GroupingSeconds;
		Accumulator.D7D3DestinationMassSeconds += Value.D7D3DestinationMassSeconds;
		Accumulator.D7D4SlabBreakSeconds += Value.D7D4SlabBreakSeconds;
		Accumulator.D7D5SutureSeconds += Value.D7D5SutureSeconds;
		Accumulator.D3DestinationComponentExpansionSeconds += Value.D3DestinationComponentExpansionSeconds;
		Accumulator.D7UpliftPlanSeconds += Value.D7UpliftPlanSeconds;
		Accumulator.D7ApplyFromPlanSeconds += Value.D7ApplyFromPlanSeconds;
		Accumulator.D7TopologyMutationSeconds += Value.D7TopologyMutationSeconds;
		Accumulator.D7RecordApplySeconds += Value.D7RecordApplySeconds;
		Accumulator.D7ProjectionRefreshSeconds += Value.D7ProjectionRefreshSeconds;
		Accumulator.D7ApplyHashSeconds += Value.D7ApplyHashSeconds;
	}

	FString FormatCallCounts(const FCarrierLabPhaseIIIDiagnosticCallCounts& Calls)
	{
		return FString::Printf(
			TEXT("D1=%d D2=%d D3=%d D4=%d D5=%d D6=%d D7p=%d D7a=%d"),
			Calls.DetectTerranes,
			Calls.GroupCollisions,
			Calls.DestinationMass,
			Calls.SlabBreakPlan,
			Calls.SuturePlan,
			Calls.TopologyMutation,
			Calls.UpliftPlan,
			Calls.UpliftApply);
	}

	double D1MeasuredSeconds(const FCarrierLabPhaseIIIDiagnosticCallCounts& Calls)
	{
		return Calls.D1DecisionIndexSeconds +
			Calls.D1HitSortSeconds +
			Calls.D1HitClassificationSeconds +
			Calls.D1ComponentExpansionSeconds +
			Calls.D1InnerSeaScanSeconds +
			Calls.D1RecordConstructionSeconds +
			Calls.D1AuditHashSeconds;
	}

	double D7MeasuredSubSplitSeconds(const FCarrierLabPhaseIIIDiagnosticCallCounts& Calls)
	{
		return Calls.D7InputPipelineSeconds +
			Calls.D7UpliftPlanSeconds +
			Calls.D7ApplyFromPlanSeconds;
	}

	double D7MeasuredInputStageSeconds(const FCarrierLabPhaseIIIDiagnosticCallCounts& Calls)
	{
		return D1MeasuredSeconds(Calls) +
			Calls.D7D2GroupingSeconds +
			Calls.D7D3DestinationMassSeconds +
			Calls.D7D4SlabBreakSeconds +
			Calls.D7D5SutureSeconds;
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIID8"), Stamp);
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
			ReportPath = FPaths::Combine(
				FPaths::ProjectDir(),
				TEXT("docs"),
				TEXT("checkpoints"),
				TEXT("phase-iii-slice-iiid8-report.md"));
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

	struct FPrimaryReplayResult
	{
		FString FixtureName;
		int32 Replay = 0;
		bool bCompleted = false;
		bool bIIIDActive = false;
		double Seconds = 0.0;
		int32 StepCount = 0;
		int32 CollisionAttemptCount = 0;
		int32 CollisionEventCount = 0;
		int32 CollisionUpliftRecordCount = 0;
		double CollisionTransferredContinentalArea = 0.0;
		double CollisionUpliftDeltaKm = 0.0;
		double CollisionProbeSeconds = 0.0;
		double CollisionMutationProbeSeconds = 0.0;
		double CollisionNoMutationProbeSeconds = 0.0;
		FCarrierLabPhaseIIIDiagnosticCallCounts CollisionCallCounts;
		int32 PolicyResolvedMultiHitCount = 0;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterMetrics;
		FCarrierLabPhaseIIMaterialLedgerMetrics LedgerMetrics;
		FCarrierLabPhaseIIIB7HashClosureAudit ClosureAudit;
		FString CollisionMutationHash;
		FString ReplayHash;
	};

	double SingleHitUniformOceanicNet(const FCarrierLabPhaseIIMaterialLedgerMetrics& Ledger)
	{
		return NetDelta(Ledger.SingleHitUniformOceanicGain, Ledger.SingleHitUniformOceanicLoss);
	}

	double SingleHitUniformContinentalNet(const FCarrierLabPhaseIIMaterialLedgerMetrics& Ledger)
	{
		return NetDelta(Ledger.SingleHitUniformContinentalGain, Ledger.SingleHitUniformContinentalLoss);
	}

	double SingleHitMixedNet(const FCarrierLabPhaseIIMaterialLedgerMetrics& Ledger)
	{
		return NetDelta(Ledger.SingleHitMixedTriangleGain, Ledger.SingleHitMixedTriangleLoss);
	}

	bool RunSlice55Bypass(const int32 Replay, FSlice55BypassResult& OutResult)
	{
		UE_LOG(LogTemp, Display, TEXT("IIID8 Slice 5.5 bypass replay %d: start (%d samples, %d plates, %d steps)"), Replay, BaselineSamples, BaselinePlates, BaselineSteps);
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
			if (((Step + 1) % 4) == 0 || (Step + 1) == BaselineSteps)
			{
				UE_LOG(LogTemp, Display, TEXT("IIID8 Slice 5.5 bypass replay %d: step %d/%d"), Replay, Step + 1, BaselineSteps);
			}
		}

		UE_LOG(LogTemp, Display, TEXT("IIID8 Slice 5.5 bypass replay %d: contacts/labels/filter"), Replay);
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
		UE_LOG(LogTemp, Display, TEXT("IIID8 Slice 5.5 bypass replay %d: %s in %.3fs"), Replay, bOk ? TEXT("complete") : TEXT("failed"), OutResult.Seconds);
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
		UE_LOG(LogTemp, Display, TEXT("IIID8 IIIB signature replay %d: start"), Replay);
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
				UE_LOG(LogTemp, Display, TEXT("IIID8 IIIB signature replay %d: matched at step %d signature=%s in %.3fs"), Replay, Step, *OutResult.IndependentSignatureHash, OutResult.Seconds);
				break;
			}
			if ((Step % 10) == 0)
			{
				UE_LOG(LogTemp, Display, TEXT("IIID8 IIIB signature replay %d: searched step %d/%d"), Replay, Step, MaxSearchSteps);
			}
		}

		if (!OutResult.bCompleted)
		{
			UE_LOG(LogTemp, Warning, TEXT("IIID8 IIIB signature replay %d: no discriminator match after %d steps"), Replay, MaxSearchSteps);
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

	FString ComputePrimaryReplayHash(const FPrimaryReplayResult& Result)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIID8-primary-replay-v1"));
		HashMixString(Hash, Result.FixtureName);
		HashMix(Hash, Result.bCompleted ? 1ull : 0ull);
		HashMix(Hash, Result.bIIIDActive ? 1ull : 0ull);
		HashMix(Hash, static_cast<uint64>(Result.StepCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.CollisionEventCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.CollisionUpliftRecordCount + 1));
		HashMixDouble(Hash, Result.CollisionTransferredContinentalArea);
		HashMixDouble(Hash, Result.CollisionUpliftDeltaKm);
		HashMix(Hash, static_cast<uint64>(Result.PolicyResolvedMultiHitCount + 1));
		HashMixString(Hash, Result.ContactMetrics.ContactLogHash);
		HashMixString(Hash, Result.LabelMetrics.TriangleLabelHash);
		HashMixString(Hash, Result.FilterMetrics.FilterDecisionHash);
		HashMixString(Hash, Result.FilterMetrics.StateHashAfter);
		HashMixString(Hash, Result.LedgerMetrics.MaterialLedgerHash);
		HashMixString(Hash, Result.ClosureAudit.ComputedConvergenceTrackingHash);
		HashMixString(Hash, Result.CollisionMutationHash);
		HashMixDouble(Hash, SingleHitUniformOceanicNet(Result.LedgerMetrics));
		return HashToString(Hash);
	}

	FPrimaryReplayResult MakePrimaryReplayFromBypass(const FSlice55BypassResult& Bypass)
	{
		FPrimaryReplayResult Result;
		Result.FixtureName = TEXT("phase_ii_slice55_baseline_60k");
		Result.Replay = Bypass.Replay;
		Result.bCompleted = Bypass.bCompleted;
		Result.bIIIDActive = false;
		Result.Seconds = Bypass.Seconds;
		Result.StepCount = BaselineSteps;
		Result.PolicyResolvedMultiHitCount = 0;
		Result.ContactMetrics = Bypass.ContactMetrics;
		Result.LabelMetrics = Bypass.LabelMetrics;
		Result.FilterMetrics = Bypass.FilterMetrics;
		Result.LedgerMetrics = Bypass.LedgerMetrics;
		Result.ClosureAudit = Bypass.ClosureAudit;
		Result.CollisionMutationHash = TEXT("0000000000000000");
		Result.ReplayHash = ComputePrimaryReplayHash(Result);
		return Result;
	}

	bool ApplyOneCollisionIfAvailable(ACarrierLabVisualizationActor& Actor, FPrimaryReplayResult& Result)
	{
		UE_LOG(LogTemp, Display, TEXT("IIID8 collision probe: start attempt %d"), Result.CollisionAttemptCount + 1);
		const double StartSeconds = FPlatformTime::Seconds();
		Actor.ResetPhaseIIIDiagnosticCallCounts();
		FCarrierLabPhaseIIID7CollisionUpliftAudit Audit;
		if (!Actor.ApplyPhaseIIID7CollisionUplift(Audit, CollisionThresholdKm, DestinationMassThresholdRatio))
		{
			UE_LOG(LogTemp, Warning, TEXT("IIID8 collision probe: failed in %.3fs"), FPlatformTime::Seconds() - StartSeconds);
			return false;
		}
		const double ProbeSeconds = FPlatformTime::Seconds() - StartSeconds;
		Result.CollisionProbeSeconds += ProbeSeconds;
		AddDiagnosticCallCounts(Result.CollisionCallCounts, Actor.GetPhaseIIIDiagnosticCallCounts());
		++Result.CollisionAttemptCount;
		if (!Audit.bTopologyMutationApplied)
		{
			Result.CollisionNoMutationProbeSeconds += ProbeSeconds;
			UE_LOG(LogTemp, Display, TEXT("IIID8 collision probe: no mutation in %.3fs"), ProbeSeconds);
			return true;
		}

		++Result.CollisionEventCount;
		Result.CollisionMutationProbeSeconds += ProbeSeconds;
		Result.CollisionUpliftRecordCount += Audit.UpliftRecordCount;
		Result.CollisionUpliftDeltaKm += Audit.TotalAppliedDeltaKm;
		for (const FCarrierLabPhaseIIID6TopologyMutationRecord& Record : Audit.TopologyAudit.Records)
		{
			if (Record.bApplied)
			{
				Result.CollisionTransferredContinentalArea += FMath::Max(0.0, Record.DestinationContinentalAreaDelta);
			}
		}

		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, Result.CollisionMutationHash);
		HashMixString(Hash, Audit.UpliftHash);
		HashMixString(Hash, Audit.SourceTopologyMutationHash);
		HashMix(Hash, static_cast<uint64>(Result.CollisionEventCount + 1));
		HashMixDouble(Hash, Result.CollisionTransferredContinentalArea);
		Result.CollisionMutationHash = HashToString(Hash);
		UE_LOG(LogTemp, Display, TEXT("IIID8 collision probe: mutation event=%d transfer=%.12f uplift_records=%d in %.3fs"), Result.CollisionEventCount, Result.CollisionTransferredContinentalArea, Audit.UpliftRecordCount, ProbeSeconds);
		return true;
	}

	bool RunPrimaryReplay(const int32 Replay, const bool bEnableIIID, FPrimaryReplayResult& OutResult)
	{
		UE_LOG(LogTemp, Display, TEXT("IIID8 primary replay %d (%s): start (%d samples, %d plates, %d steps)"), Replay, bEnableIIID ? TEXT("IIID active") : TEXT("baseline"), BaselineSamples, BaselinePlates, BaselineSteps);
		OutResult = FPrimaryReplayResult();
		OutResult.Replay = Replay;
		OutResult.bIIIDActive = bEnableIIID;
		OutResult.FixtureName = bEnableIIID ? TEXT("iiid_active_primary_60k") : TEXT("phase_ii_slice55_baseline_60k");
		OutResult.CollisionMutationHash = TEXT("0000000000000000");
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
		Actor->ConfigurePhaseIIICProcessLayer(bEnableIIID, false);

		for (int32 Step = 0; Step < BaselineSteps; ++Step)
		{
			const double StepStartSeconds = FPlatformTime::Seconds();
			UE_LOG(LogTemp, Display, TEXT("IIID8 primary replay %d (%s): begin step %d/%d"), Replay, bEnableIIID ? TEXT("IIID active") : TEXT("baseline"), Step + 1, BaselineSteps);
			Actor->StepOnce();
			UE_LOG(LogTemp, Display, TEXT("IIID8 primary replay %d (%s): StepOnce step %d/%d complete in %.3fs"), Replay, bEnableIIID ? TEXT("IIID active") : TEXT("baseline"), Step + 1, BaselineSteps, FPlatformTime::Seconds() - StepStartSeconds);
			if (bEnableIIID && !ApplyOneCollisionIfAvailable(*Actor, OutResult))
			{
				Actor->Destroy();
				return false;
			}
			if (((Step + 1) % 4) == 0 || (Step + 1) == BaselineSteps)
			{
				UE_LOG(LogTemp, Display, TEXT("IIID8 primary replay %d (%s): step %d/%d collisions=%d"), Replay, bEnableIIID ? TEXT("IIID active") : TEXT("baseline"), Step + 1, BaselineSteps, OutResult.CollisionEventCount);
			}
		}

		UE_LOG(LogTemp, Display, TEXT("IIID8 primary replay %d (%s): contacts/labels/filter"), Replay, bEnableIIID ? TEXT("IIID active") : TEXT("baseline"));
		TArray<FCarrierLabPhaseIIContactRecord> Contacts;
		TArray<FCarrierLabPhaseIITriangleLabelRecord> Labels;
		TArray<FCarrierLabPhaseIIFilterDecisionRecord> Decisions;
		FCarrierLabPhaseIITriangleLabelConfig LabelConfig;
		const bool bOk =
			Actor->DetectPhaseIIContacts(Contacts, OutResult.ContactMetrics) &&
			Actor->BuildPhaseIITriangleLabels(Contacts, LabelConfig, Labels, OutResult.LabelMetrics) &&
			Actor->ApplyPhaseIIResamplingFilterEvent(Labels, Decisions, OutResult.FilterMetrics, nullptr, &OutResult.LedgerMetrics) &&
			Actor->GetPhaseIIIB7HashClosureAudit(OutResult.ClosureAudit);

		OutResult.PolicyResolvedMultiHitCount = Actor->CurrentMetrics.PolicyResolvedMultiHitCount;
		OutResult.StepCount = Actor->CurrentMetrics.Step;
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bCompleted = bOk;
		OutResult.ReplayHash = ComputePrimaryReplayHash(OutResult);
		UE_LOG(LogTemp, Display, TEXT("IIID8 primary replay %d (%s): %s in %.3fs collisions=%d transfer=%.12f uniform_oceanic_net=%.12f"), Replay, bEnableIIID ? TEXT("IIID active") : TEXT("baseline"), bOk ? TEXT("complete") : TEXT("failed"), OutResult.Seconds, OutResult.CollisionEventCount, OutResult.CollisionTransferredContinentalArea, SingleHitUniformOceanicNet(OutResult.LedgerMetrics));

		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return bOk;
	}

	bool PrimaryReplayStable(const FPrimaryReplayResult& A, const FPrimaryReplayResult& B)
	{
		return A.bCompleted && B.bCompleted &&
			A.FixtureName == B.FixtureName &&
			A.ReplayHash == B.ReplayHash &&
			A.FilterMetrics.FilterDecisionHash == B.FilterMetrics.FilterDecisionHash &&
			A.FilterMetrics.StateHashAfter == B.FilterMetrics.StateHashAfter &&
			A.LedgerMetrics.MaterialLedgerHash == B.LedgerMetrics.MaterialLedgerHash &&
			A.CollisionMutationHash == B.CollisionMutationHash &&
			A.CollisionEventCount == B.CollisionEventCount &&
			A.PolicyResolvedMultiHitCount == B.PolicyResolvedMultiHitCount;
	}

	double ReductionFraction(const FPrimaryReplayResult& Baseline, const FPrimaryReplayResult& Active)
	{
		const double BaselineLoss = NetLossMagnitude(SingleHitUniformOceanicNet(Baseline.LedgerMetrics));
		const double ActiveLoss = NetLossMagnitude(SingleHitUniformOceanicNet(Active.LedgerMetrics));
		return SafeRatio(BaselineLoss - ActiveLoss, BaselineLoss);
	}

	double EliminatedLoss(const FPrimaryReplayResult& Baseline, const FPrimaryReplayResult& Active)
	{
		const double BaselineLoss = NetLossMagnitude(SingleHitUniformOceanicNet(Baseline.LedgerMetrics));
		const double ActiveLoss = NetLossMagnitude(SingleHitUniformOceanicNet(Active.LedgerMetrics));
		return BaselineLoss - ActiveLoss;
	}

	double CollisionAttributionFraction(const FPrimaryReplayResult& Baseline, const FPrimaryReplayResult& Active)
	{
		return SafeRatio(Active.CollisionTransferredContinentalArea, EliminatedLoss(Baseline, Active));
	}

	bool IIIDActivePasses(
		const FPrimaryReplayResult& BaselineA,
		const FPrimaryReplayResult& ActiveA,
		const FPrimaryReplayResult& ActiveB)
	{
		const double Reduction = ReductionFraction(BaselineA, ActiveA);
		const double Attribution = CollisionAttributionFraction(BaselineA, ActiveA);
		return PrimaryReplayStable(ActiveA, ActiveB) &&
			ActiveA.CollisionEventCount > 0 &&
			ActiveA.PolicyResolvedMultiHitCount == 0 &&
			ActiveB.PolicyResolvedMultiHitCount == 0 &&
			Reduction >= RequiredReductionFraction &&
			Attribution >= RequiredCollisionAttributionFraction;
	}

	void AppendPrimaryJson(const FPrimaryReplayResult& Result, TArray<FString>& Lines)
	{
		const double UniformOceanicNet = SingleHitUniformOceanicNet(Result.LedgerMetrics);
		Lines.Add(FString::Printf(
			TEXT("{\"kind\":\"primary_replay\",\"fixture\":%s,\"replay\":%d,\"completed\":%s,\"iiid_active\":%s,\"step_count\":%d,\"collision_events\":%d,\"collision_attempts\":%d,\"collision_transferred_continental_area\":%.15f,\"collision_uplift_records\":%d,\"collision_uplift_delta_km\":%.15f,\"policy_resolved_multi_hit_count\":%d,\"contact_hash\":%s,\"label_hash\":%s,\"filter_decision_hash\":%s,\"state_hash_after\":%s,\"material_ledger_hash\":%s,\"single_hit_uniform_oceanic_count\":%d,\"single_hit_uniform_oceanic_loss\":%.15f,\"single_hit_uniform_oceanic_gain\":%.15f,\"single_hit_uniform_oceanic_net\":%.15f,\"single_hit_uniform_continental_count\":%d,\"single_hit_uniform_continental_net\":%.15f,\"single_hit_mixed_count\":%d,\"single_hit_mixed_net\":%.15f,\"auth_caf_before\":%.12f,\"auth_caf_after\":%.12f,\"seconds\":%.6f,\"replay_hash\":%s}"),
			*JsonString(Result.FixtureName),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.bIIIDActive ? TEXT("true") : TEXT("false"),
			Result.StepCount,
			Result.CollisionEventCount,
			Result.CollisionAttemptCount,
			Result.CollisionTransferredContinentalArea,
			Result.CollisionUpliftRecordCount,
			Result.CollisionUpliftDeltaKm,
			Result.PolicyResolvedMultiHitCount,
			*JsonString(Result.ContactMetrics.ContactLogHash),
			*JsonString(Result.LabelMetrics.TriangleLabelHash),
			*JsonString(Result.FilterMetrics.FilterDecisionHash),
			*JsonString(Result.FilterMetrics.StateHashAfter),
			*JsonString(Result.LedgerMetrics.MaterialLedgerHash),
			Result.LedgerMetrics.SingleHitUniformOceanicRecordCount,
			Result.LedgerMetrics.SingleHitUniformOceanicLoss,
			Result.LedgerMetrics.SingleHitUniformOceanicGain,
			UniformOceanicNet,
			Result.LedgerMetrics.SingleHitUniformContinentalRecordCount,
			SingleHitUniformContinentalNet(Result.LedgerMetrics),
			Result.LedgerMetrics.SingleHitMixedTriangleRecordCount,
			SingleHitMixedNet(Result.LedgerMetrics),
			Result.FilterMetrics.AuthoritativeCAFBefore,
			Result.FilterMetrics.AuthoritativeCAFAfter,
			Result.Seconds,
			*JsonString(Result.ReplayHash)));
		if (Result.bIIIDActive)
		{
			Lines.Add(FString::Printf(
				TEXT("{\"kind\":\"iiid_collision_probe_timing\",\"fixture\":%s,\"replay\":%d,\"attempts\":%d,\"events\":%d,\"total_seconds\":%.6f,\"mutation_seconds\":%.6f,\"no_mutation_seconds\":%.6f,\"seconds_per_attempt\":%.9f,\"seconds_per_event\":%.9f,\"calls\":%s}"),
				*JsonString(Result.FixtureName),
				Result.Replay,
				Result.CollisionAttemptCount,
				Result.CollisionEventCount,
				Result.CollisionProbeSeconds,
				Result.CollisionMutationProbeSeconds,
				Result.CollisionNoMutationProbeSeconds,
				Result.CollisionAttemptCount > 0 ? Result.CollisionProbeSeconds / static_cast<double>(Result.CollisionAttemptCount) : 0.0,
				Result.CollisionEventCount > 0 ? Result.CollisionMutationProbeSeconds / static_cast<double>(Result.CollisionEventCount) : 0.0,
				*JsonString(FormatCallCounts(Result.CollisionCallCounts))));
			Lines.Add(FString::Printf(
				TEXT("{\"kind\":\"iiid_d1_detection_split\",\"fixture\":%s,\"replay\":%d,\"calls\":%d,\"measured_seconds\":%.6f,\"decision_index_seconds\":%.6f,\"hit_sort_seconds\":%.6f,\"hit_classification_seconds\":%.6f,\"component_expansion_seconds\":%.6f,\"inner_sea_scan_seconds\":%.6f,\"record_construction_seconds\":%.6f,\"audit_hash_seconds\":%.6f,\"sorted_hits\":%d,\"collision_candidates\":%d,\"component_builds\":%d,\"component_cache_hits\":%d,\"expanded_continental_triangles\":%d,\"scanned_oceanic_triangles\":%d,\"inner_sea_triangles\":%d,\"records\":%d}"),
				*JsonString(Result.FixtureName),
				Result.Replay,
				Result.CollisionCallCounts.DetectTerranes,
				D1MeasuredSeconds(Result.CollisionCallCounts),
				Result.CollisionCallCounts.D1DecisionIndexSeconds,
				Result.CollisionCallCounts.D1HitSortSeconds,
				Result.CollisionCallCounts.D1HitClassificationSeconds,
				Result.CollisionCallCounts.D1ComponentExpansionSeconds,
				Result.CollisionCallCounts.D1InnerSeaScanSeconds,
				Result.CollisionCallCounts.D1RecordConstructionSeconds,
				Result.CollisionCallCounts.D1AuditHashSeconds,
				Result.CollisionCallCounts.D1SortedHitCount,
				Result.CollisionCallCounts.D1CollisionCandidateHitCount,
				Result.CollisionCallCounts.D1ComponentBuildCount,
				Result.CollisionCallCounts.D1ComponentCacheHitCount,
				Result.CollisionCallCounts.D1ExpandedContinentalTriangleCount,
				Result.CollisionCallCounts.D1ScannedOceanicTriangleCount,
				Result.CollisionCallCounts.D1InnerSeaTriangleCount,
				Result.CollisionCallCounts.D1RecordCount));
			Lines.Add(FString::Printf(
				TEXT("{\"kind\":\"iiid_d7_apply_split\",\"fixture\":%s,\"replay\":%d,\"calls\":%d,\"measured_total_seconds\":%.6f,\"measured_subsplit_seconds\":%.6f,\"input_pipeline_seconds\":%.6f,\"uplift_plan_seconds\":%.6f,\"apply_from_plan_seconds\":%.6f,\"topology_mutation_seconds\":%.6f,\"record_apply_seconds\":%.6f,\"projection_refresh_seconds\":%.6f,\"apply_hash_seconds\":%.6f,\"planned_records\":%d,\"applied_records\":%d,\"topology_mutations_applied\":%d,\"no_uplift_available\":%d}"),
				*JsonString(Result.FixtureName),
				Result.Replay,
				Result.CollisionCallCounts.UpliftApply,
				Result.CollisionCallCounts.D7ApplyTotalSeconds,
				D7MeasuredSubSplitSeconds(Result.CollisionCallCounts),
				Result.CollisionCallCounts.D7InputPipelineSeconds,
				Result.CollisionCallCounts.D7UpliftPlanSeconds,
				Result.CollisionCallCounts.D7ApplyFromPlanSeconds,
				Result.CollisionCallCounts.D7TopologyMutationSeconds,
				Result.CollisionCallCounts.D7RecordApplySeconds,
				Result.CollisionCallCounts.D7ProjectionRefreshSeconds,
				Result.CollisionCallCounts.D7ApplyHashSeconds,
				Result.CollisionCallCounts.D7PlannedRecordCount,
				Result.CollisionCallCounts.D7AppliedRecordCount,
				Result.CollisionCallCounts.D7TopologyMutationAppliedCount,
				Result.CollisionCallCounts.D7NoUpliftAvailableCount));
			Lines.Add(FString::Printf(
				TEXT("{\"kind\":\"iiid_d7_input_pipeline_split\",\"fixture\":%s,\"replay\":%d,\"input_pipeline_seconds\":%.6f,\"measured_stage_seconds\":%.6f,\"d1_measured_seconds\":%.6f,\"d2_grouping_seconds\":%.6f,\"d3_destination_mass_seconds\":%.6f,\"d4_slab_break_seconds\":%.6f,\"d5_suture_seconds\":%.6f,\"d3_destination_component_seconds\":%.6f,\"d3_destination_component_builds\":%d,\"d3_destination_component_cache_hits\":%d}"),
				*JsonString(Result.FixtureName),
				Result.Replay,
				Result.CollisionCallCounts.D7InputPipelineSeconds,
				D7MeasuredInputStageSeconds(Result.CollisionCallCounts),
				D1MeasuredSeconds(Result.CollisionCallCounts),
				Result.CollisionCallCounts.D7D2GroupingSeconds,
				Result.CollisionCallCounts.D7D3DestinationMassSeconds,
				Result.CollisionCallCounts.D7D4SlabBreakSeconds,
				Result.CollisionCallCounts.D7D5SutureSeconds,
				Result.CollisionCallCounts.D3DestinationComponentExpansionSeconds,
				Result.CollisionCallCounts.D3DestinationComponentBuildCount,
				Result.CollisionCallCounts.D3DestinationComponentCacheHitCount));
		}
	}

	FString BuildTierPolicyReport(const FString& OutputRoot, const EValidationTier Tier)
	{
		const double IIID8SecondsPerStep = 1151.130 / static_cast<double>(BaselineSteps);
		const double PaperRatio = SafeRatio(IIID8SecondsPerStep, PaperTable2TotalTectonicsSecondsPerStep60k40);

		FString Report;
		Report += TEXT("# Phase III Slice IIID.8 Report - Tier Policy Check\n\n");
		Report += FString::Printf(
			TEXT("Status: PASS as a `%s` tier policy check. This invocation intentionally does not launch the 60k/40/32 collision-active replay. It is not a passing IIID.8 integrated evidence run.\n\n"),
			*TierName(Tier));
		Report += FString::Printf(TEXT("Output root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("## Tier Rule\n\n");
		Report += TEXT("IIID.8 runs at `Integrated` tier when invoked for sub-phase consolidation. Per-slice or hardening review may use `Slice` or `Tiny` tier fast checks, but those tiers emit a yellow flag instead of pretending the integrated replay was proven.\n\n");
		Report += TEXT("Sub-phase consolidation integrated runs are mandatory. No soft-skip is allowed at consolidation: skipped, failed, or interrupted integrated runs must be dispatched before the consolidation can close.\n\n");
		Report += TEXT("## Paper Table 2 Cost Flag\n\n");
		Report += FString::Printf(
			TEXT("The last IIID.8 integrated replay 0 measurement was `1151.130s` for `%d` steps, or `%.6fs/step`. Paper Table 2 reports `%.2fs/step` total tectonic-process cost at `60k` samples / `40` plates, so the current integrated run is `%s` paper baseline. The soft target is `<= %.0fx`; exceeding it is a tracked performance finding, not a reason to skip integrated consolidation evidence.\n\n"),
			BaselineSteps,
			IIID8SecondsPerStep,
			PaperTable2TotalTectonicsSecondsPerStep60k40,
			*RatioString(PaperRatio),
			SoftPaperRatioTarget);
		Report += TEXT("## Yellow Flags\n\n");
		Report += TEXT("- `iiid8_integrated_replay_required_for_consolidation`: this tier did not run the integrated replay.\n");
		Report += TEXT("- `paper_table2_ratio_over_target`: the known integrated run exceeds the paper Table 2 baseline by far more than the `<=10x` soft target.\n\n");
		Report += TEXT("## Scope Notes\n\n");
		Report += TEXT("This policy report preserves the workflow rule only. It does not update the IIID.8 quantitative gate, does not run replay 1, and does not claim Slice 5.5 asymmetry reduction.\n");
		return Report;
	}

	FString BuildReport(
		const FString& OutputRoot,
		const EValidationTier Tier,
		const FSlice55BypassResult& BypassA,
		const FSlice55BypassResult& BypassB,
		const FIIIBSignatureResult& IIIBA,
		const FIIIBSignatureResult& IIIBB,
		const FPrimaryReplayResult& BaselineA,
		const FPrimaryReplayResult& BaselineB,
		const FPrimaryReplayResult& ActiveA,
		const FPrimaryReplayResult& ActiveB,
		const bool bActiveReplay1Requested)
	{
		const bool bBypassPass = BypassPasses(BypassA, BypassB);
		const bool bIIIBPass = IIIBIndependentSignaturePasses(IIIBA, IIIBB);
		const bool bBaselineStable = PrimaryReplayStable(BaselineA, BaselineB);
		const bool bActivePass = IIIDActivePasses(BaselineA, ActiveA, ActiveB);
		const double Reduction = ReductionFraction(BaselineA, ActiveA);
		const double Eliminated = EliminatedLoss(BaselineA, ActiveA);
		const double Attribution = CollisionAttributionFraction(BaselineA, ActiveA);
		const bool bAllPass = bBypassPass && bIIIBPass && bBaselineStable && bActivePass;

		FString Report;
		Report += TEXT("# Phase III Slice IIID.8 Report - Slice 5.5 Asymmetry Recheck\n\n");
		Report += FString::Printf(TEXT("Status: %s. This slice re-runs the 60k Slice 5.5 single-hit source-triangle subdivision with IIID collision handling active, then compares the uniform-oceanic single-hit continental loss against the accepted Phase II Slice 5.5 baseline. It does not add paper remeshing, qGamma oceanic generation, rifting, erosion, terrain displacement, ownership recovery, or projection repair.\n\n"), bAllPass ? TEXT("PASS") : TEXT("FAIL"));
		Report += FString::Printf(TEXT("Validation tier: `%s`. Integrated tier runs the 60k/40/32 path and is mandatory at sub-phase consolidation.\n\n"), *TierName(Tier));
		Report += FString::Printf(TEXT("Output root: `%s`\n\n"), *OutputRoot);

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---:|---|\n");
		Report += FString::Printf(
			TEXT("| Slice 5.5 fixed-fixture regression | %s | state `%s` / `%s`, ledger `%s` / `%s` |\n"),
			*PassFail(bBypassPass),
			*BypassA.FilterMetrics.StateHashAfter,
			*BypassB.FilterMetrics.StateHashAfter,
			*BypassA.LedgerMetrics.MaterialLedgerHash,
			*BypassB.LedgerMetrics.MaterialLedgerHash);
		Report += FString::Printf(
			TEXT("| IIIB independent signature regression | %s | replay A `%s`, replay B `%s`, expected `%s` |\n"),
			*PassFail(bIIIBPass),
			*IIIBA.IndependentSignatureHash,
			*IIIBB.IndependentSignatureHash,
			ExpectedIIIBIndependentSignature);
		Report += FString::Printf(
			TEXT("| Baseline primary replay stable | %s | replay A `%s`, replay B `%s`, ledger `%s` / `%s` |\n"),
			*PassFail(bBaselineStable),
			*BaselineA.ReplayHash,
			*BaselineB.ReplayHash,
			*BaselineA.LedgerMetrics.MaterialLedgerHash,
			*BaselineB.LedgerMetrics.MaterialLedgerHash);
		Report += FString::Printf(
			TEXT("| Uniform-oceanic single-hit loss reduction | %s | baseline %.12f, IIID %.12f, reduction %.2f%%, target %.2f%% |\n"),
			*PassFail(Reduction >= RequiredReductionFraction),
			SingleHitUniformOceanicNet(BaselineA.LedgerMetrics),
			SingleHitUniformOceanicNet(ActiveA.LedgerMetrics),
			Reduction * 100.0,
			RequiredReductionFraction * 100.0);
		Report += FString::Printf(
			TEXT("| Collision-transfer attribution | %s | eliminated %.12f, transferred %.12f, attribution %.2f%%, target %.2f%% |\n"),
			*PassFail(Attribution >= RequiredCollisionAttributionFraction),
			Eliminated,
			ActiveA.CollisionTransferredContinentalArea,
			Attribution * 100.0,
			RequiredCollisionAttributionFraction * 100.0);
		Report += FString::Printf(
			TEXT("| No lab multi-hit policy influence | %s | baseline %d / %d, IIID %d / %d |\n\n"),
			*PassFail(BaselineA.PolicyResolvedMultiHitCount == 0 && BaselineB.PolicyResolvedMultiHitCount == 0 && ActiveA.PolicyResolvedMultiHitCount == 0 && (!bActiveReplay1Requested || ActiveB.PolicyResolvedMultiHitCount == 0)),
			BaselineA.PolicyResolvedMultiHitCount,
			BaselineB.PolicyResolvedMultiHitCount,
			ActiveA.PolicyResolvedMultiHitCount,
			ActiveB.PolicyResolvedMultiHitCount);
		if (!bActiveReplay1Requested)
		{
			Report += TEXT("| IIID active replay 1 | SKIP | Context-aware skip: replay 0 already failed or this path is investigation-only. Pass/fail remains false unless replay 1 runs and stability is proven. |\n\n");
		}

		Report += TEXT("## Primary Rows\n\n");
		Report += TEXT("| Fixture | Replay | IIID | Events | Uniform oceanic count/net | Uniform continental count/net | Mixed count/net | Auth CAF before/after | Ledger hash | Replay hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---|---|\n");
		const FPrimaryReplayResult* Rows[] = { &BaselineA, &BaselineB, &ActiveA, &ActiveB };
		for (const FPrimaryReplayResult* Row : Rows)
		{
			if (Row == &ActiveB && !bActiveReplay1Requested)
			{
				Report += TEXT("| iiid_active_primary_60k | 1 | yes | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |\n");
				continue;
			}
			Report += FString::Printf(
				TEXT("| %s | %d | %s | %d | %d / %.12f | %d / %.12f | %d / %.12f | %.12f / %.12f | `%s` | `%s` |\n"),
				*Row->FixtureName,
				Row->Replay,
				Row->bIIIDActive ? TEXT("yes") : TEXT("no"),
				Row->CollisionEventCount,
				Row->LedgerMetrics.SingleHitUniformOceanicRecordCount,
				SingleHitUniformOceanicNet(Row->LedgerMetrics),
				Row->LedgerMetrics.SingleHitUniformContinentalRecordCount,
				SingleHitUniformContinentalNet(Row->LedgerMetrics),
				Row->LedgerMetrics.SingleHitMixedTriangleRecordCount,
				SingleHitMixedNet(Row->LedgerMetrics),
				Row->FilterMetrics.AuthoritativeCAFBefore,
				Row->FilterMetrics.AuthoritativeCAFAfter,
				*Row->LedgerMetrics.MaterialLedgerHash,
				*Row->ReplayHash);
		}

		Report += TEXT("\n## Integrated Collision Timing\n\n");
		Report += TEXT("| Fixture | Replay | Attempts | Events | Collision probe seconds | Mutation seconds | No-mutation seconds | Seconds / attempt | Seconds / event | Nested calls |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		Report += FString::Printf(
			TEXT("| %s | %d | %d | %d | %.6f | %.6f | %.6f | %.9f | %.9f | %s |\n"),
			*ActiveA.FixtureName,
			ActiveA.Replay,
			ActiveA.CollisionAttemptCount,
			ActiveA.CollisionEventCount,
			ActiveA.CollisionProbeSeconds,
			ActiveA.CollisionMutationProbeSeconds,
			ActiveA.CollisionNoMutationProbeSeconds,
			ActiveA.CollisionAttemptCount > 0 ? ActiveA.CollisionProbeSeconds / static_cast<double>(ActiveA.CollisionAttemptCount) : 0.0,
			ActiveA.CollisionEventCount > 0 ? ActiveA.CollisionMutationProbeSeconds / static_cast<double>(ActiveA.CollisionEventCount) : 0.0,
			*FormatCallCounts(ActiveA.CollisionCallCounts));
		if (bActiveReplay1Requested)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %.6f | %.6f | %.6f | %.9f | %.9f | %s |\n"),
				*ActiveB.FixtureName,
				ActiveB.Replay,
				ActiveB.CollisionAttemptCount,
				ActiveB.CollisionEventCount,
				ActiveB.CollisionProbeSeconds,
				ActiveB.CollisionMutationProbeSeconds,
				ActiveB.CollisionNoMutationProbeSeconds,
				ActiveB.CollisionAttemptCount > 0 ? ActiveB.CollisionProbeSeconds / static_cast<double>(ActiveB.CollisionAttemptCount) : 0.0,
				ActiveB.CollisionEventCount > 0 ? ActiveB.CollisionMutationProbeSeconds / static_cast<double>(ActiveB.CollisionEventCount) : 0.0,
				*FormatCallCounts(ActiveB.CollisionCallCounts));
		}
		else
		{
			Report += TEXT("| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |\n");
		}

		Report += TEXT("\n## D1 Detection Split\n\n");
		Report += TEXT("| Fixture | Replay | D1 calls | Measured D1 seconds | Decision index | Hit sort | Hit classification | Component expansion | Inner-sea scan | Record construction | Audit hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		auto AppendD1TimingRow = [&Report](const FPrimaryReplayResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %.6f | %.6f | %.6f | %.6f | %.6f | %.6f | %.6f | %.6f |\n"),
				*Result.FixtureName,
				Result.Replay,
				Result.CollisionCallCounts.DetectTerranes,
				D1MeasuredSeconds(Result.CollisionCallCounts),
				Result.CollisionCallCounts.D1DecisionIndexSeconds,
				Result.CollisionCallCounts.D1HitSortSeconds,
				Result.CollisionCallCounts.D1HitClassificationSeconds,
				Result.CollisionCallCounts.D1ComponentExpansionSeconds,
				Result.CollisionCallCounts.D1InnerSeaScanSeconds,
				Result.CollisionCallCounts.D1RecordConstructionSeconds,
				Result.CollisionCallCounts.D1AuditHashSeconds);
		};
		AppendD1TimingRow(ActiveA);
		if (bActiveReplay1Requested)
		{
			AppendD1TimingRow(ActiveB);
		}
		else
		{
			Report += TEXT("| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |\n");
		}

		Report += TEXT("\n## D1 Detection Counts\n\n");
		Report += TEXT("| Fixture | Replay | Sorted hits | Collision candidates | Component builds | Component cache hits | Expanded continental triangles | Scanned oceanic triangles | Inner-sea triangles | Records |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		auto AppendD1CountRow = [&Report](const FPrimaryReplayResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d | %d | %d | %d | %d |\n"),
				*Result.FixtureName,
				Result.Replay,
				Result.CollisionCallCounts.D1SortedHitCount,
				Result.CollisionCallCounts.D1CollisionCandidateHitCount,
				Result.CollisionCallCounts.D1ComponentBuildCount,
				Result.CollisionCallCounts.D1ComponentCacheHitCount,
				Result.CollisionCallCounts.D1ExpandedContinentalTriangleCount,
				Result.CollisionCallCounts.D1ScannedOceanicTriangleCount,
				Result.CollisionCallCounts.D1InnerSeaTriangleCount,
				Result.CollisionCallCounts.D1RecordCount);
		};
		AppendD1CountRow(ActiveA);
		if (bActiveReplay1Requested)
		{
			AppendD1CountRow(ActiveB);
		}
		else
		{
			Report += TEXT("| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |\n");
		}

		Report += TEXT("\n## D7 Apply Split\n\n");
		Report += TEXT("| Fixture | Replay | D7 apply calls | Total seconds | Input pipeline | Uplift plan | Apply-from-plan | Topology mutation | Record apply | Projection refresh | Hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		auto AppendD7TimingRow = [&Report](const FPrimaryReplayResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %.6f | %.6f | %.6f | %.6f | %.6f | %.6f | %.6f | %.6f |\n"),
				*Result.FixtureName,
				Result.Replay,
				Result.CollisionCallCounts.UpliftApply,
				Result.CollisionCallCounts.D7ApplyTotalSeconds,
				Result.CollisionCallCounts.D7InputPipelineSeconds,
				Result.CollisionCallCounts.D7UpliftPlanSeconds,
				Result.CollisionCallCounts.D7ApplyFromPlanSeconds,
				Result.CollisionCallCounts.D7TopologyMutationSeconds,
				Result.CollisionCallCounts.D7RecordApplySeconds,
				Result.CollisionCallCounts.D7ProjectionRefreshSeconds,
				Result.CollisionCallCounts.D7ApplyHashSeconds);
		};
		AppendD7TimingRow(ActiveA);
		if (bActiveReplay1Requested)
		{
			AppendD7TimingRow(ActiveB);
		}
		else
		{
			Report += TEXT("| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |\n");
		}

		Report += TEXT("\n## D7 Input Pipeline Split\n\n");
		Report += TEXT("| Fixture | Replay | Input pipeline | Measured stage subtotal | D1 measured | D2 grouping | D3 destination mass | D4 slab-break | D5 suture | D3 destination component | D3 component builds | D3 cache hits |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		auto AppendD7InputRow = [&Report](const FPrimaryReplayResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %.6f | %.6f | %.6f | %.6f | %.6f | %.6f | %.6f | %.6f | %d | %d |\n"),
				*Result.FixtureName,
				Result.Replay,
				Result.CollisionCallCounts.D7InputPipelineSeconds,
				D7MeasuredInputStageSeconds(Result.CollisionCallCounts),
				D1MeasuredSeconds(Result.CollisionCallCounts),
				Result.CollisionCallCounts.D7D2GroupingSeconds,
				Result.CollisionCallCounts.D7D3DestinationMassSeconds,
				Result.CollisionCallCounts.D7D4SlabBreakSeconds,
				Result.CollisionCallCounts.D7D5SutureSeconds,
				Result.CollisionCallCounts.D3DestinationComponentExpansionSeconds,
				Result.CollisionCallCounts.D3DestinationComponentBuildCount,
				Result.CollisionCallCounts.D3DestinationComponentCacheHitCount);
		};
		AppendD7InputRow(ActiveA);
		if (bActiveReplay1Requested)
		{
			AppendD7InputRow(ActiveB);
		}
		else
		{
			Report += TEXT("| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP | SKIP |\n");
		}

		Report += TEXT("\n## D7 Apply Counts\n\n");
		Report += TEXT("| Fixture | Replay | Planned records | Applied records | Applied topology mutations | No-uplift attempts |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|\n");
		auto AppendD7CountRow = [&Report](const FPrimaryReplayResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d |\n"),
				*Result.FixtureName,
				Result.Replay,
				Result.CollisionCallCounts.D7PlannedRecordCount,
				Result.CollisionCallCounts.D7AppliedRecordCount,
				Result.CollisionCallCounts.D7TopologyMutationAppliedCount,
				Result.CollisionCallCounts.D7NoUpliftAvailableCount);
		};
		AppendD7CountRow(ActiveA);
		if (bActiveReplay1Requested)
		{
			AppendD7CountRow(ActiveB);
		}
		else
		{
			Report += TEXT("| iiid_active_primary_60k | 1 | SKIP | SKIP | SKIP | SKIP |\n");
		}

		Report += TEXT("\n## Interpretation\n\n");
		if (!bActiveReplay1Requested)
		{
			Report += TEXT("The second IIID-active replay was not run because replay 0 is already an investigation result or the operator explicitly selected that policy. This report can still serve as an investigation checkpoint when replay 0 fails the quantitative gate, but it cannot serve as a passing determinism checkpoint. Consolidation gates expected to pass run replay 1 by default.\n\n");
		}
		if (bActivePass)
		{
			Report += TEXT("The IIID-active row reduces the Slice 5.5 uniform-oceanic-source single-hit continental loss by the required amount, and the collision-transfer ledger is large enough to explain at least half of the eliminated loss. This supports the Phase III hypothesis that continental collision/suture is the missing persistence mechanism for this Slice 5.5 bucket.\n\n");
		}
		else
		{
			Report += TEXT("The IIID-active row does not meet one or both quantitative exit gates. Per the slice plan, this is an investigation checkpoint rather than permission to claim IIID closed the Slice 5.5 asymmetry. Do not use this report to claim full carrier/remesh success.\n\n");
		}
		Report += TEXT("This is still a Phase II filtered-resampling comparison, not the IIIE paper remesh. Stage 1.5 remains foundation characterization; IIIE owns subducting/colliding-triangle remesh filtering, continuous q1/q2, qGamma oceanic generation, and process-state reset.\n\n");

		Report += TEXT("## Verdict\n\n");
		Report += bAllPass
			? TEXT("PASS. IIID collision handling materially reduces the Slice 5.5 uniform-oceanic-source loss under this 60k recheck. IIID consolidation may summarize this as the sub-phase headline result, while preserving the IIIE remesh caveat.\n")
			: TEXT("FAIL. Pause before IIID consolidation and write/extend an investigation checkpoint for the failed quantitative gate.\n");
		return Report;
	}
}

UCarrierLabPhaseIIID8Commandlet::UCarrierLabPhaseIIID8Commandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIID8Commandlet::Main(const FString& Params)
{
	const EValidationTier Tier = ParseValidationTier(Params);
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIID8 tier=%s output root: %s"), *TierName(Tier), *OutputRoot);

	if (!IsIntegratedLikeTier(Tier))
	{
		TArray<FString> JsonLines;
		const double IIID8SecondsPerStep = 1151.130 / static_cast<double>(BaselineSteps);
		JsonLines.Add(FString::Printf(
			TEXT("{\"kind\":\"validation_tier\",\"tier\":%s,\"integrated_replay_launched\":false,\"completed\":true,\"yellow_flag\":%s}"),
			*JsonString(TierName(Tier)),
			*JsonString(TEXT("iiid8_integrated_replay_required_for_consolidation"))));
		JsonLines.Add(FString::Printf(
			TEXT("{\"kind\":\"paper_table2_ratio\",\"surface\":\"iiid8_integrated_total_last_run\",\"wall_seconds\":1151.130000,\"step_count\":%d,\"seconds_per_step\":%.9f,\"paper_table2_seconds_per_step\":%.9f,\"paper_ratio\":%.6f,\"target_ratio\":%.6f,\"target_status\":%s}"),
			BaselineSteps,
			IIID8SecondsPerStep,
			PaperTable2TotalTectonicsSecondsPerStep60k40,
			SafeRatio(IIID8SecondsPerStep, PaperTable2TotalTectonicsSecondsPerStep60k40),
			SoftPaperRatioTarget,
			*JsonString(SafeRatio(IIID8SecondsPerStep, PaperTable2TotalTectonicsSecondsPerStep60k40) <= SoftPaperRatioTarget ? TEXT("within_target") : TEXT("over_target"))));

		const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
		FFileHelper::SaveStringToFile(
			FString::Join(JsonLines, TEXT("\n")) + TEXT("\n"),
			*MetricsPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

		const FString Report = BuildTierPolicyReport(OutputRoot, Tier);
		const FString ReportPath = ResolveReportPath(Params);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
		FFileHelper::SaveStringToFile(Report, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

		UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIID8 tier policy report: %s"), *ReportPath);
		UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIID8 tier policy metrics: %s"), *MetricsPath);
		return 0;
	}

	FSlice55BypassResult BypassA;
	FSlice55BypassResult BypassB;
	const bool bBypassA = RunSlice55Bypass(0, BypassA);
	const bool bBypassB = RunSlice55Bypass(1, BypassB);

	FIIIBSignatureResult IIIBA;
	FIIIBSignatureResult IIIBB;
	const bool bIIIBA = RunIIIBLocalVsPairSignatureReplay(0, BypassA, BypassB, IIIBA);
	const bool bIIIBB = RunIIIBLocalVsPairSignatureReplay(1, BypassA, BypassB, IIIBB);

	FPrimaryReplayResult BaselineA = MakePrimaryReplayFromBypass(BypassA);
	FPrimaryReplayResult BaselineB = MakePrimaryReplayFromBypass(BypassB);
	FPrimaryReplayResult ActiveA;
	FPrimaryReplayResult ActiveB;
	const bool bActiveA = RunPrimaryReplay(0, true, ActiveA);
	const bool bReplay0QuantitativePass =
		bActiveA &&
		ActiveA.CollisionEventCount > 0 &&
		ActiveA.PolicyResolvedMultiHitCount == 0 &&
		ReductionFraction(BaselineA, ActiveA) >= RequiredReductionFraction &&
		CollisionAttributionFraction(BaselineA, ActiveA) >= RequiredCollisionAttributionFraction;
	const bool bRunActiveReplay1 =
		Params.Contains(TEXT("RunActiveReplay1")) ||
		(!Params.Contains(TEXT("SkipActiveReplay1")) && bReplay0QuantitativePass);
	const bool bActiveB = bRunActiveReplay1 ? RunPrimaryReplay(1, true, ActiveB) : false;
	if (!bRunActiveReplay1)
	{
		UE_LOG(LogTemp, Warning, TEXT("IIID8 active replay 1 skipped by context-aware policy. Replay 0 quantitative pass=%s."), bReplay0QuantitativePass ? TEXT("true") : TEXT("false"));
	}

	TArray<FString> JsonLines;
	JsonLines.Add(FString::Printf(
		TEXT("{\"kind\":\"validation_tier\",\"tier\":%s,\"integrated_replay_launched\":true,\"replay0_quantitative_pass\":%s,\"active_replay1_launched\":%s}"),
		*JsonString(TierName(Tier)),
		bReplay0QuantitativePass ? TEXT("true") : TEXT("false"),
		bRunActiveReplay1 ? TEXT("true") : TEXT("false")));
	JsonLines.Add(FString::Printf(
		TEXT("{\"kind\":\"slice55_bypass\",\"replay\":0,\"completed\":%s,\"state_hash\":%s,\"material_ledger_hash\":%s,\"seconds\":%.6f}"),
		bBypassA ? TEXT("true") : TEXT("false"),
		*JsonString(BypassA.FilterMetrics.StateHashAfter),
		*JsonString(BypassA.LedgerMetrics.MaterialLedgerHash),
		BypassA.Seconds));
	JsonLines.Add(FString::Printf(
		TEXT("{\"kind\":\"slice55_bypass\",\"replay\":1,\"completed\":%s,\"state_hash\":%s,\"material_ledger_hash\":%s,\"seconds\":%.6f}"),
		bBypassB ? TEXT("true") : TEXT("false"),
		*JsonString(BypassB.FilterMetrics.StateHashAfter),
		*JsonString(BypassB.LedgerMetrics.MaterialLedgerHash),
		BypassB.Seconds));
	JsonLines.Add(FString::Printf(
		TEXT("{\"kind\":\"iiib_signature\",\"replay\":0,\"completed\":%s,\"computed_signature\":%s,\"expected_signature\":%s,\"step\":%d,\"seconds\":%.6f}"),
		bIIIBA ? TEXT("true") : TEXT("false"),
		*JsonString(IIIBA.IndependentSignatureHash),
		*JsonString(ExpectedIIIBIndependentSignature),
		IIIBA.StepCount,
		IIIBA.Seconds));
	JsonLines.Add(FString::Printf(
		TEXT("{\"kind\":\"iiib_signature\",\"replay\":1,\"completed\":%s,\"computed_signature\":%s,\"expected_signature\":%s,\"step\":%d,\"seconds\":%.6f}"),
		bIIIBB ? TEXT("true") : TEXT("false"),
		*JsonString(IIIBB.IndependentSignatureHash),
		*JsonString(ExpectedIIIBIndependentSignature),
		IIIBB.StepCount,
		IIIBB.Seconds));
	AppendPrimaryJson(BaselineA, JsonLines);
	AppendPrimaryJson(BaselineB, JsonLines);
	AppendPrimaryJson(ActiveA, JsonLines);
	if (bRunActiveReplay1)
	{
		AppendPrimaryJson(ActiveB, JsonLines);
	}
	else
	{
		JsonLines.Add(TEXT("{\"kind\":\"primary_replay\",\"fixture\":\"iiid_active_primary_60k\",\"replay\":1,\"iiid_active\":true,\"completed\":false,\"skipped\":true,\"skip_reason\":\"context-aware skip: replay 0 failed or path marked investigation-only\"}"));
	}

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	FFileHelper::SaveStringToFile(
		FString::Join(JsonLines, TEXT("\n")) + TEXT("\n"),
		*MetricsPath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	const FString Report = BuildReport(OutputRoot, Tier, BypassA, BypassB, IIIBA, IIIBB, BaselineA, BaselineB, ActiveA, ActiveB, bRunActiveReplay1);
	const FString ReportPath = ResolveReportPath(Params);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	FFileHelper::SaveStringToFile(Report, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

	const bool bPass =
		bBypassA &&
		bBypassB &&
		bIIIBA &&
		bIIIBB &&
		BaselineA.bCompleted &&
		BaselineB.bCompleted &&
		bActiveA &&
		bRunActiveReplay1 &&
		bActiveB &&
		BypassPasses(BypassA, BypassB) &&
		IIIBIndependentSignaturePasses(IIIBA, IIIBB) &&
		PrimaryReplayStable(BaselineA, BaselineB) &&
		IIIDActivePasses(BaselineA, ActiveA, ActiveB);

	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIID8 report: %s"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIID8 output: %s"), *OutputRoot);
	return bPass ? 0 : 1;
}
