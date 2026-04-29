// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabVisualizationActor.h"

#include "Components/DynamicMeshComponent.h"
#include "Components/InputComponent.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "IndexTypes.h"
#include "InputCoreTypes.h"
#include "Spatial/SpatialInterfaces.h"

using UE::Geometry::FDynamicMesh3;
using UE::Geometry::FDynamicMeshAABBTree3;
using UE::Geometry::FDynamicMeshColorOverlay;
using UE::Geometry::FIndex3i;
using UE::Geometry::IMeshSpatial;

namespace
{
	constexpr double EarthRadiusKm = 6371.0;
	constexpr double DeltaTimeMa = 2.0;
	constexpr double BoundaryBarycentricEpsilon = 1.0e-7;
	constexpr int32 BoundaryLonBins = 72;
	constexpr int32 BoundaryLatBins = 36;
	constexpr int32 BoundarySearchRadiusBins = 2;

	struct FCarrierLabVizCandidate
	{
		int32 PlateId = INDEX_NONE;
		int32 LocalTriangleId = INDEX_NONE;
		FVector3d Bary = FVector3d::ZeroVector;
		double Distance = 0.0;
		bool bBoundary = false;
	};

	struct FCarrierLabVizPlateMesh
	{
		FDynamicMesh3 Mesh;
		TUniquePtr<FDynamicMeshAABBTree3> Tree;
		TMap<int32, int32> MeshTriangleIdToLocalTriangleId;
	};

	struct FCarrierLabVizBoundaryPoint
	{
		int32 PlateId = INDEX_NONE;
		FVector3d UnitPosition = FVector3d::UnitZ();
		double ContinentalFraction = 0.0;
	};

	struct FCarrierLabVizBoundaryIndex
	{
		TArray<FCarrierLabVizBoundaryPoint> Points;
		TMap<int32, TArray<int32>> Bins;
	};

	FVector3d NormalizeOrFallback(const FVector3d& Vector, const FVector3d& Fallback)
	{
		const double Size = Vector.Size();
		return Size > UE_DOUBLE_SMALL_NUMBER ? Vector / Size : Fallback;
	}

	FVector3d RotateVector(const FVector3d& Vector, const FVector3d& Axis, const double AngleRadians)
	{
		const double C = FMath::Cos(AngleRadians);
		const double S = FMath::Sin(AngleRadians);
		return Vector * C + FVector3d::CrossProduct(Axis, Vector) * S + Axis * FVector3d::DotProduct(Axis, Vector) * (1.0 - C);
	}

	double AngularSpeedRadiansPerStep(const double VelocityMmPerYear)
	{
		const double DistanceKmPerStep = VelocityMmPerYear * 1.0e-6 * DeltaTimeMa * 1000000.0;
		return DistanceKmPerStep / EarthRadiusKm;
	}

	bool IsBoundaryHit(const FVector3d& Bary)
	{
		return Bary.X <= BoundaryBarycentricEpsilon || Bary.Y <= BoundaryBarycentricEpsilon || Bary.Z <= BoundaryBarycentricEpsilon;
	}

	int32 CountBits64(uint64 Value)
	{
		int32 Count = 0;
		while (Value != 0)
		{
			Value &= (Value - 1);
			++Count;
		}
		return Count;
	}

