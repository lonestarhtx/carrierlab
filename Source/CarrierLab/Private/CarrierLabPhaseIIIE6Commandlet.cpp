// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIE6Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr double ContinentalOverwriteThreshold = 1.0e-4;

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
		for (const TCHAR Ch : Value)
		{
			HashMix(Hash, static_cast<uint64>(Ch));
		}
		HashMix(Hash, 0xffull);
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

	FString HoldPassFail(const bool bPass, const bool bHold)
	{
		return bHold ? TEXT("hold") : PassFail(bPass);
	}

	FVector3d UnitFromLonLat(const double LonDeg, const double LatDeg)
	{
		const double Lon = FMath::DegreesToRadians(LonDeg);
		const double Lat = FMath::DegreesToRadians(LatDeg);
		const double CosLat = FMath::Cos(Lat);
		return FVector3d(CosLat * FMath::Cos(Lon), CosLat * FMath::Sin(Lon), FMath::Sin(Lat));
	}

	FString SelectionClassName(const ECarrierLabPhaseIIIE3SelectionClass SelectionClass)
	{
		switch (SelectionClass)
		{
		case ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap:
			return TEXT("no-hit divergent gap");
		case ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit:
			return TEXT("resolved single hit");
		case ECarrierLabPhaseIIIE3SelectionClass::DivergentGapAfterFiltering:
			return TEXT("divergent gap after filtering");
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit:
			return TEXT("unresolved same-material multi-hit");
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedMixedMaterialMultiHit:
			return TEXT("unresolved mixed-material multi-hit");
		case ECarrierLabPhaseIIIE3SelectionClass::UnresolvedThirdPlateMultiHit:
			return TEXT("unresolved third-plate multi-hit");
		default:
			return TEXT("unknown");
		}
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
				TEXT("IIIE6"));
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
				TEXT("phase-iii-slice-iiie6-report.md"));
		}
		else if (FPaths::IsRelative(ReportPath))
		{
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), ReportPath);
		}
		return FPaths::ConvertRelativePathToFull(ReportPath);
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
		Actor->Seed = 42;
		Actor->ContinentalPlateFraction = FMath::Clamp(ContinentalPlateFraction, 0.0, 1.0);
		Actor->bEnableNaturalResamplingEvents = false;
		Actor->ConfigurePhaseIIICProcessLayer(false, false);
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	void AddPointEdge(
		TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe>& Edges,
		const int32 PlateId,
		const double LonDeg,
		const double LatDeg,
		const double Elevation)
	{
		FCarrierLabPhaseIIIE2BoundaryEdgeProbe Edge;
		Edge.PlateId = PlateId;
		Edge.StartUnitPosition = UnitFromLonLat(LonDeg, LatDeg);
		Edge.EndUnitPosition = Edge.StartUnitPosition;
		Edge.StartElevation = Elevation;
		Edge.EndElevation = Elevation;
		Edges.Add(Edge);
	}

	TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe> MakeDivergentBoundaryEdges()
	{
		TArray<FCarrierLabPhaseIIIE2BoundaryEdgeProbe> Edges;
		AddPointEdge(Edges, 0, -20.0, 0.0, -4.0);
		AddPointEdge(Edges, 1, 20.0, 0.0, -2.0);
		return Edges;
	}

	TArray<FCarrierLabPhaseIIIE3CandidateProbe> MakeFilteredCandidateProbes()
	{
		TArray<FCarrierLabPhaseIIIE3CandidateProbe> Probes;
		FCarrierLabPhaseIIIE3CandidateProbe& SubductingProbe = Probes.AddDefaulted_GetRef();
		SubductingProbe.PlateId = 0;
		SubductingProbe.LocalTriangleId = 0;
		SubductingProbe.ContinentalFraction = 0.0;
		SubductingProbe.Elevation = -3.0;
		SubductingProbe.FilterReason = ECarrierLabPhaseIIIE3FilterReason::Subducting;

		FCarrierLabPhaseIIIE3CandidateProbe& CollisionProbe = Probes.AddDefaulted_GetRef();
		CollisionProbe.PlateId = 1;
		CollisionProbe.LocalTriangleId = 0;
		CollisionProbe.ContinentalFraction = 0.0;
		CollisionProbe.Elevation = -4.0;
		CollisionProbe.FilterReason = ECarrierLabPhaseIIIE3FilterReason::CollisionPending;
		return Probes;
	}

	struct FRemeshEventFixtureSpec
	{
		FString Name;
		FString Purpose;
		int32 SampleCount = 96;
		int32 PlateCount = 2;
		bool bPreExistingContinental = false;
		bool bUseFilteredGapRoute = false;
		bool bExpectOverwriteHold = false;
	};

	struct FRemeshEventLedgerResult
	{
		FString Name;
		FString Purpose;
		bool bRan = false;
		bool bPass = false;
		bool bHold = false;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIE3SelectionRecord SelectionRecord;
		FCarrierLabPhaseIIIE4OceanicGenerationRecord OceanicRecord;
		FCarrierLabPhaseIIIE5TopologyRebuildAudit TopologyAudit;
		int32 GeneratedRecordCount = 0;
		int32 NewOceanicCreationCount = 0;
		int32 OverwrittenByRidgeGenerationCount = 0;
		int32 GeophysicsDerivedZGammaCount = 0;
		int32 PaperFaithfulZGammaCount = 0;
		double NewOceanicSampleEquivalent = 0.0;
		double OverwrittenContinentalFraction = 0.0;
		bool bSelectionDivergentRoute = false;
		bool bGenerationAllowed = false;
		bool bLedgerReconciles = false;
		bool bNoForbiddenPolicy = false;
		bool bQAndZGammaPreserved = false;
		FString EventHash;
	};

	struct FPostRebuildIIIBResult
	{
		bool bRan = false;
		bool bPass = false;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIE5TopologyRebuildAudit TopologyAudit;
		FCarrierLabPhaseIIIB6SeedMetrics SeedMetrics;
		FCarrierLabPhaseIIIB1TrackingAudit TrackingAudit;
		FCarrierLabPhaseIIIB2DistanceAudit DistanceAudit;
		FCarrierLabPhaseIIIB3SubductionMatrixAudit MatrixAudit;
		FCarrierLabPhaseIIIB6NeighborPropagationAudit PropagationAudit;
		FCarrierLabPhaseIIIB7HashClosureAudit ClosureAudit;
		FString TrackingHash;
	};

	FRemeshEventFixtureSpec MakeNoHitOceanicFixture()
	{
		FRemeshEventFixtureSpec Spec;
		Spec.Name = TEXT("No-hit live remesh event ledger");
		Spec.Purpose = TEXT("No raw ray candidates route through IIIE.4 generation, IIIE.5 topology rebuild, reset, and the new-oceanic ledger line.");
		Spec.bPreExistingContinental = false;
		Spec.bUseFilteredGapRoute = false;
		return Spec;
	}

	FRemeshEventFixtureSpec MakeFilterExhaustedOverwriteFixture()
	{
		FRemeshEventFixtureSpec Spec;
		Spec.Name = TEXT("Filter-exhausted continental overwrite hold");
		Spec.Purpose = TEXT("Filtered-out process-state hits may generate oceanic fields, but pre-existing continental material is ledgered as ridge overwrite hold, not new oceanic creation.");
		Spec.bPreExistingContinental = true;
		Spec.bUseFilteredGapRoute = true;
		Spec.bExpectOverwriteHold = true;
		return Spec;
	}

	void FinalizeEventHash(FRemeshEventLedgerResult& Result)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIIE6-remesh-event-ledger-v1"));
		HashMixString(Hash, Result.Name);
		HashMix(Hash, static_cast<uint64>(static_cast<uint8>(Result.SelectionRecord.SelectionClass)) + 1ull);
		HashMix(Hash, Result.SelectionRecord.bDivergentGapRoute ? 1ull : 0ull);
		HashMix(Hash, Result.OceanicRecord.bGeneratedOceanicCrust ? 1ull : 0ull);
		HashMix(Hash, Result.OceanicRecord.bUsedPolicyWinner ? 1ull : 0ull);
		HashMix(Hash, Result.OceanicRecord.bUsedPriorOwnerFallback ? 1ull : 0ull);
		HashMix(Hash, static_cast<uint64>(Result.GeneratedRecordCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.NewOceanicCreationCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.OverwrittenByRidgeGenerationCount + 1));
		HashMixDouble(Hash, Result.NewOceanicSampleEquivalent);
		HashMixDouble(Hash, Result.OverwrittenContinentalFraction);
		HashMix(Hash, static_cast<uint64>(Result.GeophysicsDerivedZGammaCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PaperFaithfulZGammaCount + 1));
		HashMixString(Hash, Result.TopologyAudit.AssignmentHash);
		HashMixString(Hash, Result.TopologyAudit.TopologyHash);
		HashMix(Hash, Result.TopologyAudit.ResetAudit.bProcessStateEmptyAfter ? 1ull : 0ull);
		HashMix(Hash, Result.bLedgerReconciles ? 1ull : 0ull);
		Result.EventHash = HashToString(Hash);
	}

	bool EvaluateRemeshEventFixture(
		const FRemeshEventFixtureSpec& Spec,
		FRemeshEventLedgerResult& OutResult,
		UWorld& World)
	{
		OutResult = FRemeshEventLedgerResult();
		OutResult.Name = Spec.Name;
		OutResult.Purpose = Spec.Purpose;
		OutResult.bHold = Spec.bExpectOverwriteHold;
		const double StartSeconds = FPlatformTime::Seconds();

		ACarrierLabVisualizationActor* Actor = SpawnActor(
			World,
			Spec.SampleCount,
			Spec.PlateCount,
			Spec.bPreExistingContinental ? 1.0 : 0.0);
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
		for (int32 PlateId = 0; PlateId < Spec.PlateCount; ++PlateId)
		{
			Actor->SetPlateContinentalForTest(PlateId, Spec.bPreExistingContinental);
		}

		const FVector3d SamplePosition = FVector3d::UnitX();
		const TArray<FCarrierLabPhaseIIIE3CandidateProbe> CandidateProbes =
			Spec.bUseFilteredGapRoute ? MakeFilteredCandidateProbes() : TArray<FCarrierLabPhaseIIIE3CandidateProbe>();
		const bool bSelectionOk = Actor->QueryPhaseIIIE3FilteredRemeshSelectionForTest(
			SamplePosition,
			CandidateProbes,
			OutResult.SelectionRecord);

		const bool bGenerationOk = bSelectionOk && Actor->QueryPhaseIIIE4DivergentOceanicFieldsForTest(
			SamplePosition,
			OutResult.SelectionRecord.SelectionClass,
			MakeDivergentBoundaryEdges(),
			0.004,
			OutResult.OceanicRecord);

		FCarrierLabPhaseIIIE5RemeshInputFixture RebuildFixture;
		RebuildFixture.bInjectGeneratedOceanicRecord = OutResult.OceanicRecord.bGeneratedOceanicCrust;
		RebuildFixture.OverrideTriangleId = 0;
		RebuildFixture.GeneratedOceanicRecord = OutResult.OceanicRecord;
		const bool bRebuildOk = bGenerationOk && Actor->RunPhaseIIIE5TopologyRebuildFixtureForTest(
			RebuildFixture,
			OutResult.TopologyAudit);

		for (const FCarrierLabPhaseIIIE5RemeshVertexRecord& Record : OutResult.TopologyAudit.VertexRecords)
		{
			if (!Record.bGeneratedOceanicCrust)
			{
				continue;
			}
			++OutResult.GeneratedRecordCount;
			if (Record.bUsedZGammaGeophysicsDerivedProfile || Record.OceanicRecord.bUsedZGammaGeophysicsDerivedProfile)
			{
				++OutResult.GeophysicsDerivedZGammaCount;
			}
			if (Record.bPaperFaithfulZGammaProfile || Record.OceanicRecord.bPaperFaithfulZGammaProfile)
			{
				++OutResult.PaperFaithfulZGammaCount;
			}
			const double PreContinental = FMath::Clamp(Record.PreRemeshContinentalFraction, 0.0, 1.0);
			if (PreContinental > ContinentalOverwriteThreshold)
			{
				++OutResult.OverwrittenByRidgeGenerationCount;
				OutResult.OverwrittenContinentalFraction += PreContinental;
			}
			else
			{
				++OutResult.NewOceanicCreationCount;
				OutResult.NewOceanicSampleEquivalent += 1.0 - PreContinental;
			}
		}

		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bRan = bSelectionOk && bGenerationOk && bRebuildOk && OutResult.TopologyAudit.bRan;
		OutResult.bSelectionDivergentRoute =
			OutResult.SelectionRecord.bDivergentGapRoute &&
			(Spec.bUseFilteredGapRoute
				? OutResult.SelectionRecord.SelectionClass == ECarrierLabPhaseIIIE3SelectionClass::DivergentGapAfterFiltering
				: OutResult.SelectionRecord.SelectionClass == ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap);
		OutResult.bGenerationAllowed =
			OutResult.OceanicRecord.bGeneratedOceanicCrust &&
			OutResult.OceanicRecord.bBoundaryPairFound &&
			!OutResult.OceanicRecord.bNonSeparatingAnomaly &&
			!OutResult.OceanicRecord.bRejectedNonDivergentRoute &&
			!OutResult.OceanicRecord.bRejectedUnresolvedMultiHit;
		OutResult.bLedgerReconciles =
			OutResult.GeneratedRecordCount ==
			OutResult.NewOceanicCreationCount + OutResult.OverwrittenByRidgeGenerationCount;
		OutResult.bNoForbiddenPolicy =
			OutResult.SelectionRecord.bUsedPolicyWinner == false &&
			OutResult.SelectionRecord.bUsedPriorOwnerFallback == false &&
			OutResult.OceanicRecord.bUsedPolicyWinner == false &&
			OutResult.OceanicRecord.bUsedPriorOwnerFallback == false &&
			OutResult.TopologyAudit.PolicyWinnerCount == 0 &&
			OutResult.TopologyAudit.PriorOwnerFallbackCount == 0 &&
			OutResult.TopologyAudit.ProjectionOwnerFallbackCount == 0 &&
			OutResult.TopologyAudit.UnresolvedMultiHitRoutedCount == 0;
		OutResult.bQAndZGammaPreserved =
			OutResult.TopologyAudit.bQProvenancePreserved &&
			OutResult.TopologyAudit.bZGammaHoldPreserved &&
			OutResult.GeophysicsDerivedZGammaCount == OutResult.GeneratedRecordCount &&
			OutResult.PaperFaithfulZGammaCount == 0;

		const bool bExpectedLedgerLine =
			Spec.bExpectOverwriteHold
				? (OutResult.OverwrittenByRidgeGenerationCount > 0 && OutResult.NewOceanicCreationCount == 0)
				: (OutResult.NewOceanicCreationCount > 0 && OutResult.OverwrittenByRidgeGenerationCount == 0);
		OutResult.bPass =
			OutResult.bRan &&
			OutResult.bSelectionDivergentRoute &&
			OutResult.bGenerationAllowed &&
			OutResult.TopologyAudit.bApplied &&
			OutResult.TopologyAudit.ResetAudit.bProcessStateEmptyAfter &&
			OutResult.TopologyAudit.ResetAudit.bResetSerialAdvanced &&
			OutResult.TopologyAudit.bMotionPreserved &&
			OutResult.bLedgerReconciles &&
			bExpectedLedgerLine &&
			OutResult.bNoForbiddenPolicy &&
			OutResult.bQAndZGammaPreserved;

		FinalizeEventHash(OutResult);
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bRan;
	}

	void FinalizeTrackingHash(FPostRebuildIIIBResult& Result)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-IIIE6-post-rebuild-IIIB-tracking-v1"));
		HashMixString(Hash, Result.TopologyAudit.TopologyHash);
		HashMix(Hash, static_cast<uint64>(Result.SeedMetrics.SeedPlateId + 2));
		HashMix(Hash, static_cast<uint64>(Result.SeedMetrics.SeedOtherPlateId + 2));
		HashMix(Hash, static_cast<uint64>(Result.SeedMetrics.SeedLocalTriangleId + 2));
		HashMix(Hash, static_cast<uint64>(Result.TrackingAudit.ActiveBoundaryTriangleCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.DistanceAudit.DistanceRecordCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.MatrixAudit.MatrixPairCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.MatrixAudit.AcceptedLocalPositiveHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PropagationAudit.PropagationSeedHitCount + 1));
		HashMix(Hash, static_cast<uint64>(Result.PropagationAudit.PropagationAddedCount + 1));
		HashMixString(Hash, Result.ClosureAudit.ComputedConvergenceTrackingHash);
		HashMix(Hash, Result.ClosureAudit.bMetricsHashMatchesComputed ? 1ull : 0ull);
		Result.TrackingHash = HashToString(Hash);
	}

	bool EvaluatePostRebuildIIIBGate(FPostRebuildIIIBResult& OutResult, UWorld& World)
	{
		OutResult = FPostRebuildIIIBResult();
		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(World, 10000, 2, 0.0);
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
		Actor->SetPlateContinentalForTest(0, false);
		Actor->SetPlateContinentalForTest(1, true);

		FCarrierLabPhaseIIIE5RemeshInputFixture RebuildFixture;
		const bool bRebuilt = Actor->RunPhaseIIIE5TopologyRebuildFixtureForTest(RebuildFixture, OutResult.TopologyAudit);
		const bool bSeeded = bRebuilt && Actor->SeedPhaseIIIB6SingleConvergentTriangleForTest(0, OutResult.SeedMetrics);
		if (bSeeded)
		{
			Actor->RefreshPhaseIIIMetricsForTest();
		}
		const bool bAudited =
			bSeeded &&
			Actor->GetPhaseIIIB1TrackingAudit(OutResult.TrackingAudit) &&
			Actor->GetPhaseIIIB2DistanceAudit(OutResult.DistanceAudit) &&
			Actor->GetPhaseIIIB3SubductionMatrixAudit(OutResult.MatrixAudit) &&
			Actor->GetPhaseIIIB6NeighborPropagationAudit(OutResult.PropagationAudit) &&
			Actor->GetPhaseIIIB7HashClosureAudit(OutResult.ClosureAudit);

		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bRan = bRebuilt && bSeeded && bAudited;
		OutResult.bPass =
			OutResult.bRan &&
			OutResult.TopologyAudit.bApplied &&
			OutResult.TopologyAudit.ResetAudit.bProcessStateEmptyAfter &&
			OutResult.TopologyAudit.ResetAudit.bResetSerialAdvanced &&
			OutResult.SeedMetrics.SeedLocalTriangleId != INDEX_NONE &&
			OutResult.TrackingAudit.ActiveBoundaryTriangleCount > 0 &&
			OutResult.DistanceAudit.DistanceRecordCount > 0 &&
			OutResult.MatrixAudit.MatrixPairCount > 0 &&
			OutResult.MatrixAudit.AcceptedLocalPositiveHitCount > 0 &&
			OutResult.PropagationAudit.PropagationSeedHitCount > 0 &&
			OutResult.PropagationAudit.PropagationAddedCount > 0 &&
			OutResult.ClosureAudit.ResetSerial == OutResult.TopologyAudit.ResetAudit.ResetSerialAfter &&
			OutResult.ClosureAudit.bMetricsHashMatchesComputed;
		FinalizeTrackingHash(OutResult);
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bRan;
	}

	FString BuildEventJsonLine(const FRemeshEventLedgerResult& Result)
	{
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"pass\":%s,\"hold\":%s,\"selection\":%s,\"generated\":%s,\"new_oceanic\":%d,\"overwritten_by_ridge\":%d,\"new_oceanic_sample_equivalent\":%.12f,\"overwritten_continental_fraction\":%.12f,\"ledger_reconciles\":%s,\"policy_prior_projection\":\"%d/%d/%d\",\"q_preserved\":%s,\"zgamma_preserved\":%s,\"geophysics_zgamma\":%d,\"paper_zgamma\":%d,\"topology_hash\":%s,\"event_hash\":%s,\"seconds\":%.6f}"),
			*JsonString(Result.Name),
			Result.bPass ? TEXT("true") : TEXT("false"),
			Result.bHold ? TEXT("true") : TEXT("false"),
			*JsonString(SelectionClassName(Result.SelectionRecord.SelectionClass)),
			Result.OceanicRecord.bGeneratedOceanicCrust ? TEXT("true") : TEXT("false"),
			Result.NewOceanicCreationCount,
			Result.OverwrittenByRidgeGenerationCount,
			Result.NewOceanicSampleEquivalent,
			Result.OverwrittenContinentalFraction,
			Result.bLedgerReconciles ? TEXT("true") : TEXT("false"),
			Result.TopologyAudit.PolicyWinnerCount,
			Result.TopologyAudit.PriorOwnerFallbackCount,
			Result.TopologyAudit.ProjectionOwnerFallbackCount,
			Result.TopologyAudit.bQProvenancePreserved ? TEXT("true") : TEXT("false"),
			Result.TopologyAudit.bZGammaHoldPreserved ? TEXT("true") : TEXT("false"),
			Result.GeophysicsDerivedZGammaCount,
			Result.PaperFaithfulZGammaCount,
			*JsonString(Result.TopologyAudit.TopologyHash),
			*JsonString(Result.EventHash),
			Result.Seconds);
	}

	FString BuildPostRebuildIIIBJsonLine(const FPostRebuildIIIBResult& Result)
	{
		return FString::Printf(
			TEXT("{\"fixture\":\"post_rebuild_iiib_tracking\",\"pass\":%s,\"seed_plate\":%d,\"seed_other_plate\":%d,\"seed_local_triangle\":%d,\"active_triangles\":%d,\"distance_records\":%d,\"matrix_pairs\":%d,\"accepted_positive_hits\":%d,\"propagation_seed_hits\":%d,\"propagation_added\":%d,\"reset_serial_after\":%d,\"closure_reset_serial\":%d,\"tracking_hash\":%s,\"seconds\":%.6f}"),
			Result.bPass ? TEXT("true") : TEXT("false"),
			Result.SeedMetrics.SeedPlateId,
			Result.SeedMetrics.SeedOtherPlateId,
			Result.SeedMetrics.SeedLocalTriangleId,
			Result.TrackingAudit.ActiveBoundaryTriangleCount,
			Result.DistanceAudit.DistanceRecordCount,
			Result.MatrixAudit.MatrixPairCount,
			Result.MatrixAudit.AcceptedLocalPositiveHitCount,
			Result.PropagationAudit.PropagationSeedHitCount,
			Result.PropagationAudit.PropagationAddedCount,
			Result.TopologyAudit.ResetAudit.ResetSerialAfter,
			Result.ClosureAudit.ResetSerial,
			*JsonString(Result.TrackingHash),
			Result.Seconds);
	}

	FString BuildReport(
		const TArray<FRemeshEventLedgerResult>& EventResults,
		const FPostRebuildIIIBResult& PostRebuildIIIB,
		const bool bReplayPass,
		const FString& ReplayHashA,
		const FString& ReplayHashB,
		const FString& MetricsPath)
	{
		bool bEventsPass = true;
		for (const FRemeshEventLedgerResult& Result : EventResults)
		{
			bEventsPass = bEventsPass && Result.bPass;
		}
		const bool bAllPass = bEventsPass && PostRebuildIIIB.bPass && bReplayPass;

		FString Report;
		Report += TEXT("# Phase IIIE.6 Remesh Ledger Reframe And Cadence Wire-Up\n\n");
		Report += TEXT("Verdict: ");
		Report += bAllPass ? TEXT("PASS / IIIE CONSOLIDATION UNBLOCKED; NAMED LAB-POLICY HOLDS CARRIED") : TEXT("FAIL / HOLD IIIE CONSOLIDATION");
		Report += TEXT(". This slice wires the IIIE.3 divergent route, IIIE.4 oceanic generation, and IIIE.5 topology rebuild/reset helpers into a focused remesh-event audit, adds ledger lines for new oceanic creation and ridge overwrite, and closes the post-rebuild IIIB tracking discontinuity. It does not add optimization, resolve triple-junction policy, claim zGamma's profile law is paper-sourced, or retire legacy comparison code.\n\n");

		Report += TEXT("## Scope\n\n");
		Report += TEXT("- The event audit consumes only IIIE.3 routes that are legal for IIIE.4: no-hit divergent gaps and filter-exhausted divergent gaps. Unresolved multi-hit selection classes remain stop conditions and are not routed to ocean generation.\n");
		Report += TEXT("- Generated oceanic vertices are fed into the IIIE.5 duplicate/re-index/re-compact helper, so selection, gap fill, topology rebuild, and process reset are exercised in one bounded cadence gate.\n");
		Report += TEXT("- The material ledger is reframed into two explicit generated-ocean lines: `new oceanic creation` when the pre-remesh continental fraction is effectively zero, and `overwritten by ridge generation` when generated oceanic crust would replace pre-existing continental material.\n");
		Report += TEXT("- `overwritten by ridge generation` is a hold/anomaly line, not a hidden correction. IIIE consolidation must decide whether this remains an approved lab policy, becomes a stop condition in production cadence, or receives paper-cited handling.\n");
		Report += TEXT("- The post-rebuild IIIB gate seeds convergence tracking after IIIE.5 topology rebuild/reset, then checks IIIB active lists, distance records, subduction matrix evidence, neighbor propagation, and hash closure on the rebuilt local topology.\n\n");

		Report += TEXT("## Gates\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---:|---|\n");
		for (const FRemeshEventLedgerResult& Result : EventResults)
		{
			Report += FString::Printf(
				TEXT("| %s | %s | selection `%s`, raw/post-filter `%d/%d`, generated `%d`, q1/q2 `%d/%d`, new oceanic `%d` (`%.3f` sample-eq), overwritten `%d` (`%.3f` continental fraction), ledger reconciles `%d`, reset `%d->%d`, policy/prior/projection `%d/%d/%d`, q/zGamma `%d/%d`, geophysics/paper zGamma `%d/%d`, topology `%s`, event `%s`. |\n"),
				*Result.Name,
				*HoldPassFail(Result.bPass, Result.bHold),
				*SelectionClassName(Result.SelectionRecord.SelectionClass),
				Result.SelectionRecord.RawCandidateCount,
				Result.SelectionRecord.PostFilterCandidateCount,
				Result.GeneratedRecordCount,
				Result.OceanicRecord.Q1PlateId,
				Result.OceanicRecord.Q2PlateId,
				Result.NewOceanicCreationCount,
				Result.NewOceanicSampleEquivalent,
				Result.OverwrittenByRidgeGenerationCount,
				Result.OverwrittenContinentalFraction,
				Result.bLedgerReconciles ? 1 : 0,
				Result.TopologyAudit.ResetAudit.ResetSerialBefore,
				Result.TopologyAudit.ResetAudit.ResetSerialAfter,
				Result.TopologyAudit.PolicyWinnerCount,
				Result.TopologyAudit.PriorOwnerFallbackCount,
				Result.TopologyAudit.ProjectionOwnerFallbackCount,
				Result.TopologyAudit.bQProvenancePreserved ? 1 : 0,
				Result.TopologyAudit.bZGammaHoldPreserved ? 1 : 0,
				Result.GeophysicsDerivedZGammaCount,
				Result.PaperFaithfulZGammaCount,
				*Result.TopologyAudit.TopologyHash,
				*Result.EventHash);
		}
		Report += FString::Printf(
			TEXT("| Post-rebuild IIIB tracking gate | %s | topology `%s`, reset `%d->%d`, seed plate/local `%d/%d`, other `%d`, active `%d`, distances `%d`, matrix pairs `%d`, accepted positive `%d`, propagation seed/added `%d/%d`, closure reset `%d`, hash `%s`. |\n"),
			*PassFail(PostRebuildIIIB.bPass),
			*PostRebuildIIIB.TopologyAudit.TopologyHash,
			PostRebuildIIIB.TopologyAudit.ResetAudit.ResetSerialBefore,
			PostRebuildIIIB.TopologyAudit.ResetAudit.ResetSerialAfter,
			PostRebuildIIIB.SeedMetrics.SeedPlateId,
			PostRebuildIIIB.SeedMetrics.SeedLocalTriangleId,
			PostRebuildIIIB.SeedMetrics.SeedOtherPlateId,
			PostRebuildIIIB.TrackingAudit.ActiveBoundaryTriangleCount,
			PostRebuildIIIB.DistanceAudit.DistanceRecordCount,
			PostRebuildIIIB.MatrixAudit.MatrixPairCount,
			PostRebuildIIIB.MatrixAudit.AcceptedLocalPositiveHitCount,
			PostRebuildIIIB.PropagationAudit.PropagationSeedHitCount,
			PostRebuildIIIB.PropagationAudit.PropagationAddedCount,
			PostRebuildIIIB.ClosureAudit.ResetSerial,
			*PostRebuildIIIB.TrackingHash);
		Report += FString::Printf(
			TEXT("| Same-seed remesh-event replay | %s | Event hashes `%s` and `%s`. |\n\n"),
			*PassFail(bReplayPass),
			*ReplayHashA,
			*ReplayHashB);

		Report += TEXT("## Contract Table\n\n");
		Report += TEXT("| Paper / IIIE.1 requirement | CarrierLab support now | Remaining obligation | Gate |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += TEXT("| Zero valid ray hits become divergent gap fill | No-hit IIIE.3 records route through IIIE.4 q1/q2/qGamma and then IIIE.5 rebuild/reset | Production cadence still needs scheduling around the natural remesh timestep | No-hit live remesh event ledger |\n");
		Report += TEXT("| Filter-exhausted hits become divergent gap fill only after process-state filtering | Filter-exhausted records route through the same oceanic generation path and preserve filter provenance via the IIIE.3 selection class | Collision/remesh same-step ordering remains the IIIE.1 convention: collision/suture authorization before remesh filtering | Filter-exhausted overwrite hold |\n");
		Report += TEXT("| New oceanic crust must be ledgered distinctly | IIIE.6 records `new oceanic creation` as a generated-ocean ledger line when pre-remesh continental material is absent | Convert this audit line into production material-ledger accounting when full cadence mutates live state | Ledger reconciliation columns |\n");
		Report += TEXT("| Ridge generation must not silently overwrite continental material | IIIE.6 records `overwritten by ridge generation` separately and marks it hold/anomaly evidence | Consolidation must approve, forbid, or paper-cite this behavior before broad validation claims | Overwrite hold fixture |\n");
		Report += TEXT("| Plate-local topology rebuild/reset must be the event continuation | IIIE.6 feeds generated records into the IIIE.5 duplicate/re-index/re-compact helper and observes reset in the same event gate | Keep Stage 1.5 recovery out of the primary path | Topology hash and reset columns |\n");
		Report += TEXT("| IIIB tracking must work after rebuild | A post-rebuild actor seeds IIIB tracking from rebuilt local topology and checks active lists, distances, matrix evidence, propagation, and hash closure | Consolidation should still rerun the `CarrierLabPhaseIIID7` computed-vs-expected regression separately; this local gate only closes the topology boundary discontinuity | Post-rebuild IIIB tracking gate |\n\n");

		Report += TEXT("## Forbidden Policy Checks\n\n");
		Report += TEXT("| Forbidden or held policy | IIIE.6 status |\n");
		Report += TEXT("|---|---|\n");
		Report += TEXT("| Prior global owner/fraction fallback | Selection, generation, and topology counters remain zero. |\n");
		Report += TEXT("| Projection-derived ownership authority | Topology projection-authority counter remains zero. |\n");
		Report += TEXT("| Centroid/random/synthetic remesh winner | Policy-winner counters remain zero; no multi-hit route is consumed. |\n");
		Report += TEXT("| Stage 1.5 recovery/backfill/retention/hysteresis/anchoring | Not called by the event audit; IIIE.5 rebuild remains the authority path. |\n");
		Report += TEXT("| Unresolved multi-hit ridge generation | Not routed; IIIE.4 receives only no-hit and filter-exhausted divergent classes. |\n");
		Report += TEXT("| zGamma paper-fidelity overclaim | Generated records preserve `bUsedZGammaGeophysicsDerivedProfile = true` and `bPaperFaithfulZGammaProfile = false`. |\n");
		Report += TEXT("| Silent continental overwrite by divergent ridge generation | Moved into an explicit hold/anomaly ledger line. |\n\n");

		Report += TEXT("## Open Decisions\n\n");
		Report += TEXT("| Decision | Status | Rationale |\n");
		Report += TEXT("|---|---|---|\n");
		Report += TEXT("| zGamma profile law | Deferred / named lab extension | Current sqrt-distance profile is geophysics-derived and realistic, but the paper/thesis do not provide a closed-form zGamma equation. |\n");
		Report += TEXT("| Two-of-three mixed triangle majority | Deferred / named lab policy | IIIE.5 exposes the majority rule; consolidation must disclose it as lab policy unless a paper citation is found. |\n");
		Report += TEXT("| One-one-one triple-junction topology | Deferred / hold | IIIE.5 leaves these unresolved rather than inventing authority. |\n");
		Report += TEXT("| Continental overwrite by divergent ridge generation | Deferred / hold | IIIE.6 makes the ledger line visible and non-silent; production cadence policy remains a consolidation decision. |\n\n");

		Report += TEXT("## Stop Conditions For IIIE Consolidation+\n\n");
		Report += TEXT("- Stop if unresolved multi-hit IIIE.3 classes are routed into IIIE.4 generation.\n");
		Report += TEXT("- Stop if `overwritten by ridge generation` is merged into `new oceanic creation` or hidden in aggregate material accounting.\n");
		Report += TEXT("- Stop if Stage 1.5 owner/projection/recovery behavior becomes primary IIIE remesh authority.\n");
		Report += TEXT("- Stop if post-rebuild IIIB tracking cannot seed active lists, distance records, matrix evidence, propagation, and closure from rebuilt plate-local topology.\n");
		Report += TEXT("- Stop if reports claim paper-faithful zGamma while generated records still report `bUsedZGammaGeophysicsDerivedProfile = true` and `bPaperFaithfulZGammaProfile = false`.\n");
		Report += TEXT("- Stop if majority or triple-junction topology policy is used without explicit consolidation disclosure.\n\n");

		Report += TEXT("## Next Slice Boundary\n\n");
		Report += TEXT("Next is IIIE consolidation: disclose the three named lab choices (geophysics-derived zGamma, two-of-three majority assignment, triple-junction hold) plus the new ridge-overwrite hold, rerun the relevant IIIE gates, keep the inherited IIIB/IIID signature trail visible, and measure the integrated paper Table 2 cost ratio. Do not start IIIF rifting, IIIG per-step elevation evolution, or IIIH long-horizon validation until consolidation explicitly clears.\n\n");
		Report += FString::Printf(TEXT("Metrics: `%s`.\n"), *MetricsPath);
		return Report;
	}
}

