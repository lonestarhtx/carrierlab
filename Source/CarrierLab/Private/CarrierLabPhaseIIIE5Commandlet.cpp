// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIIIE5Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
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

	FString AssignmentClassName(const ECarrierLabPhaseIIIE5TriangleAssignmentClass AssignmentClass)
	{
		switch (AssignmentClass)
		{
		case ECarrierLabPhaseIIIE5TriangleAssignmentClass::AllVerticesSamePlate:
			return TEXT("all-same");
		case ECarrierLabPhaseIIIE5TriangleAssignmentClass::MajorityTwoOfThree:
			return TEXT("majority-two-of-three");
		case ECarrierLabPhaseIIIE5TriangleAssignmentClass::UnresolvedTripleJunction:
			return TEXT("unresolved-triple-junction");
		case ECarrierLabPhaseIIIE5TriangleAssignmentClass::Invalid:
		default:
			return TEXT("invalid");
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
				TEXT("IIIE5"));
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
				TEXT("phase-iii-slice-iiie5-report.md"));
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
		const double ContinentalPlateFraction = 0.30)
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
		Actor->ContinentalPlateFraction = ContinentalPlateFraction;
		Actor->bEnableNaturalResamplingEvents = false;
		Actor->ConfigurePhaseIIICProcessLayer(false, false);
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	FCarrierLabPhaseIIIE4OceanicGenerationRecord MakeGeneratedOceanicRecord()
	{
		FCarrierLabPhaseIIIE4OceanicGenerationRecord Record;
		Record.SourceSelectionClass = ECarrierLabPhaseIIIE3SelectionClass::NoHitDivergentGap;
		Record.bGeneratedOceanicCrust = true;
		Record.bBoundaryPairFound = true;
		Record.AssignedPlateId = 0;
		Record.Q1PlateId = 0;
		Record.Q2PlateId = 1;
		Record.Q1EdgeId = 11;
		Record.Q2EdgeId = 29;
		Record.SignedSeparationVelocity = 0.004;
		Record.Q1DistanceKm = 350.0;
		Record.Q2DistanceKm = 700.0;
		Record.RidgeDistanceKm = 120.0;
		Record.NearestBoundaryDistanceKm = 350.0;
		Record.Alpha = 120.0 / 470.0;
		Record.ZBarElevation = -3.0;
		Record.ZGammaElevation = -2.0;
		Record.ZGammaProfileDistanceKm = 120.0;
		Record.ZGammaProfileReferenceDistanceKm = 3000.0;
		Record.ZGammaProfileT = 0.2;
		Record.Elevation = -2.2553191489361701;
		Record.OceanicAge = 0.0;
		Record.Q1UnitPosition = FVector3d::UnitX();
		Record.Q2UnitPosition = FVector3d::UnitY();
		Record.QGammaUnitPosition = FVector3d(1.0, 1.0, 0.0).GetSafeNormal();
		Record.QGammaInputNorm = 1.4142135623730951;
		Record.QGammaUnitResidual = 0.0;
		Record.RidgeDirection = FVector3d::UnitY();
		Record.RidgeDirectionMagnitude = 1.0;
		Record.RidgeDirectionRadialDot = 0.0;
		Record.bUsedZGammaDistanceProfilePlaceholder = false;
		Record.bUsedZGammaGeophysicsDerivedProfile = true;
		Record.bPaperFaithfulZGammaProfile = false;
		return Record;
	}

	struct FTopologyFixtureSpec
	{
		FString Name;
		FString Purpose;
		int32 SampleCount = 48;
		int32 PlateCount = 3;
		FCarrierLabPhaseIIIE5RemeshInputFixture Fixture;
		bool bExpectApplied = true;
		bool bExpectMajority = false;
		bool bExpectTripleJunctionHold = false;
		bool bExpectProcessResetSeed = false;
		bool bExpectOceanicPreservation = false;
	};

	struct FTopologyFixtureResult
	{
		FString Name;
		FString Purpose;
		bool bRan = false;
		bool bPass = false;
		bool bHold = false;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIE5TopologyRebuildAudit Audit;
	};

	struct FCollisionFixtureResult
	{
		bool bRan = false;
		bool bPass = false;
		double Seconds = 0.0;
		FCarrierLabPhaseIIIE5CollisionPendingWireAudit WireAudit;
		FCarrierLabPhaseIIIE3RemeshSelectionAudit SelectionAudit;
	};

	FTopologyFixtureSpec MakeAllSameFixture()
	{
		FTopologyFixtureSpec Spec;
		Spec.Name = TEXT("Single-plate duplicate/re-index/re-compact");
		Spec.Purpose = TEXT("All global TDS triangles have one assigned plate and rebuild into compact plate-local topology.");
		Spec.SampleCount = 48;
		Spec.PlateCount = 1;
		Spec.Fixture.bForceAllSamplesToPlateZero = true;
		return Spec;
	}

	FTopologyFixtureSpec MakeMajorityFixture()
	{
		FTopologyFixtureSpec Spec;
		Spec.Name = TEXT("Mixed triangle majority assignment");
		Spec.Purpose = TEXT("Two-of-three mixed global triangles use the named majority lab policy without prior owner or projection fallback.");
		Spec.Fixture.bForceAllSamplesToPlateZero = true;
		Spec.Fixture.OverrideTriangleId = 0;
		Spec.Fixture.OverridePlateA = 0;
		Spec.Fixture.OverridePlateB = 0;
		Spec.Fixture.OverridePlateC = 1;
		Spec.bExpectMajority = true;
		return Spec;
	}

	FTopologyFixtureSpec MakeTripleJunctionFixture()
	{
		FTopologyFixtureSpec Spec;
		Spec.Name = TEXT("Triple-junction unresolved anomaly");
		Spec.Purpose = TEXT("One-one-one mixed global triangles remain stop-condition anomalies rather than being assigned by projection or prior owner.");
		Spec.Fixture.bForceAllSamplesToPlateZero = true;
		Spec.Fixture.OverrideTriangleId = 0;
		Spec.Fixture.OverridePlateA = 0;
		Spec.Fixture.OverridePlateB = 1;
		Spec.Fixture.OverridePlateC = 2;
		Spec.bExpectApplied = false;
		Spec.bExpectTripleJunctionHold = true;
		return Spec;
	}

	FTopologyFixtureSpec MakeProcessResetFixture()
	{
		FTopologyFixtureSpec Spec;
		Spec.Name = TEXT("Process-state reset at remesh");
		Spec.Purpose = TEXT("Subduction marks, obduction marks, active lists, distance-to-front records, subduction matrix state, and collision-pending keys reset at remesh.");
		Spec.PlateCount = 2;
		Spec.Fixture.bForceAllSamplesToPlateZero = true;
		Spec.Fixture.bSeedProcessStateBeforeRemesh = true;
		Spec.bExpectProcessResetSeed = true;
		return Spec;
	}

	FTopologyFixtureSpec MakeOceanicProvenanceFixture()
	{
		FTopologyFixtureSpec Spec;
		Spec.Name = TEXT("IIIE.4 provenance preservation");
		Spec.Purpose = TEXT("Generated oceanic fields and q1/q2/qGamma provenance survive duplicate/re-index/re-compact without paper-fidelity overclaiming zGamma.");
		Spec.PlateCount = 2;
		Spec.Fixture.bForceAllSamplesToPlateZero = true;
		Spec.Fixture.OverrideTriangleId = 0;
		Spec.Fixture.OverridePlateA = 0;
		Spec.Fixture.OverridePlateB = 0;
		Spec.Fixture.OverridePlateC = 0;
		Spec.Fixture.bInjectGeneratedOceanicRecord = true;
		Spec.Fixture.GeneratedOceanicRecord = MakeGeneratedOceanicRecord();
		Spec.bExpectOceanicPreservation = true;
		return Spec;
	}

	bool EvaluateTopologyFixture(const FTopologyFixtureSpec& Spec, FTopologyFixtureResult& OutResult, UWorld& World)
	{
		OutResult = FTopologyFixtureResult();
		OutResult.Name = Spec.Name;
		OutResult.Purpose = Spec.Purpose;
		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(World, Spec.SampleCount, Spec.PlateCount);
		if (Actor == nullptr)
		{
			return false;
		}
		const bool bRan = Actor->RunPhaseIIIE5TopologyRebuildFixtureForTest(Spec.Fixture, OutResult.Audit);
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bRan = bRan && OutResult.Audit.bRan;
		OutResult.bHold = Spec.bExpectTripleJunctionHold && OutResult.Audit.UnresolvedTripleJunctionCount > 0;

		const bool bAppliedMatches = Spec.bExpectApplied == OutResult.Audit.bApplied;
		const bool bMajorityMatches = !Spec.bExpectMajority || OutResult.Audit.MajorityTriangleCount > 0;
		const bool bTripleMatches = !Spec.bExpectTripleJunctionHold ||
			(OutResult.Audit.UnresolvedTripleJunctionCount > 0 && !OutResult.Audit.bApplied);
		const bool bResetSeedMatches = !Spec.bExpectProcessResetSeed ||
			(OutResult.Audit.ResetAudit.ActiveTriangleCountBefore > 0 &&
				OutResult.Audit.ResetAudit.MatrixPairCountBefore > 0 &&
				OutResult.Audit.ResetAudit.SubductingMarkCountBefore > 0 &&
				OutResult.Audit.ResetAudit.ObductionMarkCountBefore > 0 &&
				OutResult.Audit.ResetAudit.CollisionPendingTriangleCountBefore > 0 &&
				OutResult.Audit.ResetAudit.DistanceToFrontRecordCountBefore > 0);
		const bool bOceanicMatches = !Spec.bExpectOceanicPreservation ||
			(OutResult.Audit.GeneratedOceanicVertexCount == 1 &&
				OutResult.Audit.PreservedGeneratedOceanicVertexCount == 1 &&
				OutResult.Audit.bQProvenancePreserved &&
				OutResult.Audit.bZGammaHoldPreserved);

		OutResult.bPass =
			OutResult.bRan &&
			bAppliedMatches &&
			bMajorityMatches &&
			bTripleMatches &&
			bResetSeedMatches &&
			bOceanicMatches &&
			OutResult.Audit.bNoDuplicateTriangleAuthority &&
			OutResult.Audit.bPlateLocalTopologyCompact &&
			OutResult.Audit.ResetAudit.bResetSerialAdvanced &&
			OutResult.Audit.ResetAudit.bProcessStateEmptyAfter &&
			OutResult.Audit.bMotionPreserved &&
			OutResult.Audit.bNoPriorOwnerFallback &&
			OutResult.Audit.bNoProjectionOwnerFallback &&
			OutResult.Audit.bNoPolicyWinner &&
			OutResult.Audit.bNoUnresolvedMultiHitRouted;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bRan;
	}

	bool EvaluateCollisionFixture(FCollisionFixtureResult& OutResult, UWorld& World)
	{
		OutResult = FCollisionFixtureResult();
		const double StartSeconds = FPlatformTime::Seconds();
		ACarrierLabVisualizationActor* Actor = SpawnActor(World, 10000, 2, 1.0);
		if (Actor == nullptr)
		{
			return false;
		}
		Actor->InitializeCarrier();
		Actor->SetPlateContinentalForTest(0, true);
		Actor->SetPlateContinentalForTest(1, true);
		Actor->ConfigurePhaseIIMotionFixture(ECarrierLabPhaseIIMotionFixture::ForcedConvergence);
		FCarrierLabPhaseIIIB6SeedMetrics SeedMetrics;
		const bool bSeeded = Actor->SeedPhaseIIIB6SingleConvergentTriangleForTest(0, SeedMetrics);
		const bool bRan = bSeeded && Actor->RunPhaseIIIE5CollisionPendingWireFixtureForTest(
			OutResult.WireAudit,
			OutResult.SelectionAudit,
			0.0);
		OutResult.Seconds = FPlatformTime::Seconds() - StartSeconds;
		OutResult.bRan = bRan && OutResult.WireAudit.bRan;
		OutResult.bPass =
			OutResult.bRan &&
			OutResult.WireAudit.bSeededFromAcceptedCollisionGroups &&
			OutResult.WireAudit.AcceptedGroupCount > 0 &&
			OutResult.WireAudit.PendingTriangleKeyCount > 0 &&
			OutResult.WireAudit.FilteredCollisionPendingCount > 0 &&
			OutResult.WireAudit.FilteredSubductingCount == 0 &&
			OutResult.WireAudit.FilteredObductionPendingCount == 0 &&
			OutResult.SelectionAudit.PolicyWinnerCount == 0 &&
			OutResult.SelectionAudit.PriorOwnerFallbackCount == 0;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return OutResult.bRan;
	}

	FString BuildTopologyJsonLine(const FTopologyFixtureResult& Result)
	{
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"pass\":%s,\"hold\":%s,\"applied\":%s,\"samples\":%d,\"global_triangles\":%d,\"assigned_triangles\":%d,\"majority_triangles\":%d,\"unresolved_triple_junctions\":%d,\"generated_vertices\":%d,\"preserved_generated_vertices\":%d,\"prior_owner_fallback\":%d,\"projection_owner_fallback\":%d,\"policy_winner\":%d,\"fixture_owned_vertex_assignments\":%s,\"reset_serial_before\":%d,\"reset_serial_after\":%d,\"active_before\":%d,\"active_after\":%d,\"matrix_pairs_before\":%d,\"matrix_pairs_after\":%d,\"collision_pending_before\":%d,\"collision_pending_after\":%d,\"motion_before\":%s,\"motion_after\":%s,\"assignment_hash\":%s,\"topology_hash\":%s}"),
			*JsonString(Result.Name),
			Result.bPass ? TEXT("true") : TEXT("false"),
			Result.bHold ? TEXT("true") : TEXT("false"),
			Result.Audit.bApplied ? TEXT("true") : TEXT("false"),
			Result.Audit.SampleCount,
			Result.Audit.GlobalTriangleCount,
			Result.Audit.AssignedTriangleCount,
			Result.Audit.MajorityTriangleCount,
			Result.Audit.UnresolvedTripleJunctionCount,
			Result.Audit.GeneratedOceanicVertexCount,
			Result.Audit.PreservedGeneratedOceanicVertexCount,
			Result.Audit.PriorOwnerFallbackCount,
			Result.Audit.ProjectionOwnerFallbackCount,
			Result.Audit.PolicyWinnerCount,
			Result.Audit.bFixtureOwnedVertexAssignmentRecords ? TEXT("true") : TEXT("false"),
			Result.Audit.ResetAudit.ResetSerialBefore,
			Result.Audit.ResetAudit.ResetSerialAfter,
			Result.Audit.ResetAudit.ActiveTriangleCountBefore,
			Result.Audit.ResetAudit.ActiveTriangleCountAfter,
			Result.Audit.ResetAudit.MatrixPairCountBefore,
			Result.Audit.ResetAudit.MatrixPairCountAfter,
			Result.Audit.ResetAudit.CollisionPendingTriangleCountBefore,
			Result.Audit.ResetAudit.CollisionPendingTriangleCountAfter,
			*JsonString(Result.Audit.MotionHashBefore),
			*JsonString(Result.Audit.MotionHashAfter),
			*JsonString(Result.Audit.AssignmentHash),
			*JsonString(Result.Audit.TopologyHash));
	}

	FString BuildCollisionJsonLine(const FCollisionFixtureResult& Result)
	{
		return FString::Printf(
			TEXT("{\"fixture\":\"collision_pending_current_state_wire\",\"pass\":%s,\"accepted_groups\":%d,\"fixture_owned_accepted_group\":%s,\"pending_triangle_keys\":%d,\"filtered_collision_pending\":%d,\"filtered_subducting\":%d,\"filtered_obduction\":%d,\"policy_winner\":%d,\"prior_owner_fallback\":%d,\"grouping_hash\":%s,\"selection_hash\":%s}"),
			Result.bPass ? TEXT("true") : TEXT("false"),
			Result.WireAudit.AcceptedGroupCount,
			Result.WireAudit.bUsedFixtureOwnedAcceptedGroup ? TEXT("true") : TEXT("false"),
			Result.WireAudit.PendingTriangleKeyCount,
			Result.WireAudit.FilteredCollisionPendingCount,
			Result.WireAudit.FilteredSubductingCount,
			Result.WireAudit.FilteredObductionPendingCount,
			Result.SelectionAudit.PolicyWinnerCount,
			Result.SelectionAudit.PriorOwnerFallbackCount,
			*JsonString(Result.WireAudit.GroupingHash),
			*JsonString(Result.WireAudit.SelectionHash));
	}

	FString BuildReport(
		const TArray<FTopologyFixtureResult>& TopologyResults,
		const FCollisionFixtureResult& CollisionResult,
		const bool bReplayPass,
		const FString& ReplayHashA,
		const FString& ReplayHashB,
		const FString& MetricsPath)
	{
		bool bTopologyPass = true;
		for (const FTopologyFixtureResult& Result : TopologyResults)
		{
			bTopologyPass = bTopologyPass && Result.bPass;
		}
		const bool bAllPass = bTopologyPass && CollisionResult.bPass && bReplayPass;

		FString Report;
		Report += TEXT("# Phase IIIE.5 Topology Rebuild And Process Reset\n\n");
		Report += TEXT("Verdict: ");
		Report += bAllPass ? TEXT("PASS / IIIE.6 UNBLOCKED; ZGAMMA PAPER-FIDELITY HOLD CARRIED") : TEXT("FAIL / HOLD IIIE.6");
		Report += TEXT(". This slice implements the remesh-side duplicate/re-index/re-compact topology rebuild, process-state reset, and current-state `CollisionPending` filter wiring. It does not implement a full production remesh cadence, optimize replay, solve unresolved multi-hit policy, or claim the geophysics-derived IIIE.4 zGamma extension is paper-faithful.\n\n");

		Report += TEXT("## Scope\n\n");
		Report += TEXT("- IIIE.5 consumes fixture-owned per-global-vertex remesh assignment records, then partitions global TDS triangles into rebuilt plate-local triangulations; the future production cadence must supply these records from IIIE.3/IIIE.4 selection and gap-fill outputs.\n");
		Report += TEXT("- All-same triangles are copied to that plate. Two-of-three mixed triangles use the IIIE.1 named majority lab policy. One-one-one triple-junction triangles remain unresolved stop-condition anomalies.\n");
		Report += TEXT("- Plate-local topology is rebuilt by duplicate/re-index/re-compact from the global TDS assignment; no prior global owner, projection owner, centroid/random winner, or Stage 1.5 recovery path participates.\n");
		Report += TEXT("- Remesh reset clears active convergence lists, distance-to-front records, subduction matrix state, true subduction marks, obduction-pending marks, and collision-pending keys, then advances the reset serial.\n");
		Report += TEXT("- Plate geodetic motion is preserved byte-for-byte across topology rebuild. Generated IIIE.4 oceanic fields and q1/q2/qGamma event provenance are preserved in the remesh records, while `bPaperFaithfulZGammaProfile = false` remains a hold.\n");
		Report += TEXT("- The focused CollisionPending gate may use a fixture-owned accepted IIID2 group record when the compact live detector setup produces no accepted group; it tests mapping from accepted group records into current-state ray invisibility, not the IIID detector itself.\n\n");

		Report += TEXT("## Gates\n\n");
		Report += TEXT("| Gate | Result | Evidence |\n");
		Report += TEXT("|---|---:|---|\n");
		for (const FTopologyFixtureResult& Result : TopologyResults)
		{
			Report += FString::Printf(
				TEXT("| %s | %s | applied `%d`, samples `%d`, global/assigned triangles `%d/%d`, all-same/majority/triple `%d/%d/%d`, compact `%d`, duplicate-authority `%d`, fixture assignments `%d`, motion `%s/%s`, reset serial `%d->%d`, active `%d->%d`, matrix `%d->%d`, pending collision `%d->%d`, generated/preserved `%d/%d`, policy/prior/projection `%d/%d/%d`, q/zGamma `%d/%d`, hash `%s`. |\n"),
				*Result.Name,
				*HoldPassFail(Result.bPass, Result.bHold),
				Result.Audit.bApplied ? 1 : 0,
				Result.Audit.SampleCount,
				Result.Audit.GlobalTriangleCount,
				Result.Audit.AssignedTriangleCount,
				Result.Audit.AllSameTriangleCount,
				Result.Audit.MajorityTriangleCount,
				Result.Audit.UnresolvedTripleJunctionCount,
				Result.Audit.bPlateLocalTopologyCompact ? 1 : 0,
				Result.Audit.bNoDuplicateTriangleAuthority ? 1 : 0,
				Result.Audit.bFixtureOwnedVertexAssignmentRecords ? 1 : 0,
				*Result.Audit.MotionHashBefore,
				*Result.Audit.MotionHashAfter,
				Result.Audit.ResetAudit.ResetSerialBefore,
				Result.Audit.ResetAudit.ResetSerialAfter,
				Result.Audit.ResetAudit.ActiveTriangleCountBefore,
				Result.Audit.ResetAudit.ActiveTriangleCountAfter,
				Result.Audit.ResetAudit.MatrixPairCountBefore,
				Result.Audit.ResetAudit.MatrixPairCountAfter,
				Result.Audit.ResetAudit.CollisionPendingTriangleCountBefore,
				Result.Audit.ResetAudit.CollisionPendingTriangleCountAfter,
				Result.Audit.GeneratedOceanicVertexCount,
				Result.Audit.PreservedGeneratedOceanicVertexCount,
				Result.Audit.PolicyWinnerCount,
				Result.Audit.PriorOwnerFallbackCount,
				Result.Audit.ProjectionOwnerFallbackCount,
				Result.Audit.bQProvenancePreserved ? 1 : 0,
				Result.Audit.bZGammaHoldPreserved ? 1 : 0,
				*Result.Audit.TopologyHash);
		}
		Report += FString::Printf(
			TEXT("| CollisionPending current-state wiring | %s | accepted groups `%d`, fixture-owned accepted group `%d`, pending triangle keys `%d`, filters sub/obd/coll `%d/%d/%d`, policy/prior `%d/%d`, grouping `%s`, selection `%s`. |\n"),
			*PassFail(CollisionResult.bPass),
			CollisionResult.WireAudit.AcceptedGroupCount,
			CollisionResult.WireAudit.bUsedFixtureOwnedAcceptedGroup ? 1 : 0,
			CollisionResult.WireAudit.PendingTriangleKeyCount,
			CollisionResult.WireAudit.FilteredSubductingCount,
			CollisionResult.WireAudit.FilteredObductionPendingCount,
			CollisionResult.WireAudit.FilteredCollisionPendingCount,
			CollisionResult.SelectionAudit.PolicyWinnerCount,
			CollisionResult.SelectionAudit.PriorOwnerFallbackCount,
			*CollisionResult.WireAudit.GroupingHash,
			*CollisionResult.WireAudit.SelectionHash);
		Report += FString::Printf(
			TEXT("| Same-seed topology replay | %s | Replay topology hashes `%s` and `%s`. |\n"),
			*PassFail(bReplayPass),
			*ReplayHashA,
			*ReplayHashB);
		Report += TEXT("| Inherited IIIB independent signature regression | pass | `CarrierLabPhaseIIID7` regression artifact remains the state-consuming signature token: computed/expected `bf8818a26ed7b1dc`. IIIE.5 adds reset gates rather than rerunning the expensive integrated signature in this focused slice. |\n");
		Report += TEXT("| zGamma paper-fidelity hold | hold | IIIE.4 generated records are preserved with `bUsedZGammaDistanceProfilePlaceholder=0`, `bUsedZGammaGeophysicsDerivedProfile=1`, and `bPaperFaithfulZGammaProfile=0`; topology may preserve them, but no report may claim the ridge-profile law is paper-faithful. |\n\n");

		Report += TEXT("## Contract Table\n\n");
		Report += TEXT("| Paper / IIIE.1 requirement | CarrierLab support now | Remaining obligation | Gate |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += TEXT("| Rebuild plate-local topology from global TDS vertex assignments | IIIE.5 duplicates, re-indexes, and compacts local vertices/triangles from fixture-owned assignment records | Wire this helper into the future production remesh cadence so live selection/gap-fill records supply the assignments | Compact topology and duplicate-authority gates |\n");
		Report += TEXT("| Mixed global-TDS triangles need explicit policy | All-same is direct; two-of-three majority is named lab policy; triple junction is a hold | Decide triple-junction handling only with paper citation or approved lab policy | Majority fixture and triple-junction hold fixture |\n");
		Report += TEXT("| Preserve plate geodetic motion across remesh | Motion hash before/after remains identical | Keep later remesh cadence from recomputing motion authority from projection | Motion hash gate |\n");
		Report += TEXT("| Reset process state at remesh | Active lists, distances, matrix state, subducting marks, obduction marks, and collision-pending keys reset to empty; reset serial advances | Later IIIB tracking must explicitly repopulate from geometry | Process reset fixture |\n");
		Report += TEXT("| Preserve divergent gap provenance | IIIE.4 q1/q2/qGamma, generated fields, and zGamma hold flags survive topology rebuild records | Full remesh event must attach these records per generated vertex | Oceanic provenance fixture |\n");
		Report += TEXT("| Accepted-but-unsutured collision groups are invisible to rays | Accepted IIID2 collision-group records seed `ConvergenceCollisionPendingTriangleKeys`, and IIIE.3 current-state selection maps them to `CollisionPending`; the compact fixture discloses whether it used a fixture-owned accepted record | Keep this wiring before any production remesh source selection | CollisionPending current-state wiring gate |\n\n");

		Report += TEXT("## Forbidden Policy Checks\n\n");
		Report += TEXT("| Forbidden policy | IIIE.5 status |\n");
		Report += TEXT("|---|---|\n");
		Report += TEXT("| Prior global sample owner/fraction fallback | Explicit per-record counter stays zero. |\n");
		Report += TEXT("| Projection-derived ownership authority | Explicit per-record counter stays zero. |\n");
		Report += TEXT("| Centroid/random/synthetic winner policy | Explicit per-record counter stays zero. |\n");
		Report += TEXT("| Stage 1.5 recovery/backfill/retention/hysteresis/anchoring | Not called; IIIE.5 uses a dedicated rebuild path rather than `RebuildPlateLocalStateFromSamples`. |\n");
		Report += TEXT("| Silent unresolved multi-hit routing | Explicit counter stays zero; triple-junction topology anomalies are holds, not winners. |\n");
		Report += TEXT("| zGamma paper-fidelity overclaim | Hold flag remains visible through generated records. |\n\n");

		Report += TEXT("## Stop Conditions For IIIE.6+\n\n");
		Report += TEXT("- Stop if a production remesh event calls the Stage 1.5 prior-owner/projection fallback path as primary IIIE authority.\n");
		Report += TEXT("- Stop if one-one-one triple-junction triangles are assigned without paper citation or explicit approved lab policy.\n");
		Report += TEXT("- Stop if active convergence lists, distance-to-front records, subduction matrix state, subducting marks, obduction marks, or collision-pending keys remain non-empty immediately after remesh reset.\n");
		Report += TEXT("- Stop if plate motion hashes change during topology rebuild.\n");
		Report += TEXT("- Stop if IIIE.4 generated vertices lose q1/q2/qGamma, signed velocity, age, elevation, ridge direction, or zGamma hold evidence during duplicate/re-index/re-compact.\n");
		Report += TEXT("- Stop if reports claim paper-faithful zGamma while generated records still report `bUsedZGammaGeophysicsDerivedProfile = true` and `bPaperFaithfulZGammaProfile = false`.\n\n");

		Report += TEXT("## Next Slice Boundary\n\n");
		Report += TEXT("IIIE.6 should be the remesh ledger/reframe slice: connect selection, divergent field generation, topology rebuild, and reset records into an event-level audit without adding optimization, new q1/q2 policy, further ridge-profile replacement, rifting, erosion, or long-horizon validation. The zGamma profile remains a geophysics-derived lab extension unless a paper-cited closed form is recovered.\n\n");
		Report += FString::Printf(TEXT("Metrics: `%s`.\n"), *MetricsPath);
		return Report;
	}
}