	int32 LowestSetBitIndex(const uint64 Value)
	{
		if (Value == 0)
		{
			return INDEX_NONE;
		}
		for (int32 Index = 0; Index < 64; ++Index)
		{
			if ((Value & (1ull << Index)) != 0)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	bool IntersectRayWithTriangle(
		const FVector3d& RayDirection,
		const FVector3d& A,
		const FVector3d& B,
		const FVector3d& C,
		FVector3d& OutBarycentric)
	{
		constexpr double GeometryEpsilon = 1.0e-12;
		const FVector3d Normal = FVector3d::CrossProduct(B - A, C - A);
		const double PlaneDenominator = FVector3d::DotProduct(Normal, RayDirection);
		if (FMath::Abs(PlaneDenominator) <= GeometryEpsilon)
		{
			return false;
		}
		const double T = FVector3d::DotProduct(Normal, A) / PlaneDenominator;
		if (T <= GeometryEpsilon)
		{
			return false;
		}
		const FVector3d Intersection = RayDirection * T;
		const FVector3d V0 = B - A;
		const FVector3d V1 = C - A;
		const FVector3d V2 = Intersection - A;
		const double D00 = FVector3d::DotProduct(V0, V0);
		const double D01 = FVector3d::DotProduct(V0, V1);
		const double D11 = FVector3d::DotProduct(V1, V1);
		const double D20 = FVector3d::DotProduct(V2, V0);
		const double D21 = FVector3d::DotProduct(V2, V1);
		const double Denominator = D00 * D11 - D01 * D01;
		if (FMath::Abs(Denominator) <= GeometryEpsilon)
		{
			return false;
		}
		const double V = (D11 * D20 - D01 * D21) / Denominator;
		const double W = (D00 * D21 - D01 * D20) / Denominator;
		const double U = 1.0 - V - W;
		constexpr double BarycentricEpsilon = 1.0e-10;
		if (U < -BarycentricEpsilon || V < -BarycentricEpsilon || W < -BarycentricEpsilon)
		{
			return false;
		}
		OutBarycentric = FVector3d(U, V, W);
		return true;
	}

	bool BuildPlateRayMeshes(const CarrierLab::FCarrierState& State, TArray<FCarrierLabVizPlateMesh>& OutPlateMeshes, FString& OutError)
	{
		OutPlateMeshes.Reset(State.Plates.Num());
		OutPlateMeshes.SetNum(State.Plates.Num());
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			if (!OutPlateMeshes.IsValidIndex(Plate.PlateId))
			{
				OutError = FString::Printf(TEXT("Invalid plate id %d while building visualization ray meshes."), Plate.PlateId);
				return false;
			}

			FCarrierLabVizPlateMesh& PlateMesh = OutPlateMeshes[Plate.PlateId];
			PlateMesh.Mesh.EnableTriangleGroups();
			for (const CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
			{
				PlateMesh.Mesh.AppendVertex(Vertex.UnitPosition);
			}
			for (int32 LocalTriangleId = 0; LocalTriangleId < Plate.LocalTriangles.Num(); ++LocalTriangleId)
			{
				const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[LocalTriangleId];
				const int32 MeshTriangleId = PlateMesh.Mesh.AppendTriangle(Triangle.A, Triangle.B, Triangle.C, 0);
				if (MeshTriangleId < 0)
				{
					OutError = FString::Printf(TEXT("FDynamicMesh3 rejected visualization plate %d local triangle %d."), Plate.PlateId, LocalTriangleId);
					return false;
				}
				PlateMesh.MeshTriangleIdToLocalTriangleId.Add(MeshTriangleId, LocalTriangleId);
			}
			PlateMesh.Tree = MakeUnique<FDynamicMeshAABBTree3>(&PlateMesh.Mesh, true);
		}
		return true;
	}

	double InterpolateContinentalFraction(const CarrierLab::FCarrierPlate& Plate, const FCarrierLabVizCandidate& Candidate)
	{
		if (!Plate.LocalTriangles.IsValidIndex(Candidate.LocalTriangleId))
		{
			return 0.0;
		}
		const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[Candidate.LocalTriangleId];
		if (!Plate.Vertices.IsValidIndex(Triangle.A) || !Plate.Vertices.IsValidIndex(Triangle.B) || !Plate.Vertices.IsValidIndex(Triangle.C))
		{
			return 0.0;
		}
		return FMath::Clamp(
			Candidate.Bary.X * Plate.Vertices[Triangle.A].ContinentalFraction +
			Candidate.Bary.Y * Plate.Vertices[Triangle.B].ContinentalFraction +
			Candidate.Bary.Z * Plate.Vertices[Triangle.C].ContinentalFraction,
			0.0,
			1.0);
	}

	void QuerySampleCandidates(
		const CarrierLab::FCarrierState& State,
		const TArray<FCarrierLabVizPlateMesh>& PlateMeshes,
		const CarrierLab::FSphereSample& Sample,
		TArray<FCarrierLabVizCandidate>& Candidates,
		uint64& PlateMask,
		bool& bAnyBoundary)
	{
		Candidates.Reset();
		PlateMask = 0;
		bAnyBoundary = false;

		IMeshSpatial::FQueryOptions QueryOptions(2.0 + 1.0e-6);
		TArray<MeshIntersection::FHitIntersectionResult> Hits;
		const FRay3d Ray(FVector3d::Zero(), Sample.UnitPosition);
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			if (!PlateMeshes.IsValidIndex(Plate.PlateId) || !PlateMeshes[Plate.PlateId].Tree.IsValid())
			{
				continue;
			}

			Hits.Reset();
			PlateMeshes[Plate.PlateId].Tree->FindAllHitTriangles(Ray, Hits, QueryOptions);
			bool bAcceptedHit = false;
			for (const MeshIntersection::FHitIntersectionResult& Hit : Hits)
			{
				if (Hit.Distance <= 0.0 || Hit.Distance > 2.0 + 1.0e-6)
				{
					continue;
				}
				const int32* LocalTriangleId = PlateMeshes[Plate.PlateId].MeshTriangleIdToLocalTriangleId.Find(Hit.TriangleId);
				if (LocalTriangleId == nullptr)
				{
					continue;
				}

				FCarrierLabVizCandidate Candidate;
				Candidate.PlateId = Plate.PlateId;
				Candidate.LocalTriangleId = *LocalTriangleId;
				Candidate.Bary = Hit.BaryCoords;
				Candidate.Distance = Hit.Distance;
				Candidate.bBoundary = IsBoundaryHit(Hit.BaryCoords);
				bAnyBoundary = bAnyBoundary || Candidate.bBoundary;
				PlateMask |= (1ull << static_cast<uint64>(Plate.PlateId));
				Candidates.Add(Candidate);
				bAcceptedHit = true;
			}

			if (!bAcceptedHit)
			{
				double NearestDistSqr = TNumericLimits<double>::Max();
				const int32 NearestTriangleId = PlateMeshes[Plate.PlateId].Tree->FindNearestTriangle(Sample.UnitPosition, NearestDistSqr, IMeshSpatial::FQueryOptions(1.0e-8));
				const int32* LocalTriangleId = PlateMeshes[Plate.PlateId].MeshTriangleIdToLocalTriangleId.Find(NearestTriangleId);
				if (LocalTriangleId != nullptr && NearestDistSqr <= 1.0e-16)
				{
					const CarrierLab::FCarrierPlateTriangle& Triangle = Plate.LocalTriangles[*LocalTriangleId];
					FVector3d Bary = FVector3d(1.0, 0.0, 0.0);
					if (Plate.Vertices.IsValidIndex(Triangle.A) &&
						Plate.Vertices.IsValidIndex(Triangle.B) &&
						Plate.Vertices.IsValidIndex(Triangle.C))
					{
						if (!IntersectRayWithTriangle(
							Sample.UnitPosition,
							Plate.Vertices[Triangle.A].UnitPosition,
							Plate.Vertices[Triangle.B].UnitPosition,
							Plate.Vertices[Triangle.C].UnitPosition,
							Bary))
						{
							const double DotA = FVector3d::DotProduct(Sample.UnitPosition, Plate.Vertices[Triangle.A].UnitPosition);
							const double DotB = FVector3d::DotProduct(Sample.UnitPosition, Plate.Vertices[Triangle.B].UnitPosition);
							const double DotC = FVector3d::DotProduct(Sample.UnitPosition, Plate.Vertices[Triangle.C].UnitPosition);
							Bary = DotB >= DotA && DotB >= DotC ? FVector3d(0.0, 1.0, 0.0) :
								(DotC >= DotA && DotC >= DotB ? FVector3d(0.0, 0.0, 1.0) : FVector3d(1.0, 0.0, 0.0));
						}
					}

					FCarrierLabVizCandidate Candidate;
					Candidate.PlateId = Plate.PlateId;
					Candidate.LocalTriangleId = *LocalTriangleId;
					Candidate.Bary = Bary;
					Candidate.Distance = 1.0;
					Candidate.bBoundary = true;
					bAnyBoundary = true;
					PlateMask |= (1ull << static_cast<uint64>(Plate.PlateId));
					Candidates.Add(Candidate);
				}
			}
		}
	}

	int32 ChooseNearestCandidatePlate(
		const CarrierLab::FSphereSample& Sample,
		const TArray<FCarrierLabVizCandidate>& Candidates,
		const TArray<FCarrierLabVisualizationMotion>& Motions)
	{
		int32 BestPlateId = INDEX_NONE;
		double BestDot = -TNumericLimits<double>::Max();
		for (const FCarrierLabVizCandidate& Candidate : Candidates)
		{
			if (!Motions.IsValidIndex(Candidate.PlateId))
			{
				continue;
			}
			const double Dot = FVector3d::DotProduct(Sample.UnitPosition, Motions[Candidate.PlateId].CurrentCenter);
			if (Dot > BestDot + 1.0e-12 ||
				(FMath::IsNearlyEqual(Dot, BestDot, 1.0e-12) && (BestPlateId == INDEX_NONE || Candidate.PlateId < BestPlateId)))
			{
				BestDot = Dot;
				BestPlateId = Candidate.PlateId;
			}
		}
		return BestPlateId;
	}

	int32 BoundaryBinKey(const FVector3d& UnitPosition)
	{
		const double Lon = FMath::Atan2(UnitPosition.Y, UnitPosition.X);
		const double Lat = FMath::Asin(FMath::Clamp(UnitPosition.Z, -1.0, 1.0));
		const int32 LonIndex = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble((Lon + UE_DOUBLE_PI) / (2.0 * UE_DOUBLE_PI) * BoundaryLonBins)), 0, BoundaryLonBins - 1);
		const int32 LatIndex = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble((Lat + 0.5 * UE_DOUBLE_PI) / UE_DOUBLE_PI * BoundaryLatBins)), 0, BoundaryLatBins - 1);
		return LatIndex * BoundaryLonBins + LonIndex;
	}

	void AddBoundaryPoint(FCarrierLabVizBoundaryIndex& Index, const int32 PlateId, const FVector3d& Position, const double ContinentalFraction)
	{
		FCarrierLabVizBoundaryPoint Point;
		Point.PlateId = PlateId;
		Point.UnitPosition = NormalizeOrFallback(Position, FVector3d::UnitZ());
		Point.ContinentalFraction = FMath::Clamp(ContinentalFraction, 0.0, 1.0);
		const int32 PointIndex = Index.Points.Add(Point);
		Index.Bins.FindOrAdd(BoundaryBinKey(Point.UnitPosition)).Add(PointIndex);
	}

	FCarrierLabVizBoundaryIndex BuildBoundaryIndex(const CarrierLab::FCarrierState& State)
	{
		FCarrierLabVizBoundaryIndex Index;
		for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
		{
			for (const CarrierLab::FCarrierPlateTriangle& Triangle : Plate.LocalTriangles)
			{
				if (!Plate.Vertices.IsValidIndex(Triangle.A) || !Plate.Vertices.IsValidIndex(Triangle.B) || !Plate.Vertices.IsValidIndex(Triangle.C))
				{
					continue;
				}
				const int32 Vertices[3] = {Triangle.A, Triangle.B, Triangle.C};
				for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
				{
					const CarrierLab::FCarrierVertex& A = Plate.Vertices[Vertices[EdgeIndex]];
					const CarrierLab::FCarrierVertex& B = Plate.Vertices[Vertices[(EdgeIndex + 1) % 3]];
					if (State.Samples.IsValidIndex(A.GlobalSampleId) &&
						State.Samples.IsValidIndex(B.GlobalSampleId) &&
						State.Samples[A.GlobalSampleId].PlateId != State.Samples[B.GlobalSampleId].PlateId)
					{
						AddBoundaryPoint(Index, Plate.PlateId, A.UnitPosition, A.ContinentalFraction);
						AddBoundaryPoint(Index, Plate.PlateId, B.UnitPosition, B.ContinentalFraction);
						AddBoundaryPoint(Index, Plate.PlateId, NormalizeOrFallback(A.UnitPosition + B.UnitPosition, A.UnitPosition), 0.5 * (A.ContinentalFraction + B.ContinentalFraction));
					}
				}
			}
		}
		return Index;
	}

	bool FindNearestBoundaryPair(
		const FCarrierLabVizBoundaryIndex& Index,
		const FVector3d& SamplePosition,
		FCarrierLabVizBoundaryPoint& OutQ1,
		FCarrierLabVizBoundaryPoint& OutQ2)
	{
		double BestDot = -TNumericLimits<double>::Max();
		double SecondDot = -TNumericLimits<double>::Max();
		const int32 CenterKey = BoundaryBinKey(SamplePosition);
		const int32 CenterLat = CenterKey / BoundaryLonBins;
		const int32 CenterLon = CenterKey % BoundaryLonBins;
		auto ConsiderPoint = [&](const int32 PointIndex)
		{
			if (!Index.Points.IsValidIndex(PointIndex))
			{
				return;
			}
			const FCarrierLabVizBoundaryPoint& Point = Index.Points[PointIndex];
			const double Dot = FVector3d::DotProduct(SamplePosition, Point.UnitPosition);
			if (Dot > BestDot)
			{
				if (Point.PlateId != OutQ1.PlateId)
				{
					OutQ2 = OutQ1;
					SecondDot = BestDot;
				}
				OutQ1 = Point;
				BestDot = Dot;
			}
			else if (Point.PlateId != OutQ1.PlateId && Dot > SecondDot)
			{
				OutQ2 = Point;
				SecondDot = Dot;
			}
		};

		for (int32 Radius = 0; Radius <= BoundarySearchRadiusBins; ++Radius)
		{
			for (int32 DLat = -Radius; DLat <= Radius; ++DLat)
			{
				const int32 Lat = CenterLat + DLat;
				if (Lat < 0 || Lat >= BoundaryLatBins)
				{
					continue;
				}
				for (int32 DLon = -Radius; DLon <= Radius; ++DLon)
				{
					const int32 Lon = (CenterLon + DLon + BoundaryLonBins) % BoundaryLonBins;
					if (const TArray<int32>* Bin = Index.Bins.Find(Lat * BoundaryLonBins + Lon))
					{
						for (const int32 PointIndex : *Bin)
						{
							ConsiderPoint(PointIndex);
						}
					}
				}
			}
			if (OutQ1.PlateId != INDEX_NONE && OutQ2.PlateId != INDEX_NONE)
			{
				return true;
			}
		}

		for (int32 PointIndex = 0; PointIndex < Index.Points.Num(); ++PointIndex)
		{
			ConsiderPoint(PointIndex);
		}
		return OutQ1.PlateId != INDEX_NONE && OutQ2.PlateId != INDEX_NONE;
	}

	double SignedPairSeparationVelocityForPlatePair(
		const FVector3d& Position,
		const TArray<FCarrierLabVisualizationMotion>& Motions,
		const int32 A,
		const int32 B)
	{
		if (!Motions.IsValidIndex(A) || !Motions.IsValidIndex(B))
		{
			return 0.0;
		}
		const FVector3d ToB = NormalizeOrFallback(
			Motions[B].CurrentCenter - FVector3d::DotProduct(Motions[B].CurrentCenter, Position) * Position,
			FVector3d::UnitX());
		const FVector3d VelocityA = FVector3d::CrossProduct(Motions[A].Axis, Position) * Motions[A].AngularSpeedRadiansPerStep;
		const FVector3d VelocityB = FVector3d::CrossProduct(Motions[B].Axis, Position) * Motions[B].AngularSpeedRadiansPerStep;
		return FVector3d::DotProduct(VelocityB - VelocityA, ToB);
	}

	FLinearColor PlateColor(const int32 PlateId)
	{
		if (PlateId == INDEX_NONE)
		{
			return FLinearColor(0.04f, 0.04f, 0.05f, 1.0f);
		}
		uint32 Hash = static_cast<uint32>(PlateId + 1) * 747796405u + 2891336453u;
		Hash = ((Hash >> ((Hash >> 28) + 4)) ^ Hash) * 277803737u;
		Hash = (Hash >> 22) ^ Hash;
		const uint8 Hue = static_cast<uint8>(Hash % 256u);
		return FLinearColor::MakeFromHSV8(Hue, 150, 230);
	}

	FLinearColor ContinentalColor(const double Fraction)
	{
		const double Alpha = FMath::Clamp(Fraction, 0.0, 1.0);
		const FLinearColor Ocean(0.02f, 0.18f, 0.44f, 1.0f);
		const FLinearColor Coast(0.12f, 0.52f, 0.40f, 1.0f);
		const FLinearColor Land(0.76f, 0.62f, 0.30f, 1.0f);
		return Alpha < 0.5
			? FMath::Lerp(Ocean, Coast, static_cast<float>(Alpha * 2.0))
			: FMath::Lerp(Coast, Land, static_cast<float>((Alpha - 0.5) * 2.0));
	}

	FVector4f ToVector4f(const FLinearColor& Color)
	{
		return FVector4f(Color.R, Color.G, Color.B, Color.A);
	}
}

