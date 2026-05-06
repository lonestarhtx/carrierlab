// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIE3Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
	constexpr double ScalarTolerance = 1.0e-9;

	void HashMix(uint64& Hash, const uint64 Value)
	{
		Hash ^= Value;
		Hash *= 1099511628211ull;
	}

	void HashMixDouble(uint64& Hash, const double Value)
	{
		HashMix(Hash, static_cast<uint64>(FMath::RoundToInt64(Value * 1000000000.0)));
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
				TEXT("IIIE3"));
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
				TEXT("phase-iii-slice-iiie3-report.md"));
		}
		else if (FPaths::IsRelative(ReportPath))
		{
			ReportPath = FPaths::Combine(FPaths::ProjectDir(), ReportPath);
		}
		return FPaths::ConvertRelativePathToFull(ReportPath);
	}

	ACarrierLabVisualizationActor* SpawnActor(UWorld& World)
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
		Actor->Seed = 42;
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	FCarrierLabPhaseIIIE3CandidateProbe MakeProbe(
		const int32 PlateId,
		const int32 LocalTriangleId,
		const double ContinentalFraction,
		const double Elevation,
		const double HistoricalElevation,
		const double OceanicAge,
		const ECarrierLabPhaseIIIE3FilterReason FilterReason)
	{
		FCarrierLabPhaseIIIE3CandidateProbe Probe;
		Probe.PlateId = PlateId;
		Probe.LocalTriangleId = LocalTriangleId;
		Probe.Bary = FVector3d(0.20, 0.30, 0.50);
		Probe.ContinentalFraction = ContinentalFraction;
		Probe.Elevation = Elevation;
		Probe.HistoricalElevation = HistoricalElevation;
		Probe.OceanicAge = OceanicAge;
		Probe.RidgeDirection = FVector3d::UnitY();
		Probe.FoldDirection = FVector3d::UnitZ();
		Probe.FilterReason = FilterReason;
		return Probe;
	}

	struct FFixtureSpec
	{
		FString Name;
		FString Purpose;
		FVector3d Sample = FVector3d::UnitX();
		TArray<FCarrierLabPhaseIIIE3CandidateProbe> Candidates;
		ECarrierLabPhaseIIIE3SelectionClass ExpectedClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
		int32 ExpectedResolvedPlateId = INDEX_NONE;
		int32 ExpectedFilteredSubducting = 0;
		int32 ExpectedFilteredObductionPending = 0;
		int32 ExpectedFilteredCollisionPending = 0;
		bool bExpectedResolved = false;
		bool bExpectedDivergentGap = false;
		bool bExpectedUnresolved = false;
		double ExpectedContinentalFraction = 0.0;
		double ExpectedElevation = 0.0;
		double ExpectedHistoricalElevation = 0.0;
		double ExpectedOceanicAge = 0.0;
	};

	struct FFixtureResult
	{
		FString Name;
		FString Purpose;
		bool bQueryReturned = false;
		bool bPass = false;
		double Seconds = 0.0;
		double ContinentalResidual = 0.0;
		double ElevationResidual = 0.0;
		double HistoricalResidual = 0.0;
		double OceanicAgeResidual = 0.0;
		FString RecordHash;
		FCarrierLabPhaseIIIE3SelectionRecord Record;
	};

	FString ComputeRecordHash(const FCarrierLabPhaseIIIE3SelectionRecord& Record)
	{
		uint64 Hash = 1469598103934665603ull;
		HashMix(Hash, static_cast<uint64>(Record.RawCandidateCount + 1));
		HashMix(Hash, static_cast<uint64>(Record.RawPlateCount + 1));
		HashMix(Hash, static_cast<uint64>(Record.FilteredCandidateCount + 1));
		HashMix(Hash, static_cast<uint64>(Record.FilteredSubductingCount + 1));
		HashMix(Hash, static_cast<uint64>(Record.FilteredObductionPendingCount + 1));
		HashMix(Hash, static_cast<uint64>(Record.FilteredCollisionPendingCount + 1));
		HashMix(Hash, static_cast<uint64>(Record.PostFilterCandidateCount + 1));
		HashMix(Hash, static_cast<uint64>(Record.PostFilterPlateCount + 1));
		HashMix(Hash, static_cast<uint64>(Record.SelectionClass) + 1ull);
		HashMix(Hash, static_cast<uint64>(Record.ResolvedPlateId + 2));
		HashMix(Hash, static_cast<uint64>(Record.ResolvedLocalTriangleId + 2));
		HashMixDouble(Hash, Record.ContinentalFraction);
		HashMixDouble(Hash, Record.Elevation);
		HashMixDouble(Hash, Record.HistoricalElevation);
		HashMixDouble(Hash, Record.OceanicAge);
		return HashToString(Hash);
	}

	bool RunFixture(
		ACarrierLabVisualizationActor& Actor,
		const FFixtureSpec& Fixture,
		FFixtureResult& OutResult)
	{
		OutResult = FFixtureResult();
		OutResult.Name = Fixture.Name;
		OutResult.Purpose = Fixture.Purpose;
		const double StartSeconds = FPlatformTime::Seconds();
		OutResult.bQueryReturned = Actor.QueryPhaseIIIE3FilteredRemeshSelectionForTest(Fixture.Sample, Fixture.Candidates, OutResult.Record);
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.RecordHash = ComputeRecordHash(OutResult.Record);

		if (Fixture.bExpectedResolved)
		{
			OutResult.ContinentalResidual = FMath::Abs(OutResult.Record.ContinentalFraction - Fixture.ExpectedContinentalFraction);
			OutResult.ElevationResidual = FMath::Abs(OutResult.Record.Elevation - Fixture.ExpectedElevation);
			OutResult.HistoricalResidual = FMath::Abs(OutResult.Record.HistoricalElevation - Fixture.ExpectedHistoricalElevation);
			OutResult.OceanicAgeResidual = FMath::Abs(OutResult.Record.OceanicAge - Fixture.ExpectedOceanicAge);
		}

		OutResult.bPass =
			OutResult.bQueryReturned &&
			OutResult.Record.SelectionClass == Fixture.ExpectedClass &&
			OutResult.Record.FilteredSubductingCount == Fixture.ExpectedFilteredSubducting &&
			OutResult.Record.FilteredObductionPendingCount == Fixture.ExpectedFilteredObductionPending &&
			OutResult.Record.FilteredCollisionPendingCount == Fixture.ExpectedFilteredCollisionPending &&
			OutResult.Record.bResolvedSingleHit == Fixture.bExpectedResolved &&
			OutResult.Record.bDivergentGapRoute == Fixture.bExpectedDivergentGap &&
			OutResult.Record.bUnresolvedMultiHit == Fixture.bExpectedUnresolved &&
			OutResult.Record.bUsedPolicyWinner == false &&
			OutResult.Record.bUsedPriorOwnerFallback == false;

		if (Fixture.bExpectedResolved)
		{
			OutResult.bPass =
				OutResult.bPass &&
				OutResult.Record.ResolvedPlateId == Fixture.ExpectedResolvedPlateId &&
				OutResult.ContinentalResidual <= ScalarTolerance &&
				OutResult.ElevationResidual <= ScalarTolerance &&
				OutResult.HistoricalResidual <= ScalarTolerance &&
				OutResult.OceanicAgeResidual <= ScalarTolerance;
		}

		return OutResult.bPass;
	}

	FFixtureSpec MakeSubductingInvisibleFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("subducting invisible over-plate transfer");
		Fixture.Purpose = TEXT("true subducting candidates are filtered before source selection and the remaining overriding hit supplies crust fields");
		Fixture.Candidates.Add(MakeProbe(0, 10, 0.0, -4.0, -3.5, 84.0, ECarrierLabPhaseIIIE3FilterReason::Subducting));
		Fixture.Candidates.Add(MakeProbe(1, 20, 1.0, 3.0, 2.5, 0.0, ECarrierLabPhaseIIIE3FilterReason::None));
		Fixture.ExpectedClass = ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit;
		Fixture.ExpectedResolvedPlateId = 1;
		Fixture.ExpectedFilteredSubducting = 1;
		Fixture.bExpectedResolved = true;
		Fixture.ExpectedContinentalFraction = 1.0;
		Fixture.ExpectedElevation = 3.0;
		Fixture.ExpectedHistoricalElevation = 2.5;
		return Fixture;
	}

	FFixtureSpec MakeObductionPendingFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("obduction pending invisible");
		Fixture.Purpose = TEXT("Pre-IIIE.8 obduction-pending marks are filtered distinctly from true subduction marks");
		Fixture.Candidates.Add(MakeProbe(0, 11, 1.0, 1.0, 1.0, 0.0, ECarrierLabPhaseIIIE3FilterReason::ObductionPending));
		Fixture.Candidates.Add(MakeProbe(1, 21, 1.0, 2.0, 2.0, 0.0, ECarrierLabPhaseIIIE3FilterReason::None));
		Fixture.ExpectedClass = ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit;
		Fixture.ExpectedResolvedPlateId = 1;
		Fixture.ExpectedFilteredObductionPending = 1;
		Fixture.bExpectedResolved = true;
		Fixture.ExpectedContinentalFraction = 1.0;
		Fixture.ExpectedElevation = 2.0;
		Fixture.ExpectedHistoricalElevation = 2.0;
		return Fixture;
	}

	FFixtureSpec MakeCollisionPendingFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("collision pending invisible");
		Fixture.Purpose = TEXT("collision-pending candidates have a separate filter reason and do not become source winners");
		Fixture.Candidates.Add(MakeProbe(0, 12, 1.0, 5.0, 5.0, 0.0, ECarrierLabPhaseIIIE3FilterReason::CollisionPending));
		Fixture.Candidates.Add(MakeProbe(1, 22, 0.0, -2.0, -2.0, 12.0, ECarrierLabPhaseIIIE3FilterReason::None));
		Fixture.ExpectedClass = ECarrierLabPhaseIIIE3SelectionClass::ResolvedSingleHit;
		Fixture.ExpectedResolvedPlateId = 1;
		Fixture.ExpectedFilteredCollisionPending = 1;
		Fixture.bExpectedResolved = true;
		Fixture.ExpectedContinentalFraction = 0.0;
		Fixture.ExpectedElevation = -2.0;
		Fixture.ExpectedHistoricalElevation = -2.0;
		Fixture.ExpectedOceanicAge = 12.0;
		return Fixture;
	}

	FFixtureSpec MakeFilterExhaustedGapFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("filter-exhausted divergent gap route");
		Fixture.Purpose = TEXT("zero valid hits after paper filtering routes to IIIE.2 divergent q1/q2 gap fill, not prior owner fallback");
		Fixture.Candidates.Add(MakeProbe(0, 13, 0.0, -3.0, -3.0, 55.0, ECarrierLabPhaseIIIE3FilterReason::Subducting));
		Fixture.Candidates.Add(MakeProbe(1, 23, 1.0, 4.0, 4.0, 0.0, ECarrierLabPhaseIIIE3FilterReason::ObductionPending));
		Fixture.ExpectedClass = ECarrierLabPhaseIIIE3SelectionClass::DivergentGapAfterFiltering;
		Fixture.ExpectedFilteredSubducting = 1;
		Fixture.ExpectedFilteredObductionPending = 1;
		Fixture.bExpectedDivergentGap = true;
		return Fixture;
	}

	FFixtureSpec MakeNoRawHitGapFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("no raw hit divergent gap route");
		Fixture.Purpose = TEXT("zero ray hits routes to IIIE.2 divergent q1/q2 gap fill, not centroid/random/synthetic ownership");
		Fixture.ExpectedClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
		Fixture.bExpectedDivergentGap = true;
		return Fixture;
	}

	FFixtureSpec MakeSameMaterialUnresolvedFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("same-material multi-hit fails loud");
		Fixture.Purpose = TEXT("two same-material valid hits remain unresolved after paper filtering");
		Fixture.Candidates.Add(MakeProbe(0, 14, 1.0, 1.0, 1.0, 0.0, ECarrierLabPhaseIIIE3FilterReason::None));
		Fixture.Candidates.Add(MakeProbe(1, 24, 1.0, 2.0, 2.0, 0.0, ECarrierLabPhaseIIIE3FilterReason::None));
		Fixture.ExpectedClass = ECarrierLabPhaseIIIE3SelectionClass::UnresolvedSameMaterialMultiHit;
		Fixture.bExpectedUnresolved = true;
		return Fixture;
	}

	FFixtureSpec MakeMixedMaterialUnresolvedFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("mixed-material multi-hit fails loud");
		Fixture.Purpose = TEXT("oceanic and continental valid hits remain unresolved after paper filtering");
		Fixture.Candidates.Add(MakeProbe(0, 15, 0.0, -1.0, -1.0, 33.0, ECarrierLabPhaseIIIE3FilterReason::None));
		Fixture.Candidates.Add(MakeProbe(1, 25, 1.0, 3.0, 3.0, 0.0, ECarrierLabPhaseIIIE3FilterReason::None));
		Fixture.ExpectedClass = ECarrierLabPhaseIIIE3SelectionClass::UnresolvedMixedMaterialMultiHit;
		Fixture.bExpectedUnresolved = true;
		return Fixture;
	}

	FFixtureSpec MakeThirdPlateUnresolvedFixture()
	{
		FFixtureSpec Fixture;
		Fixture.Name = TEXT("third-plate multi-hit fails loud");
		Fixture.Purpose = TEXT("three valid plates after filtering are reported separately, not collapsed into a winner");
		Fixture.Candidates.Add(MakeProbe(0, 16, 0.0, -1.0, -1.0, 21.0, ECarrierLabPhaseIIIE3FilterReason::None));
		Fixture.Candidates.Add(MakeProbe(1, 26, 1.0, 2.0, 2.0, 0.0, ECarrierLabPhaseIIIE3FilterReason::None));
		Fixture.Candidates.Add(MakeProbe(2, 36, 0.0, -2.0, -2.0, 44.0, ECarrierLabPhaseIIIE3FilterReason::None));
		Fixture.ExpectedClass = ECarrierLabPhaseIIIE3SelectionClass::UnresolvedThirdPlateMultiHit;
		Fixture.bExpectedUnresolved = true;
		return Fixture;
	}

	struct FRaySmokeResult
	{
		bool bPass = false;
		bool bRunA = false;
		bool bRunB = false;
		FString HashA;
		FString HashB;
		FCarrierLabPhaseIIIE3RemeshSelectionAudit AuditA;
		FCarrierLabPhaseIIIE3RemeshSelectionAudit AuditB;
	};

	bool RunRaySmoke(ACarrierLabVisualizationActor& Actor, FRaySmokeResult& OutResult)
	{
		OutResult = FRaySmokeResult();
		Actor.SampleCount = 512;
		Actor.PlateCount = 4;
		Actor.ContinentalPlateFraction = 0.50;
		if (!Actor.InitializeCarrier())
		{
			return false;
		}

		TArray<int32> SampleIds;
		for (int32 SampleId = 0; SampleId < 64; ++SampleId)
		{
			SampleIds.Add(SampleId);
		}

		OutResult.bRunA = Actor.RunPhaseIIIE3FilteredRemeshSelectionAuditForSamples(SampleIds, OutResult.AuditA);
		OutResult.bRunB = Actor.RunPhaseIIIE3FilteredRemeshSelectionAuditForSamples(SampleIds, OutResult.AuditB);
		OutResult.HashA = OutResult.AuditA.SelectionHash;
		OutResult.HashB = OutResult.AuditB.SelectionHash;
		OutResult.bPass =
			OutResult.bRunA &&
			OutResult.bRunB &&
			OutResult.AuditA.bRan &&
			OutResult.AuditB.bRan &&
			OutResult.AuditA.SampleCount == SampleIds.Num() &&
			OutResult.AuditB.SampleCount == SampleIds.Num() &&
			OutResult.AuditA.PolicyWinnerCount == 0 &&
			OutResult.AuditA.PriorOwnerFallbackCount == 0 &&
			OutResult.HashA == OutResult.HashB;
		return OutResult.bPass;
	}

	FString BuildJsonLine(const FFixtureResult& Result)
	{
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"purpose\":%s,\"pass\":%s,\"selection_class\":%s,\"raw_candidates\":%d,\"raw_plates\":%d,\"filtered_subducting\":%d,\"filtered_obduction_pending\":%d,\"filtered_collision_pending\":%d,\"post_filter_candidates\":%d,\"post_filter_plates\":%d,\"resolved_plate\":%d,\"divergent_gap\":%s,\"unresolved_multi_hit\":%s,\"policy_winner\":%s,\"prior_owner_fallback\":%s,\"elevation_residual\":%.12g,\"record_hash\":%s}"),
			*JsonString(Result.Name),
			*JsonString(Result.Purpose),
			Result.bPass ? TEXT("true") : TEXT("false"),
			*JsonString(SelectionClassName(Result.Record.SelectionClass)),
			Result.Record.RawCandidateCount,
			Result.Record.RawPlateCount,
			Result.Record.FilteredSubductingCount,
			Result.Record.FilteredObductionPendingCount,
			Result.Record.FilteredCollisionPendingCount,
			Result.Record.PostFilterCandidateCount,
			Result.Record.PostFilterPlateCount,
			Result.Record.ResolvedPlateId,
			Result.Record.bDivergentGapRoute ? TEXT("true") : TEXT("false"),
			Result.Record.bUnresolvedMultiHit ? TEXT("true") : TEXT("false"),
			Result.Record.bUsedPolicyWinner ? TEXT("true") : TEXT("false"),
			Result.Record.bUsedPriorOwnerFallback ? TEXT("true") : TEXT("false"),
			Result.ElevationResidual,
			*JsonString(Result.RecordHash));
	}

	FString BuildReport(
		const TArray<FFixtureResult>& Results,
		const bool bReplayPass,
		const FString& ReplayHashA,
		const FString& ReplayHashB,
		const FRaySmokeResult& RaySmoke,
		const FString& MetricsPath)
	{
		bool bFixturePass = true;
		for (const FFixtureResult& Result : Results)
		{
			bFixturePass = bFixturePass && Result.bPass;
		}
		const bool bAllPass = bFixturePass && bReplayPass && RaySmoke.bPass;

		FString Report;
		Report += TEXT("# Phase IIIE.3 Filtered Remesh Candidate Selection\n\n");
		Report += TEXT("Verdict: ");
		Report += bAllPass ? TEXT("PASS / IIIE.4 UNBLOCKED") : TEXT("FAIL / HOLD IIIE.4");
		Report += TEXT(". This slice implements the Phase IIIE remesh source selector and audit gates against the paper contract only. It does not mutate global samples, rebuild topology, reset process state, generate new oceanic fields, optimize replay, or promote Stage 1.5 lab policy.\n\n");

		Report += TEXT("## Scope\n\n");
		Report += TEXT("- The current-state audit casts from the planet center through global TDS sample positions against plate-local BVHs.\n");
		Report += TEXT("- True subducting marks, Pre-IIIE.8 obduction-pending marks, and explicit collision-pending candidates are invisible before source selection.\n");
		Report += TEXT("- Exactly one remaining hit is the only barycentric transfer path. Zero remaining hits route to the IIIE.2 q1/q2/qGamma divergent gap path. Multiple remaining hits are reported as unresolved same-material, mixed-material, or third-plate anomalies.\n");
		Report += TEXT("- Prior-owner fallback, centroid/random/synthetic winners, recovery, repair, retention, hysteresis, and anchoring remain forbidden in this primary IIIE path.\n\n");

		Report += TEXT("## Gates\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---:|---|\n");
		for (const FFixtureResult& Result : Results)
		{
			Report += FString::Printf(
				TEXT("| %s | %s | class `%s`, raw `%d`, visible `%d`, filters sub/obd/coll `%d/%d/%d`, resolved plate `%d`, policy/prior `%d/%d`, hash `%s`. |\n"),
				*Result.Name,
				*PassFail(Result.bPass),
				*SelectionClassName(Result.Record.SelectionClass),
				Result.Record.RawCandidateCount,
				Result.Record.PostFilterCandidateCount,
				Result.Record.FilteredSubductingCount,
				Result.Record.FilteredObductionPendingCount,
				Result.Record.FilteredCollisionPendingCount,
				Result.Record.ResolvedPlateId,
				Result.Record.bUsedPolicyWinner ? 1 : 0,
				Result.Record.bUsedPriorOwnerFallback ? 1 : 0,
				*Result.RecordHash);
		}
		Report += FString::Printf(
			TEXT("| Same-seed filtered-selection replay | %s | Replay hashes `%s` and `%s`. |\n"),
			*PassFail(bReplayPass),
			*ReplayHashA,
			*ReplayHashB);
		Report += FString::Printf(
			TEXT("| Current-state plate-local ray smoke | %s | samples `%d`, raw hits `%d`, gaps `%d`, unresolved `%d`, policy/prior `%d/%d`, hashes `%s` / `%s`. |\n"),
			*PassFail(RaySmoke.bPass),
			RaySmoke.AuditA.SampleCount,
			RaySmoke.AuditA.RawHitSampleCount,
			RaySmoke.AuditA.DivergentGapRouteCount,
			RaySmoke.AuditA.UnresolvedMultiHitCount,
			RaySmoke.AuditA.PolicyWinnerCount,
			RaySmoke.AuditA.PriorOwnerFallbackCount,
			*RaySmoke.HashA,
			*RaySmoke.HashB);

		Report += TEXT("\n## Inherited Regression Note\n\n");
		Report += TEXT("IIIE.3 consumes convergence-side process state, so the checkpoint closeout must also cite the inherited IIIB independent-signature regression. The IIIE.3 commandlet does not duplicate that larger harness; run the existing IIID7 regression commandlet and require computed signatures `bf8818a26ed7b1dc` / `bf8818a26ed7b1dc` against expected `bf8818a26ed7b1dc` before committing the subphase.\n\n");

		Report += TEXT("\n## Contract Table\n\n");
		Report += TEXT("| Paper requirement | CarrierLab support now | IIIE obligation still ahead | Gate needed |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += TEXT("| Center ray from planet center through each global TDS vertex | IIIE.3 current-state audit uses plate-local BVH ray candidates | Wire into the actual remesh event in IIIE.5 | End-to-end remesh event fixture |\n");
		Report += TEXT("| Ignore subducting and colliding/obducting-in-process triangles before source selection | Selector filters true subducting, obduction-pending, and collision-pending reasons separately | Map any future persistent collision-pending carrier state into the same reason before remesh | Per-reason invisibility counters remain nonzero in fixtures |\n");
		Report += TEXT("| Single valid hit interpolates crust fields barycentrically | Single visible candidate copies the interpolated/probed fields and source plate id | Mutate global samples only when remesh event is implemented | Independent scalar/vector field residuals |\n");
		Report += TEXT("| Zero valid hits become divergent gap fill | No-hit and filter-exhausted samples route to IIIE.2 q1/q2/qGamma | IIIE.4 creates oceanic fields from that provenance | Gap-fill q1/q2 field oracle |\n");
		Report += TEXT("| Multiple valid hits after filtering are not silently resolved | Same-material, mixed-material, and third-plate unresolved classes fail loud | Decide only with paper citation or explicit lab policy | Unresolved counts block primary remesh |\n\n");

		Report += TEXT("## Stop Conditions For IIIE.4+\n\n");
		Report += TEXT("- Stop if any primary remesh path uses previous sample owner, previous continental fraction, centroid/random/synthetic winner policy, or recovery/backfill/retention/hysteresis/anchoring.\n");
		Report += TEXT("- Stop if obduction-pending marks are collapsed into true subduction marks or left visible during remesh source selection.\n");
		Report += TEXT("- Stop if unresolved multi-hit samples are converted into winners without a paper-cited rule or explicit approved lab policy.\n");
		Report += TEXT("- Stop if zero-hit/filter-exhausted samples bypass the IIIE.2 continuous q1/q2/qGamma route.\n");
		Report += TEXT("- Stop if actual remesh mutation is added before topology rebuild and process-state reset gates are specified.\n\n");

		Report += TEXT("## Next Slice Boundary\n\n");
		Report += TEXT("IIIE.4 should implement divergent oceanic field generation from the IIIE.2 provenance route for samples that IIIE.3 classifies as no-hit or filter-exhausted gaps. It should not rebuild topology or reset process state yet unless the slice boundary is explicitly revised.\n\n");

		Report += FString::Printf(TEXT("Metrics: `%s`.\n"), *MetricsPath);
		return Report;
	}
}

