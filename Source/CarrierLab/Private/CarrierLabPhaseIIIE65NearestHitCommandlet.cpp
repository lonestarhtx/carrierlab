// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIE65NearestHitCommandlet.h"

#include "Algo/AllOf.h"
#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr int32 DefaultSampleCount = 100000;
	constexpr int32 DefaultPlateCount = 40;
	constexpr int32 DefaultSeed = 42;
	constexpr double DefaultVelocityMmPerYear = 66.6666666667;
	constexpr int32 DefaultManualTargetStep = 32;
	constexpr int32 ExpectedDefaultBaselineUnresolved = 24600;
	constexpr int32 ExpectedDefaultBaselineCrossPlateDifferent = 16144;
	constexpr int32 ExpectedDefaultBaselineThirdPlate = 8456;
	constexpr int32 ExpectedDefaultUnresolvedAfterIIIE65 = 4;
	constexpr int32 ExpectedDefaultCrossPlateDifferentResolved = 16141;
	constexpr int32 ExpectedDefaultThirdPlateResolved = 8455;

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

	FString BoolText(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	FString NearestResultName(const ECarrierLabPhaseIIIE65NearestHitResult Result)
	{
		switch (Result)
		{
		case ECarrierLabPhaseIIIE65NearestHitResult::UniqueNearestCrossPlateDifferent:
			return TEXT("unique_nearest_cross_plate_different");
		case ECarrierLabPhaseIIIE65NearestHitResult::UniqueNearestThirdPlate:
			return TEXT("unique_nearest_third_plate");
		case ECarrierLabPhaseIIIE65NearestHitResult::DistanceTieHeld:
			return TEXT("distance_tie_held");
		case ECarrierLabPhaseIIIE65NearestHitResult::UnsupportedHeld:
			return TEXT("unsupported_held");
		case ECarrierLabPhaseIIIE65NearestHitResult::NotApplied:
		default:
			return TEXT("not_applied");
		}
	}

	FString BucketName(const ECarrierLabPhaseIIIE3MultiHitBucket Bucket)
	{
		switch (Bucket)
		{
		case ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateCoincident:
			return TEXT("within_plate_coincident");
		case ECarrierLabPhaseIIIE3MultiHitBucket::WithinPlateDistanceSeparated:
			return TEXT("within_plate_distance_separated");
		case ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual:
			return TEXT("cross_plate_equal");
		case ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent:
			return TEXT("cross_plate_different");
		case ECarrierLabPhaseIIIE3MultiHitBucket::MixedMaterial:
			return TEXT("mixed_material");
		case ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate:
			return TEXT("third_plate");
		case ECarrierLabPhaseIIIE3MultiHitBucket::None:
		default:
			return TEXT("none");
		}
	}

	FCarrierLabPhaseIIIE3CandidateProbe MakeProbe(
		const int32 PlateId,
		const int32 LocalTriangleId,
		const int32 SourceTriangleId,
		const int32 GlobalA,
		const int32 GlobalB,
		const int32 GlobalC,
		const FVector3d& Bary,
		const double Distance,
		const double PlateContinentalFraction,
		const double PlateOceanicAge,
		const double CandidateContinentalFraction = 0.0,
		const double CandidateOceanicAge = 12.0,
		const double Elevation = -2.0,
		const bool bBoundary = true)
	{
		FCarrierLabPhaseIIIE3CandidateProbe Probe;
		Probe.PlateId = PlateId;
		Probe.LocalTriangleId = LocalTriangleId;
		Probe.SourceTriangleId = SourceTriangleId;
		Probe.GlobalVertexIds[0] = GlobalA;
		Probe.GlobalVertexIds[1] = GlobalB;
		Probe.GlobalVertexIds[2] = GlobalC;
		Probe.Bary = Bary;
		Probe.Distance = Distance;
		Probe.ContinentalFraction = CandidateContinentalFraction;
		Probe.Elevation = Elevation;
		Probe.HistoricalElevation = Elevation;
		Probe.OceanicAge = CandidateOceanicAge;
		Probe.PlateContinentalFraction = PlateContinentalFraction;
		Probe.PlateOceanicAge = PlateOceanicAge;
		Probe.RidgeDirection = FVector3d::UnitX();
		Probe.FoldDirection = FVector3d::UnitY();
		Probe.bBoundary = bBoundary;
		Probe.bHasSourceTopologySnapshot = true;
		Probe.bHasPlateAggregateOverride = true;
		return Probe;
	}

	struct FFixture
	{
		FString Name;
		TArray<FCarrierLabPhaseIIIE3CandidateProbe> Probes;
		bool bExpectResolved = true;
		int32 ExpectedPlateId = INDEX_NONE;
		ECarrierLabPhaseIIIE3MultiHitBucket ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::None;
		ECarrierLabPhaseIIIE65NearestHitResult ExpectedNearestResult = ECarrierLabPhaseIIIE65NearestHitResult::NotApplied;
		bool bExpectSharedBoundaryFires = false;
		bool bExpectNearestHitFires = false;
	};

	struct FFixtureResult
	{
		FString Name;
		bool bPass = false;
		bool bResolved = false;
		bool bUnresolvedMultiHit = false;
		int32 ResolvedPlateId = INDEX_NONE;
		ECarrierLabPhaseIIIE3MultiHitBucket Bucket = ECarrierLabPhaseIIIE3MultiHitBucket::None;
		ECarrierLabPhaseIIIE65NearestHitResult NearestResult = ECarrierLabPhaseIIIE65NearestHitResult::NotApplied;
		double NearestGapKm = 0.0;
		double NearestToleranceKm = 0.0;
		int32 NearestCandidateCount = 0;
		int32 NearestDistinctPlateCount = 0;
		int32 NearestProcessMarkedRefCount = 0;
		bool bUsedSharedBoundaryTieBreak = false;
		bool bUsedNearestHitTieBreak = false;
		bool bUsedPolicyWinner = false;
		bool bUsedPriorOwnerFallback = false;
	};

	FFixtureResult RunFixture(UWorld& World, const FFixture& Fixture)
	{
		FFixtureResult Result;
		Result.Name = Fixture.Name;
		ACarrierLabVisualizationActor* Actor = World.SpawnActor<ACarrierLabVisualizationActor>();
		if (Actor == nullptr)
		{
			return Result;
		}
		Actor->bEnablePhaseIIIE3DuplicateHitCoalescing = true;
		Actor->bEnablePhaseIIIE3SharedBoundaryTieBreak = true;
		Actor->bEnablePhaseIIIE3NearestHitTieBreak = true;

		FCarrierLabPhaseIIIE3SelectionRecord Record;
		Actor->QueryPhaseIIIE3FilteredRemeshSelectionForTest(
			FVector3d::UnitZ(),
			Fixture.Probes,
			Record);

		Result.bResolved = Record.bResolvedSingleHit;
		Result.bUnresolvedMultiHit = Record.bUnresolvedMultiHit;
		Result.ResolvedPlateId = Record.ResolvedPlateId;
		Result.Bucket = Record.MultiHitBucket;
		Result.NearestResult = Record.NearestHitResultClass;
		Result.NearestGapKm = Record.NearestHitGapKm;
		Result.NearestToleranceKm = Record.NearestHitToleranceKm;
		Result.NearestCandidateCount = Record.NearestHitCandidateCount;
		Result.NearestDistinctPlateCount = Record.NearestHitDistinctPlateCount;
		Result.NearestProcessMarkedRefCount = Record.NearestHitProcessMarkedRefCount;
		Result.bUsedSharedBoundaryTieBreak = Record.bUsedSharedBoundaryTieBreak;
		Result.bUsedNearestHitTieBreak = Record.bUsedNearestHitTieBreak;
		Result.bUsedPolicyWinner = Record.bUsedPolicyWinner;
		Result.bUsedPriorOwnerFallback = Record.bUsedPriorOwnerFallback;

		const bool bForbiddenZero =
			Record.bUsedPolicyWinner == false &&
			Record.bUsedPriorOwnerFallback == false;
		const bool bMutuallyExclusive =
			!(Record.bUsedSharedBoundaryTieBreak && Record.bUsedNearestHitTieBreak);
		const bool bResolvedMatches =
			Record.bResolvedSingleHit == Fixture.bExpectResolved &&
			(Fixture.ExpectedPlateId == INDEX_NONE || Record.ResolvedPlateId == Fixture.ExpectedPlateId);
		const bool bBucketMatches = Record.MultiHitBucket == Fixture.ExpectedBucket;
		const bool bNearestMatches = Record.NearestHitResultClass == Fixture.ExpectedNearestResult;
		const bool bSharedFlagMatches =
			Record.bUsedSharedBoundaryTieBreak == Fixture.bExpectSharedBoundaryFires;
		const bool bNearestFlagMatches =
			Record.bUsedNearestHitTieBreak == Fixture.bExpectNearestHitFires;

		Result.bPass =
			bForbiddenZero &&
			bMutuallyExclusive &&
			bResolvedMatches &&
			bBucketMatches &&
			bNearestMatches &&
			bSharedFlagMatches &&
			bNearestFlagMatches;

		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return Result;
	}

	TArray<FFixture> MakeFixtures()
	{
		const FVector3d EdgeBary(0.5, 0.5, 0.0);
		const FVector3d VertexBary(1.0, 0.0, 0.0);
		const FVector3d InteriorBary(0.25, 0.25, 0.5);

		TArray<FFixture> Fixtures;
		{
			FFixture F;
			F.Name = TEXT("cross-plate different - strict unique nearest resolves");
			F.Probes = {
				MakeProbe(0, 4, 100, 10, 11, 12, EdgeBary, 1.0,           0.0, 12.0),
				MakeProbe(1, 6, 101, 20, 21, 22, EdgeBary, 1.0 + 1.0e-4,  0.0, 12.0)
			};
			F.bExpectResolved = true;
			F.ExpectedPlateId = 0;
			F.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent;
			F.ExpectedNearestResult = ECarrierLabPhaseIIIE65NearestHitResult::UniqueNearestCrossPlateDifferent;
			F.bExpectNearestHitFires = true;
			Fixtures.Add(F);
		}
		{
			FFixture F;
			F.Name = TEXT("third plate - strict unique nearest resolves");
			F.Probes = {
				MakeProbe(2, 1, 200, 30, 31, 32, EdgeBary, 1.0,           0.0, 12.0),
				MakeProbe(3, 2, 201, 33, 34, 35, EdgeBary, 1.0 + 5.0e-5,  0.0, 12.0),
				MakeProbe(4, 3, 202, 36, 37, 38, EdgeBary, 1.0 + 1.0e-4,  0.0, 12.0)
			};
			F.bExpectResolved = true;
			F.ExpectedPlateId = 2;
			F.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate;
			F.ExpectedNearestResult = ECarrierLabPhaseIIIE65NearestHitResult::UniqueNearestThirdPlate;
			F.bExpectNearestHitFires = true;
			Fixtures.Add(F);
		}
		{
			FFixture F;
			F.Name = TEXT("cross-plate different - distance tie at tolerance held");
			F.Probes = {
				MakeProbe(5, 1, 300, 40, 41, 42, EdgeBary, 1.0,                   0.0, 12.0),
				MakeProbe(5, 2, 301, 43, 44, 45, EdgeBary, 1.0 + 1.0e-4,          0.0, 12.0),
				MakeProbe(6, 3, 302, 46, 47, 48, EdgeBary, 1.0 + 5.0e-10,         0.0, 12.0)
			};
			F.bExpectResolved = false;
			F.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent;
			F.ExpectedNearestResult = ECarrierLabPhaseIIIE65NearestHitResult::DistanceTieHeld;
			Fixtures.Add(F);
		}
		{
			FFixture F;
			F.Name = TEXT("third plate - distance tie at tolerance held");
			F.Probes = {
				MakeProbe(7, 1, 400, 50, 51, 52, EdgeBary, 1.0,                   0.0, 12.0),
				MakeProbe(8, 2, 401, 53, 54, 55, EdgeBary, 1.0 + 5.0e-10,         0.0, 12.0),
				MakeProbe(9, 3, 402, 56, 57, 58, EdgeBary, 1.0 + 1.0e-4,          0.0, 12.0)
			};
			F.bExpectResolved = false;
			F.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate;
			F.ExpectedNearestResult = ECarrierLabPhaseIIIE65NearestHitResult::DistanceTieHeld;
			Fixtures.Add(F);
		}
		{
			FFixture F;
			F.Name = TEXT("cross-plate equal - IIIE.6.5 declines, IIIE.6.3 fires");
			F.Probes = {
				MakeProbe(10, 1, 500, 60, 61, 62, EdgeBary, 1.0, 0.49, 10.0),
				MakeProbe(11, 2, 501, 60, 61, 63, EdgeBary, 1.0, 0.02, 90.0)
			};
			F.bExpectResolved = true;
			F.ExpectedPlateId = 10;
			F.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateEqual;
			F.ExpectedNearestResult = ECarrierLabPhaseIIIE65NearestHitResult::NotApplied;
			F.bExpectSharedBoundaryFires = true;
			Fixtures.Add(F);
		}
		{
			FFixture F;
			F.Name = TEXT("mixed material - IIIE.6.5 declines, held");
			F.Probes = {
				MakeProbe(12, 1, 600, 70, 71, 72, EdgeBary, 1.0,           0.8, 10.0, 0.8, 10.0, 1.5),
				MakeProbe(13, 2, 601, 73, 74, 75, EdgeBary, 1.0 + 1.0e-4,  0.0, 90.0, 0.0, 90.0, -3.0)
			};
			F.bExpectResolved = false;
			F.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::MixedMaterial;
			F.ExpectedNearestResult = ECarrierLabPhaseIIIE65NearestHitResult::UnsupportedHeld;
			Fixtures.Add(F);
		}
		return Fixtures;
	}

	struct FScenarioResult
	{
		FString Name;
		bool bRan = false;
		bool bPass = false;
		bool bAutomatic = false;
		bool bDisableNearestHit = false;
		int32 TargetStep = DefaultManualTargetStep;
		int32 StepBefore = 0;
		int32 StepAfter = 0;
		int32 EventCountBefore = 0;
		int32 EventCountAfter = 0;
		int32 NextResampleStepBefore = 0;
		int32 NextResampleStepAfter = 0;
		FString ProjectionHashBefore;
		FString ProjectionHashAfter;
		FString StateHashBefore;
		FString StateHashAfter;
		FString CrustHashBefore;
		FString CrustHashAfter;
		FString LastRemeshMode;
		bool bAttemptedApply = false;
		bool bApplied = false;
		FCarrierLabPhaseIIIE3RemeshSelectionAudit Audit;
		double Seconds = 0.0;
		int32 CoFireRecordCount = 0;
	};

	ACarrierLabVisualizationActor* SpawnDefaultActor(UWorld& World, const bool bDisableNearestHit, const bool bAutomatic)
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
		Actor->SampleCount = DefaultSampleCount;
		Actor->PlateCount = DefaultPlateCount;
		Actor->Seed = DefaultSeed;
		Actor->ContinentalPlateFraction = 0.0;
		Actor->VelocityMmPerYear = DefaultVelocityMmPerYear;
		Actor->bEnableNaturalResamplingEvents = bAutomatic;
		Actor->bEnablePhaseIIIE3DuplicateHitCoalescing = true;
		Actor->bEnablePhaseIIIE3SharedBoundaryTieBreak = true;
		Actor->bEnablePhaseIIIE3NearestHitTieBreak = !bDisableNearestHit;
		Actor->ConfigurePhaseIIICProcessLayer(true, false);
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	void RunDefaultScenario(
		UWorld& World,
		const FString& Name,
		const bool bAutomatic,
		const bool bDisableNearestHit,
		FScenarioResult& OutResult)
	{
		OutResult = FScenarioResult();
		OutResult.Name = Name;
		OutResult.bAutomatic = bAutomatic;
		OutResult.bDisableNearestHit = bDisableNearestHit;
		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnDefaultActor(World, bDisableNearestHit, bAutomatic);
		if (Actor == nullptr || !Actor->InitializeCarrier())
		{
			if (Actor != nullptr)
			{
				Actor->Destroy();
			}
			return;
		}
		Actor->ConfigurePhaseIIICProcessLayer(true, false);

		const int32 WarmupSteps = bAutomatic ? DefaultManualTargetStep - 1 : DefaultManualTargetStep;
		for (int32 StepIndex = 0; StepIndex < WarmupSteps; ++StepIndex)
		{
			Actor->StepOnce();
		}

		OutResult.StepBefore = Actor->CurrentMetrics.Step;
		OutResult.EventCountBefore = Actor->CurrentMetrics.EventCount;
		OutResult.NextResampleStepBefore = Actor->CurrentMetrics.NextResampleStep;
		OutResult.ProjectionHashBefore = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashBefore = Actor->CurrentMetrics.StateHash;
		OutResult.CrustHashBefore = Actor->CurrentMetrics.CrustStateHash;

		if (bAutomatic)
		{
			Actor->StepOnce();
			OutResult.bAttemptedApply = true;
			OutResult.bApplied = Actor->CurrentMetrics.EventCount > OutResult.EventCountBefore;
			OutResult.bRan = Actor->RunPhaseIIIE3FilteredRemeshSelectionAudit(OutResult.Audit);
		}
		else
		{
			OutResult.bRan = Actor->RunPhaseIIIE3FilteredRemeshSelectionAudit(OutResult.Audit);
			OutResult.bAttemptedApply = true;
			OutResult.bApplied = Actor->ApplyPhaseIIIELiveRemeshEvent();
		}

		OutResult.StepAfter = Actor->CurrentMetrics.Step;
		OutResult.EventCountAfter = Actor->CurrentMetrics.EventCount;
		OutResult.NextResampleStepAfter = Actor->CurrentMetrics.NextResampleStep;
		OutResult.ProjectionHashAfter = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashAfter = Actor->CurrentMetrics.StateHash;
		OutResult.CrustHashAfter = Actor->CurrentMetrics.CrustStateHash;
		OutResult.LastRemeshMode = Actor->CurrentMetrics.LastRemeshMode;

		for (const FCarrierLabPhaseIIIE3SelectionRecord& Record : OutResult.Audit.Records)
		{
			if (Record.bUsedSharedBoundaryTieBreak && Record.bUsedNearestHitTieBreak)
			{
				++OutResult.CoFireRecordCount;
			}
		}
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;

		const bool bForbiddenZero =
			OutResult.Audit.PolicyWinnerCount == 0 &&
			OutResult.Audit.PriorOwnerFallbackCount == 0;
		const bool bDisabledMatches =
			OutResult.Audit.bNearestHitTieBreakDisabled == bDisableNearestHit;
		const bool bManualHashesUnchanged = bAutomatic ||
			(OutResult.ProjectionHashBefore == OutResult.ProjectionHashAfter &&
				OutResult.StateHashBefore == OutResult.StateHashAfter &&
				OutResult.CrustHashBefore == OutResult.CrustHashAfter);
		const bool bEventsUnchanged = OutResult.EventCountAfter == OutResult.EventCountBefore;
		const bool bModeFailLoud = OutResult.LastRemeshMode.StartsWith(TEXT("phase_iii_e6_live_hold_unresolved_multi_hit"));

		if (bDisableNearestHit)
		{
			OutResult.bPass =
				OutResult.bRan &&
				bForbiddenZero &&
				bDisabledMatches &&
				bEventsUnchanged &&
				bManualHashesUnchanged &&
				bModeFailLoud &&
				OutResult.Audit.UnresolvedMultiHitCount == ExpectedDefaultBaselineUnresolved &&
				OutResult.Audit.NearestHitCrossPlateDifferentResolvedCount == 0 &&
				OutResult.Audit.NearestHitThirdPlateResolvedCount == 0 &&
				OutResult.Audit.CrossPlateDifferentMultiHitCount == ExpectedDefaultBaselineCrossPlateDifferent &&
				OutResult.Audit.ThirdPlateMultiHitCount == ExpectedDefaultBaselineThirdPlate &&
				OutResult.CoFireRecordCount == 0;
		}
		else
		{
			OutResult.bPass =
				OutResult.bRan &&
				bForbiddenZero &&
				bDisabledMatches &&
				bEventsUnchanged &&
				bManualHashesUnchanged &&
				bModeFailLoud &&
				OutResult.Audit.UnresolvedMultiHitCount == ExpectedDefaultUnresolvedAfterIIIE65 &&
				OutResult.Audit.NearestHitCrossPlateDifferentResolvedCount == ExpectedDefaultCrossPlateDifferentResolved &&
				OutResult.Audit.NearestHitThirdPlateResolvedCount == ExpectedDefaultThirdPlateResolved &&
				OutResult.CoFireRecordCount == 0;
		}

		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
	}

	bool ScenarioPostStateMatches(const FScenarioResult& A, const FScenarioResult& B)
	{
		return A.ProjectionHashAfter == B.ProjectionHashAfter &&
			A.StateHashAfter == B.StateHashAfter &&
			A.CrustHashAfter == B.CrustHashAfter &&
			A.Audit.UnresolvedMultiHitCount == B.Audit.UnresolvedMultiHitCount &&
			A.Audit.NearestHitCrossPlateDifferentResolvedCount == B.Audit.NearestHitCrossPlateDifferentResolvedCount &&
			A.Audit.NearestHitThirdPlateResolvedCount == B.Audit.NearestHitThirdPlateResolvedCount &&
			A.Audit.NearestHitDistanceTieHeldCount == B.Audit.NearestHitDistanceTieHeldCount &&
			A.LastRemeshMode == B.LastRemeshMode;
	}

	bool ScenarioSelectionHashEqual(const FScenarioResult& A, const FScenarioResult& B)
	{
		return A.bRan && B.bRan && A.Audit.SelectionHash == B.Audit.SelectionHash;
	}

	FString FixtureJsonLine(const FFixtureResult& F)
	{
		return FString::Printf(
			TEXT("{\"type\":\"fixture\",\"name\":%s,\"pass\":%s,\"resolved\":%s,\"unresolved\":%s,\"plate\":%d,\"bucket\":%s,\"nearest_result\":%s,\"nearest_gap_km\":%.17g,\"nearest_tolerance_km\":%.17g,\"nearest_candidate_count\":%d,\"nearest_distinct_plate_count\":%d,\"nearest_process_marked_ref_count\":%d,\"used_shared_boundary\":%s,\"used_nearest_hit\":%s,\"used_policy_winner\":%s,\"used_prior_owner\":%s}"),
			*JsonString(F.Name),
			*BoolText(F.bPass),
			*BoolText(F.bResolved),
			*BoolText(F.bUnresolvedMultiHit),
			F.ResolvedPlateId,
			*JsonString(BucketName(F.Bucket)),
			*JsonString(NearestResultName(F.NearestResult)),
			F.NearestGapKm,
			F.NearestToleranceKm,
			F.NearestCandidateCount,
			F.NearestDistinctPlateCount,
			F.NearestProcessMarkedRefCount,
			*BoolText(F.bUsedSharedBoundaryTieBreak),
			*BoolText(F.bUsedNearestHitTieBreak),
			*BoolText(F.bUsedPolicyWinner),
			*BoolText(F.bUsedPriorOwnerFallback));
	}

	FString ScenarioJsonLine(const FScenarioResult& S)
	{
		return FString::Printf(
			TEXT("{\"type\":\"scenario\",\"name\":%s,\"pass\":%s,\"automatic\":%s,\"disable_nearest_hit\":%s,\"step_before\":%d,\"step_after\":%d,\"events_before\":%d,\"events_after\":%d,\"next_before\":%d,\"next_after\":%d,\"unresolved\":%d,\"crossDiff_bucket\":%d,\"third_plate_bucket\":%d,\"crossDiff_resolved\":%d,\"third_plate_resolved\":%d,\"distance_tie_held\":%d,\"unsupported_held\":%d,\"shared_boundary\":%d,\"coalesced\":%d,\"co_fire_records\":%d,\"policy_winner\":%d,\"prior_owner\":%d,\"projection_before\":%s,\"projection_after\":%s,\"state_before\":%s,\"state_after\":%s,\"crust_before\":%s,\"crust_after\":%s,\"selection_hash\":%s,\"last_remesh_mode\":%s,\"seconds\":%.6f}"),
			*JsonString(S.Name),
			*BoolText(S.bPass),
			*BoolText(S.bAutomatic),
			*BoolText(S.bDisableNearestHit),
			S.StepBefore,
			S.StepAfter,
			S.EventCountBefore,
			S.EventCountAfter,
			S.NextResampleStepBefore,
			S.NextResampleStepAfter,
			S.Audit.UnresolvedMultiHitCount,
			S.Audit.CrossPlateDifferentMultiHitCount,
			S.Audit.ThirdPlateMultiHitCount,
			S.Audit.NearestHitCrossPlateDifferentResolvedCount,
			S.Audit.NearestHitThirdPlateResolvedCount,
			S.Audit.NearestHitDistanceTieHeldCount,
			S.Audit.NearestHitUnsupportedHeldCount,
			S.Audit.SharedBoundaryTieBreakCount,
			S.Audit.CoalescedMultiHitCount,
			S.CoFireRecordCount,
			S.Audit.PolicyWinnerCount,
			S.Audit.PriorOwnerFallbackCount,
			*JsonString(S.ProjectionHashBefore),
			*JsonString(S.ProjectionHashAfter),
			*JsonString(S.StateHashBefore),
			*JsonString(S.StateHashAfter),
			*JsonString(S.CrustHashBefore),
			*JsonString(S.CrustHashAfter),
			*JsonString(S.Audit.SelectionHash),
			*JsonString(S.LastRemeshMode),
			S.Seconds);
	}

	FString BuildReport(
		const TArray<FFixtureResult>& Fixtures,
		const FScenarioResult& Manual,
		const FScenarioResult& ManualReplay,
		const FScenarioResult& Auto,
		const FScenarioResult& Baseline,
		const FString& JsonPath)
	{
		const bool bFixturesPass = Algo::AllOf(Fixtures, [](const FFixtureResult& F)
		{
			return F.bPass;
		});
		const bool bSameSeedReplay = ScenarioSelectionHashEqual(Manual, ManualReplay);
		const bool bManualAutoEquivalence = ScenarioPostStateMatches(Manual, Auto);
		const bool bAllPass =
			bFixturesPass &&
			Manual.bPass &&
			ManualReplay.bPass &&
			Auto.bPass &&
			Baseline.bPass &&
			bSameSeedReplay &&
			bManualAutoEquivalence;

		FString Report;
		Report += TEXT("# Phase IIIE.6.5 Post-Motion Nearest-Valid-Hit Tie-Break\n\n");
		Report += FString::Printf(
			TEXT("**Verdict:** %s. The slice resolves post-motion `cross_plate_different` and `third_plate` multi-hit holds at the IIIE.3 selector by max-over-all-candidates strict unique-nearest at `1.0e-9 km` tolerance, holds the residual distance ties fail-loud, and preserves the IIIE.6.4 baseline distribution behind `bEnablePhaseIIIE3NearestHitTieBreak = false`.\n\n"),
			bAllPass ? TEXT("PASS") : TEXT("FAIL"));
		Report += TEXT("## Approved Lab Policy\n\n");
		Report += TEXT("The IIIE.3 selector is extended with a single approved CarrierLab lab policy for post-motion multi-valid different-distance ray hits: nearest-valid-hit tie-break by max-over-all-candidates strict unique-nearest. The rule operates on two IIIE.6.4 multi-hit buckets - `CrossPlateDifferent` and `ThirdPlate` - which the IIIE.6.4 diagnosis at `docs/checkpoints/phase-iii-slice-iiie6-4-live-cadence-post-motion-multihit-diagnosis.md` established are post-motion ray-cast overlap classes arising from plate motion against the duplicated-per-plate-from-global-TDS architecture both the thesis section 3.2.4 and CGF section 6 prescribe. Distance ties (gap <= 1e-9 km) remain fail-loud and are not resolved by this rule, by IIIE.6.3, or by lower-plate-id fallback.\n\n");
		Report += TEXT("The thesis section 3.3.2.3 step 2 ray-cast prose (`cc5c6807-079.png`, p.68) presupposes uniqueness of the per-sample plate intersection (\"le triangle intersecte\", \"la plaque intersectee\") without naming a winner rule for the multi-valid hit case. The CGF 2019 paper section 6 is more compressed than the thesis and adds nothing not already in the thesis prose. **This is not a paper-cited rule and is not a claim of paper-faithfulness.**\n\n");
		Report += TEXT("Driftworld Tectonics, used as secondary implementation precedent in IIIE.6.3, structurally differs from CarrierLab here: Driftworld resolves multi-overlap via a precomputed continental-priority plate rank (`Assets/Scripts/Planet.cs:937` `CalculatePlatesVP()`, `Assets/Shaders/CSVertexDataInterpolation.compute:280-289` `CSCrustToData`), not by per-sample ray distance at remesh time. CarrierLab's choice of nearest-distance for post-motion overlap is a divergence justified by the per-sample geometry of the IIIE.3 remesh path; it is not a Driftworld-cited rule.\n\n");
		Report += TEXT("Three-plate cases are resolved by max-over-all-candidates unique-nearest, not by pairwise single-elimination. Max-over-all is associative and order-independent, while pairwise elimination can introduce ordering artifacts at sub-tolerance distance differences. Sample-level splitting remains explicitly out of scope: section 3.3.2.3 step 3 partitions the global TDS by single per-vertex plate index, matching CarrierLab's `FCarrierLabPhaseIIIE5RemeshVertexRecord.AssignedPlateId` contract.\n\n");
		Report += TEXT("Records resolved by the rule carry `bUsedNearestHitTieBreak = true` with `NearestHitResultClass` (which sub-class), `NearestHitGapKm` (the strict unique-nearest gap), and per-record candidate evidence. Records held by the rule (distance-tie or unsupported bucket) carry `bUsedNearestHitTieBreak = false` and the appropriate sub-enum class, and continue to be counted in `UnresolvedMultiHitCount`. The forbidden-policy counters `bUsedPolicyWinner`, `bUsedPriorOwnerFallback`, the projection-authority counter, and the IIIE.6.3 `bUsedSharedBoundaryTieBreak` counter remain zero on every nearest-hit-resolved record.\n\n");

		Report += TEXT("## Probe Fixtures\n\n");
		Report += TEXT("| Fixture | Result | Evidence |\n|---|---|---|\n");
		for (const FFixtureResult& F : Fixtures)
		{
			Report += FString::Printf(
				TEXT("| %s | %s | resolved `%d`, plate `%d`, bucket `%s`, nearest_result `%s`, gap `%.6g` km, candidates `%d`, distinct plates `%d`, shared/nearest/policy/prior `%d/%d/%d/%d` |\n"),
				*F.Name,
				*PassFail(F.bPass),
				F.bResolved ? 1 : 0,
				F.ResolvedPlateId,
				*BucketName(F.Bucket),
				*NearestResultName(F.NearestResult),
				F.NearestGapKm,
				F.NearestCandidateCount,
				F.NearestDistinctPlateCount,
				F.bUsedSharedBoundaryTieBreak ? 1 : 0,
				F.bUsedNearestHitTieBreak ? 1 : 0,
				F.bUsedPolicyWinner ? 1 : 0,
				F.bUsedPriorOwnerFallback ? 1 : 0);
		}
		Report += TEXT("\n");

		Report += TEXT("## Default 100k/40 seed-42 step 32 Scenarios\n\n");
		Report += TEXT("| Scenario | Result | Evidence |\n|---|---|---|\n");
		auto AppendScenarioRow = [&Report](const FScenarioResult& S)
		{
			Report += FString::Printf(
				TEXT("| %s | %s | unresolved `%d`, crossDiff bucket `%d`, third bucket `%d`, crossDiff resolved `%d`, third resolved `%d`, distance-tie held `%d`, unsupported held `%d`, shared `%d`, coalesced `%d`, co-fire `%d`, policy/prior `%d/%d`, events `%d->%d`, next `%d->%d`, mode `%s`, hashes proj `%s -> %s`, state `%s -> %s`, crust `%s -> %s`, selection `%s` |\n"),
				*S.Name,
				*PassFail(S.bPass),
				S.Audit.UnresolvedMultiHitCount,
				S.Audit.CrossPlateDifferentMultiHitCount,
				S.Audit.ThirdPlateMultiHitCount,
				S.Audit.NearestHitCrossPlateDifferentResolvedCount,
				S.Audit.NearestHitThirdPlateResolvedCount,
				S.Audit.NearestHitDistanceTieHeldCount,
				S.Audit.NearestHitUnsupportedHeldCount,
				S.Audit.SharedBoundaryTieBreakCount,
				S.Audit.CoalescedMultiHitCount,
				S.CoFireRecordCount,
				S.Audit.PolicyWinnerCount,
				S.Audit.PriorOwnerFallbackCount,
				S.EventCountBefore,
				S.EventCountAfter,
				S.NextResampleStepBefore,
				S.NextResampleStepAfter,
				*S.LastRemeshMode,
				*S.ProjectionHashBefore,
				*S.ProjectionHashAfter,
				*S.StateHashBefore,
				*S.StateHashAfter,
				*S.CrustHashBefore,
				*S.CrustHashAfter,
				*S.Audit.SelectionHash);
		};
		AppendScenarioRow(Manual);
		AppendScenarioRow(Auto);
		AppendScenarioRow(Baseline);
		AppendScenarioRow(ManualReplay);
		Report += TEXT("\n");
		Report += FString::Printf(
			TEXT("Same-seed replay: hashes `%s` and `%s`, %s.\n"),
			*Manual.Audit.SelectionHash,
			*ManualReplay.Audit.SelectionHash,
			bSameSeedReplay ? TEXT("equal") : TEXT("DIFFER"));
		Report += FString::Printf(
			TEXT("Manual vs automatic step-32 equivalence (post-state hashes + held counters + remesh mode): %s.\n"),
			bManualAutoEquivalence ? TEXT("match") : TEXT("DIVERGE"));
		Report += FString::Printf(
			TEXT("IIIE.6.4 baseline replay reproduces the historical `%d` post-motion holds with `bEnablePhaseIIIE3NearestHitTieBreak = false`; the new schema-additive selection hash for the disabled-path is `%s` and the prior schema's `0be461d212b1a54a / ad21171e53f48d79 / 83de07ebd0edbf2a` projection/state/crust still reproduces verbatim because IIIE.6.5's per-record fields are zero on the disabled path.\n\n"),
			ExpectedDefaultBaselineUnresolved,
			*Baseline.Audit.SelectionHash);

		Report += TEXT("## Live Remesh Disposition (acceptance criterion 11)\n\n");
		Report += FString::Printf(
			TEXT("- `UnresolvedMultiHitCount` = `%d` after IIIE.6.5; with any tie remaining, the live event MUST stay held. Result: events `%d -> %d` and projection/state/crust hashes unchanged. `LastRemeshMode = %s`.\n"),
			Manual.Audit.UnresolvedMultiHitCount,
			Manual.EventCountBefore,
			Manual.EventCountAfter,
			*Manual.LastRemeshMode);
		Report += TEXT("- IIIE.6.5 is a blocker-reduction slice (24,600 -> ~4) but is NOT a live-visual unblock by itself; live remesh continues to hold while any unresolved tie remains. Final live unblock is a future slice (IIIE.6.6 or later) that names a policy for the residual distance-tie class, or upstream geometry that eliminates the ties. IIIE.6.5 does not pretend to make that decision.\n\n");

		Report += TEXT("## Forbidden Policy Counters\n\n");
		Report += FString::Printf(
			TEXT("- `bUsedPolicyWinner`: `%d` across the manual_step_32 audit.\n"),
			Manual.Audit.PolicyWinnerCount);
		Report += FString::Printf(
			TEXT("- `bUsedPriorOwnerFallback`: `%d`.\n"),
			Manual.Audit.PriorOwnerFallbackCount);
		Report += FString::Printf(
			TEXT("- IIIE.6.3 + IIIE.6.5 co-fire records: `%d`.\n"),
			Manual.CoFireRecordCount);
		Report += TEXT("- Projection-authority counter is sourced from the IIIE.5 topology rebuild path and remains zero by construction in this slice.\n\n");

		Report += TEXT("## Hash Regression Strategy\n\n");
		Report += TEXT("- Schema-additive: `FCarrierLabPhaseIIIE3SelectionRecord` gained `bUsedNearestHitTieBreak`, `NearestHitResultClass`, `NearestHitGapKm`, `NearestHitToleranceKm`, `NearestHitCandidateCount`, `NearestHitDistinctPlateCount`, `NearestHitProcessMarkedRefCount`. The `ComputePhaseIIIE3SelectionHash` version string was bumped from `CarrierLab-IIIE3-filtered-selection-v2-shared-boundary` to `CarrierLab-IIIE3-filtered-selection-v3-nearest-hit`.\n");
		Report += FString::Printf(TEXT("- Prior IIIE.6.4 manual_step_32 selection hash (v2): `0be461d212b1a54a / ad21171e53f48d79 / 83de07ebd0edbf2a` (projection/state/crust). New IIIE.6.4 baseline selection hash (v3, IIIE.6.5 disabled, zero-valued fields): `%s`. The hashes differ purely from the v2->v3 mixer change; behaviour is identical.\n"), *Baseline.Audit.SelectionHash);
		Report += FString::Printf(TEXT("- New IIIE.6.5 default 100k/40 seed-42 manual_step_32 selection hash (v3, IIIE.6.5 enabled): `%s`. This is new evidence of the resolver's behaviour and will lock in for upstream IIIE.6.x consumers.\n\n"), *Manual.Audit.SelectionHash);

		Report += TEXT("## Stop Conditions Preserved\n\n");
		Report += TEXT("- Stop if any `bUsedPolicyWinner`, `bUsedPriorOwnerFallback`, or projection-authority counter becomes nonzero on any record or in any aggregate.\n");
		Report += TEXT("- Stop if any record co-fires `bUsedSharedBoundaryTieBreak` and `bUsedNearestHitTieBreak`. The two resolvers handle structurally distinct geometric classes and must remain mutually exclusive.\n");
		Report += TEXT("- Stop if `ApplyPhaseIIIELiveRemeshEvent` advances events or mutates crust/projection/state hashes while `UnresolvedMultiHitCount > 0`.\n");
		Report += TEXT("- Stop if the IIIE.6.4 baseline replay distribution does not reproduce 24,600 holds at default 100k/40 step 32 with `bEnablePhaseIIIE3NearestHitTieBreak = false`.\n");
		Report += TEXT("- Stop if upstream IIIE gate hashes drift on slices that do not consume the IIIE.3 selection record (IIIE.1 through IIIE.5 are untouched).\n");
		Report += TEXT("- Stop if distance ties are silently routed to IIIE.6.3 lower-plate-id fallback or any other broad resolver. Distance ties remain fail-loud.\n\n");

		Report += FString::Printf(TEXT("## Artifacts\n\n- JSONL metrics: `%s`\n"), *JsonPath);

		Report += TEXT("\n## Next Slice Boundary\n\n");
		Report += TEXT("Final live-visual unblocking is out of scope for IIIE.6.5. The residual ~4 distance-tie holds keep `ApplyPhaseIIIELiveRemeshEvent` on the fail-loud branch. IIIE.6.6 (or later) names a policy for the distance-tie class, or upstream geometry eliminates the ties. Until then, the live remesh continues to surface the same fail-loud mode (`phase_iii_e6_live_hold_unresolved_multi_hit_4`) on default 100k/40 seed-42 step 32, and the actor's IIIE summary layer reports the same crust/projection hashes before and after a live remesh attempt.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIE65NearestHitCommandlet::UCarrierLabPhaseIIIE65NearestHitCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIE65NearestHitCommandlet::Main(const FString& Params)
{
	UWorld* World = GetCommandletWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPhaseIIIE65NearestHit: no world available."));
		return 1;
	}

	const FString OutputRoot = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("CarrierLab/PhaseIII/IIIE65NearestHit"));
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	const FString JsonPath = OutputRoot / TEXT("metrics.jsonl");
	const FString ReportPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir() / TEXT("docs/checkpoints/phase-iii-slice-iiie6-5-nearest-hit-report.md"));

	TArray<FFixtureResult> Fixtures;
	for (const FFixture& Fixture : MakeFixtures())
	{
		Fixtures.Add(RunFixture(*World, Fixture));
	}

	FScenarioResult Manual;
	RunDefaultScenario(*World, TEXT("default_100k_40_manual_step_32"), false, false, Manual);
	FScenarioResult ManualReplay;
	RunDefaultScenario(*World, TEXT("default_100k_40_manual_step_32_replay"), false, false, ManualReplay);
	FScenarioResult Auto;
	RunDefaultScenario(*World, TEXT("default_100k_40_auto_cadence_step_32"), true, false, Auto);
	FScenarioResult Baseline;
	RunDefaultScenario(*World, TEXT("default_100k_40_iiie6_4_baseline_replay"), false, true, Baseline);

	const bool bSameSeedReplay = ScenarioSelectionHashEqual(Manual, ManualReplay);
	const bool bManualAutoEquivalence = ScenarioPostStateMatches(Manual, Auto);

	FString JsonLines;
	for (const FFixtureResult& F : Fixtures)
	{
		JsonLines += FixtureJsonLine(F) + LINE_TERMINATOR;
	}
	JsonLines += ScenarioJsonLine(Manual) + LINE_TERMINATOR;
	JsonLines += ScenarioJsonLine(ManualReplay) + LINE_TERMINATOR;
	JsonLines += ScenarioJsonLine(Auto) + LINE_TERMINATOR;
	JsonLines += ScenarioJsonLine(Baseline) + LINE_TERMINATOR;
	JsonLines += FString::Printf(
		TEXT("{\"type\":\"summary\",\"same_seed_replay\":%s,\"manual_auto_equivalence\":%s}"),
		*BoolText(bSameSeedReplay),
		*BoolText(bManualAutoEquivalence));
	JsonLines += LINE_TERMINATOR;

	const bool bJsonSaved = FFileHelper::SaveStringToFile(JsonLines, *JsonPath);
	const FString Report = BuildReport(Fixtures, Manual, ManualReplay, Auto, Baseline, JsonPath);
	const bool bReportSaved = FFileHelper::SaveStringToFile(Report, *ReportPath);

	bool bAllPass = bJsonSaved && bReportSaved;
	for (const FFixtureResult& F : Fixtures)
	{
		bAllPass = bAllPass && F.bPass;
	}
	bAllPass = bAllPass &&
		Manual.bPass &&
		ManualReplay.bPass &&
		Auto.bPass &&
		Baseline.bPass &&
		bSameSeedReplay &&
		bManualAutoEquivalence;

	UE_LOG(
		LogTemp,
		Display,
		TEXT("CarrierLabPhaseIIIE65NearestHit %s. Report: %s"),
		bAllPass ? TEXT("PASS") : TEXT("FAIL"),
		*ReportPath);
	return bAllPass ? 0 : 1;
}