ACarrierLabVisualizationActor::ACarrierLabVisualizationActor()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoReceiveInput = EAutoReceiveInput::Player0;

	MeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("CarrierMesh"));
	RootComponent = MeshComponent;
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MeshComponent->SetColorOverrideMode(EDynamicMeshComponentColorOverrideMode::VertexColors);
	MeshComponent->SetVertexColorSpaceTransformMode(EDynamicMeshVertexColorTransformMode::NoTransform);
	MeshComponent->SetTwoSided(true);
	MeshComponent->SetEnableFlatShading(true);
	MeshComponent->SetEnableWireframeRenderPass(true);
}

void ACarrierLabVisualizationActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (!HasAnyFlags(RF_ClassDefaultObject) && bAutoInitialize)
	{
		InitializeCarrier();
	}
}

void ACarrierLabVisualizationActor::BeginPlay()
{
	Super::BeginPlay();
	bPlaying = bPlayOnBegin;
	if (bAutoInitialize && !bInitialized)
	{
		InitializeCarrier();
	}
	BindInputControls();
}

void ACarrierLabVisualizationActor::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPlaying && bInitialized)
	{
		StepAccumulator += FMath::Max(0.0, DeltaSeconds) * StepsPerSecond;
		int32 StepsThisFrame = 0;
		while (StepAccumulator >= 1.0 && StepsThisFrame < 8)
		{
			AdvanceOneStep();
			StepAccumulator -= 1.0;
			++StepsThisFrame;
		}
		if (StepsThisFrame > 0)
		{
			ProjectCurrentCarrier();
		}
	}
	if (bShowHud)
	{
		ShowHud();
	}
}

