// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierFoundationStepperActor.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "IndexTypes.h"

using UE::Geometry::FDynamicMesh3;
using UE::Geometry::FDynamicMeshColorOverlay;
using UE::Geometry::FIndex3i;

namespace
{
	FVector4f ToVector4f(const FLinearColor& Color)
	{
		return FVector4f(Color.R, Color.G, Color.B, Color.A);
	}

	FString StepName(const ECarrierFoundationStepperStep Step)
	{
		switch (Step)
		{
		case ECarrierFoundationStepperStep::ColdStart:
			return TEXT("Cold Start");
		case ECarrierFoundationStepperStep::RigidMotion:
			return TEXT("Rigid Motion");
		case ECarrierFoundationStepperStep::ContactCandidates:
			return TEXT("Contact Candidates");
		case ECarrierFoundationStepperStep::ProcessMarking:
			return TEXT("Process Marking");
		case ECarrierFoundationStepperStep::FilteredSampling:
			return TEXT("Filtered Global Sampling");
		case ECarrierFoundationStepperStep::Q1Q2GapFill:
			return TEXT("Q1/Q2 Gap Fill");
		case ECarrierFoundationStepperStep::TopologyRebuild:
			return TEXT("Topology Rebuild");
		case ECarrierFoundationStepperStep::ProcessReset:
			return TEXT("Process Reset");
		default:
			return TEXT("Unknown");
		}
	}

	FString LayerName(const ECarrierFoundationStepperLayer Layer)
	{
		switch (Layer)
		{
		case ECarrierFoundationStepperLayer::StepDefault:
			return TEXT("Step Default");
		case ECarrierFoundationStepperLayer::PlateAssignment:
			return TEXT("Plate Assignment");
		case ECarrierFoundationStepperLayer::ContactCandidates:
			return TEXT("Contact Candidates");
		case ECarrierFoundationStepperLayer::ProcessMarks:
			return TEXT("Process Marks");
		case ECarrierFoundationStepperLayer::FilteredSampling:
			return TEXT("Filtered Sampling");
		case ECarrierFoundationStepperLayer::Q1Q2GapFill:
			return TEXT("Q1/Q2 Gap Fill");
		case ECarrierFoundationStepperLayer::RebuiltTopology:
			return TEXT("Rebuilt Topology");
		case ECarrierFoundationStepperLayer::ProcessReset:
			return TEXT("Process Reset");
		case ECarrierFoundationStepperLayer::ForbiddenFallbacks:
			return TEXT("Forbidden Fallbacks");
		case ECarrierFoundationStepperLayer::ReplayDeterminism:
			return TEXT("Replay Determinism");
		default:
			return TEXT("Unknown");
		}
	}
}

ACarrierFoundationStepperActor::ACarrierFoundationStepperActor()
{
	PrimaryActorTick.bCanEverTick = false;

	MeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("FoundationMesh"));
	RootComponent = MeshComponent;
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetColorOverrideMode(EDynamicMeshComponentColorOverrideMode::VertexColors);
	MeshComponent->SetVertexColorSpaceTransformMode(EDynamicMeshVertexColorTransformMode::NoTransform);
	MeshComponent->SetTwoSided(true);
	MeshComponent->SetEnableFlatShading(true);
	MeshComponent->SetEnableWireframeRenderPass(bShowWireframeOverlay);

	StatusText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("StatusText"));
	StatusText->SetupAttachment(RootComponent);
	StatusText->SetHorizontalAlignment(EHTA_Center);
	StatusText->SetVerticalAlignment(EVRTA_TextTop);
	StatusText->SetWorldSize(StatusTextWorldSize);
	StatusText->SetRelativeLocation(FVector(0.0, 0.0, SphereRadius * 1.22));
}

void ACarrierFoundationStepperActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bAutoBuildOnConstruction)
	{
		const int32 ClampedScale = FMath::Clamp(InspectableScaleSampleCount, 128, 50000);
		const bool bConfigChanged =
			!bSnapshotValid ||
			CachedFixture != Fixture ||
			CachedInspectableScaleSampleCount != ClampedScale;
		if (bConfigChanged)
		{
			RebuildFoundationSnapshot();
			return;
		}
	}

	RebuildRenderMesh();
	UpdateMetricsFromSnapshot();
	UpdateStatusText();
}

void ACarrierFoundationStepperActor::BeginPlay()
{
	Super::BeginPlay();

	if (!bSnapshotValid)
	{
		RebuildFoundationSnapshot();
	}
}

#if WITH_EDITOR
void ACarrierFoundationStepperActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property != nullptr
		? PropertyChangedEvent.Property->GetFName()
		: NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(ACarrierFoundationStepperActor, Fixture) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ACarrierFoundationStepperActor, InspectableScaleSampleCount))
	{
		RebuildFoundationSnapshot();
		return;
	}

	bRenderMeshTopologyDirty = true;
	RebuildRenderMesh();
	UpdateMetricsFromSnapshot();
	UpdateStatusText();
}
#endif

CarrierLab::V2::FCarrierV2Stage5Config ACarrierFoundationStepperActor::BuildConfigForFixture() const
{
	if (Fixture == ECarrierFoundationStepperFixture::InspectableScale)
	{
		const int32 SampleCount = FMath::Clamp(InspectableScaleSampleCount, 128, 50000);
		CarrierLab::V2::FCarrierV2Stage5Config Config = CarrierLab::V2::FCarrierV2Stage5::MakeScaleConfig(SampleCount, false);
		Config.FixtureId = FString::Printf(TEXT("INSPECT-%d-Q1Q2-REBUILD"), SampleCount);
		Config.FixtureName = FString::Printf(TEXT("Inspectable%dQ1Q2Rebuild"), SampleCount);
		return Config;
	}

	const FString WantedFixtureId = Fixture == ECarrierFoundationStepperFixture::FX014
		? TEXT("FX-014")
		: TEXT("FX-015");
	const TArray<CarrierLab::V2::FCarrierV2Stage5Config> Configs = CarrierLab::V2::FCarrierV2Stage5::MakeMicroFixtureConfigs();
	for (const CarrierLab::V2::FCarrierV2Stage5Config& Config : Configs)
	{
		if (Config.FixtureId == WantedFixtureId)
		{
			return Config;
		}
	}
	return Configs.IsEmpty()
		? CarrierLab::V2::FCarrierV2Stage5::MakeScaleConfig(FMath::Clamp(InspectableScaleSampleCount, 128, 50000), false)
		: Configs.Last();
}

void ACarrierFoundationStepperActor::RebuildFoundationSnapshot()
{
	InspectableScaleSampleCount = FMath::Clamp(InspectableScaleSampleCount, 128, 50000);
	CachedFixture = Fixture;
	CachedInspectableScaleSampleCount = InspectableScaleSampleCount;
	LastError.Reset();

	const CarrierLab::V2::FCarrierV2Stage5Config Config = BuildConfigForFixture();
	bSnapshotValid = CarrierLab::V2::FCarrierV2FoundationStepper::BuildSnapshot(Config, Snapshot);
	LastSnapshotSummary = Snapshot.Summary;
	if (!bSnapshotValid)
	{
		LastError = Snapshot.Error;
	}

	bRenderMeshTopologyDirty = true;
	UpdateMetricsFromSnapshot();
	RebuildRenderMesh();
	UpdateStatusText();
}

void ACarrierFoundationStepperActor::StepForward()
{
	const int32 NextStep = FMath::Min(
		static_cast<int32>(CurrentStep) + 1,
		static_cast<int32>(ECarrierFoundationStepperStep::ProcessReset));
	CurrentStep = static_cast<ECarrierFoundationStepperStep>(NextStep);
	RebuildRenderMesh();
	UpdateMetricsFromSnapshot();
	UpdateStatusText();
}

