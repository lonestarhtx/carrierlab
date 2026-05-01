// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabPhaseIISlice1Commandlet.h"

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

	FString GetOutputRoot(const FString& Params)
	{
		FString OutputRoot;
		if (!FParse::Value(*Params, TEXT("Out="), OutputRoot))
		{
			const FString Stamp = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutputRoot = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CarrierLab"), TEXT("PhaseII"), TEXT("Slice1"), Stamp);
		}
		return FPaths::ConvertRelativePathToFull(OutputRoot);
	}

	int32 ParseIntParam(const FString& Params, const TCHAR* Key, const int32 DefaultValue)
	{
		int32 Value = DefaultValue;
		FParse::Value(*Params, Key, Value);
		return Value;
	}

	struct FSlice1FixtureConfig
	{
		FString Name;
		int32 SampleCount = 10000;
		int32 PlateCount = 40;
		int32 StepCount = 20;
		ECarrierLabPhaseIIMotionFixture Fixture = ECarrierLabPhaseIIMotionFixture::Default;
		bool bExpectNoSubduction = false;
		bool bExpectConvergentContacts = false;
		bool bExpectThirdPlate = false;
		bool bCheckPairSign = false;
		int32 PairA = 0;
		int32 PairB = 1;
		double ExpectedPairSign = 0.0;
	};

	struct FSlice1ReplayResult
	{
		FString FixtureName;
		int32 Replay = 0;
		int32 StepCount = 0;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 Seed = 42;
		double PairSignedConvergenceVelocity = 0.0;
		FCarrierLabVisualizationMotion MotionA;
		FCarrierLabVisualizationMotion MotionB;
		FCarrierLabPhaseIIContactMetrics ContactMetrics;
		FCarrierLabVisualizationMetrics ProjectionMetrics;
		FString ProjectionHashBeforeContact;
		FString StateHashBeforeContact;
		bool bProjectionHashUnchanged = false;
		bool bStateHashUnchanged = false;
		FString ContactJsonl;
		FString CanonicalContactJsonl;
		bool bCompleted = false;
	};

	struct FSlice1FixtureSummary
	{
		FString FixtureName;
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 StepCount = 0;
		bool bReplayContactHashMatch = false;
		bool bReplayContactLogByteMatch = false;
		bool bNoMutation = false;
		bool bNoSubductionGate = true;
		bool bConvergenceGate = true;
		bool bThirdPlateGate = true;
		bool bPairSignGate = true;
		bool bCompleted = false;
		double PairSignedConvergenceA = 0.0;
		double PairSignedConvergenceB = 0.0;
		FCarrierLabPhaseIIContactMetrics MetricsA;
		FCarrierLabPhaseIIContactMetrics MetricsB;
		FString ContactHashA;
		FString ContactHashB;
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

	ACarrierLabVisualizationActor* SpawnSlice1Actor(UWorld& World, const FSlice1FixtureConfig& Config)
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
		Actor->ContinentalPlateFraction = 0.30;
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

	FString ReplayMetricJson(const FSlice1ReplayResult& Result)
	{
		const FCarrierLabPhaseIIContactMetrics& Contact = Result.ContactMetrics;
		const FCarrierLabVisualizationMetrics& Projection = Result.ProjectionMetrics;
		const double MemoryGb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0 * 1024.0);
		return FString::Printf(
			TEXT("{\"fixture\":%s,\"replay\":%d,\"step\":%d,\"step_count\":%d,\"sample_count\":%d,\"plate_count\":%d,\"seed\":%d,\"pair_signed_convergence_velocity\":%.12f,\"raw_evidence_sample_count\":%d,\"contact_record_count\":%d,\"convergent_contact_count\":%d,\"divergent_contact_count\":%d,\"transform_low_margin_contact_count\":%d,\"third_plate_contact_count\":%d,\"subduction_candidate_count\":%d,\"boundary_evidence_count\":%d,\"contact_nan_or_inf_count\":%d,\"contact_detection_seconds\":%.12f,\"contact_log_hash\":%s,\"projection_hash_before_contact\":%s,\"projection_hash_after_contact\":%s,\"state_hash_before_contact\":%s,\"state_hash_after_contact\":%s,\"projection_hash_unchanged\":%s,\"state_hash_unchanged\":%s,\"projected_miss_count\":%d,\"projected_multi_hit_count\":%d,\"authoritative_caf\":%.12f,\"projected_caf\":%.12f,\"drift_mean_km\":%.12f,\"drift_p95_km\":%.12f,\"memory_gb\":%.12f}"),
			*JsonString(Result.FixtureName),
			Result.Replay,
			Contact.Step,
			Result.StepCount,
			Result.SampleCount,
			Result.PlateCount,
			Result.Seed,
			Result.PairSignedConvergenceVelocity,
			Contact.RawEvidenceSampleCount,
			Contact.ContactRecordCount,
			Contact.ConvergentContactCount,
			Contact.DivergentContactCount,
			Contact.TransformLowMarginContactCount,
			Contact.ThirdPlateContactCount,
			Contact.SubductionCandidateCount,
			Contact.BoundaryEvidenceCount,
			Contact.NaNOrInfCount,
			Contact.ContactDetectionSeconds,
			*JsonString(Contact.ContactLogHash),
			*JsonString(Result.ProjectionHashBeforeContact),
			*JsonString(Projection.LastHash),
			*JsonString(Result.StateHashBeforeContact),
			*JsonString(Projection.StateHash),
			Result.bProjectionHashUnchanged ? TEXT("true") : TEXT("false"),
			Result.bStateHashUnchanged ? TEXT("true") : TEXT("false"),
			Projection.RawMissCount,
			Projection.RawMultiHitCount,
			Projection.AuthoritativeCAF,
			Projection.ProjectedCAF,
			Projection.DriftErrorMeanKm,
			Projection.DriftErrorP95Km,
			MemoryGb);
	}

	bool RunFixtureReplay(const FSlice1FixtureConfig& Config, const int32 Replay, FSlice1ReplayResult& OutResult)
	{
		OutResult = FSlice1ReplayResult();
		OutResult.FixtureName = Config.Name;
		OutResult.Replay = Replay;
		OutResult.StepCount = Config.StepCount;
		OutResult.SampleCount = Config.SampleCount;
		OutResult.PlateCount = Config.PlateCount;
		OutResult.Seed = 42;

		UWorld* World = GetCommandletWorld();
		if (World == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 1 contact fixture %s replay %d could not find a world."), *Config.Name, Replay);
			return false;
		}

		ACarrierLabVisualizationActor* Actor = SpawnSlice1Actor(*World, Config);
		if (Actor == nullptr)
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 1 contact fixture %s replay %d could not spawn actor."), *Config.Name, Replay);
			return false;
		}

		if (!Actor->InitializeCarrier())
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 1 contact fixture %s replay %d failed initialization."), *Config.Name, Replay);
			Actor->Destroy();
			return false;
		}

		Actor->ConfigurePhaseIIMotionFixture(Config.Fixture);
		if (Config.bCheckPairSign)
		{
			OutResult.PairSignedConvergenceVelocity = Actor->ComputePhaseIIPairSignedConvergenceVelocity(Config.PairA, Config.PairB);
			Actor->GetPhaseIIMotion(Config.PairA, OutResult.MotionA);
			Actor->GetPhaseIIMotion(Config.PairB, OutResult.MotionB);
		}

		for (int32 StepIndex = 0; StepIndex < Config.StepCount; ++StepIndex)
		{
			Actor->StepOnce();
		}

		OutResult.ProjectionHashBeforeContact = Actor->CurrentMetrics.LastHash;
		OutResult.StateHashBeforeContact = Actor->CurrentMetrics.StateHash;

		TArray<FCarrierLabPhaseIIContactRecord> Contacts;
		if (!Actor->DetectPhaseIIContacts(Contacts, OutResult.ContactMetrics))
		{
			UE_LOG(LogTemp, Error, TEXT("Slice 1 contact fixture %s replay %d failed contact detection."), *Config.Name, Replay);
			Actor->Destroy();
			return false;
		}

		OutResult.ProjectionMetrics = Actor->CurrentMetrics;
		OutResult.bProjectionHashUnchanged = OutResult.ProjectionHashBeforeContact == Actor->CurrentMetrics.LastHash;
		OutResult.bStateHashUnchanged = OutResult.StateHashBeforeContact == Actor->CurrentMetrics.StateHash;
		for (const FCarrierLabPhaseIIContactRecord& Contact : Contacts)
		{
			OutResult.ContactJsonl += ContactRecordJson(Config.Name, Replay, Contact);
			OutResult.ContactJsonl += LINE_TERMINATOR;
			OutResult.CanonicalContactJsonl += ContactRecordJson(Config.Name, -1, Contact);
			OutResult.CanonicalContactJsonl += LINE_TERMINATOR;
		}
		OutResult.bCompleted = true;
		Actor->Destroy();
		CollectGarbage(RF_NoFlags);
		return true;
	}

	FSlice1FixtureSummary SummarizeFixture(const FSlice1FixtureConfig& Config, const FSlice1ReplayResult& A, const FSlice1ReplayResult& B)
	{
		FSlice1FixtureSummary Summary;
		Summary.FixtureName = Config.Name;
		Summary.SampleCount = Config.SampleCount;
		Summary.PlateCount = Config.PlateCount;
		Summary.StepCount = Config.StepCount;
		Summary.bCompleted = A.bCompleted && B.bCompleted;
		Summary.MetricsA = A.ContactMetrics;
		Summary.MetricsB = B.ContactMetrics;
		Summary.ContactHashA = A.ContactMetrics.ContactLogHash;
		Summary.ContactHashB = B.ContactMetrics.ContactLogHash;
		Summary.PairSignedConvergenceA = A.PairSignedConvergenceVelocity;
		Summary.PairSignedConvergenceB = B.PairSignedConvergenceVelocity;
		Summary.bReplayContactHashMatch = A.bCompleted && B.bCompleted && A.ContactMetrics.ContactLogHash == B.ContactMetrics.ContactLogHash;
		Summary.bReplayContactLogByteMatch = A.bCompleted && B.bCompleted && A.CanonicalContactJsonl == B.CanonicalContactJsonl;
		Summary.bNoMutation = A.bProjectionHashUnchanged && A.bStateHashUnchanged && B.bProjectionHashUnchanged && B.bStateHashUnchanged;
		Summary.bNoSubductionGate = !Config.bExpectNoSubduction || (A.ContactMetrics.SubductionCandidateCount == 0 && B.ContactMetrics.SubductionCandidateCount == 0);
		Summary.bConvergenceGate = !Config.bExpectConvergentContacts || (A.ContactMetrics.SubductionCandidateCount > 0 && B.ContactMetrics.SubductionCandidateCount > 0);
		Summary.bThirdPlateGate = !Config.bExpectThirdPlate || (A.ContactMetrics.ThirdPlateContactCount > 0 && B.ContactMetrics.ThirdPlateContactCount > 0);
		Summary.bPairSignGate = !Config.bCheckPairSign ||
			(Config.ExpectedPairSign > 0.0 && A.PairSignedConvergenceVelocity > 1.0e-6 && B.PairSignedConvergenceVelocity > 1.0e-6) ||
			(Config.ExpectedPairSign < 0.0 && A.PairSignedConvergenceVelocity < -1.0e-6 && B.PairSignedConvergenceVelocity < -1.0e-6) ||
			(Config.ExpectedPairSign == 0.0 && FMath::Abs(A.PairSignedConvergenceVelocity) <= 1.0e-6 && FMath::Abs(B.PairSignedConvergenceVelocity) <= 1.0e-6);
		if (Config.Fixture == ECarrierLabPhaseIIMotionFixture::ForcedDivergence && Config.StepCount == 0)
		{
			Summary.Notes = TEXT("Divergence gate is evaluated at step 0 to prove sign reversal without the closed-sphere backside convergence stress that belongs in later filter integration.");
		}
		if (Config.Fixture == ECarrierLabPhaseIIMotionFixture::TripleJunction)
		{
			Summary.Notes = TEXT("Third-plate evidence is emitted as explicit third_plate records and excluded from subduction_candidate_count; ordinary two-plate contacts may coexist away from the triple locus.");
		}
		else if (Config.bExpectThirdPlate)
		{
			Summary.Notes = TEXT("This 40-plate default fixture supplies the explicit third-plate intrusion control: third-plate evidence is emitted as third_plate records and is not folded into subduction_candidate_count.");
		}
		return Summary;
	}

	bool SummaryPasses(const FSlice1FixtureSummary& Summary)
	{
		return Summary.bCompleted &&
			Summary.bReplayContactHashMatch &&
			Summary.bReplayContactLogByteMatch &&
			Summary.bNoMutation &&
			Summary.bNoSubductionGate &&
			Summary.bConvergenceGate &&
			Summary.bThirdPlateGate &&
			Summary.bPairSignGate;
	}

	FString BuildReport(const FString& OutputRoot, const TArray<FSlice1FixtureSummary>& Summaries)
	{
		FString Report = TEXT("# Phase II Slice 1 Checkpoint: Contact Detection\n\n");
		Report += FString::Printf(TEXT("Artifacts root: `%s`\n\n"), *OutputRoot);
		Report += TEXT("This checkpoint adds geometry-derived contact detection only. It consumes raw ray-from-origin projection candidates from the current plate-local triangulations and emits deterministic contact records keyed by sample evidence, plate pair, local triangle ids, signed convergence velocity, contact class, and third-plate flag. It does not mutate material, plate topology, projection output, resampling behavior, or triangle process labels.\n\n");
		Report += TEXT("Signed convergence is the negative of the existing signed separation velocity. Positive values mean the two plates move toward each other at the evidence point; negative values mean separation. `subduction_candidate_count` is only the count of non-third-plate convergent contacts above the velocity margin. No triangle is filtered in Slice 1.\n\n");

		Report += TEXT("## Gate Summary\n\n");
		Report += TEXT("| Fixture | Samples | Plates | Steps | Contact hash match | Contact log byte match | No mutation | Sign gate | No-subduction gate | Convergence gate | Third-plate gate | Verdict |\n");
		Report += TEXT("|---|---:|---:|---:|---|---|---|---|---|---|---|---|\n");
		for (const FSlice1FixtureSummary& Summary : Summaries)
		{
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %s | %s | %s | %s | %s | %s | %s | %s |\n"),
				*Summary.FixtureName,
				Summary.SampleCount,
				Summary.PlateCount,
				Summary.StepCount,
				Summary.bReplayContactHashMatch ? TEXT("pass") : TEXT("fail"),
				Summary.bReplayContactLogByteMatch ? TEXT("pass") : TEXT("fail"),
				Summary.bNoMutation ? TEXT("pass") : TEXT("fail"),
				Summary.bPairSignGate ? TEXT("pass") : TEXT("fail"),
				Summary.bNoSubductionGate ? TEXT("pass") : TEXT("fail"),
				Summary.bConvergenceGate ? TEXT("pass") : TEXT("fail"),
				Summary.bThirdPlateGate ? TEXT("pass") : TEXT("fail"),
				SummaryPasses(Summary) ? TEXT("pass") : TEXT("investigate"));
		}

		Report += TEXT("\n## Contact Metrics\n\n");
		Report += TEXT("| Fixture | Raw evidence samples | Records | Convergent | Divergent | Low-margin | Third-plate | Subduction candidates | Boundary evidence | Contact seconds | Hash |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FSlice1FixtureSummary& Summary : Summaries)
		{
			const FCarrierLabPhaseIIContactMetrics& Metrics = Summary.MetricsA;
			Report += FString::Printf(
				TEXT("| %s | %d | %d | %d | %d | %d | %d | %d | %d | %.6f | `%s` |\n"),
				*Summary.FixtureName,
				Metrics.RawEvidenceSampleCount,
				Metrics.ContactRecordCount,
				Metrics.ConvergentContactCount,
				Metrics.DivergentContactCount,
				Metrics.TransformLowMarginContactCount,
				Metrics.ThirdPlateContactCount,
				Metrics.SubductionCandidateCount,
				Metrics.BoundaryEvidenceCount,
				Metrics.ContactDetectionSeconds,
				*Metrics.ContactLogHash);
		}

		Report += TEXT("\n## Directional Motion Probe\n\n");
		Report += TEXT("| Fixture | Signed convergence velocity replay A | Signed convergence velocity replay B | Interpretation |\n");
		Report += TEXT("|---|---:|---:|---|\n");
		for (const FSlice1FixtureSummary& Summary : Summaries)
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
		for (const FSlice1FixtureSummary& Summary : Summaries)
		{
			if (!Summary.Notes.IsEmpty())
			{
				Report += FString::Printf(TEXT("- `%s`: %s\n"), *Summary.FixtureName, *Summary.Notes);
			}
		}
		Report += TEXT("- Contact detection is deliberately not a process labeler. Slice 2 owns polarity and triangle labels; Slice 3 owns resampling filter integration.\n");
		Report += TEXT("- Third-plate contact records are excluded from `subduction_candidate_count` so they cannot silently become ordinary two-plate subduction.\n");

		bool bAllPass = true;
		for (const FSlice1FixtureSummary& Summary : Summaries)
		{
			bAllPass &= SummaryPasses(Summary);
		}
		Report += TEXT("\n## Recommendation\n\n");
		Report += bAllPass
			? TEXT("Go for user review of Phase II Slice 1. The detector is deterministic, sign-aware, non-mutating, and keeps third-plate evidence explicit. Do not advance to Slice 2 until the user records explicit go/no-go.\n")
			: TEXT("No-go for Slice 2. One or more Slice 1 gates failed; investigate before adding polarity or triangle labels.\n");
		return Report;
	}
}