bool ACarrierLabVisualizationActor::InitializeCarrier()
{
	CarrierLab::FStage0Config Config;
	Config.SampleCount = FMath::Max(12, SampleCount);
	Config.PlateCount = FMath::Clamp(PlateCount, 1, FMath::Min(63, Config.SampleCount));
	Config.Seed = Seed;
	Config.ContinentalPlateFraction = FMath::Clamp(ContinentalPlateFraction, 0.0, 1.0);

	FString Error;
	CarrierLab::FCarrierState NewState;
	if (!CarrierLab::FCarrierLabStage0::BuildColdStartCarrier(Config, NewState, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab visualization initialization failed: %s"), *Error);
		return false;
	}

	State = MoveTemp(NewState);
	CurrentMetrics = FCarrierLabVisualizationMetrics();
	RenderPlateIds.SetNum(State.Samples.Num());
	RenderContinentalFractions.SetNum(State.Samples.Num());
	MissMask.SetNum(State.Samples.Num());
	OverlapMask.SetNum(State.Samples.Num());
	BoundaryMask.SetNum(State.Samples.Num());

	Motions.Reset(State.Plates.Num());
	Motions.SetNum(State.Plates.Num());
	const double AngularSpeed = AngularSpeedRadiansPerStep(VelocityMmPerYear);
	for (const CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		const FVector3d SeedVector = NormalizeOrFallback(
			FVector3d(
				FMath::Sin(static_cast<double>(Plate.PlateId + 1) * 12.9898),
				FMath::Sin(static_cast<double>(Plate.PlateId + 1) * 78.233),
				FMath::Sin(static_cast<double>(Plate.PlateId + 1) * 37.719)),
			FVector3d::UnitZ());
		FVector3d Axis = FVector3d::CrossProduct(Plate.InitialCenter, SeedVector);
		if (Axis.SquaredLength() <= UE_SMALL_NUMBER)
		{
			Axis = FVector3d::CrossProduct(Plate.InitialCenter, FVector3d::UnitX());
		}
		Motions[Plate.PlateId].Axis = NormalizeOrFallback(Axis, FVector3d::UnitZ());
		Motions[Plate.PlateId].AngularSpeedRadiansPerStep = AngularSpeed;
		Motions[Plate.PlateId].CurrentCenter = Plate.InitialCenter;
	}

	bInitialized = true;
	StepAccumulator = 0.0;
	ProjectCurrentCarrier();
	return true;
}

void ACarrierLabVisualizationActor::TogglePlay()
{
	bPlaying = !bPlaying;
}

void ACarrierLabVisualizationActor::StepOnce()
{
	if (!bInitialized && !InitializeCarrier())
	{
		return;
	}
	AdvanceOneStep();
	ProjectCurrentCarrier();
}

void ACarrierLabVisualizationActor::ApplyResampleEvent()
{
	if (!bInitialized && !InitializeCarrier())
	{
		return;
	}

	const double StartSeconds = FPlatformTime::Seconds();
	TArray<FCarrierLabVizPlateMesh> PlateMeshes;
	FString MeshError;
	if (!BuildPlateRayMeshes(State, PlateMeshes, MeshError))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab visualization resample failed: %s"), *MeshError);
		return;
	}

	const FCarrierLabVizBoundaryIndex BoundaryIndex = BuildBoundaryIndex(State);
	TArray<int32> NewPlateIds;
	TArray<double> NewFractions;
	NewPlateIds.Init(INDEX_NONE, State.Samples.Num());
	NewFractions.Init(0.0, State.Samples.Num());

	int32 GapFillCount = 0;
	int32 NonSeparatingGapFillCount = 0;
	TArray<FCarrierLabVizCandidate> Candidates;
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		uint64 PlateMask = 0;
		bool bAnyBoundary = false;
		QuerySampleCandidates(State, PlateMeshes, Sample, Candidates, PlateMask, bAnyBoundary);
		const int32 PlateHitCount = CountBits64(PlateMask);
		if (PlateHitCount == 0)
		{
			FCarrierLabVizBoundaryPoint Q1;
			FCarrierLabVizBoundaryPoint Q2;
			if (FindNearestBoundaryPair(BoundaryIndex, Sample.UnitPosition, Q1, Q2))
			{
				const FVector3d RidgePoint = NormalizeOrFallback(Q1.UnitPosition + Q2.UnitPosition, Sample.UnitPosition);
				const double SignedVelocity = SignedPairSeparationVelocityForPlatePair(RidgePoint, Motions, Q1.PlateId, Q2.PlateId);
				++GapFillCount;
				if (SignedVelocity <= 0.0)
				{
					++NonSeparatingGapFillCount;
				}
				NewPlateIds[Sample.Id] = Q1.PlateId;
				NewFractions[Sample.Id] = FMath::Clamp(0.5 * (Q1.ContinentalFraction + Q2.ContinentalFraction), 0.0, 1.0);
			}
			else
			{
				NewPlateIds[Sample.Id] = Sample.PlateId;
				NewFractions[Sample.Id] = Sample.ContinentalFraction;
			}
			continue;
		}

		const int32 ResolvedPlateId = PlateHitCount == 1 ? LowestSetBitIndex(PlateMask) : ChooseNearestCandidatePlate(Sample, Candidates, Motions);
		const FCarrierLabVizCandidate* Chosen = Candidates.FindByPredicate([ResolvedPlateId](const FCarrierLabVizCandidate& Candidate)
		{
			return Candidate.PlateId == ResolvedPlateId;
		});
		NewPlateIds[Sample.Id] = ResolvedPlateId;
		NewFractions[Sample.Id] = (Chosen != nullptr && State.Plates.IsValidIndex(ResolvedPlateId))
			? InterpolateContinentalFraction(State.Plates[ResolvedPlateId], *Chosen)
			: 0.0;
	}

	for (CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (NewPlateIds.IsValidIndex(Sample.Id) && NewPlateIds[Sample.Id] != INDEX_NONE)
		{
			Sample.PlateId = NewPlateIds[Sample.Id];
			Sample.ContinentalFraction = FMath::Clamp(NewFractions[Sample.Id], 0.0, 1.0);
			Sample.bContinental = Sample.ContinentalFraction >= 0.5;
		}
	}

	CarrierLab::FCarrierLabStage0::RebuildPlateLocalStateFromSamples(State);
	for (FCarrierLabVisualizationMotion& Motion : Motions)
	{
		Motion.CurrentCenter = FVector3d::ZeroVector;
	}
	TArray<int32> Counts;
	Counts.Init(0, Motions.Num());
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		if (Motions.IsValidIndex(Sample.PlateId))
		{
			Motions[Sample.PlateId].CurrentCenter += Sample.UnitPosition;
			++Counts[Sample.PlateId];
		}
	}
	for (int32 PlateId = 0; PlateId < Motions.Num(); ++PlateId)
	{
		Motions[PlateId].CurrentCenter = NormalizeOrFallback(
			Motions[PlateId].CurrentCenter,
			State.Plates.IsValidIndex(PlateId) ? State.Plates[PlateId].InitialCenter : FVector3d::UnitZ());
	}

	++CurrentMetrics.EventCount;
	const int32 EventCount = CurrentMetrics.EventCount;
	ProjectCurrentCarrier();
	CurrentMetrics.EventCount = EventCount;
	CurrentMetrics.LastGapFillCount = GapFillCount;
	CurrentMetrics.LastNonSeparatingGapFillCount = NonSeparatingGapFillCount;
	CurrentMetrics.ProjectionSeconds += FPlatformTime::Seconds() - StartSeconds;
}

