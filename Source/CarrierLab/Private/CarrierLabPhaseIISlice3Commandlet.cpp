// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIISlice3Commandlet.h"

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

	const TCHAR* DecisionClassName(const ECarrierLabPhaseIIFilterDecisionClass DecisionClass)
	{
		switch (DecisionClass)
		{
		case ECarrierLabPhaseIIFilterDecisionClass::CandidateFiltered:
			return TEXT("candidate_filtered");
		case ECarrierLabPhaseIIFilterDecisionClass::GapFill:
			return TEXT("gap_fill");
		case ECarrierLabPhaseIIFilterDecisionClass::UnresolvedMultiHit:
			return TEXT("unresolved_multi_hit");
		case ECarrierLabPhaseIIFilterDecisionClass::FilterExhausted:
			return TEXT("filter_exhausted");
		case ECarrierLabPhaseIIFilterDecisionClass::ResolvedSingle:
		default:
			return TEXT("resolved_single");
		}
	}

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseII"), TEXT("Slice3"), Stamp);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	int32 ParseIntParam(const FString& Params, const TCHAR* Key, const int32 DefaultValue)
	{
		int32 Value = DefaultValue;
		FParse::Value(*Params, Key, Value);
		return Value;
	}

	struct FSlice3FixtureConfig
	{
		FString Name;
		int32 SampleCount = 10000;
		int32 PlateCount = 40;
		int32 StepCount = 32;
		double ContinentalPlateFraction = 0.30;
		ECarrierLabPhaseIIMotionFixture Fixture = ECarrierLabPhaseIIMotionFixture::Default;
		bool bUseFixturePolarity = false;
		int32 FixtureUnderPlate = INDEX_NONE;
		int32 FixtureOverPlate = INDEX_NONE;
		bool bExpectNoFilteredCandidates = false;
		bool bExpectFilteredCandidates = false;
		bool bExpectMixedSignal = false;
		bool bExpectThirdPlateExplanation = false;
		bool bCheckPairSign = false;
		double ExpectedPairSign = 0.0;
		int32 ExpectedFilteredPlate = INDEX_NONE;
	};

	struct FSlice3ReplayResult
	{
		FString FixtureName;
		int32 Replay = 0;
		int32 StepCount = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		double PairSignedConvergenceVelocity = 0.0;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabPhaseIITriangleLabelMetrics LabelMetrics;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterMetrics;
		FCarrierLabVisualizationMetrics ProjectionMetrics;
		int32 UnexpectedFilteredPlateCount = 0;
		int32 InvalidDecisionTraceCount = 0;
		FString CanonicalContactJsonl;
		FString ContactJsonl;
		FString CanonicalLabelJsonl;
		FString LabelJsonl;
		FString CanonicalDecisionJsonl;
		FString DecisionJsonl;
		bool bCompleted = false;
	};

	struct FSlice3FixtureSummary
	{
		FString FixtureName;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		bool bCompleted = false;
		bool bContactReplayHash = false;
		bool bLabelReplayHash = false;
		bool bDecisionReplayHash = false;
		bool bDecisionLogByteMatch = false;
		bool bPostStateHashMatch = false;
		bool bAuthCafGate = false;
		bool bNoThirdPlateConsumption = false;
		bool bNoFilteredControlGate = true;
		bool bFilteredExpectedGate = true;
		bool bExpectedFilteredPlateGate = true;
		bool bMixedSignalGate = true;
		bool bPostFilterMultiGate = true;
		bool bTraceGate = false;
		bool bPairSignGate = true;
		double PairSignedConvergenceA = 0.0;
		double PairSignedConvergenceB = 0.0;
		int32 UnexpectedFilteredPlateA = 0;
		int32 UnexpectedFilteredPlateB = 0;
		int32 InvalidDecisionTraceA = 0;
		int32 InvalidDecisionTraceB = 0;
		FCarrierLabPhaseIIContactMetrics ContactA;
		FCarrierLabPhaseIITriangleLabelMetrics LabelA;
		FCarrierLabPhaseIIResamplingFilterMetrics FilterA;
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

	ACarrierLabVisualizationActor* SpawnSlice3Actor(UWorld& World, const FSlice3FixtureConfig& Config)
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
			TEXT("{\"fixture\":%s,\"replay\":%d,\"contact_id\":%d,\"step\":%d,\"sample_id\":%d,\"plate_a\":%d,\"plate_b\":%d,\"plate_a_local_triangle_id\":%d,\"plate_b_local_triangle_id\":%d,\"signed_convergence_velocity\":%.12f,\"boundary_evidence\":%s,\"third_plate\":%s,\"contact_class\":%s}"),
			*JsonString(FixtureName),
			Replay,
			Record.ContactId,
			Record.Step,
			Record.SampleId,
			Record.PlateA,
			Record.PlateB,
			Record.PlateALocalTriangleId,
			Record.PlateBLocalTriangleId,
			Record.SignedConvergenceVelocity,
			Record.bBoundaryEvidence ? TEXT("true") : TEXT("false"),
			Record.bThirdPlate ? TEXT("true") : TEXT("false"),
			*JsonString(ContactClassName(Record.ContactClass)));
	}

	FString LabelRecordJson(const FString& FixtureName, const int32 Replay, const FCarrierLabPhaseIITriangleLabelRecord& Record)
	{
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"label_id\":%d,\"contact_id\":%d,\"step\":%d,\"sample_id\":%d,\"plate_id\":%d,\"other_plate_id\":%d,\"local_triangle_id\":%d,\"source_global_triangle_id\":%d,\"signed_convergence_velocity\":%.12f,\"label\":%s,\"polarity_source\":%s,\"from_third_plate_contact\":%s}"),
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
			Record.SignedConvergenceVelocity,
			*JsonString(TriangleLabelName(Record.Label)),
			*JsonString(PolaritySourceName(Record.PolaritySource)),
			Record.bFromThirdPlateContact ? TEXT("true") : TEXT("false"));
	}

	FString DecisionRecordJson(const FString& FixtureName, const int32 Replay, const FCarrierLabPhaseIIFilterDecisionRecord& Record)
	{
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"decision_id\":%d,\"event_id\":%d,\"step\":%d,\"sample_id\":%d,\"raw_candidate_count\":%d,\"raw_plate_count\":%d,\"filtered_candidate_count\":%d,\"post_filter_candidate_count\":%d,\"post_filter_plate_count\":%d,\"resolved_plate_id\":%d,\"filtered_plate_id\":%d,\"filtered_local_triangle_id\":%d,\"source_contact_id\":%d,\"source_label_id\":%d,\"boundary_evidence\":%s,\"third_plate_evidence\":%s,\"decision_class\":%s}"),
			*JsonString(FixtureName),
			Replay,
			Record.DecisionId,
			Record.EventId,
			Record.Step,
			Record.SampleId,
			Record.RawCandidateCount,
			Record.RawPlateCount,
			Record.FilteredCandidateCount,
			Record.PostFilterCandidateCount,
			Record.PostFilterPlateCount,
			Record.ResolvedPlateId,
			Record.FilteredPlateId,
			Record.FilteredLocalTriangleId,
			Record.SourceContactId,
			Record.SourceLabelId,
			Record.bBoundaryEvidence ? TEXT("true") : TEXT("false"),
			Record.bThirdPlateEvidence ? TEXT("true") : TEXT("false"),
			*JsonString(DecisionClassName(Record.DecisionClass)));
	}

	FString ReplayMetricJson(const FSlice3ReplayResult& Result)
	{
		const FCarrierLabPhaseIIContactMetrics& Contact = Result.ContactMetrics;
		const FCarrierLabPhaseIITriangleLabelMetrics& Label = Result.LabelMetrics;
		const FCarrierLabPhaseIIResamplingFilterMetrics& Filter = Result.FilterMetrics;
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"step_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"pair_signed_convergence_velocity\":%.12f,\"contact_hash\":%s,\"contact_record_count\":%d,\"convergent_contact_count\":%d,\"divergent_contact_count\":%d,\"third_plate_contact_count\":%d,\"label_hash\":%s,\"label_record_count\":%d,\"subducting_label_count\":%d,\"ambiguous_label_count\":%d,\"third_plate_out_of_scope_contact_count\":%d,\"event_id\":%d,\"event_step\":%d,\"filter_decision_hash\":%s,\"raw_multi_hit_sample_count\":%d,\"raw_miss_sample_count\":%d,\"raw_third_plate_sample_count\":%d,\"subducting_label_input_count\":%d,\"ambiguous_label_input_count\":%d,\"third_plate_label_input_count\":%d,\"filtered_candidate_count\":%d,\"filtered_sample_count\":%d,\"post_filter_single_hit_sample_count\":%d,\"post_filter_multi_hit_sample_count\":%d,\"post_filter_non_boundary_multi_hit_sample_count\":%d,\"unresolved_multi_hit_sample_count\":%d,\"filter_exhausted_sample_count\":%d,\"gap_fill_count\":%d,\"non_separating_gap_fill_count\":%d,\"decisions_from_third_plate_label_count\":%d,\"unexpected_filtered_plate_count\":%d,\"invalid_decision_trace_count\":%d,\"auth_caf_before\":%.12f,\"auth_caf_after\":%.12f,\"projected_caf_before\":%.12f,\"projected_caf_after\":%.12f,\"max_plate_area_delta_percent\":%.12f,\"max_plate_area_delta_plate_id\":%d,\"filter_seconds\":%.12f,\"event_seconds\":%.12f,\"projection_hash_before\":%s,\"projection_hash_after\":%s,\"state_hash_before\":%s,\"state_hash_after\":%s,\"memory_gb\":%.12f}"),
			*JsonString(Result.FixtureName),
			Result.Replay,
			Result.StepCount,
			Result.SampleCount,
			Result.PlateCount,
			Result.PairSignedConvergenceVelocity,
			*JsonString(Contact.ContactLogHash),
			Contact.ContactRecordCount,
			Contact.ConvergentContactCount,
			Contact.DivergentContactCount,
			Contact.ThirdPlateContactCount,
			*JsonString(Label.TriangleLabelHash),
			Label.LabelRecordCount,
			Label.SubductingLabelCount,
			Label.AmbiguousLabelCount,
			Label.ThirdPlateOutOfScopeContactCount,
			Filter.EventId,
			Filter.Step,
			*JsonString(Filter.FilterDecisionHash),
			Filter.RawMultiHitSampleCount,
			Filter.RawMissSampleCount,
			Filter.RawThirdPlateSampleCount,
			Filter.SubductingLabelInputCount,
			Filter.AmbiguousLabelInputCount,
			Filter.ThirdPlateLabelInputCount,
			Filter.FilteredCandidateCount,
			Filter.FilteredSampleCount,
			Filter.PostFilterSingleHitSampleCount,
			Filter.PostFilterMultiHitSampleCount,
			Filter.PostFilterNonBoundaryMultiHitSampleCount,
			Filter.UnresolvedMultiHitSampleCount,
			Filter.FilterExhaustedSampleCount,
			Filter.GapFillCount,
			Filter.NonSeparatingGapFillCount,
			Filter.DecisionsFromThirdPlateLabelCount,
			Result.UnexpectedFilteredPlateCount,
			Result.InvalidDecisionTraceCount,
			Filter.AuthoritativeCAFBefore,
			Filter.AuthoritativeCAFAfter,
			Filter.ProjectedCAFBefore,
			Filter.ProjectedCAFAfter,
			Filter.MaxPlateAreaDeltaPercent,
			Filter.MaxPlateAreaDeltaPlateId,
			Filter.FilterSeconds,
			Filter.ResampleEventSeconds,
			*JsonString(Filter.ProjectionHashBefore),
			*JsonString(Filter.ProjectionHashAfter),
			*JsonString(Filter.StateHashBefore),
			*JsonString(Filter.StateHashAfter),
			MemoryGb);
	}

	bool RunFixtureReplay(const FSlice3FixtureConfig& Config, const int32 Replay, FSlice3ReplayResult& OutResult)
	{
		OutResult = FSlice3ReplayResult();
		OutResult.FixtureName = Config.Name;
		OutResult.Replay = Replay;
		OutResult.StepCount = Config.StepCount;
		OutResult.SampleCount = Config.SampleCount;
		OutResult.PlateCount = Config.PlateCount;

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 3 fixture %s replay %d could not find a world."), *Config.Name, Replay);
			return false;
		}

		ACarrierLabVisualizationActor* Actor = SpawnSlice3Actor(*World, Config);
		if (Actor == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 3 fixture %s replay %d could not spawn actor."), *Config.Name, Replay);
			return false;
		}
		if (!Actor->InitializeCarrier())
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 3 fixture %s replay %d failed initialization."), *Config.Name, Replay);
			Actor->Destroy();
			return false;
		}

		Actor->ConfigurePhaseIIMotionFixture(Config.Fixture);
		if (Config.bCheckPairSign)
		{
			OutResult.PairSignedConvergenceVelocity = Actor->ComputePhaseIIPairSignedConvergenceVelocity(0, 1);
		}
		for (int32 Step = 0; Step < Config.StepCount; ++Step)
		{
			Actor->StepOnce();
		}

		TArray<FCarrierLabPhaseIIContactRecord> Contacts;
		if (!Actor->DetectPhaseIIContacts(Contacts, OutResult.ContactMetrics))
		{
			Actor->Destroy();
			return false;
		}

		FCarrierLabPhaseIITriangleLabelConfig LabelConfig;
		LabelConfig.bUseFixturePolarity = Config.bUseFixturePolarity;
		LabelConfig.FixtureUnderPlate = Config.FixtureUnderPlate;
		LabelConfig.FixtureOverPlate = Config.FixtureOverPlate;

		TArray<FCarrierLabPhaseIITriangleLabelRecord> Labels;
		if (!Actor->BuildPhaseIITriangleLabels(Contacts, LabelConfig, Labels, OutResult.LabelMetrics))
		{
			Actor->Destroy();
			return false;
		}

		TArray<FCarrierLabPhaseIIFilterDecisionRecord> Decisions;
		if (!Actor->ApplyPhaseIIResamplingFilterEvent(Labels, Decisions, OutResult.FilterMetrics))
		{
			Actor->Destroy();
			return false;
		}
		OutResult.ProjectionMetrics = Actor->CurrentMetrics;

		for (const FCarrierLabPhaseIIContactRecord& Contact : Contacts)
		{
			OutResult.ContactJsonl += ContactRecordJson(Config.Name, Replay, Contact) + LINE_TERMINATOR;
			OutResult.CanonicalContactJsonl += ContactRecordJson(Config.Name, -1, Contact) + LINE_TERMINATOR;
		}
		for (const FCarrierLabPhaseIITriangleLabelRecord& Label : Labels)
		{
			OutResult.LabelJsonl += LabelRecordJson(Config.Name, Replay, Label) + LINE_TERMINATOR;
			OutResult.CanonicalLabelJsonl += LabelRecordJson(Config.Name, -1, Label) + LINE_TERMINATOR;
		}
		for (const FCarrierLabPhaseIIFilterDecisionRecord& Decision : Decisions)
		{
			OutResult.DecisionJsonl += DecisionRecordJson(Config.Name, Replay, Decision) + LINE_TERMINATOR;
			OutResult.CanonicalDecisionJsonl += DecisionRecordJson(Config.Name, -1, Decision) + LINE_TERMINATOR;
			if (Decision.DecisionClass == ECarrierLabPhaseIIFilterDecisionClass::CandidateFiltered &&
				Config.ExpectedFilteredPlate != INDEX_NONE &&
				Decision.FilteredPlateId != Config.ExpectedFilteredPlate)
			{
				++OutResult.UnexpectedFilteredPlateCount;
			}
			if (Decision.DecisionClass == ECarrierLabPhaseIIFilterDecisionClass::CandidateFiltered &&
				(Decision.FilteredPlateId == INDEX_NONE ||
					Decision.FilteredLocalTriangleId == INDEX_NONE ||
					Decision.SourceContactId == INDEX_NONE ||
					Decision.SourceLabelId == INDEX_NONE))
			{
				++OutResult.InvalidDecisionTraceCount;
			}
		}

		OutResult.bCompleted = true;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return true;
	}

	FSlice3FixtureSummary SummarizeFixture(const FSlice3FixtureConfig& Config, const FSlice3ReplayResult& A, const FSlice3ReplayResult& B)
	{
		FSlice3FixtureSummary Summary;
		Summary.FixtureName = Config.Name;
		Summary.SampleCount = Config.SampleCount;
		Summary.PlateCount = Config.PlateCount;
		Summary.StepCount = Config.StepCount;
		Summary.bCompleted = A.bCompleted && B.bCompleted;
		Summary.ContactA = A.ContactMetrics;
		Summary.LabelA = A.LabelMetrics;
		Summary.FilterA = A.FilterMetrics;
		Summary.PairSignedConvergenceA = A.PairSignedConvergenceVelocity;
		Summary.PairSignedConvergenceB = B.PairSignedConvergenceVelocity;
		Summary.UnexpectedFilteredPlateA = A.UnexpectedFilteredPlateCount;
		Summary.UnexpectedFilteredPlateB = B.UnexpectedFilteredPlateCount;
		Summary.InvalidDecisionTraceA = A.InvalidDecisionTraceCount;
		Summary.InvalidDecisionTraceB = B.InvalidDecisionTraceCount;

		Summary.bContactReplayHash = A.bCompleted && B.bCompleted && A.ContactMetrics.ContactLogHash == B.ContactMetrics.ContactLogHash && A.CanonicalContactJsonl == B.CanonicalContactJsonl;
		Summary.bLabelReplayHash = A.bCompleted && B.bCompleted && A.LabelMetrics.TriangleLabelHash == B.LabelMetrics.TriangleLabelHash && A.CanonicalLabelJsonl == B.CanonicalLabelJsonl;
		Summary.bDecisionReplayHash = A.bCompleted && B.bCompleted && A.FilterMetrics.FilterDecisionHash == B.FilterMetrics.FilterDecisionHash;
		Summary.bDecisionLogByteMatch = A.bCompleted && B.bCompleted && A.CanonicalDecisionJsonl == B.CanonicalDecisionJsonl;
		Summary.bPostStateHashMatch = A.bCompleted && B.bCompleted && A.FilterMetrics.StateHashAfter == B.FilterMetrics.StateHashAfter;
		const double AuthDeltaA = FMath::Abs(A.FilterMetrics.AuthoritativeCAFAfter - A.FilterMetrics.AuthoritativeCAFBefore);
		const double AuthDeltaB = FMath::Abs(B.FilterMetrics.AuthoritativeCAFAfter - B.FilterMetrics.AuthoritativeCAFBefore);
		Summary.bAuthCafGate = AuthDeltaA <= 0.02 && AuthDeltaB <= 0.02;
		Summary.bNoThirdPlateConsumption =
			A.FilterMetrics.DecisionsFromThirdPlateLabelCount == 0 &&
			B.FilterMetrics.DecisionsFromThirdPlateLabelCount == 0 &&
			A.LabelMetrics.LabelsFromThirdPlateContactCount == 0 &&
			B.LabelMetrics.LabelsFromThirdPlateContactCount == 0;
		Summary.bNoFilteredControlGate = !Config.bExpectNoFilteredCandidates ||
			(A.FilterMetrics.FilteredCandidateCount == 0 && B.FilterMetrics.FilteredCandidateCount == 0);
		Summary.bFilteredExpectedGate = !Config.bExpectFilteredCandidates ||
			(A.FilterMetrics.FilteredCandidateCount > 0 && B.FilterMetrics.FilteredCandidateCount > 0);
		Summary.bExpectedFilteredPlateGate = Config.ExpectedFilteredPlate == INDEX_NONE ||
			(A.UnexpectedFilteredPlateCount == 0 && B.UnexpectedFilteredPlateCount == 0);
		Summary.bMixedSignalGate = !Config.bExpectMixedSignal ||
			(A.ContactMetrics.ConvergentContactCount > 0 && A.ContactMetrics.DivergentContactCount > 0 &&
				B.ContactMetrics.ConvergentContactCount > 0 && B.ContactMetrics.DivergentContactCount > 0 &&
				A.FilterMetrics.FilteredCandidateCount > 0 && B.FilterMetrics.FilteredCandidateCount > 0);
		if (Config.bExpectMixedSignal)
		{
			const int32 ExpectedRemainderA = FMath::Max(0, A.FilterMetrics.RawMultiHitSampleCount - A.FilterMetrics.FilteredSampleCount);
			const int32 ExpectedRemainderB = FMath::Max(0, B.FilterMetrics.RawMultiHitSampleCount - B.FilterMetrics.FilteredSampleCount);
			Summary.bPostFilterMultiGate =
				A.FilterMetrics.FilteredCandidateCount > 0 &&
				B.FilterMetrics.FilteredCandidateCount > 0 &&
				A.FilterMetrics.UnresolvedMultiHitSampleCount <= ExpectedRemainderA + 1 &&
				B.FilterMetrics.UnresolvedMultiHitSampleCount <= ExpectedRemainderB + 1;
		}
		else if (Config.bExpectFilteredCandidates && Config.PlateCount == 2)
		{
			const int32 ExpectedRemainderA = FMath::Max(0, A.FilterMetrics.RawMultiHitSampleCount - A.FilterMetrics.FilteredSampleCount);
			const int32 ExpectedRemainderB = FMath::Max(0, B.FilterMetrics.RawMultiHitSampleCount - B.FilterMetrics.FilteredSampleCount);
			Summary.bPostFilterMultiGate =
				A.FilterMetrics.FilteredCandidateCount > 0 &&
				B.FilterMetrics.FilteredCandidateCount > 0 &&
				A.FilterMetrics.UnresolvedMultiHitSampleCount <= ExpectedRemainderA + 1 &&
				B.FilterMetrics.UnresolvedMultiHitSampleCount <= ExpectedRemainderB + 1;
		}
		else
		{
			const double NonBoundaryRateA = A.SampleCount > 0 ? static_cast<double>(A.FilterMetrics.PostFilterNonBoundaryMultiHitSampleCount) / static_cast<double>(A.SampleCount) : 0.0;
			const double NonBoundaryRateB = B.SampleCount > 0 ? static_cast<double>(B.FilterMetrics.PostFilterNonBoundaryMultiHitSampleCount) / static_cast<double>(B.SampleCount) : 0.0;
			const bool bExplainedA = A.LabelMetrics.ThirdPlateOutOfScopeContactCount > 0 || A.LabelMetrics.SameMaterialAmbiguousContactCount > 0;
			const bool bExplainedB = B.LabelMetrics.ThirdPlateOutOfScopeContactCount > 0 || B.LabelMetrics.SameMaterialAmbiguousContactCount > 0;
			Summary.bPostFilterMultiGate = (NonBoundaryRateA < 0.02 && NonBoundaryRateB < 0.02) || (bExplainedA && bExplainedB);
		}
		Summary.bTraceGate =
			A.InvalidDecisionTraceCount == 0 &&
			B.InvalidDecisionTraceCount == 0 &&
			A.FilterMetrics.ThirdPlateLabelInputCount == 0 &&
			B.FilterMetrics.ThirdPlateLabelInputCount == 0;
		Summary.bPairSignGate = !Config.bCheckPairSign ||
			(Config.ExpectedPairSign > 0.0 && A.PairSignedConvergenceVelocity > 1.0e-6 && B.PairSignedConvergenceVelocity > 1.0e-6) ||
			(Config.ExpectedPairSign < 0.0 && A.PairSignedConvergenceVelocity < -1.0e-6 && B.PairSignedConvergenceVelocity < -1.0e-6) ||
			(Config.ExpectedPairSign == 0.0 && FMath::Abs(A.PairSignedConvergenceVelocity) <= 1.0e-6 && FMath::Abs(B.PairSignedConvergenceVelocity) <= 1.0e-6);

		if (Config.bExpectThirdPlateExplanation)
		{
			Summary.Notes = TEXT("Residual unresolved overlap is expected to be third-plate or ambiguous process evidence, not centroid fallback.");
		}
		else if (Config.bExpectMixedSignal)
		{
			Summary.Notes = TEXT("Same plate pair intentionally includes convergent and divergent evidence; only locally labeled convergent triangles can filter.");
		}
		else if (Config.bExpectNoFilteredCandidates)
		{
			Summary.Notes = TEXT("Control fixture should produce no subducting-triangle filter decisions.");
		}
		else if (Config.ExpectedFilteredPlate != INDEX_NONE)
		{
			Summary.Notes = FString::Printf(TEXT("Fixture polarity expects filtered/subducting plate %d."), Config.ExpectedFilteredPlate);
		}
		return Summary;
	}

	bool SummaryPasses(const FSlice3FixtureSummary& Summary)
	{
		return Summary.bCompleted &&
			Summary.bContactReplayHash &&
			Summary.bLabelReplayHash &&
			Summary.bDecisionReplayHash &&
			Summary.bDecisionLogByteMatch &&
			Summary.bPostStateHashMatch &&
			Summary.bAuthCafGate &&
			Summary.bNoThirdPlateConsumption &&
			Summary.bNoFilteredControlGate &&
			Summary.bFilteredExpectedGate &&
			Summary.bExpectedFilteredPlateGate &&
			Summary.bMixedSignalGate &&
			Summary.bPostFilterMultiGate &&
			Summary.bTraceGate &&
			Summary.bPairSignGate;
	}

	FString BuildReport(const FString& OutputRoot, const TArray<FSlice3FixtureSummary>& Summaries)
	{
		FString Report = TEXT("# Phase II Slice 3 Checkpoint: Resampling Filter Integration\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This checkpoint consumes only Slice 2 `subducting` triangle labels inside the thesis resampling filter hook. It does not consume ambiguous labels, third-plate evidence, or centroid/random policies in the primary path. Samples that remain multi-hit after filtering are reported as unresolved; samples whose candidates are all filtered are reported as filter-exhausted rather than gap-filled.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Fixture | Samples | Plates | Steps | Contact replay | Label replay | Decision replay | Decision log | Post-state replay | Auth CAF | Third-plate isolation | No-filter control | Expected filter | Filtered plate | Mixed signal | Post-filter multi | Trace | Sign | Verdict |\n");
		Report += TEXT("|---|---:|---:|---:|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|\n");
		for (const FSlice3FixtureSummary& Summary : Summaries)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s | %s |\n"),
				*Summary.FixtureName,
				Summary.SampleCount,
				Summary.PlateCount,
				Summary.StepCount,
				Summary.bContactReplayHash ? TEXT("pass") : TEXT("fail"),
				Summary.bLabelReplayHash ? TEXT("pass") : TEXT("fail"),
				Summary.bDecisionReplayHash ? TEXT("pass") : TEXT("fail"),
				Summary.bDecisionLogByteMatch ? TEXT("pass") : TEXT("fail"),
				Summary.bPostStateHashMatch ? TEXT("pass") : TEXT("fail"),
				Summary.bAuthCafGate ? TEXT("pass") : TEXT("fail"),
				Summary.bNoThirdPlateConsumption ? TEXT("pass") : TEXT("fail"),
				Summary.bNoFilteredControlGate ? TEXT("pass") : TEXT("fail"),
				Summary.bFilteredExpectedGate ? TEXT("pass") : TEXT("fail"),
				Summary.bExpectedFilteredPlateGate ? TEXT("pass") : TEXT("fail"),
				Summary.bMixedSignalGate ? TEXT("pass") : TEXT("fail"),
				Summary.bPostFilterMultiGate ? TEXT("pass") : TEXT("investigate"),
				Summary.bTraceGate ? TEXT("pass") : TEXT("fail"),
				Summary.bPairSignGate ? TEXT("pass") : TEXT("fail"),
				SummaryPasses(Summary) ? TEXT("pass") : TEXT("investigate"));
		}

		Report += TEXT("\n## Filter Metrics\n\n");
		Report += TEXT("| Fixture | Contacts | Conv | Div | Third | Labels | Subducting labels | Ambiguous labels | Filtered candidates | Filtered samples | Raw multi | Post multi | Post non-boundary multi | Unresolved | Exhausted | Gaps | Auth CAF before | Auth CAF after | Proj CAF before | Proj CAF after | Max area delta % | Event s | Decision hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FSlice3FixtureSummary& Summary : Summaries)
		{
			const FCarrierLabPhaseIIContactMetrics& Contact = Summary.ContactA;
			const FCarrierLabPhaseIITriangleLabelMetrics& Label = Summary.LabelA;
			const FCarrierLabPhaseIIResamplingFilterMetrics& Filter = Summary.FilterA;
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %.12f | %.12f | %.12f | %.12f | %.6f | %.6f | `%s` |\n"),
				*Summary.FixtureName,
				Contact.ContactRecordCount,
				Contact.ConvergentContactCount,
				Contact.DivergentContactCount,
				Contact.ThirdPlateContactCount,
				Label.LabelRecordCount,
				Label.SubductingLabelCount,
				Label.AmbiguousLabelCount,
				Filter.FilteredCandidateCount,
				Filter.FilteredSampleCount,
				Filter.RawMultiHitSampleCount,
				Filter.PostFilterMultiHitSampleCount,
				Filter.PostFilterNonBoundaryMultiHitSampleCount,
				Filter.UnresolvedMultiHitSampleCount,
				Filter.FilterExhaustedSampleCount,
				Filter.GapFillCount,
				Filter.AuthoritativeCAFBefore,
				Filter.AuthoritativeCAFAfter,
				Filter.ProjectedCAFBefore,
				Filter.ProjectedCAFAfter,
				Filter.MaxPlateAreaDeltaPercent,
				Filter.ResampleEventSeconds,
				*Filter.FilterDecisionHash);
		}

		Report += TEXT("\n## Directional Motion Probe\n\n");
		Report += TEXT("| Fixture | Signed convergence replay A | Signed convergence replay B | Interpretation |\n");
		Report += TEXT("|---|---:|---:|---|\n");
		for (const FSlice3FixtureSummary& Summary : Summaries)
		{
			if (Summary.PairSignedConvergenceA == 0.0 && Summary.PairSignedConvergenceB == 0.0)
			{
				continue;
			}
			Report += FString::Printf(
				TEXT("| %s | %.12f | %.12f | %s |\n"),
				*Summary.FixtureName,
				Summary.PairSignedConvergenceA,
				Summary.PairSignedConvergenceB,
				Summary.PairSignedConvergenceA > 0.0 ? TEXT("converging") : TEXT("separating"));
		}

		Report += TEXT("\n## Notes\n\n");
		for (const FSlice3FixtureSummary& Summary : Summaries)
		{
			if (!Summary.Notes.IsEmpty())
			{
				Report += FString::Printf(TEXT("- `%s`: %s\n"), *Summary.FixtureName, *Summary.Notes);
			}
		}
		Report += TEXT("- Slice 3's primary path does not call `ChooseNearestCandidatePlate` for post-filter multi-hit samples. Those samples are reported as unresolved instead of being silently policy-resolved.\n");
		Report += TEXT("- `filter_decisions.jsonl` is replay-sufficient evidence for every consumed subducting-triangle label and every unresolved/filter-exhausted sample.\n");
		Report += TEXT("- The cadence 60k Auth CAF gate remains failed, consistent with Stage 1.5's accepted foundation-characterization finding: q1/q2 gap material transfer changes authoritative CAF even when convergent filtering is explicit. Slice 3 improves the convergent filter surface; it does not resolve material accounting.\n");
		Report += TEXT("- 250k scaling remains deferred to Slice 5; this Slice 3 checkpoint is the cadence-faithful 60k integration gate plus focused controls.\n");

		bool bAllPass = true;
		for (const FSlice3FixtureSummary& Summary : Summaries)
		{
			bAllPass &= SummaryPasses(Summary);
		}
		Report += TEXT("\n## Recommendation\n\n");
		Report += bAllPass
			? TEXT("Go for user review of Phase II Slice 3. Explicit subducting-triangle labels are consumed deterministically in the resampling filter hook, controls remain isolated, and residual unresolved multi-hit samples are reported rather than hidden behind centroid policy. Do not advance to Slice 4 until the user records explicit go/no-go.\n")
			: TEXT("Pause before Slice 4. One or more Slice 3 gates require investigation before material accounting is layered on top.\n");
		return Report;
	}
}