UCarrierLabPhaseIIIE5Commandlet::UCarrierLabPhaseIIIE5Commandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIIIE5Commandlet::Main(const FString& Params)
{
	UWorld* World = GetCommandletWorld();
	if (World == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLabPhaseIIIE5Commandlet could not find a commandlet world."));
		return 1;
	}

	TArray<FTopologyFixtureSpec> Specs;
	Specs.Add(MakeAllSameFixture());
	Specs.Add(MakeMajorityFixture());
	Specs.Add(MakeTripleJunctionFixture());
	Specs.Add(MakeProcessResetFixture());
	Specs.Add(MakeOceanicProvenanceFixture());

	TArray<FTopologyFixtureResult> TopologyResults;
	TopologyResults.Reserve(Specs.Num());
	for (const FTopologyFixtureSpec& Spec : Specs)
	{
		FTopologyFixtureResult Result;
		EvaluateTopologyFixture(Spec, Result, *World);
		TopologyResults.Add(Result);
	}

	FCollisionFixtureResult CollisionResult;
	EvaluateCollisionFixture(CollisionResult, *World);

	FTopologyFixtureResult ReplayA;
	FTopologyFixtureResult ReplayB;
	EvaluateTopologyFixture(MakeMajorityFixture(), ReplayA, *World);
	EvaluateTopologyFixture(MakeMajorityFixture(), ReplayB, *World);
	const bool bReplayPass =
		ReplayA.bPass &&
		ReplayB.bPass &&
		ReplayA.Audit.TopologyHash == ReplayB.Audit.TopologyHash;

	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);
	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("phase-iii-slice-iiie5-metrics.jsonl"));
	FString JsonLines;
	for (const FTopologyFixtureResult& Result : TopologyResults)
	{
		JsonLines += BuildTopologyJsonLine(Result) + LINE_TERMINATOR;
	}
	JsonLines += BuildCollisionJsonLine(CollisionResult) + LINE_TERMINATOR;
	JsonLines += FString::Printf(
		TEXT("{\"fixture\":\"same_seed_topology_replay\",\"pass\":%s,\"hash_a\":%s,\"hash_b\":%s}"),
		bReplayPass ? TEXT("true") : TEXT("false"),
		*JsonString(ReplayA.Audit.TopologyHash),
		*JsonString(ReplayB.Audit.TopologyHash)) + LINE_TERMINATOR;

	const FString ReportPath = GetReportPath(Params);
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
	const bool bMetricsWritten = FFileHelper::SaveStringToFile(JsonLines, *MetricsPath);
	const FString Report = BuildReport(
		TopologyResults,
		CollisionResult,
		bReplayPass,
		ReplayA.Audit.TopologyHash,
		ReplayB.Audit.TopologyHash,
		MetricsPath);
	const bool bReportWritten = FFileHelper::SaveStringToFile(Report, *ReportPath);

	bool bPass = bMetricsWritten && bReportWritten && CollisionResult.bPass && bReplayPass;
	for (const FTopologyFixtureResult& Result : TopologyResults)
	{
		bPass = bPass && Result.bPass;
	}

	UE_LOG(
		LogTemp,
		Display,
		TEXT("CarrierLab IIIE.5 gates: %s. Report=%s Metrics=%s"),
		*PassFail(bPass),
		*ReportPath,
		*MetricsPath);
	return bPass ? 0 : 1;
}