UCarrierLabPhaseIIIE6Commandlet::UCarrierLabPhaseIIIE6Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIE6Commandlet::Main(const FString& Params)
{
	UWorld* World = GetCommandletWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPhaseIIIE6Commandlet could not find a commandlet world."));
		return 1;
	}

	TArray<FRemeshEventFixtureSpec> Specs;
	Specs.Add(MakeNoHitOceanicFixture());
	Specs.Add(MakeFilterExhaustedOverwriteFixture());

	TArray<FRemeshEventLedgerResult> EventResults;
	EventResults.Reserve(Specs.Num());
	for (const FRemeshEventFixtureSpec& Spec : Specs)
	{
		FRemeshEventLedgerResult Result;
		EvaluateRemeshEventFixture(Spec, Result, *World);
		EventResults.Add(Result);
	}

	FPostRebuildIIIBResult PostRebuildIIIB;
	EvaluatePostRebuildIIIBGate(PostRebuildIIIB, *World);

	FRemeshEventLedgerResult ReplayA;
	FRemeshEventLedgerResult ReplayB;
	EvaluateRemeshEventFixture(MakeNoHitOceanicFixture(), ReplayA, *World);
	EvaluateRemeshEventFixture(MakeNoHitOceanicFixture(), ReplayB, *World);
	const bool bReplayPass =
		ReplayA.bPass &&
		ReplayB.bPass &&
		ReplayA.EventHash == ReplayB.EventHash;

	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("phase-iii-slice-iiie6-metrics.jsonl"));
	FString JsonLines;
	for (const FRemeshEventLedgerResult& Result : EventResults)
	{
		JsonLines += BuildEventJsonLine(Result) + LINE_TERMINATOR;
	}
	JsonLines += BuildPostRebuildIIIBJsonLine(PostRebuildIIIB) + LINE_TERMINATOR;
	JsonLines += FString::Printf(
		TEXT("{\"fixture\":\"same_seed_remesh_event_replay\",\"pass\":%s,\"hash_a\":%s,\"hash_b\":%s}"),
		bReplayPass ? TEXT("true") : TEXT("false"),
		*JsonString(ReplayA.EventHash),
		*JsonString(ReplayB.EventHash));
	JsonLines += LINE_TERMINATOR;
	FFileHelper::SaveStringToFile(JsonLines, *MetricsPath);

	const FString Report = BuildReport(
		EventResults,
		PostRebuildIIIB,
		bReplayPass,
		ReplayA.EventHash,
		ReplayB.EventHash,
		MetricsPath);
	const FString ReportPath = GetReportPath(Params);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	bool bEventsPass = true;
	for (const FRemeshEventLedgerResult& Result : EventResults)
	{
		bEventsPass = bEventsPass && Result.bPass;
	}
	const bool bAllPass = bEventsPass && PostRebuildIIIB.bPass && bReplayPass;
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIE.6 report: %s"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIE.6 metrics: %s"), *MetricsPath);
	return bAllPass ? 0 : 1;
}