void ACarrierFoundationStepperActor::StepBackward()
{
	const int32 PreviousStep = FMath::Max(static_cast<int32>(CurrentStep) - 1, 0);
	CurrentStep = static_cast<ECarrierFoundationStepperStep>(PreviousStep);
	RebuildRenderMesh();
	UpdateMetricsFromSnapshot();
	UpdateStatusText();
}

void ACarrierFoundationStepperActor::ResetToColdStart()
{
	CurrentStep = ECarrierFoundationStepperStep::ColdStart;
	VisualizationLayer = ECarrierFoundationStepperLayer::StepDefault;
	RebuildRenderMesh();
	UpdateMetricsFromSnapshot();
	UpdateStatusText();
}

void ACarrierFoundationStepperActor::UseFX014()
{
	Fixture = ECarrierFoundationStepperFixture::FX014;
	RebuildFoundationSnapshot();
}

void ACarrierFoundationStepperActor::UseFX015()
{
	Fixture = ECarrierFoundationStepperFixture::FX015;
	RebuildFoundationSnapshot();
}

void ACarrierFoundationStepperActor::UseInspectableScale()
{
	Fixture = ECarrierFoundationStepperFixture::InspectableScale;
	RebuildFoundationSnapshot();
}

void ACarrierFoundationStepperActor::UpdateMetricsFromSnapshot()
{
	Metrics = FCarrierFoundationStepperActorMetrics();
	Metrics.StepName = StepName(CurrentStep);
	Metrics.LayerName = LayerName(EffectiveLayer());

	if (!bSnapshotValid)
	{
		Metrics.Verdict = TEXT("NO_SNAPSHOT");
		return;
	}

	const CarrierLab::V2::FCarrierV2Stage5Metrics& M = Snapshot.Stage5Result.Metrics;
	Metrics.FixtureId = M.FixtureId;
	Metrics.Verdict = M.Verdict;
	Metrics.SampleCount = M.GlobalSampleCount;
	Metrics.TriangleCount = M.GlobalTriangleCount;
	Metrics.ContactCandidateCount = Snapshot.Stage5Result.Stage3Result.Stage2Result.Metrics.ContactCandidateCount;
	Metrics.ProcessEventCount = Snapshot.Stage5Result.Stage3Result.Metrics.ProcessEventCount;
	Metrics.PreResetTriangleMarkCount = M.PreResetTriangleMarkCount;
	Metrics.PostResetTriangleMarkCount = M.PostResetTriangleMarkCount;
	Metrics.ZeroValidHitCount = M.ZeroValidHitCount;
	Metrics.GeneratedOceanicCount = M.GeneratedOceanicCount;
	Metrics.RebuiltTriangleAssignmentCount = M.RebuiltTriangleAssignmentCount;
	Metrics.UnassignedTriangleCount = M.UnassignedTriangleCount;
	Metrics.bFixturePass = M.bFixturePass;
	Metrics.bReplayDeterministic = M.bReplayDeterministic;
	Metrics.bForbiddenFallbackDetected = Snapshot.bForbiddenFallbackDetected;
}

void ACarrierFoundationStepperActor::UpdateStatusText()
{
	if (!StatusText)
	{
		return;
	}

	StatusText->SetWorldSize(StatusTextWorldSize);
	StatusText->SetRelativeLocation(FVector(0.0, 0.0, SphereRadius * 1.22));
	StatusText->SetText(FText::FromString(BuildStatusText()));
}