void ACarrierLabVisualizationActor::SetVisualizationLayer(const ECarrierLabVisualizationLayer NewLayer)
{
	VisualizationLayer = NewLayer;
	if (bInitialized)
	{
		RebuildRenderMesh();
	}
}

void ACarrierLabVisualizationActor::ShowPlateIdLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::PlateId);
}

void ACarrierLabVisualizationActor::ShowContinentalFractionLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::ContinentalFraction);
}

void ACarrierLabVisualizationActor::ShowMissMaskLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::MissMask);
}

void ACarrierLabVisualizationActor::ShowOverlapMaskLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::OverlapMask);
}

void ACarrierLabVisualizationActor::ShowBoundaryMaskLayer()
{
	SetVisualizationLayer(ECarrierLabVisualizationLayer::BoundaryMask);
}

void ACarrierLabVisualizationActor::BindInputControls()
{
	APlayerController* PlayerController = GetWorld() != nullptr ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (PlayerController == nullptr)
	{
		return;
	}
	EnableInput(PlayerController);
	if (InputComponent == nullptr)
	{
		return;
	}

	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ACarrierLabVisualizationActor::TogglePlay);
	InputComponent->BindKey(EKeys::Period, IE_Pressed, this, &ACarrierLabVisualizationActor::StepOnce);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &ACarrierLabVisualizationActor::ApplyResampleEvent);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowPlateIdLayer);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowContinentalFractionLayer);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowMissMaskLayer);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowOverlapMaskLayer);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &ACarrierLabVisualizationActor::ShowBoundaryMaskLayer);
}

