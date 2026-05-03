// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIICConsolidationCommandlet.h"

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
	constexpr int32 LocalDiscriminatorMaxSteps = 80;
	constexpr int32 FixtureSamples = 10000;
	constexpr int32 FixturePlates = 2;
	constexpr double VelocityMmPerYear = 66.6666666667;
	constexpr double SeedElevationKm = 2.5;
	constexpr double SlabPullSpeedMmPerYear = 8.0;
	constexpr double ReferenceVelocityMmPerYear = 100.0;
	constexpr double EarthRadiusKm = 6371.0;
	constexpr double DeltaTimeMa = 2.0;
	constexpr double GateTolerance = 1.0e-10;
	constexpr double GateToleranceKm = 1.0e-8;
	constexpr TCHAR ExpectedSlice55StateHash[] = TEXT("3b4a85366dab80db");
	constexpr TCHAR ExpectedSlice55MaterialLedgerHash[] = TEXT("bc3077100ba291b4");
	constexpr TCHAR ExpectedIIIBIndependentSignature[] = TEXT("bf8818a26ed7b1dc");

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

	double AngularSpeedRadiansPerStep(const double VelocityMmPerYearValue)
	{
		return VelocityMmPerYearValue * DeltaTimeMa / EarthRadiusKm;
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseIII"), TEXT("IIICConsolidation"), Stamp);
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
				TEXT("phase-iii-iiic-consolidated.md"));
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
		const bool bEnableProcessLayer,
		const bool bEnableSlabPull)
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
		Actor->PhaseIIICSlabPullSpeedMmPerYear = SlabPullSpeedMmPerYear;
		Actor->PhaseIIICReferenceVelocityMmPerYear = ReferenceVelocityMmPerYear;
		Actor->ConfigurePhaseIIICProcessLayer(bEnableProcessLayer, bEnableSlabPull);
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	bool ConfigureMixedFixture(ACarrierLabVisualizationActor& Actor)
	{
		return Actor.SetPlateContinentalForTest(0, true) &&
			Actor.SetPlateContinentalForTest(1, false) &&
			Actor.SetPlateElevationForTest(1, SeedElevationKm);
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

	struct FIIICReplayResult
	{
		FString Fixture;
		int32 Replay = 0;
		bool bCompleted = false;
		bool bProcessLayerEnabled = false;
		bool bSlabPullRequested = false;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIC1SubductingMarkAudit MarkAudit;
		FCarrierLabPhaseIIIC2ElevationAudit ElevationAudit;
		FCarrierLabPhaseIIIC3UpliftAudit UpliftAudit;
		FCarrierLabPhaseIIIC4SlabPullAudit SlabPullAudit;
		FCarrierLabPhaseIIIC5ElevationLedgerAudit LedgerAudit;
		FCarrierLabPhaseIIIB7HashClosureAudit ClosureAudit;
		double OracleRecordDeltaKm = 0.0;
		double OracleLedgerResidualKm = 0.0;
		double OracleActualResidualKm = 0.0;
		double UpliftAuditResidualKm = 0.0;
		double OracleMaxAxisResidual = 0.0;
		double OracleMaxAngularResidual = 0.0;
		double OracleMaxContributionResidual = 0.0;
		FString RollupSignature;
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
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, BaselineSamples, BaselinePlates, 0.30, false, false);
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
		ACarrierLabVisualizationActor* Actor = SpawnActor(*World, MixedSignalSamples, MixedSignalPlates, 0.50, false, false);
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
			A.TrackingAudit.ConvergenceTrackingHash == B.TrackingAudit.ConvergenceTrackingHash &&
			A.DistanceAudit.ConvergenceTrackingHash == B.DistanceAudit.ConvergenceTrackingHash &&
			A.MatrixAudit.ConvergenceTrackingHash == B.MatrixAudit.ConvergenceTrackingHash &&
			A.MatrixAudit.MatrixEvidenceHash == B.MatrixAudit.MatrixEvidenceHash &&
			A.PolarityAudit.PolarityHash == B.PolarityAudit.PolarityHash &&
			A.PolarityAudit.ConvergenceTrackingHash == B.PolarityAudit.ConvergenceTrackingHash &&
			A.PropagationAudit.ConvergenceTrackingHash == B.PropagationAudit.ConvergenceTrackingHash &&
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
			A.PropagationAudit.PropagationAddedCount > 0;
	}

	void ComputeLedgerOracle(FIIICReplayResult& Result)
	{
		double TrenchDeltaKm = 0.0;
		double UpliftDeltaKm = 0.0;
		for (const FCarrierLabPhaseIIIC5ElevationLedgerRecord& Record : Result.LedgerAudit.Records)
		{
			Result.OracleRecordDeltaKm += Record.DeltaKm;
			if (Record.LedgerClass == ECarrierLabPhaseIIIC5ElevationLedgerClass::TrenchVisibleElevation)
			{
				TrenchDeltaKm += Record.DeltaKm;
			}
			else if (Record.LedgerClass == ECarrierLabPhaseIIIC5ElevationLedgerClass::OverridingUplift)
			{
				UpliftDeltaKm += Record.DeltaKm;
			}
		}

		Result.OracleLedgerResidualKm = FMath::Abs(Result.OracleRecordDeltaKm - Result.LedgerAudit.LedgerVisibleElevationDeltaKm);
		Result.OracleActualResidualKm = Result.LedgerAudit.ActualVisibleElevationDeltaKm - Result.OracleRecordDeltaKm;
		Result.UpliftAuditResidualKm = Result.UpliftAudit.TotalAppliedDeltaKm - UpliftDeltaKm;
		Result.OracleLedgerResidualKm = FMath::Max(Result.OracleLedgerResidualKm, FMath::Abs(TrenchDeltaKm - Result.LedgerAudit.TrenchVisibleElevationDeltaKm));
		Result.OracleLedgerResidualKm = FMath::Max(Result.OracleLedgerResidualKm, FMath::Abs(UpliftDeltaKm - Result.LedgerAudit.UpliftVisibleElevationDeltaKm));
	}

	void ComputeSlabPullOracle(FIIICReplayResult& Result)
	{
		TMap<int32, FVector3d> ContributionSumsByPlate;
		for (const FCarrierLabPhaseIIIC4SlabPullContributionRecord& Record : Result.SlabPullAudit.Contributions)
		{
			const FVector3d Raw = FVector3d::CrossProduct(Record.PlateCenter, Record.FrontBarycenter);
			const FVector3d ExpectedUnit = Raw.SquaredLength() > UE_DOUBLE_SMALL_NUMBER
				? Raw.GetSafeNormal()
				: FVector3d::ZeroVector;
			Result.OracleMaxContributionResidual = FMath::Max(
				Result.OracleMaxContributionResidual,
				(ExpectedUnit - Record.ContributionUnit).Size());
			ContributionSumsByPlate.FindOrAdd(Record.PlateId) += ExpectedUnit;
		}

		const double PullAngularStep = AngularSpeedRadiansPerStep(Result.SlabPullAudit.SlabPullSpeedMmPerYear);
		const double MaxAngularStep = AngularSpeedRadiansPerStep(Result.SlabPullAudit.ReferenceVelocityMmPerYear);
		for (const FCarrierLabPhaseIIIC4SlabPullPlateRecord& Record : Result.SlabPullAudit.PlateRecords)
		{
			const FVector3d ContributionSum = ContributionSumsByPlate.Contains(Record.PlateId)
				? ContributionSumsByPlate[Record.PlateId]
				: FVector3d::ZeroVector;
			Result.OracleMaxContributionResidual = FMath::Max(
				Result.OracleMaxContributionResidual,
				(ContributionSum - Record.ContributionSum).Size());

			const FVector3d OldOmega = Record.OldAxis * Record.OldAngularSpeedRadiansPerStep;
			const FVector3d RawOmega = OldOmega + ContributionSum * PullAngularStep;
			const double RawSpeed = RawOmega.Size();
			FVector3d ExpectedOmega = RawOmega;
			if (RawSpeed > MaxAngularStep && RawSpeed > UE_DOUBLE_SMALL_NUMBER)
			{
				ExpectedOmega = RawOmega / RawSpeed * MaxAngularStep;
			}
			const double ExpectedSpeed = ExpectedOmega.Size();
			const FVector3d ExpectedAxis = ExpectedSpeed > UE_DOUBLE_SMALL_NUMBER
				? ExpectedOmega / ExpectedSpeed
				: Record.OldAxis;
			Result.OracleMaxAxisResidual = FMath::Max(Result.OracleMaxAxisResidual, (ExpectedAxis - Record.NewAxis).Size());
			Result.OracleMaxAngularResidual = FMath::Max(Result.OracleMaxAngularResidual, FMath::Abs(ExpectedSpeed - Record.NewAngularSpeedRadiansPerStep));
		}
	}

	FString ComputeReplayRollupSignature(const FIIICReplayResult& Result, const FSlice55BypassResult& Bypass)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, Result.Fixture);
		HashMix(Hash, Result.bProcessLayerEnabled ? 1ull : 0ull);
		HashMix(Hash, Result.bSlabPullRequested ? 1ull : 0ull);
		HashMixString(Hash, ExpectedIIIBIndependentSignature);
		HashMixString(Hash, Bypass.FilterMetrics.StateHashAfter);
		HashMixString(Hash, Bypass.LedgerMetrics.MaterialLedgerHash);
		HashMixString(Hash, Result.MarkAudit.SubductingMarkHash);
		HashMixString(Hash, Result.ElevationAudit.VisibleElevationHash);
		HashMixString(Hash, Result.ElevationAudit.HistoricalElevationHash);
		HashMixString(Hash, Result.UpliftAudit.UpliftHash);
		HashMixString(Hash, Result.SlabPullAudit.SlabPullHash);
		HashMixString(Hash, Result.SlabPullAudit.MotionHashBefore);
		HashMixString(Hash, Result.SlabPullAudit.MotionHashAfter);
		HashMixString(Hash, Result.LedgerAudit.ElevationLedgerHash);
		HashMixString(Hash, Result.LedgerAudit.CrustStateHash);
		HashMixString(Hash, Result.ClosureAudit.ComputedConvergenceTrackingHash);
		HashMixDouble(Hash, Result.LedgerAudit.ActualVisibleElevationDeltaKm);
		HashMixDouble(Hash, Result.LedgerAudit.LedgerVisibleElevationDeltaKm);
		HashMixDouble(Hash, Result.SlabPullAudit.MaxVelocityMmPerYear);
		return HashToString(Hash);
	}

	bool RunIIICReplay(
		const FString& Fixture,
		const int32 Replay,
		const ECarrierLabPhaseIIMotionFixture MotionFixture,
		const int32 SampleCount,
		const int32 PlateCount,
		const double ContinentalFraction,
		const bool bConfigureMixed,
		const bool bEnableProcessLayer,
		const bool bEnableSlabPull,
		const FSlice55BypassResult& Bypass,
		FIIICReplayResult& OutResult)
	{
		OutResult = FIIICReplayResult();
		OutResult.Fixture = Fixture;
		OutResult.Replay = Replay;
		OutResult.bProcessLayerEnabled = bEnableProcessLayer;
		OutResult.bSlabPullRequested = bEnableSlabPull;
		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			return false;
		}

		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(
			*World,
			SampleCount,
			PlateCount,
			ContinentalFraction,
			bEnableProcessLayer,
			bEnableSlabPull);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier() ||
			(bConfigureMixed && !ConfigureMixedFixture(*Actor)))
		{
			Actor->Destroy();
			return false;
		}

		Actor->ConfigurePhaseIIMotionFixture(MotionFixture);
		Actor->StepOnce();
		const bool bAudits =
			Actor->GetPhaseIIIC1SubductingMarkAudit(OutResult.MarkAudit) &&
			Actor->GetPhaseIIIC2ElevationAudit(OutResult.ElevationAudit) &&
			Actor->GetPhaseIIIC3UpliftAudit(OutResult.UpliftAudit) &&
			Actor->GetPhaseIIIC4SlabPullAudit(OutResult.SlabPullAudit) &&
			Actor->GetPhaseIIIC5ElevationLedgerAudit(OutResult.LedgerAudit) &&
			Actor->GetPhaseIIIB7HashClosureAudit(OutResult.ClosureAudit);
		if (bAudits)
		{
			ComputeLedgerOracle(OutResult);
			ComputeSlabPullOracle(OutResult);
			OutResult.RollupSignature = ComputeReplayRollupSignature(OutResult, Bypass);
		}

		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bCompleted = bAudits;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return bAudits;
	}

	bool BypassPasses(const FSlice55BypassResult& Result)
	{
		return Result.bCompleted &&
			Result.FilterMetrics.StateHashAfter == ExpectedSlice55StateHash &&
			Result.LedgerMetrics.MaterialLedgerHash == ExpectedSlice55MaterialLedgerHash &&
			Result.FilterMetrics.NoBoundaryPairMissCount == 0 &&
			Result.FilterMetrics.ProjectionHashAfter == Result.ClosureAudit.ProjectionHash &&
			Result.FilterMetrics.StateHashAfter == Result.ClosureAudit.StateHash &&
			Result.ClosureAudit.bMetricsHashMatchesComputed;
	}

	bool BypassReplayStable(const FSlice55BypassResult& A, const FSlice55BypassResult& B)
	{
		return BypassPasses(A) && BypassPasses(B) &&
			A.ContactMetrics.ContactLogHash == B.ContactMetrics.ContactLogHash &&
			A.LabelMetrics.TriangleLabelHash == B.LabelMetrics.TriangleLabelHash &&
			A.FilterMetrics.FilterDecisionHash == B.FilterMetrics.FilterDecisionHash &&
			A.FilterMetrics.ProjectionHashAfter == B.FilterMetrics.ProjectionHashAfter &&
			A.FilterMetrics.StateHashAfter == B.FilterMetrics.StateHashAfter &&
			A.ClosureAudit.CrustStateHash == B.ClosureAudit.CrustStateHash &&
			A.ClosureAudit.ComputedConvergenceTrackingHash == B.ClosureAudit.ComputedConvergenceTrackingHash &&
			A.LedgerMetrics.MaterialLedgerHash == B.LedgerMetrics.MaterialLedgerHash;
	}

	bool LedgerReconciles(const FIIICReplayResult& Result)
	{
		return Result.bCompleted &&
			Result.ClosureAudit.bMetricsHashMatchesComputed &&
			FMath::Abs(Result.OracleLedgerResidualKm) <= GateToleranceKm &&
			FMath::Abs(Result.OracleActualResidualKm) <= GateToleranceKm &&
			FMath::Abs(Result.LedgerAudit.VisibleElevationResidualKm) <= GateToleranceKm;
	}

	bool ProcessLayerPasses(const FIIICReplayResult& Result)
	{
		return LedgerReconciles(Result) &&
			Result.MarkAudit.bEnabled &&
			Result.ElevationAudit.bElevationSplitEnabled &&
			Result.UpliftAudit.bUpliftEnabled &&
			!Result.SlabPullAudit.bSlabPullEnabled &&
			Result.MarkAudit.MarkCount > 0 &&
			Result.LedgerAudit.TrenchRecordCount > 0 &&
			Result.LedgerAudit.UpliftRecordCount > 0 &&
			Result.SlabPullAudit.MotionHashBefore == Result.SlabPullAudit.MotionHashAfter &&
			FMath::Abs(Result.UpliftAuditResidualKm) <= GateToleranceKm;
	}

	bool DisabledPasses(const FIIICReplayResult& Result)
	{
		return LedgerReconciles(Result) &&
			!Result.MarkAudit.bEnabled &&
			!Result.ElevationAudit.bElevationSplitEnabled &&
			!Result.UpliftAudit.bUpliftEnabled &&
			!Result.SlabPullAudit.bSlabPullEnabled &&
			Result.MarkAudit.MarkCount == 0 &&
			Result.LedgerAudit.RecordCount == 0 &&
			Result.SlabPullAudit.MotionHashBefore == Result.SlabPullAudit.MotionHashAfter;
	}

	bool SlabPullPasses(const FIIICReplayResult& Result)
	{
		return LedgerReconciles(Result) &&
			Result.MarkAudit.MarkCount > 0 &&
			Result.SlabPullAudit.bSlabPullEnabled &&
			Result.SlabPullAudit.ContributionCount > 0 &&
			Result.SlabPullAudit.AffectedPlateCount > 0 &&
			Result.SlabPullAudit.MotionHashBefore != Result.SlabPullAudit.MotionHashAfter &&
			Result.SlabPullAudit.MaxVelocityMmPerYear <= ReferenceVelocityMmPerYear + 1.0e-6 &&
			Result.OracleMaxAxisResidual <= GateTolerance &&
			Result.OracleMaxAngularResidual <= GateTolerance &&
			Result.OracleMaxContributionResidual <= GateTolerance;
	}

	bool NegativePasses(const FIIICReplayResult& Result)
	{
		return LedgerReconciles(Result) &&
			Result.MarkAudit.MarkCount == 0 &&
			Result.LedgerAudit.RecordCount == 0 &&
			Result.SlabPullAudit.ContributionCount == 0 &&
			Result.SlabPullAudit.MotionHashBefore == Result.SlabPullAudit.MotionHashAfter;
	}

	FString BypassJson(const FSlice55BypassResult& Result)
	{
		return FString::Printf(
			TEXT("{\"kind\":\"slice55_bypass\",\"replay\":%d,\"completed\":%s,\"contact_hash\":%s,\"label_hash\":%s,\"filter_decision_hash\":%s,\"projection_hash\":%s,\"state_hash\":%s,\"crust_hash\":%s,\"material_ledger_hash\":%s,\"convergence_metrics_hash\":%s,\"convergence_computed_hash\":%s,\"convergence_closure_matches\":%s,\"persistent_mark_inputs\":%d,\"no_boundary_pair_miss_count\":%d,\"seconds\":%.6f}"),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			*JsonString(Result.ContactMetrics.ContactLogHash),
			*JsonString(Result.LabelMetrics.TriangleLabelHash),
			*JsonString(Result.FilterMetrics.FilterDecisionHash),
			*JsonString(Result.FilterMetrics.ProjectionHashAfter),
			*JsonString(Result.FilterMetrics.StateHashAfter),
			*JsonString(Result.ClosureAudit.CrustStateHash),
			*JsonString(Result.LedgerMetrics.MaterialLedgerHash),
			*JsonString(Result.ClosureAudit.MetricsConvergenceTrackingHash),
			*JsonString(Result.ClosureAudit.ComputedConvergenceTrackingHash),
			Result.ClosureAudit.bMetricsHashMatchesComputed ? TEXT("true") : TEXT("false"),
			Result.FilterMetrics.PersistentSubductingMarkInputCount,
			Result.FilterMetrics.NoBoundaryPairMissCount,
			Result.Seconds);
	}

	FString IIIBSignatureJson(const FIIIBSignatureResult& Result)
	{
		return FString::Printf(
			TEXT("{\"kind\":\"iiib_independent_signature_replay\",\"fixture\":%s,\"replay\":%d,\"completed\":%s,\"step\":%d,\"pair_signed_convergence_velocity\":%.12f,\"accepted_local_positive_hits\":%d,\"rejected_local_non_positive_hits\":%d,\"matrix_pairs\":%d,\"polarity_decisions\":%d,\"propagation_seed_hits\":%d,\"propagation_added\":%d,\"expected_signature\":%s,\"computed_signature\":%s,\"active_component_hash\":%s,\"distance_component_hash\":%s,\"matrix_evidence_component_hash\":%s,\"polarity_component_hash\":%s,\"propagation_component_hash\":%s,\"closure_component_hash\":%s,\"slice55_component_hash\":%s,\"closure_hash\":%s,\"closure_matches\":%s,\"seconds\":%.6f}"),
			*JsonString(Result.FixtureName),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.StepCount,
			Result.PairSignedConvergenceVelocity,
			Result.MatrixAudit.AcceptedLocalPositiveHitCount,
			Result.MatrixAudit.RejectedLocalNonPositiveHitCount,
			Result.MatrixAudit.MatrixPairCount,
			Result.PolarityAudit.DecisionCount,
			Result.PropagationAudit.PropagationSeedHitCount,
			Result.PropagationAudit.PropagationAddedCount,
			*JsonString(ExpectedIIIBIndependentSignature),
			*JsonString(Result.IndependentSignatureHash),
			*JsonString(Result.ActiveListComponentHash),
			*JsonString(Result.DistanceComponentHash),
			*JsonString(Result.MatrixEvidenceComponentHash),
			*JsonString(Result.PolarityComponentHash),
			*JsonString(Result.PropagationComponentHash),
			*JsonString(Result.ClosureComponentHash),
			*JsonString(Result.Slice55ComponentHash),
			*JsonString(Result.ClosureAudit.ComputedConvergenceTrackingHash),
			Result.ClosureAudit.bMetricsHashMatchesComputed ? TEXT("true") : TEXT("false"),
			Result.Seconds);
	}

	FString ReplayJson(const FIIICReplayResult& Result)
	{
		return FString::Printf(
			TEXT("{\"kind\":\"iiic_consolidation\",\"fixture\":%s,\"replay\":%d,\"completed\":%s,\"process_layer\":%s,\"slab_pull_requested\":%s,\"marks\":%d,\"trench_records\":%d,\"uplift_records\":%d,\"ledger_records\":%d,\"actual_delta_km\":%.15f,\"ledger_delta_km\":%.15f,\"ledger_residual_km\":%.15e,\"uplift_audit_residual_km\":%.15e,\"slab_contributions\":%d,\"affected_plates\":%d,\"max_velocity_mm_per_year\":%.12f,\"oracle_axis_residual\":%.15e,\"oracle_angular_residual\":%.15e,\"oracle_contribution_residual\":%.15e,\"motion_hash_before\":%s,\"motion_hash_after\":%s,\"mark_hash\":%s,\"elevation_hash\":%s,\"uplift_hash\":%s,\"ledger_hash\":%s,\"slab_hash\":%s,\"rollup_signature\":%s,\"closure_hash\":%s,\"closure_matches\":%s,\"seconds\":%.6f}"),
			*JsonString(Result.Fixture),
			Result.Replay,
			Result.bCompleted ? TEXT("true") : TEXT("false"),
			Result.bProcessLayerEnabled ? TEXT("true") : TEXT("false"),
			Result.bSlabPullRequested ? TEXT("true") : TEXT("false"),
			Result.MarkAudit.MarkCount,
			Result.LedgerAudit.TrenchRecordCount,
			Result.LedgerAudit.UpliftRecordCount,
			Result.LedgerAudit.RecordCount,
			Result.LedgerAudit.ActualVisibleElevationDeltaKm,
			Result.LedgerAudit.LedgerVisibleElevationDeltaKm,
			Result.LedgerAudit.VisibleElevationResidualKm,
			Result.UpliftAuditResidualKm,
			Result.SlabPullAudit.ContributionCount,
			Result.SlabPullAudit.AffectedPlateCount,
			Result.SlabPullAudit.MaxVelocityMmPerYear,
			Result.OracleMaxAxisResidual,
			Result.OracleMaxAngularResidual,
			Result.OracleMaxContributionResidual,
			*JsonString(Result.SlabPullAudit.MotionHashBefore),
			*JsonString(Result.SlabPullAudit.MotionHashAfter),
			*JsonString(Result.MarkAudit.SubductingMarkHash),
			*JsonString(Result.ElevationAudit.VisibleElevationHash),
			*JsonString(Result.UpliftAudit.UpliftHash),
			*JsonString(Result.LedgerAudit.ElevationLedgerHash),
			*JsonString(Result.SlabPullAudit.SlabPullHash),
			*JsonString(Result.RollupSignature),
			*JsonString(Result.ClosureAudit.ComputedConvergenceTrackingHash),
			Result.ClosureAudit.bMetricsHashMatchesComputed ? TEXT("true") : TEXT("false"),
			Result.Seconds);
	}

	FString BuildReport(
		const FString& OutputRoot,
		const FSlice55BypassResult& BypassA,
		const FSlice55BypassResult& BypassB,
		const FIIIBSignatureResult& IIIBSignatureA,
		const FIIIBSignatureResult& IIIBSignatureB,
		const FIIICReplayResult& ProcessA,
		const FIIICReplayResult& ProcessB,
		const FIIICReplayResult& Disabled,
		const FIIICReplayResult& SlabPullA,
		const FIIICReplayResult& SlabPullB,
		const FIIICReplayResult& ZeroMotion,
		const FIIICReplayResult& SinglePlate,
		const FIIICReplayResult& ForcedDivergenceNoSubduction)
	{
		const bool bBypassPass = BypassReplayStable(BypassA, BypassB);
		const bool bProcessPass =
			ProcessLayerPasses(ProcessA) &&
			ProcessLayerPasses(ProcessB) &&
			ProcessA.RollupSignature == ProcessB.RollupSignature &&
			ProcessA.MarkAudit.SubductingMarkHash == ProcessB.MarkAudit.SubductingMarkHash &&
			ProcessA.LedgerAudit.ElevationLedgerHash == ProcessB.LedgerAudit.ElevationLedgerHash;
		const bool bDisabledPass = DisabledPasses(Disabled);
		const bool bSlabPass =
			SlabPullPasses(SlabPullA) &&
			SlabPullPasses(SlabPullB) &&
			SlabPullA.RollupSignature == SlabPullB.RollupSignature &&
			SlabPullA.SlabPullAudit.SlabPullHash == SlabPullB.SlabPullAudit.SlabPullHash &&
			SlabPullA.SlabPullAudit.MotionHashAfter == SlabPullB.SlabPullAudit.MotionHashAfter;
		const bool bSlabDifferential =
			bSlabPass &&
			ProcessA.SlabPullAudit.MotionHashBefore == SlabPullA.SlabPullAudit.MotionHashBefore &&
			ProcessA.SlabPullAudit.MotionHashAfter != SlabPullA.SlabPullAudit.MotionHashAfter;
		const bool bNegativePass =
			NegativePasses(ZeroMotion) &&
			NegativePasses(SinglePlate) &&
			NegativePasses(ForcedDivergenceNoSubduction);
		const bool bIIIBSignatureGate = IIIBIndependentSignaturePasses(IIIBSignatureA, IIIBSignatureB);
		const bool bAllPass = bBypassPass && bProcessPass && bDisabledPass && bSlabPass && bSlabDifferential && bNegativePass && bIIIBSignatureGate;

		FString Report;
		Report += TEXT("# Phase III Sub-Phase IIIC Consolidated Checkpoint\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("Status: IIIC.1-IIIC.5 consolidated. This checkpoint closes the subduction-mutation sub-phase by proving the consolidated process-layer preset, the disabled regression path, and the slab-pull on/off differential. It does not add collision, rifting, erosion, terrain displacement, projection-derived ownership, or any new resampling mutation path.\n\n");
		Report += TEXT("Consolidated control shape: `ConfigurePhaseIIICProcessLayer(true, false)` enables subducting-triangle marks, visible/historical elevation split, and overriding-plate uplift. Slab pull remains a separate authority-feedback switch and stays off unless requested with `ConfigurePhaseIIICProcessLayer(true, true)`.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		Report += FString::Printf(
			TEXT("| Slice 5.5 bypass | %s | projection `%s` / `%s`, state `%s` / `%s`, crust `%s` / `%s`, material `%s` / `%s`, convergence `%s` / `%s`, no-boundary fallback %d/%d |\n"),
			*PassFail(bBypassPass),
			*BypassA.FilterMetrics.ProjectionHashAfter,
			*BypassB.FilterMetrics.ProjectionHashAfter,
			*BypassA.FilterMetrics.StateHashAfter,
			*BypassB.FilterMetrics.StateHashAfter,
			*BypassA.ClosureAudit.CrustStateHash,
			*BypassB.ClosureAudit.CrustStateHash,
			*BypassA.LedgerMetrics.MaterialLedgerHash,
			*BypassB.LedgerMetrics.MaterialLedgerHash,
			*BypassA.ClosureAudit.ComputedConvergenceTrackingHash,
			*BypassB.ClosureAudit.ComputedConvergenceTrackingHash,
			BypassA.FilterMetrics.NoBoundaryPairMissCount,
			BypassB.FilterMetrics.NoBoundaryPairMissCount);
		Report += FString::Printf(
			TEXT("| IIIB independent signature gate | %s | computed `%s` / `%s`, expected `%s`; closure `%s` / `%s` |\n"),
			*PassFail(bIIIBSignatureGate),
			*IIIBSignatureA.IndependentSignatureHash,
			*IIIBSignatureB.IndependentSignatureHash,
			ExpectedIIIBIndependentSignature,
			*IIIBSignatureA.ClosureAudit.ComputedConvergenceTrackingHash,
			*IIIBSignatureB.ClosureAudit.ComputedConvergenceTrackingHash);
		Report += FString::Printf(
			TEXT("| Consolidated process layer, slab pull off | %s | marks %d / %d, trench %d / %d, uplift %d / %d, rollup `%s` |\n"),
			*PassFail(bProcessPass),
			ProcessA.MarkAudit.MarkCount,
			ProcessB.MarkAudit.MarkCount,
			ProcessA.LedgerAudit.TrenchRecordCount,
			ProcessB.LedgerAudit.TrenchRecordCount,
			ProcessA.LedgerAudit.UpliftRecordCount,
			ProcessB.LedgerAudit.UpliftRecordCount,
			*ProcessA.RollupSignature);
		Report += FString::Printf(
			TEXT("| Disabled process layer | %s | marks %d, records %d, motion `%s` -> `%s` |\n"),
			*PassFail(bDisabledPass),
			Disabled.MarkAudit.MarkCount,
			Disabled.LedgerAudit.RecordCount,
			*Disabled.SlabPullAudit.MotionHashBefore,
			*Disabled.SlabPullAudit.MotionHashAfter);
		Report += FString::Printf(
			TEXT("| Slab pull opt-in differential | %s | contributions %d / %d, motion `%s` -> `%s`, rollup `%s` |\n"),
			*PassFail(bSlabDifferential),
			SlabPullA.SlabPullAudit.ContributionCount,
			SlabPullB.SlabPullAudit.ContributionCount,
			*SlabPullA.SlabPullAudit.MotionHashBefore,
			*SlabPullA.SlabPullAudit.MotionHashAfter,
			*SlabPullA.RollupSignature);
		Report += FString::Printf(
			TEXT("| Slab pull independent oracle | %s | axis %.12e, angular %.12e, contribution %.12e, max velocity %.6f mm/yr |\n"),
			*PassFail(bSlabPass),
			SlabPullA.OracleMaxAxisResidual,
			SlabPullA.OracleMaxAngularResidual,
			SlabPullA.OracleMaxContributionResidual,
			SlabPullA.SlabPullAudit.MaxVelocityMmPerYear);
		Report += FString::Printf(
			TEXT("| Negative controls | %s | zero %d, single %d, divergence-no-subduction %d ledger records |\n"),
			*PassFail(bNegativePass),
			ZeroMotion.LedgerAudit.RecordCount,
			SinglePlate.LedgerAudit.RecordCount,
			ForcedDivergenceNoSubduction.LedgerAudit.RecordCount);

		Report += TEXT("\n## Primary Replays\n\n");
		Report += TEXT("| Fixture | Replay | Process layer | Slab pull | Marks | Trench records | Uplift records | Ledger records | Actual delta km | Ledger residual km | Motion before | Motion after | Rollup | Seconds |\n");
		Report += TEXT("|---|---:|---|---|---:|---:|---:|---:|---:|---:|---|---|---|---:|\n");
		auto AddReplayRow = [&Report](const FIIICReplayResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %s | %s | %d | %d | %d | %d | %.12f | %.12e | `%s` | `%s` | `%s` | %.3f |\n"),
				*Result.Fixture,
				Result.Replay,
				Result.bProcessLayerEnabled ? TEXT("on") : TEXT("off"),
				Result.bSlabPullRequested ? TEXT("on") : TEXT("off"),
				Result.MarkAudit.MarkCount,
				Result.LedgerAudit.TrenchRecordCount,
				Result.LedgerAudit.UpliftRecordCount,
				Result.LedgerAudit.RecordCount,
				Result.LedgerAudit.ActualVisibleElevationDeltaKm,
				Result.LedgerAudit.VisibleElevationResidualKm,
				*Result.SlabPullAudit.MotionHashBefore,
				*Result.SlabPullAudit.MotionHashAfter,
				*Result.RollupSignature,
				Result.Seconds);
		};
		AddReplayRow(ProcessA);
		AddReplayRow(ProcessB);
		AddReplayRow(Disabled);
		AddReplayRow(SlabPullA);
		AddReplayRow(SlabPullB);
		AddReplayRow(ZeroMotion);
		AddReplayRow(SinglePlate);
		AddReplayRow(ForcedDivergenceNoSubduction);

		Report += TEXT("\n## IIIB Regression Signature\n\n");
		Report += FString::Printf(
			TEXT("This is a replay of the IIIB hardening discriminator fixture inside the IIIC consolidation commandlet. It compares the computed IIIB independent signature directly to the accepted `%s` token; closure-hash self-recomputation alone is not sufficient for this gate. This token supersedes the original IIIB checkpoint token because the pre-IIID hardening added the zero no-boundary-pair fallback metric to the Slice 5.5 component of the independent signature.\n\n"),
			ExpectedIIIBIndependentSignature);
		Report += TEXT("| Replay | Step | Pair sign | Accepted local positives | Rejected local non-positives | Matrix pairs | Decisions | Propagation seeds | Propagation added | Computed signature | Expected signature | Slice 5.5 component | Closure component |\n");
		Report += TEXT("|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|---|---|\n");
		auto AddIIIBRow = [&Report](const FIIIBSignatureResult& Result)
		{
			Report += FString::Printf(
				TEXT("| %d | %d | %.12f | %d | %d | %d | %d | %d | %d | `%s` | `%s` | `%s` | `%s` |\n"),
				Result.Replay,
				Result.StepCount,
				Result.PairSignedConvergenceVelocity,
				Result.MatrixAudit.AcceptedLocalPositiveHitCount,
				Result.MatrixAudit.RejectedLocalNonPositiveHitCount,
				Result.MatrixAudit.MatrixPairCount,
				Result.PolarityAudit.DecisionCount,
				Result.PropagationAudit.PropagationSeedHitCount,
				Result.PropagationAudit.PropagationAddedCount,
				*Result.IndependentSignatureHash,
				ExpectedIIIBIndependentSignature,
				*Result.Slice55ComponentHash,
				*Result.ClosureComponentHash);
		};
		AddIIIBRow(IIIBSignatureA);
		AddIIIBRow(IIIBSignatureB);

		Report += TEXT("\n## Sub-Slice Closure\n\n");
		Report += TEXT("| Slice | Consolidated result |\n");
		Report += TEXT("|---|---|\n");
		Report += FString::Printf(TEXT("| IIIC.1 marks | %s: consolidated process layer emits %d deterministic subducting-triangle marks. |\n"), *PassFail(ProcessA.MarkAudit.MarkCount > 0 && ProcessA.MarkAudit.SubductingMarkHash == ProcessB.MarkAudit.SubductingMarkHash), ProcessA.MarkAudit.MarkCount);
		Report += FString::Printf(TEXT("| IIIC.2 elevation split | %s: trench line emits %d records and visible/historical hashes are stable. |\n"), *PassFail(ProcessA.LedgerAudit.TrenchRecordCount > 0 && ProcessA.ElevationAudit.VisibleElevationHash == ProcessB.ElevationAudit.VisibleElevationHash), ProcessA.LedgerAudit.TrenchRecordCount);
		Report += FString::Printf(TEXT("| IIIC.3 uplift | %s: uplift line emits %d records with residual %.12e km. |\n"), *PassFail(ProcessA.LedgerAudit.UpliftRecordCount > 0 && FMath::Abs(ProcessA.UpliftAuditResidualKm) <= GateToleranceKm), ProcessA.LedgerAudit.UpliftRecordCount, ProcessA.UpliftAuditResidualKm);
		Report += FString::Printf(TEXT("| IIIC.4 slab pull | %s: opt-in path emits %d contributions and changes motion hash without affecting the off path. |\n"), *PassFail(bSlabDifferential), SlabPullA.SlabPullAudit.ContributionCount);
		Report += FString::Printf(TEXT("| IIIC.5 elevation ledger | %s: full ledger has %d records and residual %.12e km. |\n"), *PassFail(ProcessLayerPasses(ProcessA)), ProcessA.LedgerAudit.RecordCount, ProcessA.LedgerAudit.VisibleElevationResidualKm);

		Report += TEXT("\n## Scope Notes\n\n");
		Report += TEXT("- IIIC is now consolidated as a process-layer preset plus an independent slab-pull switch. The slice-level booleans remain for commandlet fixtures and narrow regression tests, but new Phase III code should prefer `ConfigurePhaseIIICProcessLayer` for the normal IIIC stack.\n");
		Report += TEXT("- Slab pull remains default-off and opt-in because it is the first authority-feedback loop. Turning it on is deterministic and oracle-checked here, but future paper-faithful long-horizon runs must still declare it explicitly.\n");
		Report += TEXT("- Stage 1.5 and Slice 5.5 open evidence remains preserved. IIIC explains subduction/elevation/slab-pull behavior only; it does not claim carrier success, collision resolution, rifting, divergent oceanic generation, erosion, or terrain morphology.\n");
		Report += TEXT("- The disabled process-layer replay is a fixed-fixture regression, not a proof that every possible run is unchanged.\n\n");
		Report += FString::Printf(TEXT("Overall result: **%s**.\n\n"), *PassFail(bAllPass));
		Report += TEXT("## Recommendation\n\n");
		Report += bAllPass
			? TEXT("IIIC consolidation passes. Phase IIID planning/implementation may begin after user review.\n")
			: TEXT("IIIC consolidation fails. Pause before Phase IIID and investigate the failing gate.\n");
		return Report;
	}
}