UCarrierLabPhaseIIIE3Commandlet::UCarrierLabPhaseIIIE3Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIE3Commandlet::Main(const FString& Params)
{
	UWorld* World = GetCommandletWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPhaseIIIE3Commandlet could not find a commandlet world."));
		return 1;
	}

	ACarrierLabVisualizationActor* Actor = SpawnActor(*World);
	if (Actor == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPhaseIIIE3Commandlet could not spawn CarrierLabVisualizationActor."));
		return 1;
	}

	TArray<FFixtureSpec> Fixtures;
	Fixtures.Add(MakeSubductingInvisibleFixture());
	Fixtures.Add(MakeObductionPendingFixture());
	Fixtures.Add(MakeCollisionPendingFixture());
	Fixtures.Add(MakeFilterExhaustedGapFixture());
	Fixtures.Add(MakeNoRawHitGapFixture());
	Fixtures.Add(MakeSameMaterialUnresolvedFixture());
	Fixtures.Add(MakeMixedMaterialUnresolvedFixture());
	Fixtures.Add(MakeThirdPlateUnresolvedFixture());

	TArray<FFixtureResult> Results;
	Results.Reserve(Fixtures.Num());
	for (const FFixtureSpec& Fixture : Fixtures)
	{
		FFixtureResult Result;
		RunFixture(*Actor, Fixture, Result);
		Results.Add(Result);
	}

	FFixtureResult ReplayA;
	FFixtureResult ReplayB;
	RunFixture(*Actor, MakeSubductingInvisibleFixture(), ReplayA);
	RunFixture(*Actor, MakeSubductingInvisibleFixture(), ReplayB);
	const bool bReplayPass = ReplayA.bPass && ReplayB.bPass && ReplayA.RecordHash == ReplayB.RecordHash;

	FRaySmokeResult RaySmoke;
	RunRaySmoke(*Actor, RaySmoke);

	Actor->Destroy();
	CollectGarbage(RF_NoFlags);

	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("phase-iii-slice-iiie3-metrics.jsonl"));
	FString JsonLines;
	for (const FFixtureResult& Result : Results)
	{
		JsonLines += BuildJsonLine(Result) + LINE_TERMINATOR;
	}
	JsonLines += FString::Printf(
		TEXT("{\"fixture\":\"same_seed_filtered_selection_replay\",\"pass\":%s,\"hash_a\":%s,\"hash_b\":%s}"),
		bReplayPass ? TEXT("true") : TEXT("false"),
		*JsonString(ReplayA.RecordHash),
		*JsonString(ReplayB.RecordHash)) + LINE_TERMINATOR;
	JsonLines += FString::Printf(
		TEXT("{\"fixture\":\"current_state_plate_local_ray_smoke\",\"pass\":%s,\"sample_count\":%d,\"raw_hits\":%d,\"gaps\":%d,\"unresolved\":%d,\"policy_winner\":%d,\"prior_owner_fallback\":%d,\"hash_a\":%s,\"hash_b\":%s}"),
		RaySmoke.bPass ? TEXT("true") : TEXT("false"),
		RaySmoke.AuditA.SampleCount,
		RaySmoke.AuditA.RawHitSampleCount,
		RaySmoke.AuditA.DivergentGapRouteCount,
		RaySmoke.AuditA.UnresolvedMultiHitCount,
		RaySmoke.AuditA.PolicyWinnerCount,
		RaySmoke.AuditA.PriorOwnerFallbackCount,
		*JsonString(RaySmoke.HashA),
		*JsonString(RaySmoke.HashB)) + LINE_TERMINATOR;
	FFileHelper::SaveStringToFile(JsonLines, *MetricsPath);

	const FString ReportPath = GetReportPath(Params);
	const FString Report = BuildReport(Results, bReplayPass, ReplayA.RecordHash, ReplayB.RecordHash, RaySmoke, MetricsPath);
	FFileHelper::SaveStringToFile(Report, *ReportPath);

	bool bFixturePass = true;
	for (const FFixtureResult& Result : Results)
	{
		bFixturePass = bFixturePass && Result.bPass;
	}
	if (!(bFixturePass && bReplayPass && RaySmoke.bPass))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab Phase IIIE.3 gates failed. Report: %s"), *ReportPath);
		return 1;
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIE.3 gates passed."));
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIE.3 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase IIIE.3 report: %s"), *ReportPath);
	return 0;
}