void ACarrierLabVisualizationActor::AdvanceOneStep()
{
	for (CarrierLab::FCarrierPlate& Plate : State.Plates)
	{
		if (!Motions.IsValidIndex(Plate.PlateId))
		{
			continue;
		}
		const FCarrierLabVisualizationMotion& Motion = Motions[Plate.PlateId];
		for (CarrierLab::FCarrierVertex& Vertex : Plate.Vertices)
		{
			Vertex.UnitPosition = NormalizeOrFallback(RotateVector(Vertex.UnitPosition, Motion.Axis, Motion.AngularSpeedRadiansPerStep), Vertex.UnitPosition);
		}
	}
	for (FCarrierLabVisualizationMotion& Motion : Motions)
	{
		Motion.CurrentCenter = NormalizeOrFallback(RotateVector(Motion.CurrentCenter, Motion.Axis, Motion.AngularSpeedRadiansPerStep), Motion.CurrentCenter);
	}
	++CurrentMetrics.Step;
}

void ACarrierLabVisualizationActor::ProjectCurrentCarrier()
{
	if (!bInitialized)
	{
		return;
	}

	const double StartSeconds = FPlatformTime::Seconds();
	CurrentMetrics.SampleCount = State.Samples.Num();
	CurrentMetrics.PlateCount = State.Plates.Num();
	CurrentMetrics.RawMissCount = 0;
	CurrentMetrics.RawMultiHitCount = 0;
	CurrentMetrics.BoundaryHitCount = 0;

	RenderPlateIds.Init(INDEX_NONE, State.Samples.Num());
	RenderContinentalFractions.Init(0.0, State.Samples.Num());
	MissMask.Init(0, State.Samples.Num());
	OverlapMask.Init(0, State.Samples.Num());
	BoundaryMask.Init(0, State.Samples.Num());

	TArray<FCarrierLabVizPlateMesh> PlateMeshes;
	FString MeshError;
	if (!BuildPlateRayMeshes(State, PlateMeshes, MeshError))
	{
		UE_LOG(LogTemp, Error, TEXT("CarrierLab visualization projection failed: %s"), *MeshError);
		return;
	}

	double TotalArea = 0.0;
	double AuthoritativeContinentalArea = 0.0;
	double ProjectedContinentalArea = 0.0;
	TArray<FCarrierLabVizCandidate> Candidates;
	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		TotalArea += Sample.AreaWeight;
		AuthoritativeContinentalArea += Sample.AreaWeight * Sample.ContinentalFraction;

		uint64 PlateMask = 0;
		bool bAnyBoundary = false;
		QuerySampleCandidates(State, PlateMeshes, Sample, Candidates, PlateMask, bAnyBoundary);
		const int32 PlateHitCount = CountBits64(PlateMask);
		BoundaryMask[Sample.Id] = bAnyBoundary ? 1 : 0;
		if (bAnyBoundary)
		{
			++CurrentMetrics.BoundaryHitCount;
		}

		if (PlateHitCount == 0)
		{
			++CurrentMetrics.RawMissCount;
			MissMask[Sample.Id] = 1;
			continue;
		}
		if (PlateHitCount > 1)
		{
			++CurrentMetrics.RawMultiHitCount;
			OverlapMask[Sample.Id] = 1;
		}

		const int32 ResolvedPlateId = PlateHitCount == 1 ? LowestSetBitIndex(PlateMask) : ChooseNearestCandidatePlate(Sample, Candidates, Motions);
		const FCarrierLabVizCandidate* Chosen = Candidates.FindByPredicate([ResolvedPlateId](const FCarrierLabVizCandidate& Candidate)
		{
			return Candidate.PlateId == ResolvedPlateId;
		});
		const double Fraction = (Chosen != nullptr && State.Plates.IsValidIndex(ResolvedPlateId))
			? InterpolateContinentalFraction(State.Plates[ResolvedPlateId], *Chosen)
			: 0.0;
		RenderPlateIds[Sample.Id] = ResolvedPlateId;
		RenderContinentalFractions[Sample.Id] = Fraction;
		ProjectedContinentalArea += Sample.AreaWeight * Fraction;
	}

	CurrentMetrics.AuthoritativeCAF = TotalArea > UE_DOUBLE_SMALL_NUMBER ? AuthoritativeContinentalArea / TotalArea : 0.0;
	CurrentMetrics.ProjectedCAF = TotalArea > UE_DOUBLE_SMALL_NUMBER ? ProjectedContinentalArea / TotalArea : 0.0;
	CurrentMetrics.ProjectionSeconds = FPlatformTime::Seconds() - StartSeconds;
	RebuildRenderMesh();
}