UCarrierLabPhaseIIICConsolidationCommandlet::UCarrierLabPhaseIIICConsolidationCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIICConsolidationCommandlet::Main(const FString& Params)
{
	const FString OutputRoot = GetOutputRoot(Params);
	const FString ReportPath = ResolveReportPath(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);

	FSlice55BypassResult BypassA;
	FSlice55BypassResult BypassB;
	const bool bBypassA = RunSlice55Bypass(0, BypassA);
	const bool bBypassB = RunSlice55Bypass(1, BypassB);

	FIIIBSignatureResult IIIBSignatureA;
	FIIIBSignatureResult IIIBSignatureB;
	const bool bIIIBSignatureA = RunIIIBLocalVsPairSignatureReplay(0, BypassA, BypassB, IIIBSignatureA);
	const bool bIIIBSignatureB = RunIIIBLocalVsPairSignatureReplay(1, BypassA, BypassB, IIIBSignatureB);

	FIIICReplayResult ProcessA;
	FIIICReplayResult ProcessB;
	const bool bProcessA = RunIIICReplay(
		TEXT("process_layer_default"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		false,
		BypassA,
		ProcessA);
	const bool bProcessB = RunIIICReplay(
		TEXT("process_layer_default"),
		1,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		false,
		BypassA,
		ProcessB);

	FIIICReplayResult Disabled;
	const bool bDisabled = RunIIICReplay(
		TEXT("process_layer_disabled"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		false,
		false,
		BypassA,
		Disabled);

	FIIICReplayResult SlabPullA;
	FIIICReplayResult SlabPullB;
	const bool bSlabA = RunIIICReplay(
		TEXT("slab_pull_opt_in"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		true,
		BypassA,
		SlabPullA);
	const bool bSlabB = RunIIICReplay(
		TEXT("slab_pull_opt_in"),
		1,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		true,
		BypassA,
		SlabPullB);

	FIIICReplayResult ZeroMotion;
	FIIICReplayResult SinglePlate;
	FIIICReplayResult ForcedDivergenceNoSubduction;
	const bool bZero = RunIIICReplay(
		TEXT("zero_motion"),
		0,
		ECarrierLabPhaseIIMotionFixture::Zero,
		FixtureSamples,
		FixturePlates,
		0.50,
		true,
		true,
		false,
		BypassA,
		ZeroMotion);
	const bool bSingle = RunIIICReplay(
		TEXT("single_plate"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedConvergence,
		FixtureSamples,
		1,
		1.00,
		false,
		true,
		false,
		BypassA,
		SinglePlate);
	const bool bDivergence = RunIIICReplay(
		TEXT("forced_divergence_no_subduction"),
		0,
		ECarrierLabPhaseIIMotionFixture::ForcedDivergence,
		FixtureSamples,
		FixturePlates,
		1.00,
		false,
		true,
		false,
		BypassA,
		ForcedDivergenceNoSubduction);

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	TArray<FString> JsonLines;
	JsonLines.Add(BypassJson(BypassA));
	JsonLines.Add(BypassJson(BypassB));
	JsonLines.Add(IIIBSignatureJson(IIIBSignatureA));
	JsonLines.Add(IIIBSignatureJson(IIIBSignatureB));
	JsonLines.Add(ReplayJson(ProcessA));
	JsonLines.Add(ReplayJson(ProcessB));
	JsonLines.Add(ReplayJson(Disabled));
	JsonLines.Add(ReplayJson(SlabPullA));
	JsonLines.Add(ReplayJson(SlabPullB));
	JsonLines.Add(ReplayJson(ZeroMotion));
	JsonLines.Add(ReplayJson(SinglePlate));
	JsonLines.Add(ReplayJson(ForcedDivergenceNoSubduction));
	FFileHelper::SaveStringArrayToFile(JsonLines, *MetricsPath);

	const FString Report = BuildReport(
		OutputRoot,
		BypassA,
		BypassB,
		IIIBSignatureA,
		IIIBSignatureB,
		ProcessA,
		ProcessB,
		Disabled,
		SlabPullA,
		SlabPullB,
		ZeroMotion,
		SinglePlate,
		ForcedDivergenceNoSubduction);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	const bool bPass =
		bBypassA && bBypassB &&
		bIIIBSignatureA && bIIIBSignatureB &&
		bProcessA && bProcessB &&
		bDisabled &&
		bSlabA && bSlabB &&
		bZero && bSingle && bDivergence &&
		BypassPasses(BypassA) &&
		BypassPasses(BypassB) &&
		BypassReplayStable(BypassA, BypassB) &&
		IIIBIndependentSignaturePasses(IIIBSignatureA, IIIBSignatureB) &&
		ProcessLayerPasses(ProcessA) &&
		ProcessLayerPasses(ProcessB) &&
		ProcessA.RollupSignature == ProcessB.RollupSignature &&
		DisabledPasses(Disabled) &&
		SlabPullPasses(SlabPullA) &&
		SlabPullPasses(SlabPullB) &&
		SlabPullA.RollupSignature == SlabPullB.RollupSignature &&
		ProcessA.SlabPullAudit.MotionHashBefore == SlabPullA.SlabPullAudit.MotionHashBefore &&
		ProcessA.SlabPullAudit.MotionHashAfter != SlabPullA.SlabPullAudit.MotionHashAfter &&
		NegativePasses(ZeroMotion) &&
		NegativePasses(SinglePlate) &&
		NegativePasses(ForcedDivergenceNoSubduction);

	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIIC consolidation report: %s"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab IIIC consolidation metrics: %s"), *MetricsPath);
	return bPass ? 0 : 1;
}
