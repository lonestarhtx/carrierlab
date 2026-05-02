// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIISlice2Commandlet.h"

#include "CarrierLabVisualizationActor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/PlatformMemory.h"
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
			case TEXT('\\'):
				Escaped += TEXT("\\\\");
				break;
			case TEXT('"'):
				Escaped += TEXT("\\\"");
				break;
			case TEXT('\n'):
				Escaped += TEXT("\\n");
				break;
			case TEXT('\r'):
				Escaped += TEXT("\\r");
				break;
			case TEXT('\t'):
				Escaped += TEXT("\\t");
				break;
			default:
				Escaped.AppendChar(Ch);
				break;
			}
		}
		return FString::Printf(TEXT("\"%s\""), *Escaped);
	}

	const TCHAR* FixtureName(const ECarrierLabPhaseIIMotionFixture Fixture)
	{
		switch (Fixture)
		{
		case ECarrierLabPhaseIIMotionFixture::Zero:
			return TEXT("zero_motion");
		case ECarrierLabPhaseIIMotionFixture::ForcedConvergence:
			return TEXT("forced_convergence");
		case ECarrierLabPhaseIIMotionFixture::ForcedDivergence:
			return TEXT("forced_divergence");
		case ECarrierLabPhaseIIMotionFixture::TripleJunction:
			return TEXT("triple_junction");
		case ECarrierLabPhaseIIMotionFixture::Default:
		default:
			return TEXT("default");
		}
	}

	const TCHAR* ContactClassName(const ECarrierLabPhaseIIContactClass ContactClass)
	{
		switch (ContactClass)
		{
		case ECarrierLabPhaseIIContactClass::Divergent:
			return TEXT("divergent");
		case ECarrierLabPhaseIIContactClass::Convergent:
			return TEXT("convergent");
		case ECarrierLabPhaseIIContactClass::ThirdPlate:
			return TEXT("third_plate");
		case ECarrierLabPhaseIIContactClass::TransformLowMargin:
		default:
			return TEXT("transform_low_margin");
		}
	}

	const TCHAR* TriangleLabelName(const ECarrierLabPhaseIITriangleLabel Label)
	{
		switch (Label)
		{
		case ECarrierLabPhaseIITriangleLabel::Subducting:
			return TEXT("subducting");
		case ECarrierLabPhaseIITriangleLabel::Overriding:
			return TEXT("overriding");
		case ECarrierLabPhaseIITriangleLabel::CollisionCandidate:
			return TEXT("collision_candidate");
		case ECarrierLabPhaseIITriangleLabel::Ambiguous:
			return TEXT("ambiguous");
		case ECarrierLabPhaseIITriangleLabel::None:
		default:
			return TEXT("none");
		}
	}

	const TCHAR* PolaritySourceName(const ECarrierLabPhaseIIPolaritySource Source)
	{
		switch (Source)
		{
		case ECarrierLabPhaseIIPolaritySource::MixedMaterial:
			return TEXT("mixed_material");
		case ECarrierLabPhaseIIPolaritySource::FixtureSpecified:
			return TEXT("fixture_specified");
		case ECarrierLabPhaseIIPolaritySource::SameMaterialAmbiguous:
			return TEXT("same_material_ambiguous");
		case ECarrierLabPhaseIIPolaritySource::ThirdPlateOutOfScope:
			return TEXT("third_plate_out_of_scope");
		case ECarrierLabPhaseIIPolaritySource::None:
		default:
			return TEXT("none");
		}
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseII"), TEXT("Slice2"), Stamp);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	int32 ParseIntParam(const FString& Params, const TCHAR* Key, const int32 DefaultValue)
	{
		int32 Value = DefaultValue;
		FParse::Value(*Params, Key, Value);
		return Value;
	}

	struct FSlice2FixtureConfig
	{
		FString Name;
		int32 SampleCount = 10000;
		int32 PlateCount = 40;
		int32 StepCount = 20;
		double ContinentalPlateFraction = 0.30;
		ECarrierLabPhaseIIMotionFixture Fixture = ECarrierLabPhaseIIMotionFixture::Default;
		bool bUseFixturePolarity = false;
		int32 FixtureUnderPlate = INDEX_NONE;
		int32 FixtureOverPlate = INDEX_NONE;
		bool bExpectNoLabels = false;
		bool bExpectLabels = false;
		bool bExpectAmbiguousOnly = false;
		bool bExpectThirdPlateOutOfScope = false;
		bool bCheckPairSign = false;
		int32 PairA = 0;
		int32 PairB = 1;
		double ExpectedPairSign = 0.0;
		int32 ExpectedSubductingPlate = INDEX_NONE;
	};

	struct FSlice2ReplayResult
	{
		FString FixtureName;
		int32 Replay = 0;
		int32 StepCount = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 Seed = 42;
		double ContinentalPlateFraction = 0.30;
		double PairSignedConvergenceVelocity = 0.0;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
		FCarrierLabVisualizationMetrics ProjectionMetrics;
		FString ProjectionHashBeforeLabels;
		FString StateHashBeforeLabels;
		bool bProjectionHashUnchanged = false;
		bool bStateHashUnchanged = false;
		int32 UnexpectedSubductingPlateCount = 0;
		int32 InvalidTraceLabelCount = 0;
		FString ContactJsonl;
		FString CanonicalContactJsonl;
		FString LabelJsonl;
		FString CanonicalLabelJsonl;
		bool bCompleted = false;
	};

	struct FSlice2FixtureSummary
	{
		FString FixtureName;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		bool bReplayLabelHashMatch = false;
		bool bReplayLabelLogByteMatch = false;
		bool bNoMutation = false;
		bool bNoLabelGate = true;
		bool bPolarityGate = true;
		bool bAmbiguityGate = true;
		bool bThirdPlateGate = true;
		bool bTraceabilityGate = true;
		bool bBoundedLabelGate = true;
		bool bPairSignGate = true;
		bool bCompleted = false;
		double PairSignedConvergenceA = 0.0;
		double PairSignedConvergenceB = 0.0;
		FCarrierLabPhaseIIContactMetrics ContactMetricsA;
		FCarrierLabPhaseIIContactMetrics ContactMetricsB;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetricsA;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetricsB;
		FString ContactHashA;
		FString ContactHashB;
		FString LabelHashA;
		FString LabelHashB;
		int32 UnexpectedSubductingPlateA = 0;
		int32 UnexpectedSubductingPlateB = 0;
		int32 InvalidTraceLabelsA = 0;
		int32 InvalidTraceLabelsB = 0;
		FString Notes;
	};

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

	ACarrierLabVisualizationActor* SpawnSlice2Actor(UWorld& World, const FSlice2FixtureConfig& Config)
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
		Actor->SampleCount = Config.SampleCount;
		Actor->PlateCount = Config.PlateCount;
		Actor->Seed = 42;
		Actor->ContinentalPlateFraction = Config.ContinentalPlateFraction;
		Actor->VelocityMmPerYear = 66.6666666667;
		Actor->MultiHitPolicy = ECarrierLabMultiHitPolicy::Centroid;
		Actor->VisualizationLayer = ECarrierLabVisualizationLayer::PlateId;
		Actor->FinishSpawning(FTransform::Identity);
		return Actor;
	}

	FString ContactRecordJson(const FString& FixtureName, const int32 Replay, const FCarrierLabPhaseIIContactRecord& Record)
	{
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"contact_id\":%d,\"step\":%d,\"sample_id\":%d,\"plate_a\":%d,\"plate_b\":%d,\"plate_a_local_triangle_id\":%d,\"plate_b_local_triangle_id\":%d,\"evidence_unit_position\":[%.12f,%.12f,%.12f],\"signed_convergence_velocity\":%.12f,\"velocity_margin\":%.12f,\"boundary_evidence\":%s,\"third_plate\":%s,\"contact_class\":%s}"),
			*JsonString(FixtureName),
			Replay,
			Record.ContactId,
			Record.Step,
			Record.SampleId,
			Record.PlateA,
			Record.PlateB,
			Record.PlateALocalTriangleId,
			Record.PlateBLocalTriangleId,
			Record.EvidenceUnitPosition.X,
			Record.EvidenceUnitPosition.Y,
			Record.EvidenceUnitPosition.Z,
			Record.SignedConvergenceVelocity,
			Record.VelocityMargin,
			Record.bBoundaryEvidence ? TEXT("true") : TEXT("false"),
			Record.bThirdPlate ? TEXT("true") : TEXT("false"),
			*JsonString(ContactClassName(Record.ContactClass)));
	}

	FString TriangleLabelRecordJson(const FString& FixtureName, const int32 Replay, const FCarrierLabPhaseIITriangleLabelRecord& Record)
	{
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"label_id\":%d,\"contact_id\":%d,\"step\":%d,\"sample_id\":%d,\"plate_id\":%d,\"other_plate_id\":%d,\"local_triangle_id\":%d,\"source_global_triangle_id\":%d,\"evidence_unit_position\":[%.12f,%.12f,%.12f],\"signed_convergence_velocity\":%.12f,\"velocity_margin\":%.12f,\"distance_from_contact_km\":%.12f,\"label\":%s,\"polarity_source\":%s,\"from_third_plate_contact\":%s,\"label_reason\":%s}"),
			*JsonString(FixtureName),
			Replay,
			Record.LabelId,
			Record.ContactId,
			Record.Step,
			Record.SampleId,
			Record.PlateId,
			Record.OtherPlateId,
			Record.LocalTriangleId,
			Record.SourceGlobalTriangleId,
			Record.EvidenceUnitPosition.X,
			Record.EvidenceUnitPosition.Y,
			Record.EvidenceUnitPosition.Z,
			Record.SignedConvergenceVelocity,
			Record.VelocityMargin,
			Record.DistanceFromContactKm,
			*JsonString(TriangleLabelName(Record.Label)),
			*JsonString(PolaritySourceName(Record.PolaritySource)),
			Record.bFromThirdPlateContact ? TEXT("true") : TEXT("false"),
			*JsonString(Record.LabelReason));
	}

	FString ReplayMetricJson(const FSlice2ReplayResult& Result)
	{
		const FCarrierLabPhaseIIContactMetrics& Contact = Result.ContactMetrics;
		const FCarrierLabPhaseIITriangleLabelMetrics& Label = Result.LabelMetrics;
		const FCarrierLabVisualizationMetrics& Projection = Result.ProjectionMetrics;
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"step\":%d,\"step_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"seed\":%d,\"continental_plate_fraction\":%.12f,\"pair_signed_convergence_velocity\":%.12f,\"contact_record_count\":%d,\"convergent_contact_count\":%d,\"divergent_contact_count\":%d,\"third_plate_contact_count\":%d,\"subduction_candidate_count\":%d,\"contact_hash\":%s,\"label_record_count\":%d,\"unique_labeled_triangle_count\":%d,\"labelable_contact_count\":%d,\"fixture_specified_polarity_contact_count\":%d,\"mixed_material_polarity_contact_count\":%d,\"same_material_ambiguous_contact_count\":%d,\"third_plate_out_of_scope_contact_count\":%d,\"non_convergent_skipped_contact_count\":%d,\"subducting_label_count\":%d,\"overriding_label_count\":%d,\"ambiguous_label_count\":%d,\"collision_candidate_label_count\":%d,\"labels_from_third_plate_contact_count\":%d,\"max_labels_per_contact\":%d,\"unique_labeled_triangle_area_proxy\":%.12f,\"triangle_label_seconds\":%.12f,\"label_hash\":%s,\"projection_hash_before_labels\":%s,\"projection_hash_after_labels\":%s,\"state_hash_before_labels\":%s,\"state_hash_after_labels\":%s,\"projection_hash_unchanged\":%s,\"state_hash_unchanged\":%s,\"unexpected_subducting_plate_count\":%d,\"invalid_trace_label_count\":%d,\"projected_miss_count\":%d,\"projected_multi_hit_count\":%d,\"authoritative_caf\":%.12f,\"projected_caf\":%.12f,\"drift_mean_km\":%.12f,\"drift_p95_km\":%.12f,\"memory_gb\":%.12f}"),
			*JsonString(Result.FixtureName),
			Result.Replay,
			Contact.Step,
			Result.StepCount,
			Result.SampleCount,
			Result.PlateCount,
			Result.Seed,
			Result.ContinentalPlateFraction,
			Result.PairSignedConvergenceVelocity,
			Contact.ContactRecordCount,
			Contact.ConvergentContactCount,
			Contact.DivergentContactCount,
			Contact.ThirdPlateContactCount,
			Contact.SubductionCandidateCount,
			*JsonString(Contact.ContactLogHash),
			Label.LabelRecordCount,
			Label.UniqueLabeledTriangleCount,
			Label.LabelableContactCount,
			Label.FixtureSpecifiedPolarityContactCount,
			Label.MixedMaterialPolarityContactCount,
			Label.SameMaterialAmbiguousContactCount,
			Label.ThirdPlateOutOfScopeContactCount,
			Label.NonConvergentSkippedContactCount,
			Label.SubductingLabelCount,
			Label.OverridingLabelCount,
			Label.AmbiguousLabelCount,
			Label.CollisionCandidateLabelCount,
			Label.LabelsFromThirdPlateContactCount,
			Label.MaxLabelsPerContact,
			Label.UniqueLabeledTriangleAreaProxy,
			Label.TriangleLabelSeconds,
			*JsonString(Label.TriangleLabelHash),
			*JsonString(Result.ProjectionHashBeforeLabels),
			*JsonString(Projection.LastHash),
			*JsonString(Result.StateHashBeforeLabels),
			*JsonString(Projection.StateHash),
			Result.bProjectionHashUnchanged ? TEXT("true") : TEXT("false"),
			Result.bStateHashUnchanged ? TEXT("true") : TEXT("false"),
			Result.UnexpectedSubductingPlateCount,
			Result.InvalidTraceLabelCount,
			Projection.RawMissCount,
			Projection.RawMultiHitCount,
			Projection.AuthoritativeCAF,
			Projection.ProjectedCAF,
			Projection.DriftErrorMeanKm,
			Projection.DriftErrorP95Km,
			MemoryGb);
	}

	bool RunFixtureReplay(const FSlice2FixtureConfig& Config, const int32 Replay, FSlice2ReplayResult& OutResult)
	{
		OutResult = FSlice2ReplayResult();
		OutResult.FixtureName = Config.Name;
		OutResult.Replay = Replay;
		OutResult.StepCount = Config.StepCount;
		OutResult.SampleCount = Config.SampleCount;
		OutResult.PlateCount = Config.PlateCount;
		OutResult.Seed = 42;
		OutResult.ContinentalPlateFraction = Config.ContinentalPlateFraction;

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 2 fixture %s replay %d could not find a world."), *Config.Name, Replay);
			return false;
		}

		ACarrierLabVisualizationActor* Actor = SpawnSlice2Actor(*World, Config);
		if (Actor == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 2 fixture %s replay %d could not spawn actor."), *Config.Name, Replay);
			return false;
		}

		if (!Actor->InitializeCarrier())
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 2 fixture %s replay %d failed initialization."), *Config.Name, Replay);
			Actor->Destroy();
			return false;
		}

		Actor->ConfigurePhaseIIMotionFixture(Config.Fixture);
		if (Config.bCheckPairSign)
		{
			OutResult.PairSignedConvergenceVelocity = Actor->ComputePhaseIIPairSignedConvergenceVelocity(Config.PairA, Config.PairB);
		}

		for (int32 StepIndex = 0; StepIndex < Config.StepCount; ++StepIndex)
		{
			Actor->StepOnce();
		}

		TArray<FCarrierLabPhaseIIContactRecord> Contacts;
		if (!Actor->DetectPhaseIIContacts(Contacts, OutResult.ContactMetrics))
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 2 fixture %s replay %d failed contact detection."), *Config.Name, Replay);
			Actor->Destroy();
			return false;
		}

		OutResult.ProjectionHashBeforeLabels = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashBeforeLabels = Actor->CurrentMetrics.StateHash;

		FCarrierLabPhaseIITriangleLabelConfig LabelConfig;
		LabelConfig.bUseFixturePolarity = Config.bUseFixturePolarity;
		LabelConfig.FixtureUnderPlate = Config.FixtureUnderPlate;
		LabelConfig.FixtureOverPlate = Config.FixtureOverPlate;

		TArray<FCarrierLabPhaseIITriangleLabelRecord> Labels;
		if (!Actor->BuildPhaseIITriangleLabels(Contacts, LabelConfig, Labels, OutResult.LabelMetrics))
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 2 fixture %s replay %d failed triangle labeling."), *Config.Name, Replay);
			Actor->Destroy();
			return false;
		}

		OutResult.ProjectionMetrics = Actor->CurrentMetrics;
		OutResult.bProjectionHashUnchanged = OutResult.ProjectionHashBeforeLabels == Actor->CurrentMetrics.LastHash;
		OutResult.bStateHashUnchanged = OutResult.StateHashBeforeLabels == Actor->CurrentMetrics.StateHash;

		for (const FCarrierLabPhaseIIContactRecord& Contact : Contacts)
		{
			OutResult.ContactJsonl += ContactRecordJson(Config.Name, Replay, Contact);
			OutResult.ContactJsonl += LINE_TERMINATOR;
			OutResult.CanonicalContactJsonl += ContactRecordJson(Config.Name, -1, Contact);
			OutResult.CanonicalContactJsonl += LINE_TERMINATOR;
		}
		for (const FCarrierLabPhaseIITriangleLabelRecord& Label : Labels)
		{
			OutResult.LabelJsonl += TriangleLabelRecordJson(Config.Name, Replay, Label);
			OutResult.LabelJsonl += LINE_TERMINATOR;
			OutResult.CanonicalLabelJsonl += TriangleLabelRecordJson(Config.Name, -1, Label);
			OutResult.CanonicalLabelJsonl += LINE_TERMINATOR;

			if (Label.ContactId == INDEX_NONE ||
				Label.PlateId == INDEX_NONE ||
				Label.OtherPlateId == INDEX_NONE ||
				Label.LocalTriangleId == INDEX_NONE ||
				Label.SourceGlobalTriangleId == INDEX_NONE)
			{
				++OutResult.InvalidTraceLabelCount;
			}
			if (Config.ExpectedSubductingPlate != INDEX_NONE &&
				Label.Label == ECarrierLabPhaseIITriangleLabel::Subducting &&
				Label.PlateId != Config.ExpectedSubductingPlate)
			{
				++OutResult.UnexpectedSubductingPlateCount;
			}
		}

		OutResult.bCompleted = true;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return true;
	}

	FSlice2FixtureSummary SummarizeFixture(const FSlice2FixtureConfig& Config, const FSlice2ReplayResult& A, const FSlice2ReplayResult& B)
	{
		FSlice2FixtureSummary Summary;
		Summary.FixtureName = Config.Name;
		Summary.SampleCount = Config.SampleCount;
		Summary.PlateCount = Config.PlateCount;
		Summary.StepCount = Config.StepCount;
		Summary.bCompleted = A.bCompleted && B.bCompleted;
		Summary.ContactMetricsA = A.ContactMetrics;
		Summary.ContactMetricsB = B.ContactMetrics;
		Summary.LabelMetricsA = A.LabelMetrics;
		Summary.LabelMetricsB = B.LabelMetrics;
		Summary.ContactHashA = A.ContactMetrics.ContactLogHash;
		Summary.ContactHashB = B.ContactMetrics.ContactLogHash;
		Summary.LabelHashA = A.LabelMetrics.TriangleLabelHash;
		Summary.LabelHashB = B.LabelMetrics.TriangleLabelHash;
		Summary.PairSignedConvergenceA = A.PairSignedConvergenceVelocity;
		Summary.PairSignedConvergenceB = B.PairSignedConvergenceVelocity;
		Summary.UnexpectedSubductingPlateA = A.UnexpectedSubductingPlateCount;
		Summary.UnexpectedSubductingPlateB = B.UnexpectedSubductingPlateCount;
		Summary.InvalidTraceLabelsA = A.InvalidTraceLabelCount;
		Summary.InvalidTraceLabelsB = B.InvalidTraceLabelCount;

		Summary.bReplayLabelHashMatch = A.bCompleted && B.bCompleted && A.LabelMetrics.TriangleLabelHash == B.LabelMetrics.TriangleLabelHash;
		Summary.bReplayLabelLogByteMatch = A.bCompleted && B.bCompleted && A.CanonicalLabelJsonl == B.CanonicalLabelJsonl;
		Summary.bNoMutation = A.bProjectionHashUnchanged && A.bStateHashUnchanged && B.bProjectionHashUnchanged && B.bStateHashUnchanged;
		Summary.bNoLabelGate = !Config.bExpectNoLabels || (A.LabelMetrics.LabelRecordCount == 0 && B.LabelMetrics.LabelRecordCount == 0);
		Summary.bPolarityGate = Config.ExpectedSubductingPlate == INDEX_NONE ||
			(A.LabelMetrics.SubductingLabelCount > 0 && B.LabelMetrics.SubductingLabelCount > 0 &&
				A.UnexpectedSubductingPlateCount == 0 && B.UnexpectedSubductingPlateCount == 0);
		Summary.bAmbiguityGate = !Config.bExpectAmbiguousOnly ||
			(A.LabelMetrics.AmbiguousLabelCount > 0 && B.LabelMetrics.AmbiguousLabelCount > 0 &&
				A.LabelMetrics.SubductingLabelCount == 0 && B.LabelMetrics.SubductingLabelCount == 0 &&
				A.LabelMetrics.OverridingLabelCount == 0 && B.LabelMetrics.OverridingLabelCount == 0 &&
				A.LabelMetrics.CollisionCandidateLabelCount == 0 && B.LabelMetrics.CollisionCandidateLabelCount == 0);
		Summary.bThirdPlateGate = !Config.bExpectThirdPlateOutOfScope ||
			(A.LabelMetrics.ThirdPlateOutOfScopeContactCount > 0 && B.LabelMetrics.ThirdPlateOutOfScopeContactCount > 0 &&
				A.LabelMetrics.LabelsFromThirdPlateContactCount == 0 && B.LabelMetrics.LabelsFromThirdPlateContactCount == 0);
		Summary.bTraceabilityGate =
			A.InvalidTraceLabelCount == 0 && B.InvalidTraceLabelCount == 0 &&
			A.LabelMetrics.NaNOrInfCount == 0 && B.LabelMetrics.NaNOrInfCount == 0 &&
			A.LabelMetrics.LabelsFromThirdPlateContactCount == 0 && B.LabelMetrics.LabelsFromThirdPlateContactCount == 0;
		Summary.bBoundedLabelGate =
			A.LabelMetrics.MaxLabelsPerContact <= 2 && B.LabelMetrics.MaxLabelsPerContact <= 2 &&
			A.LabelMetrics.LabelRecordCount <= A.LabelMetrics.LabelableContactCount * 2 &&
			B.LabelMetrics.LabelRecordCount <= B.LabelMetrics.LabelableContactCount * 2;
		Summary.bPairSignGate = !Config.bCheckPairSign ||
			(Config.ExpectedPairSign > 0.0 && A.PairSignedConvergenceVelocity > 1.0e-6 && B.PairSignedConvergenceVelocity > 1.0e-6) ||
			(Config.ExpectedPairSign < 0.0 && A.PairSignedConvergenceVelocity < -1.0e-6 && B.PairSignedConvergenceVelocity < -1.0e-6) ||
			(Config.ExpectedPairSign == 0.0 && FMath::Abs(A.PairSignedConvergenceVelocity) <= 1.0e-6 && FMath::Abs(B.PairSignedConvergenceVelocity) <= 1.0e-6);

		if (Config.bUseFixturePolarity)
		{
			Summary.Notes = FString::Printf(TEXT("Fixture polarity pins under plate %d and over plate %d; this is a Slice 2 oracle, not a production fallback."), Config.FixtureUnderPlate, Config.FixtureOverPlate);
		}
		else if (Config.bExpectAmbiguousOnly)
		{
			Summary.Notes = TEXT("Same-material convergent contacts emit ambiguous labels only; no subducting or overriding labels are invented.");
		}
		else if (Config.bExpectThirdPlateOutOfScope)
		{
			Summary.Notes = TEXT("Third-plate contacts are counted as out-of-scope evidence and emit no subducting or overriding triangle labels.");
		}
		else if (Config.bExpectNoLabels)
		{
			Summary.Notes = TEXT("Control fixture should emit no triangle labels because it supplies no two-plate convergent labeling evidence.");
		}
		return Summary;
	}

	bool SummaryPasses(const FSlice2FixtureSummary& Summary)
	{
		return Summary.bCompleted &&
			Summary.bReplayLabelHashMatch &&
			Summary.bReplayLabelLogByteMatch &&
			Summary.bNoMutation &&
			Summary.bNoLabelGate &&
			Summary.bPolarityGate &&
			Summary.bAmbiguityGate &&
			Summary.bThirdPlateGate &&
			Summary.bTraceabilityGate &&
			Summary.bBoundedLabelGate &&
			Summary.bPairSignGate;
	}

	FString BuildReport(const FString& OutputRoot, const TArray<FSlice2FixtureSummary>& Summaries)
	{
		FString Report = TEXT("# Phase II Slice 2 Checkpoint: Polarity And Triangle Labels\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This checkpoint adds read-only polarity and plate-local triangle labels on top of Slice 1 contact records. It does not filter projection candidates, resample, mutate material, or change plate topology. Triangle labels are evidence records for Slice 3's future filter integration.\n\n");
		Report += TEXT("Policy: fixture polarity wins only inside explicit polarity fixtures; otherwise mixed material uses oceanic-under-continental. Same-material contacts remain ambiguous. Third-plate contacts are outside the Slice 2 two-plate polarity model and emit no `subducting` or `overriding` labels.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Fixture | Samples | Plates | Steps | Label hash match | Label log byte match | No mutation | Sign gate | No-label gate | Polarity gate | Ambiguity gate | Third-plate gate | Traceability | Direct-hit bound | Verdict |\n");
		Report += TEXT("|---|---:|---:|---:|---|---|---|---|---|---|---|---|---|---|---|\n");
		for (const FSlice2FixtureSummary& Summary : Summaries)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |\n"),
				*Summary.FixtureName,
				Summary.SampleCount,
				Summary.PlateCount,
				Summary.StepCount,
				Summary.bReplayLabelHashMatch ? TEXT("pass") : TEXT("fail"),
				Summary.bReplayLabelLogByteMatch ? TEXT("pass") : TEXT("fail"),
				Summary.bNoMutation ? TEXT("pass") : TEXT("fail"),
				Summary.bPairSignGate ? TEXT("pass") : TEXT("fail"),
				Summary.bNoLabelGate ? TEXT("pass") : TEXT("fail"),
				Summary.bPolarityGate ? TEXT("pass") : TEXT("fail"),
				Summary.bAmbiguityGate ? TEXT("pass") : TEXT("fail"),
				Summary.bThirdPlateGate ? TEXT("pass") : TEXT("fail"),
				Summary.bTraceabilityGate ? TEXT("pass") : TEXT("fail"),
				Summary.bBoundedLabelGate ? TEXT("pass") : TEXT("fail"),
				SummaryPasses(Summary) ? TEXT("pass") : TEXT("investigate"));
		}

		Report += TEXT("\n## Label Metrics\n\n");
		Report += TEXT("| Fixture | Contacts | Labelable | Labels | Unique triangles | Subducting | Overriding | Ambiguous | Third-plate out-of-scope | From third-plate | Max labels/contact | Label seconds | Label hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FSlice2FixtureSummary& Summary : Summaries)
		{
			const FCarrierLabPhaseIITriangleLabelMetrics& Label = Summary.LabelMetricsA;
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %.6f | `%s` |\n"),
				*Summary.FixtureName,
				Label.ContactRecordCount,
				Label.LabelableContactCount,
				Label.LabelRecordCount,
				Label.UniqueLabeledTriangleCount,
				Label.SubductingLabelCount,
				Label.OverridingLabelCount,
				Label.AmbiguousLabelCount,
				Label.ThirdPlateOutOfScopeContactCount,
				Label.LabelsFromThirdPlateContactCount,
				Label.MaxLabelsPerContact,
				Label.TriangleLabelSeconds,
				*Label.TriangleLabelHash);
		}

		Report += TEXT("\n## Polarity Source Metrics\n\n");
		Report += TEXT("| Fixture | Fixture-specified contacts | Mixed-material contacts | Same-material ambiguous contacts | Unexpected subducting plate | Invalid trace labels | Area proxy |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|\n");
		for (const FSlice2FixtureSummary& Summary : Summaries)
		{
			const FCarrierLabPhaseIITriangleLabelMetrics& Label = Summary.LabelMetricsA;
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d | %.12f |\n"),
				*Summary.FixtureName,
				Label.FixtureSpecifiedPolarityContactCount,
				Label.MixedMaterialPolarityContactCount,
				Label.SameMaterialAmbiguousContactCount,
				Summary.UnexpectedSubductingPlateA,
				Summary.InvalidTraceLabelsA,
				Label.UniqueLabeledTriangleAreaProxy);
		}

		Report += TEXT("\n## Directional Motion Probe\n\n");
		Report += TEXT("| Fixture | Signed convergence velocity replay A | Signed convergence velocity replay B | Interpretation |\n");
		Report += TEXT("|---|---:|---:|---|\n");
		for (const FSlice2FixtureSummary& Summary : Summaries)
		{
			if (Summary.PairSignedConvergenceA == 0.0 && Summary.PairSignedConvergenceB == 0.0)
			{
				continue;
			}
			const TCHAR* Interpretation = Summary.PairSignedConvergenceA > 0.0 ? TEXT("converging") : TEXT("separating");
			Report += FString::Printf(
				TEXT("| %s | %.12f | %.12f | %s |\n"),
				*Summary.FixtureName,
				Summary.PairSignedConvergenceA,
				Summary.PairSignedConvergenceB,
				Interpretation);
		}

		Report += TEXT("\n## Notes\n\n");
		for (const FSlice2FixtureSummary& Summary : Summaries)
		{
			if (!Summary.Notes.IsEmpty())
			{
				Report += FString::Printf(TEXT("- `%s`: %s\n"), *Summary.FixtureName, *Summary.Notes);
			}
		}
		Report += TEXT("- The direct-hit bound means Slice 2 labels only the two plate-local triangles cited by a non-third-plate convergent contact. There is no propagation into a broad mask.\n");
		Report += TEXT("- `labels.jsonl` is the Slice 2 process evidence. Slice 3 may consume `subducting` labels through the resampling filter hook, but this checkpoint deliberately does not filter anything.\n");

		bool bAllPass = true;
		for (const FSlice2FixtureSummary& Summary : Summaries)
		{
			bAllPass &= SummaryPasses(Summary);
		}
		Report += TEXT("\n## Recommendation\n\n");
		Report += bAllPass
			? TEXT("Go for user review of Phase II Slice 2. The labeler is deterministic, non-mutating, traceable to Slice 1 contacts and plate-local triangles, and keeps third-plate evidence outside the two-plate polarity model. Do not advance to Slice 3 until the user records explicit go/no-go.\n")
			: TEXT("No-go for Slice 3. One or more Slice 2 gates failed; investigate before wiring labels into the resampling filter.\n");
		return Report;
	}
}