FString ACarrierFoundationStepperActor::BuildStatusText() const
{
	if (!bSnapshotValid)
	{
		return LastError.IsEmpty()
			? TEXT("Carrier Foundation Stepper\nNo snapshot")
			: FString::Printf(TEXT("Carrier Foundation Stepper\nNo snapshot\n%s"), *LastError);
	}

	const CarrierLab::V2::FCarrierV2Stage5Metrics& M = Snapshot.Stage5Result.Metrics;
	return FString::Printf(
		TEXT("Carrier Foundation Stepper\n%s | Step: %s | Layer: %s\npass=%s replay=%s verdict=%s\nsamples=%d triangles=%d contacts=%d events=%d marks=%d/%d zero=%d generated=%d rebuilt=%d/%d forbidden=%s"),
		*M.FixtureId,
		*StepName(CurrentStep),
		*LayerName(EffectiveLayer()),
		M.bFixturePass ? TEXT("true") : TEXT("false"),
		M.bReplayDeterministic ? TEXT("true") : TEXT("false"),
		*M.Verdict,
		M.GlobalSampleCount,
		M.GlobalTriangleCount,
		Snapshot.Stage5Result.Stage3Result.Stage2Result.Metrics.ContactCandidateCount,
		Snapshot.Stage5Result.Stage3Result.Metrics.ProcessEventCount,
		M.PreResetTriangleMarkCount,
		M.PostResetTriangleMarkCount,
		M.ZeroValidHitCount,
		M.GeneratedOceanicCount,
		M.RebuiltTriangleAssignmentCount,
		M.GlobalTriangleCount,
		Snapshot.bForbiddenFallbackDetected ? TEXT("true") : TEXT("false"));
}

ECarrierFoundationStepperLayer ACarrierFoundationStepperActor::EffectiveLayer() const
{
	if (VisualizationLayer != ECarrierFoundationStepperLayer::StepDefault)
	{
		return VisualizationLayer;
	}

	switch (CurrentStep)
	{
	case ECarrierFoundationStepperStep::ColdStart:
		return ECarrierFoundationStepperLayer::PlateAssignment;
	case ECarrierFoundationStepperStep::RigidMotion:
	case ECarrierFoundationStepperStep::ContactCandidates:
		return ECarrierFoundationStepperLayer::ContactCandidates;
	case ECarrierFoundationStepperStep::ProcessMarking:
		return ECarrierFoundationStepperLayer::ProcessMarks;
	case ECarrierFoundationStepperStep::FilteredSampling:
		return ECarrierFoundationStepperLayer::FilteredSampling;
	case ECarrierFoundationStepperStep::Q1Q2GapFill:
		return ECarrierFoundationStepperLayer::Q1Q2GapFill;
	case ECarrierFoundationStepperStep::TopologyRebuild:
		return ECarrierFoundationStepperLayer::RebuiltTopology;
	case ECarrierFoundationStepperStep::ProcessReset:
		return ECarrierFoundationStepperLayer::ProcessReset;
	default:
		return ECarrierFoundationStepperLayer::PlateAssignment;
	}
}

void ACarrierFoundationStepperActor::RebuildRenderMesh()
{
	if (!bSnapshotValid)
	{
		return;
	}

	BuildRenderMeshTopology();
}