void ACarrierLabVisualizationActor::RebuildRenderMesh()
{
	if (!MeshComponent || State.Samples.IsEmpty() || State.Triangles.IsEmpty())
	{
		return;
	}

	const double StartSeconds = FPlatformTime::Seconds();
	FDynamicMesh3 Mesh;
	Mesh.EnableTriangleGroups();
	Mesh.EnableAttributes();
	Mesh.Attributes()->EnablePrimaryColors();
	FDynamicMeshColorOverlay* Colors = Mesh.Attributes()->PrimaryColors();

	for (const CarrierLab::FSphereSample& Sample : State.Samples)
	{
		Mesh.AppendVertex(Sample.UnitPosition * SphereRadius);
	}

	auto ColorForSample = [this](const int32 SampleId)
	{
		switch (VisualizationLayer)
		{
		case ECarrierLabVisualizationLayer::ContinentalFraction:
			return ContinentalColor(RenderContinentalFractions.IsValidIndex(SampleId) ? RenderContinentalFractions[SampleId] : 0.0);
		case ECarrierLabVisualizationLayer::MissMask:
			return MissMask.IsValidIndex(SampleId) && MissMask[SampleId] != 0
				? FLinearColor(0.95f, 0.05f, 0.04f, 1.0f)
				: FLinearColor(0.02f, 0.05f, 0.09f, 1.0f);
		case ECarrierLabVisualizationLayer::OverlapMask:
			return OverlapMask.IsValidIndex(SampleId) && OverlapMask[SampleId] != 0
				? FLinearColor(1.0f, 0.46f, 0.05f, 1.0f)
				: FLinearColor(0.02f, 0.05f, 0.09f, 1.0f);
		case ECarrierLabVisualizationLayer::BoundaryMask:
			return BoundaryMask.IsValidIndex(SampleId) && BoundaryMask[SampleId] != 0
				? FLinearColor(0.95f, 0.95f, 0.90f, 1.0f)
				: FLinearColor(0.02f, 0.05f, 0.09f, 1.0f);
		case ECarrierLabVisualizationLayer::PlateId:
		default:
			return PlateColor(RenderPlateIds.IsValidIndex(SampleId) ? RenderPlateIds[SampleId] : INDEX_NONE);
		}
	};

	for (const CarrierLab::FSphereTriangle& Triangle : State.Triangles)
	{
		const int32 TriangleId = Mesh.AppendTriangle(Triangle.A, Triangle.B, Triangle.C, 0);
		if (TriangleId < 0)
		{
			continue;
		}

		const int32 ColorA = Colors->AppendElement(ToVector4f(ColorForSample(Triangle.A)));
		const int32 ColorB = Colors->AppendElement(ToVector4f(ColorForSample(Triangle.B)));
		const int32 ColorC = Colors->AppendElement(ToVector4f(ColorForSample(Triangle.C)));
		Colors->SetParentVertex(ColorA, Triangle.A);
		Colors->SetParentVertex(ColorB, Triangle.B);
		Colors->SetParentVertex(ColorC, Triangle.C);
		Colors->SetTriangle(TriangleId, FIndex3i(ColorA, ColorB, ColorC));
	}

	MeshComponent->SetMesh(MoveTemp(Mesh));
	MeshComponent->SetColorOverrideMode(EDynamicMeshComponentColorOverrideMode::VertexColors);
	MeshComponent->SetVertexColorSpaceTransformMode(EDynamicMeshVertexColorTransformMode::NoTransform);
	MeshComponent->SetTwoSided(true);
	MeshComponent->SetEnableFlatShading(true);
	MeshComponent->SetEnableWireframeRenderPass(bShowWireframeOverlay);
	CurrentMetrics.MeshUpdateSeconds = FPlatformTime::Seconds() - StartSeconds;
}