UCarrierLabPhaseIISlice1Commandlet::UCarrierLabPhaseIISlice1Commandlet()
{
	IsClient = false;
	IsEditor = false;
	LogToConsole = true;
}

int32 UCarrierLabPhaseIISlice1Commandlet::Main(const FString& Params)
{
	const int32 DefaultSteps = FMath::Max(1, ParseIntParam(Params, TEXT("Steps="), 40));
	const FString OutputRoot = GetOutputRoot(Params);
	IFileManager::Get().MakeDirectory(*OutputRoot, true);

	const TArray<FSlice1FixtureConfig> Fixtures = {
		{TEXT("default_60k"), 60000, 40, DefaultSteps, ECarrierLabPhaseIIMotionFixture::Default, false, false, true, false, 0, 1, 0.0},
		{TEXT("zero_motion"), 10000, 40, DefaultSteps, ECarrierLabPhaseIIMotionFixture::Zero, true, false, false, true, 0, 1, 0.0},
		{TEXT("single_plate"), 10000, 1, DefaultSteps, ECarrierLabPhaseIIMotionFixture::Default, true, false, false, false, 0, 1, 0.0},
		{TEXT("forced_convergence"), 10000, 2, DefaultSteps, ECarrierLabPhaseIIMotionFixture::ForcedConvergence, false, true, false, true, 0, 1, 1.0},
		{TEXT("forced_divergence"), 10000, 2, 0, ECarrierLabPhaseIIMotionFixture::ForcedDivergence, true, false, false, true, 0, 1, -1.0}
	};

	TArray<FSlice1ReplayResult> ReplayResults;
	TArray<FSlice1FixtureSummary> Summaries;
	bool bAllRunsCompleted = true;

	for (const FSlice1FixtureConfig& Fixture : Fixtures)
	{
		UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 1: fixture=%s samples=%d plates=%d steps=%d"),
			*Fixture.Name,
			Fixture.SampleCount,
			Fixture.PlateCount,
			Fixture.StepCount);
		FSlice1ReplayResult A;
		FSlice1ReplayResult B;
		bAllRunsCompleted &= RunFixtureReplay(Fixture, 0, A);
		bAllRunsCompleted &= RunFixtureReplay(Fixture, 1, B);
		ReplayResults.Add(A);
		ReplayResults.Add(B);
		Summaries.Add(SummarizeFixture(Fixture, A, B));
	}

	FString MetricsJsonl;
	FString ContactsJsonl;
	for (const FSlice1ReplayResult& Result : ReplayResults)
	{
		MetricsJsonl += ReplayMetricJson(Result);
		MetricsJsonl += LINE_TERMINATOR;
		ContactsJsonl += Result.ContactJsonl;
	}

	const FString MetricsPath = FPaths::Combine(OutputRoot, TEXT("metrics.jsonl"));
	if (!FFileHelper::SaveStringToFile(MetricsJsonl, *MetricsPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write Slice 1 metrics: %s"), *MetricsPath);
		return 1;
	}
	const FString ContactsPath = FPaths::Combine(OutputRoot, TEXT("contacts.jsonl"));
	if (!FFileHelper::SaveStringToFile(ContactsJsonl, *ContactsPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write Slice 1 contacts: %s"), *ContactsPath);
		return 1;
	}

	const FString Report = BuildReport(OutputRoot, Summaries);
	const FString ReportPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("docs"), TEXT("checkpoints"), TEXT("phase-ii-slice-1-report.md"));
	if (!FFileHelper::SaveStringToFile(Report, *ReportPath))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to write Slice 1 report: %s"), *ReportPath);
		return 1;
	}

	bool bAllPass = bAllRunsCompleted;
	for (const FSlice1FixtureSummary& Summary : Summaries)
	{
		bAllPass &= SummaryPasses(Summary);
	}

	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 1 metrics: %s"), *MetricsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 1 contacts: %s"), *ContactsPath);
	UE_LOG(LogTemp, Display, TEXT("CarrierLab Phase II Slice 1 report: %s"), *ReportPath);
	return bAllPass ? 0 : 2;
}
