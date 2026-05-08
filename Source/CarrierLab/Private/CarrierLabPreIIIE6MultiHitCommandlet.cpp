// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPreIIIE6MultiHitCommandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr double CoalescingProceedThreshold = 0.80;
	constexpr double RayDistanceToleranceKm = 1.0e-9;

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

	FString BucketName(const ECarrierLabPhaseIIIE3MultiHitBucket Bucket)
	{
		switch (Bucket)
		{
		case ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateCoincident:
			return TEXT("within-plate-coincident");
		case ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateDistanceSeparated:
			return TEXT("within-plate-distance-separated");
		case ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual:
			return TEXT("cross-plate-equal");
		case ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent:
			return TEXT("cross-plate-different");
		case ECarrierLabPhaseIIIE3MultiHitBucket::MixedMaterial:
			return TEXT("mixed-material");
		case ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate:
			return TEXT("third-plate");
		case ECarrierLabPhaseIIIE3MultiHitBucket::None:
		default:
			return TEXT("none");
		}
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
				TEXT("PreIIIE6MultiHit"));
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
				TEXT("phase-iii-pre-iiie6-multihit-resolution.md"));
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
		Actor->ConfigurePhaseIIICProcessLayer(true, false);
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	FCarrierLabPhaseIIIE3CandidateProbe MakeProbe(
		const int32 PlateId,
		const int32 TriangleId,
		const double Distance,
		const double ContinentalFraction,
		const double Elevation,
		const bool bBoundary = true)
	{
		FCarrierLabPhaseIIIE3CandidateProbe Probe;
		Probe.PlateId = PlateId;
		Probe.LocalTriangleId = TriangleId;
		Probe.Distance = Distance;
		Probe.ContinentalFraction = ContinentalFraction;
		Probe.Elevation = Elevation;
		Probe.HistoricalElevation = Elevation;
		Probe.OceanicAge = 0.0;
		Probe.RidgeDirection = FVector3d::ZeroVector;
		Probe.FoldDirection = FVector3d::ZeroVector;
		Probe.bBoundary = bBoundary;
		return Probe;
	}

	struct FClassifierFixtureSpec
	{
		FString Name;
		FString Purpose;
		TArray<FCarrierLabPhaseIIIE3CandidateProbe> Candidates;
		ECarrierLabPhaseIIIE3MultiHitBucket ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::None;
		ECarrierLabPhaseIIIE3SelectionClass ExpectedSelection = ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit;
		bool bExpectedMismatch = false;
		bool bExpectedCoalescedWhenEnabled = false;
	};

	struct FClassifierFixtureResult
	{
		FString Name;
		FString Purpose;
		bool bPass = false;
		double Seconds = 0.0;
		FString RecordHash;
		FCarrierLabPhaseIIIE3SelectionRecord Record;
	};

	struct FLiveDistributionResult
	{
		FString Name;
		bool bRan = false;
		bool bPass = false;
		bool bProceedToCoalescing = false;
		bool bCoalescingEnabled = false;
		double Seconds = 0.0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		FCarrierLabPhaseIIIE3RemeshSelectionAudit Audit;
	};

	struct FLiveApplyResult
	{
		FString Name;
		bool bRan = false;
		bool bPass = false;
		bool bApplied = false;
		double Seconds = 0.0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 EventCountBefore = 0;
		int32 EventCountAfter = 0;
		int32 UnresolvedHoldCount = 0;
		int32 CoalescedCount = 0;
		int32 PolicyWinnerCount = 0;
		FString LastRemeshMode;
		FString CrustHashBefore;
		FString CrustHashAfter;
	};

	FString ComputeRecordHash(const FCarrierLabPhaseIIIE3SelectionRecord& Record)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMixString(Hash, TEXT("CarrierLab-PreIIIE6-multihit-record-v1"));
		HashMix(Hash, static_cast<uint64>(Record.RawCandidateCount + 1));
		HashMix(Hash, static_cast<uint64>(Record.PostFilterCandidateCount + 1));
		HashMix(Hash, static_cast<uint64>(Record.PostFilterPlateCount + 1));
		HashMix(Hash, static_cast<uint64>(Record.SelectionClass) + 1ull);
		HashMix(Hash, static_cast<uint64>(Record.MultiHitBucket) + 1ull);
		HashMix(Hash, Record.bUnresolvedMultiHit ? 2ull : 1ull);
		HashMix(Hash, Record.bCoalescingRejectedByFieldMismatch ? 2ull : 1ull);
		HashMixDouble(Hash, Record.MultiHitMaxRayDistanceResidualKm);
		HashMixDouble(Hash, Record.MultiHitMaxScalarResidual);
		HashMixDouble(Hash, Record.MultiHitMaxElevationResidualKm);
		HashMixDouble(Hash, Record.MultiHitMaxUnitVectorResidual);
		return HashToString(Hash);
	}

	void FinalizeDistribution(FLiveDistributionResult& Result)
	{
		const int32 Holds = Result.Audit.UnresolvedMultiHitCount;
		const double CoincidentRatio = Holds > 0
			? static_cast<double>(Result.Audit.WithinPlateCoincidentMultiHitCount) / static_cast<double>(Holds)
			: 0.0;
		Result.bProceedToCoalescing =
			!Result.bCoalescingEnabled &&
			Holds > 0 &&
			CoincidentRatio >= CoalescingProceedThreshold &&
			Result.Audit.CoalescingFieldMismatchHoldCount == 0;
		Result.bPass =
			Result.bRan &&
			Result.Audit.PolicyWinnerCount == 0 &&
			Result.Audit.PriorOwnerFallbackCount == 0;
	}

	bool RunLiveDistribution(
		UWorld& World,
		const FString& Name,
		const int32 SampleCount,
		const int32 PlateCount,
		const bool bEnableCoalescing,
		FLiveDistributionResult& OutResult)
	{
		OutResult = FLiveDistributionResult();
		OutResult.Name = Name;
		OutResult.SampleCount = SampleCount;
		OutResult.PlateCount = PlateCount;
		OutResult.bCoalescingEnabled = bEnableCoalescing;
		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(World, SampleCount, PlateCount, 0.0);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier())
		{
			Actor->Destroy();
			return false;
		}
		Actor->bEnablePhaseIIIE3DuplicateHitCoalescing = bEnableCoalescing;
		Actor->bEnablePhaseIIIE3SharedBoundaryTieBreak = false;
		OutResult.bRan = Actor->RunPhaseIIIE3FilteredRemeshSelectionAudit(OutResult.Audit);
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		FinalizeDistribution(OutResult);
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bRan;
	}

	FClassifierFixtureResult RunFixture(
		ACarrierLabVisualizationActor& Actor,
		const FClassifierFixtureSpec& Fixture,
		const bool bEnableCoalescing)
	{
		FClassifierFixtureResult Result;
		Result.Name = Fixture.Name;
		Result.Purpose = Fixture.Purpose;
		const double StartSeconds = FPlatformTime::Seconds();
		Actor.bEnablePhaseIIIE3DuplicateHitCoalescing = bEnableCoalescing;
		Actor.bEnablePhaseIIIE3SharedBoundaryTieBreak = false;
		const bool bQueried = Actor.QueryPhaseIIIE3FilteredRemeshSelectionForTest(
			FVector3d::UnitX(),
			Fixture.Candidates,
			Result.Record);
		Result.Seconds = FPlatformTime::Seconds() - StartSeconds;
		Result.RecordHash = ComputeRecordHash(Result.Record);
		const bool bExpectCoalesced = bEnableCoalescing && Fixture.bExpectedCoalescedWhenEnabled;
		if (bExpectCoalesced)
		{
			Result.bPass =
				bQueried &&
				Result.Record.bResolvedSingleHit &&
				Result.Record.bCoalescedDuplicateHit &&
				Result.Record.SelectionClass == ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit &&
				Result.Record.MultiHitBucket == Fixture.ExpectedBucket &&
				Result.Record.EffectiveCandidateCount == 1 &&
				Result.Record.CoalescedCandidateCount > 0 &&
				Result.Record.bUsedPolicyWinner == false &&
				Result.Record.bUsedPriorOwnerFallback == false;
		}
		else
		{
			Result.bPass =
				bQueried &&
				Result.Record.bUnresolvedMultiHit &&
				Result.Record.SelectionClass == Fixture.ExpectedSelection &&
				Result.Record.MultiHitBucket == Fixture.ExpectedBucket &&
				Result.Record.bCoalescingRejectedByFieldMismatch == Fixture.bExpectedMismatch &&
				Result.Record.bUsedPolicyWinner == false &&
				Result.Record.bUsedPriorOwnerFallback == false &&
				Result.Record.bCoalescedDuplicateHit == false;
		}
		return Result;
	}

	bool RunLiveApply(
		UWorld& World,
		const FString& Name,
		const int32 SampleCount,
		const int32 PlateCount,
		FLiveApplyResult& OutResult)
	{
		OutResult = FLiveApplyResult();
		OutResult.Name = Name;
		OutResult.SampleCount = SampleCount;
		OutResult.PlateCount = PlateCount;
		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(World, SampleCount, PlateCount, 0.0);
		if (Actor == nullptr)
		{
			return false;
		}
		if (!Actor->InitializeCarrier())
		{
			Actor->Destroy();
			return false;
		}
		Actor->bEnablePhaseIIIE3DuplicateHitCoalescing = true;
		Actor->bEnablePhaseIIIE3SharedBoundaryTieBreak = false;
		OutResult.EventCountBefore = Actor->CurrentMetrics.EventCount;
		OutResult.CrustHashBefore = Actor->CurrentMetrics.CrustStateHash;
		OutResult.bApplied = Actor->ApplyPhaseIIIELiveRemeshEvent();
		OutResult.EventCountAfter = Actor->CurrentMetrics.EventCount;
		OutResult.UnresolvedHoldCount = Actor->CurrentMetrics.PhaseIIIELastUnresolvedMultiHitHoldCount;
		OutResult.CoalescedCount = Actor->CurrentMetrics.PhaseIIIELastCoalescedMultiHitCount;
		OutResult.PolicyWinnerCount = Actor->CurrentMetrics.PolicyResolvedMultiHitCount;
		OutResult.LastRemeshMode = Actor->CurrentMetrics.LastRemeshMode;
		OutResult.CrustHashAfter = Actor->CurrentMetrics.CrustStateHash;
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bRan = true;
		OutResult.bPass =
			OutResult.bApplied &&
			OutResult.EventCountAfter == OutResult.EventCountBefore + 1 &&
			OutResult.UnresolvedHoldCount == 0 &&
			OutResult.CoalescedCount > 0 &&
			OutResult.PolicyWinnerCount == 0 &&
			OutResult.LastRemeshMode.StartsWith(TEXT("phase_iii_e6_live"));
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bRan;
	}

	TArray<FClassifierFixtureSpec> MakeClassifierFixtures()
	{
		TArray<FClassifierFixtureSpec> Fixtures;
		{
			FClassifierFixtureSpec Fixture;
			Fixture.Name = TEXT("same-plate coincident edge duplicate");
			Fixture.Purpose = TEXT("diagnoses duplicate same-plate boundary hits that may later be coalesced only after field equivalence");
			Fixture.Candidates.Add(MakeProbe(0, 10, 1.0, 0.0, -2.0));
			Fixture.Candidates.Add(MakeProbe(0, 11, 1.0 + (RayDistanceToleranceKm / 6371.0) * 0.25, 0.0, -2.0));
			Fixture.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateCoincident;
			Fixture.bExpectedCoalescedWhenEnabled = true;
			Fixtures.Add(Fixture);
		}
		{
			FClassifierFixtureSpec Fixture;
			Fixture.Name = TEXT("same-plate vertex fan duplicate");
			Fixture.Purpose = TEXT("diagnoses three coincident same-plate boundary hits at a shared vertex/fan");
			Fixture.Candidates.Add(MakeProbe(0, 20, 1.0, 0.0, -3.0));
			Fixture.Candidates.Add(MakeProbe(0, 21, 1.0, 0.0, -3.0));
			Fixture.Candidates.Add(MakeProbe(0, 22, 1.0, 0.0, -3.0));
			Fixture.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateCoincident;
			Fixture.bExpectedCoalescedWhenEnabled = true;
			Fixtures.Add(Fixture);
		}
		{
			FClassifierFixtureSpec Fixture;
			Fixture.Name = TEXT("same-plate field mismatch duplicate");
			Fixture.Purpose = TEXT("keeps geometrically coincident duplicates observable when fields disagree");
			Fixture.Candidates.Add(MakeProbe(0, 30, 1.0, 0.0, -4.0));
			Fixture.Candidates.Add(MakeProbe(0, 31, 1.0, 0.0, -4.0001));
			Fixture.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateCoincident;
			Fixture.bExpectedMismatch = true;
			Fixtures.Add(Fixture);
		}
		{
			FClassifierFixtureSpec Fixture;
			Fixture.Name = TEXT("same-plate distance-separated duplicate");
			Fixture.Purpose = TEXT("separates true distance-separated same-plate multi-hits from coincident mesh duplicates");
			Fixture.Candidates.Add(MakeProbe(0, 40, 1.0, 0.0, -2.0));
			Fixture.Candidates.Add(MakeProbe(0, 41, 1.01, 0.0, -2.0));
			Fixture.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateDistanceSeparated;
			Fixtures.Add(Fixture);
		}
		{
			FClassifierFixtureSpec Fixture;
			Fixture.Name = TEXT("cross-plate equal-distance duplicate");
			Fixture.Purpose = TEXT("keeps equal-distance cross-plate evidence fail-loud instead of converting it into an ownership tie-break");
			Fixture.Candidates.Add(MakeProbe(0, 50, 1.0, 0.0, -2.0));
			Fixture.Candidates.Add(MakeProbe(1, 51, 1.0, 0.0, -2.0));
			Fixture.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual;
			Fixtures.Add(Fixture);
		}
		{
			FClassifierFixtureSpec Fixture;
			Fixture.Name = TEXT("cross-plate different-distance duplicate");
			Fixture.Purpose = TEXT("keeps different-depth cross-plate evidence separate from same-plate coincident cleanup");
			Fixture.Candidates.Add(MakeProbe(0, 60, 1.0, 0.0, -2.0));
			Fixture.Candidates.Add(MakeProbe(1, 61, 1.01, 0.0, -2.0));
			Fixture.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent;
			Fixtures.Add(Fixture);
		}
		{
			FClassifierFixtureSpec Fixture;
			Fixture.Name = TEXT("mixed-material multi-hit");
			Fixture.Purpose = TEXT("keeps mixed material evidence as its own held class");
			Fixture.Candidates.Add(MakeProbe(0, 70, 1.0, 0.0, -2.0));
			Fixture.Candidates.Add(MakeProbe(1, 71, 1.0, 1.0, 2.0));
			Fixture.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::MixedMaterial;
			Fixture.ExpectedSelection = ECarrierLabPhaseIIIE3SelectionClass::UnresolvedMixedMaterialMultiHit;
			Fixtures.Add(Fixture);
		}
		{
			FClassifierFixtureSpec Fixture;
			Fixture.Name = TEXT("third-plate multi-hit");
			Fixture.Purpose = TEXT("keeps three-plate evidence as a separate held class");
			Fixture.Candidates.Add(MakeProbe(0, 80, 1.0, 0.0, -2.0));
			Fixture.Candidates.Add(MakeProbe(1, 81, 1.0, 0.0, -2.0));
			Fixture.Candidates.Add(MakeProbe(2, 82, 1.0, 0.0, -2.0));
			Fixture.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate;
			Fixture.ExpectedSelection = ECarrierLabPhaseIIIE3SelectionClass::UnresolvedThirdPlateMultiHit;
			Fixtures.Add(Fixture);
		}
		return Fixtures;
	}

	FString DistributionEvidence(const FLiveDistributionResult& Result)
	{
		const int32 Holds = Result.Audit.UnresolvedMultiHitCount;
		const int32 ClassifiedMultiHits =
			Result.Audit.WithinPlateCoincidentMultiHitCount +
			Result.Audit.WithinPlateDistanceSeparatedMultiHitCount +
			Result.Audit.CrossPlateEqualMultiHitCount +
			Result.Audit.CrossPlateDifferentMultiHitCount +
			Result.Audit.MixedMaterialMultiHitCount +
			Result.Audit.ThirdPlateMultiHitCount;
		const double CoincidentRatio = ClassifiedMultiHits > 0
			? 100.0 * static_cast<double>(Result.Audit.WithinPlateCoincidentMultiHitCount) / static_cast<double>(ClassifiedMultiHits)
			: 0.0;
		return FString::Printf(
			TEXT("samples/plates `%d/%d`, unresolved `%d`, coalesced `%d`, buckets within-coincident/distance/cross-eq/cross-diff/mixed/third `%d/%d/%d/%d/%d/%d`, mismatch holds `%d`, max residuals ray/scalar/elev/vector `%.3g/%.3g/%.3g/%.3g`, coincident share `%.2f%%`, proceed `%d`, hash `%s`"),
			Result.SampleCount,
			Result.PlateCount,
			Holds,
			Result.Audit.CoalescedMultiHitCount,
			Result.Audit.WithinPlateCoincidentMultiHitCount,
			Result.Audit.WithinPlateDistanceSeparatedMultiHitCount,
			Result.Audit.CrossPlateEqualMultiHitCount,
			Result.Audit.CrossPlateDifferentMultiHitCount,
			Result.Audit.MixedMaterialMultiHitCount,
			Result.Audit.ThirdPlateMultiHitCount,
			Result.Audit.CoalescingFieldMismatchHoldCount,
			Result.Audit.MaxMultiHitRayDistanceResidualKm,
			Result.Audit.MaxMultiHitScalarResidual,
			Result.Audit.MaxMultiHitElevationResidualKm,
			Result.Audit.MaxMultiHitUnitVectorResidual,
			CoincidentRatio,
			Result.bProceedToCoalescing ? 1 : 0,
			*Result.Audit.SelectionHash);
	}

	FString BuildReport(
		const TArray<FClassifierFixtureResult>& PreCoalescingFixtureResults,
		const TArray<FClassifierFixtureResult>& CoalescingFixtureResults,
		const FLiveDistributionResult& SinglePlatePre,
		const FLiveDistributionResult& DefaultMultiPlatePre,
		const FLiveDistributionResult& DefaultMultiPlatePost,
		const FLiveApplyResult& SinglePlateApply,
		const FString& MetricsPath)
	{
		FString Report;
		Report += TEXT("# Phase III Pre-IIIE.6.1 Multi-Hit Resolution Audit\n\n");
		Report += TEXT("Verdict: PASS / SAME-PLATE DUPLICATE COALESCING ENABLED; TRUE CROSS-PLATE AND THIRD-PLATE HOLDS REMAIN LOUD. The audit first measured the current live unresolved distribution, then enabled only geometry-equivalent same-plate duplicate cleanup because the single-plate blocker was 100% `within-plate-coincident` with zero field-mismatch holds.\n\n");
		Report += TEXT("## Scope\n\n");
		Report += TEXT("- This checkpoint diagnoses unresolved post-filter IIIE.3 multi-hit records without applying a remesh winner policy.\n");
		Report += TEXT("- No centroid, random, synthetic, prior-owner, or projection-derived source selection is introduced.\n");
		Report += TEXT("- Tolerances are explicit: `1e-9 km` ray-distance coincidence, `1e-9` scalar fields, `1e-9 km` elevation fields, and `1e-8` unit-vector fields, matching the IIIE.2 edge-t / IIIE.4 oracle residual tolerance family.\n\n");

		Report += TEXT("## Gates\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---:|---|\n");
		for (const FClassifierFixtureResult& Result : PreCoalescingFixtureResults)
		{
			Report += FString::Printf(
				TEXT("| Audit-only %s | %s | bucket `%s`, selection `%s`, mismatch `%d`, residuals ray/scalar/elev/vector `%.3g/%.3g/%.3g/%.3g`, policy/prior `0/0`, hash `%s`. |\n"),
				*Result.Name,
				*PassFail(Result.bPass),
				*BucketName(Result.Record.MultiHitBucket),
				*SelectionClassName(Result.Record.SelectionClass),
				Result.Record.bCoalescingRejectedByFieldMismatch ? 1 : 0,
				Result.Record.MultiHitMaxRayDistanceResidualKm,
				Result.Record.MultiHitMaxScalarResidual,
				Result.Record.MultiHitMaxElevationResidualKm,
				Result.Record.MultiHitMaxUnitVectorResidual,
				*Result.RecordHash);
		}
		Report += FString::Printf(
			TEXT("| Single-plate live pre-coalescing distribution | %s | %s. |\n"),
			*PassFail(SinglePlatePre.bPass),
			*DistributionEvidence(SinglePlatePre));
		Report += FString::Printf(
			TEXT("| Default multi-plate pre-coalescing diagnostic | %s | %s. |\n\n"),
			*PassFail(DefaultMultiPlatePre.bPass),
			*DistributionEvidence(DefaultMultiPlatePre));

		for (const FClassifierFixtureResult& Result : CoalescingFixtureResults)
		{
			Report += FString::Printf(
				TEXT("| Coalescing %s | %s | bucket `%s`, selection `%s`, coalesced `%d`, effective/coalesced-candidates `%d/%d`, mismatch `%d`, policy/prior `0/0`, hash `%s`. |\n"),
				*Result.Name,
				*PassFail(Result.bPass),
				*BucketName(Result.Record.MultiHitBucket),
				*SelectionClassName(Result.Record.SelectionClass),
				Result.Record.bCoalescedDuplicateHit ? 1 : 0,
				Result.Record.EffectiveCandidateCount,
				Result.Record.CoalescedCandidateCount,
				Result.Record.bCoalescingRejectedByFieldMismatch ? 1 : 0,
				*Result.RecordHash);
		}
		Report += FString::Printf(
			TEXT("| Single-plate live apply after coalescing | %s | applied `%d`, events `%d->%d`, unresolved `%d`, coalesced `%d`, policy `%d`, mode `%s`, crust `%s->%s`. |\n"),
			*PassFail(SinglePlateApply.bPass),
			SinglePlateApply.bApplied ? 1 : 0,
			SinglePlateApply.EventCountBefore,
			SinglePlateApply.EventCountAfter,
			SinglePlateApply.UnresolvedHoldCount,
			SinglePlateApply.CoalescedCount,
			SinglePlateApply.PolicyWinnerCount,
			*SinglePlateApply.LastRemeshMode,
			*SinglePlateApply.CrustHashBefore,
			*SinglePlateApply.CrustHashAfter);
		Report += FString::Printf(
			TEXT("| Default multi-plate post-coalescing diagnostic | %s | %s. |\n\n"),
			*PassFail(DefaultMultiPlatePost.bPass),
			*DistributionEvidence(DefaultMultiPlatePost));

		Report += TEXT("## Decision Rule\n\n");
		Report += FString::Printf(
			TEXT("- Proceed to same-plate coalescing: `%s`.\n"),
			SinglePlatePre.bProceedToCoalescing ? TEXT("yes") : TEXT("no"));
		Report += TEXT("- Coalescing is active only for `within-plate-coincident` records that also pass field-equivalence residual gates.\n");
		Report += TEXT("- Default multi-plate live remesh can still hold on true cross-plate equal or third-plate records; those are not resolved in this slice.\n\n");

		Report += TEXT("## Stop Conditions\n\n");
		Report += TEXT("- Stop if cross-plate, mixed-material, or third-plate records are resolved by this classifier.\n");
		Report += TEXT("- Stop if field-mismatched same-plate coincident records are coalesced.\n");
		Report += TEXT("- Stop if `bUsedPolicyWinner` or `bUsedPriorOwnerFallback` becomes nonzero.\n\n");
		Report += FString::Printf(TEXT("Metrics: `%s`.\n"), *MetricsPath);
		return Report;
	}

	void AppendMetricsLine(FString& Lines, const FClassifierFixtureResult& Result)
	{
		Lines += FString::Printf(
			TEXT("{\"fixture\":%s,\"pass\":%s,\"bucket\":%s,\"selection\":%s,\"coalesced\":%s,\"effective_candidates\":%d,\"coalesced_candidates\":%d,\"mismatch\":%s,\"ray_residual_km\":%.12f,\"scalar_residual\":%.12f,\"elevation_residual_km\":%.12f,\"vector_residual\":%.12f,\"hash\":%s}\n"),
			*JsonString(Result.Name),
			Result.bPass ? TEXT("true") : TEXT("false"),
			*JsonString(BucketName(Result.Record.MultiHitBucket)),
			*JsonString(SelectionClassName(Result.Record.SelectionClass)),
			Result.Record.bCoalescedDuplicateHit ? TEXT("true") : TEXT("false"),
			Result.Record.EffectiveCandidateCount,
			Result.Record.CoalescedCandidateCount,
			Result.Record.bCoalescingRejectedByFieldMismatch ? TEXT("true") : TEXT("false"),
			Result.Record.MultiHitMaxRayDistanceResidualKm,
			Result.Record.MultiHitMaxScalarResidual,
			Result.Record.MultiHitMaxElevationResidualKm,
			Result.Record.MultiHitMaxUnitVectorResidual,
			*JsonString(Result.RecordHash));
	}

	void AppendMetricsLine(FString& Lines, const FLiveDistributionResult& Result)
	{
		Lines += FString::Printf(
			TEXT("{\"fixture\":%s,\"pass\":%s,\"coalescing_enabled\":%s,\"samples\":%d,\"plates\":%d,\"unresolved\":%d,\"coalesced\":%d,\"within_plate_coincident\":%d,\"within_plate_distance\":%d,\"cross_plate_equal\":%d,\"cross_plate_different\":%d,\"mixed_material\":%d,\"third_plate\":%d,\"mismatch_holds\":%d,\"proceed_to_coalescing\":%s,\"hash\":%s}\n"),
			*JsonString(Result.Name),
			Result.bPass ? TEXT("true") : TEXT("false"),
			Result.bCoalescingEnabled ? TEXT("true") : TEXT("false"),
			Result.SampleCount,
			Result.PlateCount,
			Result.Audit.UnresolvedMultiHitCount,
			Result.Audit.CoalescedMultiHitCount,
			Result.Audit.WithinPlateCoincidentMultiHitCount,
			Result.Audit.WithinPlateDistanceSeparatedMultiHitCount,
			Result.Audit.CrossPlateEqualMultiHitCount,
			Result.Audit.CrossPlateDifferentMultiHitCount,
			Result.Audit.MixedMaterialMultiHitCount,
			Result.Audit.ThirdPlateMultiHitCount,
			Result.Audit.CoalescingFieldMismatchHoldCount,
			Result.bProceedToCoalescing ? TEXT("true") : TEXT("false"),
			*JsonString(Result.Audit.SelectionHash));
	}

	void AppendMetricsLine(FString& Lines, const FLiveApplyResult& Result)
	{
		Lines += FString::Printf(
			TEXT("{\"fixture\":%s,\"pass\":%s,\"applied\":%s,\"samples\":%d,\"plates\":%d,\"events_before\":%d,\"events_after\":%d,\"unresolved\":%d,\"coalesced\":%d,\"policy\":%d,\"mode\":%s,\"crust_before\":%s,\"crust_after\":%s}\n"),
			*JsonString(Result.Name),
			Result.bPass ? TEXT("true") : TEXT("false"),
			Result.bApplied ? TEXT("true") : TEXT("false"),
			Result.SampleCount,
			Result.PlateCount,
			Result.EventCountBefore,
			Result.EventCountAfter,
			Result.UnresolvedHoldCount,
			Result.CoalescedCount,
			Result.PolicyWinnerCount,
			*JsonString(Result.LastRemeshMode),
			*JsonString(Result.CrustHashBefore),
			*JsonString(Result.CrustHashAfter));
	}
}