FString ACarrierLabVisualizationActor::BuildHudText() const
{
	const TCHAR* LayerName = TEXT("plate id");
	switch (VisualizationLayer)
	{
	case ECarrierLabVisualizationLayer::ContinentalFraction:
		LayerName = TEXT("continental fraction");
		break;
	case ECarrierLabVisualizationLayer::MissMask:
		LayerName = TEXT("miss mask");
		break;
	case ECarrierLabVisualizationLayer::OverlapMask:
		LayerName = TEXT("overlap mask");
		break;
	case ECarrierLabVisualizationLayer::BoundaryMask:
		LayerName = TEXT("boundary mask");
		break;
	case ECarrierLabVisualizationLayer::PlateId:
	default:
		break;
	}

	return FString::Printf(
		TEXT("CarrierLab Phase I Viewer | %s | layer=%s\nstep=%d events=%d samples=%d plates=%d\nmiss=%d multi=%d boundary=%d gap_fill=%d nonsep_gap=%d\nAuthCAF=%.6f ProjCAF=%.6f projection=%.3fs mesh=%.3fs\nSpace play/pause | . step | R resample | 1-5 layers"),
		bPlaying ? TEXT("PLAY") : TEXT("PAUSED"),
		LayerName,
		CurrentMetrics.Step,
		CurrentMetrics.EventCount,
		CurrentMetrics.SampleCount,
		CurrentMetrics.PlateCount,
		CurrentMetrics.RawMissCount,
		CurrentMetrics.RawMultiHitCount,
		CurrentMetrics.BoundaryHitCount,
		CurrentMetrics.LastGapFillCount,
		CurrentMetrics.LastNonSeparatingGapFillCount,
		CurrentMetrics.AuthoritativeCAF,
		CurrentMetrics.ProjectedCAF,
		CurrentMetrics.ProjectionSeconds,
		CurrentMetrics.MeshUpdateSeconds);
}

void ACarrierLabVisualizationActor::ShowHud() const
{
	if (GEngine == nullptr)
	{
		return;
	}
	GEngine->AddOnScreenDebugMessage(
		static_cast<uint64>(GetUniqueID()),
		0.0f,
		FColor::Cyan,
		BuildHudText(),
		false,
		FVector2D(0.85f, 0.85f));
}