bool ACarrierFoundationStepperActor::BuildRenderMeshTopology()
{
	if (!MeshComponent || !bSnapshotValid || Snapshot.Samples.IsEmpty() || Snapshot.Triangles.IsEmpty())
	{
		return false;
	}

	FDynamicMesh3 Mesh;
	Mesh.EnableTriangleGroups();
	Mesh.EnableAttributes();
	Mesh.Attributes()->EnablePrimaryColors();
	FDynamicMeshColorOverlay* Colors = Mesh.Attributes()->PrimaryColors();

	TMap<int32, int32> VertexBySampleId;
	VertexBySampleId.Reserve(Snapshot.Samples.Num());
	TMap<int32, int32> SampleIndexById;
	SampleIndexById.Reserve(Snapshot.Samples.Num());
	for (const CarrierLab::V2::FCarrierV2FoundationStepperSampleVisual& Sample : Snapshot.Samples)
	{
		const int32 VertexId = Mesh.AppendVertex(Sample.UnitPosition * SphereRadius);
		VertexBySampleId.Add(Sample.SampleId, VertexId);
		SampleIndexById.Add(Sample.SampleId, SampleIndexById.Num());
	}

	for (const CarrierLab::V2::FCarrierV2FoundationStepperTriangleVisual& Triangle : Snapshot.Triangles)
	{
		const int32* VertexA = VertexBySampleId.Find(Triangle.SampleIds[0]);
		const int32* VertexB = VertexBySampleId.Find(Triangle.SampleIds[1]);
		const int32* VertexC = VertexBySampleId.Find(Triangle.SampleIds[2]);
		if (VertexA == nullptr || VertexB == nullptr || VertexC == nullptr)
		{
			continue;
		}

		const int32* SampleIndexA = SampleIndexById.Find(Triangle.SampleIds[0]);
		const int32* SampleIndexB = SampleIndexById.Find(Triangle.SampleIds[1]);
		const int32* SampleIndexC = SampleIndexById.Find(Triangle.SampleIds[2]);
		if (SampleIndexA == nullptr || SampleIndexB == nullptr || SampleIndexC == nullptr)
		{
			continue;
		}

		const int32 TriangleId = Mesh.AppendTriangle(*VertexA, *VertexB, *VertexC, 0);
		if (TriangleId < 0)
		{
			continue;
		}

		const int32 ColorA = Colors->AppendElement(ToVector4f(ColorForTriangleCorner(Triangle, Snapshot.Samples[*SampleIndexA])));
		const int32 ColorB = Colors->AppendElement(ToVector4f(ColorForTriangleCorner(Triangle, Snapshot.Samples[*SampleIndexB])));
		const int32 ColorC = Colors->AppendElement(ToVector4f(ColorForTriangleCorner(Triangle, Snapshot.Samples[*SampleIndexC])));
		Colors->SetParentVertex(ColorA, *VertexA);
		Colors->SetParentVertex(ColorB, *VertexB);
		Colors->SetParentVertex(ColorC, *VertexC);
		Colors->SetTriangle(TriangleId, FIndex3i(ColorA, ColorB, ColorC));
	}

	MeshComponent->SetMesh(MoveTemp(Mesh));
	MeshComponent->SetColorOverrideMode(EDynamicMeshComponentColorOverrideMode::VertexColors);
	MeshComponent->SetVertexColorSpaceTransformMode(EDynamicMeshVertexColorTransformMode::NoTransform);
	MeshComponent->SetTwoSided(true);
	MeshComponent->SetEnableFlatShading(true);
	MeshComponent->SetEnableWireframeRenderPass(bShowWireframeOverlay);
	CachedRenderMeshSampleCount = Snapshot.Samples.Num();
	CachedRenderMeshTriangleCount = Snapshot.Triangles.Num();
	bRenderMeshTopologyDirty = false;
	return true;
}