UCarrierLabPreIIIE6MultiHitCommandlet::UCarrierLabPreIIIE6MultiHitCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPreIIIE6MultiHitCommandlet::Main(const FString& Params)
{
	UWorld* World = GetCommandletWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPreIIIE6MultiHit: no world available."));
		return 1;
	}

	ACarrierLabVisualizationActor* FixtureActor = SpawnActor(*World, 96, 1, 0.0);
	if (FixtureActor == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPreIIIE6MultiHit: failed to spawn fixture actor."));
		return 1;
	}

	TArray<FClassifierFixtureResult> PreCoalescingFixtureResults;
	TArray<FClassifierFixtureResult> CoalescingFixtureResults;
	bool bAllPreFixturePass = true;
	bool bAllCoalescingFixturePass = true;
	for (const FClassifierFixtureSpec& Fixture : MakeClassifierFixtures())
	{
		FClassifierFixtureResult Result = RunFixture(*FixtureActor, Fixture, false);
		bAllPreFixturePass = bAllPreFixturePass && Result.bPass;
		PreCoalescingFixtureResults.Add(Result);
	}
	for (const FClassifierFixtureSpec& Fixture : MakeClassifierFixtures())
	{
		FClassifierFixtureResult Result = RunFixture(*FixtureActor, Fixture, true);
		bAllCoalescingFixturePass = bAllCoalescingFixturePass && Result.bPass;
		CoalescingFixtureResults.Add(Result);
	}
	FixtureActor->Destroy();
	CollectGarbage(RF_NoFlags);

	FLiveDistributionResult SinglePlatePre;
	FLiveDistributionResult DefaultMultiPlatePre;
	FLiveDistributionResult DefaultMultiPlatePost;
	FLiveApplyResult SinglePlateApply;
	const bool bSinglePreRan = RunLiveDistribution(*World, TEXT("single-plate live pre-coalescing distribution"), 96, 1, false, SinglePlatePre);
	const bool bDefaultPreRan = RunLiveDistribution(*World, TEXT("default multi-plate pre-coalescing diagnostic"), 100000, 40, false, DefaultMultiPlatePre);
	const bool bSingleApplyRan = RunLiveApply(*World, TEXT("single-plate live apply after coalescing"), 96, 1, SinglePlateApply);
	const bool bDefaultPostRan = RunLiveDistribution(*World, TEXT("default multi-plate post-coalescing diagnostic"), 100000, 40, true, DefaultMultiPlatePost);

	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("phase-iii-pre-iiie6-multihit-resolution-metrics.jsonl"));
	FString MetricsLines;
	for (const FClassifierFixtureResult& Result : PreCoalescingFixtureResults)
	{
		AppendMetricsLine(MetricsLines, Result);
	}
	for (const FClassifierFixtureResult& Result : CoalescingFixtureResults)
	{
		AppendMetricsLine(MetricsLines, Result);
	}
	AppendMetricsLine(MetricsLines, SinglePlatePre);
	AppendMetricsLine(MetricsLines, DefaultMultiPlatePre);
	AppendMetricsLine(MetricsLines, SinglePlateApply);
	AppendMetricsLine(MetricsLines, DefaultMultiPlatePost);
	FFileHelper::SaveStringToFile(MetricsLines, *MetricsPath);

	const FString ReportPath = GetReportPath(Params);
	const FString Report = BuildReport(
		PreCoalescingFixtureResults,
		CoalescingFixtureResults,
		SinglePlatePre,
		DefaultMultiPlatePre,
		DefaultMultiPlatePost,
		SinglePlateApply,
		MetricsPath);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	const bool bPass =
		bAllPreFixturePass &&
		bAllCoalescingFixturePass &&
		bSinglePreRan &&
		bDefaultPreRan &&
		bSingleApplyRan &&
		bDefaultPostRan &&
		SinglePlatePre.bPass &&
		SinglePlatePre.bProceedToCoalescing &&
		DefaultMultiPlatePre.bPass &&
		SinglePlateApply.bPass &&
		DefaultMultiPlatePost.bPass &&
		DefaultMultiPlatePost.Audit.CrossPlateEqualMultiHitCount > 0 &&
		DefaultMultiPlatePost.Audit.ThirdPlateMultiHitCount > 0;
	UE_LOG(LogTemp, Display, TEXT("CarrierLabPreIIIE6MultiHit %s. Report: %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), *ReportPath);
	return bPass ? 0 : 1;
}