UCarrierLabPhaseIISlice2Commandlet::UCarrierLabPhaseIISlice2Commandlet()
{
	IsClient = false;
	IsEditor = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIISlice2Commandlet::Main(const FString& Params)
{
	const int32 DefaultSteps = FMath::Max(1, ParseIntParam(Params, TEXT("Steps="), 40));
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	const TArray<FSlice2FixtureConfig> Fixtures = {
		{TEXT("default_60k"), 60000, 40, DefaultSteps, 0.30, ECarrierLabPhaseIIMotionFixture::Default, false, INDEX_NONE, INDEX_NONE, false, true, false, true, false, 0, 1, 0.0, INDEX_NONE},
		{TEXT("zero_motion"), 10000, 40, DefaultSteps, 0.30, ECarrierLabPhaseIIMotionFixture::Zero, false, INDEX_NONE, INDEX_NONE, true, false, false, false, true, 0, 1, 0.0, INDEX_NONE},
		{TEXT("single_plate"), 10000, 1, DefaultSteps, 0.30, ECarrierLabPhaseIIMotionFixture::Default, false, INDEX_NONE, INDEX_NONE, true, false, false, false, false, 0, 1, 0.0, INDEX_NONE},
		{TEXT("forced_divergence"), 10000, 2, 0, 0.50, ECarrierLabPhaseIIMotionFixture::ForcedDivergence, false, INDEX_NONE, INDEX_NONE, true, false, false, false, true, 0, 1, -1.0, INDEX_NONE},
		{TEXT("mixed_material"), 10000, 2, DefaultSteps, 0.50, ECarrierLabPhaseIIMotionFixture::ForcedConvergence, false, INDEX_NONE, INDEX_NONE, false, true, false, false, true, 0, 1, 1.0, 1},
		{TEXT("polarity_under_1"), 10000, 2, DefaultSteps, 1.00, ECarrierLabPhaseIIMotionFixture::ForcedConvergence, true, 1, 0, false, true, false, false, true, 0, 1, 1.0, 1},
		{TEXT("polarity_under_0"), 10000, 2, DefaultSteps, 1.00, ECarrierLabPhaseIIMotionFixture::ForcedConvergence, true, 0, 1, false, true, false, false, true, 0, 1, 1.0, 0},
		{TEXT("all_continental"), 10000, 2, DefaultSteps, 1.00, ECarrierLabPhaseIIMotionFixture::ForcedConvergence, false, INDEX_NONE, INDEX_NONE, false, true, true, false, true, 0, 1, 1.0, INDEX_NONE},
		{TEXT("ocean_only"), 10000, 2, DefaultSteps, 0.00, ECarrierLabPhaseIIMotionFixture::ForcedConvergence, false, INDEX_NONE, INDEX_NONE, false, true, true, false, true, 0, 1, 1.0, INDEX_NONE}
	};

	TArray<FSlice2ReplayResult> ReplayResults;
	TArray<FSlice2FixtureSummary> Summaries;
	bool bAllRunsCompleted = true;

	for (const FSlice2FixtureConfig& Fixture : Fixtures)
	{
		UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 2: fixture=%s samples=%d plates=%d steps=%d"),
			*Fixture.Name,
			Fixture.SampleCount,
			Fixture.PlateCount,
			Fixture.StepCount);
		FSlice2ReplayResult A;
		FSlice2ReplayResult B;
		bAllRunsCompleted &= RunFixtureReplay(Fixture, 0, A);
		bAllRunsCompleted &= RunFixtureReplay(Fixture, 1, B);
		ReplayResults.Add(A);
		ReplayResults.Add(B);
		Summaries.Add(SummarizeFixture(Fixture, A, B));
	}

	FString MetricsJsonl;
	FString ContactsJsonl;
	FString LabelsJsonl;
	for (const FSlice2ReplayResult& Result : ReplayResults)
	{
		MetricsJsonl += ReplayMetricJson(Result);
		MetricsJsonl += LINE_TERMINATOR;
		ContactsJsonl += Result.ContactJsonl;
		LabelsJsonl += Result.LabelJsonl;
	}

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	if (!FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write Slice 2 metrics: %s"), *MetricsPath);
		return 1;
	}
	const FString ContactsPath = FPaths::Combine(OutputRoot, TEXT("contacts.jsonl"));
	if (!FFileHelper::SaveStringToFile(ContactsJsonl, *ContactsPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write Slice 2 contacts: %s"), *ContactsPath);
		return 1;
	}
	const FString LabelsPath = FPaths::Combine(OutputRoot, TEXT("labels.jsonl"));
	if (!FFileHelper::SaveStringToFile(LabelsJsonl, *LabelsPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write Slice 2 labels: %s"), *LabelsPath);
		return 1;
	}

	const FString Report = BuildReport(OutputRoot, Summaries);
	const FString ReportPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("docs"), TEXT("checkpoints"), TEXT("phase-ii-slice-2-report.md"));
	if (!FFileHelper::SaveStringToFile(Report, *ReportPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write Slice 2 report: %s"), *ReportPath);
		return 1;
	}

	bool bAllPass = bAllRunsCompleted;
	for (const FSlice2FixtureSummary& Summary : Summaries)
	{
		bAllPass &= SummaryPasses(Summary);
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 2 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 2 contacts: %s"), *ContactsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 2 labels: %s"), *LabelsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 2 report: %s"), *ReportPath);
	return bAllPass ? 0 : 2;
}