FLinearColor ACarrierFoundationStepperActor::ColorForTriangleCorner(
	const CarrierLab::V2::FCarrierV2FoundationStepperTriangleVisual& Triangle,
	const CarrierLab::V2::FCarrierV2FoundationStepperSampleVisual& Sample) const
{
	const ECarrierFoundationStepperLayer Layer = EffectiveLayer();
	switch (Layer)
	{
	case ECarrierFoundationStepperLayer::PlateAssignment:
		return ColorForPlate(Sample.ColdStartPlateId);

	case ECarrierFoundationStepperLayer::ContactCandidates:
		if (Sample.RawHitCount == 0)
		{
			return FLinearColor(0.52f, 0.08f, 0.06f, 1.0f);
		}
		if (Sample.RawHitCount > 1)
		{
			return FLinearColor(0.98f, 0.55f, 0.12f, 1.0f);
		}
		return FLinearColor(0.20f, 0.72f, 0.44f, 1.0f);

	case ECarrierFoundationStepperLayer::ProcessMarks:
		if (Triangle.bSubductingProcessMarked || Sample.bSubductingProcessMarked)
		{
			return FLinearColor(0.95f, 0.12f, 0.09f, 1.0f);
		}
		if (Triangle.bCollidingProcessMarked || Sample.bCollidingProcessMarked)
		{
			return FLinearColor(0.70f, 0.20f, 0.92f, 1.0f);
		}
		if (Triangle.bProcessMarked || Sample.bProcessMarked)
		{
			return FLinearColor(0.95f, 0.78f, 0.18f, 1.0f);
		}
		return FLinearColor(0.22f, 0.24f, 0.26f, 1.0f);

	case ECarrierFoundationStepperLayer::FilteredSampling:
		if (Sample.FilteredSubductingHitCount > 0)
		{
			return FLinearColor(0.92f, 0.18f, 0.13f, 1.0f);
		}
		if (Sample.FilteredCollidingHitCount > 0)
		{
			return FLinearColor(0.58f, 0.22f, 0.78f, 1.0f);
		}
		if (Sample.ValidHitCount > 0)
		{
			return FLinearColor(0.18f, 0.68f, 0.38f, 1.0f);
		}
		return Sample.bZeroValidHit
			? FLinearColor(0.12f, 0.36f, 0.82f, 1.0f)
			: FLinearColor(0.18f, 0.20f, 0.22f, 1.0f);

	case ECarrierFoundationStepperLayer::Q1Q2GapFill:
		if (Sample.bGeneratedOceanic)
		{
			return FLinearColor(0.08f, 0.86f, 0.92f, 1.0f);
		}
		if (Sample.bZeroValidHit && !Sample.bBoundaryPairFound)
		{
			return FLinearColor(0.95f, 0.08f, 0.08f, 1.0f);
		}
		if (Sample.bBoundaryPairFound)
		{
			return FLinearColor(0.18f, 0.68f, 0.66f, 1.0f);
		}
		return FLinearColor(0.20f, 0.24f, 0.28f, 1.0f);

	case ECarrierFoundationStepperLayer::RebuiltTopology:
		if (Triangle.bUnassignedAfterRebuild)
		{
			return FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
		}
		if (Triangle.bMixedVertexAssignment)
		{
			return FLinearColor::LerpUsingHSV(ColorForPlate(Triangle.RebuiltPlateId), FLinearColor::White, 0.32f);
		}
		return ColorForPlate(Triangle.RebuiltPlateId);

	case ECarrierFoundationStepperLayer::ProcessReset:
		if (Snapshot.Stage5Result.Metrics.PostResetTriangleMarkCount > 0)
		{
			return FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);
		}
		return Triangle.bProcessMarked || Sample.bProcessMarked
			? FLinearColor(0.16f, 0.86f, 0.48f, 1.0f)
			: FLinearColor(0.24f, 0.26f, 0.28f, 1.0f);

	case ECarrierFoundationStepperLayer::ForbiddenFallbacks:
		return Snapshot.bForbiddenFallbackDetected
			? FLinearColor(1.0f, 0.0f, 0.0f, 1.0f)
			: FLinearColor(0.12f, 0.72f, 0.38f, 1.0f);

	case ECarrierFoundationStepperLayer::ReplayDeterminism:
		return Snapshot.bReplayDeterministic
			? FLinearColor(0.12f, 0.72f, 0.38f, 1.0f)
			: FLinearColor(1.0f, 0.0f, 0.0f, 1.0f);

	default:
		return ColorForPlate(Sample.ColdStartPlateId);
	}
}

FLinearColor ACarrierFoundationStepperActor::ColorForPlate(const int32 PlateId) const
{
	static const FLinearColor Palette[] =
	{
		FLinearColor(0.16f, 0.48f, 0.82f, 1.0f),
		FLinearColor(0.86f, 0.30f, 0.20f, 1.0f),
		FLinearColor(0.20f, 0.66f, 0.36f, 1.0f),
		FLinearColor(0.92f, 0.70f, 0.18f, 1.0f),
		FLinearColor(0.58f, 0.32f, 0.78f, 1.0f),
		FLinearColor(0.10f, 0.68f, 0.70f, 1.0f),
		FLinearColor(0.90f, 0.46f, 0.16f, 1.0f),
		FLinearColor(0.74f, 0.22f, 0.46f, 1.0f)
	};

	if (PlateId == INDEX_NONE)
	{
		return FLinearColor(0.08f, 0.08f, 0.08f, 1.0f);
	}
	return Palette[FMath::Abs(PlateId) % UE_ARRAY_COUNT(Palette)];
}