UCarrierLabPhaseIISlice3Commandlet::UCarrierLabPhaseIISlice3Commandlet()
{
	IsClient = false;
	IsEditor = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIISlice3Commandlet::Main(const FString& Params)
{
	const int32 DefaultSteps = FMath::Max(1, ParseIntParam(Params, TEXT("Steps="), 32));
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	const TArray<FSlice3FixtureConfig> Fixtures = {
		{TEXT("cadence_60k_primary"), 60000, 40, DefaultSteps, 0.30, ECarrierLabPhaseIIMotionFixture::Default, false, INDEX_NONE, INDEX_NONE, false, false, false, true, false, 0.0, INDEX_NONE},
		{TEXT("forced_convergence_under_1"), 10000, 2, 40, 1.00, ECarrierLabPhaseIIMotionFixture::ForcedConvergence, true, 1, 0, false, true, false, false, true, 1.0, 1},
		{TEXT("forced_convergence_under_0"), 10000, 2, 40, 1.00, ECarrierLabPhaseIIMotionFixture::ForcedConvergence, true, 0, 1, false, true, false, false, true, 1.0, 0},
		{TEXT("forced_divergence_step0"), 10000, 2, 0, 0.50, ECarrierLabPhaseIIMotionFixture::ForcedDivergence, false, INDEX_NONE, INDEX_NONE, true, false, false, false, true, -1.0, INDEX_NONE},
		{TEXT("same_pair_mixed_signal"), 10000, 2, 40, 1.00, ECarrierLabPhaseIIMotionFixture::ForcedDivergence, true, 1, 0, false, true, true, false, true, -1.0, 1},
		{TEXT("zero_motion"), 10000, 40, DefaultSteps, 0.30, ECarrierLabPhaseIIMotionFixture::Zero, false, INDEX_NONE, INDEX_NONE, true, false, false, false, false, 0.0, INDEX_NONE},
		{TEXT("single_plate"), 10000, 1, DefaultSteps, 0.30, ECarrierLabPhaseIIMotionFixture::Default, false, INDEX_NONE, INDEX_NONE, true, false, false, false, false, 0.0, INDEX_NONE}
	};

	TArray<FSlice3ReplayResult> ReplayResults;
	TArray<FSlice3FixtureSummary> Summaries;
	bool bAllRunsCompleted = true;
	for (const FSlice3FixtureConfig& Fixture : Fixtures)
	{
		UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 3: fixture=%s samples=%d plates=%d steps=%d fixture_motion=%s"),
			*Fixture.Name,
			Fixture.SampleCount,
			Fixture.PlateCount,
			Fixture.StepCount,
			FixtureName(Fixture.Fixture));
		FSlice3ReplayResult A;
		FSlice3ReplayResult B;
		bAllRunsCompleted &= RunFixtureReplay(Fixture, 0, A);
		bAllRunsCompleted &= RunFixtureReplay(Fixture, 1, B);
		ReplayResults.Add(A);
		ReplayResults.Add(B);
		Summaries.Add(SummarizeFixture(Fixture, A, B));
	}

	FString MetricsJsonl;
	FString ContactsJsonl;
	FString LabelsJsonl;
	FString DecisionsJsonl;
	for (const FSlice3ReplayResult& Result : ReplayResults)
	{
		MetricsJsonl += ReplayMetricJson(Result) + LINE_TERMINATOR;
		ContactsJsonl += Result.ContactJsonl;
		LabelsJsonl += Result.LabelJsonl;
		DecisionsJsonl += Result.DecisionJsonl;
	}

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	const FString ContactsPath = FPaths::Combine(OutputRoot, TEXT("contacts.jsonl"));
	const FString LabelsPath = FPaths::Combine(OutputRoot, TEXT("labels.jsonl"));
	const FString DecisionsPath = FPaths::Combine(OutputRoot, TEXT("filter_decisions.jsonl"));
	if (!FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath) ||
		!FFileHelper::SaveStringToFile(ContactsJsonl, *ContactsPath) ||
		!FFileHelper::SaveStringToFile(LabelsJsonl, *LabelsPath) ||
		!FFileHelper::SaveStringToFile(DecisionsJsonl, *DecisionsPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write one or more Slice 3 JSONL artifacts under %s."), *OutputRoot);
		return 1;
	}

	const FString Report = BuildReport(OutputRoot, Summaries);
	const FString ReportPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("docs"), TEXT("checkpoints"), TEXT("phase-ii-slice-3-report.md"));
	if (!FFileHelper::SaveStringToFile(Report, *ReportPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write Slice 3 report: %s"), *ReportPath);
		return 1;
	}

	bool bAllPass = bAllRunsCompleted;
	for (const FSlice3FixtureSummary& Summary : Summaries)
	{
		bAllPass &= SummaryPasses(Summary);
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 3 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 3 contacts: %s"), *ContactsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 3 labels: %s"), *LabelsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 3 decisions: %s"), *DecisionsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 3 report: %s"), *ReportPath);
	return bAllPass ? 0 : 2;
}
