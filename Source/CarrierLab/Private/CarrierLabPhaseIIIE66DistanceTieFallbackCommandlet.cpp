// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIE66DistanceTieFallbackCommandlet.h"

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
	constexpr int32 ExpectedIIIE64BaselineUnresolved = 24600;
	constexpr int32 ExpectedIIIE65BaselineUnresolved = 4;

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

	FString BoolText(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
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

	FString FallbackLayerName(const ECarrierLabPhaseIIIE66DistanceTieFallbackLayer Layer)
	{
		switch (Layer)
		{
		case ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::ContinentalPriority:
			return TEXT("continental_priority");
		case ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::OlderOceanicAge:
			return TEXT("older_oceanic_age");
		case ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::LowerPlateId:
			return TEXT("lower_plate_id");
		case ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::Unresolved:
			return TEXT("unresolved");
		case ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::None:
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
		const double PlateOceanicAge)
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
		Probe.ContinentalFraction = 0.0;
		Probe.Elevation = -2.0;
		Probe.HistoricalElevation = -2.0;
		Probe.OceanicAge = 24.0;
		Probe.PlateContinentalFraction = PlateContinentalFraction;
		Probe.PlateOceanicAge = PlateOceanicAge;
		Probe.RidgeDirection = FVector3d::UnitX();
		Probe.FoldDirection = FVector3d::UnitY();
		Probe.bBoundary = true;
		Probe.bHasSourceTopologySnapshot = true;
		Probe.bHasPlateAggregateOverride = true;
		return Probe;
	}

	struct FFixture
	{
		FString Name;
		TArray<FCarrierLabPhaseIIIE3CandidateProbe> Probes;
		bool bDisableDistanceTieFallback = false;
		bool bExpectResolved = true;
		int32 ExpectedPlateId = INDEX_NONE;
		ECarrierLabPhaseIIIE3MultiHitBucket ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::None;
		ECarrierLabPhaseIIIE66DistanceTieFallbackLayer ExpectedLayer = ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::None;
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
		ECarrierLabPhaseIIIE66DistanceTieFallbackLayer FallbackLayer = ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::None;
		bool bUsedSharedBoundaryTieBreak = false;
		bool bUsedNearestHitTieBreak = false;
		bool bUsedDistanceTieFallback = false;
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
		Actor->bEnablePhaseIIIE3DistanceTieFallback = !Fixture.bDisableDistanceTieFallback;

		FCarrierLabPhaseIIIE3SelectionRecord Record;
		Actor->QueryPhaseIIIE3FilteredRemeshSelectionForTest(FVector3d::UnitZ(), Fixture.Probes, Record);

		Result.bResolved = Record.bResolvedSingleHit;
		Result.bUnresolvedMultiHit = Record.bUnresolvedMultiHit;
		Result.ResolvedPlateId = Record.ResolvedPlateId;
		Result.Bucket = Record.MultiHitBucket;
		Result.NearestResult = Record.NearestHitResultClass;
		Result.FallbackLayer = Record.DistanceTieFallbackLayer;
		Result.bUsedSharedBoundaryTieBreak = Record.bUsedSharedBoundaryTieBreak;
		Result.bUsedNearestHitTieBreak = Record.bUsedNearestHitTieBreak;
		Result.bUsedDistanceTieFallback = Record.bUsedDistanceTieFallback;
		Result.bUsedPolicyWinner = Record.bUsedPolicyWinner;
		Result.bUsedPriorOwnerFallback = Record.bUsedPriorOwnerFallback;

		const int32 PositiveResolverCount =
			(Record.bUsedSharedBoundaryTieBreak ? 1 : 0) +
			(Record.bUsedNearestHitTieBreak ? 1 : 0) +
			(Record.bUsedDistanceTieFallback ? 1 : 0);
		const bool bForbiddenZero =
			!Record.bUsedPolicyWinner &&
			!Record.bUsedPriorOwnerFallback;
		const bool bResolvedMatches =
			Record.bResolvedSingleHit == Fixture.bExpectResolved &&
			(Fixture.ExpectedPlateId == INDEX_NONE || Record.ResolvedPlateId == Fixture.ExpectedPlateId);
		const bool bBucketMatches = Record.MultiHitBucket == Fixture.ExpectedBucket;
		const bool bNearestClassifiesDistanceTie =
			Record.NearestHitResultClass == ECarrierLabPhaseIIIE65NearestHitResult::DistanceTieHeld;
		const bool bFallbackMatches =
			Record.DistanceTieFallbackLayer == Fixture.ExpectedLayer &&
			Record.bUsedDistanceTieFallback == (!Fixture.bDisableDistanceTieFallback && Fixture.bExpectResolved);

		Result.bPass =
			bForbiddenZero &&
			PositiveResolverCount <= 1 &&
			bResolvedMatches &&
			bBucketMatches &&
			bNearestClassifiesDistanceTie &&
			bFallbackMatches;

		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return Result;
	}

	TArray<FFixture> MakeFixtures()
	{
		const FVector3d EdgeBary(0.5, 0.5, 0.0);
		TArray<FFixture> Fixtures;
		{
			FFixture F;
			F.Name = TEXT("distance tie fallback layer 1 - continental priority");
			F.Probes = {
				MakeProbe(2, 1, 100, 10, 11, 12, EdgeBary, 1.0,          0.10, 20.0),
				MakeProbe(9, 2, 101, 20, 21, 22, EdgeBary, 1.0 + 5e-10, 0.80, 10.0)
			};
			F.ExpectedPlateId = 9;
			F.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent;
			F.ExpectedLayer = ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::ContinentalPriority;
			Fixtures.Add(F);
		}
		{
			FFixture F;
			F.Name = TEXT("distance tie fallback layer 2 - older oceanic age");
			F.Probes = {
				MakeProbe(2, 1, 200, 30, 31, 32, EdgeBary, 1.0,          0.20, 10.0),
				MakeProbe(8, 2, 201, 40, 41, 42, EdgeBary, 1.0 + 5e-10, 0.20, 90.0)
			};
			F.ExpectedPlateId = 8;
			F.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent;
			F.ExpectedLayer = ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::OlderOceanicAge;
			Fixtures.Add(F);
		}
		{
			FFixture F;
			F.Name = TEXT("distance tie fallback layer 3 - lower plate id");
			F.Probes = {
				MakeProbe(8, 1, 300, 50, 51, 52, EdgeBary, 1.0,          0.20, 30.0),
				MakeProbe(2, 2, 301, 60, 61, 62, EdgeBary, 1.0 + 5e-10, 0.20, 30.0)
			};
			F.ExpectedPlateId = 2;
			F.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent;
			F.ExpectedLayer = ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::LowerPlateId;
			Fixtures.Add(F);
		}
		{
			FFixture F;
			F.Name = TEXT("third-plate distance tie fallback layer 1 - max over all candidates");
			F.Probes = {
				MakeProbe(3, 1, 400, 70, 71, 72, EdgeBary, 1.0,          0.10, 20.0),
				MakeProbe(4, 2, 401, 80, 81, 82, EdgeBary, 1.0 + 2e-10, 0.90, 10.0),
				MakeProbe(5, 3, 402, 90, 91, 92, EdgeBary, 1.0 + 5e-10, 0.30, 80.0)
			};
			F.ExpectedPlateId = 4;
			F.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::ThirdPlate;
			F.ExpectedLayer = ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::ContinentalPriority;
			Fixtures.Add(F);
		}
		{
			FFixture F;
			F.Name = TEXT("distance tie fallback opt-out preserves IIIE.6.5 hold");
			F.bDisableDistanceTieFallback = true;
			F.bExpectResolved = false;
			F.Probes = {
				MakeProbe(2, 1, 500, 110, 111, 112, EdgeBary, 1.0,          0.10, 20.0),
				MakeProbe(9, 2, 501, 120, 121, 122, EdgeBary, 1.0 + 5e-10, 0.80, 10.0)
			};
			F.ExpectedBucket = ECarrierLabPhaseIIIE3MultiHitBucket::CrossPlateDifferent;
			F.ExpectedLayer = ECarrierLabPhaseIIIE66DistanceTieFallbackLayer::None;
			Fixtures.Add(F);
		}
		return Fixtures;
	}

	struct FScenario
	{
		FString Name;
		int32 TargetStep = 32;
		bool bAutomatic = false;
		bool bAttemptApply = false;
		bool bDisableNearestHit = false;
		bool bDisableDistanceTieFallback = false;
	};

	struct FScenarioResult
	{
		FString Name;
		bool bPass = false;
		bool bRan = false;
		bool bApplied = false;
		int32 TargetStep = 32;
		bool bAutomatic = false;
		bool bAttemptApply = false;
		bool bDisableNearestHit = false;
		bool bDisableDistanceTieFallback = false;
		int32 StepBefore = 0;
		int32 StepAfter = 0;
		int32 EventCountBefore = 0;
		int32 EventCountAfter = 0;
		FString ProjectionHashBefore;
		FString ProjectionHashAfter;
		FString StateHashBefore;
		FString StateHashAfter;
		FString CrustHashBefore;
		FString CrustHashAfter;
		FString LastRemeshMode;
		FCarrierLabPhaseIIIE3RemeshSelectionAudit Audit;
		double Seconds = 0.0;
		int32 CoFireRecordCount = 0;
		int32 InvalidRecordCount = 0;
		int32 GeneratedCandidateCount = 0;
		int32 AppliedGeneratedCount = 0;
		int32 RiftingPendingCount = 0;
		int32 NoBoundaryPairMissCount = 0;
		int32 NonSeparatingGapFillCount = 0;
	};

	ACarrierLabVisualizationActor* SpawnDefaultActor(UWorld& World, const FScenario& Scenario)
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
		Actor->bEnableNaturalResamplingEvents = Scenario.bAutomatic;
		Actor->bEnablePhaseIIIE3DuplicateHitCoalescing = true;
		Actor->bEnablePhaseIIIE3SharedBoundaryTieBreak = true;
		Actor->bEnablePhaseIIIE3NearestHitTieBreak = !Scenario.bDisableNearestHit;
		Actor->bEnablePhaseIIIE3DistanceTieFallback = !Scenario.bDisableDistanceTieFallback;
		Actor->ConfigurePhaseIIICProcessLayer(true, false);
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	void RunScenario(UWorld& World, const FScenario& Scenario, FScenarioResult& OutResult)
	{
		UE_LOG(
			LogTemp,
			Display,
			TEXT("CarrierLabPhaseIIIE66DistanceTieFallback: scenario start %s step=%d auto=%d apply=%d disable_nearest=%d disable_dtie=%d"),
			*Scenario.Name,
			Scenario.TargetStep,
			Scenario.bAutomatic ? 1 : 0,
			Scenario.bAttemptApply ? 1 : 0,
			Scenario.bDisableNearestHit ? 1 : 0,
			Scenario.bDisableDistanceTieFallback ? 1 : 0);

		OutResult = FScenarioResult();
		OutResult.Name = Scenario.Name;
		OutResult.TargetStep = Scenario.TargetStep;
		OutResult.bAutomatic = Scenario.bAutomatic;
		OutResult.bAttemptApply = Scenario.bAttemptApply;
		OutResult.bDisableNearestHit = Scenario.bDisableNearestHit;
		OutResult.bDisableDistanceTieFallback = Scenario.bDisableDistanceTieFallback;
		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnDefaultActor(World, Scenario);
		if (Actor == nullptr || !Actor->InitializeCarrier())
		{
			if (Actor != nullptr)
			{
				Actor->Destroy();
			}
			return;
		}

		const int32 WarmupSteps = Scenario.bAutomatic ? Scenario.TargetStep - 1 : Scenario.TargetStep;
		for (int32 StepIndex = 0; StepIndex < WarmupSteps; ++StepIndex)
		{
			Actor->StepOnce();
		}

		OutResult.StepBefore = Actor->CurrentMetrics.Step;
		OutResult.EventCountBefore = Actor->CurrentMetrics.EventCount;
		OutResult.ProjectionHashBefore = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashBefore = Actor->CurrentMetrics.StateHash;
		OutResult.CrustHashBefore = Actor->CurrentMetrics.CrustStateHash;
		OutResult.bRan = Actor->RunPhaseIIIE3FilteredRemeshSelectionAudit(OutResult.Audit);

		if (Scenario.bAttemptApply && Scenario.bAutomatic)
		{
			Actor->StepOnce();
			OutResult.bApplied = Actor->CurrentMetrics.EventCount > OutResult.EventCountBefore;
		}
		else if (Scenario.bAttemptApply)
		{
			OutResult.bApplied = Actor->ApplyPhaseIIIELiveRemeshEvent();
		}

		OutResult.StepAfter = Actor->CurrentMetrics.Step;
		OutResult.EventCountAfter = Actor->CurrentMetrics.EventCount;
		OutResult.ProjectionHashAfter = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashAfter = Actor->CurrentMetrics.StateHash;
		OutResult.CrustHashAfter = Actor->CurrentMetrics.CrustStateHash;
		OutResult.LastRemeshMode = Scenario.bAttemptApply
			? Actor->CurrentMetrics.LastRemeshMode
			: TEXT("selection_only");
		OutResult.InvalidRecordCount = Actor->CurrentMetrics.PhaseIIIELastInvalidRecordCount;
		OutResult.GeneratedCandidateCount = Actor->CurrentMetrics.PhaseIIIELastGeneratedCandidateCount;
		OutResult.AppliedGeneratedCount = Actor->CurrentMetrics.PhaseIIIELastAppliedGeneratedCount;
		OutResult.RiftingPendingCount = Actor->CurrentMetrics.PhaseIIIELastRiftingPendingCount;
		OutResult.NoBoundaryPairMissCount = Actor->CurrentMetrics.LastNoBoundaryPairMissCount;
		OutResult.NonSeparatingGapFillCount = Actor->CurrentMetrics.LastNonSeparatingGapFillCount;
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;

		for (const FCarrierLabPhaseIIIE3SelectionRecord& Record : OutResult.Audit.Records)
		{
			const int32 PositiveResolverCount =
				(Record.bUsedSharedBoundaryTieBreak ? 1 : 0) +
				(Record.bUsedNearestHitTieBreak ? 1 : 0) +
				(Record.bUsedDistanceTieFallback ? 1 : 0);
			if (PositiveResolverCount > 1)
			{
				++OutResult.CoFireRecordCount;
			}
		}

		const bool bForbiddenZero =
			OutResult.Audit.PolicyWinnerCount == 0 &&
			OutResult.Audit.PriorOwnerFallbackCount == 0;
		const bool bDisabledMatches =
			OutResult.Audit.bNearestHitTieBreakDisabled == Scenario.bDisableNearestHit &&
			OutResult.Audit.bDistanceTieFallbackDisabled == Scenario.bDisableDistanceTieFallback;
		const bool bHashesChanged =
			OutResult.ProjectionHashBefore != OutResult.ProjectionHashAfter ||
			OutResult.StateHashBefore != OutResult.StateHashAfter ||
			OutResult.CrustHashBefore != OutResult.CrustHashAfter;
		const bool bModeApplied = OutResult.LastRemeshMode.StartsWith(TEXT("phase_iii_e6_live")) &&
			!OutResult.LastRemeshMode.Contains(TEXT("_hold_"));
		const bool bModeHeld = OutResult.LastRemeshMode.StartsWith(TEXT("phase_iii_e6_live_hold_unresolved_multi_hit"));
		const bool bModeInvalidHeld =
			OutResult.LastRemeshMode.StartsWith(TEXT("phase_iii_e6_live_hold_invalid_records"));
		const bool bBaselineScenario = Scenario.bDisableNearestHit || Scenario.bDisableDistanceTieFallback;
		if (bBaselineScenario)
		{
			const int32 ExpectedUnresolved = Scenario.bDisableNearestHit
				? ExpectedIIIE64BaselineUnresolved
				: ExpectedIIIE65BaselineUnresolved;
			OutResult.bPass =
				OutResult.bRan &&
				bForbiddenZero &&
				bDisabledMatches &&
				OutResult.CoFireRecordCount == 0 &&
				!OutResult.bApplied &&
				OutResult.EventCountAfter == OutResult.EventCountBefore &&
				!bHashesChanged &&
				(!Scenario.bAttemptApply || bModeHeld) &&
				OutResult.Audit.UnresolvedMultiHitCount == ExpectedUnresolved;
		}
		else if (!Scenario.bAttemptApply)
		{
			OutResult.bPass =
				OutResult.bRan &&
				bForbiddenZero &&
				bDisabledMatches &&
				OutResult.CoFireRecordCount == 0 &&
				!OutResult.bApplied &&
				OutResult.EventCountAfter == OutResult.EventCountBefore &&
				!bHashesChanged &&
				OutResult.Audit.UnresolvedMultiHitCount == 0 &&
				OutResult.Audit.NearestHitDistanceTieHeldCount == 0 &&
				OutResult.Audit.DistanceTieFallbackCount > 0;
		}
		else
		{
			const bool bSelectionResolved =
				OutResult.Audit.UnresolvedMultiHitCount == 0 &&
				OutResult.Audit.NearestHitDistanceTieHeldCount == 0 &&
				OutResult.Audit.DistanceTieFallbackCount > 0;
			const bool bAppliedCleanly =
				OutResult.bApplied &&
				OutResult.EventCountAfter == OutResult.EventCountBefore + 1 &&
				bHashesChanged &&
				bModeApplied &&
				OutResult.InvalidRecordCount == 0;
			const bool bHeldOnNextGate =
				!OutResult.bApplied &&
				OutResult.EventCountAfter == OutResult.EventCountBefore &&
				!bHashesChanged &&
				bModeInvalidHeld &&
				OutResult.InvalidRecordCount > 0;
			OutResult.bPass =
				OutResult.bRan &&
				bForbiddenZero &&
				bDisabledMatches &&
				OutResult.CoFireRecordCount == 0 &&
				bSelectionResolved &&
				(bAppliedCleanly || bHeldOnNextGate);
		}

		Actor->Destroy();
		CollectGarbage(RF_NoFlags);

		UE_LOG(
			LogTemp,
			Display,
			TEXT("CarrierLabPhaseIIIE66DistanceTieFallback: scenario done %s pass=%d unresolved=%d fallback=%d invalid=%d mode=%s seconds=%.3f"),
			*OutResult.Name,
			OutResult.bPass ? 1 : 0,
			OutResult.Audit.UnresolvedMultiHitCount,
			OutResult.Audit.DistanceTieFallbackCount,
			OutResult.InvalidRecordCount,
			*OutResult.LastRemeshMode,
			OutResult.Seconds);
	}

	bool ScenarioReplayMatches(const FScenarioResult& A, const FScenarioResult& B)
	{
		return A.bRan &&
			B.bRan &&
			A.Audit.SelectionHash == B.Audit.SelectionHash &&
			A.ProjectionHashAfter == B.ProjectionHashAfter &&
			A.StateHashAfter == B.StateHashAfter &&
			A.CrustHashAfter == B.CrustHashAfter &&
			A.LastRemeshMode == B.LastRemeshMode;
	}

	FString FixtureJsonLine(const FFixtureResult& F)
	{
		return FString::Printf(
			TEXT("{\"type\":\"fixture\",\"name\":%s,\"pass\":%s,\"resolved\":%s,\"unresolved\":%s,\"plate\":%d,\"bucket\":%s,\"nearest_result\":%s,\"fallback_layer\":%s,\"used_shared_boundary\":%s,\"used_nearest_hit\":%s,\"used_distance_tie_fallback\":%s,\"used_policy_winner\":%s,\"used_prior_owner\":%s}"),
			*JsonString(F.Name),
			*BoolText(F.bPass),
			*BoolText(F.bResolved),
			*BoolText(F.bUnresolvedMultiHit),
			F.ResolvedPlateId,
			*JsonString(BucketName(F.Bucket)),
			*JsonString(NearestResultName(F.NearestResult)),
			*JsonString(FallbackLayerName(F.FallbackLayer)),
			*BoolText(F.bUsedSharedBoundaryTieBreak),
			*BoolText(F.bUsedNearestHitTieBreak),
			*BoolText(F.bUsedDistanceTieFallback),
			*BoolText(F.bUsedPolicyWinner),
			*BoolText(F.bUsedPriorOwnerFallback));
	}

	FString ScenarioJsonLine(const FScenarioResult& S)
	{
		return FString::Printf(
			TEXT("{\"type\":\"scenario\",\"name\":%s,\"pass\":%s,\"automatic\":%s,\"attempt_apply\":%s,\"disable_nearest\":%s,\"disable_distance_tie_fallback\":%s,\"target_step\":%d,\"step_before\":%d,\"step_after\":%d,\"events_before\":%d,\"events_after\":%d,\"unresolved\":%d,\"nearest_cross\":%d,\"nearest_third\":%d,\"nearest_tie_held\":%d,\"fallback_total\":%d,\"fallback_cross\":%d,\"fallback_third\":%d,\"fallback_layer1\":%d,\"fallback_layer2\":%d,\"fallback_layer3\":%d,\"fallback_cross_layers\":\"%d/%d/%d\",\"fallback_third_layers\":\"%d/%d/%d\",\"invalid_records\":%d,\"generated\":%d,\"applied_generated\":%d,\"rifting_pending\":%d,\"no_boundary_pair\":%d,\"nonseparating\":%d,\"co_fire_records\":%d,\"policy_winner\":%d,\"prior_owner\":%d,\"projection_before\":%s,\"projection_after\":%s,\"state_before\":%s,\"state_after\":%s,\"crust_before\":%s,\"crust_after\":%s,\"selection_hash\":%s,\"last_remesh_mode\":%s,\"seconds\":%.6f}"),
			*JsonString(S.Name),
			*BoolText(S.bPass),
			*BoolText(S.bAutomatic),
			*BoolText(S.bAttemptApply),
			*BoolText(S.bDisableNearestHit),
			*BoolText(S.bDisableDistanceTieFallback),
			S.TargetStep,
			S.StepBefore,
			S.StepAfter,
			S.EventCountBefore,
			S.EventCountAfter,
			S.Audit.UnresolvedMultiHitCount,
			S.Audit.NearestHitCrossPlateDifferentResolvedCount,
			S.Audit.NearestHitThirdPlateResolvedCount,
			S.Audit.NearestHitDistanceTieHeldCount,
			S.Audit.DistanceTieFallbackCount,
			S.Audit.DistanceTieFallbackCrossPlateDifferentCount,
			S.Audit.DistanceTieFallbackThirdPlateCount,
			S.Audit.DistanceTieFallbackLayer1WinsCount,
			S.Audit.DistanceTieFallbackLayer2WinsCount,
			S.Audit.DistanceTieFallbackLayer3WinsCount,
			S.Audit.DistanceTieFallbackCrossPlateDifferentLayer1Count,
			S.Audit.DistanceTieFallbackCrossPlateDifferentLayer2Count,
			S.Audit.DistanceTieFallbackCrossPlateDifferentLayer3Count,
			S.Audit.DistanceTieFallbackThirdPlateLayer1Count,
			S.Audit.DistanceTieFallbackThirdPlateLayer2Count,
			S.Audit.DistanceTieFallbackThirdPlateLayer3Count,
			S.InvalidRecordCount,
			S.GeneratedCandidateCount,
			S.AppliedGeneratedCount,
			S.RiftingPendingCount,
			S.NoBoundaryPairMissCount,
			S.NonSeparatingGapFillCount,
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
		const TArray<FScenarioResult>& Scenarios,
		const bool bReplayPass,
		const FString& MetricsPath)
	{
		bool bAllFixturesPass = true;
		for (const FFixtureResult& Fixture : Fixtures)
		{
			bAllFixturesPass = bAllFixturesPass && Fixture.bPass;
		}
		bool bAllScenariosPass = true;
		bool bAnyApplyAttempt = false;
		bool bAnyApplyInvalidHold = false;
		bool bAnyApplySuccess = false;
		for (const FScenarioResult& Scenario : Scenarios)
		{
			bAllScenariosPass = bAllScenariosPass && Scenario.bPass;
			bAnyApplyAttempt = bAnyApplyAttempt || Scenario.bAttemptApply;
			bAnyApplyInvalidHold = bAnyApplyInvalidHold || (Scenario.bAttemptApply && !Scenario.bApplied && Scenario.InvalidRecordCount > 0);
			bAnyApplySuccess = bAnyApplySuccess || (Scenario.bAttemptApply && Scenario.bApplied);
		}
		const bool bAllPass = bAllFixturesPass && bAllScenariosPass && bReplayPass;

		FString Report;
		Report += TEXT("# Phase IIIE.6.6 Distance-Tie Last-Resort Fallback Report\n\n");
		Report += FString::Printf(
			TEXT("**Verdict:** %s%s. IIIE.6.6 resolves only the residual IIIE.6.5 `DistanceTieHeld` records using the approved continental-priority -> older-oceanic-age -> lower-plate-id hierarchy. The selection gate is expected to clear `UnresolvedMultiHitCount == 0`; any later live-apply invalid-record hold is reported separately and is not treated as distance-tie success. This is a named CarrierLab lab policy, not paper-cited thesis behavior.\n\n"),
			bAllPass ? TEXT("PASS") : TEXT("FAIL"),
			bAnyApplyInvalidHold && !bAnyApplySuccess ? TEXT(" / LIVE APPLY HELD BY INVALID GAP RECORDS") : TEXT(""));
		Report += TEXT("## Scope\n\n");
		Report += TEXT("- The fallback fires only when IIIE.6.5 classified a record as `DistanceTieHeld`; disabling IIIE.6.5 preserves the IIIE.6.4 baseline instead of handing resolution to IIIE.6.6.\n");
		Report += TEXT("- The `1e-9 km` tolerance is a micron-scale tie window. It is a numerical-coincidence last resort, not a broad remesh ownership policy.\n");
		Report += TEXT("- Partial-apply remains rejected because carrying old sample state forward would be a prior-owner fallback in IIIE.1 terms.\n");
		if (!bAnyApplyAttempt)
		{
			Report += TEXT("- This commandlet is intentionally selection-only at default scale. The live-apply path is an independent expensive gate and must not be inferred from selection success.\n");
		}
		Report += TEXT("- Metrics JSONL: ");
		Report += MetricsPath;
		Report += TEXT("\n\n");

		Report += TEXT("## Fixture Gates\n\n");
		Report += TEXT("| Fixture | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		for (const FFixtureResult& Fixture : Fixtures)
		{
			Report += FString::Printf(
				TEXT("| %s | %s | resolved `%d`, plate `%d`, bucket `%s`, nearest `%s`, fallback `%s`, flags shared/nearest/dtie `%d/%d/%d`, policy/prior `%d/%d` |\n"),
				*Fixture.Name,
				*PassFail(Fixture.bPass),
				Fixture.bResolved ? 1 : 0,
				Fixture.ResolvedPlateId,
				*BucketName(Fixture.Bucket),
				*NearestResultName(Fixture.NearestResult),
				*FallbackLayerName(Fixture.FallbackLayer),
				Fixture.bUsedSharedBoundaryTieBreak ? 1 : 0,
				Fixture.bUsedNearestHitTieBreak ? 1 : 0,
				Fixture.bUsedDistanceTieFallback ? 1 : 0,
				Fixture.bUsedPolicyWinner ? 1 : 0,
				Fixture.bUsedPriorOwnerFallback ? 1 : 0);
		}
		Report += TEXT("\n");

		Report += TEXT("## Default Scale Scenarios\n\n");
		Report += TEXT("| Scenario | Result | Evidence |\n");
		Report += TEXT("|---|---|---|\n");
		for (const FScenarioResult& Scenario : Scenarios)
		{
			Report += FString::Printf(
				TEXT("| %s | %s | apply `%d`, step `%d->%d`, events `%d->%d`, unresolved `%d`, nearest cross/third/tie `%d/%d/%d`, fallback total/cross/third `%d/%d/%d`, fallback layers `%d/%d/%d`, cross layers `%d/%d/%d`, third layers `%d/%d/%d`, invalid/gen/applied/rift/noBoundary/nonsep `%d/%d/%d/%d/%d/%d`, cofire `%d`, policy/prior `%d/%d`, mode `%s`, hashes proj `%s->%s`, state `%s->%s`, crust `%s->%s`, selection `%s` |\n"),
				*Scenario.Name,
				*PassFail(Scenario.bPass),
				Scenario.bAttemptApply ? 1 : 0,
				Scenario.StepBefore,
				Scenario.StepAfter,
				Scenario.EventCountBefore,
				Scenario.EventCountAfter,
				Scenario.Audit.UnresolvedMultiHitCount,
				Scenario.Audit.NearestHitCrossPlateDifferentResolvedCount,
				Scenario.Audit.NearestHitThirdPlateResolvedCount,
				Scenario.Audit.NearestHitDistanceTieHeldCount,
				Scenario.Audit.DistanceTieFallbackCount,
				Scenario.Audit.DistanceTieFallbackCrossPlateDifferentCount,
				Scenario.Audit.DistanceTieFallbackThirdPlateCount,
				Scenario.Audit.DistanceTieFallbackLayer1WinsCount,
				Scenario.Audit.DistanceTieFallbackLayer2WinsCount,
				Scenario.Audit.DistanceTieFallbackLayer3WinsCount,
				Scenario.Audit.DistanceTieFallbackCrossPlateDifferentLayer1Count,
				Scenario.Audit.DistanceTieFallbackCrossPlateDifferentLayer2Count,
				Scenario.Audit.DistanceTieFallbackCrossPlateDifferentLayer3Count,
				Scenario.Audit.DistanceTieFallbackThirdPlateLayer1Count,
				Scenario.Audit.DistanceTieFallbackThirdPlateLayer2Count,
				Scenario.Audit.DistanceTieFallbackThirdPlateLayer3Count,
				Scenario.InvalidRecordCount,
				Scenario.GeneratedCandidateCount,
				Scenario.AppliedGeneratedCount,
				Scenario.RiftingPendingCount,
				Scenario.NoBoundaryPairMissCount,
				Scenario.NonSeparatingGapFillCount,
				Scenario.CoFireRecordCount,
				Scenario.Audit.PolicyWinnerCount,
				Scenario.Audit.PriorOwnerFallbackCount,
				*Scenario.LastRemeshMode,
				*Scenario.ProjectionHashBefore,
				*Scenario.ProjectionHashAfter,
				*Scenario.StateHashBefore,
				*Scenario.StateHashAfter,
				*Scenario.CrustHashBefore,
				*Scenario.CrustHashAfter,
				*Scenario.Audit.SelectionHash);
		}
		Report += FString::Printf(TEXT("\nSame-seed manual step-32 replay: **%s**.\n\n"), *PassFail(bReplayPass));

		Report += TEXT("## Baseline Preservation\n\n");
		Report += TEXT("- IIIE.6.4 baseline: disabling nearest-hit also prevents IIIE.6.6 from firing because the fallback is gated on IIIE.6.5's `DistanceTieHeld` classification; default step 32 remains at 24,600 holds.\n");
		Report += TEXT("- IIIE.6.5 baseline: enabling nearest-hit but disabling distance-tie fallback preserves the 4 residual holds at default step 32.\n");
		Report += TEXT("- Default IIIE.6.6: all resolver flags enabled, default manual selection reports 0 unresolved multi-hit holds. Live apply remains an independent gate and must not be reported as successful from this selection-only evidence.\n\n");

		Report += TEXT("## Stop Conditions\n\n");
		Report += TEXT("- Stop if `bUsedDistanceTieFallback` appears without `NearestHitResultClass == DistanceTieHeld`.\n");
		Report += TEXT("- Stop if any record co-fires shared-boundary, nearest-hit, and/or distance-tie positive resolver flags.\n");
		Report += TEXT("- Stop if forbidden-policy counters increment.\n");
		Report += TEXT("- Stop if either historical baseline cannot be reproduced with its opt-out flag composition.\n\n");

		Report += TEXT("## Next Slice Boundary\n\n");
		Report += TEXT("Next is a narrow live-apply / divergent-gap record-builder diagnostic if the editor still holds or spends too long inside `ApplyPhaseIIIELiveRemeshEvent`; otherwise IIIE consolidation can rerun and disclose the named IIIE lab policies, verify the live actor visually mutates through the IIIE path without Stage 1.5 fallback, and measure the integrated paper Table 2 cost ratio.\n");
		return Report;
	}
}

UCarrierLabPhaseIIIE66DistanceTieFallbackCommandlet::UCarrierLabPhaseIIIE66DistanceTieFallbackCommandlet()
{
	IsClient = false;
	IsEditor = true;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIE66DistanceTieFallbackCommandlet::Main(const FString& Params)
{
	UWorld* World = GetCommandletWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPhaseIIIE66DistanceTieFallback: no world available."));
		return 1;
	}

	const FString OutputDir = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectSavedDir() / TEXT("CarrierLab/PhaseIII/IIIE66DistanceTieFallback"));
	IFileManager::Get().MakeDirectory(*OutputDir, true);
	const FString MetricsPath = OutputDir / TEXT("metrics.jsonl");

	TArray<FFixtureResult> FixtureResults;
	for (const FFixture& Fixture : MakeFixtures())
	{
		FixtureResults.Add(RunFixture(*World, Fixture));
	}

	const TArray<FScenario> ScenarioSpecs = {
		{ TEXT("manual_step_10_enabled_selection"), 10, false, false, false, false },
		{ TEXT("manual_step_20_enabled_selection"), 20, false, false, false, false },
		{ TEXT("manual_step_32_enabled_selection"), 32, false, false, false, false },
		{ TEXT("manual_step_32_replay_selection"), 32, false, false, false, false },
		{ TEXT("manual_step_60_enabled_selection"), 60, false, false, false, false },
		{ TEXT("iiie6_5_baseline_distance_fallback_disabled"), 32, false, false, false, true },
		{ TEXT("iiie6_4_baseline_nearest_disabled"), 32, false, false, true, true }
	};

	TArray<FScenarioResult> ScenarioResults;
	for (const FScenario& Scenario : ScenarioSpecs)
	{
		FScenarioResult Result;
		RunScenario(*World, Scenario, Result);
		ScenarioResults.Add(Result);
	}

	const FScenarioResult* Manual32 = ScenarioResults.FindByPredicate([](const FScenarioResult& R)
	{
		return R.Name == TEXT("manual_step_32_enabled_selection");
	});
	const FScenarioResult* Manual32Replay = ScenarioResults.FindByPredicate([](const FScenarioResult& R)
	{
		return R.Name == TEXT("manual_step_32_replay_selection");
	});
	const bool bReplayPass = Manual32 != nullptr && Manual32Replay != nullptr && ScenarioReplayMatches(*Manual32, *Manual32Replay);

	FString JsonLines;
	for (const FFixtureResult& Fixture : FixtureResults)
	{
		JsonLines += FixtureJsonLine(Fixture) + LINE_TERMINATOR;
	}
	for (const FScenarioResult& Scenario : ScenarioResults)
	{
		JsonLines += ScenarioJsonLine(Scenario) + LINE_TERMINATOR;
	}
	JsonLines += FString::Printf(
		TEXT("{\"type\":\"same_seed_replay\",\"pass\":%s,\"manual_hash\":%s,\"replay_hash\":%s}"),
		*BoolText(bReplayPass),
		Manual32 != nullptr ? *JsonString(Manual32->Audit.SelectionHash) : TEXT("\"\""),
		Manual32Replay != nullptr ? *JsonString(Manual32Replay->Audit.SelectionHash) : TEXT("\"\""));
	JsonLines += LINE_TERMINATOR;
	FFileHelper::SaveStringToFile(JsonLines, *MetricsPath);

	const FString Report = BuildReport(FixtureResults, ScenarioResults, bReplayPass, MetricsPath);
	const FString ReportPath = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectDir() / TEXT("docs/checkpoints/phase-iii-slice-iiie6-6-distance-tie-fallback-report.md"));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	bool bFixturesPass = true;
	for (const FFixtureResult& Fixture : FixtureResults)
	{
		bFixturesPass = bFixturesPass && Fixture.bPass;
	}
	bool bScenariosPass = true;
	for (const FScenarioResult& Scenario : ScenarioResults)
	{
		bScenariosPass = bScenariosPass && Scenario.bPass;
	}
	const bool bAllPass = bFixturesPass && bScenariosPass && bReplayPass;
	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIIE66DistanceTieFallback %s. Report: %s"), bAllPass ? TEXT("PASS") : TEXT("FAIL"), *ReportPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLabPhaseIIIE66DistanceTieFallback metrics: %s"), *MetricsPath);
	return bAllPass ? 0 : 1;
}
