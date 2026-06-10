// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabV2Core.h"

#include "CompGeom/ConvexHull3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"
#include "Math/Ray.h"

namespace CarrierLab::V2
{
	namespace
	{
		constexpr double GoldenAngle = 2.3999632297286533222;
		constexpr uint64 FnvOffset = 1469598103934665603ull;
		constexpr uint64 FnvPrime = 1099511628211ull;

		struct FCarrierV2RayCandidateRef
		{
			int32 PlateId = INDEX_NONE;
			int32 LocalTriangleId = INDEX_NONE;
		};

		struct FCarrierV2BuildState
		{
			FCarrierV2Stage0Config Config;
			TArray<FCarrierV2SubstrateSample> Samples;
			TArray<FCarrierV2SubstrateTriangle> Triangles;
			TArray<int32> TrianglePlateIds;
			TArray<FCarrierV2Plate> Plates;
			TArray<TArray<FCarrierV2RayCandidateRef>> SampleRayCandidates;
			TArray<int32> SourceTriangleCopyCounts;
		};

		void HashMix(uint64& Hash, const uint64 Value)
		{
			Hash ^= Value;
			Hash *= FnvPrime;
		}

		void HashMixInt(uint64& Hash, const int64 Value)
		{
			HashMix(Hash, static_cast<uint64>(Value));
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
		}

		FString HashToString(const uint64 Hash)
		{
			return FString::Printf(TEXT("%016llx"), Hash);
		}

		FString FixtureKindName(const ECarrierV2FixtureKind Kind)
		{
			switch (Kind)
			{
			case ECarrierV2FixtureKind::Negative:
				return TEXT("negative");
			case ECarrierV2FixtureKind::Scale:
				return TEXT("scale");
			case ECarrierV2FixtureKind::Positive:
			default:
				return TEXT("positive");
			}
		}

		FString MaterialClassName(const ECarrierV2MaterialClass MaterialClass)
		{
			switch (MaterialClass)
			{
			case ECarrierV2MaterialClass::Oceanic:
				return TEXT("oceanic");
			case ECarrierV2MaterialClass::Continental:
				return TEXT("continental");
			case ECarrierV2MaterialClass::Mixed:
				return TEXT("mixed");
			case ECarrierV2MaterialClass::Unknown:
			default:
				return TEXT("unknown");
			}
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

		FCarrierV2MaterialRecord ExpectedMaterialForSample(const int32 SampleId, const int32 Seed)
		{
			uint32 X = static_cast<uint32>(SampleId + 1) * 747796405u;
			X ^= static_cast<uint32>(Seed) * 2891336453u;
			X ^= X >> 16;
			X *= 2246822519u;
			X ^= X >> 13;
			const double Unit = static_cast<double>(X & 0x00ffffffu) / static_cast<double>(0x01000000u);

			FCarrierV2MaterialRecord Material;
			Material.ContinentalFraction = Unit < 0.30 ? 1.0 : 0.0;
			Material.MaterialClass = Material.ContinentalFraction > 0.0
				? ECarrierV2MaterialClass::Continental
				: ECarrierV2MaterialClass::Oceanic;
			Material.Provenance = TEXT("fixture_defined");
			return Material;
		}

		void AddSample(
			FCarrierV2BuildState& State,
			const FVector3d& UnitPosition)
		{
			FCarrierV2SubstrateSample Sample;
			Sample.SampleId = State.Samples.Num();
			Sample.UnitPosition = UnitPosition.GetSafeNormal();
			Sample.AreaWeight = 4.0 * UE_DOUBLE_PI / static_cast<double>(FMath::Max(1, State.Config.SampleCount));
			State.Samples.Add(Sample);
		}

		void AddTriangle(
			FCarrierV2BuildState& State,
			const int32 A,
			const int32 B,
			const int32 C,
			const int32 PlateId)
		{
			FCarrierV2SubstrateTriangle Triangle;
			Triangle.TriangleId = State.Triangles.Num();
			Triangle.SampleIds[0] = A;
			Triangle.SampleIds[1] = B;
			Triangle.SampleIds[2] = C;
			State.Triangles.Add(Triangle);
			State.TrianglePlateIds.Add(PlateId);
		}

		bool OrientTriangle(const TArray<FCarrierV2SubstrateSample>& Samples, FCarrierV2SubstrateTriangle& Triangle)
		{
			if (!Samples.IsValidIndex(Triangle.SampleIds[0]) ||
				!Samples.IsValidIndex(Triangle.SampleIds[1]) ||
				!Samples.IsValidIndex(Triangle.SampleIds[2]))
			{
				return false;
			}

			const FVector3d A = Samples[Triangle.SampleIds[0]].UnitPosition;
			const FVector3d B = Samples[Triangle.SampleIds[1]].UnitPosition;
			const FVector3d C = Samples[Triangle.SampleIds[2]].UnitPosition;
			const FVector3d Centroid = (A + B + C).GetSafeNormal();
			const FVector3d Normal = FVector3d::CrossProduct(B - A, C - A);
			if (FVector3d::DotProduct(Normal, Centroid) < 0.0)
			{
				Swap(Triangle.SampleIds[1], Triangle.SampleIds[2]);
			}
			return true;
		}

		void BuildHandcraftedOctahedron(FCarrierV2BuildState& State)
		{
			State.Samples.Reset();
			State.Triangles.Reset();
			State.TrianglePlateIds.Reset();

			AddSample(State, FVector3d(1.0, 0.0, 0.0));
			AddSample(State, FVector3d(-1.0, 0.0, 0.0));
			AddSample(State, FVector3d(0.0, 1.0, 0.0));
			AddSample(State, FVector3d(0.0, -1.0, 0.0));
			AddSample(State, FVector3d(0.0, 0.0, 1.0));
			AddSample(State, FVector3d(0.0, 0.0, -1.0));

			const bool bSinglePlate = State.Config.PlateCount <= 1;
			const bool bThirdPlateAvailable = State.Config.PlateCount >= 3;
			AddTriangle(State, 0, 2, 4, 0);
			AddTriangle(State, 2, 1, 4, bSinglePlate ? 0 : (bThirdPlateAvailable ? 2 : 0));
			AddTriangle(State, 1, 3, 4, bSinglePlate ? 0 : 1);
			AddTriangle(State, 3, 0, 4, bSinglePlate ? 0 : (bThirdPlateAvailable ? 2 : 1));
			AddTriangle(State, 2, 0, 5, 0);
			AddTriangle(State, 1, 2, 5, bSinglePlate ? 0 : (bThirdPlateAvailable ? 2 : 0));
			AddTriangle(State, 3, 1, 5, bSinglePlate ? 0 : 1);
			AddTriangle(State, 0, 3, 5, bSinglePlate ? 0 : (bThirdPlateAvailable ? 2 : 1));

			for (FCarrierV2SubstrateTriangle& Triangle : State.Triangles)
			{
				OrientTriangle(State.Samples, Triangle);
			}
		}

		void GenerateFibonacciSamples(FCarrierV2BuildState& State)
		{
			State.Samples.Reset(State.Config.SampleCount);
			for (int32 Index = 0; Index < State.Config.SampleCount; ++Index)
			{
				const double Z = 1.0 - (2.0 * (static_cast<double>(Index) + 0.5) / static_cast<double>(State.Config.SampleCount));
				const double Radius = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
				const double Theta = GoldenAngle * static_cast<double>(Index);
				AddSample(State, FVector3d(FMath::Cos(Theta) * Radius, FMath::Sin(Theta) * Radius, Z));
			}
		}

		void GeneratePlateCenters(const FCarrierV2Stage0Config& Config, TArray<FVector3d>& OutCenters)
		{
			OutCenters.Reset(Config.PlateCount);
			const double SeedPhase = FMath::Frac(static_cast<double>(Config.Seed) * 0.6180339887498948482);
			for (int32 Index = 0; Index < Config.PlateCount; ++Index)
			{
				const double Z = 1.0 - (2.0 * (static_cast<double>(Index) + 0.5) / static_cast<double>(Config.PlateCount));
				const double Radius = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
				const double Theta = GoldenAngle * (static_cast<double>(Index) + SeedPhase);
				OutCenters.Add(FVector3d(FMath::Cos(Theta) * Radius, FMath::Sin(Theta) * Radius, Z).GetSafeNormal());
			}
		}

		int32 FindNearestCenter(const TArray<FVector3d>& Centers, const FVector3d& UnitPosition)
		{
			int32 BestPlateId = 0;
			double BestDot = -MAX_dbl;
			for (int32 PlateId = 0; PlateId < Centers.Num(); ++PlateId)
			{
				const double Dot = FVector3d::DotProduct(Centers[PlateId], UnitPosition);
				if (Dot > BestDot)
				{
					BestDot = Dot;
					BestPlateId = PlateId;
				}
			}
			return BestPlateId;
		}

		bool BuildSphericalDelaunay(FCarrierV2BuildState& State, FString& OutError)
		{
			if (State.Samples.Num() < 4)
			{
				OutError = TEXT("V2 spherical Delaunay needs at least four samples.");
				return false;
			}

			TArray<FVector3d> Points;
			Points.Reserve(State.Samples.Num());
			for (const FCarrierV2SubstrateSample& Sample : State.Samples)
			{
				Points.Add(Sample.UnitPosition);
			}

			UE::Geometry::FConvexHull3d Hull;
			Hull.bSaveTriangleNeighbors = false;
			if (!Hull.Solve(TArrayView<const FVector3d>(Points.GetData(), Points.Num())) || !Hull.IsSolutionAvailable())
			{
				OutError = FString::Printf(
					TEXT("V2 FConvexHull3d failed; reported dimension %d."),
					Hull.GetDimension());
				return false;
			}

			if (Hull.GetNumHullPoints() != State.Samples.Num())
			{
				OutError = FString::Printf(
					TEXT("V2 FConvexHull3d used %d hull vertices for %d samples."),
					Hull.GetNumHullPoints(),
					State.Samples.Num());
				return false;
			}

			TArray<FVector3d> PlateCenters;
			GeneratePlateCenters(State.Config, PlateCenters);

			TSet<uint64> SeenTriangles;
			State.Triangles.Reset();
			State.TrianglePlateIds.Reset();
			const TArray<UE::Geometry::FIndex3i>& HullTriangles = Hull.GetTriangles();
			State.Triangles.Reserve(HullTriangles.Num());
			State.TrianglePlateIds.Reserve(HullTriangles.Num());
			for (const UE::Geometry::FIndex3i& HullTriangle : HullTriangles)
			{
				FCarrierV2SubstrateTriangle Triangle;
				Triangle.TriangleId = State.Triangles.Num();
				Triangle.SampleIds[0] = HullTriangle.A;
				Triangle.SampleIds[1] = HullTriangle.B;
				Triangle.SampleIds[2] = HullTriangle.C;
				if (!OrientTriangle(State.Samples, Triangle))
				{
					continue;
				}

				TArray<int32> Sorted = {Triangle.SampleIds[0], Triangle.SampleIds[1], Triangle.SampleIds[2]};
				Sorted.Sort();
				uint64 Key = FnvOffset;
				HashMixInt(Key, Sorted[0]);
				HashMixInt(Key, Sorted[1]);
				HashMixInt(Key, Sorted[2]);
				if (SeenTriangles.Contains(Key))
				{
					continue;
				}
				SeenTriangles.Add(Key);

				const FVector3d Centroid = (
					State.Samples[Triangle.SampleIds[0]].UnitPosition +
					State.Samples[Triangle.SampleIds[1]].UnitPosition +
					State.Samples[Triangle.SampleIds[2]].UnitPosition).GetSafeNormal();
				const int32 PlateId = FindNearestCenter(PlateCenters, Centroid);
				State.Triangles.Add(Triangle);
				State.TrianglePlateIds.Add(PlateId);
			}

			return !State.Triangles.IsEmpty();
		}

		int32 AddOrFindLocalVertex(
			FCarrierV2BuildState& State,
			FCarrierV2Plate& Plate,
			const int32 SourceSampleId)
		{
			if (const int32* Existing = Plate.SourceSampleToLocalVertex.Find(SourceSampleId))
			{
				return *Existing;
			}

			if (!State.Samples.IsValidIndex(SourceSampleId))
			{
				return INDEX_NONE;
			}

			FCarrierV2PlateVertex Vertex;
			Vertex.LocalVertexId = Plate.LocalVertices.Num();
			Vertex.SourceSampleId = SourceSampleId;
			Vertex.UnitPosition = State.Samples[SourceSampleId].UnitPosition;
			Vertex.Material = ExpectedMaterialForSample(SourceSampleId, State.Config.Seed);
			Plate.SourceSampleToLocalVertex.Add(SourceSampleId, Vertex.LocalVertexId);
			Plate.LocalVertices.Add(Vertex);
			return Vertex.LocalVertexId;
		}

		bool AddLocalTriangle(
			FCarrierV2BuildState& State,
			const int32 SourceTriangleId,
			const int32 PlateId)
		{
			if (!State.Triangles.IsValidIndex(SourceTriangleId) || !State.Plates.IsValidIndex(PlateId))
			{
				return false;
			}

			FCarrierV2Plate& Plate = State.Plates[PlateId];
			const FCarrierV2SubstrateTriangle& SourceTriangle = State.Triangles[SourceTriangleId];

			FCarrierV2PlateTriangle LocalTriangle;
			LocalTriangle.LocalTriangleId = Plate.LocalTriangles.Num();
			LocalTriangle.SourceTriangleId = SourceTriangleId;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 SourceSampleId = SourceTriangle.SampleIds[Corner];
				LocalTriangle.SourceSampleIds[Corner] = SourceSampleId;
				LocalTriangle.LocalVertexIds[Corner] = AddOrFindLocalVertex(State, Plate, SourceSampleId);
				if (LocalTriangle.LocalVertexIds[Corner] == INDEX_NONE)
				{
					return false;
				}
			}
			Plate.LocalTriangles.Add(LocalTriangle);
			if (State.SourceTriangleCopyCounts.IsValidIndex(SourceTriangleId))
			{
				++State.SourceTriangleCopyCounts[SourceTriangleId];
			}
			return true;
		}

		bool BuildPlateLocalState(FCarrierV2BuildState& State, FString& OutError)
		{
			State.Plates.Reset(State.Config.PlateCount);
			for (int32 PlateId = 0; PlateId < State.Config.PlateCount; ++PlateId)
			{
				FCarrierV2Plate Plate;
				Plate.PlateId = PlateId;
				State.Plates.Add(MoveTemp(Plate));
			}

			State.SourceTriangleCopyCounts.Init(0, State.Triangles.Num());
			for (int32 SourceTriangleId = 0; SourceTriangleId < State.Triangles.Num(); ++SourceTriangleId)
			{
				if (State.Config.bDeliberateTopologyHole && SourceTriangleId == 0)
				{
					continue;
				}

				const int32 PlateId = State.TrianglePlateIds.IsValidIndex(SourceTriangleId)
					? State.TrianglePlateIds[SourceTriangleId]
					: 0;
				if (!AddLocalTriangle(State, SourceTriangleId, PlateId))
				{
					OutError = FString::Printf(TEXT("Failed to add local triangle %d to plate %d."), SourceTriangleId, PlateId);
					return false;
				}
			}

			if (State.Config.bDeliberateDuplicatedOverlap && !State.Triangles.IsEmpty() && State.Plates.Num() >= 2)
			{
				const int32 OriginalPlateId = State.TrianglePlateIds.IsValidIndex(0) ? State.TrianglePlateIds[0] : 0;
				const int32 DuplicatePlateId = (OriginalPlateId + 1) % State.Plates.Num();
				if (!AddLocalTriangle(State, 0, DuplicatePlateId))
				{
					OutError = TEXT("Failed to inject V2 duplicated-overlap negative control.");
					return false;
				}
			}

			State.SampleRayCandidates.Reset();
			State.SampleRayCandidates.SetNum(State.Samples.Num());
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
				{
					for (const int32 SourceSampleId : Triangle.SourceSampleIds)
					{
						if (State.SampleRayCandidates.IsValidIndex(SourceSampleId))
						{
							State.SampleRayCandidates[SourceSampleId].Add(FCarrierV2RayCandidateRef{Plate.PlateId, Triangle.LocalTriangleId});
						}
					}
				}
			}

			return true;
		}

		bool IntersectRayTriangle(
			const FVector3d& Direction,
			const FVector3d& A,
			const FVector3d& B,
			const FVector3d& C,
			const double Epsilon,
			FVector3d& OutBarycentric,
			double& OutT,
			bool& bOutBoundaryDegenerate)
		{
			const FVector3d Normal = FVector3d::CrossProduct(B - A, C - A);
			const double Denom = FVector3d::DotProduct(Normal, Direction);
			if (FMath::Abs(Denom) <= Epsilon)
			{
				return false;
			}

			const double T = FVector3d::DotProduct(Normal, A) / Denom;
			if (T <= Epsilon)
			{
				return false;
			}

			const FVector3d Q = Direction * T;
			const FVector3d V0 = B - A;
			const FVector3d V1 = C - A;
			const FVector3d V2 = Q - A;
			const double D00 = FVector3d::DotProduct(V0, V0);
			const double D01 = FVector3d::DotProduct(V0, V1);
			const double D11 = FVector3d::DotProduct(V1, V1);
			const double D20 = FVector3d::DotProduct(V2, V0);
			const double D21 = FVector3d::DotProduct(V2, V1);
			const double BaryDenom = D00 * D11 - D01 * D01;
			if (FMath::Abs(BaryDenom) <= Epsilon)
			{
				return false;
			}

			const double V = (D11 * D20 - D01 * D21) / BaryDenom;
			const double W = (D00 * D21 - D01 * D20) / BaryDenom;
			const double U = 1.0 - V - W;

			if (U < -Epsilon || V < -Epsilon || W < -Epsilon ||
				U > 1.0 + Epsilon || V > 1.0 + Epsilon || W > 1.0 + Epsilon)
			{
				return false;
			}

			OutBarycentric = FVector3d(U, V, W);
			OutT = T;
			bOutBoundaryDegenerate =
				FMath::Abs(U) <= Epsilon || FMath::Abs(V) <= Epsilon || FMath::Abs(W) <= Epsilon ||
				FMath::Abs(1.0 - U) <= Epsilon || FMath::Abs(1.0 - V) <= Epsilon || FMath::Abs(1.0 - W) <= Epsilon;
			return true;
		}

		double InterpolateContinentalFraction(const FCarrierV2Plate& Plate, const FCarrierV2PlateTriangle& Triangle, const FVector3d& Barycentric)
		{
			double Value = 0.0;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 VertexId = Triangle.LocalVertexIds[Corner];
				if (Plate.LocalVertices.IsValidIndex(VertexId))
				{
					Value += Barycentric[Corner] * Plate.LocalVertices[VertexId].Material.ContinentalFraction;
				}
			}
			return Value;
		}

		void ComputeTopologyMetrics(const FCarrierV2BuildState& State, FCarrierV2Stage0Metrics& Metrics)
		{
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				Metrics.LocalPlateVertexCountSum += Plate.LocalVertices.Num();
				Metrics.LocalPlateTriangleCountSum += Plate.LocalTriangles.Num();
			}

			for (const int32 CopyCount : State.SourceTriangleCopyCounts)
			{
				if (CopyCount == 0)
				{
					++Metrics.TopologyHoleErrorCount;
				}
				else if (CopyCount > 1)
				{
					++Metrics.TopologyDuplicateErrorCount;
				}
			}
		}

		FString ObserveFailureReason(const FCarrierV2Stage0Metrics& Metrics)
		{
			if (Metrics.ProjectionReadsGlobalOwnerCount > 0)
			{
				return TEXT("global_owner_authority");
			}
			if (Metrics.TopologyHoleErrorCount > 0)
			{
				return TEXT("topology_hole");
			}
			if (Metrics.TopologyDuplicateErrorCount > 0)
			{
				return TEXT("topology_duplicate");
			}
			if (Metrics.NonDegenerateMissCount > 0)
			{
				return TEXT("nondegenerate_miss");
			}
			if (Metrics.NonDegenerateOverlapCount > 0)
			{
				return TEXT("nondegenerate_overlap");
			}
			if (!FMath::IsNearlyZero(Metrics.MaterialAuthorityProjectedDelta, 1.0e-6))
			{
				return TEXT("material_delta");
			}
			return TEXT("none");
		}

		bool IsPositiveGatePass(const FCarrierV2Stage0Metrics& Metrics)
		{
			return Metrics.ProjectionReadsGlobalOwnerCount == 0 &&
				Metrics.TopologyDuplicateErrorCount == 0 &&
				Metrics.TopologyHoleErrorCount == 0 &&
				Metrics.NonDegenerateMissCount == 0 &&
				Metrics.NonDegenerateOverlapCount == 0 &&
				FMath::IsNearlyZero(Metrics.MaterialAuthorityProjectedDelta, 1.0e-6);
		}

		bool IsNegativeGatePass(const FCarrierV2Stage0Metrics& Metrics)
		{
			return Metrics.ProjectionReadsGlobalOwnerCount == 0 &&
				Metrics.ExpectedFailureReason == Metrics.ObservedFailureReason &&
				Metrics.ObservedFailureReason != TEXT("none");
		}

		void FinalizeFixtureVerdict(FCarrierV2Stage0Metrics& Metrics, const ECarrierV2FixtureKind Kind)
		{
			Metrics.ObservedFailureReason = ObserveFailureReason(Metrics);
			if (Kind == ECarrierV2FixtureKind::Negative)
			{
				Metrics.bFixturePass = IsNegativeGatePass(Metrics);
				Metrics.Verdict = Metrics.bFixturePass ? TEXT("PASS_NEGATIVE_CONTROL") : TEXT("FAIL_NEGATIVE_CONTROL");
			}
			else
			{
				Metrics.bFixturePass = IsPositiveGatePass(Metrics);
				Metrics.Verdict = Metrics.bFixturePass ? TEXT("PASS_FIXTURE") : TEXT("FAIL_IMPLEMENTATION");
			}
			Metrics.bStageGatePass = Metrics.bFixturePass;
		}

		uint64 HashInputConfig(const FCarrierV2Stage0Config& Config)
		{
			uint64 Hash = FnvOffset;
			HashMixString(Hash, Config.FixtureId);
			HashMixString(Hash, Config.FixtureName);
			HashMixInt(Hash, Config.SampleCount);
			HashMixInt(Hash, Config.PlateCount);
			HashMixInt(Hash, Config.Seed);
			HashMixDouble(Hash, Config.RayEpsilon);
			HashMixInt(Hash, Config.bUseFibonacciSubstrate ? 1 : 0);
			HashMixInt(Hash, Config.bDeliberateTopologyHole ? 1 : 0);
			HashMixInt(Hash, Config.bDeliberateDuplicatedOverlap ? 1 : 0);
			return Hash;
		}

		uint64 HashCarrierState(const FCarrierV2BuildState& State)
		{
			uint64 Hash = HashInputConfig(State.Config);
			HashMixInt(Hash, State.Samples.Num());
			HashMixInt(Hash, State.Triangles.Num());
			for (const FCarrierV2SubstrateSample& Sample : State.Samples)
			{
				HashMixInt(Hash, Sample.SampleId);
				HashMixDouble(Hash, Sample.UnitPosition.X);
				HashMixDouble(Hash, Sample.UnitPosition.Y);
				HashMixDouble(Hash, Sample.UnitPosition.Z);
			}
			for (const FCarrierV2SubstrateTriangle& Triangle : State.Triangles)
			{
				HashMixInt(Hash, Triangle.TriangleId);
				HashMixInt(Hash, Triangle.SampleIds[0]);
				HashMixInt(Hash, Triangle.SampleIds[1]);
				HashMixInt(Hash, Triangle.SampleIds[2]);
			}
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				HashMixInt(Hash, Plate.PlateId);
				HashMixInt(Hash, Plate.LocalVertices.Num());
				HashMixInt(Hash, Plate.LocalTriangles.Num());
				for (const FCarrierV2PlateVertex& Vertex : Plate.LocalVertices)
				{
					HashMixInt(Hash, Vertex.SourceSampleId);
					HashMixDouble(Hash, Vertex.UnitPosition.X);
					HashMixDouble(Hash, Vertex.UnitPosition.Y);
					HashMixDouble(Hash, Vertex.UnitPosition.Z);
					HashMixDouble(Hash, Vertex.Material.ContinentalFraction);
				}
				for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
				{
					HashMixInt(Hash, Triangle.SourceTriangleId);
					HashMixInt(Hash, Triangle.SourceSampleIds[0]);
					HashMixInt(Hash, Triangle.SourceSampleIds[1]);
					HashMixInt(Hash, Triangle.SourceSampleIds[2]);
				}
			}
			return Hash;
		}

		uint64 HashProjectionMetrics(const FCarrierV2Stage0Metrics& Metrics, const bool bIncludeMetricHash)
		{
			uint64 Hash = FnvOffset;
			HashMixString(Hash, Metrics.InputHash);
			HashMixString(Hash, Metrics.CarrierHash);
			HashMixInt(Hash, Metrics.GlobalSampleCount);
			HashMixInt(Hash, Metrics.GlobalTriangleCount);
			HashMixInt(Hash, Metrics.LocalPlateVertexCountSum);
			HashMixInt(Hash, Metrics.LocalPlateTriangleCountSum);
			HashMixInt(Hash, Metrics.TopologyDuplicateErrorCount);
			HashMixInt(Hash, Metrics.TopologyHoleErrorCount);
			HashMixInt(Hash, Metrics.RawHitCountTotal);
			HashMixInt(Hash, Metrics.RayTriangleTestCount);
			HashMixInt(Hash, Metrics.NonDegenerateMissCount);
			HashMixInt(Hash, Metrics.NonDegenerateOverlapCount);
			HashMixInt(Hash, Metrics.BoundaryDegenerateCount);
			HashMixInt(Hash, Metrics.BoundaryPolicySelectedCount);
			HashMixInt(Hash, Metrics.ProjectionReadsGlobalOwnerCount);
			HashMixDouble(Hash, Metrics.MaterialAuthorityProjectedDelta);
			HashMixString(Hash, Metrics.ExpectedFailureReason);
			HashMixString(Hash, Metrics.ObservedFailureReason);
			HashMixString(Hash, Metrics.Verdict);
			if (bIncludeMetricHash)
			{
				HashMixString(Hash, Metrics.MetricsHash);
			}
			return Hash;
		}

		bool ProjectAndMeasure(FCarrierV2BuildState& State, FCarrierV2Stage0Metrics& Metrics)
		{
			const double ProjectionStart = FPlatformTime::Seconds();
			ComputeTopologyMetrics(State, Metrics);

			for (const FCarrierV2SubstrateSample& Sample : State.Samples)
			{
				TArray<FCarrierV2ProjectionHit, TInlineAllocator<16>> Hits;
				if (State.SampleRayCandidates.IsValidIndex(Sample.SampleId))
				{
					for (const FCarrierV2RayCandidateRef& CandidateRef : State.SampleRayCandidates[Sample.SampleId])
					{
						if (!State.Plates.IsValidIndex(CandidateRef.PlateId))
						{
							continue;
						}
						const FCarrierV2Plate& Plate = State.Plates[CandidateRef.PlateId];
						if (!Plate.LocalTriangles.IsValidIndex(CandidateRef.LocalTriangleId))
						{
							continue;
						}
						const FCarrierV2PlateTriangle& Triangle = Plate.LocalTriangles[CandidateRef.LocalTriangleId];
						if (!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[0]) ||
							!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[1]) ||
							!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[2]))
						{
							continue;
						}

						FVector3d Barycentric;
						double HitT = 0.0;
						bool bBoundaryDegenerate = false;
						++Metrics.RayTriangleTestCount;
						if (IntersectRayTriangle(
							Sample.UnitPosition,
							Plate.LocalVertices[Triangle.LocalVertexIds[0]].UnitPosition,
							Plate.LocalVertices[Triangle.LocalVertexIds[1]].UnitPosition,
							Plate.LocalVertices[Triangle.LocalVertexIds[2]].UnitPosition,
							State.Config.RayEpsilon,
							Barycentric,
							HitT,
							bBoundaryDegenerate))
						{
							FCarrierV2ProjectionHit Hit;
							Hit.SampleId = Sample.SampleId;
							Hit.PlateId = Plate.PlateId;
							Hit.LocalTriangleId = Triangle.LocalTriangleId;
							Hit.SourceTriangleId = Triangle.SourceTriangleId;
							Hit.Barycentric = Barycentric;
							Hit.HitT = HitT;
							Hit.bBoundaryDegenerate = bBoundaryDegenerate;
							Hit.ContinentalFraction = InterpolateContinentalFraction(Plate, Triangle, Barycentric);
							Hits.Add(Hit);
						}
					}
				}

				Metrics.RawHitCountTotal += Hits.Num();
				if (Hits.IsEmpty())
				{
					++Metrics.NonDegenerateMissCount;
					continue;
				}

				bool bAllBoundary = true;
				bool bAnyBoundary = false;
				for (const FCarrierV2ProjectionHit& Hit : Hits)
				{
					bAllBoundary = bAllBoundary && Hit.bBoundaryDegenerate;
					bAnyBoundary = bAnyBoundary || Hit.bBoundaryDegenerate;
				}

				if (bAnyBoundary)
				{
					++Metrics.BoundaryDegenerateCount;
				}

				if (Hits.Num() > 1)
				{
					if (bAllBoundary)
					{
						++Metrics.BoundaryPolicySelectedCount;
					}
					else
					{
						++Metrics.NonDegenerateOverlapCount;
					}
				}

				const FCarrierV2ProjectionHit& SelectedHit = Hits[0];
				const double ExpectedContinental = ExpectedMaterialForSample(Sample.SampleId, State.Config.Seed).ContinentalFraction;
				Metrics.MaterialAuthorityProjectedDelta += FMath::Abs(ExpectedContinental - SelectedHit.ContinentalFraction) * Sample.AreaWeight;
			}

			Metrics.ProjectionKernelMs = (FPlatformTime::Seconds() - ProjectionStart) * 1000.0;
			return true;
		}

		bool BuildStateForConfig(const FCarrierV2Stage0Config& Config, FCarrierV2BuildState& OutState, FCarrierV2Stage0Metrics& Metrics, FString& OutError)
		{
			OutState = FCarrierV2BuildState();
			OutState.Config = Config;

			const double SubstrateStart = FPlatformTime::Seconds();
			if (Config.bUseFibonacciSubstrate)
			{
				GenerateFibonacciSamples(OutState);
				if (!BuildSphericalDelaunay(OutState, OutError))
				{
					return false;
				}
			}
			else
			{
				BuildHandcraftedOctahedron(OutState);
			}
			Metrics.BuildSubstrateMs = (FPlatformTime::Seconds() - SubstrateStart) * 1000.0;

			const double PlateLocalStart = FPlatformTime::Seconds();
			if (!BuildPlateLocalState(OutState, OutError))
			{
				return false;
			}
			Metrics.BuildPlateLocalMs = (FPlatformTime::Seconds() - PlateLocalStart) * 1000.0;
			return true;
		}

		bool RunFixtureOnce(const FCarrierV2Stage0Config& Config, FCarrierV2FixtureResult& OutResult)
		{
			OutResult = FCarrierV2FixtureResult();
			OutResult.Config = Config;
			OutResult.Metrics.RunId = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutResult.Metrics.FixtureId = Config.FixtureId;
			OutResult.Metrics.FixtureName = Config.FixtureName;
			OutResult.Metrics.FixtureKind = FixtureKindName(Config.FixtureKind);
			OutResult.Metrics.SampleCount = Config.SampleCount;
			OutResult.Metrics.PlateCount = Config.PlateCount;
			OutResult.Metrics.RayEpsilon = Config.RayEpsilon;
			OutResult.Metrics.PartitionPolicyId = Config.PartitionPolicyId;
			OutResult.Metrics.FixtureSubstrateId = Config.FixtureSubstrateId;
			OutResult.Metrics.ExpectedFailureReason = Config.ExpectedFailureReason;
			OutResult.Metrics.InputHash = HashToString(HashInputConfig(Config));

			const double TotalStart = FPlatformTime::Seconds();
			FCarrierV2BuildState State;
			if (!BuildStateForConfig(Config, State, OutResult.Metrics, OutResult.Error))
			{
				OutResult.Metrics.ObservedFailureReason = TEXT("build_failed");
				OutResult.Metrics.Verdict = TEXT("FAIL_IMPLEMENTATION");
				OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
				return false;
			}

			OutResult.Metrics.GlobalSampleCount = State.Samples.Num();
			OutResult.Metrics.GlobalTriangleCount = State.Triangles.Num();
			OutResult.Metrics.TriangleCount = State.Triangles.Num();
			OutResult.Metrics.CarrierHash = HashToString(HashCarrierState(State));

			const double MetricsStart = FPlatformTime::Seconds();
			ProjectAndMeasure(State, OutResult.Metrics);
			OutResult.Metrics.MetricsMs = (FPlatformTime::Seconds() - MetricsStart) * 1000.0 - OutResult.Metrics.ProjectionKernelMs;
			if (OutResult.Metrics.MetricsMs < 0.0)
			{
				OutResult.Metrics.MetricsMs = 0.0;
			}

			FinalizeFixtureVerdict(OutResult.Metrics, Config.FixtureKind);
			OutResult.Metrics.ProjectionHash = HashToString(HashProjectionMetrics(OutResult.Metrics, false));
			OutResult.Metrics.MetricsHash = HashToString(HashProjectionMetrics(OutResult.Metrics, true));
			OutResult.Metrics.PeakMemoryMb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);
			OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
			OutResult.bCompleted = true;
			return true;
		}

		struct FCarrierV2Stage1VertexSnapshot
		{
			int32 PlateId = INDEX_NONE;
			int32 LocalVertexId = INDEX_NONE;
			int32 SourceSampleId = INDEX_NONE;
			FVector3d UnitPosition = FVector3d::ZeroVector;
			FCarrierV2MaterialRecord Material;
		};

		struct FCarrierV2Stage1ProjectionHit
		{
			int32 SampleId = INDEX_NONE;
			int32 PlateId = INDEX_NONE;
			int32 LocalTriangleId = INDEX_NONE;
			int32 SourceTriangleId = INDEX_NONE;
			FVector3d Barycentric = FVector3d::ZeroVector;
			double HitT = 0.0;
			bool bBoundaryDegenerate = false;
			double ContinentalFraction = 0.0;
		};

		struct FCarrierV2Stage1TreeTriangleRef
		{
			int32 PlateId = INDEX_NONE;
			int32 LocalTriangleId = INDEX_NONE;
			int32 SourceTriangleId = INDEX_NONE;
		};

		struct FCarrierV2Stage1PlateQueryRuntime
		{
			int32 PlateId = INDEX_NONE;
			UE::Geometry::FDynamicMesh3 Mesh;
			TMap<int32, FCarrierV2Stage1TreeTriangleRef> TriangleRefs;
			TUniquePtr<UE::Geometry::FDynamicMeshAABBTree3> Tree;
			FVector3d MovedCapCenter = FVector3d(0.0, 0.0, 1.0);
			double MovedCapRadiusRad = UE_DOUBLE_PI;
		};

		FVector3d SafeMotionAxis(const FVector3d& Axis)
		{
			const double Size = Axis.Size();
			return Size > SMALL_NUMBER ? Axis / Size : FVector3d(0.0, 0.0, 1.0);
		}

		FCarrierV2MotionSpec MotionForPlate(const FCarrierV2Stage1Config& Config, const int32 PlateId)
		{
			if (Config.PlateMotions.IsValidIndex(PlateId))
			{
				FCarrierV2MotionSpec Spec = Config.PlateMotions[PlateId];
				Spec.Axis = SafeMotionAxis(Spec.Axis);
				return Spec;
			}
			return FCarrierV2MotionSpec();
		}

		FVector3d RotateActualByGeodeticMotion(const FVector3d& UnitPosition, const FVector3d& Axis, const double AngleRad)
		{
			const FVector3d SafeAxis = SafeMotionAxis(Axis);
			const double C = FMath::Cos(AngleRad);
			const double S = FMath::Sin(AngleRad);
			return (
				UnitPosition * C +
				FVector3d::CrossProduct(SafeAxis, UnitPosition) * S +
				SafeAxis * (FVector3d::DotProduct(SafeAxis, UnitPosition) * (1.0 - C))).GetSafeNormal();
		}

		FVector3d OracleRotateFromSnapshot(const FVector3d& UnitPosition, const FVector3d& Axis, const double AngleRad)
		{
			const FVector3d SafeAxis = SafeMotionAxis(Axis);
			const double Parallel = FVector3d::DotProduct(UnitPosition, SafeAxis);
			const FVector3d ParallelVector = SafeAxis * Parallel;
			const FVector3d Perpendicular = UnitPosition - ParallelVector;
			const FVector3d Side = FVector3d::CrossProduct(SafeAxis, Perpendicular);
			const double C = FMath::Cos(AngleRad);
			const double S = FMath::Sin(AngleRad);
			return (ParallelVector + Perpendicular * C + Side * S).GetSafeNormal();
		}

		bool MaterialRecordsMatch(const FCarrierV2MaterialRecord& A, const FCarrierV2MaterialRecord& B)
		{
			return A.MaterialClass == B.MaterialClass &&
				FMath::IsNearlyEqual(A.ContinentalFraction, B.ContinentalFraction, 1.0e-12) &&
				A.Provenance == B.Provenance;
		}

		uint64 HashStage1Config(const FCarrierV2Stage1Config& Config)
		{
			uint64 Hash = HashInputConfig(Config.BaseConfig);
			HashMixString(Hash, Config.FixtureId);
			HashMixString(Hash, Config.FixtureName);
			HashMixString(Hash, Config.ExpectedMotionClass);
			HashMixInt(Hash, Config.MotionStepCount);
			HashMixDouble(Hash, Config.DtMa);
			HashMixDouble(Hash, Config.PlanetRadiusKm);
			HashMixDouble(Hash, Config.MotionToleranceKm);
			HashMixDouble(Hash, Config.UnitLengthTolerance);
			HashMixDouble(Hash, Config.RayParallelTolerance);
			HashMixDouble(Hash, Config.RayTMinTolerance);
			HashMixDouble(Hash, Config.BarycentricSlop);
			HashMixDouble(Hash, Config.BoundaryBand);
			HashMixDouble(Hash, Config.MotionOracleToleranceKm);
			HashMixDouble(Hash, Config.ExpectedMaxStepKernelMs);
			HashMixInt(Hash, Config.bUseFullTriangleScanForProjection ? 1 : 0);
			HashMixInt(Hash, Config.bRunBruteforceProjectionOracle ? 1 : 0);
			HashMixInt(Hash, Config.bUseAngularCapPlateBroadphase ? 1 : 0);
			HashMixInt(Hash, Config.bRunAllPlateBroadphaseEquivalence ? 1 : 0);
			HashMixDouble(Hash, Config.BroadphaseAngularMarginRad);
			HashMixInt(Hash, Config.bRequireOracleFrameSensitivity ? 1 : 0);
			for (const FCarrierV2MotionSpec& Motion : Config.PlateMotions)
			{
				const FVector3d Axis = SafeMotionAxis(Motion.Axis);
				HashMixDouble(Hash, Axis.X);
				HashMixDouble(Hash, Axis.Y);
				HashMixDouble(Hash, Axis.Z);
				HashMixDouble(Hash, Motion.AngularSpeedRadPerMa);
			}
			return Hash;
		}

		uint64 HashStage1Authority(const FCarrierV2BuildState& State, const FCarrierV2Stage1Config& Config)
		{
			uint64 Hash = HashStage1Config(Config);
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				HashMixInt(Hash, Plate.PlateId);
				for (const FCarrierV2PlateVertex& Vertex : Plate.LocalVertices)
				{
					HashMixInt(Hash, Vertex.LocalVertexId);
					HashMixInt(Hash, Vertex.SourceSampleId);
					HashMixDouble(Hash, Vertex.UnitPosition.X);
					HashMixDouble(Hash, Vertex.UnitPosition.Y);
					HashMixDouble(Hash, Vertex.UnitPosition.Z);
					HashMixInt(Hash, static_cast<int32>(Vertex.Material.MaterialClass));
					HashMixDouble(Hash, Vertex.Material.ContinentalFraction);
					HashMixString(Hash, Vertex.Material.Provenance);
				}
				for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
				{
					HashMixInt(Hash, Triangle.LocalTriangleId);
					HashMixInt(Hash, Triangle.SourceTriangleId);
					HashMixInt(Hash, Triangle.LocalVertexIds[0]);
					HashMixInt(Hash, Triangle.LocalVertexIds[1]);
					HashMixInt(Hash, Triangle.LocalVertexIds[2]);
				}
			}
			return Hash;
		}

		uint64 HashStage1ProjectionMetrics(const FCarrierV2Stage1Metrics& Metrics, const bool bIncludeMetricHash)
		{
			uint64 Hash = FnvOffset;
			HashMixString(Hash, Metrics.ConfigHash);
			HashMixString(Hash, Metrics.PreMotionAuthorityHash);
			HashMixString(Hash, Metrics.PostMotionAuthorityHash);
			HashMixInt(Hash, Metrics.MotionVertexCount);
			HashMixDouble(Hash, Metrics.AnalyticMotionMaxErrorKm);
			HashMixDouble(Hash, Metrics.AnalyticMotionMeanErrorKm);
			HashMixDouble(Hash, Metrics.UnitLengthMaxError);
			HashMixInt(Hash, Metrics.MaterialAttachmentErrorCount);
			HashMixInt(Hash, Metrics.RawMotionMissCount);
			HashMixInt(Hash, Metrics.RawMotionOverlapCount);
			HashMixDouble(Hash, Metrics.RawMotionMissFraction);
			HashMixDouble(Hash, Metrics.RawMotionOverlapFraction);
			HashMixString(Hash, Metrics.TopMissPlatePairs);
			HashMixString(Hash, Metrics.TopOverlapPlatePairs);
			HashMixInt(Hash, Metrics.BoundaryDegenerateCount);
			HashMixInt(Hash, Metrics.BoundaryPolicySelectedCount);
			HashMixInt(Hash, Metrics.DivergentCandidateCount);
			HashMixInt(Hash, Metrics.ConvergentCandidateCount);
			HashMixInt(Hash, Metrics.ThirdPlateIntrusionCount);
			HashMixInt(Hash, Metrics.MaterialInterpolationCount);
			HashMixInt(Hash, Metrics.MotionRepairCount);
			HashMixInt(Hash, Metrics.RemeshDuringMotionCount);
			HashMixInt(Hash, Metrics.PrimaryResolverConsumedCount);
			HashMixInt(Hash, Metrics.ProjectionReadsGlobalOwnerCount);
			HashMixInt(Hash, Metrics.TreeTriangleCountSum);
			HashMixInt(Hash, Metrics.RayQueryCount);
			HashMixInt(Hash, Metrics.AabbHitCountTotal);
			HashMixInt(Hash, Metrics.BruteForceHitCountTotal);
			HashMixInt(Hash, Metrics.AabbBruteforceClassificationMismatchCount);
			HashMixInt(Hash, Metrics.LegacyMovedFrameBruteforceMismatchCount);
			HashMixInt(Hash, Metrics.BroadphaseCandidateQueryCount);
			HashMixInt(Hash, Metrics.BroadphaseSkippedPlateQueryCount);
			HashMixInt(Hash, Metrics.AllPlateEquivalenceRayQueryCount);
			HashMixInt(Hash, Metrics.BroadphaseEquivalenceMismatchCount);
			HashMixInt(Hash, Metrics.RawHitCountTotal);
			HashMixInt(Hash, Metrics.RayTriangleTestCount);
			HashMixInt(Hash, Metrics.bAabbBruteforceEquivalencePass ? 1 : 0);
			HashMixInt(Hash, Metrics.bOracleFrameSensitivityPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bBroadphaseEquivalencePass ? 1 : 0);
			HashMixInt(Hash, Metrics.bMotionOraclePass ? 1 : 0);
			HashMixInt(Hash, Metrics.bPerformanceBudgetPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bProjectionExpectationPass ? 1 : 0);
			if (bIncludeMetricHash)
			{
				HashMixString(Hash, Metrics.ProjectionOutputHash);
				HashMixInt(Hash, Metrics.bReplayDeterministic ? 1 : 0);
				HashMixString(Hash, Metrics.Verdict);
			}
			return Hash;
		}

		void CopyStage1TopologyMetrics(const FCarrierV2BuildState& State, FCarrierV2Stage1Metrics& Metrics)
		{
			Metrics.LocalPlateVertexCountSum = 0;
			Metrics.LocalPlateTriangleCountSum = 0;
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				Metrics.LocalPlateVertexCountSum += Plate.LocalVertices.Num();
				Metrics.LocalPlateTriangleCountSum += Plate.LocalTriangles.Num();
			}
		}

		double Stage1TotalMotionAngle(const FCarrierV2Stage1Config& Config, const int32 PlateId)
		{
			const FCarrierV2MotionSpec Motion = MotionForPlate(Config, PlateId);
			return Motion.AngularSpeedRadPerMa * Config.DtMa * static_cast<double>(Config.MotionStepCount);
		}

		bool IsStage1BoundaryBarycentric(const FVector3d& Barycentric, const double BoundaryBand)
		{
			return FMath::Abs(Barycentric.X) <= BoundaryBand ||
				FMath::Abs(Barycentric.Y) <= BoundaryBand ||
				FMath::Abs(Barycentric.Z) <= BoundaryBand ||
				FMath::Abs(1.0 - Barycentric.X) <= BoundaryBand ||
				FMath::Abs(1.0 - Barycentric.Y) <= BoundaryBand ||
				FMath::Abs(1.0 - Barycentric.Z) <= BoundaryBand;
		}

		bool IsStage1InsideBarycentric(const FVector3d& Barycentric, const double Slop)
		{
			return Barycentric.X >= -Slop &&
				Barycentric.Y >= -Slop &&
				Barycentric.Z >= -Slop &&
				Barycentric.X <= 1.0 + Slop &&
				Barycentric.Y <= 1.0 + Slop &&
				Barycentric.Z <= 1.0 + Slop;
		}

		bool IntersectStage1RayTriangle(
			const FVector3d& Direction,
			const FVector3d& A,
			const FVector3d& B,
			const FVector3d& C,
			const FCarrierV2Stage1Config& Config,
			FVector3d& OutBarycentric,
			double& OutT,
			bool& bOutBoundaryDegenerate)
		{
			const FVector3d SafeDirection = Direction.GetSafeNormal();
			const FVector3d Normal = FVector3d::CrossProduct(B - A, C - A);
			const double NormalSize = Normal.Size();
			if (NormalSize <= SMALL_NUMBER)
			{
				return false;
			}

			const double Denom = FVector3d::DotProduct(Normal, SafeDirection);
			const double NormalizedDenom = FMath::Abs(Denom) / NormalSize;
			if (NormalizedDenom <= Config.RayParallelTolerance)
			{
				return false;
			}

			const double T = FVector3d::DotProduct(Normal, A) / Denom;
			if (T <= Config.RayTMinTolerance)
			{
				return false;
			}

			const FVector3d Q = SafeDirection * T;
			const FVector3d V0 = B - A;
			const FVector3d V1 = C - A;
			const FVector3d V2 = Q - A;
			const double D00 = FVector3d::DotProduct(V0, V0);
			const double D01 = FVector3d::DotProduct(V0, V1);
			const double D11 = FVector3d::DotProduct(V1, V1);
			const double D20 = FVector3d::DotProduct(V2, V0);
			const double D21 = FVector3d::DotProduct(V2, V1);
			const double BaryDenom = D00 * D11 - D01 * D01;
			if (FMath::Abs(BaryDenom) <= SMALL_NUMBER)
			{
				return false;
			}

			const double V = (D11 * D20 - D01 * D21) / BaryDenom;
			const double W = (D00 * D21 - D01 * D20) / BaryDenom;
			const double U = 1.0 - V - W;
			const FVector3d Barycentric(U, V, W);
			if (!IsStage1InsideBarycentric(Barycentric, Config.BarycentricSlop))
			{
				return false;
			}

			OutBarycentric = Barycentric;
			OutT = T;
			bOutBoundaryDegenerate = IsStage1BoundaryBarycentric(Barycentric, Config.BoundaryBand);
			return true;
		}

		void AddStage1ProjectionHit(
			const int32 SampleId,
			const FCarrierV2Plate& Plate,
			const FCarrierV2PlateTriangle& Triangle,
			const FVector3d& Barycentric,
			const double HitT,
			const bool bBoundaryDegenerate,
			FCarrierV2Stage1Metrics& Metrics,
			TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>>& Hits)
		{
			FCarrierV2Stage1ProjectionHit Hit;
			Hit.SampleId = SampleId;
			Hit.PlateId = Plate.PlateId;
			Hit.LocalTriangleId = Triangle.LocalTriangleId;
			Hit.SourceTriangleId = Triangle.SourceTriangleId;
			Hit.Barycentric = Barycentric;
			Hit.HitT = HitT;
			Hit.bBoundaryDegenerate = bBoundaryDegenerate;
			Hit.ContinentalFraction = InterpolateContinentalFraction(Plate, Triangle, Barycentric);
			++Metrics.MaterialInterpolationCount;
			Hits.Add(Hit);
		}

		void TestStage1Triangle(
			const int32 SampleId,
			const FVector3d& Direction,
			const FCarrierV2Stage1Config& Config,
			const FCarrierV2Plate& Plate,
			const FCarrierV2PlateTriangle& Triangle,
			FCarrierV2Stage1Metrics& Metrics,
			TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>>& Hits)
		{
			if (!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[0]) ||
				!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[1]) ||
				!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[2]))
			{
				return;
			}

			FVector3d Barycentric;
			double HitT = 0.0;
			bool bBoundaryDegenerate = false;
			++Metrics.RayTriangleTestCount;
			if (IntersectStage1RayTriangle(
				Direction,
				Plate.LocalVertices[Triangle.LocalVertexIds[0]].UnitPosition,
				Plate.LocalVertices[Triangle.LocalVertexIds[1]].UnitPosition,
				Plate.LocalVertices[Triangle.LocalVertexIds[2]].UnitPosition,
				Config,
				Barycentric,
				HitT,
				bBoundaryDegenerate))
			{
				AddStage1ProjectionHit(SampleId, Plate, Triangle, Barycentric, HitT, bBoundaryDegenerate, Metrics, Hits);
			}
		}

		bool BuildStage1PlateQueryRuntimes(
			const FCarrierV2BuildState& State,
			TArray<TUniquePtr<FCarrierV2Stage1PlateQueryRuntime>>& OutRuntimes,
			FCarrierV2Stage1Metrics& Metrics,
			FString& OutError)
		{
			OutRuntimes.Reset();
			const double AabbStart = FPlatformTime::Seconds();
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				TUniquePtr<FCarrierV2Stage1PlateQueryRuntime> Runtime = MakeUnique<FCarrierV2Stage1PlateQueryRuntime>();
				Runtime->PlateId = Plate.PlateId;
				for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
				{
					if (!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[0]) ||
						!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[1]) ||
						!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[2]))
					{
						OutError = FString::Printf(
							TEXT("Milestone 1 AABB mesh build found invalid local triangle %d on plate %d."),
							Triangle.LocalTriangleId,
							Plate.PlateId);
						return false;
					}

					const int32 A = Runtime->Mesh.AppendVertex(Plate.LocalVertices[Triangle.LocalVertexIds[0]].UnitPosition);
					const int32 B = Runtime->Mesh.AppendVertex(Plate.LocalVertices[Triangle.LocalVertexIds[1]].UnitPosition);
					const int32 C = Runtime->Mesh.AppendVertex(Plate.LocalVertices[Triangle.LocalVertexIds[2]].UnitPosition);
					const int32 TreeTriangleId = Runtime->Mesh.AppendTriangle(A, B, C, Plate.PlateId);
					if (TreeTriangleId < 0)
					{
						OutError = FString::Printf(
							TEXT("Milestone 1 AABB mesh append failed for plate %d local triangle %d with result %d."),
							Plate.PlateId,
							Triangle.LocalTriangleId,
							TreeTriangleId);
						return false;
					}

					FCarrierV2Stage1TreeTriangleRef Ref;
					Ref.PlateId = Plate.PlateId;
					Ref.LocalTriangleId = Triangle.LocalTriangleId;
					Ref.SourceTriangleId = Triangle.SourceTriangleId;
					Runtime->TriangleRefs.Add(TreeTriangleId, Ref);
					++Metrics.TreeTriangleCountSum;
				}

				if (Runtime->Mesh.TriangleCount() > 0)
				{
					Runtime->Tree = MakeUnique<UE::Geometry::FDynamicMeshAABBTree3>(&Runtime->Mesh, true);
					OutRuntimes.Add(MoveTemp(Runtime));
				}
			}
			Metrics.PlateAabbBuildMs = (FPlatformTime::Seconds() - AabbStart) * 1000.0;
			return Metrics.TreeTriangleCountSum > 0;
		}

		double Stage1GeodesicDistanceRad(const FVector3d& A, const FVector3d& B)
		{
			return FMath::Acos(FMath::Clamp(FVector3d::DotProduct(A.GetSafeNormal(), B.GetSafeNormal()), -1.0, 1.0));
		}

		void UpdateStage1MovedBroadphaseCaps(
			const FCarrierV2Stage1Config& Config,
			const FCarrierV2BuildState& State,
			TArray<TUniquePtr<FCarrierV2Stage1PlateQueryRuntime>>& Runtimes)
		{
			for (TUniquePtr<FCarrierV2Stage1PlateQueryRuntime>& Runtime : Runtimes)
			{
				if (!Runtime.IsValid() || !State.Plates.IsValidIndex(Runtime->PlateId))
				{
					continue;
				}

				const FCarrierV2Plate& Plate = State.Plates[Runtime->PlateId];
				FVector3d Center = FVector3d::ZeroVector;
				for (const FCarrierV2PlateVertex& Vertex : Plate.LocalVertices)
				{
					Center += Vertex.UnitPosition;
				}
				Center = Center.GetSafeNormal();
				if (Center.IsNearlyZero())
				{
					Center = Plate.LocalVertices.IsEmpty()
						? FVector3d(0.0, 0.0, 1.0)
						: Plate.LocalVertices[0].UnitPosition.GetSafeNormal();
				}

				double Radius = 0.0;
				for (const FCarrierV2PlateVertex& Vertex : Plate.LocalVertices)
				{
					Radius = FMath::Max(Radius, Stage1GeodesicDistanceRad(Center, Vertex.UnitPosition));
				}
				Runtime->MovedCapCenter = Center;
				Runtime->MovedCapRadiusRad = FMath::Min(UE_DOUBLE_PI, Radius + FMath::Max(0.0, Config.BroadphaseAngularMarginRad));
			}
		}

		bool Stage1BroadphaseAcceptsSample(
			const FCarrierV2Stage1Config& Config,
			const FCarrierV2SubstrateSample& Sample,
			const FCarrierV2Stage1PlateQueryRuntime& Runtime)
		{
			if (!Config.bUseAngularCapPlateBroadphase)
			{
				return true;
			}
			return Stage1GeodesicDistanceRad(Sample.UnitPosition, Runtime.MovedCapCenter) <= Runtime.MovedCapRadiusRad;
		}

		void CollectStage1AabbHits(
			const FCarrierV2Stage1Config& Config,
			const FCarrierV2BuildState& State,
			const FCarrierV2SubstrateSample& Sample,
			const TArray<TUniquePtr<FCarrierV2Stage1PlateQueryRuntime>>& Runtimes,
			FCarrierV2Stage1Metrics& Metrics,
			TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>>& Hits,
			const bool bIgnoreBroadphase = false)
		{
			for (const TUniquePtr<FCarrierV2Stage1PlateQueryRuntime>& Runtime : Runtimes)
			{
				if (!Runtime.IsValid() || !Runtime->Tree.IsValid())
				{
					continue;
				}
				if (!bIgnoreBroadphase && !Stage1BroadphaseAcceptsSample(Config, Sample, *Runtime))
				{
					++Metrics.BroadphaseSkippedPlateQueryCount;
					continue;
				}
				if (Config.bUseAngularCapPlateBroadphase && !bIgnoreBroadphase)
				{
					++Metrics.BroadphaseCandidateQueryCount;
				}
				if (bIgnoreBroadphase)
				{
					++Metrics.AllPlateEquivalenceRayQueryCount;
				}
				const FCarrierV2MotionSpec Motion = MotionForPlate(Config, Runtime->PlateId);
				const FVector3d LocalDirection = RotateActualByGeodeticMotion(
					Sample.UnitPosition,
					Motion.Axis,
					-Stage1TotalMotionAngle(Config, Runtime->PlateId));
				const FRay3d Ray(FVector3d::ZeroVector, LocalDirection);
				UE::Geometry::IMeshSpatial::FQueryOptions QueryOptions;
				QueryOptions.MaxDistance = 2.0;
				TArray<MeshIntersection::FHitIntersectionResult> RawHits;
				++Metrics.RayQueryCount;
				Runtime->Tree->FindAllHitTriangles(Ray, RawHits, QueryOptions);
				Metrics.AabbHitCountTotal += RawHits.Num();

				for (const MeshIntersection::FHitIntersectionResult& RawHit : RawHits)
				{
					if (RawHit.Distance <= Config.RayTMinTolerance ||
						!IsStage1InsideBarycentric(RawHit.BaryCoords, Config.BarycentricSlop))
					{
						continue;
					}
					const FCarrierV2Stage1TreeTriangleRef* Ref = Runtime->TriangleRefs.Find(RawHit.TriangleId);
					if (Ref == nullptr || !State.Plates.IsValidIndex(Ref->PlateId))
					{
						continue;
					}
					const FCarrierV2Plate& Plate = State.Plates[Ref->PlateId];
					if (!Plate.LocalTriangles.IsValidIndex(Ref->LocalTriangleId))
					{
						continue;
					}
					AddStage1ProjectionHit(
						Sample.SampleId,
						Plate,
						Plate.LocalTriangles[Ref->LocalTriangleId],
						RawHit.BaryCoords,
						RawHit.Distance,
						IsStage1BoundaryBarycentric(RawHit.BaryCoords, Config.BoundaryBand),
						Metrics,
						Hits);
				}
			}
		}

		void CollectStage1BruteforceHits(
			const FCarrierV2Stage1Config& Config,
			const FCarrierV2BuildState& State,
			const FCarrierV2SubstrateSample& Sample,
			const TArray<TUniquePtr<FCarrierV2Stage1PlateQueryRuntime>>& Runtimes,
			FCarrierV2Stage1Metrics& Metrics,
			TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>>& Hits)
		{
			for (const TUniquePtr<FCarrierV2Stage1PlateQueryRuntime>& Runtime : Runtimes)
			{
				if (!Runtime.IsValid() || !State.Plates.IsValidIndex(Runtime->PlateId))
				{
					continue;
				}

				const FCarrierV2MotionSpec Motion = MotionForPlate(Config, Runtime->PlateId);
				const FVector3d LocalDirection = RotateActualByGeodeticMotion(
					Sample.UnitPosition,
					Motion.Axis,
					-Stage1TotalMotionAngle(Config, Runtime->PlateId));

				for (const TPair<int32, FCarrierV2Stage1TreeTriangleRef>& Pair : Runtime->TriangleRefs)
				{
					const FCarrierV2Stage1TreeTriangleRef& Ref = Pair.Value;
					if (!State.Plates.IsValidIndex(Ref.PlateId))
					{
						continue;
					}

					const FCarrierV2Plate& Plate = State.Plates[Ref.PlateId];
					if (!Plate.LocalTriangles.IsValidIndex(Ref.LocalTriangleId))
					{
						continue;
					}

					const auto MeshTriangle = Runtime->Mesh.GetTriangle(Pair.Key);
					FVector3d Barycentric;
					double HitT = 0.0;
					bool bBoundaryDegenerate = false;
					++Metrics.RayTriangleTestCount;
					if (IntersectStage1RayTriangle(
						LocalDirection,
						Runtime->Mesh.GetVertex(MeshTriangle[0]),
						Runtime->Mesh.GetVertex(MeshTriangle[1]),
						Runtime->Mesh.GetVertex(MeshTriangle[2]),
						Config,
						Barycentric,
						HitT,
						bBoundaryDegenerate))
					{
						AddStage1ProjectionHit(
							Sample.SampleId,
							Plate,
							Plate.LocalTriangles[Ref.LocalTriangleId],
							Barycentric,
							HitT,
							bBoundaryDegenerate,
							Metrics,
							Hits);
					}
				}
			}
		}

		void CollectStage1LegacyMovedFrameBruteforceHits(
			const FCarrierV2Stage1Config& Config,
			const FCarrierV2BuildState& State,
			const FCarrierV2SubstrateSample& Sample,
			TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>>& Hits)
		{
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				const FCarrierV2MotionSpec Motion = MotionForPlate(Config, Plate.PlateId);
				const FVector3d LocalDirection = RotateActualByGeodeticMotion(
					Sample.UnitPosition,
					Motion.Axis,
					-Stage1TotalMotionAngle(Config, Plate.PlateId));
				for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
				{
					if (!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[0]) ||
						!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[1]) ||
						!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[2]))
					{
						continue;
					}

					FVector3d Barycentric;
					double HitT = 0.0;
					bool bBoundaryDegenerate = false;
					if (IntersectStage1RayTriangle(
						LocalDirection,
						Plate.LocalVertices[Triangle.LocalVertexIds[0]].UnitPosition,
						Plate.LocalVertices[Triangle.LocalVertexIds[1]].UnitPosition,
						Plate.LocalVertices[Triangle.LocalVertexIds[2]].UnitPosition,
						Config,
						Barycentric,
						HitT,
						bBoundaryDegenerate))
					{
						FCarrierV2Stage1ProjectionHit Hit;
						Hit.SampleId = Sample.SampleId;
						Hit.PlateId = Plate.PlateId;
						Hit.LocalTriangleId = Triangle.LocalTriangleId;
						Hit.SourceTriangleId = Triangle.SourceTriangleId;
						Hit.Barycentric = Barycentric;
						Hit.HitT = HitT;
						Hit.bBoundaryDegenerate = bBoundaryDegenerate;
						Hits.Add(Hit);
					}
				}
			}
		}

		FString ClassifyStage1HitSet(const TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>>& Hits)
		{
			if (Hits.IsEmpty())
			{
				return TEXT("miss");
			}

			TArray<int32, TInlineAllocator<8>> UniquePlateIds;
			bool bAnyBoundary = false;
			bool bAllBoundary = true;
			for (const FCarrierV2Stage1ProjectionHit& Hit : Hits)
			{
				UniquePlateIds.AddUnique(Hit.PlateId);
				bAnyBoundary = bAnyBoundary || Hit.bBoundaryDegenerate;
				bAllBoundary = bAllBoundary && Hit.bBoundaryDegenerate;
			}
			if (UniquePlateIds.Num() > 2)
			{
				return TEXT("third_plate_intrusion");
			}
			if (Hits.Num() > 1 && !bAllBoundary)
			{
				return TEXT("overlap");
			}
			if (bAnyBoundary)
			{
				return TEXT("boundary");
			}
			return TEXT("single");
		}

		uint64 Stage1PlatePairKey(const int32 PlateA, const int32 PlateB)
		{
			const uint32 A = static_cast<uint32>(FMath::Min(PlateA, PlateB));
			const uint32 B = static_cast<uint32>(FMath::Max(PlateA, PlateB));
			return (static_cast<uint64>(A) << 32) | static_cast<uint64>(B);
		}

		FString Stage1PlatePairLabel(const uint64 Key)
		{
			const int32 A = static_cast<int32>((Key >> 32) & 0xffffffffull);
			const int32 B = static_cast<int32>(Key & 0xffffffffull);
			return FString::Printf(TEXT("%d/%d"), A, B);
		}

		void AccumulateStage1PairCountsFromPlates(
			const TArray<int32, TInlineAllocator<8>>& PlateIds,
			TMap<uint64, int32>& PairCounts)
		{
			TArray<int32, TInlineAllocator<8>> UniquePlateIds;
			for (const int32 PlateId : PlateIds)
			{
				if (PlateId != INDEX_NONE)
				{
					UniquePlateIds.AddUnique(PlateId);
				}
			}

			if (UniquePlateIds.Num() == 1)
			{
				const uint64 Key = Stage1PlatePairKey(UniquePlateIds[0], UniquePlateIds[0]);
				PairCounts.FindOrAdd(Key) += 1;
				return;
			}

			for (int32 I = 0; I < UniquePlateIds.Num(); ++I)
			{
				for (int32 J = I + 1; J < UniquePlateIds.Num(); ++J)
				{
					const uint64 Key = Stage1PlatePairKey(UniquePlateIds[I], UniquePlateIds[J]);
					PairCounts.FindOrAdd(Key) += 1;
				}
			}
		}

		void AccumulateStage1MissAttribution(
			const FCarrierV2BuildState& State,
			const FCarrierV2SubstrateSample& Sample,
			TMap<uint64, int32>& PairCounts)
		{
			TArray<int32, TInlineAllocator<8>> CandidatePlateIds;
			if (State.SampleRayCandidates.IsValidIndex(Sample.SampleId))
			{
				for (const FCarrierV2RayCandidateRef& Candidate : State.SampleRayCandidates[Sample.SampleId])
				{
					CandidatePlateIds.AddUnique(Candidate.PlateId);
				}
			}
			AccumulateStage1PairCountsFromPlates(CandidatePlateIds, PairCounts);
		}

		void AccumulateStage1OverlapAttribution(
			const TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>>& Hits,
			TMap<uint64, int32>& PairCounts)
		{
			TArray<int32, TInlineAllocator<8>> HitPlateIds;
			for (const FCarrierV2Stage1ProjectionHit& Hit : Hits)
			{
				HitPlateIds.AddUnique(Hit.PlateId);
			}
			AccumulateStage1PairCountsFromPlates(HitPlateIds, PairCounts);
		}

		FString FormatStage1TopPlatePairs(const TMap<uint64, int32>& PairCounts)
		{
			TArray<TPair<uint64, int32>> Entries;
			for (const TPair<uint64, int32>& Pair : PairCounts)
			{
				Entries.Add(Pair);
			}
			Entries.Sort([](const TPair<uint64, int32>& A, const TPair<uint64, int32>& B)
			{
				if (A.Value != B.Value)
				{
					return A.Value > B.Value;
				}
				return A.Key < B.Key;
			});

			const int32 Count = FMath::Min(Entries.Num(), 8);
			if (Count == 0)
			{
				return TEXT("none");
			}

			FString Summary;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				if (!Summary.IsEmpty())
				{
					Summary += TEXT(", ");
				}
				Summary += FString::Printf(
					TEXT("%s:%d"),
					*Stage1PlatePairLabel(Entries[Index].Key),
					Entries[Index].Value);
			}
			return Summary;
		}

		void AccumulateStage1ProjectionClassification(
			const TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>>& Hits,
			FCarrierV2Stage1Metrics& Metrics)
		{
			Metrics.RawHitCountTotal += Hits.Num();
			if (Hits.IsEmpty())
			{
				++Metrics.RawMotionMissCount;
				++Metrics.DivergentCandidateCount;
				return;
			}

			TArray<int32, TInlineAllocator<8>> UniquePlateIds;
			bool bAnyBoundary = false;
			bool bAllBoundary = true;
			for (const FCarrierV2Stage1ProjectionHit& Hit : Hits)
			{
				UniquePlateIds.AddUnique(Hit.PlateId);
				bAnyBoundary = bAnyBoundary || Hit.bBoundaryDegenerate;
				bAllBoundary = bAllBoundary && Hit.bBoundaryDegenerate;
			}
			if (bAnyBoundary)
			{
				++Metrics.BoundaryDegenerateCount;
			}
			if (UniquePlateIds.Num() > 2)
			{
				++Metrics.ThirdPlateIntrusionCount;
			}
			if (Hits.Num() > 1)
			{
				if (bAllBoundary)
				{
					++Metrics.BoundaryPolicySelectedCount;
				}
				else
				{
					++Metrics.RawMotionOverlapCount;
					++Metrics.ConvergentCandidateCount;
				}
			}
		}

		void ProjectStage1MovedGeometry(
			const FCarrierV2Stage1Config& Config,
			const FCarrierV2BuildState& State,
			const TArray<TUniquePtr<FCarrierV2Stage1PlateQueryRuntime>>& Runtimes,
			FCarrierV2Stage1Metrics& Metrics)
		{
			const double ProjectionStart = FPlatformTime::Seconds();
			TMap<uint64, int32> MissPairCounts;
			TMap<uint64, int32> OverlapPairCounts;
			for (const FCarrierV2SubstrateSample& Sample : State.Samples)
			{
				TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>> AabbHits;
				CollectStage1AabbHits(Config, State, Sample, Runtimes, Metrics, AabbHits);
				AccumulateStage1ProjectionClassification(AabbHits, Metrics);
				if (AabbHits.IsEmpty())
				{
					AccumulateStage1MissAttribution(State, Sample, MissPairCounts);
				}
				else if (AabbHits.Num() > 1 && ClassifyStage1HitSet(AabbHits) == TEXT("overlap"))
				{
					AccumulateStage1OverlapAttribution(AabbHits, OverlapPairCounts);
				}

				if (Config.bRunAllPlateBroadphaseEquivalence)
				{
					TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>> AllPlateHits;
					CollectStage1AabbHits(Config, State, Sample, Runtimes, Metrics, AllPlateHits, true);
					if (ClassifyStage1HitSet(AabbHits) != ClassifyStage1HitSet(AllPlateHits))
					{
						++Metrics.BroadphaseEquivalenceMismatchCount;
					}
				}

				if (Config.bRunBruteforceProjectionOracle)
				{
					TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>> BruteforceHits;
					CollectStage1BruteforceHits(Config, State, Sample, Runtimes, Metrics, BruteforceHits);
					Metrics.BruteForceHitCountTotal += BruteforceHits.Num();
					if (ClassifyStage1HitSet(AabbHits) != ClassifyStage1HitSet(BruteforceHits))
					{
						++Metrics.AabbBruteforceClassificationMismatchCount;
					}

					if (Config.bRequireOracleFrameSensitivity)
					{
						TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>> LegacyMovedFrameHits;
						CollectStage1LegacyMovedFrameBruteforceHits(Config, State, Sample, LegacyMovedFrameHits);
						if (ClassifyStage1HitSet(BruteforceHits) != ClassifyStage1HitSet(LegacyMovedFrameHits))
						{
							++Metrics.LegacyMovedFrameBruteforceMismatchCount;
						}
					}
				}
			}
			if (Metrics.GlobalSampleCount > 0)
			{
				Metrics.RawMotionMissFraction =
					static_cast<double>(Metrics.RawMotionMissCount) / static_cast<double>(Metrics.GlobalSampleCount);
				Metrics.RawMotionOverlapFraction =
					static_cast<double>(Metrics.RawMotionOverlapCount) / static_cast<double>(Metrics.GlobalSampleCount);
			}
			Metrics.TopMissPlatePairs = FormatStage1TopPlatePairs(MissPairCounts);
			Metrics.TopOverlapPlatePairs = FormatStage1TopPlatePairs(OverlapPairCounts);
			Metrics.InverseRayProjectionKernelMs = (FPlatformTime::Seconds() - ProjectionStart) * 1000.0;
			Metrics.ProjectionKernelMs = Metrics.InverseRayProjectionKernelMs;
		}

		void ProjectStage1MovedGeometry(
			const FCarrierV2Stage1Config& Config,
			const FCarrierV2BuildState& State,
			FCarrierV2Stage1Metrics& Metrics)
		{
			const double ProjectionStart = FPlatformTime::Seconds();
			for (const FCarrierV2SubstrateSample& Sample : State.Samples)
			{
				TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>> Hits;
				if (Config.bUseFullTriangleScanForProjection)
				{
					for (const FCarrierV2Plate& Plate : State.Plates)
					{
						for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
						{
							TestStage1Triangle(Sample.SampleId, Sample.UnitPosition, Config, Plate, Triangle, Metrics, Hits);
						}
					}
				}
				else if (State.SampleRayCandidates.IsValidIndex(Sample.SampleId))
				{
					for (const FCarrierV2RayCandidateRef& CandidateRef : State.SampleRayCandidates[Sample.SampleId])
					{
						if (!State.Plates.IsValidIndex(CandidateRef.PlateId))
						{
							continue;
						}
						const FCarrierV2Plate& Plate = State.Plates[CandidateRef.PlateId];
						if (!Plate.LocalTriangles.IsValidIndex(CandidateRef.LocalTriangleId))
						{
							continue;
						}
						TestStage1Triangle(
							Sample.SampleId,
							Sample.UnitPosition,
							Config,
							Plate,
							Plate.LocalTriangles[CandidateRef.LocalTriangleId],
							Metrics,
							Hits);
					}
				}
				AccumulateStage1ProjectionClassification(Hits, Metrics);
			}
			Metrics.ProjectionKernelMs = (FPlatformTime::Seconds() - ProjectionStart) * 1000.0;
			Metrics.InverseRayProjectionKernelMs = 0.0;
			Metrics.bAabbBruteforceEquivalencePass = !Config.bRunBruteforceProjectionOracle;
		}

		void ApplyStage1MotionAndMeasure(
			const FCarrierV2Stage1Config& Config,
			FCarrierV2BuildState& State,
			FCarrierV2Stage1Metrics& Metrics)
		{
			TArray<FCarrierV2Stage1VertexSnapshot> Snapshots;
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				for (const FCarrierV2PlateVertex& Vertex : Plate.LocalVertices)
				{
					FCarrierV2Stage1VertexSnapshot Snapshot;
					Snapshot.PlateId = Plate.PlateId;
					Snapshot.LocalVertexId = Vertex.LocalVertexId;
					Snapshot.SourceSampleId = Vertex.SourceSampleId;
					Snapshot.UnitPosition = Vertex.UnitPosition;
					Snapshot.Material = Vertex.Material;
					Snapshots.Add(Snapshot);
				}
			}
			Metrics.MotionVertexCount = Snapshots.Num();

			const double MotionStart = FPlatformTime::Seconds();
			for (int32 Step = 0; Step < Config.MotionStepCount; ++Step)
			{
				for (FCarrierV2Plate& Plate : State.Plates)
				{
					const FCarrierV2MotionSpec Motion = MotionForPlate(Config, Plate.PlateId);
					const double Angle = Motion.AngularSpeedRadPerMa * Config.DtMa;
					for (FCarrierV2PlateVertex& Vertex : Plate.LocalVertices)
					{
						Vertex.UnitPosition = RotateActualByGeodeticMotion(Vertex.UnitPosition, Motion.Axis, Angle);
					}
				}
			}
			Metrics.MotionApplyMs = (FPlatformTime::Seconds() - MotionStart) * 1000.0;

			double ErrorSumKm = 0.0;
			for (const FCarrierV2Stage1VertexSnapshot& Snapshot : Snapshots)
			{
				if (!State.Plates.IsValidIndex(Snapshot.PlateId))
				{
					++Metrics.MaterialAttachmentErrorCount;
					continue;
				}
				const FCarrierV2Plate& Plate = State.Plates[Snapshot.PlateId];
				if (!Plate.LocalVertices.IsValidIndex(Snapshot.LocalVertexId))
				{
					++Metrics.MaterialAttachmentErrorCount;
					continue;
				}
				const FCarrierV2PlateVertex& Vertex = Plate.LocalVertices[Snapshot.LocalVertexId];
				const FCarrierV2MotionSpec Motion = MotionForPlate(Config, Plate.PlateId);
				const double TotalAngle = Motion.AngularSpeedRadPerMa * Config.DtMa * static_cast<double>(Config.MotionStepCount);
				const FVector3d Expected = OracleRotateFromSnapshot(Snapshot.UnitPosition, Motion.Axis, TotalAngle);
				const double ErrorKm = (Vertex.UnitPosition - Expected).Size() * Config.PlanetRadiusKm;
				Metrics.AnalyticMotionMaxErrorKm = FMath::Max(Metrics.AnalyticMotionMaxErrorKm, ErrorKm);
				ErrorSumKm += ErrorKm;
				Metrics.UnitLengthMaxError = FMath::Max(Metrics.UnitLengthMaxError, FMath::Abs(Vertex.UnitPosition.Size() - 1.0));
				if (Vertex.SourceSampleId != Snapshot.SourceSampleId || !MaterialRecordsMatch(Vertex.Material, Snapshot.Material))
				{
					++Metrics.MaterialAttachmentErrorCount;
				}
			}

			if (Metrics.MotionVertexCount > 0)
			{
				Metrics.AnalyticMotionMeanErrorKm = ErrorSumKm / static_cast<double>(Metrics.MotionVertexCount);
			}
		}

		void FinalizeStage1Verdict(const FCarrierV2Stage1Config& Config, FCarrierV2Stage1Metrics& Metrics)
		{
			Metrics.bMotionOraclePass =
				Metrics.AnalyticMotionMaxErrorKm <= Config.MotionOracleToleranceKm &&
				Metrics.AnalyticMotionMeanErrorKm <= Config.MotionOracleToleranceKm;
			Metrics.bUnitLengthPass = Metrics.UnitLengthMaxError <= Config.UnitLengthTolerance;
			Metrics.bMaterialAttachmentPass = Metrics.MaterialAttachmentErrorCount == 0;
			Metrics.bPerformanceBudgetPass =
				Config.ExpectedMaxStepKernelMs <= 0.0 ||
				Metrics.StepKernelMs <= Config.ExpectedMaxStepKernelMs;
			Metrics.bAabbBruteforceEquivalencePass =
				!Config.bRunBruteforceProjectionOracle ||
				Metrics.AabbBruteforceClassificationMismatchCount == 0;
			Metrics.bOracleFrameSensitivityPass =
				!Config.bRequireOracleFrameSensitivity ||
				Metrics.LegacyMovedFrameBruteforceMismatchCount > 0;
			Metrics.bBroadphaseEquivalencePass =
				!Config.bRunAllPlateBroadphaseEquivalence ||
				Metrics.BroadphaseEquivalenceMismatchCount == 0;
			Metrics.bNoRepairOrRemeshPass =
				Metrics.MotionRepairCount == 0 &&
				Metrics.RemeshDuringMotionCount == 0 &&
				Metrics.PrimaryResolverConsumedCount == 0;
			Metrics.bProjectionExpectationPass = true;
			if (Config.bRequireNoMotionGapOrOverlap)
			{
				Metrics.bProjectionExpectationPass =
					Metrics.RawMotionMissCount == 0 &&
					Metrics.RawMotionOverlapCount == 0;
			}
			if (Config.bRequireDivergentCandidate)
			{
				Metrics.bProjectionExpectationPass =
					Metrics.bProjectionExpectationPass &&
					Metrics.RawMotionMissCount > 0 &&
					Metrics.DivergentCandidateCount > 0;
			}
			if (Config.bRequireConvergentCandidate)
			{
				Metrics.bProjectionExpectationPass =
					Metrics.bProjectionExpectationPass &&
					Metrics.RawMotionOverlapCount > 0 &&
					Metrics.ConvergentCandidateCount > 0;
			}
			if (Config.bRequireThirdPlateIntrusion)
			{
				Metrics.bProjectionExpectationPass =
					Metrics.bProjectionExpectationPass &&
					Metrics.ThirdPlateIntrusionCount > 0;
			}

			Metrics.bFixturePass =
				Metrics.bMotionOraclePass &&
				Metrics.bUnitLengthPass &&
				Metrics.bMaterialAttachmentPass &&
				Metrics.bPerformanceBudgetPass &&
				Metrics.bAabbBruteforceEquivalencePass &&
				Metrics.bOracleFrameSensitivityPass &&
				Metrics.bBroadphaseEquivalencePass &&
				Metrics.bProjectionExpectationPass &&
				Metrics.bNoRepairOrRemeshPass &&
				Metrics.ProjectionReadsGlobalOwnerCount == 0;
			Metrics.bStageGatePass = Metrics.bFixturePass;
			Metrics.Verdict = Metrics.bFixturePass ? TEXT("PASS_RIGID_MOTION") : TEXT("FAIL_RIGID_MOTION");
		}

		bool RunStage1FixtureOnce(const FCarrierV2Stage1Config& Config, FCarrierV2Stage1FixtureResult& OutResult)
		{
			OutResult = FCarrierV2Stage1FixtureResult();
			OutResult.Config = Config;
			OutResult.Metrics.RunId = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutResult.Metrics.FixtureId = Config.FixtureId;
			OutResult.Metrics.FixtureName = Config.FixtureName;
			OutResult.Metrics.FixtureKind = FixtureKindName(Config.BaseConfig.FixtureKind);
			OutResult.Metrics.ExpectedMotionClass = Config.ExpectedMotionClass;
			OutResult.Metrics.SourceStatus = Config.SourceStatus;
			OutResult.Metrics.ProjectionCandidatePolicyId = Config.ProjectionCandidatePolicyId;
			OutResult.Metrics.SampleCount = Config.BaseConfig.SampleCount;
			OutResult.Metrics.PlateCount = Config.BaseConfig.PlateCount;
			OutResult.Metrics.MotionStepCount = Config.MotionStepCount;
			OutResult.Metrics.DtMa = Config.DtMa;
			OutResult.Metrics.TotalMotionMa = Config.DtMa * static_cast<double>(Config.MotionStepCount);
			OutResult.Metrics.PlanetRadiusKm = Config.PlanetRadiusKm;
			OutResult.Metrics.MotionToleranceKm = Config.MotionToleranceKm;
			OutResult.Metrics.UnitLengthTolerance = Config.UnitLengthTolerance;
			OutResult.Metrics.RayEpsilon = Config.BaseConfig.RayEpsilon;
			OutResult.Metrics.RayParallelTolerance = Config.RayParallelTolerance;
			OutResult.Metrics.RayTMinTolerance = Config.RayTMinTolerance;
			OutResult.Metrics.BarycentricSlop = Config.BarycentricSlop;
			OutResult.Metrics.BoundaryBand = Config.BoundaryBand;
			OutResult.Metrics.MotionOracleToleranceKm = Config.MotionOracleToleranceKm;
			OutResult.Metrics.ExpectedMaxStepKernelMs = Config.ExpectedMaxStepKernelMs;
			OutResult.Metrics.ConfigHash = HashToString(HashStage1Config(Config));

			const double TotalStart = FPlatformTime::Seconds();
			FCarrierV2BuildState State;
			FCarrierV2Stage0Metrics BuildMetrics;
			if (!BuildStateForConfig(Config.BaseConfig, State, BuildMetrics, OutResult.Error))
			{
				OutResult.Metrics.Verdict = TEXT("FAIL_BUILD");
				OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
				return false;
			}

			OutResult.Metrics.BuildSubstrateMs = BuildMetrics.BuildSubstrateMs;
			OutResult.Metrics.BuildPlateLocalMs = BuildMetrics.BuildPlateLocalMs;
			OutResult.Metrics.GlobalSampleCount = State.Samples.Num();
			OutResult.Metrics.GlobalTriangleCount = State.Triangles.Num();
			OutResult.Metrics.TriangleCount = State.Triangles.Num();
			CopyStage1TopologyMetrics(State, OutResult.Metrics);
			OutResult.Metrics.PreMotionAuthorityHash = HashToString(HashStage1Authority(State, Config));

			TArray<TUniquePtr<FCarrierV2Stage1PlateQueryRuntime>> QueryRuntimes;
			if (!BuildStage1PlateQueryRuntimes(State, QueryRuntimes, OutResult.Metrics, OutResult.Error))
			{
				OutResult.Metrics.Verdict = TEXT("FAIL_PLATE_AABB_BUILD");
				OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
				return false;
			}

			const double MetricsStart = FPlatformTime::Seconds();
			ApplyStage1MotionAndMeasure(Config, State, OutResult.Metrics);
			OutResult.Metrics.PostMotionAuthorityHash = HashToString(HashStage1Authority(State, Config));
			UpdateStage1MovedBroadphaseCaps(Config, State, QueryRuntimes);
			ProjectStage1MovedGeometry(Config, State, QueryRuntimes, OutResult.Metrics);
			OutResult.Metrics.MetricsMs = (FPlatformTime::Seconds() - MetricsStart) * 1000.0 -
				OutResult.Metrics.MotionApplyMs -
				OutResult.Metrics.ProjectionKernelMs;
			if (OutResult.Metrics.MetricsMs < 0.0)
			{
				OutResult.Metrics.MetricsMs = 0.0;
			}

			OutResult.Metrics.PeakMemoryMb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);
			OutResult.Metrics.StepKernelMs =
				OutResult.Metrics.MotionApplyMs +
				OutResult.Metrics.PlateAabbBuildMs +
				OutResult.Metrics.InverseRayProjectionKernelMs;
			OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
			OutResult.Metrics.StepWithDiagnosticsMs = OutResult.Metrics.TotalMs;
			FinalizeStage1Verdict(Config, OutResult.Metrics);
			OutResult.Metrics.ProjectionOutputHash = HashToString(HashStage1ProjectionMetrics(OutResult.Metrics, false));
			OutResult.Metrics.MetricsHash = HashToString(HashStage1ProjectionMetrics(OutResult.Metrics, true));
			OutResult.bCompleted = true;
			return true;
		}

		struct FCarrierV2Stage2ContactHit
		{
			int32 PlateId = INDEX_NONE;
			int32 LocalTriangleId = INDEX_NONE;
			int32 SourceTriangleId = INDEX_NONE;
			FVector3d Barycentric = FVector3d::ZeroVector;
			double HitT = 0.0;
			bool bBoundaryDegenerate = false;
			double ContinentalFraction = 0.0;
		};

		FString JsonIntArray(const TArray<int32>& Values)
		{
			FString Out = TEXT("[");
			for (int32 Index = 0; Index < Values.Num(); ++Index)
			{
				if (Index > 0)
				{
					Out += TEXT(",");
				}
				Out += FString::FromInt(Values[Index]);
			}
			Out += TEXT("]");
			return Out;
		}

		void InsertUniqueContactPlate(
			const FCarrierV2Stage2ContactHit& Hit,
			TArray<int32>& UniquePlateIds,
			TArray<int32>& UniqueLocalTriangleIds)
		{
			for (int32 Index = 0; Index < UniquePlateIds.Num(); ++Index)
			{
				if (UniquePlateIds[Index] == Hit.PlateId)
				{
					return;
				}
				if (UniquePlateIds[Index] > Hit.PlateId)
				{
					UniquePlateIds.Insert(Hit.PlateId, Index);
					UniqueLocalTriangleIds.Insert(Hit.LocalTriangleId, Index);
					return;
				}
			}
			UniquePlateIds.Add(Hit.PlateId);
			UniqueLocalTriangleIds.Add(Hit.LocalTriangleId);
		}

		void InsertUniqueContactHitTriangle(
			const FCarrierV2Stage2ContactHit& Hit,
			TArray<int32>& ContactHitPlateIds,
			TArray<int32>& ContactHitLocalTriangleIds)
		{
			for (int32 Index = 0; Index < ContactHitPlateIds.Num(); ++Index)
			{
				const bool bSameRef =
					ContactHitPlateIds[Index] == Hit.PlateId &&
					ContactHitLocalTriangleIds.IsValidIndex(Index) &&
					ContactHitLocalTriangleIds[Index] == Hit.LocalTriangleId;
				if (bSameRef)
				{
					return;
				}
				const bool bSortBefore =
					ContactHitPlateIds[Index] > Hit.PlateId ||
					(ContactHitPlateIds[Index] == Hit.PlateId &&
						ContactHitLocalTriangleIds.IsValidIndex(Index) &&
						ContactHitLocalTriangleIds[Index] > Hit.LocalTriangleId);
				if (bSortBefore)
				{
					ContactHitPlateIds.Insert(Hit.PlateId, Index);
					ContactHitLocalTriangleIds.Insert(Hit.LocalTriangleId, Index);
					return;
				}
			}
			ContactHitPlateIds.Add(Hit.PlateId);
			ContactHitLocalTriangleIds.Add(Hit.LocalTriangleId);
		}

		uint64 HashStage2Config(const FCarrierV2Stage2Config& Config)
		{
			uint64 Hash = HashStage1Config(Config.MotionConfig);
			HashMixString(Hash, Config.FixtureId);
			HashMixString(Hash, Config.FixtureName);
			HashMixString(Hash, Config.ExpectedProcessClass);
			HashMixString(Hash, Config.ContactPolicyId);
			HashMixInt(Hash, Config.bUseFullTriangleScanForContact ? 1 : 0);
			HashMixInt(Hash, Config.bRequireNoContactCandidates ? 1 : 0);
			HashMixInt(Hash, Config.bRequireContactCandidate ? 1 : 0);
			HashMixInt(Hash, Config.bRequireThirdPlateIntrusion ? 1 : 0);
			HashMixInt(Hash, Config.bRequirePolarityCandidate ? 1 : 0);
			HashMixInt(Hash, Config.ExpectedMinimumContactCandidates);
			HashMixInt(Hash, Config.ExpectedMinimumThirdPlateIntrusions);
			HashMixInt(Hash, Config.ExpectedMinimumPolarityCandidates);
			return Hash;
		}

		uint64 HashStage2ProcessCandidates(
			const FCarrierV2Stage2Config& Config,
			const TArray<FCarrierV2ProcessCandidateRecord>& Candidates)
		{
			uint64 Hash = HashStage2Config(Config);
			for (const FCarrierV2ProcessCandidateRecord& Candidate : Candidates)
			{
				HashMixInt(Hash, Candidate.CandidateId);
				HashMixInt(Hash, Candidate.SampleId);
				HashMixString(Hash, Candidate.CandidateClass);
				HashMixString(Hash, Candidate.EvidenceKind);
				HashMixInt(Hash, Candidate.bThirdPlateVisible ? 1 : 0);
				HashMixInt(Hash, Candidate.bPolarityCandidate ? 1 : 0);
				HashMixInt(Hash, Candidate.bAccepted ? 1 : 0);
				for (const int32 PlateId : Candidate.PlateIds)
				{
					HashMixInt(Hash, PlateId);
				}
				HashMixInt(Hash, -17);
				for (const int32 LocalTriangleId : Candidate.LocalTriangleIds)
				{
					HashMixInt(Hash, LocalTriangleId);
				}
				HashMixInt(Hash, -18);
				for (const int32 PlateId : Candidate.ContactHitPlateIds)
				{
					HashMixInt(Hash, PlateId);
				}
				HashMixInt(Hash, -19);
				for (const int32 LocalTriangleId : Candidate.ContactHitLocalTriangleIds)
				{
					HashMixInt(Hash, LocalTriangleId);
				}
			}
			return Hash;
		}

		uint64 HashStage2Metrics(const FCarrierV2Stage2Metrics& Metrics, const bool bIncludeReplayFields)
		{
			uint64 Hash = FnvOffset;
			HashMixString(Hash, Metrics.ConfigHash);
			HashMixString(Hash, Metrics.PreMotionAuthorityHash);
			HashMixString(Hash, Metrics.PostMotionAuthorityHash);
			HashMixString(Hash, Metrics.ProjectionOutputHash);
			HashMixString(Hash, Metrics.ProcessStateHash);
			HashMixInt(Hash, Metrics.MotionVertexCount);
			HashMixDouble(Hash, Metrics.AnalyticMotionMaxErrorKm);
			HashMixDouble(Hash, Metrics.AnalyticMotionMeanErrorKm);
			HashMixDouble(Hash, Metrics.UnitLengthMaxError);
			HashMixInt(Hash, Metrics.MaterialAttachmentErrorCount);
			HashMixInt(Hash, Metrics.RawMotionMissCount);
			HashMixInt(Hash, Metrics.RawMotionOverlapCount);
			HashMixInt(Hash, Metrics.DivergentCandidateCount);
			HashMixInt(Hash, Metrics.ConvergentCandidateCount);
			HashMixInt(Hash, Metrics.MotionRepairCount);
			HashMixInt(Hash, Metrics.RemeshDuringMotionCount);
			HashMixInt(Hash, Metrics.ProjectionReadsGlobalOwnerCount);
			HashMixInt(Hash, Metrics.ContactCandidateCount);
			HashMixInt(Hash, Metrics.AcceptedConvergenceEvidenceCount);
			HashMixInt(Hash, Metrics.RejectedContactCount);
			HashMixInt(Hash, Metrics.ThirdPlateIntrusionCount);
			HashMixInt(Hash, Metrics.PolarityCandidateCount);
			HashMixInt(Hash, Metrics.ProcessMutationCount);
			HashMixInt(Hash, Metrics.CentroidPrimaryResolutionCount);
			HashMixInt(Hash, Metrics.RandomPrimaryResolutionCount);
			HashMixInt(Hash, Metrics.NearestPrimaryResolutionCount);
			HashMixInt(Hash, Metrics.ProjectionOwnerLabelEvidenceCount);
			HashMixInt(Hash, Metrics.OverlapConsumedBeforeProcessStateCount);
			HashMixInt(Hash, Metrics.MaterialMutationCount);
			HashMixInt(Hash, Metrics.RemeshDuringProcessDryRunCount);
			HashMixInt(Hash, Metrics.GapFillDuringProcessDryRunCount);
			HashMixInt(Hash, Metrics.ContactRawHitCountTotal);
			HashMixInt(Hash, Metrics.ContactRayTriangleTestCount);
			HashMixInt(Hash, Metrics.bContactEvidencePass ? 1 : 0);
			HashMixInt(Hash, Metrics.bDryRunNoMutationPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bNoPrimaryResolverPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bThirdPlatePass ? 1 : 0);
			HashMixInt(Hash, Metrics.bPolarityPass ? 1 : 0);
			if (bIncludeReplayFields)
			{
				HashMixInt(Hash, Metrics.bReplayDeterministic ? 1 : 0);
				HashMixString(Hash, Metrics.Verdict);
			}
			return Hash;
		}

		void InitializeStage1MetricsForStage2(
			const FCarrierV2Stage1Config& Config,
			const FString& RunId,
			FCarrierV2Stage1Metrics& Metrics)
		{
			Metrics.RunId = RunId;
			Metrics.FixtureId = Config.FixtureId;
			Metrics.FixtureName = Config.FixtureName;
			Metrics.FixtureKind = FixtureKindName(Config.BaseConfig.FixtureKind);
			Metrics.ExpectedMotionClass = Config.ExpectedMotionClass;
			Metrics.SourceStatus = Config.SourceStatus;
			Metrics.ProjectionCandidatePolicyId = Config.ProjectionCandidatePolicyId;
			Metrics.SampleCount = Config.BaseConfig.SampleCount;
			Metrics.PlateCount = Config.BaseConfig.PlateCount;
			Metrics.MotionStepCount = Config.MotionStepCount;
			Metrics.DtMa = Config.DtMa;
			Metrics.TotalMotionMa = Config.DtMa * static_cast<double>(Config.MotionStepCount);
			Metrics.PlanetRadiusKm = Config.PlanetRadiusKm;
			Metrics.MotionToleranceKm = Config.MotionToleranceKm;
			Metrics.UnitLengthTolerance = Config.UnitLengthTolerance;
			Metrics.RayEpsilon = Config.BaseConfig.RayEpsilon;
			Metrics.ConfigHash = HashToString(HashStage1Config(Config));
		}

		void CopyStage1MetricsToStage2(
			const FCarrierV2Stage1Metrics& Source,
			FCarrierV2Stage2Metrics& Target)
		{
			Target.SampleCount = Source.SampleCount;
			Target.TriangleCount = Source.TriangleCount;
			Target.PlateCount = Source.PlateCount;
			Target.MotionStepCount = Source.MotionStepCount;
			Target.DtMa = Source.DtMa;
			Target.TotalMotionMa = Source.TotalMotionMa;
			Target.PlanetRadiusKm = Source.PlanetRadiusKm;
			Target.MotionToleranceKm = Source.MotionToleranceKm;
			Target.UnitLengthTolerance = Source.UnitLengthTolerance;
			Target.RayEpsilon = Source.RayEpsilon;
			Target.PreMotionAuthorityHash = Source.PreMotionAuthorityHash;
			Target.PostMotionAuthorityHash = Source.PostMotionAuthorityHash;
			Target.ProjectionOutputHash = Source.ProjectionOutputHash;
			Target.GlobalSampleCount = Source.GlobalSampleCount;
			Target.GlobalTriangleCount = Source.GlobalTriangleCount;
			Target.LocalPlateVertexCountSum = Source.LocalPlateVertexCountSum;
			Target.LocalPlateTriangleCountSum = Source.LocalPlateTriangleCountSum;
			Target.MotionVertexCount = Source.MotionVertexCount;
			Target.AnalyticMotionMaxErrorKm = Source.AnalyticMotionMaxErrorKm;
			Target.AnalyticMotionMeanErrorKm = Source.AnalyticMotionMeanErrorKm;
			Target.UnitLengthMaxError = Source.UnitLengthMaxError;
			Target.MaterialAttachmentErrorCount = Source.MaterialAttachmentErrorCount;
			Target.RawMotionMissCount = Source.RawMotionMissCount;
			Target.RawMotionOverlapCount = Source.RawMotionOverlapCount;
			Target.BoundaryDegenerateCount = Source.BoundaryDegenerateCount;
			Target.BoundaryPolicySelectedCount = Source.BoundaryPolicySelectedCount;
			Target.DivergentCandidateCount = Source.DivergentCandidateCount;
			Target.ConvergentCandidateCount = Source.ConvergentCandidateCount;
			Target.MotionRepairCount = Source.MotionRepairCount;
			Target.RemeshDuringMotionCount = Source.RemeshDuringMotionCount;
			Target.ProjectionReadsGlobalOwnerCount = Source.ProjectionReadsGlobalOwnerCount;
			Target.RawHitCountTotal = Source.RawHitCountTotal;
			Target.RayTriangleTestCount = Source.RayTriangleTestCount;
			Target.bMotionOraclePass = Source.bMotionOraclePass;
			Target.bUnitLengthPass = Source.bUnitLengthPass;
			Target.bMaterialAttachmentPass = Source.bMaterialAttachmentPass;
			Target.bProjectionExpectationPass = Source.bProjectionExpectationPass;
			Target.bNoRepairOrRemeshPass = Source.bNoRepairOrRemeshPass;
			Target.BuildSubstrateMs = Source.BuildSubstrateMs;
			Target.BuildPlateLocalMs = Source.BuildPlateLocalMs;
			Target.MotionApplyMs = Source.MotionApplyMs;
			Target.ProjectionKernelMs = Source.ProjectionKernelMs;
		}

		void TestStage2ContactTriangle(
			const FVector3d& Direction,
			const FCarrierV2Plate& Plate,
			const FCarrierV2PlateTriangle& Triangle,
			const double RayEpsilon,
			FCarrierV2Stage2Metrics& Metrics,
			TArray<FCarrierV2Stage2ContactHit, TInlineAllocator<16>>& Hits)
		{
			if (!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[0]) ||
				!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[1]) ||
				!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[2]))
			{
				return;
			}

			FVector3d Barycentric;
			double HitT = 0.0;
			bool bBoundaryDegenerate = false;
			++Metrics.ContactRayTriangleTestCount;
			if (IntersectRayTriangle(
				Direction,
				Plate.LocalVertices[Triangle.LocalVertexIds[0]].UnitPosition,
				Plate.LocalVertices[Triangle.LocalVertexIds[1]].UnitPosition,
				Plate.LocalVertices[Triangle.LocalVertexIds[2]].UnitPosition,
				RayEpsilon,
				Barycentric,
				HitT,
				bBoundaryDegenerate))
			{
				FCarrierV2Stage2ContactHit Hit;
				Hit.PlateId = Plate.PlateId;
				Hit.LocalTriangleId = Triangle.LocalTriangleId;
				Hit.SourceTriangleId = Triangle.SourceTriangleId;
				Hit.Barycentric = Barycentric;
				Hit.HitT = HitT;
				Hit.bBoundaryDegenerate = bBoundaryDegenerate;
				Hit.ContinentalFraction = InterpolateContinentalFraction(Plate, Triangle, Barycentric);
				Hits.Add(Hit);
			}
		}

		void GatherStage2ContactHits(
			const FCarrierV2Stage2Config& Config,
			const FCarrierV2BuildState& State,
			const FCarrierV2SubstrateSample& Sample,
			FCarrierV2Stage2Metrics& Metrics,
			TArray<FCarrierV2Stage2ContactHit, TInlineAllocator<16>>& Hits)
		{
			if (Config.bUseFullTriangleScanForContact)
			{
				for (const FCarrierV2Plate& Plate : State.Plates)
				{
					for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
					{
						TestStage2ContactTriangle(Sample.UnitPosition, Plate, Triangle, Config.MotionConfig.BaseConfig.RayEpsilon, Metrics, Hits);
					}
				}
				return;
			}

			if (!State.SampleRayCandidates.IsValidIndex(Sample.SampleId))
			{
				return;
			}

			for (const FCarrierV2RayCandidateRef& CandidateRef : State.SampleRayCandidates[Sample.SampleId])
			{
				if (!State.Plates.IsValidIndex(CandidateRef.PlateId))
				{
					continue;
				}
				const FCarrierV2Plate& Plate = State.Plates[CandidateRef.PlateId];
				if (!Plate.LocalTriangles.IsValidIndex(CandidateRef.LocalTriangleId))
				{
					continue;
				}
				TestStage2ContactTriangle(
					Sample.UnitPosition,
					Plate,
					Plate.LocalTriangles[CandidateRef.LocalTriangleId],
					Config.MotionConfig.BaseConfig.RayEpsilon,
					Metrics,
					Hits);
			}
		}

		void DetectStage2ProcessCandidates(
			const FCarrierV2Stage2Config& Config,
			const FCarrierV2BuildState& State,
			FCarrierV2Stage2Metrics& Metrics,
			TArray<FCarrierV2ProcessCandidateRecord>& OutCandidates)
		{
			const double ContactStart = FPlatformTime::Seconds();
			OutCandidates.Reset();

			for (const FCarrierV2SubstrateSample& Sample : State.Samples)
			{
				TArray<FCarrierV2Stage2ContactHit, TInlineAllocator<16>> Hits;
				GatherStage2ContactHits(Config, State, Sample, Metrics, Hits);
				Metrics.ContactRawHitCountTotal += Hits.Num();
				if (Hits.Num() <= 1)
				{
					continue;
				}

				TArray<int32> UniquePlateIds;
				TArray<int32> UniqueLocalTriangleIds;
				TArray<int32> ContactHitPlateIds;
				TArray<int32> ContactHitLocalTriangleIds;
				bool bAllBoundary = true;
				for (const FCarrierV2Stage2ContactHit& Hit : Hits)
				{
					bAllBoundary = bAllBoundary && Hit.bBoundaryDegenerate;
					InsertUniqueContactPlate(Hit, UniquePlateIds, UniqueLocalTriangleIds);
					InsertUniqueContactHitTriangle(Hit, ContactHitPlateIds, ContactHitLocalTriangleIds);
				}

				if (bAllBoundary || UniquePlateIds.Num() < 2)
				{
					++Metrics.RejectedContactCount;
					continue;
				}

				FCarrierV2ProcessCandidateRecord Candidate;
				Candidate.CandidateId = OutCandidates.Num();
				Candidate.SampleId = Sample.SampleId;
				Candidate.PlateIds = UniquePlateIds;
				Candidate.LocalTriangleIds = UniqueLocalTriangleIds;
				Candidate.ContactHitPlateIds = ContactHitPlateIds;
				Candidate.ContactHitLocalTriangleIds = ContactHitLocalTriangleIds;
				Candidate.bThirdPlateVisible = UniquePlateIds.Num() >= 3;
				Candidate.bPolarityCandidate = true;
				Candidate.bAccepted = true;
				Candidate.CandidateClass = Candidate.bThirdPlateVisible
					? TEXT("third_plate_intrusion")
					: TEXT("convergent_contact");
				Candidate.EvidenceKind = Config.bUseFullTriangleScanForContact
					? TEXT("moved_geometry_full_scan_multihit")
					: TEXT("moved_geometry_source_adjacent_multihit");
				OutCandidates.Add(Candidate);

				++Metrics.ContactCandidateCount;
				++Metrics.AcceptedConvergenceEvidenceCount;
				++Metrics.PolarityCandidateCount;
				if (Candidate.bThirdPlateVisible)
				{
					++Metrics.ThirdPlateIntrusionCount;
				}
			}

			Metrics.ContactDetectionMs = (FPlatformTime::Seconds() - ContactStart) * 1000.0;
		}

		void FinalizeStage2Verdict(const FCarrierV2Stage2Config& Config, FCarrierV2Stage2Metrics& Metrics)
		{
			Metrics.bContactEvidencePass = true;
			if (Config.bRequireNoContactCandidates)
			{
				Metrics.bContactEvidencePass = Metrics.ContactCandidateCount == 0;
			}
			if (Config.bRequireContactCandidate)
			{
				Metrics.bContactEvidencePass = Metrics.bContactEvidencePass &&
					Metrics.ContactCandidateCount >= FMath::Max(1, Config.ExpectedMinimumContactCandidates) &&
					Metrics.AcceptedConvergenceEvidenceCount >= FMath::Max(1, Config.ExpectedMinimumContactCandidates);
			}

			Metrics.bThirdPlatePass = !Config.bRequireThirdPlateIntrusion ||
				Metrics.ThirdPlateIntrusionCount >= FMath::Max(1, Config.ExpectedMinimumThirdPlateIntrusions);
			Metrics.bPolarityPass = !Config.bRequirePolarityCandidate ||
				Metrics.PolarityCandidateCount >= FMath::Max(1, Config.ExpectedMinimumPolarityCandidates);
			Metrics.bDryRunNoMutationPass =
				Metrics.ProcessMutationCount == 0 &&
				Metrics.OverlapConsumedBeforeProcessStateCount == 0 &&
				Metrics.MaterialMutationCount == 0 &&
				Metrics.RemeshDuringProcessDryRunCount == 0 &&
				Metrics.GapFillDuringProcessDryRunCount == 0;
			Metrics.bNoPrimaryResolverPass =
				Metrics.CentroidPrimaryResolutionCount == 0 &&
				Metrics.RandomPrimaryResolutionCount == 0 &&
				Metrics.NearestPrimaryResolutionCount == 0;

			Metrics.bFixturePass =
				Metrics.bMotionOraclePass &&
				Metrics.bUnitLengthPass &&
				Metrics.bMaterialAttachmentPass &&
				Metrics.bProjectionExpectationPass &&
				Metrics.bNoRepairOrRemeshPass &&
				Metrics.MotionRepairCount == 0 &&
				Metrics.RemeshDuringMotionCount == 0 &&
				Metrics.ProjectionReadsGlobalOwnerCount == 0 &&
				Metrics.ProjectionOwnerLabelEvidenceCount == 0 &&
				Metrics.bContactEvidencePass &&
				Metrics.bDryRunNoMutationPass &&
				Metrics.bNoPrimaryResolverPass &&
				Metrics.bThirdPlatePass &&
				Metrics.bPolarityPass;
			Metrics.bStageGatePass = Metrics.bFixturePass;

			if (!Metrics.bMotionOraclePass || !Metrics.bProjectionExpectationPass)
			{
				Metrics.Verdict = TEXT("FAIL_INHERITED_MOTION_GATE");
			}
			else if (!Metrics.bContactEvidencePass || !Metrics.bThirdPlatePass || !Metrics.bPolarityPass)
			{
				Metrics.Verdict = TEXT("FAIL_CONTACT_EVIDENCE");
			}
			else if (!Metrics.bDryRunNoMutationPass)
			{
				Metrics.Verdict = TEXT("FAIL_DRY_RUN_MUTATION");
			}
			else if (!Metrics.bNoPrimaryResolverPass || Metrics.ProjectionOwnerLabelEvidenceCount != 0)
			{
				Metrics.Verdict = TEXT("FAIL_PRIMARY_RESOLVER");
			}
			else
			{
				Metrics.Verdict = TEXT("PASS_CONTACT_DRY_RUN");
			}
		}

		bool RunStage2FixtureOnce(const FCarrierV2Stage2Config& Config, FCarrierV2Stage2FixtureResult& OutResult)
		{
			OutResult = FCarrierV2Stage2FixtureResult();
			OutResult.Config = Config;
			OutResult.Metrics.RunId = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutResult.Metrics.FixtureId = Config.FixtureId;
			OutResult.Metrics.FixtureName = Config.FixtureName;
			OutResult.Metrics.FixtureKind = FixtureKindName(Config.MotionConfig.BaseConfig.FixtureKind);
			OutResult.Metrics.ExpectedMotionClass = Config.MotionConfig.ExpectedMotionClass;
			OutResult.Metrics.ExpectedProcessClass = Config.ExpectedProcessClass;
			OutResult.Metrics.SourceStatus = Config.MotionConfig.SourceStatus;
			OutResult.Metrics.ProjectionCandidatePolicyId = Config.MotionConfig.ProjectionCandidatePolicyId;
			OutResult.Metrics.ContactPolicyId = Config.ContactPolicyId;
			OutResult.Metrics.ConfigHash = HashToString(HashStage2Config(Config));

			const double TotalStart = FPlatformTime::Seconds();
			FCarrierV2BuildState State;
			FCarrierV2Stage0Metrics BuildMetrics;
			if (!BuildStateForConfig(Config.MotionConfig.BaseConfig, State, BuildMetrics, OutResult.Error))
			{
				OutResult.Metrics.Verdict = TEXT("FAIL_BUILD");
				OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
				return false;
			}

			FCarrierV2Stage1Metrics Stage1Metrics;
			InitializeStage1MetricsForStage2(Config.MotionConfig, OutResult.Metrics.RunId, Stage1Metrics);
			Stage1Metrics.BuildSubstrateMs = BuildMetrics.BuildSubstrateMs;
			Stage1Metrics.BuildPlateLocalMs = BuildMetrics.BuildPlateLocalMs;
			Stage1Metrics.GlobalSampleCount = State.Samples.Num();
			Stage1Metrics.GlobalTriangleCount = State.Triangles.Num();
			Stage1Metrics.TriangleCount = State.Triangles.Num();
			CopyStage1TopologyMetrics(State, Stage1Metrics);
			Stage1Metrics.PreMotionAuthorityHash = HashToString(HashStage1Authority(State, Config.MotionConfig));

			const double MetricsStart = FPlatformTime::Seconds();
			ApplyStage1MotionAndMeasure(Config.MotionConfig, State, Stage1Metrics);
			Stage1Metrics.PostMotionAuthorityHash = HashToString(HashStage1Authority(State, Config.MotionConfig));
			ProjectStage1MovedGeometry(Config.MotionConfig, State, Stage1Metrics);
			FinalizeStage1Verdict(Config.MotionConfig, Stage1Metrics);
			Stage1Metrics.ProjectionOutputHash = HashToString(HashStage1ProjectionMetrics(Stage1Metrics, false));
			Stage1Metrics.MetricsHash = HashToString(HashStage1ProjectionMetrics(Stage1Metrics, true));

			CopyStage1MetricsToStage2(Stage1Metrics, OutResult.Metrics);
			DetectStage2ProcessCandidates(Config, State, OutResult.Metrics, OutResult.ProcessCandidates);
			OutResult.Metrics.ProcessStateHash = HashToString(HashStage2ProcessCandidates(Config, OutResult.ProcessCandidates));
			FinalizeStage2Verdict(Config, OutResult.Metrics);
			OutResult.Metrics.MetricsMs = (FPlatformTime::Seconds() - MetricsStart) * 1000.0 -
				OutResult.Metrics.MotionApplyMs -
				OutResult.Metrics.ProjectionKernelMs -
				OutResult.Metrics.ContactDetectionMs;
			if (OutResult.Metrics.MetricsMs < 0.0)
			{
				OutResult.Metrics.MetricsMs = 0.0;
			}
			OutResult.Metrics.MetricsHash = HashToString(HashStage2Metrics(OutResult.Metrics, true));
			OutResult.Metrics.PeakMemoryMb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);
			OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
			OutResult.bCompleted = true;
			return true;
		}

		void AddPlateMaterialProfile(
			FCarrierV2Stage3Config& Config,
			const int32 PlateId,
			const ECarrierV2MaterialClass MaterialClass,
			const double OceanicAgeMa,
			const FString& Provenance)
		{
			FCarrierV2Stage3PlateMaterialProfile Profile;
			Profile.PlateId = PlateId;
			Profile.MaterialClass = MaterialClass;
			Profile.OceanicAgeMa = OceanicAgeMa;
			Profile.Provenance = Provenance;
			Config.PlateMaterialProfiles.Add(Profile);
		}

		FCarrierV2Stage3PlateMaterialProfile MakeDefaultStage3PlateProfile(const int32 PlateId)
		{
			FCarrierV2Stage3PlateMaterialProfile Profile;
			Profile.PlateId = PlateId;
			const int32 Bucket = ((PlateId % 4) + 4) % 4;
			if (Bucket == 0)
			{
				Profile.MaterialClass = ECarrierV2MaterialClass::Oceanic;
				Profile.OceanicAgeMa = 120.0;
			}
			else if (Bucket == 1)
			{
				Profile.MaterialClass = ECarrierV2MaterialClass::Oceanic;
				Profile.OceanicAgeMa = 20.0;
			}
			else
			{
				Profile.MaterialClass = ECarrierV2MaterialClass::Continental;
				Profile.OceanicAgeMa = 0.0;
			}
			Profile.Provenance = TEXT("v2_3_deterministic_plate_profile_lab_policy");
			return Profile;
		}

		FCarrierV2Stage3PlateMaterialProfile Stage3ProfileForPlate(
			const FCarrierV2Stage3Config& Config,
			const int32 PlateId)
		{
			for (const FCarrierV2Stage3PlateMaterialProfile& Profile : Config.PlateMaterialProfiles)
			{
				if (Profile.PlateId == PlateId)
				{
					return Profile;
				}
			}
			return MakeDefaultStage3PlateProfile(PlateId);
		}

		uint64 HashStage3Config(const FCarrierV2Stage3Config& Config)
		{
			uint64 Hash = HashStage2Config(Config.ContactConfig);
			HashMixString(Hash, Config.FixtureId);
			HashMixString(Hash, Config.FixtureName);
			HashMixString(Hash, Config.ExpectedProcessClass);
			HashMixString(Hash, Config.ProcessMutationPolicyId);
			HashMixInt(Hash, Config.bRequireSubductionMark ? 1 : 0);
			HashMixInt(Hash, Config.bRequireOceanicAgePolarity ? 1 : 0);
			HashMixInt(Hash, Config.bRequireCollisionCandidate ? 1 : 0);
			HashMixInt(Hash, Config.bRequireProcessEvents ? 1 : 0);
			HashMixInt(Hash, Config.bEnableSlabPull ? 1 : 0);
			HashMixInt(Hash, Config.ExpectedMinimumSubductionEvents);
			HashMixInt(Hash, Config.ExpectedMinimumSubductingTriangleMarks);
			HashMixInt(Hash, Config.ExpectedMinimumCollisionCandidates);
			HashMixInt(Hash, Config.ExpectedMinimumCollisionEvents);
			HashMixInt(Hash, Config.ExpectedMinimumProcessEvents);
			for (const FCarrierV2Stage3PlateMaterialProfile& Profile : Config.PlateMaterialProfiles)
			{
				HashMixInt(Hash, Profile.PlateId);
				HashMixInt(Hash, static_cast<int32>(Profile.MaterialClass));
				HashMixDouble(Hash, Profile.OceanicAgeMa);
				HashMixString(Hash, Profile.Provenance);
			}
			return Hash;
		}

		uint64 HashStage3ProcessState(
			const FCarrierV2Stage3Config& Config,
			const FCarrierV2Stage3Metrics& Metrics,
			const TArray<FCarrierV2ProcessEventRecord>& ProcessEvents,
			const TArray<FCarrierV2TriangleProcessMarkRecord>& TriangleMarks)
		{
			uint64 Hash = HashStage3Config(Config);
			HashMixString(Hash, Metrics.Stage2ProcessStateHash);
			for (const FCarrierV2ProcessEventRecord& Event : ProcessEvents)
			{
				HashMixInt(Hash, Event.EventId);
				HashMixInt(Hash, Event.SourceCandidateId);
				HashMixInt(Hash, Event.SampleId);
				HashMixString(Hash, Event.EventClass);
				HashMixString(Hash, Event.ProcessClass);
				HashMixInt(Hash, Event.SourcePlateId);
				HashMixInt(Hash, Event.DestinationPlateId);
				HashMixInt(Hash, Event.SourceLocalTriangleId);
				HashMixInt(Hash, Event.DestinationLocalTriangleId);
				HashMixInt(Hash, Event.SubductingPlateId);
				HashMixInt(Hash, Event.OverridingPlateId);
				HashMixInt(Hash, Event.SubductingLocalTriangleId);
				HashMixInt(Hash, static_cast<int32>(Event.SourceMaterialClass));
				HashMixInt(Hash, static_cast<int32>(Event.DestinationMaterialClass));
				HashMixDouble(Hash, Event.SourceOceanicAgeMa);
				HashMixDouble(Hash, Event.DestinationOceanicAgeMa);
				HashMixInt(Hash, Event.bOlderOceanicSubducts ? 1 : 0);
				HashMixInt(Hash, Event.bHasProvenance ? 1 : 0);
				HashMixString(Hash, Event.Provenance);
			}
			HashMixInt(Hash, -23);
			for (const FCarrierV2TriangleProcessMarkRecord& Mark : TriangleMarks)
			{
				HashMixInt(Hash, Mark.MarkId);
				HashMixInt(Hash, Mark.EventId);
				HashMixInt(Hash, Mark.SourceCandidateId);
				HashMixInt(Hash, Mark.SampleId);
				HashMixInt(Hash, Mark.PlateId);
				HashMixInt(Hash, Mark.LocalTriangleId);
				HashMixString(Hash, Mark.MarkClass);
				HashMixString(Hash, Mark.Provenance);
			}
			return Hash;
		}

		uint64 HashStage3Metrics(const FCarrierV2Stage3Metrics& Metrics, const bool bIncludeReplayFields)
		{
			uint64 Hash = FnvOffset;
			HashMixString(Hash, Metrics.ConfigHash);
			HashMixString(Hash, Metrics.Stage2ProcessStateHash);
			HashMixString(Hash, Metrics.ProcessStateHash);
			HashMixInt(Hash, Metrics.ContactCandidateCount);
			HashMixInt(Hash, Metrics.AcceptedConvergenceEvidenceCount);
			HashMixInt(Hash, Metrics.ThirdPlateIntrusionCount);
			HashMixInt(Hash, Metrics.ThirdPlatePassthroughCount);
			HashMixInt(Hash, Metrics.ProcessEventCount);
			HashMixInt(Hash, Metrics.ProcessEventWithProvenanceCount);
			HashMixInt(Hash, Metrics.ProcessEventWithoutProvenanceCount);
			HashMixInt(Hash, Metrics.SubductionEventCount);
			HashMixInt(Hash, Metrics.OceanicAgePolarityDecisionCount);
			HashMixInt(Hash, Metrics.OlderOceanicSubductingPassCount);
			HashMixInt(Hash, Metrics.SubductingTriangleMarkCount);
			HashMixInt(Hash, Metrics.CollisionCandidateCount);
			HashMixInt(Hash, Metrics.CollisionEventCount);
			HashMixInt(Hash, Metrics.TerraneDetachTriangleCount);
			HashMixInt(Hash, Metrics.TerraneSutureTriangleCount);
			HashMixInt(Hash, Metrics.MaterialDestroyedWithoutProcessCount);
			HashMixInt(Hash, Metrics.MaterialCreatedWithoutProcessCount);
			HashMixInt(Hash, Metrics.TopologyMutationWithoutEventCount);
			HashMixInt(Hash, Metrics.MaterialMutationCount);
			HashMixInt(Hash, Metrics.RemeshDuringProcessMutationCount);
			HashMixInt(Hash, Metrics.TopologyRebuildDuringProcessMutationCount);
			HashMixInt(Hash, Metrics.GapFillDuringProcessMutationCount);
			HashMixInt(Hash, Metrics.DivergentGenerationDuringProcessMutationCount);
			HashMixInt(Hash, Metrics.TerrainBeautyMutationCount);
			HashMixInt(Hash, Metrics.OwnershipRepairDuringProcessMutationCount);
			HashMixInt(Hash, Metrics.CentroidPrimaryResolutionCount);
			HashMixInt(Hash, Metrics.RandomPrimaryResolutionCount);
			HashMixInt(Hash, Metrics.NearestPrimaryResolutionCount);
			HashMixDouble(Hash, Metrics.SlabPullAxisDeltaDeg);
			HashMixInt(Hash, Metrics.bInheritedStage2Pass ? 1 : 0);
			HashMixInt(Hash, Metrics.bProcessEventProvenancePass ? 1 : 0);
			HashMixInt(Hash, Metrics.bSubductionPolarityPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bCollisionCandidatePass ? 1 : 0);
			HashMixInt(Hash, Metrics.bProcessEventExpectationPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bNoForbiddenMutationPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bSlabPullPass ? 1 : 0);
			if (bIncludeReplayFields)
			{
				HashMixInt(Hash, Metrics.bReplayDeterministic ? 1 : 0);
				HashMixString(Hash, Metrics.Verdict);
			}
			return Hash;
		}

		void CopyStage2MetricsToStage3(
			const FCarrierV2Stage2Metrics& Source,
			FCarrierV2Stage3Metrics& Target)
		{
			Target.ExpectedMotionClass = Source.ExpectedMotionClass;
			Target.SourceStatus = Source.SourceStatus;
			Target.ProjectionCandidatePolicyId = Source.ProjectionCandidatePolicyId;
			Target.ContactPolicyId = Source.ContactPolicyId;
			Target.SampleCount = Source.SampleCount;
			Target.TriangleCount = Source.TriangleCount;
			Target.PlateCount = Source.PlateCount;
			Target.MotionStepCount = Source.MotionStepCount;
			Target.DtMa = Source.DtMa;
			Target.TotalMotionMa = Source.TotalMotionMa;
			Target.PlanetRadiusKm = Source.PlanetRadiusKm;
			Target.MotionToleranceKm = Source.MotionToleranceKm;
			Target.UnitLengthTolerance = Source.UnitLengthTolerance;
			Target.RayEpsilon = Source.RayEpsilon;
			Target.Stage2ProcessStateHash = Source.ProcessStateHash;
			Target.GlobalSampleCount = Source.GlobalSampleCount;
			Target.GlobalTriangleCount = Source.GlobalTriangleCount;
			Target.LocalPlateVertexCountSum = Source.LocalPlateVertexCountSum;
			Target.LocalPlateTriangleCountSum = Source.LocalPlateTriangleCountSum;
			Target.MotionVertexCount = Source.MotionVertexCount;
			Target.AnalyticMotionMaxErrorKm = Source.AnalyticMotionMaxErrorKm;
			Target.AnalyticMotionMeanErrorKm = Source.AnalyticMotionMeanErrorKm;
			Target.UnitLengthMaxError = Source.UnitLengthMaxError;
			Target.MaterialAttachmentErrorCount = Source.MaterialAttachmentErrorCount;
			Target.RawMotionMissCount = Source.RawMotionMissCount;
			Target.RawMotionOverlapCount = Source.RawMotionOverlapCount;
			Target.DivergentCandidateCount = Source.DivergentCandidateCount;
			Target.ConvergentCandidateCount = Source.ConvergentCandidateCount;
			Target.ContactCandidateCount = Source.ContactCandidateCount;
			Target.AcceptedConvergenceEvidenceCount = Source.AcceptedConvergenceEvidenceCount;
			Target.ThirdPlateIntrusionCount = Source.ThirdPlateIntrusionCount;
			Target.PolarityCandidateCount = Source.PolarityCandidateCount;
			Target.MaterialMutationCount = Source.MaterialMutationCount;
			Target.CentroidPrimaryResolutionCount = Source.CentroidPrimaryResolutionCount;
			Target.RandomPrimaryResolutionCount = Source.RandomPrimaryResolutionCount;
			Target.NearestPrimaryResolutionCount = Source.NearestPrimaryResolutionCount;
			Target.ProjectionOwnerLabelEvidenceCount = Source.ProjectionOwnerLabelEvidenceCount;
			Target.OverlapConsumedBeforeProcessStateCount = Source.OverlapConsumedBeforeProcessStateCount;
			Target.bMotionOraclePass = Source.bMotionOraclePass;
			Target.bUnitLengthPass = Source.bUnitLengthPass;
			Target.bMaterialAttachmentPass = Source.bMaterialAttachmentPass;
			Target.bProjectionExpectationPass = Source.bProjectionExpectationPass;
			Target.bContactEvidencePass = Source.bContactEvidencePass;
			Target.bInheritedStage2Pass = Source.bFixturePass && Source.bReplayDeterministic;
			Target.BuildSubstrateMs = Source.BuildSubstrateMs;
			Target.BuildPlateLocalMs = Source.BuildPlateLocalMs;
			Target.MotionApplyMs = Source.MotionApplyMs;
			Target.ProjectionKernelMs = Source.ProjectionKernelMs;
			Target.ContactDetectionMs = Source.ContactDetectionMs;
		}

		void AddStage3TriangleMark(
			FCarrierV2Stage3FixtureResult& Result,
			const FCarrierV2ProcessEventRecord& Event,
			const int32 PlateId,
			const int32 LocalTriangleId,
			const FString& MarkClass,
			const FString& Provenance)
		{
			FCarrierV2TriangleProcessMarkRecord Mark;
			Mark.MarkId = Result.TriangleMarks.Num();
			Mark.EventId = Event.EventId;
			Mark.SourceCandidateId = Event.SourceCandidateId;
			Mark.SampleId = Event.SampleId;
			Mark.PlateId = PlateId;
			Mark.LocalTriangleId = LocalTriangleId;
			Mark.MarkClass = MarkClass;
			Mark.Provenance = Provenance;
			Result.TriangleMarks.Add(Mark);
		}

		bool AddStage3TriangleMarkIfMissing(
			FCarrierV2Stage3FixtureResult& Result,
			const FCarrierV2ProcessEventRecord& Event,
			const int32 PlateId,
			const int32 LocalTriangleId,
			const FString& MarkClass,
			const FString& Provenance)
		{
			for (const FCarrierV2TriangleProcessMarkRecord& Mark : Result.TriangleMarks)
			{
				if (Mark.EventId == Event.EventId &&
					Mark.PlateId == PlateId &&
					Mark.LocalTriangleId == LocalTriangleId &&
					Mark.MarkClass.Equals(MarkClass, ESearchCase::IgnoreCase))
				{
					return false;
				}
			}
			AddStage3TriangleMark(Result, Event, PlateId, LocalTriangleId, MarkClass, Provenance);
			return true;
		}

		int32 AddStage3CandidatePlateMarks(
			FCarrierV2Stage3FixtureResult& Result,
			const FCarrierV2ProcessEventRecord& Event,
			const FCarrierV2ProcessCandidateRecord& Candidate,
			const int32 PlateId,
			const int32 FallbackLocalTriangleId,
			const FString& MarkClass,
			const FString& Provenance)
		{
			int32 AddedCount = 0;
			if (Candidate.ContactHitPlateIds.Num() == Candidate.ContactHitLocalTriangleIds.Num())
			{
				for (int32 Index = 0; Index < Candidate.ContactHitPlateIds.Num(); ++Index)
				{
					if (Candidate.ContactHitPlateIds[Index] == PlateId &&
						AddStage3TriangleMarkIfMissing(
							Result,
							Event,
							PlateId,
							Candidate.ContactHitLocalTriangleIds[Index],
							MarkClass,
							Provenance))
					{
						++AddedCount;
					}
				}
			}

			if (AddedCount == 0 && FallbackLocalTriangleId != INDEX_NONE)
			{
				AddedCount += AddStage3TriangleMarkIfMissing(
					Result,
					Event,
					PlateId,
					FallbackLocalTriangleId,
					MarkClass,
					Provenance)
					? 1
					: 0;
			}
			return AddedCount;
		}

		bool IsOceanic(const FCarrierV2Stage3PlateMaterialProfile& Profile)
		{
			return Profile.MaterialClass == ECarrierV2MaterialClass::Oceanic;
		}

		bool IsContinental(const FCarrierV2Stage3PlateMaterialProfile& Profile)
		{
			return Profile.MaterialClass == ECarrierV2MaterialClass::Continental;
		}

		void AddStage3SubductionEvent(
			const FCarrierV2ProcessCandidateRecord& Candidate,
			const FCarrierV2Stage3PlateMaterialProfile& A,
			const FCarrierV2Stage3PlateMaterialProfile& B,
			const int32 ALocalTriangleId,
			const int32 BLocalTriangleId,
			FCarrierV2Stage3FixtureResult& Result)
		{
			const bool bASubducts = IsOceanic(A) && (!IsOceanic(B) || A.OceanicAgeMa >= B.OceanicAgeMa);
			const FCarrierV2Stage3PlateMaterialProfile& Subducting = bASubducts ? A : B;
			const FCarrierV2Stage3PlateMaterialProfile& Overriding = bASubducts ? B : A;
			const int32 SubductingLocalTriangleId = bASubducts ? ALocalTriangleId : BLocalTriangleId;
			const bool bBothOceanic = IsOceanic(A) && IsOceanic(B);
			const bool bOlderOceanicSubducts = bBothOceanic &&
				((bASubducts && A.OceanicAgeMa >= B.OceanicAgeMa) || (!bASubducts && B.OceanicAgeMa >= A.OceanicAgeMa));

			FCarrierV2ProcessEventRecord Event;
			Event.EventId = Result.ProcessEvents.Num();
			Event.SourceCandidateId = Candidate.CandidateId;
			Event.SampleId = Candidate.SampleId;
			Event.EventClass = bBothOceanic ? TEXT("ocean_ocean_subduction_mark") : TEXT("ocean_continent_subduction_mark");
			Event.ProcessClass = bBothOceanic ? TEXT("ocean_ocean_age_polarity") : TEXT("mixed_oceanic_subduction");
			Event.SourcePlateId = A.PlateId;
			Event.DestinationPlateId = B.PlateId;
			Event.SourceLocalTriangleId = ALocalTriangleId;
			Event.DestinationLocalTriangleId = BLocalTriangleId;
			Event.SubductingPlateId = Subducting.PlateId;
			Event.OverridingPlateId = Overriding.PlateId;
			Event.SubductingLocalTriangleId = SubductingLocalTriangleId;
			Event.SourceMaterialClass = A.MaterialClass;
			Event.DestinationMaterialClass = B.MaterialClass;
			Event.SourceOceanicAgeMa = A.OceanicAgeMa;
			Event.DestinationOceanicAgeMa = B.OceanicAgeMa;
			Event.bOlderOceanicSubducts = bOlderOceanicSubducts;
			Event.bHasProvenance = true;
			Event.Provenance = TEXT("accepted_v2_2_contact_plus_plate_material_profile");
			Result.ProcessEvents.Add(Event);
			const int32 SubductingMarkCount = AddStage3CandidatePlateMarks(
				Result,
				Event,
				Candidate,
				Subducting.PlateId,
				SubductingLocalTriangleId,
				TEXT("subducting"),
				Event.Provenance);

			++Result.Metrics.ProcessEventCount;
			++Result.Metrics.ProcessEventWithProvenanceCount;
			++Result.Metrics.SubductionEventCount;
			Result.Metrics.SubductingTriangleMarkCount += SubductingMarkCount;
			if (bBothOceanic)
			{
				++Result.Metrics.OceanicAgePolarityDecisionCount;
				if (bOlderOceanicSubducts)
				{
					++Result.Metrics.OlderOceanicSubductingPassCount;
				}
			}
		}

		void AddStage3CollisionEvent(
			const FCarrierV2ProcessCandidateRecord& Candidate,
			const FCarrierV2Stage3PlateMaterialProfile& A,
			const FCarrierV2Stage3PlateMaterialProfile& B,
			const int32 ALocalTriangleId,
			const int32 BLocalTriangleId,
			FCarrierV2Stage3FixtureResult& Result)
		{
			FCarrierV2ProcessEventRecord Event;
			Event.EventId = Result.ProcessEvents.Num();
			Event.SourceCandidateId = Candidate.CandidateId;
			Event.SampleId = Candidate.SampleId;
			Event.EventClass = TEXT("continental_collision_suture_candidate");
			Event.ProcessClass = TEXT("continental_collision_candidate");
			Event.SourcePlateId = A.PlateId;
			Event.DestinationPlateId = B.PlateId;
			Event.SourceLocalTriangleId = ALocalTriangleId;
			Event.DestinationLocalTriangleId = BLocalTriangleId;
			Event.SourceMaterialClass = A.MaterialClass;
			Event.DestinationMaterialClass = B.MaterialClass;
			Event.SourceOceanicAgeMa = A.OceanicAgeMa;
			Event.DestinationOceanicAgeMa = B.OceanicAgeMa;
			Event.bHasProvenance = true;
			Event.Provenance = TEXT("accepted_v2_2_contact_plus_continental_plate_profiles");
			Result.ProcessEvents.Add(Event);
			const int32 DetachMarkCount = AddStage3CandidatePlateMarks(
				Result,
				Event,
				Candidate,
				A.PlateId,
				ALocalTriangleId,
				TEXT("colliding_terrane_candidate"),
				Event.Provenance);
			const int32 SutureMarkCount = AddStage3CandidatePlateMarks(
				Result,
				Event,
				Candidate,
				B.PlateId,
				BLocalTriangleId,
				TEXT("suture_candidate"),
				Event.Provenance);

			++Result.Metrics.ProcessEventCount;
			++Result.Metrics.ProcessEventWithProvenanceCount;
			++Result.Metrics.CollisionCandidateCount;
			++Result.Metrics.CollisionEventCount;
			Result.Metrics.TerraneDetachTriangleCount += DetachMarkCount;
			Result.Metrics.TerraneSutureTriangleCount += SutureMarkCount;
		}

		void BuildStage3ProcessState(
			const FCarrierV2Stage3Config& Config,
			FCarrierV2Stage3FixtureResult& Result)
		{
			Result.PlateMaterialProfiles.Reset();
			for (int32 PlateId = 0; PlateId < Result.Metrics.PlateCount; ++PlateId)
			{
				Result.PlateMaterialProfiles.Add(Stage3ProfileForPlate(Config, PlateId));
			}

			const double ProcessStart = FPlatformTime::Seconds();
			for (const FCarrierV2ProcessCandidateRecord& Candidate : Result.Stage2Result.ProcessCandidates)
			{
				if (!Candidate.bAccepted || Candidate.PlateIds.Num() < 2 || Candidate.LocalTriangleIds.Num() < 2)
				{
					continue;
				}
				if (Candidate.PlateIds.Num() != 2)
				{
					++Result.Metrics.ThirdPlatePassthroughCount;
					continue;
				}

				const FCarrierV2Stage3PlateMaterialProfile A = Stage3ProfileForPlate(Config, Candidate.PlateIds[0]);
				const FCarrierV2Stage3PlateMaterialProfile B = Stage3ProfileForPlate(Config, Candidate.PlateIds[1]);
				if (IsOceanic(A) || IsOceanic(B))
				{
					AddStage3SubductionEvent(Candidate, A, B, Candidate.LocalTriangleIds[0], Candidate.LocalTriangleIds[1], Result);
				}
				else if (IsContinental(A) && IsContinental(B))
				{
					AddStage3CollisionEvent(Candidate, A, B, Candidate.LocalTriangleIds[0], Candidate.LocalTriangleIds[1], Result);
				}
			}
			Result.Metrics.ProcessMutationMs = (FPlatformTime::Seconds() - ProcessStart) * 1000.0;
		}

		void FinalizeStage3Verdict(const FCarrierV2Stage3Config& Config, FCarrierV2Stage3Metrics& Metrics)
		{
			Metrics.bProcessEventProvenancePass =
				Metrics.ProcessEventCount == Metrics.ProcessEventWithProvenanceCount &&
				Metrics.ProcessEventWithoutProvenanceCount == 0;
			Metrics.bSubductionPolarityPass = !Config.bRequireSubductionMark ||
				(Metrics.SubductionEventCount >= FMath::Max(1, Config.ExpectedMinimumSubductionEvents) &&
					Metrics.SubductingTriangleMarkCount >= FMath::Max(1, Config.ExpectedMinimumSubductingTriangleMarks));
			if (Config.bRequireOceanicAgePolarity)
			{
				Metrics.bSubductionPolarityPass =
					Metrics.bSubductionPolarityPass &&
					Metrics.OceanicAgePolarityDecisionCount >= FMath::Max(1, Config.ExpectedMinimumSubductionEvents) &&
					Metrics.OlderOceanicSubductingPassCount == Metrics.OceanicAgePolarityDecisionCount;
			}
			Metrics.bCollisionCandidatePass = !Config.bRequireCollisionCandidate ||
				(Metrics.CollisionCandidateCount >= FMath::Max(1, Config.ExpectedMinimumCollisionCandidates) &&
					Metrics.CollisionEventCount >= FMath::Max(1, Config.ExpectedMinimumCollisionEvents));
			Metrics.bProcessEventExpectationPass = !Config.bRequireProcessEvents ||
				Metrics.ProcessEventCount >= FMath::Max(1, Config.ExpectedMinimumProcessEvents);
			Metrics.bNoForbiddenMutationPass =
				Metrics.MaterialDestroyedWithoutProcessCount == 0 &&
				Metrics.MaterialCreatedWithoutProcessCount == 0 &&
				Metrics.TopologyMutationWithoutEventCount == 0 &&
				Metrics.MaterialMutationCount == 0 &&
				Metrics.RemeshDuringProcessMutationCount == 0 &&
				Metrics.TopologyRebuildDuringProcessMutationCount == 0 &&
				Metrics.GapFillDuringProcessMutationCount == 0 &&
				Metrics.DivergentGenerationDuringProcessMutationCount == 0 &&
				Metrics.TerrainBeautyMutationCount == 0 &&
				Metrics.OwnershipRepairDuringProcessMutationCount == 0 &&
				Metrics.CentroidPrimaryResolutionCount == 0 &&
				Metrics.RandomPrimaryResolutionCount == 0 &&
				Metrics.NearestPrimaryResolutionCount == 0 &&
				Metrics.ProjectionOwnerLabelEvidenceCount == 0 &&
				Metrics.OverlapConsumedBeforeProcessStateCount == 0;
			Metrics.bSlabPullPass = !Metrics.bSlabPullEnabled && FMath::IsNearlyZero(Metrics.SlabPullAxisDeltaDeg, 1.0e-12);

			Metrics.bFixturePass =
				Metrics.bInheritedStage2Pass &&
				Metrics.bMotionOraclePass &&
				Metrics.bUnitLengthPass &&
				Metrics.bMaterialAttachmentPass &&
				Metrics.bProjectionExpectationPass &&
				Metrics.bContactEvidencePass &&
				Metrics.bProcessEventProvenancePass &&
				Metrics.bSubductionPolarityPass &&
				Metrics.bCollisionCandidatePass &&
				Metrics.bProcessEventExpectationPass &&
				Metrics.bNoForbiddenMutationPass &&
				Metrics.bSlabPullPass;
			Metrics.bStageGatePass = Metrics.bFixturePass;

			if (!Metrics.bInheritedStage2Pass)
			{
				Metrics.Verdict = TEXT("FAIL_INHERITED_V2_2_GATE");
			}
			else if (!Metrics.bSubductionPolarityPass)
			{
				Metrics.Verdict = TEXT("FAIL_SUBDUCTION_POLARITY");
			}
			else if (!Metrics.bCollisionCandidatePass)
			{
				Metrics.Verdict = TEXT("FAIL_COLLISION_CANDIDATE");
			}
			else if (!Metrics.bProcessEventProvenancePass)
			{
				Metrics.Verdict = TEXT("FAIL_PROCESS_EVENT_PROVENANCE");
			}
			else if (!Metrics.bProcessEventExpectationPass)
			{
				Metrics.Verdict = TEXT("FAIL_PROCESS_EVENT_EXPECTATION");
			}
			else if (!Metrics.bNoForbiddenMutationPass)
			{
				Metrics.Verdict = TEXT("FAIL_FORBIDDEN_PROCESS_MUTATION");
			}
			else if (!Metrics.bSlabPullPass)
			{
				Metrics.Verdict = TEXT("FAIL_SLAB_PULL_BYPASS");
			}
			else
			{
				Metrics.Verdict = TEXT("PASS_PROCESS_MUTATION_FIXTURE");
			}
		}

		bool RunStage3FixtureOnce(const FCarrierV2Stage3Config& Config, FCarrierV2Stage3FixtureResult& OutResult)
		{
			OutResult = FCarrierV2Stage3FixtureResult();
			OutResult.Config = Config;
			OutResult.Metrics.RunId = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutResult.Metrics.FixtureId = Config.FixtureId;
			OutResult.Metrics.FixtureName = Config.FixtureName;
			OutResult.Metrics.FixtureKind = FixtureKindName(Config.ContactConfig.MotionConfig.BaseConfig.FixtureKind);
			OutResult.Metrics.ExpectedProcessClass = Config.ExpectedProcessClass;
			OutResult.Metrics.ProcessMutationPolicyId = Config.ProcessMutationPolicyId;
			OutResult.Metrics.ConfigHash = HashToString(HashStage3Config(Config));
			OutResult.Metrics.bSlabPullEnabled = Config.bEnableSlabPull;

			const double TotalStart = FPlatformTime::Seconds();
			FCarrierV2Stage2::RunFixtureWithReplay(Config.ContactConfig, OutResult.Stage2Result);
			if (!OutResult.Stage2Result.bCompleted)
			{
				OutResult.Error = OutResult.Stage2Result.Error;
				OutResult.Metrics.Verdict = TEXT("FAIL_INHERITED_V2_2_RUN");
				OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
				return false;
			}

			CopyStage2MetricsToStage3(OutResult.Stage2Result.Metrics, OutResult.Metrics);
			BuildStage3ProcessState(Config, OutResult);
			OutResult.Metrics.ProcessStateHash = HashToString(HashStage3ProcessState(
				Config,
				OutResult.Metrics,
				OutResult.ProcessEvents,
				OutResult.TriangleMarks));
			FinalizeStage3Verdict(Config, OutResult.Metrics);
			OutResult.Metrics.MetricsMs = FMath::Max(0.0, OutResult.Stage2Result.Metrics.MetricsMs);
			OutResult.Metrics.MetricsHash = HashToString(HashStage3Metrics(OutResult.Metrics, true));
			OutResult.Metrics.PeakMemoryMb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);
			OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
			OutResult.bCompleted = true;
			return true;
		}

		struct FCarrierV2Stage4MarkFlags
		{
			bool bSubducting = false;
			bool bColliding = false;
		};

		struct FCarrierV2Stage4TreeTriangleRef
		{
			int32 PlateId = INDEX_NONE;
			int32 LocalTriangleId = INDEX_NONE;
			int32 SourceTriangleId = INDEX_NONE;
		};

		uint64 Stage4TriangleKey(const int32 PlateId, const int32 LocalTriangleId)
		{
			return (static_cast<uint64>(static_cast<uint32>(PlateId)) << 32) |
				static_cast<uint64>(static_cast<uint32>(LocalTriangleId));
		}

		bool IsStage4CollisionMark(const FString& MarkClass)
		{
			return MarkClass.Contains(TEXT("collid"), ESearchCase::IgnoreCase) ||
				MarkClass.Contains(TEXT("collision"), ESearchCase::IgnoreCase) ||
				MarkClass.Contains(TEXT("suture"), ESearchCase::IgnoreCase);
		}

		bool IsStage4BoundaryBarycentric(const FVector3d& Barycentric, const double Epsilon)
		{
			return FMath::Abs(Barycentric.X) <= Epsilon ||
				FMath::Abs(Barycentric.Y) <= Epsilon ||
				FMath::Abs(Barycentric.Z) <= Epsilon ||
				FMath::Abs(1.0 - Barycentric.X) <= Epsilon ||
				FMath::Abs(1.0 - Barycentric.Y) <= Epsilon ||
				FMath::Abs(1.0 - Barycentric.Z) <= Epsilon;
		}

		void BuildStage4MarkLookup(
			const TArray<FCarrierV2TriangleProcessMarkRecord>& TriangleMarks,
			TMap<uint64, FCarrierV2Stage4MarkFlags>& OutLookup)
		{
			OutLookup.Reset();
			for (const FCarrierV2TriangleProcessMarkRecord& Mark : TriangleMarks)
			{
				if (Mark.PlateId == INDEX_NONE || Mark.LocalTriangleId == INDEX_NONE)
				{
					continue;
				}
				FCarrierV2Stage4MarkFlags& Flags = OutLookup.FindOrAdd(Stage4TriangleKey(Mark.PlateId, Mark.LocalTriangleId));
				if (Mark.MarkClass.Equals(TEXT("subducting"), ESearchCase::IgnoreCase))
				{
					Flags.bSubducting = true;
				}
				if (IsStage4CollisionMark(Mark.MarkClass))
				{
					Flags.bColliding = true;
				}
			}
		}

		void CountStage4PretreatedPlates(
			const FCarrierV2BuildState& State,
			const TMap<uint64, FCarrierV2Stage4MarkFlags>& MarkLookup,
			FCarrierV2Stage4Metrics& Metrics)
		{
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				bool bHasNonSubductingTriangle = false;
				for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
				{
					const FCarrierV2Stage4MarkFlags* Flags = MarkLookup.Find(Stage4TriangleKey(Plate.PlateId, Triangle.LocalTriangleId));
					if (Flags == nullptr || !Flags->bSubducting)
					{
						bHasNonSubductingTriangle = true;
						break;
					}
				}
				if (bHasNonSubductingTriangle)
				{
					++Metrics.PretreatedPlateCount;
				}
				else
				{
					++Metrics.FullySubductedPlateDestroyedCount;
				}
			}
		}

		bool BuildStage4AabbMesh(
			const FCarrierV2BuildState& State,
			UE::Geometry::FDynamicMesh3& Mesh,
			TMap<int32, FCarrierV2Stage4TreeTriangleRef>& OutTriangleRefs,
			FCarrierV2Stage4Metrics& Metrics,
			FString& OutError)
		{
			Mesh = UE::Geometry::FDynamicMesh3();
			OutTriangleRefs.Reset();
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
				{
					if (!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[0]) ||
						!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[1]) ||
						!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[2]))
					{
						OutError = FString::Printf(
							TEXT("Stage4 AABB mesh build found invalid local triangle %d on plate %d."),
							Triangle.LocalTriangleId,
							Plate.PlateId);
						return false;
					}

					const int32 A = Mesh.AppendVertex(Plate.LocalVertices[Triangle.LocalVertexIds[0]].UnitPosition);
					const int32 B = Mesh.AppendVertex(Plate.LocalVertices[Triangle.LocalVertexIds[1]].UnitPosition);
					const int32 C = Mesh.AppendVertex(Plate.LocalVertices[Triangle.LocalVertexIds[2]].UnitPosition);
					const int32 TreeTriangleId = Mesh.AppendTriangle(A, B, C, Plate.PlateId);
					if (TreeTriangleId < 0)
					{
						OutError = FString::Printf(
							TEXT("Stage4 AABB mesh append failed for plate %d local triangle %d with result %d."),
							Plate.PlateId,
							Triangle.LocalTriangleId,
							TreeTriangleId);
						return false;
					}

					FCarrierV2Stage4TreeTriangleRef Ref;
					Ref.PlateId = Plate.PlateId;
					Ref.LocalTriangleId = Triangle.LocalTriangleId;
					Ref.SourceTriangleId = Triangle.SourceTriangleId;
					OutTriangleRefs.Add(TreeTriangleId, Ref);
					++Metrics.AabbMeshTriangleCount;
				}
			}
			return Metrics.AabbMeshTriangleCount > 0;
		}

		uint64 HashStage4Config(const FCarrierV2Stage4Config& Config)
		{
			uint64 Hash = HashStage3Config(Config.ProcessConfig);
			HashMixString(Hash, Config.FixtureId);
			HashMixString(Hash, Config.FixtureName);
			HashMixString(Hash, Config.ExpectedRemeshClass);
			HashMixString(Hash, Config.RemeshSamplingPolicyId);
			HashMixString(Hash, Config.RemeshTriggerReason);
			HashMixInt(Hash, Config.bRequireFilteredSubductingHit ? 1 : 0);
			HashMixInt(Hash, Config.bRequireFilteredCollidingHit ? 1 : 0);
			HashMixInt(Hash, Config.bRequireValidHitAfterFilter ? 1 : 0);
			HashMixInt(Hash, Config.bAllowDeferredGapFill ? 1 : 0);
			HashMixInt(Hash, Config.ExpectedMinimumFilteredSubductingHits);
			HashMixInt(Hash, Config.ExpectedMinimumFilteredCollidingHits);
			HashMixInt(Hash, Config.ExpectedMinimumValidHits);
			return Hash;
		}

		uint64 HashStage4SamplingRecords(
			const FCarrierV2Stage4Config& Config,
			const FCarrierV2Stage4Metrics& Metrics,
			const TArray<FCarrierV2RemeshSampleRecord>& SampleRecords)
		{
			uint64 Hash = HashStage4Config(Config);
			HashMixString(Hash, Metrics.Stage3ProcessStateHash);
			HashMixString(Hash, Metrics.RemeshInputHash);
			for (const FCarrierV2RemeshSampleRecord& Record : SampleRecords)
			{
				HashMixInt(Hash, Record.SampleId);
				HashMixInt(Hash, Record.RawHitCount);
				HashMixInt(Hash, Record.FilteredSubductingHitCount);
				HashMixInt(Hash, Record.FilteredCollidingHitCount);
				HashMixInt(Hash, Record.ValidHitCount);
				HashMixInt(Hash, Record.SelectedPlateId);
				HashMixInt(Hash, Record.SelectedLocalTriangleId);
				HashMixInt(Hash, Record.SelectedSourceTriangleId);
				HashMixDouble(Hash, Record.SelectedContinentalFraction);
				HashMixInt(Hash, Record.bZeroValidHit ? 1 : 0);
				HashMixInt(Hash, Record.bPostFilterUnresolvedMultihit ? 1 : 0);
				HashMixInt(Hash, Record.bSelectedFilteredHit ? 1 : 0);
				HashMixString(Hash, Record.SelectionProvenance);
			}
			return Hash;
		}

		uint64 HashStage4Metrics(const FCarrierV2Stage4Metrics& Metrics, const bool bIncludeReplayFields)
		{
			uint64 Hash = FnvOffset;
			HashMixString(Hash, Metrics.ConfigHash);
			HashMixString(Hash, Metrics.Stage3ProcessStateHash);
			HashMixString(Hash, Metrics.RemeshInputHash);
			HashMixString(Hash, Metrics.GlobalSamplingHash);
			HashMixInt(Hash, Metrics.GlobalSampleCount);
			HashMixInt(Hash, Metrics.GlobalTriangleCount);
			HashMixInt(Hash, Metrics.LocalPlateVertexCountSum);
			HashMixInt(Hash, Metrics.LocalPlateTriangleCountSum);
			HashMixInt(Hash, Metrics.PretreatedPlateCount);
			HashMixInt(Hash, Metrics.FullySubductedPlateDestroyedCount);
			HashMixInt(Hash, Metrics.AabbMeshTriangleCount);
			HashMixInt(Hash, Metrics.AabbRayQueryCount);
			HashMixInt(Hash, Metrics.RawHitCountTotal);
			HashMixInt(Hash, Metrics.FilteredSubductingHitCount);
			HashMixInt(Hash, Metrics.FilteredCollidingHitCount);
			HashMixInt(Hash, Metrics.ValidHitAfterFilterCount);
			HashMixInt(Hash, Metrics.ZeroValidHitCount);
			HashMixInt(Hash, Metrics.GapFillDeferredCount);
			HashMixInt(Hash, Metrics.PostFilterUnresolvedMultihitCount);
			HashMixInt(Hash, Metrics.PostFilterBoundaryOnlyMultihitCount);
			HashMixInt(Hash, Metrics.BoundaryDegenerateHitCount);
			HashMixInt(Hash, Metrics.MaterialInterpolationCount);
			HashMixInt(Hash, Metrics.SelectedFilteredHitCount);
			HashMixInt(Hash, Metrics.PriorOwnerFallbackCount);
			HashMixInt(Hash, Metrics.CentroidPrimaryResolutionCount);
			HashMixInt(Hash, Metrics.RandomPrimaryResolutionCount);
			HashMixInt(Hash, Metrics.NearestPrimaryResolutionCount);
			HashMixInt(Hash, Metrics.OwnershipRepairDuringRemeshCount);
			HashMixInt(Hash, Metrics.RetentionHysteresisAnchorCount);
			HashMixInt(Hash, Metrics.GeneratedOceanicCount);
			HashMixInt(Hash, Metrics.Q1Q2DeferredCount);
			HashMixInt(Hash, Metrics.Q1Q2DiscreteApproxCount);
			HashMixInt(Hash, Metrics.Q1Q2PriorOwnerLookupCount);
			HashMixInt(Hash, Metrics.TopologyRebuildDuringSamplingCount);
			HashMixInt(Hash, Metrics.ProcessStateResetCount);
			HashMixInt(Hash, Metrics.TerrainBeautyMutationCount);
			HashMixInt(Hash, Metrics.MaterialCreatedWithoutGapFillCount);
			HashMixInt(Hash, Metrics.MaterialDestroyedWithoutProcessCount);
			HashMixInt(Hash, Metrics.bInheritedStage3Pass ? 1 : 0);
			HashMixInt(Hash, Metrics.bSourceFilterPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bValidSelectionPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bNoForbiddenFallbackPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bDeferredGapFillPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bNoPrematureTopologyPass ? 1 : 0);
			if (bIncludeReplayFields)
			{
				HashMixString(Hash, Metrics.ReplayStage3ProcessStateHash);
				HashMixString(Hash, Metrics.ReplayGlobalSamplingHash);
				HashMixString(Hash, Metrics.ReplayMetricsHash);
				HashMixInt(Hash, Metrics.bReplayDeterministic ? 1 : 0);
				HashMixString(Hash, Metrics.Verdict);
			}
			return Hash;
		}

		void SampleStage4GlobalTds(
			const FCarrierV2Stage4Config& Config,
			const FCarrierV2BuildState& State,
			const TMap<uint64, FCarrierV2Stage4MarkFlags>& MarkLookup,
			UE::Geometry::FDynamicMeshAABBTree3& Tree,
			const TMap<int32, FCarrierV2Stage4TreeTriangleRef>& TreeTriangleRefs,
			FCarrierV2Stage4FixtureResult& Result)
		{
			const double SamplingStart = FPlatformTime::Seconds();
			Result.SampleRecords.Reset(State.Samples.Num());
			for (const FCarrierV2SubstrateSample& Sample : State.Samples)
			{
				FCarrierV2RemeshSampleRecord Record;
				Record.SampleId = Sample.SampleId;

				TArray<MeshIntersection::FHitIntersectionResult> RawHits;
				const FRay3d Ray(FVector3d::ZeroVector, Sample.UnitPosition);
				UE::Geometry::IMeshSpatial::FQueryOptions QueryOptions;
				QueryOptions.MaxDistance = 2.0;
				++Result.Metrics.AabbRayQueryCount;
				Tree.FindAllHitTriangles(Ray, RawHits, QueryOptions);

				Record.RawHitCount = RawHits.Num();
				Result.Metrics.RawHitCountTotal += RawHits.Num();

				TArray<const MeshIntersection::FHitIntersectionResult*, TInlineAllocator<16>> ValidHits;
				TArray<FCarrierV2Stage4TreeTriangleRef, TInlineAllocator<16>> ValidRefs;
				bool bAllValidBoundary = true;
				for (const MeshIntersection::FHitIntersectionResult& Hit : RawHits)
				{
					const FCarrierV2Stage4TreeTriangleRef* Ref = TreeTriangleRefs.Find(Hit.TriangleId);
					if (Ref == nullptr)
					{
						continue;
					}

					const FCarrierV2Stage4MarkFlags* Flags = MarkLookup.Find(Stage4TriangleKey(Ref->PlateId, Ref->LocalTriangleId));
					if (Flags != nullptr && Flags->bSubducting)
					{
						++Record.FilteredSubductingHitCount;
						++Result.Metrics.FilteredSubductingHitCount;
						continue;
					}
					if (Flags != nullptr && Flags->bColliding)
					{
						++Record.FilteredCollidingHitCount;
						++Result.Metrics.FilteredCollidingHitCount;
						continue;
					}

					const bool bBoundary = IsStage4BoundaryBarycentric(Hit.BaryCoords, Config.ProcessConfig.ContactConfig.MotionConfig.BaseConfig.RayEpsilon);
					bAllValidBoundary = bAllValidBoundary && bBoundary;
					if (bBoundary)
					{
						++Result.Metrics.BoundaryDegenerateHitCount;
					}
					ValidHits.Add(&Hit);
					ValidRefs.Add(*Ref);
				}

				Record.ValidHitCount = ValidHits.Num();
				Result.Metrics.ValidHitAfterFilterCount += ValidHits.Num();
				if (ValidHits.IsEmpty())
				{
					Record.bZeroValidHit = true;
					Record.SelectionProvenance = TEXT("deferred_q1q2_gap_fill");
					++Result.Metrics.ZeroValidHitCount;
					++Result.Metrics.GapFillDeferredCount;
					++Result.Metrics.Q1Q2DeferredCount;
					Result.SampleRecords.Add(Record);
					continue;
				}

				if (ValidHits.Num() > 1)
				{
					if (bAllValidBoundary)
					{
						++Result.Metrics.PostFilterBoundaryOnlyMultihitCount;
					}
					else
					{
						++Result.Metrics.PostFilterUnresolvedMultihitCount;
						Record.bPostFilterUnresolvedMultihit = true;
						Record.SelectionProvenance = TEXT("unresolved_post_filter_multihit");
						Result.SampleRecords.Add(Record);
						continue;
					}
				}

				const MeshIntersection::FHitIntersectionResult* SelectedHit = ValidHits[0];
				const FCarrierV2Stage4TreeTriangleRef& SelectedRef = ValidRefs[0];
				Record.SelectedPlateId = SelectedRef.PlateId;
				Record.SelectedLocalTriangleId = SelectedRef.LocalTriangleId;
				Record.SelectedSourceTriangleId = SelectedRef.SourceTriangleId;
				Record.SelectionProvenance = ValidHits.Num() == 1
					? TEXT("single_valid_post_filter_intersection")
					: TEXT("boundary_degenerate_valid_intersection");

				const FCarrierV2Stage4MarkFlags* SelectedFlags = MarkLookup.Find(Stage4TriangleKey(SelectedRef.PlateId, SelectedRef.LocalTriangleId));
				Record.bSelectedFilteredHit =
					SelectedFlags != nullptr &&
					(SelectedFlags->bSubducting || SelectedFlags->bColliding);
				if (Record.bSelectedFilteredHit)
				{
					++Result.Metrics.SelectedFilteredHitCount;
				}

				if (State.Plates.IsValidIndex(SelectedRef.PlateId))
				{
					const FCarrierV2Plate& Plate = State.Plates[SelectedRef.PlateId];
					if (Plate.LocalTriangles.IsValidIndex(SelectedRef.LocalTriangleId))
					{
						Record.SelectedContinentalFraction = InterpolateContinentalFraction(
							Plate,
							Plate.LocalTriangles[SelectedRef.LocalTriangleId],
							SelectedHit->BaryCoords);
						++Result.Metrics.MaterialInterpolationCount;
					}
				}
				Result.SampleRecords.Add(Record);
			}
			Result.Metrics.GlobalSamplingMs = (FPlatformTime::Seconds() - SamplingStart) * 1000.0;
		}

		void FinalizeStage4Verdict(const FCarrierV2Stage4Config& Config, FCarrierV2Stage4Metrics& Metrics)
		{
			Metrics.bSourceFilterPass =
				(!Config.bRequireFilteredSubductingHit ||
					Metrics.FilteredSubductingHitCount >= FMath::Max(1, Config.ExpectedMinimumFilteredSubductingHits)) &&
				(!Config.bRequireFilteredCollidingHit ||
					Metrics.FilteredCollidingHitCount >= FMath::Max(1, Config.ExpectedMinimumFilteredCollidingHits)) &&
				Metrics.SelectedFilteredHitCount == 0;

			Metrics.bValidSelectionPass =
				(!Config.bRequireValidHitAfterFilter ||
					Metrics.ValidHitAfterFilterCount >= FMath::Max(1, Config.ExpectedMinimumValidHits)) &&
				Metrics.PostFilterUnresolvedMultihitCount == 0;

			Metrics.bNoForbiddenFallbackPass =
				Metrics.PriorOwnerFallbackCount == 0 &&
				Metrics.CentroidPrimaryResolutionCount == 0 &&
				Metrics.RandomPrimaryResolutionCount == 0 &&
				Metrics.NearestPrimaryResolutionCount == 0 &&
				Metrics.OwnershipRepairDuringRemeshCount == 0 &&
				Metrics.RetentionHysteresisAnchorCount == 0 &&
				Metrics.Q1Q2DiscreteApproxCount == 0 &&
				Metrics.Q1Q2PriorOwnerLookupCount == 0 &&
				Metrics.MaterialCreatedWithoutGapFillCount == 0 &&
				Metrics.MaterialDestroyedWithoutProcessCount == 0 &&
				Metrics.TerrainBeautyMutationCount == 0;

			Metrics.bDeferredGapFillPass = Config.bAllowDeferredGapFill
				? Metrics.GapFillDeferredCount == Metrics.ZeroValidHitCount && Metrics.GeneratedOceanicCount == 0
				: Metrics.ZeroValidHitCount == 0;

			Metrics.bNoPrematureTopologyPass =
				Metrics.TopologyRebuildDuringSamplingCount == 0 &&
				Metrics.ProcessStateResetCount == 0;

			Metrics.bFixturePass =
				Metrics.bInheritedStage3Pass &&
				Metrics.bSourceFilterPass &&
				Metrics.bValidSelectionPass &&
				Metrics.bNoForbiddenFallbackPass &&
				Metrics.bDeferredGapFillPass &&
				Metrics.bNoPrematureTopologyPass;
			Metrics.bStageGatePass = Metrics.bFixturePass;

			if (!Metrics.bInheritedStage3Pass)
			{
				Metrics.Verdict = TEXT("FAIL_INHERITED_V2_3_GATE");
			}
			else if (!Metrics.bSourceFilterPass)
			{
				Metrics.Verdict = TEXT("FAIL_FILTERED_MARK_GATE");
			}
			else if (!Metrics.bValidSelectionPass)
			{
				Metrics.Verdict = TEXT("FAIL_POST_FILTER_SELECTION_GATE");
			}
			else if (!Metrics.bNoForbiddenFallbackPass)
			{
				Metrics.Verdict = TEXT("FAIL_FORBIDDEN_REPAIR_OR_RESOLVER");
			}
			else if (!Metrics.bDeferredGapFillPass)
			{
				Metrics.Verdict = TEXT("FAIL_DEFERRED_GAP_FILL_LEDGER");
			}
			else if (!Metrics.bNoPrematureTopologyPass)
			{
				Metrics.Verdict = TEXT("FAIL_PREMATURE_TOPOLOGY_OR_RESET");
			}
			else if (Metrics.ZeroValidHitCount > 0)
			{
				Metrics.Verdict = TEXT("PASS_FILTERED_GLOBAL_SAMPLING_Q1Q2_DEFERRED");
			}
			else
			{
				Metrics.Verdict = TEXT("PASS_FILTERED_GLOBAL_SAMPLING");
			}
		}

		bool RunStage4FixtureOnce(const FCarrierV2Stage4Config& Config, FCarrierV2Stage4FixtureResult& OutResult)
		{
			OutResult = FCarrierV2Stage4FixtureResult();
			OutResult.Config = Config;
			OutResult.Metrics.RunId = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutResult.Metrics.FixtureId = Config.FixtureId;
			OutResult.Metrics.FixtureName = Config.FixtureName;
			OutResult.Metrics.FixtureKind = FixtureKindName(Config.ProcessConfig.ContactConfig.MotionConfig.BaseConfig.FixtureKind);
			OutResult.Metrics.ExpectedRemeshClass = Config.ExpectedRemeshClass;
			OutResult.Metrics.SourceStatus = Config.ProcessConfig.ContactConfig.MotionConfig.SourceStatus;
			OutResult.Metrics.RemeshSamplingPolicyId = Config.RemeshSamplingPolicyId;
			OutResult.Metrics.RemeshTriggerReason = Config.RemeshTriggerReason;
			OutResult.Metrics.SampleCount = Config.ProcessConfig.ContactConfig.MotionConfig.BaseConfig.SampleCount;
			OutResult.Metrics.PlateCount = Config.ProcessConfig.ContactConfig.MotionConfig.BaseConfig.PlateCount;
			OutResult.Metrics.RayEpsilon = Config.ProcessConfig.ContactConfig.MotionConfig.BaseConfig.RayEpsilon;
			OutResult.Metrics.ConfigHash = HashToString(HashStage4Config(Config));

			const double TotalStart = FPlatformTime::Seconds();
			const double Stage3Start = FPlatformTime::Seconds();
			FCarrierV2Stage3::RunFixtureWithReplay(Config.ProcessConfig, OutResult.Stage3Result);
			OutResult.Metrics.Stage3Ms = (FPlatformTime::Seconds() - Stage3Start) * 1000.0;
			OutResult.Metrics.Stage3ProcessStateHash = OutResult.Stage3Result.Metrics.ProcessStateHash;
			OutResult.Metrics.bInheritedStage3Pass =
				OutResult.Stage3Result.bCompleted &&
				OutResult.Stage3Result.Metrics.bFixturePass &&
				OutResult.Stage3Result.Metrics.bReplayDeterministic;
			if (!OutResult.Stage3Result.bCompleted)
			{
				OutResult.Error = OutResult.Stage3Result.Error;
				OutResult.Metrics.Verdict = TEXT("FAIL_INHERITED_V2_3_RUN");
				OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
				return false;
			}

			FCarrierV2BuildState State;
			FCarrierV2Stage0Metrics BuildMetrics;
			if (!BuildStateForConfig(Config.ProcessConfig.ContactConfig.MotionConfig.BaseConfig, State, BuildMetrics, OutResult.Error))
			{
				OutResult.Metrics.Verdict = TEXT("FAIL_BUILD");
				OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
				return false;
			}
			OutResult.Metrics.BuildSubstrateMs = BuildMetrics.BuildSubstrateMs;
			OutResult.Metrics.BuildPlateLocalMs = BuildMetrics.BuildPlateLocalMs;
			OutResult.Metrics.GlobalSampleCount = State.Samples.Num();
			OutResult.Metrics.GlobalTriangleCount = State.Triangles.Num();
			OutResult.Metrics.TriangleCount = State.Triangles.Num();
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				OutResult.Metrics.LocalPlateVertexCountSum += Plate.LocalVertices.Num();
				OutResult.Metrics.LocalPlateTriangleCountSum += Plate.LocalTriangles.Num();
			}

			FCarrierV2Stage1Metrics MotionMetrics;
			ApplyStage1MotionAndMeasure(Config.ProcessConfig.ContactConfig.MotionConfig, State, MotionMetrics);
			OutResult.Metrics.MotionApplyMs = MotionMetrics.MotionApplyMs;
			OutResult.Metrics.RemeshInputHash = HashToString(HashStage1Authority(State, Config.ProcessConfig.ContactConfig.MotionConfig));

			TMap<uint64, FCarrierV2Stage4MarkFlags> MarkLookup;
			BuildStage4MarkLookup(OutResult.Stage3Result.TriangleMarks, MarkLookup);
			CountStage4PretreatedPlates(State, MarkLookup, OutResult.Metrics);

			UE::Geometry::FDynamicMesh3 Mesh;
			TMap<int32, FCarrierV2Stage4TreeTriangleRef> TreeTriangleRefs;
			const double AabbStart = FPlatformTime::Seconds();
			if (!BuildStage4AabbMesh(State, Mesh, TreeTriangleRefs, OutResult.Metrics, OutResult.Error))
			{
				OutResult.Metrics.Verdict = TEXT("FAIL_AABB_BUILD");
				OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
				return false;
			}
			UE::Geometry::FDynamicMeshAABBTree3 Tree(&Mesh, true);
			OutResult.Metrics.AabbBuildMs = (FPlatformTime::Seconds() - AabbStart) * 1000.0;

			const double MetricsStart = FPlatformTime::Seconds();
			SampleStage4GlobalTds(Config, State, MarkLookup, Tree, TreeTriangleRefs, OutResult);
			OutResult.Metrics.GlobalSamplingHash = HashToString(HashStage4SamplingRecords(Config, OutResult.Metrics, OutResult.SampleRecords));
			FinalizeStage4Verdict(Config, OutResult.Metrics);
			OutResult.Metrics.MetricsMs = (FPlatformTime::Seconds() - MetricsStart) * 1000.0 - OutResult.Metrics.GlobalSamplingMs;
			if (OutResult.Metrics.MetricsMs < 0.0)
			{
				OutResult.Metrics.MetricsMs = 0.0;
			}
			OutResult.Metrics.MetricsHash = HashToString(HashStage4Metrics(OutResult.Metrics, true));
			OutResult.Metrics.PeakMemoryMb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);
			OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
			OutResult.bCompleted = true;
			return true;
		}

		struct FCarrierV2Stage5TreeTriangleRef
		{
			int32 PlateId = INDEX_NONE;
			int32 LocalTriangleId = INDEX_NONE;
			int32 SourceTriangleId = INDEX_NONE;
		};

		struct FCarrierV2Stage5BoundaryEdge
		{
			int32 PlateId = INDEX_NONE;
			int32 LocalTriangleId = INDEX_NONE;
			int32 SourceTriangleId = INDEX_NONE;
			int32 LocalVertexA = INDEX_NONE;
			int32 LocalVertexB = INDEX_NONE;
			int32 SourceSampleA = INDEX_NONE;
			int32 SourceSampleB = INDEX_NONE;
			FVector3d A = FVector3d::ZeroVector;
			FVector3d B = FVector3d::ZeroVector;
			FCarrierV2MaterialRecord MaterialA;
			FCarrierV2MaterialRecord MaterialB;
		};

		struct FCarrierV2Stage5EdgeAccumulator
		{
			int32 Count = 0;
			FCarrierV2Stage5BoundaryEdge Edge;
		};

		struct FCarrierV2Stage5BoundaryCandidate
		{
			bool bValid = false;
			bool bClippedToEndpoint = false;
			int32 PlateId = INDEX_NONE;
			int32 LocalTriangleId = INDEX_NONE;
			int32 SourceTriangleId = INDEX_NONE;
			double DistanceRad = MAX_dbl;
			double EdgeT = 0.0;
			double ContinentalFraction = 0.0;
			FVector3d Point = FVector3d::ZeroVector;
		};

		double ClampUnitDot(const double Dot)
		{
			return FMath::Clamp(Dot, -1.0, 1.0);
		}

		double GeodesicDistanceRad(const FVector3d& A, const FVector3d& B)
		{
			return FMath::Acos(ClampUnitDot(FVector3d::DotProduct(A.GetSafeNormal(), B.GetSafeNormal())));
		}

		FCarrierV2MaterialRecord MakeStage5Material(const double ContinentalFraction, const FString& Provenance)
		{
			FCarrierV2MaterialRecord Material;
			Material.ContinentalFraction = FMath::Clamp(ContinentalFraction, 0.0, 1.0);
			if (Material.ContinentalFraction <= 1.0e-9)
			{
				Material.MaterialClass = ECarrierV2MaterialClass::Oceanic;
			}
			else if (Material.ContinentalFraction >= 1.0 - 1.0e-9)
			{
				Material.MaterialClass = ECarrierV2MaterialClass::Continental;
			}
			else
			{
				Material.MaterialClass = ECarrierV2MaterialClass::Mixed;
			}
			Material.Provenance = Provenance;
			return Material;
		}

		uint64 Stage5EdgeKey(const int32 PlateId, const int32 A, const int32 B)
		{
			const int32 MinVertex = FMath::Min(A, B);
			const int32 MaxVertex = FMath::Max(A, B);
			uint64 Hash = FnvOffset;
			HashMixInt(Hash, PlateId);
			HashMixInt(Hash, MinVertex);
			HashMixInt(Hash, MaxVertex);
			return Hash;
		}

		FVector3d ClosestPointOnSphericalArc(
			const FVector3d& P,
			const FVector3d& A,
			const FVector3d& B,
			const double Epsilon,
			bool& bOutClippedToEndpoint,
			double& OutT)
		{
			bOutClippedToEndpoint = false;
			OutT = 0.0;
			const FVector3d UnitA = A.GetSafeNormal();
			const FVector3d UnitB = B.GetSafeNormal();
			const FVector3d UnitP = P.GetSafeNormal();
			const double EdgeAngle = GeodesicDistanceRad(UnitA, UnitB);
			if (EdgeAngle <= Epsilon)
			{
				bOutClippedToEndpoint = true;
				return UnitA;
			}

			const FVector3d PlaneNormal = FVector3d::CrossProduct(UnitA, UnitB).GetSafeNormal();
			if (PlaneNormal.SizeSquared() <= Epsilon * Epsilon)
			{
				bOutClippedToEndpoint = true;
				const double DistA = GeodesicDistanceRad(UnitP, UnitA);
				const double DistB = GeodesicDistanceRad(UnitP, UnitB);
				OutT = DistB < DistA ? 1.0 : 0.0;
				return DistB < DistA ? UnitB : UnitA;
			}

			FVector3d Projected = UnitP - PlaneNormal * FVector3d::DotProduct(UnitP, PlaneNormal);
			if (Projected.SizeSquared() <= Epsilon * Epsilon)
			{
				bOutClippedToEndpoint = true;
				const double DistA = GeodesicDistanceRad(UnitP, UnitA);
				const double DistB = GeodesicDistanceRad(UnitP, UnitB);
				OutT = DistB < DistA ? 1.0 : 0.0;
				return DistB < DistA ? UnitB : UnitA;
			}
			Projected.Normalize();

			const double AToProjected = GeodesicDistanceRad(UnitA, Projected);
			const double ProjectedToB = GeodesicDistanceRad(Projected, UnitB);
			if (AToProjected + ProjectedToB <= EdgeAngle + 1.0e-7)
			{
				OutT = FMath::Clamp(AToProjected / EdgeAngle, 0.0, 1.0);
				return Projected;
			}

			bOutClippedToEndpoint = true;
			const double DistA = GeodesicDistanceRad(UnitP, UnitA);
			const double DistB = GeodesicDistanceRad(UnitP, UnitB);
			OutT = DistB < DistA ? 1.0 : 0.0;
			return DistB < DistA ? UnitB : UnitA;
		}

		bool IsBetterStage5BoundaryCandidate(
			const FCarrierV2Stage5BoundaryCandidate& Candidate,
			const FCarrierV2Stage5BoundaryCandidate& Best)
		{
			if (!Candidate.bValid)
			{
				return false;
			}
			if (!Best.bValid)
			{
				return true;
			}
			if (Candidate.DistanceRad < Best.DistanceRad - 1.0e-12)
			{
				return true;
			}
			if (Candidate.DistanceRad > Best.DistanceRad + 1.0e-12)
			{
				return false;
			}
			if (Candidate.PlateId != Best.PlateId)
			{
				return Candidate.PlateId < Best.PlateId;
			}
			if (Candidate.SourceTriangleId != Best.SourceTriangleId)
			{
				return Candidate.SourceTriangleId < Best.SourceTriangleId;
			}
			return Candidate.LocalTriangleId < Best.LocalTriangleId;
		}

		FCarrierV2Stage5BoundaryCandidate MakeStage5BoundaryCandidate(
			const FCarrierV2Stage5BoundaryEdge& Edge,
			const FVector3d& SamplePosition,
			const double Epsilon)
		{
			FCarrierV2Stage5BoundaryCandidate Candidate;
			Candidate.bValid = true;
			Candidate.PlateId = Edge.PlateId;
			Candidate.LocalTriangleId = Edge.LocalTriangleId;
			Candidate.SourceTriangleId = Edge.SourceTriangleId;
			Candidate.Point = ClosestPointOnSphericalArc(
				SamplePosition,
				Edge.A,
				Edge.B,
				Epsilon,
				Candidate.bClippedToEndpoint,
				Candidate.EdgeT);
			Candidate.DistanceRad = GeodesicDistanceRad(SamplePosition, Candidate.Point);
			Candidate.ContinentalFraction = FMath::Lerp(
				Edge.MaterialA.ContinentalFraction,
				Edge.MaterialB.ContinentalFraction,
				Candidate.EdgeT);
			return Candidate;
		}

		bool FindStage5Q1Q2BoundaryPair(
			const TArray<FCarrierV2Stage5BoundaryEdge>& BoundaryEdges,
			const FVector3d& SamplePosition,
			const double Epsilon,
			FCarrierV2Stage5BoundaryCandidate& OutQ1,
			FCarrierV2Stage5BoundaryCandidate& OutQ2,
			FCarrierV2Stage5Metrics& Metrics)
		{
			OutQ1 = FCarrierV2Stage5BoundaryCandidate();
			OutQ2 = FCarrierV2Stage5BoundaryCandidate();
			TArray<FCarrierV2Stage5BoundaryCandidate> BestByPlate;
			for (const FCarrierV2Stage5BoundaryEdge& Edge : BoundaryEdges)
			{
				++Metrics.Q1Q2BoundaryQueryCount;
				const FCarrierV2Stage5BoundaryCandidate Candidate = MakeStage5BoundaryCandidate(Edge, SamplePosition, Epsilon);
				if (IsBetterStage5BoundaryCandidate(Candidate, OutQ1))
				{
					OutQ1 = Candidate;
				}
				if (Candidate.PlateId >= 0)
				{
					if (BestByPlate.Num() <= Candidate.PlateId)
					{
						BestByPlate.SetNum(Candidate.PlateId + 1);
					}
					if (IsBetterStage5BoundaryCandidate(Candidate, BestByPlate[Candidate.PlateId]))
					{
						BestByPlate[Candidate.PlateId] = Candidate;
					}
				}
			}
			if (!OutQ1.bValid)
			{
				return false;
			}
			for (const FCarrierV2Stage5BoundaryCandidate& Candidate : BestByPlate)
			{
				if (Candidate.bValid && Candidate.PlateId != OutQ1.PlateId && IsBetterStage5BoundaryCandidate(Candidate, OutQ2))
				{
					OutQ2 = Candidate;
				}
			}
			return OutQ2.bValid;
		}

		void BuildStage5BoundaryEdges(
			const FCarrierV2BuildState& State,
			const TMap<uint64, FCarrierV2Stage4MarkFlags>& MarkLookup,
			TArray<FCarrierV2Stage5BoundaryEdge>& OutEdges,
			FCarrierV2Stage5Metrics& Metrics)
		{
			OutEdges.Reset();
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				TMap<uint64, FCarrierV2Stage5EdgeAccumulator> EdgeMap;
				for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
				{
					const FCarrierV2Stage4MarkFlags* Flags = MarkLookup.Find(Stage4TriangleKey(Plate.PlateId, Triangle.LocalTriangleId));
					if (Flags != nullptr && Flags->bSubducting)
					{
						continue;
					}

					for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
					{
						const int32 LocalA = Triangle.LocalVertexIds[EdgeIndex];
						const int32 LocalB = Triangle.LocalVertexIds[(EdgeIndex + 1) % 3];
						if (!Plate.LocalVertices.IsValidIndex(LocalA) || !Plate.LocalVertices.IsValidIndex(LocalB))
						{
							continue;
						}
						FCarrierV2Stage5EdgeAccumulator& Accumulator = EdgeMap.FindOrAdd(Stage5EdgeKey(Plate.PlateId, LocalA, LocalB));
						++Accumulator.Count;
						if (Accumulator.Count == 1)
						{
							Accumulator.Edge.PlateId = Plate.PlateId;
							Accumulator.Edge.LocalTriangleId = Triangle.LocalTriangleId;
							Accumulator.Edge.SourceTriangleId = Triangle.SourceTriangleId;
							Accumulator.Edge.LocalVertexA = LocalA;
							Accumulator.Edge.LocalVertexB = LocalB;
							Accumulator.Edge.SourceSampleA = Plate.LocalVertices[LocalA].SourceSampleId;
							Accumulator.Edge.SourceSampleB = Plate.LocalVertices[LocalB].SourceSampleId;
							Accumulator.Edge.A = Plate.LocalVertices[LocalA].UnitPosition;
							Accumulator.Edge.B = Plate.LocalVertices[LocalB].UnitPosition;
							Accumulator.Edge.MaterialA = Plate.LocalVertices[LocalA].Material;
							Accumulator.Edge.MaterialB = Plate.LocalVertices[LocalB].Material;
						}
					}
				}

				for (const TPair<uint64, FCarrierV2Stage5EdgeAccumulator>& Pair : EdgeMap)
				{
					if (Pair.Value.Count == 1)
					{
						OutEdges.Add(Pair.Value.Edge);
					}
				}
			}
			Metrics.BoundaryEdgeCount = OutEdges.Num();
		}

		bool BuildStage5AabbMesh(
			const FCarrierV2BuildState& State,
			UE::Geometry::FDynamicMesh3& Mesh,
			TMap<int32, FCarrierV2Stage5TreeTriangleRef>& OutTriangleRefs,
			FCarrierV2Stage5Metrics& Metrics,
			FString& OutError)
		{
			Mesh = UE::Geometry::FDynamicMesh3();
			OutTriangleRefs.Reset();
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
				{
					if (!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[0]) ||
						!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[1]) ||
						!Plate.LocalVertices.IsValidIndex(Triangle.LocalVertexIds[2]))
					{
						OutError = FString::Printf(
							TEXT("Stage5 AABB mesh build found invalid local triangle %d on plate %d."),
							Triangle.LocalTriangleId,
							Plate.PlateId);
						return false;
					}

					const int32 A = Mesh.AppendVertex(Plate.LocalVertices[Triangle.LocalVertexIds[0]].UnitPosition);
					const int32 B = Mesh.AppendVertex(Plate.LocalVertices[Triangle.LocalVertexIds[1]].UnitPosition);
					const int32 C = Mesh.AppendVertex(Plate.LocalVertices[Triangle.LocalVertexIds[2]].UnitPosition);
					const int32 TreeTriangleId = Mesh.AppendTriangle(A, B, C, Plate.PlateId);
					if (TreeTriangleId < 0)
					{
						OutError = FString::Printf(
							TEXT("Stage5 AABB mesh append failed for plate %d local triangle %d with result %d."),
							Plate.PlateId,
							Triangle.LocalTriangleId,
							TreeTriangleId);
						return false;
					}

					FCarrierV2Stage5TreeTriangleRef Ref;
					Ref.PlateId = Plate.PlateId;
					Ref.LocalTriangleId = Triangle.LocalTriangleId;
					Ref.SourceTriangleId = Triangle.SourceTriangleId;
					OutTriangleRefs.Add(TreeTriangleId, Ref);
					++Metrics.AabbMeshTriangleCount;
				}
			}
			return Metrics.AabbMeshTriangleCount > 0;
		}

		uint64 HashStage5Config(const FCarrierV2Stage5Config& Config)
		{
			uint64 Hash = HashStage4Config(Config.SamplingConfig);
			HashMixString(Hash, Config.FixtureId);
			HashMixString(Hash, Config.FixtureName);
			HashMixString(Hash, Config.ExpectedRemeshClass);
			HashMixString(Hash, Config.RemeshSamplingPolicyId);
			HashMixString(Hash, Config.TrianglePartitionPolicyId);
			HashMixString(Hash, Config.RemeshTriggerReason);
			HashMixInt(Hash, Config.bRequireGeneratedOceanic ? 1 : 0);
			HashMixInt(Hash, Config.bRequireContinuousQ1Q2 ? 1 : 0);
			HashMixInt(Hash, Config.bRequireTopologyRebuild ? 1 : 0);
			HashMixInt(Hash, Config.bRequireProcessReset ? 1 : 0);
			HashMixInt(Hash, Config.ExpectedMinimumGeneratedOceanic);
			HashMixInt(Hash, Config.ExpectedMinimumQ1Q2Pairs);
			return Hash;
		}

		uint64 HashStage5SamplingRecords(
			const FCarrierV2Stage5Config& Config,
			const FCarrierV2Stage5Metrics& Metrics,
			const TArray<FCarrierV2Stage5SampleRecord>& SampleRecords)
		{
			uint64 Hash = HashStage5Config(Config);
			HashMixString(Hash, Metrics.Stage3ProcessStateHash);
			HashMixString(Hash, Metrics.RemeshInputHash);
			for (const FCarrierV2Stage5SampleRecord& Record : SampleRecords)
			{
				HashMixInt(Hash, Record.SampleId);
				HashMixInt(Hash, Record.RawHitCount);
				HashMixInt(Hash, Record.FilteredSubductingHitCount);
				HashMixInt(Hash, Record.FilteredCollidingHitCount);
				HashMixInt(Hash, Record.ValidHitCount);
				HashMixInt(Hash, Record.SelectedPlateId);
				HashMixInt(Hash, Record.SelectedLocalTriangleId);
				HashMixInt(Hash, Record.SelectedSourceTriangleId);
				HashMixDouble(Hash, Record.SelectedContinentalFraction);
				HashMixInt(Hash, Record.bZeroValidHit ? 1 : 0);
				HashMixInt(Hash, Record.bGeneratedOceanic ? 1 : 0);
				HashMixInt(Hash, Record.bPostFilterUnresolvedMultihit ? 1 : 0);
				HashMixInt(Hash, Record.bSelectedFilteredHit ? 1 : 0);
				HashMixInt(Hash, Record.bBoundaryPairFound ? 1 : 0);
				HashMixInt(Hash, Record.bQ1Q2DifferentPlates ? 1 : 0);
				HashMixInt(Hash, Record.Q1PlateId);
				HashMixInt(Hash, Record.Q2PlateId);
				HashMixInt(Hash, Record.AssignedPlateId);
				HashMixDouble(Hash, Record.Q1DistanceRad);
				HashMixDouble(Hash, Record.Q2DistanceRad);
				HashMixDouble(Hash, Record.QGammaDistanceRad);
				HashMixDouble(Hash, Record.QGammaAlpha);
				HashMixDouble(Hash, Record.Q1BoundaryContinentalFraction);
				HashMixDouble(Hash, Record.Q2BoundaryContinentalFraction);
				HashMixString(Hash, Record.SelectionProvenance);
			}
			return Hash;
		}

		uint64 HashStage5Topology(const FCarrierV2Stage5Config& Config, const TArray<FCarrierV2Plate>& RebuiltPlates)
		{
			uint64 Hash = HashStage5Config(Config);
			for (const FCarrierV2Plate& Plate : RebuiltPlates)
			{
				HashMixInt(Hash, Plate.PlateId);
				HashMixInt(Hash, Plate.LocalVertices.Num());
				HashMixInt(Hash, Plate.LocalTriangles.Num());
				for (const FCarrierV2PlateVertex& Vertex : Plate.LocalVertices)
				{
					HashMixInt(Hash, Vertex.LocalVertexId);
					HashMixInt(Hash, Vertex.SourceSampleId);
					HashMixDouble(Hash, Vertex.UnitPosition.X);
					HashMixDouble(Hash, Vertex.UnitPosition.Y);
					HashMixDouble(Hash, Vertex.UnitPosition.Z);
					HashMixInt(Hash, static_cast<int32>(Vertex.Material.MaterialClass));
					HashMixDouble(Hash, Vertex.Material.ContinentalFraction);
					HashMixString(Hash, Vertex.Material.Provenance);
				}
				for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
				{
					HashMixInt(Hash, Triangle.LocalTriangleId);
					HashMixInt(Hash, Triangle.SourceTriangleId);
					for (int32 Corner = 0; Corner < 3; ++Corner)
					{
						HashMixInt(Hash, Triangle.LocalVertexIds[Corner]);
						HashMixInt(Hash, Triangle.SourceSampleIds[Corner]);
					}
				}
			}
			return Hash;
		}

		uint64 HashStage5Metrics(const FCarrierV2Stage5Metrics& Metrics, const bool bIncludeReplayFields)
		{
			uint64 Hash = FnvOffset;
			HashMixString(Hash, Metrics.ConfigHash);
			HashMixString(Hash, Metrics.Stage3ProcessStateHash);
			HashMixString(Hash, Metrics.RemeshInputHash);
			HashMixString(Hash, Metrics.GlobalSamplingHash);
			HashMixString(Hash, Metrics.GapFillHash);
			HashMixString(Hash, Metrics.RebuiltTopologyHash);
			HashMixInt(Hash, Metrics.GlobalSampleCount);
			HashMixInt(Hash, Metrics.GlobalTriangleCount);
			HashMixInt(Hash, Metrics.LocalPlateVertexCountSum);
			HashMixInt(Hash, Metrics.LocalPlateTriangleCountSum);
			HashMixInt(Hash, Metrics.PretreatedPlateCount);
			HashMixInt(Hash, Metrics.FullySubductedPlateDestroyedCount);
			HashMixInt(Hash, Metrics.BoundaryEdgeCount);
			HashMixInt(Hash, Metrics.AabbMeshTriangleCount);
			HashMixInt(Hash, Metrics.AabbRayQueryCount);
			HashMixInt(Hash, Metrics.RawHitCountTotal);
			HashMixInt(Hash, Metrics.FilteredSubductingHitCount);
			HashMixInt(Hash, Metrics.FilteredCollidingHitCount);
			HashMixInt(Hash, Metrics.ValidHitAfterFilterCount);
			HashMixInt(Hash, Metrics.ZeroValidHitCount);
			HashMixInt(Hash, Metrics.Q1Q2GapFillCount);
			HashMixInt(Hash, Metrics.Q1Q2BoundaryQueryCount);
			HashMixInt(Hash, Metrics.Q1Q2BoundaryPairCount);
			HashMixInt(Hash, Metrics.Q1Q2DifferentPlatePairCount);
			HashMixInt(Hash, Metrics.QGammaComputedCount);
			HashMixInt(Hash, Metrics.GeneratedOceanicCount);
			HashMixInt(Hash, Metrics.GapFillNoBoundaryPairCount);
			HashMixInt(Hash, Metrics.PostFilterUnresolvedMultihitCount);
			HashMixInt(Hash, Metrics.PostFilterBoundaryOnlyMultihitCount);
			HashMixInt(Hash, Metrics.BoundaryDegenerateHitCount);
			HashMixInt(Hash, Metrics.MaterialInterpolationCount);
			HashMixInt(Hash, Metrics.SelectedFilteredHitCount);
			HashMixInt(Hash, Metrics.PriorOwnerFallbackCount);
			HashMixInt(Hash, Metrics.CentroidPrimaryResolutionCount);
			HashMixInt(Hash, Metrics.RandomPrimaryResolutionCount);
			HashMixInt(Hash, Metrics.NearestPrimaryResolutionCount);
			HashMixInt(Hash, Metrics.OwnershipRepairDuringRemeshCount);
			HashMixInt(Hash, Metrics.RetentionHysteresisAnchorCount);
			HashMixInt(Hash, Metrics.Q1Q2DiscreteApproxCount);
			HashMixInt(Hash, Metrics.Q1Q2PriorOwnerLookupCount);
			HashMixInt(Hash, Metrics.TopologyRebuildCount);
			HashMixInt(Hash, Metrics.RebuiltPlateCount);
			HashMixInt(Hash, Metrics.RebuiltLocalVertexCountSum);
			HashMixInt(Hash, Metrics.RebuiltLocalTriangleCountSum);
			HashMixInt(Hash, Metrics.RebuiltTriangleAssignmentCount);
			HashMixInt(Hash, Metrics.MixedVertexTriangleCount);
			HashMixInt(Hash, Metrics.MajorityTriangleAssignmentCount);
			HashMixInt(Hash, Metrics.ThreeWayTriangleAssignmentCount);
			HashMixInt(Hash, Metrics.UnassignedTriangleCount);
			HashMixInt(Hash, Metrics.ProcessStateResetCount);
			HashMixInt(Hash, Metrics.PreResetTriangleMarkCount);
			HashMixInt(Hash, Metrics.PostResetTriangleMarkCount);
			HashMixInt(Hash, Metrics.TerrainBeautyMutationCount);
			HashMixInt(Hash, Metrics.MaterialCreatedWithoutGapFillCount);
			HashMixInt(Hash, Metrics.MaterialDestroyedWithoutProcessCount);
			HashMixInt(Hash, Metrics.bInheritedStage3Pass ? 1 : 0);
			HashMixInt(Hash, Metrics.bSourceFilterPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bQ1Q2GapFillPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bTopologyRebuildPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bProcessResetPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bNoForbiddenFallbackPass ? 1 : 0);
			if (bIncludeReplayFields)
			{
				HashMixString(Hash, Metrics.ReplayStage3ProcessStateHash);
				HashMixString(Hash, Metrics.ReplayGlobalSamplingHash);
				HashMixString(Hash, Metrics.ReplayGapFillHash);
				HashMixString(Hash, Metrics.ReplayRebuiltTopologyHash);
				HashMixString(Hash, Metrics.ReplayMetricsHash);
				HashMixInt(Hash, Metrics.bReplayDeterministic ? 1 : 0);
				HashMixString(Hash, Metrics.Verdict);
			}
			return Hash;
		}

		void CountStage5PretreatedPlates(
			const FCarrierV2BuildState& State,
			const TMap<uint64, FCarrierV2Stage4MarkFlags>& MarkLookup,
			FCarrierV2Stage5Metrics& Metrics)
		{
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				bool bHasNonSubductingTriangle = false;
				for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
				{
					const FCarrierV2Stage4MarkFlags* Flags = MarkLookup.Find(Stage4TriangleKey(Plate.PlateId, Triangle.LocalTriangleId));
					if (Flags == nullptr || !Flags->bSubducting)
					{
						bHasNonSubductingTriangle = true;
						break;
					}
				}
				if (bHasNonSubductingTriangle)
				{
					++Metrics.PretreatedPlateCount;
				}
				else
				{
					++Metrics.FullySubductedPlateDestroyedCount;
				}
			}
		}

		void SampleStage5GlobalTds(
			const FCarrierV2Stage5Config& Config,
			const FCarrierV2BuildState& State,
			const TMap<uint64, FCarrierV2Stage4MarkFlags>& MarkLookup,
			const TArray<FCarrierV2Stage5BoundaryEdge>& BoundaryEdges,
			UE::Geometry::FDynamicMeshAABBTree3& Tree,
			const TMap<int32, FCarrierV2Stage5TreeTriangleRef>& TreeTriangleRefs,
			TArray<int32>& OutSamplePlateAssignments,
			TArray<FCarrierV2MaterialRecord>& OutSampleMaterials,
			FCarrierV2Stage5FixtureResult& Result)
		{
			const double SamplingStart = FPlatformTime::Seconds();
			Result.SampleRecords.Reset(State.Samples.Num());
			OutSamplePlateAssignments.Init(INDEX_NONE, State.Samples.Num());
			OutSampleMaterials.SetNum(State.Samples.Num());

			for (const FCarrierV2SubstrateSample& Sample : State.Samples)
			{
				FCarrierV2Stage5SampleRecord Record;
				Record.SampleId = Sample.SampleId;

				TArray<MeshIntersection::FHitIntersectionResult> RawHits;
				const FRay3d Ray(FVector3d::ZeroVector, Sample.UnitPosition);
				UE::Geometry::IMeshSpatial::FQueryOptions QueryOptions;
				QueryOptions.MaxDistance = 2.0;
				++Result.Metrics.AabbRayQueryCount;
				Tree.FindAllHitTriangles(Ray, RawHits, QueryOptions);

				Record.RawHitCount = RawHits.Num();
				Result.Metrics.RawHitCountTotal += RawHits.Num();

				TArray<const MeshIntersection::FHitIntersectionResult*, TInlineAllocator<16>> ValidHits;
				TArray<FCarrierV2Stage5TreeTriangleRef, TInlineAllocator<16>> ValidRefs;
				bool bAllValidBoundary = true;
				for (const MeshIntersection::FHitIntersectionResult& Hit : RawHits)
				{
					const FCarrierV2Stage5TreeTriangleRef* Ref = TreeTriangleRefs.Find(Hit.TriangleId);
					if (Ref == nullptr)
					{
						continue;
					}

					const FCarrierV2Stage4MarkFlags* Flags = MarkLookup.Find(Stage4TriangleKey(Ref->PlateId, Ref->LocalTriangleId));
					if (Flags != nullptr && Flags->bSubducting)
					{
						++Record.FilteredSubductingHitCount;
						++Result.Metrics.FilteredSubductingHitCount;
						continue;
					}
					if (Flags != nullptr && Flags->bColliding)
					{
						++Record.FilteredCollidingHitCount;
						++Result.Metrics.FilteredCollidingHitCount;
						continue;
					}

					const bool bBoundary = IsStage4BoundaryBarycentric(
						Hit.BaryCoords,
						Config.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig.BaseConfig.RayEpsilon);
					bAllValidBoundary = bAllValidBoundary && bBoundary;
					if (bBoundary)
					{
						++Result.Metrics.BoundaryDegenerateHitCount;
					}
					ValidHits.Add(&Hit);
					ValidRefs.Add(*Ref);
				}

				Record.ValidHitCount = ValidHits.Num();
				Result.Metrics.ValidHitAfterFilterCount += ValidHits.Num();
				if (ValidHits.IsEmpty())
				{
					Record.bZeroValidHit = true;
					++Result.Metrics.ZeroValidHitCount;

					FCarrierV2Stage5BoundaryCandidate Q1;
					FCarrierV2Stage5BoundaryCandidate Q2;
					if (!FindStage5Q1Q2BoundaryPair(
						BoundaryEdges,
						Sample.UnitPosition,
						Config.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig.BaseConfig.RayEpsilon,
						Q1,
						Q2,
						Result.Metrics))
					{
						Record.SelectionProvenance = TEXT("q1q2_no_boundary_pair");
						++Result.Metrics.GapFillNoBoundaryPairCount;
						Result.SampleRecords.Add(Record);
						continue;
					}

					const FVector3d QGamma = (Q1.Point + Q2.Point).GetSafeNormal();
					const double DPlate = FMath::Min(Q1.DistanceRad, Q2.DistanceRad);
					const double DGamma = GeodesicDistanceRad(Sample.UnitPosition, QGamma);
					const double Alpha = (DGamma + DPlate) > 1.0e-12 ? DGamma / (DGamma + DPlate) : 0.0;
					const bool bAssignQ2 = Q2.DistanceRad < Q1.DistanceRad;

					Record.bBoundaryPairFound = true;
					Record.bQ1Q2DifferentPlates = Q1.PlateId != Q2.PlateId;
					Record.bGeneratedOceanic = true;
					Record.Q1PlateId = Q1.PlateId;
					Record.Q2PlateId = Q2.PlateId;
					Record.AssignedPlateId = bAssignQ2 ? Q2.PlateId : Q1.PlateId;
					Record.SelectedPlateId = Record.AssignedPlateId;
					Record.Q1DistanceRad = Q1.DistanceRad;
					Record.Q2DistanceRad = Q2.DistanceRad;
					Record.QGammaDistanceRad = DGamma;
					Record.QGammaAlpha = Alpha;
					Record.Q1BoundaryContinentalFraction = Q1.ContinentalFraction;
					Record.Q2BoundaryContinentalFraction = Q2.ContinentalFraction;
					Record.SelectionProvenance = TEXT("continuous_q1q2_qgamma_oceanic_generation");

					OutSamplePlateAssignments[Sample.SampleId] = Record.AssignedPlateId;
					OutSampleMaterials[Sample.SampleId] = MakeStage5Material(0.0, TEXT("q1q2_qgamma_oceanic"));
					++Result.Metrics.Q1Q2GapFillCount;
					++Result.Metrics.Q1Q2BoundaryPairCount;
					++Result.Metrics.Q1Q2DifferentPlatePairCount;
					++Result.Metrics.QGammaComputedCount;
					++Result.Metrics.GeneratedOceanicCount;
					Result.SampleRecords.Add(Record);
					continue;
				}

				if (ValidHits.Num() > 1)
				{
					if (bAllValidBoundary)
					{
						++Result.Metrics.PostFilterBoundaryOnlyMultihitCount;
					}
					else
					{
						++Result.Metrics.PostFilterUnresolvedMultihitCount;
						Record.bPostFilterUnresolvedMultihit = true;
						Record.SelectionProvenance = TEXT("unresolved_post_filter_multihit");
						Result.SampleRecords.Add(Record);
						continue;
					}
				}

				const MeshIntersection::FHitIntersectionResult* SelectedHit = ValidHits[0];
				const FCarrierV2Stage5TreeTriangleRef& SelectedRef = ValidRefs[0];
				Record.SelectedPlateId = SelectedRef.PlateId;
				Record.AssignedPlateId = SelectedRef.PlateId;
				Record.SelectedLocalTriangleId = SelectedRef.LocalTriangleId;
				Record.SelectedSourceTriangleId = SelectedRef.SourceTriangleId;
				Record.SelectionProvenance = ValidHits.Num() == 1
					? TEXT("single_valid_post_filter_intersection")
					: TEXT("boundary_degenerate_valid_intersection");

				const FCarrierV2Stage4MarkFlags* SelectedFlags = MarkLookup.Find(Stage4TriangleKey(SelectedRef.PlateId, SelectedRef.LocalTriangleId));
				Record.bSelectedFilteredHit =
					SelectedFlags != nullptr &&
					(SelectedFlags->bSubducting || SelectedFlags->bColliding);
				if (Record.bSelectedFilteredHit)
				{
					++Result.Metrics.SelectedFilteredHitCount;
				}

				if (State.Plates.IsValidIndex(SelectedRef.PlateId))
				{
					const FCarrierV2Plate& Plate = State.Plates[SelectedRef.PlateId];
					if (Plate.LocalTriangles.IsValidIndex(SelectedRef.LocalTriangleId))
					{
						Record.SelectedContinentalFraction = InterpolateContinentalFraction(
							Plate,
							Plate.LocalTriangles[SelectedRef.LocalTriangleId],
							SelectedHit->BaryCoords);
						OutSamplePlateAssignments[Sample.SampleId] = Record.AssignedPlateId;
						OutSampleMaterials[Sample.SampleId] = MakeStage5Material(Record.SelectedContinentalFraction, TEXT("barycentric_resample"));
						++Result.Metrics.MaterialInterpolationCount;
					}
				}
				Result.SampleRecords.Add(Record);
			}
			Result.Metrics.GlobalSamplingMs = (FPlatformTime::Seconds() - SamplingStart) * 1000.0;
		}

		int32 ChooseStage5TrianglePlate(
			const FCarrierV2SubstrateTriangle& Triangle,
			const TArray<int32>& SampleAssignments,
			const TArray<int32>& SampleAssignmentCounts,
			FCarrierV2Stage5Metrics& Metrics)
		{
			int32 PlateIds[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 SampleId = Triangle.SampleIds[Corner];
				if (!SampleAssignments.IsValidIndex(SampleId) || SampleAssignments[SampleId] == INDEX_NONE)
				{
					++Metrics.UnassignedTriangleCount;
					return INDEX_NONE;
				}
				PlateIds[Corner] = SampleAssignments[SampleId];
			}

			if (PlateIds[0] == PlateIds[1] && PlateIds[1] == PlateIds[2])
			{
				return PlateIds[0];
			}

			++Metrics.MixedVertexTriangleCount;
			if (PlateIds[0] == PlateIds[1] || PlateIds[0] == PlateIds[2])
			{
				++Metrics.MajorityTriangleAssignmentCount;
				return PlateIds[0];
			}
			if (PlateIds[1] == PlateIds[2])
			{
				++Metrics.MajorityTriangleAssignmentCount;
				return PlateIds[1];
			}

			++Metrics.ThreeWayTriangleAssignmentCount;
			int32 BestPlateId = PlateIds[0];
			int32 BestCount = SampleAssignmentCounts.IsValidIndex(BestPlateId) ? SampleAssignmentCounts[BestPlateId] : 0;
			for (int32 Corner = 1; Corner < 3; ++Corner)
			{
				const int32 CandidatePlateId = PlateIds[Corner];
				const int32 CandidateCount = SampleAssignmentCounts.IsValidIndex(CandidatePlateId) ? SampleAssignmentCounts[CandidatePlateId] : 0;
				if (CandidateCount > BestCount || (CandidateCount == BestCount && CandidatePlateId < BestPlateId))
				{
					BestPlateId = CandidatePlateId;
					BestCount = CandidateCount;
				}
			}
			return BestPlateId;
		}

		int32 AddOrFindStage5RebuiltVertex(
			FCarrierV2Plate& Plate,
			const FCarrierV2BuildState& State,
			const TArray<FCarrierV2MaterialRecord>& SampleMaterials,
			const int32 SourceSampleId)
		{
			if (const int32* Existing = Plate.SourceSampleToLocalVertex.Find(SourceSampleId))
			{
				return *Existing;
			}
			if (!State.Samples.IsValidIndex(SourceSampleId) || !SampleMaterials.IsValidIndex(SourceSampleId))
			{
				return INDEX_NONE;
			}

			FCarrierV2PlateVertex Vertex;
			Vertex.LocalVertexId = Plate.LocalVertices.Num();
			Vertex.SourceSampleId = SourceSampleId;
			Vertex.UnitPosition = State.Samples[SourceSampleId].UnitPosition;
			Vertex.Material = SampleMaterials[SourceSampleId];
			Plate.SourceSampleToLocalVertex.Add(SourceSampleId, Vertex.LocalVertexId);
			Plate.LocalVertices.Add(Vertex);
			return Vertex.LocalVertexId;
		}

		void RebuildStage5PlateLocalTopology(
			const FCarrierV2Stage5Config& Config,
			const FCarrierV2BuildState& State,
			const TArray<int32>& SampleAssignments,
			const TArray<FCarrierV2MaterialRecord>& SampleMaterials,
			FCarrierV2Stage5FixtureResult& Result)
		{
			const double RebuildStart = FPlatformTime::Seconds();
			Result.RebuiltPlates.Reset(State.Config.PlateCount);
			for (int32 PlateId = 0; PlateId < State.Config.PlateCount; ++PlateId)
			{
				FCarrierV2Plate Plate;
				Plate.PlateId = PlateId;
				Result.RebuiltPlates.Add(MoveTemp(Plate));
			}

			TArray<int32> SampleAssignmentCounts;
			SampleAssignmentCounts.Init(0, State.Config.PlateCount);
			for (const int32 PlateId : SampleAssignments)
			{
				if (SampleAssignmentCounts.IsValidIndex(PlateId))
				{
					++SampleAssignmentCounts[PlateId];
				}
			}

			for (int32 PlateId = 0; PlateId < Result.RebuiltPlates.Num(); ++PlateId)
			{
				const int32 PlateSampleCount = SampleAssignmentCounts.IsValidIndex(PlateId) ? SampleAssignmentCounts[PlateId] : 0;
				FCarrierV2Plate& Plate = Result.RebuiltPlates[PlateId];
				Plate.LocalVertices.Reserve(PlateSampleCount);
				Plate.SourceSampleToLocalVertex.Reserve(PlateSampleCount);
				Plate.LocalTriangles.Reserve(FMath::Max(1, PlateSampleCount * 2));
			}

			for (const FCarrierV2SubstrateTriangle& SourceTriangle : State.Triangles)
			{
				const int32 PlateId = ChooseStage5TrianglePlate(SourceTriangle, SampleAssignments, SampleAssignmentCounts, Result.Metrics);
				if (!Result.RebuiltPlates.IsValidIndex(PlateId))
				{
					if (PlateId != INDEX_NONE)
					{
						++Result.Metrics.UnassignedTriangleCount;
					}
					continue;
				}

				FCarrierV2Plate& Plate = Result.RebuiltPlates[PlateId];
				FCarrierV2PlateTriangle LocalTriangle;
				LocalTriangle.LocalTriangleId = Plate.LocalTriangles.Num();
				LocalTriangle.SourceTriangleId = SourceTriangle.TriangleId;
				bool bTriangleValid = true;
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					const int32 SourceSampleId = SourceTriangle.SampleIds[Corner];
					LocalTriangle.SourceSampleIds[Corner] = SourceSampleId;
					LocalTriangle.LocalVertexIds[Corner] = AddOrFindStage5RebuiltVertex(Plate, State, SampleMaterials, SourceSampleId);
					if (LocalTriangle.LocalVertexIds[Corner] == INDEX_NONE)
					{
						++Result.Metrics.UnassignedTriangleCount;
						bTriangleValid = false;
						break;
					}
				}
				if (!bTriangleValid)
				{
					continue;
				}
				Plate.LocalTriangles.Add(LocalTriangle);
				++Result.Metrics.RebuiltTriangleAssignmentCount;
			}

			Result.Metrics.TopologyRebuildCount = 1;
			Result.Metrics.ProcessStateResetCount = 1;
			Result.Metrics.PostResetTriangleMarkCount = 0;
			for (const FCarrierV2Plate& Plate : Result.RebuiltPlates)
			{
				Result.Metrics.RebuiltLocalVertexCountSum += Plate.LocalVertices.Num();
				Result.Metrics.RebuiltLocalTriangleCountSum += Plate.LocalTriangles.Num();
				if (!Plate.LocalTriangles.IsEmpty())
				{
					++Result.Metrics.RebuiltPlateCount;
				}
			}
			Result.Metrics.RebuiltTopologyHash = HashToString(HashStage5Topology(Config, Result.RebuiltPlates));
			Result.Metrics.TopologyRebuildMs = (FPlatformTime::Seconds() - RebuildStart) * 1000.0;
		}

		void FinalizeStage5Verdict(const FCarrierV2Stage5Config& Config, FCarrierV2Stage5Metrics& Metrics)
		{
			const FCarrierV2Stage4Config& SamplingConfig = Config.SamplingConfig;
			Metrics.bSourceFilterPass =
				(!SamplingConfig.bRequireFilteredSubductingHit ||
					Metrics.FilteredSubductingHitCount >= FMath::Max(1, SamplingConfig.ExpectedMinimumFilteredSubductingHits)) &&
				(!SamplingConfig.bRequireFilteredCollidingHit ||
					Metrics.FilteredCollidingHitCount >= FMath::Max(1, SamplingConfig.ExpectedMinimumFilteredCollidingHits)) &&
				Metrics.SelectedFilteredHitCount == 0 &&
				Metrics.PostFilterUnresolvedMultihitCount == 0;

			Metrics.bQ1Q2GapFillPass =
				Metrics.GapFillNoBoundaryPairCount == 0 &&
				Metrics.Q1Q2GapFillCount == Metrics.ZeroValidHitCount &&
				Metrics.GeneratedOceanicCount == Metrics.ZeroValidHitCount &&
				Metrics.Q1Q2BoundaryPairCount == Metrics.ZeroValidHitCount &&
				Metrics.Q1Q2DifferentPlatePairCount == Metrics.ZeroValidHitCount &&
				Metrics.QGammaComputedCount == Metrics.ZeroValidHitCount &&
				(!Config.bRequireGeneratedOceanic ||
					Metrics.GeneratedOceanicCount >= FMath::Max(1, Config.ExpectedMinimumGeneratedOceanic)) &&
				(!Config.bRequireContinuousQ1Q2 ||
					Metrics.Q1Q2BoundaryPairCount >= FMath::Max(1, Config.ExpectedMinimumQ1Q2Pairs));

			Metrics.bTopologyRebuildPass =
				(!Config.bRequireTopologyRebuild ||
					(Metrics.TopologyRebuildCount == 1 &&
						Metrics.RebuiltTriangleAssignmentCount == Metrics.GlobalTriangleCount &&
						Metrics.RebuiltLocalTriangleCountSum == Metrics.GlobalTriangleCount &&
						Metrics.UnassignedTriangleCount == 0 &&
						!Metrics.RebuiltTopologyHash.IsEmpty()));

			Metrics.bProcessResetPass =
				(!Config.bRequireProcessReset ||
					(Metrics.ProcessStateResetCount == 1 && Metrics.PostResetTriangleMarkCount == 0));

			Metrics.bNoForbiddenFallbackPass =
				Metrics.PriorOwnerFallbackCount == 0 &&
				Metrics.CentroidPrimaryResolutionCount == 0 &&
				Metrics.RandomPrimaryResolutionCount == 0 &&
				Metrics.NearestPrimaryResolutionCount == 0 &&
				Metrics.OwnershipRepairDuringRemeshCount == 0 &&
				Metrics.RetentionHysteresisAnchorCount == 0 &&
				Metrics.Q1Q2DiscreteApproxCount == 0 &&
				Metrics.Q1Q2PriorOwnerLookupCount == 0 &&
				Metrics.MaterialCreatedWithoutGapFillCount == 0 &&
				Metrics.MaterialDestroyedWithoutProcessCount == 0 &&
				Metrics.TerrainBeautyMutationCount == 0;

			Metrics.bFixturePass =
				Metrics.bInheritedStage3Pass &&
				Metrics.bSourceFilterPass &&
				Metrics.bQ1Q2GapFillPass &&
				Metrics.bTopologyRebuildPass &&
				Metrics.bProcessResetPass &&
				Metrics.bNoForbiddenFallbackPass;
			Metrics.bStageGatePass = Metrics.bFixturePass;

			if (!Metrics.bInheritedStage3Pass)
			{
				Metrics.Verdict = TEXT("FAIL_INHERITED_V2_3_GATE");
			}
			else if (!Metrics.bSourceFilterPass)
			{
				Metrics.Verdict = TEXT("FAIL_FILTERED_MARK_GATE");
			}
			else if (!Metrics.bQ1Q2GapFillPass)
			{
				Metrics.Verdict = TEXT("FAIL_Q1Q2_GAP_FILL_GATE");
			}
			else if (!Metrics.bTopologyRebuildPass)
			{
				Metrics.Verdict = TEXT("FAIL_TOPOLOGY_REBUILD_GATE");
			}
			else if (!Metrics.bProcessResetPass)
			{
				Metrics.Verdict = TEXT("FAIL_PROCESS_RESET_GATE");
			}
			else if (!Metrics.bNoForbiddenFallbackPass)
			{
				Metrics.Verdict = TEXT("FAIL_FORBIDDEN_REPAIR_OR_RESOLVER");
			}
			else if (Metrics.ZeroValidHitCount > 0)
			{
				Metrics.Verdict = TEXT("PASS_Q1Q2_GAP_FILL_REBUILD_RESET");
			}
			else
			{
				Metrics.Verdict = TEXT("PASS_REBUILD_RESET_NO_GAPS");
			}
		}

		bool RunStage5FixtureOnce(const FCarrierV2Stage5Config& Config, FCarrierV2Stage5FixtureResult& OutResult)
		{
			OutResult = FCarrierV2Stage5FixtureResult();
			OutResult.Config = Config;
			OutResult.Metrics.RunId = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			OutResult.Metrics.FixtureId = Config.FixtureId;
			OutResult.Metrics.FixtureName = Config.FixtureName;
			OutResult.Metrics.FixtureKind = FixtureKindName(Config.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig.BaseConfig.FixtureKind);
			OutResult.Metrics.ExpectedRemeshClass = Config.ExpectedRemeshClass;
			OutResult.Metrics.SourceStatus = Config.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig.SourceStatus;
			OutResult.Metrics.RemeshSamplingPolicyId = Config.RemeshSamplingPolicyId;
			OutResult.Metrics.TrianglePartitionPolicyId = Config.TrianglePartitionPolicyId;
			OutResult.Metrics.RemeshTriggerReason = Config.RemeshTriggerReason;
			OutResult.Metrics.SampleCount = Config.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig.BaseConfig.SampleCount;
			OutResult.Metrics.PlateCount = Config.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig.BaseConfig.PlateCount;
			OutResult.Metrics.RayEpsilon = Config.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig.BaseConfig.RayEpsilon;
			OutResult.Metrics.ConfigHash = HashToString(HashStage5Config(Config));

			const double TotalStart = FPlatformTime::Seconds();
			const double Stage3Start = FPlatformTime::Seconds();
			FCarrierV2Stage3::RunFixtureWithReplay(Config.SamplingConfig.ProcessConfig, OutResult.Stage3Result);
			OutResult.Metrics.Stage3Ms = (FPlatformTime::Seconds() - Stage3Start) * 1000.0;
			OutResult.Metrics.Stage3ProcessStateHash = OutResult.Stage3Result.Metrics.ProcessStateHash;
			OutResult.Metrics.PreResetTriangleMarkCount = OutResult.Stage3Result.TriangleMarks.Num();
			OutResult.Metrics.bInheritedStage3Pass =
				OutResult.Stage3Result.bCompleted &&
				OutResult.Stage3Result.Metrics.bFixturePass &&
				OutResult.Stage3Result.Metrics.bReplayDeterministic;
			if (!OutResult.Stage3Result.bCompleted)
			{
				OutResult.Error = OutResult.Stage3Result.Error;
				OutResult.Metrics.Verdict = TEXT("FAIL_INHERITED_V2_3_RUN");
				OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
				return false;
			}

			FCarrierV2BuildState State;
			FCarrierV2Stage0Metrics BuildMetrics;
			if (!BuildStateForConfig(Config.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig.BaseConfig, State, BuildMetrics, OutResult.Error))
			{
				OutResult.Metrics.Verdict = TEXT("FAIL_BUILD");
				OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
				return false;
			}
			OutResult.Metrics.BuildSubstrateMs = BuildMetrics.BuildSubstrateMs;
			OutResult.Metrics.BuildPlateLocalMs = BuildMetrics.BuildPlateLocalMs;
			OutResult.Metrics.GlobalSampleCount = State.Samples.Num();
			OutResult.Metrics.GlobalTriangleCount = State.Triangles.Num();
			OutResult.Metrics.TriangleCount = State.Triangles.Num();
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				OutResult.Metrics.LocalPlateVertexCountSum += Plate.LocalVertices.Num();
				OutResult.Metrics.LocalPlateTriangleCountSum += Plate.LocalTriangles.Num();
			}

			FCarrierV2Stage1Metrics MotionMetrics;
			ApplyStage1MotionAndMeasure(Config.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig, State, MotionMetrics);
			OutResult.Metrics.MotionApplyMs = MotionMetrics.MotionApplyMs;
			OutResult.Metrics.RemeshInputHash = HashToString(HashStage1Authority(State, Config.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig));

			TMap<uint64, FCarrierV2Stage4MarkFlags> MarkLookup;
			BuildStage4MarkLookup(OutResult.Stage3Result.TriangleMarks, MarkLookup);
			CountStage5PretreatedPlates(State, MarkLookup, OutResult.Metrics);

			TArray<FCarrierV2Stage5BoundaryEdge> BoundaryEdges;
			BuildStage5BoundaryEdges(State, MarkLookup, BoundaryEdges, OutResult.Metrics);

			UE::Geometry::FDynamicMesh3 Mesh;
			TMap<int32, FCarrierV2Stage5TreeTriangleRef> TreeTriangleRefs;
			const double AabbStart = FPlatformTime::Seconds();
			if (!BuildStage5AabbMesh(State, Mesh, TreeTriangleRefs, OutResult.Metrics, OutResult.Error))
			{
				OutResult.Metrics.Verdict = TEXT("FAIL_AABB_BUILD");
				OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
				return false;
			}
			UE::Geometry::FDynamicMeshAABBTree3 Tree(&Mesh, true);
			OutResult.Metrics.AabbBuildMs = (FPlatformTime::Seconds() - AabbStart) * 1000.0;

			const double MetricsStart = FPlatformTime::Seconds();
			TArray<int32> SampleAssignments;
			TArray<FCarrierV2MaterialRecord> SampleMaterials;
			SampleStage5GlobalTds(Config, State, MarkLookup, BoundaryEdges, Tree, TreeTriangleRefs, SampleAssignments, SampleMaterials, OutResult);
			OutResult.Metrics.GlobalSamplingHash = HashToString(HashStage5SamplingRecords(Config, OutResult.Metrics, OutResult.SampleRecords));
			OutResult.Metrics.GapFillHash = OutResult.Metrics.GlobalSamplingHash;
			RebuildStage5PlateLocalTopology(Config, State, SampleAssignments, SampleMaterials, OutResult);
			FinalizeStage5Verdict(Config, OutResult.Metrics);
			OutResult.Metrics.MetricsMs = (FPlatformTime::Seconds() - MetricsStart) * 1000.0 -
				OutResult.Metrics.GlobalSamplingMs -
				OutResult.Metrics.TopologyRebuildMs;
			if (OutResult.Metrics.MetricsMs < 0.0)
			{
				OutResult.Metrics.MetricsMs = 0.0;
			}
			OutResult.Metrics.MetricsHash = HashToString(HashStage5Metrics(OutResult.Metrics, true));
			OutResult.Metrics.PeakMemoryMb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);
			OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
			OutResult.bCompleted = true;
			return true;
		}

		FCarrierV2MaterialRecord MakeMilestone2Material(const double ContinentalFraction, const FString& Provenance)
		{
			return MakeStage5Material(ContinentalFraction, Provenance);
		}

		double EstimateDeferredOverlapContinentalFraction(const TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>>& Hits)
		{
			if (Hits.IsEmpty())
			{
				return 0.0;
			}
			double Sum = 0.0;
			for (const FCarrierV2Stage1ProjectionHit& Hit : Hits)
			{
				Sum += Hit.ContinentalFraction;
			}
			return Sum / static_cast<double>(Hits.Num());
		}

		void AccumulateDeferredOverlapAccounting(
			const FCarrierV2SubstrateSample& Sample,
			const TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>>& Hits,
			FCarrierV2Milestone2Metrics& Metrics)
		{
			++Metrics.DeferredOverlapSampleCount;
			Metrics.DeferredOverlapAreaWeight += Sample.AreaWeight;
			Metrics.DeferredOverlapContinentalMassEstimate +=
				Sample.AreaWeight * EstimateDeferredOverlapContinentalFraction(Hits);
		}

		const TCHAR* Milestone2RequirementStatus(const bool bRequired, const bool bPass)
		{
			if (!bRequired)
			{
				return TEXT("characterization");
			}
			return bPass ? TEXT("pass") : TEXT("fail");
		}

		const TCHAR* Milestone2TopologyStatus(const FCarrierV2Milestone2Config& Config, const FCarrierV2Milestone2Metrics& Metrics)
		{
			if (Config.bRequireFullTopologyRebuild)
			{
				return Metrics.bTopologyRebuildPass ? TEXT("full pass") : TEXT("fail");
			}
			return Metrics.bUnassignedTriangleBudgetPass ? TEXT("bounded deferred") : TEXT("fail");
		}

		double Milestone2PaperResampleBudgetMs(const FCarrierV2Milestone2Metrics& Metrics)
		{
			return Metrics.GlobalSampleCount == 250000 ? 3580.0 : 0.0;
		}

		void CopyMilestone2TopologyMetrics(const FCarrierV2BuildState& State, FCarrierV2Milestone2Metrics& Metrics)
		{
			Metrics.GlobalSampleCount = State.Samples.Num();
			Metrics.GlobalTriangleCount = State.Triangles.Num();
			Metrics.TriangleCount = State.Triangles.Num();
			Metrics.LocalPlateVertexCountSum = 0;
			Metrics.LocalPlateTriangleCountSum = 0;
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				Metrics.LocalPlateVertexCountSum += Plate.LocalVertices.Num();
				Metrics.LocalPlateTriangleCountSum += Plate.LocalTriangles.Num();
			}
		}

		uint64 HashMilestone2Config(const FCarrierV2Milestone2Config& Config)
		{
			uint64 Hash = HashStage1Config(Config.MotionConfig);
			HashMixString(Hash, Config.FixtureId);
			HashMixString(Hash, Config.FixtureName);
			HashMixString(Hash, Config.CarrierCycleClass);
			HashMixString(Hash, Config.ResamplePolicyId);
			HashMixString(Hash, Config.TrianglePartitionPolicyId);
			HashMixString(Hash, Config.ResampleTriggerReason);
			HashMixInt(Hash, Config.ResampleCadenceSteps);
			HashMixInt(Hash, Config.LifecycleWindowCount);
			HashMixInt(Hash, Config.bRequireSingleHitWrites ? 1 : 0);
			HashMixInt(Hash, Config.bRequireDivergentGapFill ? 1 : 0);
			HashMixInt(Hash, Config.bRequireOverlapBlocked ? 1 : 0);
			HashMixInt(Hash, Config.bRequireFullTopologyRebuild ? 1 : 0);
			HashMixInt(Hash, Config.bRequireMaterialConservation ? 1 : 0);
			HashMixInt(Hash, Config.bRequireSharpnessPreservation ? 1 : 0);
			HashMixInt(Hash, Config.bInjectPriorOwnerLabelsForNegativeControl ? 1 : 0);
			HashMixInt(Hash, Config.bScaleCharacterization ? 1 : 0);
			HashMixInt(Hash, Config.ExpectedMinimumSingleHitWrites);
			HashMixInt(Hash, Config.ExpectedMinimumGapFill);
			HashMixInt(Hash, Config.ExpectedMinimumOverlapBlocked);
			HashMixDouble(Hash, Config.MaterialConservationTolerance);
			HashMixDouble(Hash, Config.TotalVariationTolerance);
			HashMixDouble(Hash, Config.ExpectedMaxStepKernelMs);
			return Hash;
		}

		uint64 HashMilestone2SamplingRecords(
			const FCarrierV2Milestone2Config& Config,
			const FCarrierV2Milestone2Metrics& Metrics,
			const TArray<FCarrierV2Milestone2SampleRecord>& SampleRecords)
		{
			uint64 Hash = HashMilestone2Config(Config);
			HashMixString(Hash, Metrics.PreCycleAuthorityHash);
			for (const FCarrierV2Milestone2SampleRecord& Record : SampleRecords)
			{
				HashMixInt(Hash, Record.SampleId);
				HashMixInt(Hash, Record.RawHitCount);
				HashMixInt(Hash, Record.ValidSingleHitCount);
				HashMixInt(Hash, Record.SelectedPlateId);
				HashMixInt(Hash, Record.SelectedLocalTriangleId);
				HashMixInt(Hash, Record.SelectedSourceTriangleId);
				HashMixInt(Hash, Record.AssignedPlateId);
				HashMixDouble(Hash, Record.SelectedContinentalFraction);
				HashMixInt(Hash, Record.bSingleHitWritten ? 1 : 0);
				HashMixInt(Hash, Record.bDivergentZeroHit ? 1 : 0);
				HashMixInt(Hash, Record.bGeneratedOceanic ? 1 : 0);
				HashMixInt(Hash, Record.bNondegenerateOverlapBlocked ? 1 : 0);
				HashMixInt(Hash, Record.bBoundaryOnlyOverlap ? 1 : 0);
				HashMixInt(Hash, Record.bBoundaryPairFound ? 1 : 0);
				HashMixInt(Hash, Record.bQ1Q2DifferentPlates ? 1 : 0);
				HashMixInt(Hash, Record.Q1PlateId);
				HashMixInt(Hash, Record.Q2PlateId);
				HashMixDouble(Hash, Record.Q1DistanceRad);
				HashMixDouble(Hash, Record.Q2DistanceRad);
				HashMixDouble(Hash, Record.QGammaDistanceRad);
				HashMixDouble(Hash, Record.QGammaAlpha);
				HashMixDouble(Hash, Record.Q1BoundaryContinentalFraction);
				HashMixDouble(Hash, Record.Q2BoundaryContinentalFraction);
				HashMixString(Hash, Record.SelectionProvenance.ToString());
			}
			return Hash;
		}

		uint64 HashMilestone2Topology(const FCarrierV2Milestone2Config& Config, const TArray<FCarrierV2Plate>& RebuiltPlates)
		{
			uint64 Hash = HashMilestone2Config(Config);
			for (const FCarrierV2Plate& Plate : RebuiltPlates)
			{
				HashMixInt(Hash, Plate.PlateId);
				HashMixInt(Hash, Plate.LocalVertices.Num());
				HashMixInt(Hash, Plate.LocalTriangles.Num());
				for (const FCarrierV2PlateVertex& Vertex : Plate.LocalVertices)
				{
					HashMixInt(Hash, Vertex.LocalVertexId);
					HashMixInt(Hash, Vertex.SourceSampleId);
					HashMixDouble(Hash, Vertex.UnitPosition.X);
					HashMixDouble(Hash, Vertex.UnitPosition.Y);
					HashMixDouble(Hash, Vertex.UnitPosition.Z);
					HashMixInt(Hash, static_cast<int32>(Vertex.Material.MaterialClass));
					HashMixDouble(Hash, Vertex.Material.ContinentalFraction);
					HashMixString(Hash, Vertex.Material.Provenance);
				}
				for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
				{
					HashMixInt(Hash, Triangle.LocalTriangleId);
					HashMixInt(Hash, Triangle.SourceTriangleId);
					for (int32 Corner = 0; Corner < 3; ++Corner)
					{
						HashMixInt(Hash, Triangle.LocalVertexIds[Corner]);
						HashMixInt(Hash, Triangle.SourceSampleIds[Corner]);
					}
				}
			}
			return Hash;
		}

		uint64 HashMilestone2Metrics(const FCarrierV2Milestone2Metrics& Metrics, const bool bIncludeReplayFields)
		{
			uint64 Hash = FnvOffset;
			HashMixString(Hash, Metrics.ConfigHash);
			HashMixString(Hash, Metrics.PreCycleAuthorityHash);
			HashMixString(Hash, Metrics.PostCycleAuthorityHash);
			HashMixString(Hash, Metrics.ResampleOutputHash);
			HashMixString(Hash, Metrics.RebuiltTopologyHash);
			HashMixInt(Hash, Metrics.GlobalSampleCount);
			HashMixInt(Hash, Metrics.GlobalTriangleCount);
			HashMixInt(Hash, Metrics.RebuiltTriangleAssignmentCount);
			HashMixInt(Hash, Metrics.MixedVertexTriangleCount);
			HashMixInt(Hash, Metrics.MajorityTriangleAssignmentCount);
			HashMixInt(Hash, Metrics.ThreeWayTriangleAssignmentCount);
			HashMixInt(Hash, Metrics.UnassignedTriangleCount);
			HashMixInt(Hash, Metrics.UnassignedTriangleBudget);
			HashMixInt(Hash, Metrics.BoundaryEdgeCount);
			HashMixInt(Hash, Metrics.AabbRayQueryCount);
			HashMixInt(Hash, Metrics.RawHitCountTotal);
			HashMixInt(Hash, Metrics.BroadphaseEquivalenceMismatchCount);
			HashMixInt(Hash, Metrics.ValidSingleHitWriteCount);
			HashMixInt(Hash, Metrics.MaterialInterpolationCount);
			HashMixInt(Hash, Metrics.DivergentZeroHitCount);
			HashMixInt(Hash, Metrics.Q1Q2GapFillCount);
			HashMixInt(Hash, Metrics.Q1Q2BoundaryQueryCount);
			HashMixInt(Hash, Metrics.Q1Q2BoundaryPairCount);
			HashMixInt(Hash, Metrics.Q1Q2DifferentPlatePairCount);
			HashMixInt(Hash, Metrics.QGammaComputedCount);
			HashMixInt(Hash, Metrics.GeneratedOceanicCount);
			HashMixInt(Hash, Metrics.GapFillNoBoundaryPairCount);
			HashMixInt(Hash, Metrics.NondegenerateOverlapBlockedCount);
			HashMixInt(Hash, Metrics.BoundaryOnlyOverlapCount);
			HashMixInt(Hash, Metrics.CrossPlateBoundaryOnlyOverlapCount);
			HashMixInt(Hash, Metrics.SamePlateBoundaryOnlyMultihitCount);
			HashMixInt(Hash, Metrics.DeferredOverlapSampleCount);
			HashMixDouble(Hash, Metrics.DeferredOverlapAreaWeight);
			HashMixDouble(Hash, Metrics.DeferredOverlapContinentalMassEstimate);
			HashMixInt(Hash, Metrics.UnsupportedOverlapWriteAttemptCount);
			HashMixInt(Hash, Metrics.PriorOwnerReadCount);
			HashMixInt(Hash, Metrics.PriorOwnerFallbackCount);
			HashMixInt(Hash, Metrics.GlobalOwnerReadCount);
			HashMixInt(Hash, Metrics.CentroidPrimaryResolutionCount);
			HashMixInt(Hash, Metrics.RandomPrimaryResolutionCount);
			HashMixInt(Hash, Metrics.NearestPrimaryResolutionCount);
			HashMixInt(Hash, Metrics.OwnershipRepairDuringResampleCount);
			HashMixInt(Hash, Metrics.RetentionHysteresisAnchorCount);
			HashMixInt(Hash, Metrics.Q1Q2DiscreteApproxCount);
			HashMixInt(Hash, Metrics.Q1Q2PriorOwnerLookupCount);
			HashMixInt(Hash, Metrics.TerrainBeautyMutationCount);
			HashMixInt(Hash, Metrics.SubductionMutationCount);
			HashMixInt(Hash, Metrics.CollisionMutationCount);
			HashMixInt(Hash, Metrics.RiftingMutationCount);
			HashMixInt(Hash, Metrics.TopologyRebuildCount);
			HashMixInt(Hash, Metrics.ProcessStateResetCount);
			HashMixInt(Hash, Metrics.RemeshWindowCount);
			HashMixDouble(Hash, Metrics.MaterialConservationDelta);
			HashMixDouble(Hash, Metrics.TotalVariationDelta);
			HashMixString(Hash, Metrics.TopMissPlatePairs);
			HashMixString(Hash, Metrics.TopOverlapPlatePairs);
			HashMixInt(Hash, Metrics.bBroadphaseMarginGatePass ? 1 : 0);
			HashMixInt(Hash, Metrics.bSingleHitTransferPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bDivergentGapFillPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bOverlapPolicyPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bTopologyRebuildPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bUnassignedTriangleBudgetPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bLifecycleConservationPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bNoForbiddenFallbackPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bPerformanceBudgetPass ? 1 : 0);
			HashMixInt(Hash, Metrics.bPaperResampleCycleBudgetPass ? 1 : 0);
			if (bIncludeReplayFields)
			{
				HashMixString(Hash, Metrics.ReplayPreCycleAuthorityHash);
				HashMixString(Hash, Metrics.ReplayPostCycleAuthorityHash);
				HashMixString(Hash, Metrics.ReplayResampleOutputHash);
				HashMixString(Hash, Metrics.ReplayRebuiltTopologyHash);
				HashMixString(Hash, Metrics.ReplayMetricsHash);
				HashMixInt(Hash, Metrics.bReplayDeterministic ? 1 : 0);
				HashMixString(Hash, Metrics.Verdict);
			}
			return Hash;
		}

		void InitializeMilestone2Metrics(const FCarrierV2Milestone2Config& Config, FCarrierV2Milestone2Metrics& Metrics)
		{
			Metrics.RunId = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
			Metrics.FixtureId = Config.FixtureId;
			Metrics.FixtureName = Config.FixtureName;
			Metrics.FixtureKind = FixtureKindName(Config.MotionConfig.BaseConfig.FixtureKind);
			Metrics.CarrierCycleClass = Config.CarrierCycleClass;
			Metrics.SourceStatus = Config.MotionConfig.SourceStatus;
			Metrics.ResamplePolicyId = Config.ResamplePolicyId;
			Metrics.TrianglePartitionPolicyId = Config.TrianglePartitionPolicyId;
			Metrics.ResampleTriggerReason = Config.ResampleTriggerReason;
			Metrics.SampleCount = Config.MotionConfig.BaseConfig.SampleCount;
			Metrics.PlateCount = Config.MotionConfig.BaseConfig.PlateCount;
			Metrics.ResampleCadenceSteps = Config.ResampleCadenceSteps;
			Metrics.LifecycleWindowCount = Config.LifecycleWindowCount;
			Metrics.DtMa = Config.MotionConfig.DtMa;
			Metrics.TotalMotionMa = Config.MotionConfig.DtMa * static_cast<double>(Config.MotionConfig.MotionStepCount) * static_cast<double>(FMath::Max(1, Config.LifecycleWindowCount));
			Metrics.PlanetRadiusKm = Config.MotionConfig.PlanetRadiusKm;
			Metrics.RayEpsilon = Config.MotionConfig.BaseConfig.RayEpsilon;
			Metrics.BroadphaseAngularMarginRad = Config.MotionConfig.BroadphaseAngularMarginRad;
			Metrics.bUsedAngularCapBroadphase = Config.MotionConfig.bUseAngularCapPlateBroadphase;
			Metrics.ConfigHash = HashToString(HashMilestone2Config(Config));
			Metrics.bPerformanceBudgetPass = true;
		}

		double ComputeMilestone2SampleMass(
			const FCarrierV2BuildState& State,
			const TArray<FCarrierV2MaterialRecord>& SampleMaterials)
		{
			double Mass = 0.0;
			for (const FCarrierV2SubstrateSample& Sample : State.Samples)
			{
				if (SampleMaterials.IsValidIndex(Sample.SampleId))
				{
					Mass += Sample.AreaWeight * SampleMaterials[Sample.SampleId].ContinentalFraction;
				}
			}
			return Mass;
		}

		double ComputeMilestone2SampleTotalVariation(
			const FCarrierV2BuildState& State,
			const TArray<FCarrierV2MaterialRecord>& SampleMaterials)
		{
			double Variation = 0.0;
			for (const FCarrierV2SubstrateTriangle& Triangle : State.Triangles)
			{
				const int32 A = Triangle.SampleIds[0];
				const int32 B = Triangle.SampleIds[1];
				const int32 C = Triangle.SampleIds[2];
				if (!SampleMaterials.IsValidIndex(A) || !SampleMaterials.IsValidIndex(B) || !SampleMaterials.IsValidIndex(C))
				{
					continue;
				}
				const double VA = SampleMaterials[A].ContinentalFraction;
				const double VB = SampleMaterials[B].ContinentalFraction;
				const double VC = SampleMaterials[C].ContinentalFraction;
				Variation += (FMath::Abs(VA - VB) + FMath::Abs(VB - VC) + FMath::Abs(VC - VA)) / 3.0;
			}
			return Variation;
		}

		void BuildMilestone2ExpectedSampleMaterials(
			const FCarrierV2BuildState& State,
			TArray<FCarrierV2MaterialRecord>& OutMaterials)
		{
			OutMaterials.SetNum(State.Samples.Num());
			for (const FCarrierV2SubstrateSample& Sample : State.Samples)
			{
				OutMaterials[Sample.SampleId] = ExpectedMaterialForSample(Sample.SampleId, State.Config.Seed);
			}
		}

		struct FCarrierV2Milestone2BoundarySearchEdge
		{
			FCarrierV2Stage5BoundaryEdge Edge;
			FVector3d Midpoint = FVector3d(0.0, 0.0, 1.0);
			double HalfAngleRad = 0.0;
		};

		void BuildMilestone2BoundarySearchEdges(
			const TArray<FCarrierV2Stage5BoundaryEdge>& BoundaryEdges,
			TArray<FCarrierV2Milestone2BoundarySearchEdge>& OutSearchEdges)
		{
			OutSearchEdges.Reset(BoundaryEdges.Num());
			for (const FCarrierV2Stage5BoundaryEdge& Edge : BoundaryEdges)
			{
				FCarrierV2Milestone2BoundarySearchEdge SearchEdge;
				SearchEdge.Edge = Edge;
				SearchEdge.Midpoint = (Edge.A + Edge.B).GetSafeNormal();
				if (SearchEdge.Midpoint.IsNearlyZero())
				{
					SearchEdge.Midpoint = Edge.A.GetSafeNormal();
				}
				SearchEdge.HalfAngleRad = 0.5 * GeodesicDistanceRad(Edge.A, Edge.B);
				OutSearchEdges.Add(SearchEdge);
			}
		}

		bool CanMilestone2BoundaryEdgeImprovePlate(
			const FCarrierV2Milestone2BoundarySearchEdge& SearchEdge,
			const FVector3d& SamplePosition,
			const FCarrierV2Stage5BoundaryCandidate& BestForPlate)
		{
			if (!BestForPlate.bValid)
			{
				return true;
			}

			const double RejectionAngle = BestForPlate.DistanceRad + SearchEdge.HalfAngleRad + 1.0e-12;
			if (RejectionAngle >= UE_DOUBLE_PI)
			{
				return true;
			}

			const double Dot = FVector3d::DotProduct(SamplePosition.GetSafeNormal(), SearchEdge.Midpoint);
			return Dot >= FMath::Cos(RejectionAngle);
		}

		void BuildMilestone2BoundaryEdges(
			const FCarrierV2BuildState& State,
			TArray<FCarrierV2Stage5BoundaryEdge>& OutEdges,
			FCarrierV2Milestone2Metrics& Metrics)
		{
			OutEdges.Reset();
			for (const FCarrierV2Plate& Plate : State.Plates)
			{
				TMap<uint64, FCarrierV2Stage5EdgeAccumulator> EdgeMap;
				for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
				{
					for (int32 EdgeIndex = 0; EdgeIndex < 3; ++EdgeIndex)
					{
						const int32 LocalA = Triangle.LocalVertexIds[EdgeIndex];
						const int32 LocalB = Triangle.LocalVertexIds[(EdgeIndex + 1) % 3];
						if (!Plate.LocalVertices.IsValidIndex(LocalA) || !Plate.LocalVertices.IsValidIndex(LocalB))
						{
							continue;
						}
						FCarrierV2Stage5EdgeAccumulator& Accumulator = EdgeMap.FindOrAdd(Stage5EdgeKey(Plate.PlateId, LocalA, LocalB));
						++Accumulator.Count;
						if (Accumulator.Count == 1)
						{
							Accumulator.Edge.PlateId = Plate.PlateId;
							Accumulator.Edge.LocalTriangleId = Triangle.LocalTriangleId;
							Accumulator.Edge.SourceTriangleId = Triangle.SourceTriangleId;
							Accumulator.Edge.LocalVertexA = LocalA;
							Accumulator.Edge.LocalVertexB = LocalB;
							Accumulator.Edge.SourceSampleA = Plate.LocalVertices[LocalA].SourceSampleId;
							Accumulator.Edge.SourceSampleB = Plate.LocalVertices[LocalB].SourceSampleId;
							Accumulator.Edge.A = Plate.LocalVertices[LocalA].UnitPosition;
							Accumulator.Edge.B = Plate.LocalVertices[LocalB].UnitPosition;
							Accumulator.Edge.MaterialA = Plate.LocalVertices[LocalA].Material;
							Accumulator.Edge.MaterialB = Plate.LocalVertices[LocalB].Material;
						}
					}
				}
				for (const TPair<uint64, FCarrierV2Stage5EdgeAccumulator>& Pair : EdgeMap)
				{
					if (Pair.Value.Count == 1)
					{
						OutEdges.Add(Pair.Value.Edge);
					}
				}
			}
			Metrics.BoundaryEdgeCount = OutEdges.Num();
		}

		bool FindMilestone2Q1Q2BoundaryPair(
			const TArray<FCarrierV2Milestone2BoundarySearchEdge>& BoundaryEdges,
			const FVector3d& SamplePosition,
			const double Epsilon,
			const int32 PlateCount,
			FCarrierV2Stage5BoundaryCandidate& OutQ1,
			FCarrierV2Stage5BoundaryCandidate& OutQ2,
			FCarrierV2Milestone2Metrics& Metrics)
		{
			OutQ1 = FCarrierV2Stage5BoundaryCandidate();
			OutQ2 = FCarrierV2Stage5BoundaryCandidate();
			TArray<FCarrierV2Stage5BoundaryCandidate, TInlineAllocator<64>> BestByPlate;
			BestByPlate.SetNum(FMath::Max(0, PlateCount));
			for (const FCarrierV2Milestone2BoundarySearchEdge& SearchEdge : BoundaryEdges)
			{
				if (!BestByPlate.IsValidIndex(SearchEdge.Edge.PlateId))
				{
					continue;
				}
				FCarrierV2Stage5BoundaryCandidate& BestForPlate = BestByPlate[SearchEdge.Edge.PlateId];
				if (!CanMilestone2BoundaryEdgeImprovePlate(SearchEdge, SamplePosition, BestForPlate))
				{
					continue;
				}
				++Metrics.Q1Q2BoundaryQueryCount;
				const FCarrierV2Stage5BoundaryCandidate Candidate = MakeStage5BoundaryCandidate(SearchEdge.Edge, SamplePosition, Epsilon);
				if (IsBetterStage5BoundaryCandidate(Candidate, OutQ1))
				{
					OutQ1 = Candidate;
				}
				if (IsBetterStage5BoundaryCandidate(Candidate, BestForPlate))
				{
					BestForPlate = Candidate;
				}
			}
			if (!OutQ1.bValid)
			{
				return false;
			}
			for (const FCarrierV2Stage5BoundaryCandidate& Candidate : BestByPlate)
			{
				if (Candidate.bValid && Candidate.PlateId != OutQ1.PlateId && IsBetterStage5BoundaryCandidate(Candidate, OutQ2))
				{
					OutQ2 = Candidate;
				}
			}
			return OutQ2.bValid;
		}

		int32 ChooseMilestone2TrianglePlate(
			const FCarrierV2SubstrateTriangle& Triangle,
			const TArray<int32>& SampleAssignments,
			const TArray<int32>& SampleAssignmentCounts,
			FCarrierV2Milestone2Metrics& Metrics)
		{
			int32 PlateIds[3] = {INDEX_NONE, INDEX_NONE, INDEX_NONE};
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 SampleId = Triangle.SampleIds[Corner];
				if (!SampleAssignments.IsValidIndex(SampleId) || SampleAssignments[SampleId] == INDEX_NONE)
				{
					++Metrics.UnassignedTriangleCount;
					return INDEX_NONE;
				}
				PlateIds[Corner] = SampleAssignments[SampleId];
			}

			if (PlateIds[0] == PlateIds[1] && PlateIds[1] == PlateIds[2])
			{
				return PlateIds[0];
			}

			++Metrics.MixedVertexTriangleCount;
			if (PlateIds[0] == PlateIds[1] || PlateIds[0] == PlateIds[2])
			{
				++Metrics.MajorityTriangleAssignmentCount;
				return PlateIds[0];
			}
			if (PlateIds[1] == PlateIds[2])
			{
				++Metrics.MajorityTriangleAssignmentCount;
				return PlateIds[1];
			}

			++Metrics.ThreeWayTriangleAssignmentCount;
			int32 BestPlateId = PlateIds[0];
			int32 BestCount = SampleAssignmentCounts.IsValidIndex(BestPlateId) ? SampleAssignmentCounts[BestPlateId] : 0;
			for (int32 Corner = 1; Corner < 3; ++Corner)
			{
				const int32 CandidatePlateId = PlateIds[Corner];
				const int32 CandidateCount = SampleAssignmentCounts.IsValidIndex(CandidatePlateId) ? SampleAssignmentCounts[CandidatePlateId] : 0;
				if (CandidateCount > BestCount || (CandidateCount == BestCount && CandidatePlateId < BestPlateId))
				{
					BestPlateId = CandidatePlateId;
					BestCount = CandidateCount;
				}
			}
			return BestPlateId;
		}

		int32 AddOrFindMilestone2RebuiltVertex(
			FCarrierV2Plate& Plate,
			const FCarrierV2BuildState& State,
			const TArray<FCarrierV2MaterialRecord>& SampleMaterials,
			const int32 SourceSampleId)
		{
			if (const int32* Existing = Plate.SourceSampleToLocalVertex.Find(SourceSampleId))
			{
				return *Existing;
			}
			if (!State.Samples.IsValidIndex(SourceSampleId) || !SampleMaterials.IsValidIndex(SourceSampleId))
			{
				return INDEX_NONE;
			}

			FCarrierV2PlateVertex Vertex;
			Vertex.LocalVertexId = Plate.LocalVertices.Num();
			Vertex.SourceSampleId = SourceSampleId;
			Vertex.UnitPosition = State.Samples[SourceSampleId].UnitPosition;
			Vertex.Material = SampleMaterials[SourceSampleId];
			Plate.SourceSampleToLocalVertex.Add(SourceSampleId, Vertex.LocalVertexId);
			Plate.LocalVertices.Add(Vertex);
			return Vertex.LocalVertexId;
		}

		void RebuildMilestone2PlateLocalTopology(
			const FCarrierV2Milestone2Config& Config,
			const FCarrierV2BuildState& State,
			const TArray<int32>& SampleAssignments,
			const TArray<FCarrierV2MaterialRecord>& SampleMaterials,
			FCarrierV2Milestone2FixtureResult& Result)
		{
			const double RebuildStart = FPlatformTime::Seconds();
			Result.RebuiltPlates.Reset(State.Config.PlateCount);
			for (int32 PlateId = 0; PlateId < State.Config.PlateCount; ++PlateId)
			{
				FCarrierV2Plate Plate;
				Plate.PlateId = PlateId;
				Result.RebuiltPlates.Add(MoveTemp(Plate));
			}

			TArray<int32> SampleAssignmentCounts;
			SampleAssignmentCounts.Init(0, State.Config.PlateCount);
			for (const int32 PlateId : SampleAssignments)
			{
				if (SampleAssignmentCounts.IsValidIndex(PlateId))
				{
					++SampleAssignmentCounts[PlateId];
				}
			}

			for (const FCarrierV2SubstrateTriangle& SourceTriangle : State.Triangles)
			{
				const int32 PlateId = ChooseMilestone2TrianglePlate(SourceTriangle, SampleAssignments, SampleAssignmentCounts, Result.Metrics);
				if (!Result.RebuiltPlates.IsValidIndex(PlateId))
				{
					continue;
				}

				FCarrierV2Plate& Plate = Result.RebuiltPlates[PlateId];
				FCarrierV2PlateTriangle LocalTriangle;
				LocalTriangle.LocalTriangleId = Plate.LocalTriangles.Num();
				LocalTriangle.SourceTriangleId = SourceTriangle.TriangleId;
				bool bTriangleValid = true;
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					const int32 SourceSampleId = SourceTriangle.SampleIds[Corner];
					LocalTriangle.SourceSampleIds[Corner] = SourceSampleId;
					LocalTriangle.LocalVertexIds[Corner] = AddOrFindMilestone2RebuiltVertex(Plate, State, SampleMaterials, SourceSampleId);
					if (LocalTriangle.LocalVertexIds[Corner] == INDEX_NONE)
					{
						++Result.Metrics.UnassignedTriangleCount;
						bTriangleValid = false;
						break;
					}
				}
				if (!bTriangleValid)
				{
					continue;
				}
				Plate.LocalTriangles.Add(LocalTriangle);
				++Result.Metrics.RebuiltTriangleAssignmentCount;
			}

			Result.Metrics.TopologyRebuildCount += 1;
			Result.Metrics.ProcessStateResetCount += 1;
			Result.Metrics.RebuiltPlateCount = 0;
			Result.Metrics.RebuiltLocalVertexCountSum = 0;
			Result.Metrics.RebuiltLocalTriangleCountSum = 0;
			for (const FCarrierV2Plate& Plate : Result.RebuiltPlates)
			{
				Result.Metrics.RebuiltLocalVertexCountSum += Plate.LocalVertices.Num();
				Result.Metrics.RebuiltLocalTriangleCountSum += Plate.LocalTriangles.Num();
				if (!Plate.LocalTriangles.IsEmpty())
				{
					++Result.Metrics.RebuiltPlateCount;
				}
			}
			Result.Metrics.RebuiltTopologyHash = HashToString(HashMilestone2Topology(Config, Result.RebuiltPlates));
			Result.Metrics.TopologyRebuildMs += (FPlatformTime::Seconds() - RebuildStart) * 1000.0;
		}

		void SampleMilestone2GlobalTds(
			const FCarrierV2Milestone2Config& Config,
			const FCarrierV2BuildState& State,
			const TArray<TUniquePtr<FCarrierV2Stage1PlateQueryRuntime>>& Runtimes,
			const TArray<FCarrierV2Milestone2BoundarySearchEdge>& BoundaryEdges,
			TArray<int32>& OutSamplePlateAssignments,
			TArray<FCarrierV2MaterialRecord>& OutSampleMaterials,
			FCarrierV2Milestone2FixtureResult& Result)
		{
			const double SamplingStart = FPlatformTime::Seconds();
			OutSamplePlateAssignments.Init(INDEX_NONE, State.Samples.Num());
			OutSampleMaterials.SetNum(State.Samples.Num());
			Result.SampleRecords.Reserve(Result.SampleRecords.Num() + State.Samples.Num());

			TMap<uint64, int32> MissPairCounts;
			TMap<uint64, int32> OverlapPairCounts;
			MissPairCounts.Reserve(State.Config.PlateCount);
			OverlapPairCounts.Reserve(State.Config.PlateCount);
			FCarrierV2Stage1Metrics QueryMetrics;

			for (const FCarrierV2SubstrateSample& Sample : State.Samples)
			{
				FCarrierV2Milestone2SampleRecord Record;
				Record.SampleId = Sample.SampleId;

				TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>> Hits;
				CollectStage1AabbHits(Config.MotionConfig, State, Sample, Runtimes, QueryMetrics, Hits);
				Record.RawHitCount = Hits.Num();
				Result.Metrics.RawHitCountTotal += Hits.Num();

				if (Config.MotionConfig.bRunAllPlateBroadphaseEquivalence)
				{
					TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>> AllPlateHits;
					CollectStage1AabbHits(Config.MotionConfig, State, Sample, Runtimes, QueryMetrics, AllPlateHits, true);
					if (ClassifyStage1HitSet(Hits) != ClassifyStage1HitSet(AllPlateHits))
					{
						++Result.Metrics.BroadphaseEquivalenceMismatchCount;
					}
				}

				if (Hits.IsEmpty())
				{
					Record.bDivergentZeroHit = true;
					++Result.Metrics.DivergentZeroHitCount;
					AccumulateStage1MissAttribution(State, Sample, MissPairCounts);

					FCarrierV2Stage5BoundaryCandidate Q1;
					FCarrierV2Stage5BoundaryCandidate Q2;
					if (!FindMilestone2Q1Q2BoundaryPair(
						BoundaryEdges,
						Sample.UnitPosition,
						Config.MotionConfig.BaseConfig.RayEpsilon,
						Config.MotionConfig.BaseConfig.PlateCount,
						Q1,
						Q2,
						Result.Metrics))
					{
						Record.SelectionProvenance = TEXT("q1q2_no_boundary_pair");
						++Result.Metrics.GapFillNoBoundaryPairCount;
						Result.SampleRecords.Add(Record);
						continue;
					}

					const FVector3d QGamma = (Q1.Point + Q2.Point).GetSafeNormal();
					const double DPlate = FMath::Min(Q1.DistanceRad, Q2.DistanceRad);
					const double DGamma = GeodesicDistanceRad(Sample.UnitPosition, QGamma);
					const double Alpha = (DGamma + DPlate) > 1.0e-12 ? DGamma / (DGamma + DPlate) : 0.0;
					const bool bAssignQ2 = Q2.DistanceRad < Q1.DistanceRad;

					Record.bBoundaryPairFound = true;
					Record.bQ1Q2DifferentPlates = Q1.PlateId != Q2.PlateId;
					Record.bGeneratedOceanic = true;
					Record.Q1PlateId = Q1.PlateId;
					Record.Q2PlateId = Q2.PlateId;
					Record.AssignedPlateId = bAssignQ2 ? Q2.PlateId : Q1.PlateId;
					Record.SelectedPlateId = Record.AssignedPlateId;
					Record.Q1DistanceRad = Q1.DistanceRad;
					Record.Q2DistanceRad = Q2.DistanceRad;
					Record.QGammaDistanceRad = DGamma;
					Record.QGammaAlpha = Alpha;
					Record.Q1BoundaryContinentalFraction = Q1.ContinentalFraction;
					Record.Q2BoundaryContinentalFraction = Q2.ContinentalFraction;
					Record.SelectionProvenance = TEXT("continuous_q1q2_qgamma_oceanic_generation");

					OutSamplePlateAssignments[Sample.SampleId] = Record.AssignedPlateId;
					OutSampleMaterials[Sample.SampleId] = MakeMilestone2Material(0.0, TEXT("q1q2_qgamma_oceanic"));
					++Result.Metrics.Q1Q2GapFillCount;
					++Result.Metrics.Q1Q2BoundaryPairCount;
					++Result.Metrics.Q1Q2DifferentPlatePairCount;
					++Result.Metrics.QGammaComputedCount;
					++Result.Metrics.GeneratedOceanicCount;
					Result.SampleRecords.Add(Record);
					continue;
				}

				TArray<int32, TInlineAllocator<8>> UniquePlateIds;
				bool bAllBoundary = true;
				for (const FCarrierV2Stage1ProjectionHit& Hit : Hits)
				{
					UniquePlateIds.AddUnique(Hit.PlateId);
					bAllBoundary = bAllBoundary && Hit.bBoundaryDegenerate;
				}

				if (UniquePlateIds.Num() > 1)
				{
					AccumulateStage1OverlapAttribution(Hits, OverlapPairCounts);
					AccumulateDeferredOverlapAccounting(Sample, Hits, Result.Metrics);
					if (bAllBoundary)
					{
						Record.bBoundaryOnlyOverlap = true;
						Record.SelectionProvenance = TEXT("cross_plate_boundary_overlap_blocked");
						++Result.Metrics.BoundaryOnlyOverlapCount;
						++Result.Metrics.CrossPlateBoundaryOnlyOverlapCount;
					}
					else
					{
						Record.bNondegenerateOverlapBlocked = true;
						Record.SelectionProvenance = TEXT("nondegenerate_overlap_blocked_until_process_state");
						++Result.Metrics.NondegenerateOverlapBlockedCount;
					}
					Result.SampleRecords.Add(Record);
					continue;
				}

				const FCarrierV2Stage1ProjectionHit& SelectedHit = Hits[0];
				Record.ValidSingleHitCount = 1;
				Record.bSingleHitWritten = true;
				Record.SelectedPlateId = SelectedHit.PlateId;
				Record.AssignedPlateId = SelectedHit.PlateId;
				Record.SelectedLocalTriangleId = SelectedHit.LocalTriangleId;
				Record.SelectedSourceTriangleId = SelectedHit.SourceTriangleId;
				Record.SelectedContinentalFraction = SelectedHit.ContinentalFraction;
				Record.SelectionProvenance = Hits.Num() == 1
					? TEXT("single_valid_plate_intersection_barycentric")
					: TEXT("single_plate_boundary_degenerate_barycentric");

				OutSamplePlateAssignments[Sample.SampleId] = Record.AssignedPlateId;
				OutSampleMaterials[Sample.SampleId] = MakeMilestone2Material(Record.SelectedContinentalFraction, TEXT("barycentric_resample"));
				++Result.Metrics.ValidSingleHitWriteCount;
				++Result.Metrics.MaterialInterpolationCount;
				if (Hits.Num() > 1)
				{
					++Result.Metrics.BoundaryOnlyOverlapCount;
					++Result.Metrics.SamePlateBoundaryOnlyMultihitCount;
				}
				Result.SampleRecords.Add(Record);
			}

			Result.Metrics.AabbRayQueryCount += QueryMetrics.RayQueryCount;
			Result.Metrics.TopMissPlatePairs = FormatStage1TopPlatePairs(MissPairCounts);
			Result.Metrics.TopOverlapPlatePairs = FormatStage1TopPlatePairs(OverlapPairCounts);
			Result.Metrics.ResampleMs += (FPlatformTime::Seconds() - SamplingStart) * 1000.0;
		}

		void FinalizeMilestone2Verdict(const FCarrierV2Milestone2Config& Config, FCarrierV2Milestone2Metrics& Metrics)
		{
			Metrics.bSingleHitTransferPass =
				(!Config.bRequireSingleHitWrites ||
					Metrics.ValidSingleHitWriteCount >= FMath::Max(1, Config.ExpectedMinimumSingleHitWrites));
			Metrics.bDivergentGapFillPass =
				Metrics.GapFillNoBoundaryPairCount == 0 &&
				Metrics.Q1Q2GapFillCount == Metrics.DivergentZeroHitCount &&
				Metrics.GeneratedOceanicCount == Metrics.DivergentZeroHitCount &&
				Metrics.Q1Q2BoundaryPairCount == Metrics.DivergentZeroHitCount &&
				Metrics.Q1Q2DifferentPlatePairCount == Metrics.DivergentZeroHitCount &&
				Metrics.QGammaComputedCount == Metrics.DivergentZeroHitCount &&
				(!Config.bRequireDivergentGapFill ||
					Metrics.Q1Q2GapFillCount >= FMath::Max(1, Config.ExpectedMinimumGapFill));
			Metrics.bOverlapPolicyPass =
				Metrics.UnsupportedOverlapWriteAttemptCount == 0 &&
				(!Config.bRequireOverlapBlocked ||
					Metrics.NondegenerateOverlapBlockedCount >= FMath::Max(1, Config.ExpectedMinimumOverlapBlocked));
			Metrics.bTopologyRebuildPass =
				Metrics.TopologyRebuildCount >= FMath::Max(1, Config.LifecycleWindowCount) &&
				!Metrics.RebuiltTopologyHash.IsEmpty() &&
				(!Config.bRequireFullTopologyRebuild ||
					(Metrics.RebuiltLocalTriangleCountSum == Metrics.GlobalTriangleCount &&
						Metrics.UnassignedTriangleCount == 0));
			const int32 DeferredTopologySampleCount =
				Metrics.NondegenerateOverlapBlockedCount +
				Metrics.CrossPlateBoundaryOnlyOverlapCount +
				Metrics.GapFillNoBoundaryPairCount;
			Metrics.UnassignedTriangleBudget = Config.bRequireFullTopologyRebuild
				? 0
				: 6 * DeferredTopologySampleCount;
			Metrics.bUnassignedTriangleBudgetPass = Config.bRequireFullTopologyRebuild
				? Metrics.UnassignedTriangleCount == 0
				: (Metrics.UnassignedTriangleBudget == 0 ||
					Metrics.UnassignedTriangleCount <= Metrics.UnassignedTriangleBudget);
			Metrics.bLifecycleConservationPass =
				(!Config.bRequireMaterialConservation ||
					Metrics.MaterialConservationDelta <= Config.MaterialConservationTolerance) &&
				(!Config.bRequireSharpnessPreservation ||
					Metrics.TotalVariationDelta <= Config.TotalVariationTolerance);
			Metrics.bNoForbiddenFallbackPass =
				Metrics.PriorOwnerReadCount == 0 &&
				Metrics.PriorOwnerFallbackCount == 0 &&
				Metrics.GlobalOwnerReadCount == 0 &&
				Metrics.CentroidPrimaryResolutionCount == 0 &&
				Metrics.RandomPrimaryResolutionCount == 0 &&
				Metrics.NearestPrimaryResolutionCount == 0 &&
				Metrics.OwnershipRepairDuringResampleCount == 0 &&
				Metrics.RetentionHysteresisAnchorCount == 0 &&
				Metrics.Q1Q2DiscreteApproxCount == 0 &&
				Metrics.Q1Q2PriorOwnerLookupCount == 0 &&
				Metrics.TerrainBeautyMutationCount == 0 &&
				Metrics.SubductionMutationCount == 0 &&
				Metrics.CollisionMutationCount == 0 &&
				Metrics.RiftingMutationCount == 0;
			Metrics.bPerformanceBudgetPass =
				Config.ExpectedMaxStepKernelMs <= 0.0 ||
				Metrics.StepKernelMs <= Config.ExpectedMaxStepKernelMs;
			Metrics.PaperResampleCycleBudgetMs = Milestone2PaperResampleBudgetMs(Metrics);
			Metrics.bPaperResampleCycleBudgetPass =
				Metrics.PaperResampleCycleBudgetMs <= 0.0 ||
				Metrics.FullCarrierCycleMs <= Metrics.PaperResampleCycleBudgetMs;

			Metrics.bFixturePass =
				Metrics.bBroadphaseMarginGatePass &&
				Metrics.BroadphaseEquivalenceMismatchCount == 0 &&
				Metrics.bSingleHitTransferPass &&
				Metrics.bDivergentGapFillPass &&
				Metrics.bOverlapPolicyPass &&
				Metrics.bTopologyRebuildPass &&
				Metrics.bUnassignedTriangleBudgetPass &&
				Metrics.bLifecycleConservationPass &&
				Metrics.bNoForbiddenFallbackPass &&
				Metrics.bPerformanceBudgetPass &&
				Metrics.bPaperResampleCycleBudgetPass;
			Metrics.bStageGatePass = Metrics.bFixturePass;

			if (!Metrics.bBroadphaseMarginGatePass)
			{
				Metrics.Verdict = TEXT("FAIL_BROADPHASE_MARGIN");
			}
			else if (Metrics.BroadphaseEquivalenceMismatchCount != 0)
			{
				Metrics.Verdict = TEXT("FAIL_BROADPHASE_EQUIVALENCE");
			}
			else if (!Metrics.bNoForbiddenFallbackPass)
			{
				Metrics.Verdict = TEXT("FAIL_FORBIDDEN_FALLBACK");
			}
			else if (!Metrics.bOverlapPolicyPass)
			{
				Metrics.Verdict = TEXT("FAIL_OVERLAP_POLICY");
			}
			else if (!Metrics.bDivergentGapFillPass)
			{
				Metrics.Verdict = TEXT("FAIL_Q1Q2_GAP_FILL");
			}
			else if (!Metrics.bTopologyRebuildPass)
			{
				Metrics.Verdict = TEXT("FAIL_TOPOLOGY_REBUILD");
			}
			else if (!Metrics.bUnassignedTriangleBudgetPass)
			{
				Metrics.Verdict = TEXT("FAIL_UNASSIGNED_TRIANGLE_BUDGET");
			}
			else if (!Metrics.bLifecycleConservationPass)
			{
				Metrics.Verdict = TEXT("FAIL_LIFECYCLE_CONSERVATION");
			}
			else if (!Metrics.bPerformanceBudgetPass)
			{
				Metrics.Verdict = TEXT("FAIL_PERFORMANCE_BUDGET");
			}
			else if (!Metrics.bPaperResampleCycleBudgetPass)
			{
				Metrics.Verdict = TEXT("FAIL_PAPER_RESAMPLE_CYCLE_BUDGET");
			}
			else if (!Metrics.bSingleHitTransferPass)
			{
				Metrics.Verdict = TEXT("FAIL_SINGLE_HIT_TRANSFER");
			}
			else
			{
				Metrics.Verdict = TEXT("PASS");
			}
		}

		bool ApplyMilestone2Window(
			const FCarrierV2Milestone2Config& Config,
			FCarrierV2BuildState& State,
			FCarrierV2Milestone2FixtureResult& Result)
		{
			TArray<int32> PriorOwnerLabels;
			if (Config.bInjectPriorOwnerLabelsForNegativeControl)
			{
				PriorOwnerLabels.Init(0, State.Samples.Num());
			}

			TArray<TUniquePtr<FCarrierV2Stage1PlateQueryRuntime>> QueryRuntimes;
			FCarrierV2Stage1Metrics QueryBuildMetrics;
			FString BuildError;
			if (!BuildStage1PlateQueryRuntimes(State, QueryRuntimes, QueryBuildMetrics, BuildError))
			{
				Result.Error = BuildError;
				Result.Metrics.Verdict = TEXT("FAIL_PLATE_AABB_BUILD");
				return false;
			}
			Result.Metrics.AabbBuildMs += QueryBuildMetrics.PlateAabbBuildMs;

			FCarrierV2Stage1Metrics MotionMetrics;
			ApplyStage1MotionAndMeasure(Config.MotionConfig, State, MotionMetrics);
			Result.Metrics.MotionApplyMs += MotionMetrics.MotionApplyMs;
			UpdateStage1MovedBroadphaseCaps(Config.MotionConfig, State, QueryRuntimes);

			TArray<FCarrierV2Stage5BoundaryEdge> BoundaryEdges;
			BuildMilestone2BoundaryEdges(State, BoundaryEdges, Result.Metrics);
			TArray<FCarrierV2Milestone2BoundarySearchEdge> BoundarySearchEdges;
			BuildMilestone2BoundarySearchEdges(BoundaryEdges, BoundarySearchEdges);

			TArray<int32> SampleAssignments;
			TArray<FCarrierV2MaterialRecord> SampleMaterials;
			SampleMilestone2GlobalTds(Config, State, QueryRuntimes, BoundarySearchEdges, SampleAssignments, SampleMaterials, Result);
			RebuildMilestone2PlateLocalTopology(Config, State, SampleAssignments, SampleMaterials, Result);
			State.Plates = Result.RebuiltPlates;
			++Result.Metrics.RemeshWindowCount;
			return true;
		}

		bool RunMilestone2FixtureOnce(const FCarrierV2Milestone2Config& Config, FCarrierV2Milestone2FixtureResult& OutResult)
		{
			OutResult = FCarrierV2Milestone2FixtureResult();
			OutResult.Config = Config;
			InitializeMilestone2Metrics(Config, OutResult.Metrics);
			const double TotalStart = FPlatformTime::Seconds();

			FCarrierV2BuildState State;
			FCarrierV2Stage0Metrics BuildMetrics;
			if (!BuildStateForConfig(Config.MotionConfig.BaseConfig, State, BuildMetrics, OutResult.Error))
			{
				OutResult.Metrics.Verdict = TEXT("FAIL_BUILD_STATE");
				OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
				return false;
			}
			OutResult.Metrics.BuildSubstrateMs = BuildMetrics.BuildSubstrateMs;
			OutResult.Metrics.BuildPlateLocalMs = BuildMetrics.BuildPlateLocalMs;
			CopyMilestone2TopologyMetrics(State, OutResult.Metrics);
			OutResult.Metrics.PreCycleAuthorityHash = HashToString(HashStage1Authority(State, Config.MotionConfig));

			double MaxMotionAngle = 0.0;
			for (int32 PlateId = 0; PlateId < Config.MotionConfig.BaseConfig.PlateCount; ++PlateId)
			{
				MaxMotionAngle = FMath::Max(MaxMotionAngle, FMath::Abs(Stage1TotalMotionAngle(Config.MotionConfig, PlateId)));
			}
			OutResult.Metrics.MaxPlateMotionAngleRad = MaxMotionAngle;
			OutResult.Metrics.bBroadphaseMarginGatePass =
				!Config.MotionConfig.bUseAngularCapPlateBroadphase ||
				(Config.MotionConfig.BroadphaseAngularMarginRad > 0.0 &&
					Config.MotionConfig.BroadphaseAngularMarginRad >= 10.0 * MaxMotionAngle);

			TArray<FCarrierV2MaterialRecord> BeforeMaterials;
			BuildMilestone2ExpectedSampleMaterials(State, BeforeMaterials);
			OutResult.Metrics.MaterialMassBefore = ComputeMilestone2SampleMass(State, BeforeMaterials);
			OutResult.Metrics.TotalVariationBefore = ComputeMilestone2SampleTotalVariation(State, BeforeMaterials);

			const int32 WindowCount = FMath::Max(1, Config.LifecycleWindowCount);
			for (int32 WindowIndex = 0; WindowIndex < WindowCount; ++WindowIndex)
			{
				if (!ApplyMilestone2Window(Config, State, OutResult))
				{
					OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
					return false;
				}
			}

			TArray<FCarrierV2MaterialRecord> AfterMaterials;
			BuildMilestone2ExpectedSampleMaterials(State, AfterMaterials);
			for (const FCarrierV2Plate& Plate : OutResult.RebuiltPlates)
			{
				for (const FCarrierV2PlateVertex& Vertex : Plate.LocalVertices)
				{
					if (AfterMaterials.IsValidIndex(Vertex.SourceSampleId))
					{
						AfterMaterials[Vertex.SourceSampleId] = Vertex.Material;
					}
				}
			}
			OutResult.Metrics.MaterialMassAfter = ComputeMilestone2SampleMass(State, AfterMaterials);
			OutResult.Metrics.MaterialConservationDelta = FMath::Abs(OutResult.Metrics.MaterialMassAfter - OutResult.Metrics.MaterialMassBefore);
			OutResult.Metrics.TotalVariationAfter = ComputeMilestone2SampleTotalVariation(State, AfterMaterials);
			OutResult.Metrics.TotalVariationDelta = FMath::Abs(OutResult.Metrics.TotalVariationAfter - OutResult.Metrics.TotalVariationBefore);

			OutResult.Metrics.PostCycleAuthorityHash = HashToString(HashStage1Authority(State, Config.MotionConfig));
			OutResult.Metrics.ResampleOutputHash = HashToString(HashMilestone2SamplingRecords(Config, OutResult.Metrics, OutResult.SampleRecords));
			OutResult.Metrics.StepKernelMs =
				OutResult.Metrics.MotionApplyMs +
				OutResult.Metrics.ResampleMs +
				OutResult.Metrics.TopologyRebuildMs;
			OutResult.Metrics.FullCarrierCycleMs =
				OutResult.Metrics.AabbBuildMs +
				OutResult.Metrics.StepKernelMs;
			FinalizeMilestone2Verdict(Config, OutResult.Metrics);
			OutResult.Metrics.MetricsHash = HashToString(HashMilestone2Metrics(OutResult.Metrics, true));
			OutResult.Metrics.PeakMemoryMb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);
			OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
			OutResult.bCompleted = true;
			return true;
		}
	}

	bool FCarrierV2FoundationStepper::BuildSnapshot(
		const FCarrierV2Stage5Config& Config,
		FCarrierV2FoundationStepperSnapshot& OutSnapshot)
	{
		OutSnapshot = FCarrierV2FoundationStepperSnapshot();
		OutSnapshot.Config = Config;

		const bool bRunOk = FCarrierV2Stage5::RunFixtureWithReplay(Config, OutSnapshot.Stage5Result);
		if (!bRunOk || !OutSnapshot.Stage5Result.bCompleted)
		{
			OutSnapshot.Error = OutSnapshot.Stage5Result.Error.IsEmpty()
				? TEXT("Stage 5 fixture did not complete for foundation stepper snapshot.")
				: OutSnapshot.Stage5Result.Error;
			OutSnapshot.Summary = FString::Printf(
				TEXT("fixture=%s completed=false verdict=%s error=%s"),
				*Config.FixtureId,
				*OutSnapshot.Stage5Result.Metrics.Verdict,
				*OutSnapshot.Error);
			return false;
		}

		FCarrierV2BuildState State;
		FCarrierV2Stage0Metrics BuildMetrics;
		FString BuildError;
		if (!BuildStateForConfig(
			Config.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig.BaseConfig,
			State,
			BuildMetrics,
			BuildError))
		{
			OutSnapshot.Error = BuildError;
			OutSnapshot.Summary = FString::Printf(
				TEXT("fixture=%s completed=false verdict=FAIL_SNAPSHOT_BUILD error=%s"),
				*Config.FixtureId,
				*OutSnapshot.Error);
			return false;
		}

		FCarrierV2Stage1Metrics MotionMetrics;
		ApplyStage1MotionAndMeasure(
			Config.SamplingConfig.ProcessConfig.ContactConfig.MotionConfig,
			State,
			MotionMetrics);

		const int32 SampleCount = State.Samples.Num();
		const int32 PlateCount = FMath::Max(0, State.Config.PlateCount);
		TArray<TArray<int32>> SampleColdPlateCounts;
		SampleColdPlateCounts.SetNum(SampleCount);
		for (TArray<int32>& PlateCounts : SampleColdPlateCounts)
		{
			PlateCounts.Init(0, PlateCount);
		}

		for (const FCarrierV2SubstrateTriangle& Triangle : State.Triangles)
		{
			const int32 PlateId = State.TrianglePlateIds.IsValidIndex(Triangle.TriangleId)
				? State.TrianglePlateIds[Triangle.TriangleId]
				: INDEX_NONE;
			if (!SampleColdPlateCounts.IsValidIndex(Triangle.SampleIds[0]) ||
				!SampleColdPlateCounts.IsValidIndex(Triangle.SampleIds[1]) ||
				!SampleColdPlateCounts.IsValidIndex(Triangle.SampleIds[2]) ||
				PlateId == INDEX_NONE ||
				PlateId >= PlateCount)
			{
				continue;
			}

			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				++SampleColdPlateCounts[Triangle.SampleIds[Corner]][PlateId];
			}
		}

		auto ChooseColdPlate = [](const TArray<int32>& PlateCounts) -> int32
		{
			int32 BestPlateId = INDEX_NONE;
			int32 BestCount = 0;
			for (int32 PlateId = 0; PlateId < PlateCounts.Num(); ++PlateId)
			{
				const int32 Count = PlateCounts[PlateId];
				if (Count > BestCount || (Count == BestCount && Count > 0 && (BestPlateId == INDEX_NONE || PlateId < BestPlateId)))
				{
					BestPlateId = PlateId;
					BestCount = Count;
				}
			}
			return BestPlateId;
		};

		TMap<int32, FCarrierV2Stage5SampleRecord> SampleRecordsById;
		SampleRecordsById.Reserve(OutSnapshot.Stage5Result.SampleRecords.Num());
		for (const FCarrierV2Stage5SampleRecord& Record : OutSnapshot.Stage5Result.SampleRecords)
		{
			SampleRecordsById.Add(Record.SampleId, Record);
		}

		TMap<uint64, FCarrierV2Stage4MarkFlags> MarkLookup;
		BuildStage4MarkLookup(OutSnapshot.Stage5Result.Stage3Result.TriangleMarks, MarkLookup);

		TMap<int32, FCarrierV2Stage4MarkFlags> SourceTriangleMarks;
		TSet<int32> MarkedSampleIds;
		for (const FCarrierV2TriangleProcessMarkRecord& Mark : OutSnapshot.Stage5Result.Stage3Result.TriangleMarks)
		{
			if (Mark.SampleId != INDEX_NONE)
			{
				MarkedSampleIds.Add(Mark.SampleId);
			}
		}
		for (const FCarrierV2Plate& Plate : State.Plates)
		{
			for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
			{
				const FCarrierV2Stage4MarkFlags* Flags = MarkLookup.Find(Stage4TriangleKey(Plate.PlateId, Triangle.LocalTriangleId));
				if (Flags == nullptr)
				{
					continue;
				}
				FCarrierV2Stage4MarkFlags& SourceFlags = SourceTriangleMarks.FindOrAdd(Triangle.SourceTriangleId);
				SourceFlags.bSubducting = SourceFlags.bSubducting || Flags->bSubducting;
				SourceFlags.bColliding = SourceFlags.bColliding || Flags->bColliding;
			}
		}

		TMap<int32, int32> RebuiltPlateBySourceTriangle;
		for (const FCarrierV2Plate& Plate : OutSnapshot.Stage5Result.RebuiltPlates)
		{
			for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
			{
				RebuiltPlateBySourceTriangle.Add(Triangle.SourceTriangleId, Plate.PlateId);
			}
		}

		OutSnapshot.Samples.Reserve(State.Samples.Num());
		for (const FCarrierV2SubstrateSample& Sample : State.Samples)
		{
			FCarrierV2FoundationStepperSampleVisual Visual;
			Visual.SampleId = Sample.SampleId;
			Visual.UnitPosition = Sample.UnitPosition;
			Visual.ColdStartPlateId = SampleColdPlateCounts.IsValidIndex(Sample.SampleId)
				? ChooseColdPlate(SampleColdPlateCounts[Sample.SampleId])
				: INDEX_NONE;
			Visual.bProcessMarked = MarkedSampleIds.Contains(Sample.SampleId);

			if (const FCarrierV2Stage5SampleRecord* Record = SampleRecordsById.Find(Sample.SampleId))
			{
				Visual.AssignedPlateId = Record->AssignedPlateId;
				Visual.RawHitCount = Record->RawHitCount;
				Visual.FilteredSubductingHitCount = Record->FilteredSubductingHitCount;
				Visual.FilteredCollidingHitCount = Record->FilteredCollidingHitCount;
				Visual.ValidHitCount = Record->ValidHitCount;
				Visual.ContinentalFraction = Record->SelectedContinentalFraction;
				Visual.bZeroValidHit = Record->bZeroValidHit;
				Visual.bGeneratedOceanic = Record->bGeneratedOceanic;
				Visual.bBoundaryPairFound = Record->bBoundaryPairFound;
				Visual.bQ1Q2DifferentPlates = Record->bQ1Q2DifferentPlates;
				Visual.SelectionProvenance = Record->SelectionProvenance;
			}
			else
			{
				Visual.ContinentalFraction = ExpectedMaterialForSample(Sample.SampleId, State.Config.Seed).ContinentalFraction;
				Visual.SelectionProvenance = TEXT("missing_stage5_record");
			}
			OutSnapshot.Samples.Add(Visual);
		}

		OutSnapshot.Triangles.Reserve(State.Triangles.Num());
		for (const FCarrierV2SubstrateTriangle& Triangle : State.Triangles)
		{
			FCarrierV2FoundationStepperTriangleVisual Visual;
			Visual.TriangleId = Triangle.TriangleId;
			Visual.SampleIds[0] = Triangle.SampleIds[0];
			Visual.SampleIds[1] = Triangle.SampleIds[1];
			Visual.SampleIds[2] = Triangle.SampleIds[2];
			Visual.ColdStartPlateId = State.TrianglePlateIds.IsValidIndex(Triangle.TriangleId)
				? State.TrianglePlateIds[Triangle.TriangleId]
				: INDEX_NONE;
			Visual.RebuiltPlateId = RebuiltPlateBySourceTriangle.Contains(Triangle.TriangleId)
				? RebuiltPlateBySourceTriangle[Triangle.TriangleId]
				: INDEX_NONE;
			Visual.bUnassignedAfterRebuild = Visual.RebuiltPlateId == INDEX_NONE;

			if (const FCarrierV2Stage4MarkFlags* Flags = SourceTriangleMarks.Find(Triangle.TriangleId))
			{
				Visual.bSubductingProcessMarked = Flags->bSubducting;
				Visual.bCollidingProcessMarked = Flags->bColliding;
				Visual.bProcessMarked = Flags->bSubducting || Flags->bColliding;
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					if (OutSnapshot.Samples.IsValidIndex(Triangle.SampleIds[Corner]))
					{
						OutSnapshot.Samples[Triangle.SampleIds[Corner]].bProcessMarked = true;
						OutSnapshot.Samples[Triangle.SampleIds[Corner]].bSubductingProcessMarked |= Flags->bSubducting;
						OutSnapshot.Samples[Triangle.SampleIds[Corner]].bCollidingProcessMarked |= Flags->bColliding;
					}
				}
			}

			const FCarrierV2Stage5SampleRecord* A = SampleRecordsById.Find(Triangle.SampleIds[0]);
			const FCarrierV2Stage5SampleRecord* B = SampleRecordsById.Find(Triangle.SampleIds[1]);
			const FCarrierV2Stage5SampleRecord* C = SampleRecordsById.Find(Triangle.SampleIds[2]);
			if (A != nullptr && B != nullptr && C != nullptr)
			{
				Visual.bMixedVertexAssignment =
					A->AssignedPlateId != INDEX_NONE &&
					B->AssignedPlateId != INDEX_NONE &&
					C->AssignedPlateId != INDEX_NONE &&
					(A->AssignedPlateId != B->AssignedPlateId ||
						B->AssignedPlateId != C->AssignedPlateId);
			}

			OutSnapshot.Triangles.Add(Visual);
		}

		const FCarrierV2Stage5Metrics& M = OutSnapshot.Stage5Result.Metrics;
		OutSnapshot.bCompleted = true;
		OutSnapshot.bFixturePass = M.bFixturePass;
		OutSnapshot.bReplayDeterministic = M.bReplayDeterministic;
		OutSnapshot.bForbiddenFallbackDetected =
			M.PriorOwnerFallbackCount != 0 ||
			M.CentroidPrimaryResolutionCount != 0 ||
			M.RandomPrimaryResolutionCount != 0 ||
			M.NearestPrimaryResolutionCount != 0 ||
			M.OwnershipRepairDuringRemeshCount != 0 ||
			M.RetentionHysteresisAnchorCount != 0 ||
			M.Q1Q2DiscreteApproxCount != 0 ||
			M.Q1Q2PriorOwnerLookupCount != 0 ||
			M.MaterialCreatedWithoutGapFillCount != 0 ||
			M.MaterialDestroyedWithoutProcessCount != 0 ||
			M.TerrainBeautyMutationCount != 0;
		OutSnapshot.Summary = FString::Printf(
			TEXT("fixture=%s verdict=%s pass=%s replay=%s samples=%d triangles=%d contacts=%d process_events=%d pre_marks=%d post_marks=%d zero_valid=%d generated=%d rebuilt=%d/%d forbidden=%s"),
			*M.FixtureId,
			*M.Verdict,
			M.bFixturePass ? TEXT("true") : TEXT("false"),
			M.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			M.GlobalSampleCount,
			M.GlobalTriangleCount,
			OutSnapshot.Stage5Result.Stage3Result.Stage2Result.Metrics.ContactCandidateCount,
			OutSnapshot.Stage5Result.Stage3Result.Metrics.ProcessEventCount,
			M.PreResetTriangleMarkCount,
			M.PostResetTriangleMarkCount,
			M.ZeroValidHitCount,
			M.GeneratedOceanicCount,
			M.RebuiltTriangleAssignmentCount,
			M.GlobalTriangleCount,
			OutSnapshot.bForbiddenFallbackDetected ? TEXT("true") : TEXT("false"));
		return OutSnapshot.bCompleted;
	}

	TArray<FCarrierV2Stage0Config> FCarrierV2Stage0::MakeMicroFixtureConfigs()
	{
		TArray<FCarrierV2Stage0Config> Configs;

		FCarrierV2Stage0Config FX000;
		FX000.FixtureId = TEXT("FX-000");
		FX000.FixtureName = TEXT("SinglePlateIdentity");
		FX000.SampleCount = 6;
		FX000.PlateCount = 1;
		FX000.FixtureKind = ECarrierV2FixtureKind::Positive;
		FX000.ExpectedFailureReason = TEXT("none");
		Configs.Add(FX000);

		FCarrierV2Stage0Config FX001;
		FX001.FixtureId = TEXT("FX-001");
		FX001.FixtureName = TEXT("TwoPlateCleanPartition");
		FX001.SampleCount = 6;
		FX001.PlateCount = 2;
		FX001.FixtureKind = ECarrierV2FixtureKind::Positive;
		FX001.ExpectedFailureReason = TEXT("none");
		Configs.Add(FX001);

		FCarrierV2Stage0Config FX002;
		FX002.FixtureId = TEXT("FX-002");
		FX002.FixtureName = TEXT("BoundaryDegeneracyPin");
		FX002.SampleCount = 6;
		FX002.PlateCount = 2;
		FX002.FixtureKind = ECarrierV2FixtureKind::Positive;
		FX002.ExpectedFailureReason = TEXT("none");
		Configs.Add(FX002);

		FCarrierV2Stage0Config FX003;
		FX003.FixtureId = TEXT("FX-003");
		FX003.FixtureName = TEXT("DeliberateTopologyHole");
		FX003.SampleCount = 6;
		FX003.PlateCount = 2;
		FX003.FixtureKind = ECarrierV2FixtureKind::Negative;
		FX003.bDeliberateTopologyHole = true;
		FX003.ExpectedFailureReason = TEXT("topology_hole");
		Configs.Add(FX003);

		FCarrierV2Stage0Config FX004;
		FX004.FixtureId = TEXT("FX-004");
		FX004.FixtureName = TEXT("DeliberateDuplicatedOverlap");
		FX004.SampleCount = 6;
		FX004.PlateCount = 2;
		FX004.FixtureKind = ECarrierV2FixtureKind::Negative;
		FX004.bDeliberateDuplicatedOverlap = true;
		FX004.ExpectedFailureReason = TEXT("topology_duplicate");
		Configs.Add(FX004);

		return Configs;
	}

	FCarrierV2Stage0Config FCarrierV2Stage0::MakeScaleConfig(const int32 SampleCount, const bool bComparisonScale)
	{
		FCarrierV2Stage0Config Config;
		Config.FixtureId = bComparisonScale ? TEXT("SCALE-250K") : TEXT("SCALE-50K");
		Config.FixtureName = bComparisonScale ? TEXT("Scale250kCharacterization") : TEXT("Scale50kGate");
		Config.FixtureKind = ECarrierV2FixtureKind::Scale;
		Config.FixtureSubstrateId = TEXT("fibonacci_spherical_delaunay");
		Config.PartitionPolicyId = TEXT("deterministic_centroid_nearest_plate_seed_partition");
		Config.SampleCount = SampleCount;
		Config.PlateCount = 40;
		Config.Seed = 42;
		Config.bUseFibonacciSubstrate = true;
		Config.ExpectedFailureReason = TEXT("none");
		return Config;
	}

	bool FCarrierV2Stage0::RunFixtureWithReplay(const FCarrierV2Stage0Config& Config, FCarrierV2FixtureResult& OutResult)
	{
		FCarrierV2FixtureResult Replay;
		const bool bPrimaryOk = RunFixtureOnce(Config, OutResult);
		const bool bReplayOk = RunFixtureOnce(Config, Replay);

		OutResult.Metrics.ReplayCarrierHash = Replay.Metrics.CarrierHash;
		OutResult.Metrics.ReplayProjectionHash = Replay.Metrics.ProjectionHash;
		OutResult.Metrics.ReplayMetricsHash = Replay.Metrics.MetricsHash;
		OutResult.Metrics.bReplayDeterministic =
			bPrimaryOk == bReplayOk &&
			OutResult.Metrics.CarrierHash == Replay.Metrics.CarrierHash &&
			OutResult.Metrics.ProjectionHash == Replay.Metrics.ProjectionHash &&
			OutResult.Metrics.ObservedFailureReason == Replay.Metrics.ObservedFailureReason;

		OutResult.Metrics.bFixturePass = OutResult.Metrics.bFixturePass && OutResult.Metrics.bReplayDeterministic;
		OutResult.Metrics.bStageGatePass = OutResult.Metrics.bFixturePass;
		if (!OutResult.Metrics.bReplayDeterministic)
		{
			OutResult.Metrics.Verdict = TEXT("FAIL_REPLAY_DETERMINISM");
			if (OutResult.Error.IsEmpty())
			{
				OutResult.Error = TEXT("Replay A/B hashes or observed reasons differ.");
			}
		}
		OutResult.Metrics.ProjectionHash = HashToString(HashProjectionMetrics(OutResult.Metrics, false));
		OutResult.Metrics.MetricsHash.Reset();
		OutResult.Metrics.MetricsHash = HashToString(HashProjectionMetrics(OutResult.Metrics, true));
		return bPrimaryOk && bReplayOk && OutResult.Metrics.bFixturePass;
	}

	FString FCarrierV2Stage0::MetricsToJson(const FCarrierV2FixtureResult& Result)
	{
		const FCarrierV2Stage0Metrics& M = Result.Metrics;
		return FString::Printf(
			TEXT("{")
			TEXT("\"run_id\":%s,")
			TEXT("\"stage_id\":%s,")
			TEXT("\"fixture_id\":%s,")
			TEXT("\"fixture_name\":%s,")
			TEXT("\"fixture_kind\":%s,")
			TEXT("\"sample_count\":%d,")
			TEXT("\"triangle_count\":%d,")
			TEXT("\"plate_count\":%d,")
			TEXT("\"ray_epsilon\":%.12g,")
			TEXT("\"partition_policy_id\":%s,")
			TEXT("\"fixture_substrate_id\":%s,")
			TEXT("\"input_hash\":%s,")
			TEXT("\"carrier_hash\":%s,")
			TEXT("\"projection_hash\":%s,")
			TEXT("\"metrics_hash\":%s,")
			TEXT("\"global_sample_count\":%d,")
			TEXT("\"global_triangle_count\":%d,")
			TEXT("\"local_plate_vertex_count_sum\":%d,")
			TEXT("\"local_plate_triangle_count_sum\":%d,")
			TEXT("\"topology_duplicate_error_count\":%d,")
			TEXT("\"topology_hole_error_count\":%d,")
			TEXT("\"raw_hit_count_total\":%lld,")
			TEXT("\"ray_triangle_test_count\":%lld,")
			TEXT("\"nondegenerate_miss_count\":%d,")
			TEXT("\"nondegenerate_overlap_count\":%d,")
			TEXT("\"boundary_degenerate_count\":%d,")
			TEXT("\"boundary_policy_selected_count\":%d,")
			TEXT("\"projection_reads_global_owner_count\":%d,")
			TEXT("\"material_authority_projected_delta\":%.12g,")
			TEXT("\"expected_failure_reason\":%s,")
			TEXT("\"observed_failure_reason\":%s,")
			TEXT("\"fixture_pass\":%s,")
			TEXT("\"stage_gate_pass\":%s,")
			TEXT("\"verdict\":%s,")
			TEXT("\"build_substrate_ms\":%.3f,")
			TEXT("\"build_plate_local_ms\":%.3f,")
			TEXT("\"projection_kernel_ms\":%.3f,")
			TEXT("\"metrics_ms\":%.3f,")
			TEXT("\"report_ms\":%.3f,")
			TEXT("\"total_ms\":%.3f,")
			TEXT("\"peak_memory_mb\":%.3f,")
			TEXT("\"replay_deterministic\":%s,")
			TEXT("\"replay_carrier_hash\":%s,")
			TEXT("\"replay_projection_hash\":%s,")
			TEXT("\"replay_metrics_hash\":%s")
			TEXT("}"),
			*JsonString(M.RunId),
			*JsonString(M.StageId),
			*JsonString(M.FixtureId),
			*JsonString(M.FixtureName),
			*JsonString(M.FixtureKind),
			M.SampleCount,
			M.TriangleCount,
			M.PlateCount,
			M.RayEpsilon,
			*JsonString(M.PartitionPolicyId),
			*JsonString(M.FixtureSubstrateId),
			*JsonString(M.InputHash),
			*JsonString(M.CarrierHash),
			*JsonString(M.ProjectionHash),
			*JsonString(M.MetricsHash),
			M.GlobalSampleCount,
			M.GlobalTriangleCount,
			M.LocalPlateVertexCountSum,
			M.LocalPlateTriangleCountSum,
			M.TopologyDuplicateErrorCount,
			M.TopologyHoleErrorCount,
			M.RawHitCountTotal,
			M.RayTriangleTestCount,
			M.NonDegenerateMissCount,
			M.NonDegenerateOverlapCount,
			M.BoundaryDegenerateCount,
			M.BoundaryPolicySelectedCount,
			M.ProjectionReadsGlobalOwnerCount,
			M.MaterialAuthorityProjectedDelta,
			*JsonString(M.ExpectedFailureReason),
			*JsonString(M.ObservedFailureReason),
			M.bFixturePass ? TEXT("true") : TEXT("false"),
			M.bStageGatePass ? TEXT("true") : TEXT("false"),
			*JsonString(M.Verdict),
			M.BuildSubstrateMs,
			M.BuildPlateLocalMs,
			M.ProjectionKernelMs,
			M.MetricsMs,
			M.ReportMs,
			M.TotalMs,
			M.PeakMemoryMb,
			M.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			*JsonString(M.ReplayCarrierHash),
			*JsonString(M.ReplayProjectionHash),
			*JsonString(M.ReplayMetricsHash));
	}

	FString FCarrierV2Stage0::BuildCheckpointReport(
		const FCarrierV2Stage0SuiteResult& Suite,
		const FString& CommandLine,
		const FString& CommitSha)
	{
		const double Start = FPlatformTime::Seconds();
		FString Report;
		Report += TEXT("# CarrierLab V2 Stage 0 Report\n\n");
		Report += TEXT("Status: generated by `CarrierLabV2Stage0`.\n\n");
		Report += FString::Printf(TEXT("- Commit: `%s`\n"), CommitSha.IsEmpty() ? TEXT("unknown") : *CommitSha);
		Report += FString::Printf(TEXT("- Command: `%s`\n"), *CommandLine);
		Report += FString::Printf(TEXT("- Output root: `%s`\n\n"), *Suite.OutputRoot);

		Report += TEXT("## Scope\n\n");
		Report += TEXT("V2-0 covers cold-start carrier construction, plate-local duplicated geometry, t=0 ray projection, fixture gates, replay determinism, and performance timing. It does not cover motion, remesh, editor UI, maps as gates, subduction, collision, rifting, uplift, erosion, or ownership repair.\n\n");

		Report += TEXT("## Fixture Gates\n\n");
		Report += TEXT("| fixture | kind | samples | triangles | expected | observed | pass | verdict |\n");
		Report += TEXT("|---|---:|---:|---:|---|---|---|---|\n");
		for (const FCarrierV2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage0Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %s | %d | %d | `%s` | `%s` | %s | `%s` |\n"),
				*M.FixtureId,
				*M.FixtureKind,
				M.GlobalSampleCount,
				M.GlobalTriangleCount,
				*M.ExpectedFailureReason,
				*M.ObservedFailureReason,
				M.bFixturePass ? TEXT("pass") : TEXT("fail"),
				*M.Verdict);
		}

		Report += TEXT("\n## Hard Metrics\n\n");
		Report += TEXT("| fixture | global owner reads | topo holes | topo duplicates | nondeg misses | nondeg overlaps | boundary degenerate | material delta |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FCarrierV2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage0Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %d | %d | %d | %d | %.12g |\n"),
				*M.FixtureId,
				M.ProjectionReadsGlobalOwnerCount,
				M.TopologyHoleErrorCount,
				M.TopologyDuplicateErrorCount,
				M.NonDegenerateMissCount,
				M.NonDegenerateOverlapCount,
				M.BoundaryDegenerateCount,
				M.MaterialAuthorityProjectedDelta);
		}

		Report += TEXT("\n## Performance Ladder\n\n");
		Report += TEXT("| fixture | samples | substrate ms | plate-local ms | projection kernel ms | metrics ms | total ms | peak memory mb |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FCarrierV2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage0Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f |\n"),
				*M.FixtureId,
				M.GlobalSampleCount,
				M.BuildSubstrateMs,
				M.BuildPlateLocalMs,
				M.ProjectionKernelMs,
				M.MetricsMs,
				M.TotalMs,
				M.PeakMemoryMb);
		}

		Report += TEXT("\n## Policy Ledger\n\n");
		Report += TEXT("| policy | authority class | behavior | audit evidence |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += TEXT("| `V2-0-POL-BOUNDARY-DEGENERACY-CLASSIFY` | source_silent_lab_policy | exact edge/vertex multi-hits are counted separately | `boundary_degenerate_count`, `boundary_policy_selected_count` |\n");
		Report += TEXT("| `V2-0-POL-RAY-EPSILON` | source_silent_lab_policy | one fixed ray/triangle tolerance | `ray_epsilon` |\n");
		Report += TEXT("| `V2-0-POL-FIXTURE-SUBSTRATES` | diagnostic_only | handcrafted micro substrates are fixture-only | `fixture_substrate_id` |\n");
		Report += TEXT("| `V2-0-POL-NEGATIVE-CONTROL-CORRUPTION` | diagnostic_only | holes/duplicates are injected only in negative controls | expected vs observed failure reason |\n");
		Report += TEXT("| `V2-0-POL-PARTITION-DEFERRED` | source_silent_lab_policy | production-like seeded partition is only a scale characterization input | `partition_policy_id` |\n\n");

		Report += TEXT("## Replay A/B\n\n");
		Report += TEXT("| fixture | carrier hash | replay carrier hash | projection hash | replay projection hash | deterministic |\n");
		Report += TEXT("|---|---|---|---|---|---|\n");
		for (const FCarrierV2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage0Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | `%s` | `%s` | `%s` | %s |\n"),
				*M.FixtureId,
				*M.CarrierHash,
				*M.ReplayCarrierHash,
				*M.ProjectionHash,
				*M.ReplayProjectionHash,
				M.bReplayDeterministic ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Independent Oracle\n\n");
		Report += TEXT("Fixture expectations are computed from fixture config and source sample ids, not from selected projection owner. Positive fixtures require zero global-owner reads, zero topology errors, zero nondegenerate misses/overlaps, and zero material delta. Negative fixtures pass only when the observed failure reason matches the injected corruption.\n\n");

		Report += TEXT("## Known Limitations\n\n");
		Report += TEXT("- Micro fixtures are correctness microscopes, not paper-scale evidence.\n");
		Report += TEXT("- 50k is the minimum scale gate before recommending Stage 1.\n");
		Report += TEXT("- 250k is characterization unless explicitly requested as a hard gate.\n");
		Report += TEXT("- 500k is not attempted by this V2-0 commandlet goal.\n");
		Report += TEXT("- Seeded plate partition is deterministic lab policy, not a claimed paper-complete partition algorithm.\n\n");

		Report += TEXT("## Verdict\n\n");
		Report += FString::Printf(TEXT("Verdict: `%s`\n\n"), *Suite.Verdict);
		Report += FString::Printf(TEXT("- Micro gate: %s\n"), Suite.bMicroGatePass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 50k gate: %s\n"), Suite.bScale50kPass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 250k characterization attempted: %s\n"), Suite.bAttempted250k ? TEXT("yes") : TEXT("no"));
		if (Suite.bAttempted250k)
		{
			Report += FString::Printf(TEXT("- 250k characterization: %s\n"), Suite.bScale250kPass ? TEXT("pass") : TEXT("fail"));
		}
		Report += TEXT("\nExplicit user go/no-go is required before Stage 1.\n");

		const double ReportMs = (FPlatformTime::Seconds() - Start) * 1000.0;
		Report += FString::Printf(TEXT("\nReport generation ms: %.3f\n"), ReportMs);
		return Report;
	}

	uint64 HashMilestone3Config(const FCarrierV2Milestone3Config& Config)
	{
		uint64 Hash = HashMilestone2Config(Config.CarrierCycleConfig);
		HashMixString(Hash, Config.FixtureId);
		HashMixString(Hash, Config.FixtureName);
		HashMixString(Hash, Config.ProcessFilterPolicyId);
		HashMixString(Hash, Config.ContactEvidencePolicyId);
		HashMixString(Hash, Config.PolarityPolicyId);
		HashMixString(Hash, Config.PolarityMode);
		HashMixInt(Hash, Config.bRunPinnedM2BaselinesOnly ? 1 : 0);
		HashMixInt(Hash, Config.bEnableProcessFilters ? 1 : 0);
		HashMixInt(Hash, Config.bAllowFixtureSpecifiedPolarity ? 1 : 0);
		HashMixInt(Hash, Config.bForceAllOverlapHitsFiltered ? 1 : 0);
		HashMixInt(Hash, Config.bForceNoFilterLabels ? 1 : 0);
		HashMixDouble(Hash, Config.OpeningRateTolerance);
		HashMixDouble(Hash, Config.ContinentalFractionThreshold);
		for (const FCarrierV2Milestone3PinnedM2Baseline& Baseline : Config.PinnedM2Baselines)
		{
			HashMixString(Hash, Baseline.FixtureId);
			HashMixString(Hash, Baseline.ExpectedPostCycleAuthorityHash);
			HashMixString(Hash, Baseline.ExpectedResampleOutputHash);
			HashMixString(Hash, Baseline.ExpectedRebuiltTopologyHash);
		}
		return Hash;
	}

	uint64 SourceEdgeKey(const int32 A, const int32 B)
	{
		uint64 Hash = FnvOffset;
		HashMixInt(Hash, FMath::Min(A, B));
		HashMixInt(Hash, FMath::Max(A, B));
		return Hash;
	}

	FVector3d Milestone3TangentialVelocity(
		const FCarrierV2Stage1Config& Config,
		const int32 PlateId,
		const FVector3d& UnitPosition)
	{
		const FCarrierV2MotionSpec Motion = MotionForPlate(Config, PlateId);
		return FVector3d::CrossProduct(Motion.Axis.GetSafeNormal(), UnitPosition.GetSafeNormal()) * Motion.AngularSpeedRadPerMa;
	}

	double Milestone3OpeningRate(
		const FCarrierV2Stage1Config& Config,
		const int32 PlateA,
		const int32 PlateB,
		const FVector3d& PointA,
		const FVector3d& PointB,
		const FVector3d& QGamma)
	{
		FVector3d Direction = PointB.GetSafeNormal() - PointA.GetSafeNormal();
		Direction -= QGamma.GetSafeNormal() * FVector3d::DotProduct(Direction, QGamma.GetSafeNormal());
		if (Direction.SizeSquared() <= 1.0e-18)
		{
			const FCarrierV2MotionSpec MotionA = MotionForPlate(Config, PlateA);
			Direction = FVector3d::CrossProduct(QGamma.GetSafeNormal(), MotionA.Axis.GetSafeNormal());
		}
		if (Direction.SizeSquared() <= 1.0e-18)
		{
			Direction = FVector3d::CrossProduct(QGamma.GetSafeNormal(), FVector3d(0.0, 0.0, 1.0));
		}
		if (Direction.SizeSquared() <= 1.0e-18)
		{
			Direction = FVector3d::CrossProduct(QGamma.GetSafeNormal(), FVector3d(1.0, 0.0, 0.0));
		}
		Direction.Normalize();

		const FVector3d RelativeVelocity =
			Milestone3TangentialVelocity(Config, PlateB, QGamma) -
			Milestone3TangentialVelocity(Config, PlateA, QGamma);
		return FVector3d::DotProduct(RelativeVelocity, Direction);
	}

	double Milestone3EdgeContinentalFraction(const FCarrierV2Stage5BoundaryEdge& Edge)
	{
		return 0.5 * (Edge.MaterialA.ContinentalFraction + Edge.MaterialB.ContinentalFraction);
	}

	void AddMilestone3Label(
		FCarrierV2Milestone3FixtureResult& Result,
		const int32 ContactId,
		const FCarrierV2Stage5BoundaryEdge& Edge,
		const FString& LabelClass,
		const FString& Provenance)
	{
		FCarrierV2Milestone3TriangleLabelRecord Label;
		Label.LabelId = Result.TriangleLabels.Num();
		Label.ContactId = ContactId;
		Label.PlateId = Edge.PlateId;
		Label.LocalTriangleId = Edge.LocalTriangleId;
		Label.SourceTriangleId = Edge.SourceTriangleId;
		Label.LabelClass = LabelClass;
		Label.Provenance = Provenance;
		Label.bFilterActive =
			LabelClass.Equals(TEXT("subducting"), ESearchCase::IgnoreCase) ||
			LabelClass.Equals(TEXT("colliding"), ESearchCase::IgnoreCase);
		Result.TriangleLabels.Add(Label);

		if (LabelClass.Equals(TEXT("subducting"), ESearchCase::IgnoreCase))
		{
			++Result.Metrics.SubductingTriangleLabelCount;
		}
		else if (LabelClass.Equals(TEXT("colliding"), ESearchCase::IgnoreCase))
		{
			++Result.Metrics.CollidingTriangleLabelCount;
		}
		if (Label.bFilterActive)
		{
			++Result.Metrics.FilterActiveTriangleLabelCount;
		}
	}

	void SortMilestone3Evidence(FCarrierV2Milestone3FixtureResult& Result)
	{
		Result.Contacts.Sort([](const FCarrierV2Milestone3ContactRecord& A, const FCarrierV2Milestone3ContactRecord& B)
		{
			if (A.PlateA != B.PlateA) { return A.PlateA < B.PlateA; }
			if (A.PlateB != B.PlateB) { return A.PlateB < B.PlateB; }
			if (A.PlateC != B.PlateC) { return A.PlateC < B.PlateC; }
			if (A.SourceEdgeA != B.SourceEdgeA) { return A.SourceEdgeA < B.SourceEdgeA; }
			return A.SourceEdgeB < B.SourceEdgeB;
		});
		for (int32 Index = 0; Index < Result.Contacts.Num(); ++Index)
		{
			Result.Contacts[Index].ContactId = Index;
		}
		Result.TriangleLabels.Sort([](const FCarrierV2Milestone3TriangleLabelRecord& A, const FCarrierV2Milestone3TriangleLabelRecord& B)
		{
			if (A.PlateId != B.PlateId) { return A.PlateId < B.PlateId; }
			if (A.LocalTriangleId != B.LocalTriangleId) { return A.LocalTriangleId < B.LocalTriangleId; }
			return A.LabelClass < B.LabelClass;
		});
		for (int32 Index = 0; Index < Result.TriangleLabels.Num(); ++Index)
		{
			Result.TriangleLabels[Index].LabelId = Index;
		}
	}

	uint64 HashMilestone3ContactLabels(const FCarrierV2Milestone3FixtureResult& Result)
	{
		uint64 Hash = HashMilestone3Config(Result.Config);
		for (const FCarrierV2Milestone3ContactRecord& Contact : Result.Contacts)
		{
			HashMixInt(Hash, Contact.ContactId);
			HashMixInt(Hash, Contact.PlateA);
			HashMixInt(Hash, Contact.PlateB);
			HashMixInt(Hash, Contact.PlateC);
			HashMixInt(Hash, Contact.LocalTriangleA);
			HashMixInt(Hash, Contact.LocalTriangleB);
			HashMixDouble(Hash, Contact.SignedOpeningRate);
			HashMixDouble(Hash, Contact.PlateAContinentalFraction);
			HashMixDouble(Hash, Contact.PlateBContinentalFraction);
			HashMixString(Hash, Contact.ContactClass);
			HashMixString(Hash, Contact.PolarityClass);
			HashMixString(Hash, Contact.Provenance);
		}
		for (const FCarrierV2Milestone3TriangleLabelRecord& Label : Result.TriangleLabels)
		{
			HashMixInt(Hash, Label.LabelId);
			HashMixInt(Hash, Label.PlateId);
			HashMixInt(Hash, Label.LocalTriangleId);
			HashMixInt(Hash, Label.SourceTriangleId);
			HashMixString(Hash, Label.LabelClass);
			HashMixString(Hash, Label.Provenance);
			HashMixInt(Hash, Label.bFilterActive ? 1 : 0);
		}
		return Hash;
	}

	uint64 HashMilestone3SamplingRecords(const FCarrierV2Milestone3FixtureResult& Result)
	{
		uint64 Hash = HashMilestone3Config(Result.Config);
		HashMixString(Hash, Result.Metrics.ContactLabelHash);
		for (const FCarrierV2Milestone3SampleRecord& Record : Result.SampleRecords)
		{
			HashMixInt(Hash, Record.SampleId);
			HashMixInt(Hash, Record.RawHitCount);
			HashMixInt(Hash, Record.FilteredSubductingHitCount);
			HashMixInt(Hash, Record.FilteredCollidingHitCount);
			HashMixInt(Hash, Record.ValidHitCount);
			HashMixInt(Hash, Record.SelectedPlateId);
			HashMixInt(Hash, Record.SelectedLocalTriangleId);
			HashMixInt(Hash, Record.AssignedPlateId);
			HashMixDouble(Hash, Record.SelectedContinentalFraction);
			HashMixInt(Hash, Record.bSingleHitWritten ? 1 : 0);
			HashMixInt(Hash, Record.bSelectedFilteredHit ? 1 : 0);
			HashMixInt(Hash, Record.bFilterExhausted ? 1 : 0);
			HashMixInt(Hash, Record.bPostFilterUnresolvedMultihit ? 1 : 0);
			HashMixInt(Hash, Record.bDeferredNondivergentGap ? 1 : 0);
			HashMixInt(Hash, Record.bGeneratedOceanic ? 1 : 0);
			HashMixInt(Hash, Record.bPreviouslyBlockedBecameQ1Q2Oceanic ? 1 : 0);
			HashMixInt(Hash, Record.Q1PlateId);
			HashMixInt(Hash, Record.Q2PlateId);
			HashMixDouble(Hash, Record.QGammaAlpha);
			HashMixDouble(Hash, Record.Q1Q2OpeningRate);
			HashMixString(Hash, Record.SelectionProvenance);
		}
		return Hash;
	}

	uint64 HashMilestone3Topology(const FCarrierV2Milestone3Config& Config, const TArray<FCarrierV2Plate>& RebuiltPlates)
	{
		uint64 Hash = HashMilestone3Config(Config);
		for (const FCarrierV2Plate& Plate : RebuiltPlates)
		{
			HashMixInt(Hash, Plate.PlateId);
			HashMixInt(Hash, Plate.LocalVertices.Num());
			HashMixInt(Hash, Plate.LocalTriangles.Num());
			for (const FCarrierV2PlateVertex& Vertex : Plate.LocalVertices)
			{
				HashMixInt(Hash, Vertex.SourceSampleId);
				HashMixDouble(Hash, Vertex.UnitPosition.X);
				HashMixDouble(Hash, Vertex.UnitPosition.Y);
				HashMixDouble(Hash, Vertex.UnitPosition.Z);
				HashMixDouble(Hash, Vertex.Material.ContinentalFraction);
				HashMixString(Hash, MaterialClassName(Vertex.Material.MaterialClass));
				HashMixString(Hash, Vertex.Material.Provenance);
			}
			for (const FCarrierV2PlateTriangle& Triangle : Plate.LocalTriangles)
			{
				HashMixInt(Hash, Triangle.SourceTriangleId);
				for (int32 Corner = 0; Corner < 3; ++Corner)
				{
					HashMixInt(Hash, Triangle.SourceSampleIds[Corner]);
					HashMixInt(Hash, Triangle.LocalVertexIds[Corner]);
				}
			}
		}
		return Hash;
	}

	uint64 HashMilestone3Metrics(const FCarrierV2Milestone3Metrics& Metrics, const bool bIncludeReplayFields)
	{
		uint64 Hash = FnvOffset;
		HashMixString(Hash, Metrics.ConfigHash);
		HashMixString(Hash, Metrics.PostCycleAuthorityHash);
		HashMixString(Hash, Metrics.ContactLabelHash);
		HashMixString(Hash, Metrics.ResampleDecisionHash);
		HashMixString(Hash, Metrics.RebuiltTopologyHash);
		HashMixInt(Hash, Metrics.PinnedM2BaselineMismatchCount);
		HashMixInt(Hash, Metrics.ContactEvidenceCount);
		HashMixInt(Hash, Metrics.SubductingTriangleLabelCount);
		HashMixInt(Hash, Metrics.CollidingTriangleLabelCount);
		HashMixInt(Hash, Metrics.FilteredSingleSourceWriteCount);
		HashMixInt(Hash, Metrics.FilterExhaustedSampleCount);
		HashMixInt(Hash, Metrics.PostFilterUnresolvedMultihitCount);
		HashMixInt(Hash, Metrics.Q1Q2DivergentAcceptedCount);
		HashMixInt(Hash, Metrics.Q1Q2DivergenceRejectedCount);
		HashMixInt(Hash, Metrics.PreviouslyBlockedBecameQ1Q2OceanicCount);
		HashMixInt(Hash, Metrics.HoleCountGrowth);
		HashMixInt(Hash, Metrics.UnassignedTriangleCount);
		HashMixInt(Hash, Metrics.UnsupportedOverlapWriteAttemptCount);
		HashMixInt(Hash, Metrics.PriorOwnerReadCount + Metrics.GlobalOwnerReadCount);
		HashMixDouble(Hash, Metrics.ProcessLaneMs);
		HashMixDouble(Hash, Metrics.FullCarrierCycleMs);
		HashMixString(Hash, Metrics.Verdict);
		if (bIncludeReplayFields)
		{
			HashMixString(Hash, Metrics.ReplayPostCycleAuthorityHash);
			HashMixString(Hash, Metrics.ReplayContactLabelHash);
			HashMixString(Hash, Metrics.ReplayResampleDecisionHash);
			HashMixString(Hash, Metrics.ReplayRebuiltTopologyHash);
		}
		return Hash;
	}

	void InitializeMilestone3Metrics(const FCarrierV2Milestone3Config& Config, FCarrierV2Milestone3Metrics& Metrics)
	{
		Metrics = FCarrierV2Milestone3Metrics();
		Metrics.RunId = FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ"));
		Metrics.FixtureId = Config.FixtureId;
		Metrics.FixtureName = Config.FixtureName;
		Metrics.FixtureKind = FixtureKindName(Config.CarrierCycleConfig.MotionConfig.BaseConfig.FixtureKind);
		Metrics.ProcessFilterPolicyId = Config.ProcessFilterPolicyId;
		Metrics.ContactEvidencePolicyId = Config.ContactEvidencePolicyId;
		Metrics.PolarityPolicyId = Config.PolarityPolicyId;
		Metrics.PolarityMode = Config.PolarityMode;
		Metrics.GlobalSampleCount = Config.CarrierCycleConfig.MotionConfig.BaseConfig.SampleCount;
		Metrics.PlateCount = Config.CarrierCycleConfig.MotionConfig.BaseConfig.PlateCount;
		Metrics.LifecycleWindowCount = Config.CarrierCycleConfig.LifecycleWindowCount;
		Metrics.MaxPlateMotionAngleRad = Stage1TotalMotionAngle(Config.CarrierCycleConfig.MotionConfig, 0);
		for (int32 PlateId = 1; PlateId < Config.CarrierCycleConfig.MotionConfig.BaseConfig.PlateCount; ++PlateId)
		{
			Metrics.MaxPlateMotionAngleRad = FMath::Max(
				Metrics.MaxPlateMotionAngleRad,
				Stage1TotalMotionAngle(Config.CarrierCycleConfig.MotionConfig, PlateId));
		}
		Metrics.ConfigHash = HashToString(HashMilestone3Config(Config));
		Metrics.UnassignedTriangleBudget = Config.UnassignedTriangleBudget;
	}

	bool IsMilestone3ForbiddenCounterClear(const FCarrierV2Milestone3Metrics& Metrics)
	{
		return
			Metrics.UnsupportedOverlapWriteAttemptCount == 0 &&
			Metrics.PriorOwnerReadCount == 0 &&
			Metrics.PriorOwnerFallbackCount == 0 &&
			Metrics.GlobalOwnerReadCount == 0 &&
			Metrics.CentroidPrimaryResolutionCount == 0 &&
			Metrics.RandomPrimaryResolutionCount == 0 &&
			Metrics.NearestPrimaryResolutionCount == 0 &&
			Metrics.OwnershipRepairDuringResampleCount == 0 &&
			Metrics.RetentionHysteresisAnchorCount == 0 &&
			Metrics.Q1Q2DiscreteApproxCount == 0 &&
			Metrics.Q1Q2PriorOwnerLookupCount == 0 &&
			Metrics.TerrainBeautyMutationCount == 0 &&
			Metrics.SubductionMaterialMutationCount == 0 &&
			Metrics.CollisionMaterialMutationCount == 0 &&
			Metrics.RiftingMutationCount == 0;
	}

	void BuildMilestone3MarkLookup(
		const TArray<FCarrierV2Milestone3TriangleLabelRecord>& Labels,
		const int32 FirstLabelIndex,
		TMap<uint64, FCarrierV2Stage4MarkFlags>& OutLookup)
	{
		OutLookup.Reset();
		for (int32 Index = FirstLabelIndex; Index < Labels.Num(); ++Index)
		{
			const FCarrierV2Milestone3TriangleLabelRecord& Label = Labels[Index];
			if (!Label.bFilterActive)
			{
				continue;
			}
			FCarrierV2Stage4MarkFlags& Flags = OutLookup.FindOrAdd(Stage4TriangleKey(Label.PlateId, Label.LocalTriangleId));
			if (Label.LabelClass.Equals(TEXT("subducting"), ESearchCase::IgnoreCase))
			{
				Flags.bSubducting = true;
			}
			else if (Label.LabelClass.Equals(TEXT("colliding"), ESearchCase::IgnoreCase))
			{
				Flags.bColliding = true;
			}
		}
	}

	void BuildMilestone3ContactEvidence(
		const FCarrierV2Milestone3Config& Config,
		const TArray<FCarrierV2Stage5BoundaryEdge>& BoundaryEdges,
		FCarrierV2Milestone3FixtureResult& Result)
	{
		const double Start = FPlatformTime::Seconds();
		TMap<uint64, TArray<int32>> EdgesBySourceEdge;
		for (int32 EdgeIndex = 0; EdgeIndex < BoundaryEdges.Num(); ++EdgeIndex)
		{
			const FCarrierV2Stage5BoundaryEdge& Edge = BoundaryEdges[EdgeIndex];
			EdgesBySourceEdge.FindOrAdd(SourceEdgeKey(Edge.SourceSampleA, Edge.SourceSampleB)).Add(EdgeIndex);
		}

		for (const TPair<uint64, TArray<int32>>& Pair : EdgesBySourceEdge)
		{
			TMap<int32, int32> FirstEdgeByPlate;
			for (const int32 EdgeIndex : Pair.Value)
			{
				if (!BoundaryEdges.IsValidIndex(EdgeIndex))
				{
					continue;
				}
				const FCarrierV2Stage5BoundaryEdge& Edge = BoundaryEdges[EdgeIndex];
				if (!FirstEdgeByPlate.Contains(Edge.PlateId))
				{
					FirstEdgeByPlate.Add(Edge.PlateId, EdgeIndex);
				}
			}
			if (FirstEdgeByPlate.Num() < 2)
			{
				continue;
			}

			TArray<int32> PlateIds;
			FirstEdgeByPlate.GetKeys(PlateIds);
			PlateIds.Sort();
			if (PlateIds.Num() > 2)
			{
				FCarrierV2Milestone3ContactRecord Contact;
				Contact.ContactId = Result.Contacts.Num();
				Contact.PlateA = PlateIds[0];
				Contact.PlateB = PlateIds[1];
				Contact.PlateC = PlateIds[2];
				Contact.ContactClass = TEXT("third_plate_intrusion");
				Contact.PolarityClass = TEXT("unresolved_three_plate");
				Contact.Provenance = TEXT("plate_local_shared_source_edge_group");
				Result.Contacts.Add(Contact);
				++Result.Metrics.ThirdPlateContactCount;
				continue;
			}

			const FCarrierV2Stage5BoundaryEdge& EdgeA = BoundaryEdges[*FirstEdgeByPlate.Find(PlateIds[0])];
			const FCarrierV2Stage5BoundaryEdge& EdgeB = BoundaryEdges[*FirstEdgeByPlate.Find(PlateIds[1])];
			const FVector3d MidA = (EdgeA.A + EdgeA.B).GetSafeNormal();
			const FVector3d MidB = (EdgeB.A + EdgeB.B).GetSafeNormal();
			const FVector3d QGamma = (MidA + MidB).GetSafeNormal();
			const double OpeningRate = Milestone3OpeningRate(
				Config.CarrierCycleConfig.MotionConfig,
				EdgeA.PlateId,
				EdgeB.PlateId,
				MidA,
				MidB,
				QGamma.IsNearlyZero() ? MidA : QGamma);
			const double AContinental = Milestone3EdgeContinentalFraction(EdgeA);
			const double BContinental = Milestone3EdgeContinentalFraction(EdgeB);

			FCarrierV2Milestone3ContactRecord Contact;
			Contact.ContactId = Result.Contacts.Num();
			Contact.PlateA = EdgeA.PlateId;
			Contact.PlateB = EdgeB.PlateId;
			Contact.LocalTriangleA = EdgeA.LocalTriangleId;
			Contact.LocalTriangleB = EdgeB.LocalTriangleId;
			Contact.SourceEdgeA = FMath::Min(EdgeA.SourceSampleA, EdgeA.SourceSampleB);
			Contact.SourceEdgeB = FMath::Max(EdgeA.SourceSampleA, EdgeA.SourceSampleB);
			Contact.SignedOpeningRate = OpeningRate;
			Contact.PlateAContinentalFraction = AContinental;
			Contact.PlateBContinentalFraction = BContinental;
			Contact.Provenance = TEXT("plate_local_shared_source_edge_motion_sign");
			if (OpeningRate > Config.OpeningRateTolerance)
			{
				Contact.ContactClass = TEXT("convergent");
				++Result.Metrics.ConvergentContactCount;
			}
			else if (OpeningRate < -Config.OpeningRateTolerance)
			{
				Contact.ContactClass = TEXT("divergent");
				Contact.PolarityClass = TEXT("none_divergent_no_filter");
				++Result.Metrics.DivergentContactCount;
			}
			else
			{
				Contact.ContactClass = TEXT("transform_low_margin");
				Contact.PolarityClass = TEXT("none_low_margin");
				++Result.Metrics.TransformLowMarginContactCount;
			}

			const bool bConvergent = Contact.ContactClass.Equals(TEXT("convergent"), ESearchCase::IgnoreCase);
			if (bConvergent && Config.bForceNoFilterLabels)
			{
				Contact.PolarityClass = TEXT("fixture_no_filter_labels");
			}
			else if (bConvergent)
			{
				if (Config.PolarityMode.Equals(TEXT("force_plate0_subducts"), ESearchCase::IgnoreCase))
				{
					Contact.PolarityClass = TEXT("fixture_plate0_subducts");
					AddMilestone3Label(Result, Contact.ContactId, EdgeA.PlateId == 0 ? EdgeA : EdgeB, TEXT("subducting"), TEXT("fixture_polarity_override"));
				}
				else if (Config.PolarityMode.Equals(TEXT("force_plate1_subducts"), ESearchCase::IgnoreCase))
				{
					Contact.PolarityClass = TEXT("fixture_plate1_subducts");
					AddMilestone3Label(Result, Contact.ContactId, EdgeA.PlateId == 1 ? EdgeA : EdgeB, TEXT("subducting"), TEXT("fixture_polarity_override"));
				}
				else if (Config.PolarityMode.Equals(TEXT("force_all_subducting"), ESearchCase::IgnoreCase))
				{
					Contact.PolarityClass = TEXT("fixture_all_hits_filter_active");
					AddMilestone3Label(Result, Contact.ContactId, EdgeA, TEXT("subducting"), TEXT("fixture_polarity_override"));
					AddMilestone3Label(Result, Contact.ContactId, EdgeB, TEXT("subducting"), TEXT("fixture_polarity_override"));
				}
				else if (Config.PolarityMode.Equals(TEXT("force_ocean_ocean_ambiguous"), ESearchCase::IgnoreCase))
				{
					Contact.PolarityClass = TEXT("ocean_ocean_age_ambiguous");
					++Result.Metrics.OceanOceanAmbiguousContactCount;
				}
				else if (Config.PolarityMode.Equals(TEXT("force_continental_collision"), ESearchCase::IgnoreCase))
				{
					Contact.PolarityClass = TEXT("continental_collision_candidate");
					++Result.Metrics.ContinentalCollisionCandidateCount;
					AddMilestone3Label(Result, Contact.ContactId, EdgeA, TEXT("colliding"), TEXT("fixture_collision_candidate"));
					AddMilestone3Label(Result, Contact.ContactId, EdgeB, TEXT("colliding"), TEXT("fixture_collision_candidate"));
				}
				else if (AContinental < Config.ContinentalFractionThreshold && BContinental >= Config.ContinentalFractionThreshold)
				{
					Contact.PolarityClass = TEXT("ocean_continent_plate_a_subducts");
					++Result.Metrics.OceanContinentContactCount;
					AddMilestone3Label(Result, Contact.ContactId, EdgeA, TEXT("subducting"), TEXT("contact_local_material_ocean_continent"));
				}
				else if (BContinental < Config.ContinentalFractionThreshold && AContinental >= Config.ContinentalFractionThreshold)
				{
					Contact.PolarityClass = TEXT("ocean_continent_plate_b_subducts");
					++Result.Metrics.OceanContinentContactCount;
					AddMilestone3Label(Result, Contact.ContactId, EdgeB, TEXT("subducting"), TEXT("contact_local_material_ocean_continent"));
				}
				else if (AContinental >= Config.ContinentalFractionThreshold && BContinental >= Config.ContinentalFractionThreshold)
				{
					Contact.PolarityClass = TEXT("continental_collision_candidate");
					++Result.Metrics.ContinentalCollisionCandidateCount;
					AddMilestone3Label(Result, Contact.ContactId, EdgeA, TEXT("colliding"), TEXT("contact_local_material_continent_continent"));
					AddMilestone3Label(Result, Contact.ContactId, EdgeB, TEXT("colliding"), TEXT("contact_local_material_continent_continent"));
				}
				else
				{
					Contact.PolarityClass = TEXT("ocean_ocean_age_ambiguous");
					++Result.Metrics.OceanOceanAmbiguousContactCount;
				}
			}

			Result.Contacts.Add(Contact);
		}

		Result.Metrics.ContactEvidenceCount = Result.Contacts.Num();
		Result.Metrics.ContactEvidenceMs += (FPlatformTime::Seconds() - Start) * 1000.0;
	}

	void RebuildMilestone3PlateLocalTopology(
		const FCarrierV2Milestone3Config& Config,
		const FCarrierV2BuildState& State,
		const TArray<int32>& SampleAssignments,
		const TArray<FCarrierV2MaterialRecord>& SampleMaterials,
		FCarrierV2Milestone3FixtureResult& Result)
	{
		FCarrierV2Milestone2FixtureResult TempResult;
		TempResult.Config = Config.CarrierCycleConfig;
		RebuildMilestone2PlateLocalTopology(Config.CarrierCycleConfig, State, SampleAssignments, SampleMaterials, TempResult);
		Result.RebuiltPlates = TempResult.RebuiltPlates;

		Result.Metrics.TopologyRebuildCount += TempResult.Metrics.TopologyRebuildCount;
		Result.Metrics.ProcessStateResetCount += TempResult.Metrics.ProcessStateResetCount;
		Result.Metrics.RebuiltPlateCount = TempResult.Metrics.RebuiltPlateCount;
		Result.Metrics.RebuiltLocalVertexCountSum = TempResult.Metrics.RebuiltLocalVertexCountSum;
		Result.Metrics.RebuiltLocalTriangleCountSum = TempResult.Metrics.RebuiltLocalTriangleCountSum;
		Result.Metrics.RebuiltTriangleAssignmentCount = TempResult.Metrics.RebuiltTriangleAssignmentCount;
		Result.Metrics.MixedVertexTriangleCount += TempResult.Metrics.MixedVertexTriangleCount;
		Result.Metrics.MajorityTriangleAssignmentCount += TempResult.Metrics.MajorityTriangleAssignmentCount;
		Result.Metrics.ThreeWayTriangleAssignmentCount += TempResult.Metrics.ThreeWayTriangleAssignmentCount;
		Result.Metrics.UnassignedTriangleCount += TempResult.Metrics.UnassignedTriangleCount;
		Result.Metrics.TopologyRebuildMs += TempResult.Metrics.TopologyRebuildMs;
		Result.Metrics.RebuiltTopologyHash = HashToString(HashMilestone3Topology(Config, Result.RebuiltPlates));
	}

	void SampleMilestone3GlobalTds(
		const FCarrierV2Milestone3Config& Config,
		const FCarrierV2BuildState& State,
		const TArray<TUniquePtr<FCarrierV2Stage1PlateQueryRuntime>>& Runtimes,
		const TArray<FCarrierV2Milestone2BoundarySearchEdge>& BoundaryEdges,
		const TMap<uint64, FCarrierV2Stage4MarkFlags>& MarkLookup,
		const TSet<int32>& PreviousBlockedSamples,
		TArray<int32>& OutSamplePlateAssignments,
		TArray<FCarrierV2MaterialRecord>& OutSampleMaterials,
		TSet<int32>& OutBlockedSamples,
		FCarrierV2Milestone3FixtureResult& Result)
	{
		const double Start = FPlatformTime::Seconds();
		OutSamplePlateAssignments.Init(INDEX_NONE, State.Samples.Num());
		OutSampleMaterials.SetNum(State.Samples.Num());
		Result.SampleRecords.Reserve(Result.SampleRecords.Num() + State.Samples.Num());

		FCarrierV2Stage1Metrics QueryMetrics;
		for (const FCarrierV2SubstrateSample& Sample : State.Samples)
		{
			FCarrierV2Milestone3SampleRecord Record;
			Record.SampleId = Sample.SampleId;
			Record.bPreviouslyBlockedSample = PreviousBlockedSamples.Contains(Sample.SampleId);
			if (Record.bPreviouslyBlockedSample)
			{
				++Result.Metrics.PreviouslyBlockedSampleCount;
			}

			TArray<FCarrierV2Stage1ProjectionHit, TInlineAllocator<16>> Hits;
			CollectStage1AabbHits(Config.CarrierCycleConfig.MotionConfig, State, Sample, Runtimes, QueryMetrics, Hits);
			Record.RawHitCount = Hits.Num();
			Result.Metrics.RawHitCountTotal += Hits.Num();

			if (Hits.IsEmpty())
			{
				++Result.Metrics.Q1Q2BoundaryQueryCount;
				FCarrierV2Stage5BoundaryCandidate Q1;
				FCarrierV2Stage5BoundaryCandidate Q2;
				FCarrierV2Milestone2Metrics TempM2Metrics;
				if (!FindMilestone2Q1Q2BoundaryPair(
					BoundaryEdges,
					Sample.UnitPosition,
					Config.CarrierCycleConfig.MotionConfig.BaseConfig.RayEpsilon,
					Config.CarrierCycleConfig.MotionConfig.BaseConfig.PlateCount,
					Q1,
					Q2,
					TempM2Metrics))
				{
					Record.SelectionProvenance = TEXT("q1q2_no_boundary_pair");
					++Result.Metrics.GapFillNoBoundaryPairCount;
					OutBlockedSamples.Add(Sample.SampleId);
					Result.SampleRecords.Add(Record);
					continue;
				}

				const FVector3d QGamma = (Q1.Point + Q2.Point).GetSafeNormal();
				const double DPlate = FMath::Min(Q1.DistanceRad, Q2.DistanceRad);
				const double DGamma = GeodesicDistanceRad(Sample.UnitPosition, QGamma);
				const double Alpha = (DGamma + DPlate) > 1.0e-12 ? DGamma / (DGamma + DPlate) : 0.0;
				const double OpeningRate = Milestone3OpeningRate(
					Config.CarrierCycleConfig.MotionConfig,
					Q1.PlateId,
					Q2.PlateId,
					Q1.Point,
					Q2.Point,
					QGamma);
				const FCarrierV2Stage4MarkFlags* Q1Flags = MarkLookup.Find(Stage4TriangleKey(Q1.PlateId, Q1.LocalTriangleId));
				const FCarrierV2Stage4MarkFlags* Q2Flags = MarkLookup.Find(Stage4TriangleKey(Q2.PlateId, Q2.LocalTriangleId));
				const bool bBoundaryPairFilteredByProcess =
					(Q1Flags != nullptr && (Q1Flags->bSubducting || Q1Flags->bColliding)) ||
					(Q2Flags != nullptr && (Q2Flags->bSubducting || Q2Flags->bColliding));

				Record.bBoundaryPairFound = true;
				Record.bQ1Q2DifferentPlates = Q1.PlateId != Q2.PlateId;
				Record.Q1PlateId = Q1.PlateId;
				Record.Q2PlateId = Q2.PlateId;
				Record.Q1DistanceRad = Q1.DistanceRad;
				Record.Q2DistanceRad = Q2.DistanceRad;
				Record.QGammaDistanceRad = DGamma;
				Record.QGammaAlpha = Alpha;
				Record.Q1Q2OpeningRate = OpeningRate;
				++Result.Metrics.Q1Q2BoundaryPairCount;

				if (Record.bQ1Q2DifferentPlates && !bBoundaryPairFilteredByProcess && OpeningRate > Config.OpeningRateTolerance)
				{
					const bool bAssignQ2 = Q2.DistanceRad < Q1.DistanceRad;
					Record.AssignedPlateId = bAssignQ2 ? Q2.PlateId : Q1.PlateId;
					Record.SelectedPlateId = Record.AssignedPlateId;
					Record.bGeneratedOceanic = true;
					Record.SelectionProvenance = TEXT("m3_true_divergent_q1q2_qgamma_oceanic_generation");
					OutSamplePlateAssignments[Sample.SampleId] = Record.AssignedPlateId;
					OutSampleMaterials[Sample.SampleId] = MakeMilestone2Material(0.0, TEXT("m3_true_divergent_q1q2_qgamma_oceanic"));
					++Result.Metrics.Q1Q2DivergentAcceptedCount;
					++Result.Metrics.Q1Q2GapFillCount;
					++Result.Metrics.GeneratedOceanicCount;
					if (Record.bPreviouslyBlockedSample)
					{
						Record.bPreviouslyBlockedBecameQ1Q2Oceanic = true;
						++Result.Metrics.PreviouslyBlockedBecameQ1Q2OceanicCount;
					}
					Result.SampleRecords.Add(Record);
					continue;
				}

				Record.bDeferredNondivergentGap = true;
				Record.SelectionProvenance = bBoundaryPairFilteredByProcess
					? TEXT("deferred_process_filtered_gap_no_q1q2_fallback")
					: TEXT("deferred_nondivergent_gap_no_oceanic_generation");
				++Result.Metrics.Q1Q2DivergenceRejectedCount;
				++Result.Metrics.DeferredNondivergentGapCount;
				OutBlockedSamples.Add(Sample.SampleId);
				Result.SampleRecords.Add(Record);
				continue;
			}

			TArray<const FCarrierV2Stage1ProjectionHit*, TInlineAllocator<16>> ValidHits;
			TArray<int32, TInlineAllocator<8>> RawPlateIds;
			TArray<int32, TInlineAllocator<8>> ValidPlateIds;
			for (const FCarrierV2Stage1ProjectionHit& Hit : Hits)
			{
				RawPlateIds.AddUnique(Hit.PlateId);
				const FCarrierV2Stage4MarkFlags* Flags = MarkLookup.Find(Stage4TriangleKey(Hit.PlateId, Hit.LocalTriangleId));
				const bool bForcedFiltered = Config.bForceAllOverlapHitsFiltered && Hits.Num() > 1;
				const bool bSubducting = bForcedFiltered || (Flags != nullptr && Flags->bSubducting);
				const bool bColliding = Flags != nullptr && Flags->bColliding;
				if (bSubducting)
				{
					++Record.FilteredSubductingHitCount;
					++Result.Metrics.FilteredSubductingHitCount;
				}
				if (bColliding)
				{
					++Record.FilteredCollidingHitCount;
					++Result.Metrics.FilteredCollidingHitCount;
				}
				if (!Config.bEnableProcessFilters || (!bSubducting && !bColliding))
				{
					ValidHits.Add(&Hit);
					ValidPlateIds.AddUnique(Hit.PlateId);
				}
			}
			Record.ValidHitCount = ValidHits.Num();

			if (ValidHits.IsEmpty())
			{
				Record.bFilterExhausted = true;
				Record.SelectionProvenance = TEXT("filter_exhausted_raw_overlap_no_q1q2_fallback");
				++Result.Metrics.FilterExhaustedSampleCount;
				OutBlockedSamples.Add(Sample.SampleId);
				Result.SampleRecords.Add(Record);
				continue;
			}

			if (ValidPlateIds.Num() > 1)
			{
				Record.bPostFilterUnresolvedMultihit = true;
				Record.SelectionProvenance = TEXT("post_filter_multiplate_unresolved_no_primary_resolver");
				if (RawPlateIds.Num() > 2)
				{
					++Result.Metrics.ThirdPlateContactCount;
				}
				++Result.Metrics.PostFilterUnresolvedMultihitCount;
				++Result.Metrics.DeferredOverlapSampleCount;
				OutBlockedSamples.Add(Sample.SampleId);
				Result.SampleRecords.Add(Record);
				continue;
			}

			const FCarrierV2Stage1ProjectionHit& SelectedHit = *ValidHits[0];
			Record.SelectedPlateId = SelectedHit.PlateId;
			Record.SelectedLocalTriangleId = SelectedHit.LocalTriangleId;
			Record.SelectedSourceTriangleId = SelectedHit.SourceTriangleId;
			Record.AssignedPlateId = SelectedHit.PlateId;
			Record.SelectedContinentalFraction = SelectedHit.ContinentalFraction;
			Record.bSingleHitWritten = true;
			Record.bSelectedFilteredHit = ValidHits.Num() < Hits.Num();
			Record.SelectionProvenance = Record.bSelectedFilteredHit
				? TEXT("process_filtered_single_source_barycentric_resample")
				: TEXT("single_source_barycentric_resample");
			OutSamplePlateAssignments[Sample.SampleId] = SelectedHit.PlateId;
			OutSampleMaterials[Sample.SampleId] = MakeMilestone2Material(
				SelectedHit.ContinentalFraction,
				Record.bSelectedFilteredHit ? TEXT("m3_process_filtered_barycentric_resample") : TEXT("m3_barycentric_resample"));
			++Result.Metrics.ValidSingleHitWriteCount;
			if (Record.bSelectedFilteredHit || (RawPlateIds.Num() > 1 && ValidPlateIds.Num() == 1))
			{
				++Result.Metrics.FilteredSingleSourceWriteCount;
			}
			Result.SampleRecords.Add(Record);
		}

		Result.Metrics.AabbRayQueryCount += QueryMetrics.RayQueryCount;
		Result.Metrics.ResampleFilterMs += (FPlatformTime::Seconds() - Start) * 1000.0;
	}

	bool ApplyMilestone3Window(
		const FCarrierV2Milestone3Config& Config,
		FCarrierV2BuildState& State,
		TSet<int32>& PreviousBlockedSamples,
		FCarrierV2Milestone3FixtureResult& Result)
	{
		FCarrierV2Stage1Metrics QueryRuntimeMetrics;
		TArray<TUniquePtr<FCarrierV2Stage1PlateQueryRuntime>> QueryRuntimes;
		FString QueryRuntimeError;
		if (!BuildStage1PlateQueryRuntimes(State, QueryRuntimes, QueryRuntimeMetrics, QueryRuntimeError))
		{
			Result.Error = QueryRuntimeError;
			Result.Metrics.Verdict = TEXT("FAIL_M3_AABB_RUNTIME_BUILD");
			return false;
		}
		Result.Metrics.AabbBuildMs += QueryRuntimeMetrics.PlateAabbBuildMs;

		FCarrierV2Stage1Metrics MotionMetrics;
		ApplyStage1MotionAndMeasure(Config.CarrierCycleConfig.MotionConfig, State, MotionMetrics);
		Result.Metrics.MotionApplyMs += MotionMetrics.MotionApplyMs;
		UpdateStage1MovedBroadphaseCaps(Config.CarrierCycleConfig.MotionConfig, State, QueryRuntimes);

		FCarrierV2Milestone2Metrics BoundaryMetrics;
		TArray<FCarrierV2Stage5BoundaryEdge> BoundaryEdges;
		BuildMilestone2BoundaryEdges(State, BoundaryEdges, BoundaryMetrics);
		TArray<FCarrierV2Milestone2BoundarySearchEdge> BoundarySearchEdges;
		BuildMilestone2BoundarySearchEdges(BoundaryEdges, BoundarySearchEdges);

		const int32 FirstLabelIndex = Result.TriangleLabels.Num();
		BuildMilestone3ContactEvidence(Config, BoundaryEdges, Result);
		TMap<uint64, FCarrierV2Stage4MarkFlags> MarkLookup;
		BuildMilestone3MarkLookup(Result.TriangleLabels, FirstLabelIndex, MarkLookup);

		TArray<int32> SampleAssignments;
		TArray<FCarrierV2MaterialRecord> SampleMaterials;
		TSet<int32> CurrentBlockedSamples;
		SampleMilestone3GlobalTds(
			Config,
			State,
			QueryRuntimes,
			BoundarySearchEdges,
			MarkLookup,
			PreviousBlockedSamples,
			SampleAssignments,
			SampleMaterials,
			CurrentBlockedSamples,
			Result);

		if (Result.Metrics.RemeshWindowCount == 0)
		{
			Result.Metrics.HoleCountWindow0 = CurrentBlockedSamples.Num();
		}
		Result.Metrics.HoleCountFinal = CurrentBlockedSamples.Num();
		RebuildMilestone3PlateLocalTopology(Config, State, SampleAssignments, SampleMaterials, Result);
		State.Plates = Result.RebuiltPlates;
		Result.Metrics.PostResetTriangleLabelCount += MarkLookup.Num();
		PreviousBlockedSamples = MoveTemp(CurrentBlockedSamples);
		++Result.Metrics.RemeshWindowCount;
		return true;
	}

	bool FindMilestone2ConfigForPinnedBaseline(
		const FString& FixtureId,
		FCarrierV2Milestone2Config& OutConfig)
	{
		for (const FCarrierV2Milestone2Config& Config : FCarrierV2Milestone2::MakeMicroFixtureConfigs())
		{
			if (Config.FixtureId == FixtureId)
			{
				OutConfig = Config;
				return true;
			}
		}
		if (FixtureId == TEXT("SCALE-50K-M2"))
		{
			OutConfig = FCarrierV2Milestone2::MakeScaleConfig(50000, false);
			return true;
		}
		if (FixtureId == TEXT("SCALE-100K-M2"))
		{
			OutConfig = FCarrierV2Milestone2::MakeScaleConfig(100000, true);
			return true;
		}
		if (FixtureId == TEXT("SCALE-250K-M2"))
		{
			OutConfig = FCarrierV2Milestone2::MakeScaleConfig(250000, true);
			return true;
		}
		return false;
	}

	void RunMilestone3PinnedM2Baselines(
		const FCarrierV2Milestone3Config& Config,
		FCarrierV2Milestone3FixtureResult& Result)
	{
		for (const FCarrierV2Milestone3PinnedM2Baseline& Baseline : Config.PinnedM2Baselines)
		{
			FCarrierV2Milestone2Config M2Config;
			if (!FindMilestone2ConfigForPinnedBaseline(Baseline.FixtureId, M2Config))
			{
				++Result.Metrics.PinnedM2BaselineMismatchCount;
				Result.Error += FString::Printf(TEXT("Missing pinned M2 config for %s. "), *Baseline.FixtureId);
				continue;
			}
			FCarrierV2Milestone2FixtureResult M2Result;
			FCarrierV2Milestone2::RunFixtureWithReplay(M2Config, M2Result);
			++Result.Metrics.PinnedM2BaselineComparedCount;
			const bool bMatch =
				M2Result.Metrics.bFixturePass &&
				M2Result.Metrics.PostCycleAuthorityHash == Baseline.ExpectedPostCycleAuthorityHash &&
				M2Result.Metrics.ResampleOutputHash == Baseline.ExpectedResampleOutputHash &&
				M2Result.Metrics.RebuiltTopologyHash == Baseline.ExpectedRebuiltTopologyHash;
			if (!bMatch)
			{
				++Result.Metrics.PinnedM2BaselineMismatchCount;
				Result.Error += FString::Printf(
					TEXT("Pinned M2 mismatch for %s: post=%s expected=%s resample=%s expected=%s topology=%s expected=%s. "),
					*Baseline.FixtureId,
					*M2Result.Metrics.PostCycleAuthorityHash,
					*Baseline.ExpectedPostCycleAuthorityHash,
					*M2Result.Metrics.ResampleOutputHash,
					*Baseline.ExpectedResampleOutputHash,
					*M2Result.Metrics.RebuiltTopologyHash,
					*Baseline.ExpectedRebuiltTopologyHash);
			}
		}
		Result.Metrics.bPinnedM2BaselinePass =
			Result.Metrics.PinnedM2BaselineComparedCount == Config.PinnedM2Baselines.Num() &&
			Result.Metrics.PinnedM2BaselineMismatchCount == 0;
	}

	void FinalizeMilestone3Gates(const FCarrierV2Milestone3Config& Config, FCarrierV2Milestone3FixtureResult& Result)
	{
		FCarrierV2Milestone3Metrics& M = Result.Metrics;
		M.HoleCountGrowth = M.HoleCountFinal - M.HoleCountWindow0;
		M.bPinnedM2BaselinePass = !Config.bRequirePinnedM2Baseline || M.bPinnedM2BaselinePass;
		M.bFilterInertNoopPass =
			!Config.bRequireFilterInertNoop ||
			(M.FilterActiveTriangleLabelCount == 0 &&
			 M.FilteredSingleSourceWriteCount == 0 &&
			 M.FilterExhaustedSampleCount == 0 &&
			 M.PostFilterUnresolvedMultihitCount == 0 &&
			 M.Q1Q2GapFillCount == 0 &&
			 M.UnassignedTriangleCount == 0);
		M.bContactEvidencePass =
			M.ContactEvidenceCount >= Config.ExpectedMinimumContacts &&
			M.FilterActiveTriangleLabelCount >= Config.ExpectedMinimumFilterLabels;
		M.bProcessFilterEvidencePass =
			(!Config.bRequireFilteredSingleSource || M.FilteredSingleSourceWriteCount >= FMath::Max(1, Config.ExpectedMinimumFilteredSingleSource)) &&
			(!Config.bRequireFilterExhausted || M.FilterExhaustedSampleCount > 0) &&
			(!Config.bRequirePostFilterMultihit || M.PostFilterUnresolvedMultihitCount > 0) &&
			(!Config.bRequirePolaritySwap || M.FilteredSingleSourceWriteCount > 0);
		M.bHolePumpTripwirePass =
			!Config.bRequireHolePumpTripwire ||
			(M.PreviouslyBlockedBecameQ1Q2OceanicCount == 0 &&
			 M.HoleCountGrowth <= Config.ExpectedMaximumHoleCountGrowth &&
			 (M.FilteredSingleSourceWriteCount > 0 || M.FilterExhaustedSampleCount > 0 || M.PostFilterUnresolvedMultihitCount > 0));
		M.bQ1Q2DivergencePass =
			(!Config.bRequireTrueDivergentGap || M.Q1Q2DivergentAcceptedCount >= FMath::Max(1, Config.ExpectedMinimumDivergentGapFill)) &&
			(!Config.bRequireDivergenceRejected || M.Q1Q2DivergenceRejectedCount > 0);
		if (Config.bRequireOceanOceanAmbiguous)
		{
			M.bProcessFilterEvidencePass = M.bProcessFilterEvidencePass && M.OceanOceanAmbiguousContactCount > 0 && M.SubductingTriangleLabelCount == 0;
		}
		if (Config.bRequireContinentalCollisionCandidate)
		{
			M.bProcessFilterEvidencePass = M.bProcessFilterEvidencePass && M.ContinentalCollisionCandidateCount > 0 && M.SubductingTriangleLabelCount == 0;
		}
		if (Config.bRequireThirdPlateIntrusion)
		{
			M.bProcessFilterEvidencePass = M.bProcessFilterEvidencePass && M.ThirdPlateContactCount > 0 && M.SubductingTriangleLabelCount == 0;
		}
		M.bOverlapFilterPolicyPass =
			M.UnsupportedOverlapWriteAttemptCount == 0 &&
			M.CentroidPrimaryResolutionCount == 0 &&
			M.RandomPrimaryResolutionCount == 0 &&
			M.NearestPrimaryResolutionCount == 0;
		M.bTopologyBudgetPass = Config.UnassignedTriangleBudget <= 0 || M.UnassignedTriangleCount <= Config.UnassignedTriangleBudget;
		M.bNoForbiddenFallbackPass = IsMilestone3ForbiddenCounterClear(M);
		M.ProcessLaneMs = M.ContactEvidenceMs;
		M.FullCarrierCycleMs = M.AabbBuildMs + M.MotionApplyMs + M.ContactEvidenceMs + M.ResampleFilterMs + M.TopologyRebuildMs;
		M.bPerformanceBudgetPass =
			!Config.bScaleCharacterization ||
			(M.FullCarrierCycleMs <= Config.PaperResampleCycleBudgetMs && M.ProcessLaneMs <= Config.ProcessLaneBudgetMs);
		M.bResolutionInvariantLabelPass = !Config.bRequireResolutionInvariantLabels || M.bResolutionInvariantLabelPass;
		M.bFixturePass =
			M.bPinnedM2BaselinePass &&
			M.bFilterInertNoopPass &&
			M.bContactEvidencePass &&
			M.bProcessFilterEvidencePass &&
			M.bHolePumpTripwirePass &&
			M.bQ1Q2DivergencePass &&
			M.bOverlapFilterPolicyPass &&
			M.bTopologyBudgetPass &&
			M.bNoForbiddenFallbackPass &&
			M.bPerformanceBudgetPass &&
			M.bResolutionInvariantLabelPass;
		M.bStageGatePass = M.bFixturePass;
		M.Verdict = M.bFixturePass ? TEXT("MILESTONE_3_FIXTURE_PASS") : TEXT("REVISE_MILESTONE_3_FIXTURE");
	}

	bool RunMilestone3ResolutionInvariantFixtureOnce(
		const FCarrierV2Milestone3Config& Config,
		FCarrierV2Milestone3FixtureResult& OutResult)
	{
		FCarrierV2Stage0Metrics BuildMetrics;
		FString Error;
		FCarrierV2BuildState StateA;
		if (!BuildStateForConfig(Config.CarrierCycleConfig.MotionConfig.BaseConfig, StateA, BuildMetrics, Error))
		{
			OutResult.Error = Error;
			return false;
		}
		FCarrierV2Stage1Metrics MotionMetrics;
		ApplyStage1MotionAndMeasure(Config.CarrierCycleConfig.MotionConfig, StateA, MotionMetrics);

		FCarrierV2Milestone2Metrics BoundaryMetrics;
		TArray<FCarrierV2Stage5BoundaryEdge> BoundaryA;
		BuildMilestone2BoundaryEdges(StateA, BoundaryA, BoundaryMetrics);
		BuildMilestone3ContactEvidence(Config, BoundaryA, OutResult);
		SortMilestone3Evidence(OutResult);
		const FString HashA = HashToString(HashMilestone3ContactLabels(OutResult));

		FCarrierV2BuildState StateB;
		FCarrierV2Stage0Metrics BuildMetricsB;
		FCarrierV2Stage0Config AltBase = Config.CarrierCycleConfig.MotionConfig.BaseConfig;
		AltBase.SampleCount = FMath::Max(AltBase.SampleCount * 2, AltBase.SampleCount + 6);
		if (!BuildStateForConfig(AltBase, StateB, BuildMetricsB, Error))
		{
			OutResult.Error = Error;
			return false;
		}
		StateB.Plates = StateA.Plates;
		FCarrierV2Milestone3FixtureResult AltResult;
		AltResult.Config = Config;
		InitializeMilestone3Metrics(Config, AltResult.Metrics);
		TArray<FCarrierV2Stage5BoundaryEdge> BoundaryB;
		FCarrierV2Milestone2Metrics BoundaryMetricsB;
		BuildMilestone2BoundaryEdges(StateB, BoundaryB, BoundaryMetricsB);
		BuildMilestone3ContactEvidence(Config, BoundaryB, AltResult);
		SortMilestone3Evidence(AltResult);
		const FString HashB = HashToString(HashMilestone3ContactLabels(AltResult));

		OutResult.Metrics.ContactLabelHash = HashA;
		OutResult.Metrics.bResolutionInvariantLabelPass = HashA == HashB && OutResult.Metrics.ContactEvidenceCount > 0;
		OutResult.Metrics.bContactEvidencePass = OutResult.Metrics.ContactEvidenceCount > 0;
		return true;
	}

	bool RunMilestone3FixtureOnce(const FCarrierV2Milestone3Config& Config, FCarrierV2Milestone3FixtureResult& OutResult)
	{
		const double TotalStart = FPlatformTime::Seconds();
		OutResult = FCarrierV2Milestone3FixtureResult();
		OutResult.Config = Config;
		InitializeMilestone3Metrics(Config, OutResult.Metrics);

		RunMilestone3PinnedM2Baselines(Config, OutResult);
		if (Config.bRunPinnedM2BaselinesOnly)
		{
			FinalizeMilestone3Gates(Config, OutResult);
			OutResult.Metrics.MetricsHash = HashToString(HashMilestone3Metrics(OutResult.Metrics, false));
			OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
			OutResult.Metrics.PeakMemoryMb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);
			OutResult.bCompleted = true;
			return OutResult.Metrics.bFixturePass;
		}

		if (Config.bRequireResolutionInvariantLabels)
		{
			if (!RunMilestone3ResolutionInvariantFixtureOnce(Config, OutResult))
			{
				OutResult.Metrics.Verdict = TEXT("FAIL_RESOLUTION_INVARIANT_SETUP");
				return false;
			}
			FinalizeMilestone3Gates(Config, OutResult);
			OutResult.Metrics.ResampleDecisionHash = HashToString(HashMilestone3SamplingRecords(OutResult));
			OutResult.Metrics.MetricsHash = HashToString(HashMilestone3Metrics(OutResult.Metrics, false));
			OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
			OutResult.Metrics.PeakMemoryMb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);
			OutResult.bCompleted = true;
			return OutResult.Metrics.bFixturePass;
		}

		FCarrierV2Stage0Metrics BuildMetrics;
		FCarrierV2BuildState State;
		FString Error;
		if (!BuildStateForConfig(Config.CarrierCycleConfig.MotionConfig.BaseConfig, State, BuildMetrics, Error))
		{
			OutResult.Error = Error;
			OutResult.Metrics.Verdict = TEXT("FAIL_BUILD_STATE");
			return false;
		}
		OutResult.Metrics.BuildSubstrateMs = BuildMetrics.BuildSubstrateMs;
		OutResult.Metrics.BuildPlateLocalMs = BuildMetrics.BuildPlateLocalMs;
		OutResult.Metrics.GlobalTriangleCount = State.Triangles.Num();
		OutResult.Metrics.PreCycleAuthorityHash = HashToString(HashStage1Authority(State, Config.CarrierCycleConfig.MotionConfig));

		TSet<int32> PreviousBlockedSamples;
		for (int32 WindowIndex = 0; WindowIndex < FMath::Max(1, Config.CarrierCycleConfig.LifecycleWindowCount); ++WindowIndex)
		{
			if (!ApplyMilestone3Window(Config, State, PreviousBlockedSamples, OutResult))
			{
				OutResult.Metrics.Verdict = TEXT("FAIL_M3_WINDOW");
				return false;
			}
		}

		SortMilestone3Evidence(OutResult);
		OutResult.Metrics.PostCycleAuthorityHash = HashToString(HashStage1Authority(State, Config.CarrierCycleConfig.MotionConfig));
		OutResult.Metrics.ContactLabelHash = HashToString(HashMilestone3ContactLabels(OutResult));
		OutResult.Metrics.ResampleDecisionHash = HashToString(HashMilestone3SamplingRecords(OutResult));
		if (OutResult.Metrics.RebuiltTopologyHash.IsEmpty())
		{
			OutResult.Metrics.RebuiltTopologyHash = HashToString(HashMilestone3Topology(Config, OutResult.RebuiltPlates));
		}
		FinalizeMilestone3Gates(Config, OutResult);
		OutResult.Metrics.MetricsHash = HashToString(HashMilestone3Metrics(OutResult.Metrics, false));
		OutResult.Metrics.TotalMs = (FPlatformTime::Seconds() - TotalStart) * 1000.0;
		OutResult.Metrics.PeakMemoryMb = static_cast<double>(FPlatformMemory::GetStats().UsedPhysical) / (1024.0 * 1024.0);
		OutResult.bCompleted = true;
		return OutResult.Metrics.bFixturePass;
	}

	FCarrierV2Milestone3Config MakeMilestone3ConfigFromStage1(
		const FString& Stage1FixtureId,
		const FString& FixtureId,
		const FString& FixtureName)
	{
		FCarrierV2Milestone3Config Config;
		for (const FCarrierV2Stage1Config& Stage1Config : FCarrierV2Stage1::MakeMicroFixtureConfigs())
		{
			if (Stage1Config.FixtureId == Stage1FixtureId)
			{
				Config.CarrierCycleConfig.MotionConfig = Stage1Config;
				break;
			}
		}
		Config.CarrierCycleConfig.MotionConfig.BaseConfig.FixtureId = FixtureId;
		Config.CarrierCycleConfig.MotionConfig.BaseConfig.FixtureName = FixtureName;
		Config.CarrierCycleConfig.MotionConfig.FixtureId = FixtureId;
		Config.CarrierCycleConfig.MotionConfig.FixtureName = FixtureName;
		Config.CarrierCycleConfig.MotionConfig.SourceStatus = TEXT("source_explicit_m3_process_filter_fixture");
		Config.CarrierCycleConfig.FixtureId = FixtureId;
		Config.CarrierCycleConfig.FixtureName = FixtureName;
		Config.CarrierCycleConfig.CarrierCycleClass = TEXT("m3_process_filter_carrier_cycle");
		Config.CarrierCycleConfig.ResamplePolicyId = TEXT("m3_single_valid_after_filter_or_true_divergent_q1q2");
		Config.CarrierCycleConfig.bRequireFullTopologyRebuild = false;
		Config.FixtureId = FixtureId;
		Config.FixtureName = FixtureName;
		return Config;
	}

	TArray<FCarrierV2Milestone3PinnedM2Baseline> MakeMilestone3PinnedBaselines()
	{
		TArray<FCarrierV2Milestone3PinnedM2Baseline> Baselines;
		auto Add = [&Baselines](const TCHAR* FixtureId, const TCHAR* Post, const TCHAR* Resample, const TCHAR* Topology)
		{
			FCarrierV2Milestone3PinnedM2Baseline Baseline;
			Baseline.FixtureId = FixtureId;
			Baseline.ExpectedPostCycleAuthorityHash = Post;
			Baseline.ExpectedResampleOutputHash = Resample;
			Baseline.ExpectedRebuiltTopologyHash = Topology;
			Baselines.Add(Baseline);
		};
		Add(TEXT("M2-FX-001"), TEXT("7511dcbb7c924c31"), TEXT("08b0a405ab894a99"), TEXT("5ef8bc66d609830c"));
		Add(TEXT("M2-FX-002"), TEXT("8830c6e200c00b5e"), TEXT("db35fcc15c616fcc"), TEXT("fd270209c1c27ec8"));
		Add(TEXT("M2-FX-003"), TEXT("40ba6d7c39a12365"), TEXT("c402341ca3500605"), TEXT("c2d428da806e3611"));
		Add(TEXT("M2-FX-004"), TEXT("cca652702954f563"), TEXT("31f5083a94017b64"), TEXT("88fccb82ea808753"));
		Add(TEXT("M2-FX-005"), TEXT("18722ad63508e3e1"), TEXT("8e0a2604cbc15eb1"), TEXT("58cb7fab0bb550ff"));
		Add(TEXT("M2-FX-006"), TEXT("0a6967c48d78791a"), TEXT("c2975db6a07f3163"), TEXT("e075208a19f4b9de"));
		Add(TEXT("SCALE-50K-M2"), TEXT("1e53655423b1e157"), TEXT("11b0292d7a90e121"), TEXT("02a2aa10fb13f2d6"));
		return Baselines;
	}

	TArray<FCarrierV2Milestone3Config> FCarrierV2Milestone3::MakeMicroFixtureConfigs()
	{
		TArray<FCarrierV2Milestone3Config> Configs;

		FCarrierV2Milestone3Config FX001;
		FX001.FixtureId = TEXT("M3-FX-001-PinnedM2Baseline");
		FX001.FixtureName = TEXT("PinnedM2Baseline");
		FX001.CarrierCycleConfig.FixtureId = FX001.FixtureId;
		FX001.CarrierCycleConfig.FixtureName = FX001.FixtureName;
		FX001.CarrierCycleConfig.MotionConfig.BaseConfig.FixtureId = FX001.FixtureId;
		FX001.CarrierCycleConfig.MotionConfig.FixtureId = FX001.FixtureId;
		FX001.PinnedM2Baselines = MakeMilestone3PinnedBaselines();
		FX001.bRunPinnedM2BaselinesOnly = true;
		FX001.bRequirePinnedM2Baseline = true;
		Configs.Add(FX001);

		FCarrierV2Milestone3Config FX002 = MakeMilestone3ConfigFromStage1(TEXT("FX-005"), TEXT("M3-FX-002-FiltersOnInert"), TEXT("FiltersOnInert"));
		FX002.CarrierCycleConfig.MotionConfig.BaseConfig.PlateCount = 1;
		FX002.bRequireFilterInertNoop = true;
		Configs.Add(FX002);

		FCarrierV2Milestone3Config FX003 = MakeMilestone3ConfigFromStage1(TEXT("FX-008"), TEXT("M3-FX-003-HolePumpSentinel"), TEXT("HolePumpSentinel"));
		FX003.CarrierCycleConfig.LifecycleWindowCount = 2;
		FX003.PolarityMode = TEXT("force_plate0_subducts");
		FX003.bAllowFixtureSpecifiedPolarity = true;
		FX003.bRequireHolePumpTripwire = true;
		FX003.bRequireFilteredSingleSource = true;
		FX003.ExpectedMinimumFilteredSingleSource = 1;
		FX003.ExpectedMinimumContacts = 1;
		FX003.ExpectedMinimumFilterLabels = 1;
		FX003.ExpectedMaximumHoleCountGrowth = 32;
		Configs.Add(FX003);

		FCarrierV2Milestone3Config FX004 = MakeMilestone3ConfigFromStage1(TEXT("FX-008"), TEXT("M3-FX-004-FilteredSingleSource"), TEXT("FilteredSingleSource"));
		FX004.PolarityMode = TEXT("force_plate0_subducts");
		FX004.bAllowFixtureSpecifiedPolarity = true;
		FX004.bRequireFilteredSingleSource = true;
		FX004.ExpectedMinimumFilteredSingleSource = 1;
		FX004.ExpectedMinimumContacts = 1;
		FX004.ExpectedMinimumFilterLabels = 1;
		Configs.Add(FX004);

		FCarrierV2Milestone3Config FX005 = MakeMilestone3ConfigFromStage1(TEXT("FX-008"), TEXT("M3-FX-005-FilterExhausted"), TEXT("FilterExhausted"));
		FX005.PolarityMode = TEXT("force_all_subducting");
		FX005.bAllowFixtureSpecifiedPolarity = true;
		FX005.bForceAllOverlapHitsFiltered = true;
		FX005.bRequireFilterExhausted = true;
		FX005.ExpectedMinimumContacts = 1;
		FX005.ExpectedMinimumFilterLabels = 1;
		Configs.Add(FX005);

		FCarrierV2Milestone3Config FX006 = MakeMilestone3ConfigFromStage1(TEXT("FX-008"), TEXT("M3-FX-006-PostFilterMultihit"), TEXT("PostFilterMultihit"));
		FX006.bForceNoFilterLabels = true;
		FX006.bRequirePostFilterMultihit = true;
		FX006.ExpectedMinimumContacts = 1;
		Configs.Add(FX006);

		FCarrierV2Milestone3Config FX007 = MakeMilestone3ConfigFromStage1(TEXT("FX-007"), TEXT("M3-FX-007-TrueDivergentGap"), TEXT("TrueDivergentGap"));
		FX007.bForceNoFilterLabels = true;
		FX007.bRequireTrueDivergentGap = true;
		FX007.ExpectedMinimumDivergentGapFill = 1;
		Configs.Add(FX007);

		FCarrierV2Milestone3Config FX008 = MakeMilestone3ConfigFromStage1(TEXT("FX-007"), TEXT("M3-FX-008-DivergenceNoLabels"), TEXT("DivergenceNoLabels"));
		FX008.bForceNoFilterLabels = true;
		FX008.bRequireTrueDivergentGap = true;
		FX008.ExpectedMinimumDivergentGapFill = 1;
		Configs.Add(FX008);

		FCarrierV2Milestone3Config FX009 = MakeMilestone3ConfigFromStage1(TEXT("FX-008"), TEXT("M3-FX-009-PolaritySwap"), TEXT("PolaritySwap"));
		FX009.PolarityMode = TEXT("force_plate1_subducts");
		FX009.bAllowFixtureSpecifiedPolarity = true;
		FX009.bRequirePolaritySwap = true;
		FX009.ExpectedMinimumContacts = 1;
		FX009.ExpectedMinimumFilterLabels = 1;
		Configs.Add(FX009);

		FCarrierV2Milestone3Config FX010 = MakeMilestone3ConfigFromStage1(TEXT("FX-008"), TEXT("M3-FX-010-OceanOceanAmbiguous"), TEXT("OceanOceanAmbiguous"));
		FX010.PolarityMode = TEXT("force_ocean_ocean_ambiguous");
		FX010.bAllowFixtureSpecifiedPolarity = true;
		FX010.bRequireOceanOceanAmbiguous = true;
		FX010.ExpectedMinimumContacts = 1;
		Configs.Add(FX010);

		FCarrierV2Milestone3Config FX011 = MakeMilestone3ConfigFromStage1(TEXT("FX-008"), TEXT("M3-FX-011-ContinentalCollisionCandidate"), TEXT("ContinentalCollisionCandidate"));
		FX011.PolarityMode = TEXT("force_continental_collision");
		FX011.bAllowFixtureSpecifiedPolarity = true;
		FX011.bRequireContinentalCollisionCandidate = true;
		FX011.ExpectedMinimumContacts = 1;
		Configs.Add(FX011);

		FCarrierV2Milestone3Config FX012 = MakeMilestone3ConfigFromStage1(TEXT("FX-010"), TEXT("M3-FX-012-ThirdPlateIntrusion"), TEXT("ThirdPlateIntrusion"));
		FX012.bRequireThirdPlateIntrusion = true;
		FX012.ExpectedMinimumContacts = 1;
		Configs.Add(FX012);

		FCarrierV2Milestone3Config FX013 = MakeMilestone3ConfigFromStage1(TEXT("FX-008"), TEXT("M3-FX-013-SamePairMixedSignal"), TEXT("SamePairMixedSignal"));
		FX013.ExpectedMinimumContacts = 1;
		Configs.Add(FX013);

		FCarrierV2Milestone3Config FX014 = MakeMilestone3ConfigFromStage1(TEXT("FX-008"), TEXT("M3-FX-014-ResolutionInvariantLabels"), TEXT("ResolutionInvariantLabels"));
		FX014.PolarityMode = TEXT("force_plate0_subducts");
		FX014.bAllowFixtureSpecifiedPolarity = true;
		FX014.bRequireResolutionInvariantLabels = true;
		FX014.ExpectedMinimumContacts = 1;
		FX014.ExpectedMinimumFilterLabels = 1;
		Configs.Add(FX014);

		return Configs;
	}

	FCarrierV2Milestone3Config FCarrierV2Milestone3::MakeScaleConfig(const int32 SampleCount, const bool bComparisonScale)
	{
		FCarrierV2Milestone3Config Config;
		Config.CarrierCycleConfig = FCarrierV2Milestone2::MakeScaleConfig(SampleCount, bComparisonScale);
		const FString SampleLabel = (SampleCount % 1000 == 0)
			? FString::Printf(TEXT("%dK"), SampleCount / 1000)
			: FString::Printf(TEXT("%d"), SampleCount);
		Config.FixtureId = FString::Printf(TEXT("SCALE-%s-M3-FILTERS"), *SampleLabel);
		Config.FixtureName = FString::Printf(TEXT("Scale%sMilestone3Filters"), *SampleLabel);
		Config.CarrierCycleConfig.FixtureId = Config.FixtureId;
		Config.CarrierCycleConfig.FixtureName = Config.FixtureName;
		Config.CarrierCycleConfig.MotionConfig.BaseConfig.FixtureId = Config.FixtureId;
		Config.CarrierCycleConfig.MotionConfig.BaseConfig.FixtureName = Config.FixtureName;
		Config.CarrierCycleConfig.MotionConfig.FixtureId = Config.FixtureId;
		Config.CarrierCycleConfig.MotionConfig.FixtureName = Config.FixtureName;
		Config.CarrierCycleConfig.LifecycleWindowCount = SampleCount <= 50000 ? 2 : 1;
		Config.bScaleCharacterization = true;
		Config.bRequireResolvableScale = SampleCount <= 250000;
		Config.ExpectedMinimumContacts = 1;
		Config.ExpectedMinimumFilterLabels = 1;
		Config.bRequireFilteredSingleSource = SampleCount <= 250000;
		Config.ExpectedMinimumFilteredSingleSource = 1;
		Config.ExpectedMaximumHoleCountGrowth = 0;
		Config.UnassignedTriangleBudget = SampleCount / 3;
		Config.PaperResampleCycleBudgetMs = SampleCount >= 500000 ? 7160.0 : 3580.0;
		return Config;
	}

	bool FCarrierV2Milestone3::RunFixtureWithReplay(const FCarrierV2Milestone3Config& Config, FCarrierV2Milestone3FixtureResult& OutResult)
	{
		FCarrierV2Milestone3FixtureResult Replay;
		const bool bPrimaryOk = RunMilestone3FixtureOnce(Config, OutResult);
		const bool bReplayOk = RunMilestone3FixtureOnce(Config, Replay);

		OutResult.Metrics.ReplayPostCycleAuthorityHash = Replay.Metrics.PostCycleAuthorityHash;
		OutResult.Metrics.ReplayContactLabelHash = Replay.Metrics.ContactLabelHash;
		OutResult.Metrics.ReplayResampleDecisionHash = Replay.Metrics.ResampleDecisionHash;
		OutResult.Metrics.ReplayRebuiltTopologyHash = Replay.Metrics.RebuiltTopologyHash;
		OutResult.Metrics.ReplayMetricsHash = Replay.Metrics.MetricsHash;
		OutResult.Metrics.bReplayDeterministic =
			OutResult.Metrics.PostCycleAuthorityHash == Replay.Metrics.PostCycleAuthorityHash &&
			OutResult.Metrics.ContactLabelHash == Replay.Metrics.ContactLabelHash &&
			OutResult.Metrics.ResampleDecisionHash == Replay.Metrics.ResampleDecisionHash &&
			OutResult.Metrics.RebuiltTopologyHash == Replay.Metrics.RebuiltTopologyHash;
		OutResult.Metrics.bFixturePass = OutResult.Metrics.bFixturePass && OutResult.Metrics.bReplayDeterministic;
		OutResult.Metrics.bStageGatePass = OutResult.Metrics.bFixturePass;
		if (!OutResult.Metrics.bReplayDeterministic)
		{
			OutResult.Metrics.Verdict = TEXT("FAIL_REPLAY_DETERMINISM");
			if (OutResult.Error.IsEmpty())
			{
				OutResult.Error = TEXT("Replay A/B hash differed.");
			}
		}
		OutResult.Metrics.MetricsHash = HashToString(HashMilestone3Metrics(OutResult.Metrics, true));
		return bPrimaryOk && bReplayOk && OutResult.Metrics.bFixturePass;
	}

	FString FCarrierV2Milestone3::MetricsToJson(const FCarrierV2Milestone3FixtureResult& Result)
	{
		const FCarrierV2Milestone3Metrics& M = Result.Metrics;
		FString Json = TEXT("{");
		Json += FString::Printf(TEXT("\"run_id\":%s,"), *JsonString(M.RunId));
		Json += FString::Printf(TEXT("\"stage_id\":%s,"), *JsonString(M.StageId));
		Json += FString::Printf(TEXT("\"fixture_id\":%s,"), *JsonString(M.FixtureId));
		Json += FString::Printf(TEXT("\"fixture_name\":%s,"), *JsonString(M.FixtureName));
		Json += FString::Printf(TEXT("\"fixture_kind\":%s,"), *JsonString(M.FixtureKind));
		Json += FString::Printf(TEXT("\"process_filter_policy_id\":%s,"), *JsonString(M.ProcessFilterPolicyId));
		Json += FString::Printf(TEXT("\"contact_evidence_policy_id\":%s,"), *JsonString(M.ContactEvidencePolicyId));
		Json += FString::Printf(TEXT("\"polarity_mode\":%s,"), *JsonString(M.PolarityMode));
		Json += FString::Printf(TEXT("\"sample_count\":%d,"), M.GlobalSampleCount);
		Json += FString::Printf(TEXT("\"triangle_count\":%d,"), M.GlobalTriangleCount);
		Json += FString::Printf(TEXT("\"plate_count\":%d,"), M.PlateCount);
		Json += FString::Printf(TEXT("\"lifecycle_windows\":%d,"), M.LifecycleWindowCount);
		Json += FString::Printf(TEXT("\"m2_pinned_compared_count\":%d,"), M.PinnedM2BaselineComparedCount);
		Json += FString::Printf(TEXT("\"m2_pinned_mismatch_count\":%d,"), M.PinnedM2BaselineMismatchCount);
		Json += FString::Printf(TEXT("\"contact_evidence_count\":%d,"), M.ContactEvidenceCount);
		Json += FString::Printf(TEXT("\"convergent_contact_count\":%d,"), M.ConvergentContactCount);
		Json += FString::Printf(TEXT("\"divergent_contact_count\":%d,"), M.DivergentContactCount);
		Json += FString::Printf(TEXT("\"third_plate_contact_count\":%d,"), M.ThirdPlateContactCount);
		Json += FString::Printf(TEXT("\"ocean_continent_contact_count\":%d,"), M.OceanContinentContactCount);
		Json += FString::Printf(TEXT("\"ocean_ocean_ambiguous_contact_count\":%d,"), M.OceanOceanAmbiguousContactCount);
		Json += FString::Printf(TEXT("\"continental_collision_candidate_count\":%d,"), M.ContinentalCollisionCandidateCount);
		Json += FString::Printf(TEXT("\"subducting_triangle_label_count\":%d,"), M.SubductingTriangleLabelCount);
		Json += FString::Printf(TEXT("\"colliding_triangle_label_count\":%d,"), M.CollidingTriangleLabelCount);
		Json += FString::Printf(TEXT("\"filter_active_triangle_label_count\":%d,"), M.FilterActiveTriangleLabelCount);
		Json += FString::Printf(TEXT("\"raw_hit_count_total\":%lld,"), M.RawHitCountTotal);
		Json += FString::Printf(TEXT("\"filtered_subducting_hit_count\":%d,"), M.FilteredSubductingHitCount);
		Json += FString::Printf(TEXT("\"filtered_colliding_hit_count\":%d,"), M.FilteredCollidingHitCount);
		Json += FString::Printf(TEXT("\"valid_single_hit_write_count\":%d,"), M.ValidSingleHitWriteCount);
		Json += FString::Printf(TEXT("\"filtered_single_source_write_count\":%d,"), M.FilteredSingleSourceWriteCount);
		Json += FString::Printf(TEXT("\"filter_exhausted_sample_count\":%d,"), M.FilterExhaustedSampleCount);
		Json += FString::Printf(TEXT("\"post_filter_unresolved_multihit_count\":%d,"), M.PostFilterUnresolvedMultihitCount);
		Json += FString::Printf(TEXT("\"q1q2_divergent_accepted_count\":%d,"), M.Q1Q2DivergentAcceptedCount);
		Json += FString::Printf(TEXT("\"q1q2_divergence_rejected_count\":%d,"), M.Q1Q2DivergenceRejectedCount);
		Json += FString::Printf(TEXT("\"q1q2_gap_fill_count\":%d,"), M.Q1Q2GapFillCount);
		Json += FString::Printf(TEXT("\"generated_oceanic_count\":%d,"), M.GeneratedOceanicCount);
		Json += FString::Printf(TEXT("\"previously_blocked_sample_count\":%d,"), M.PreviouslyBlockedSampleCount);
		Json += FString::Printf(TEXT("\"previously_blocked_became_q1q2_oceanic_count\":%d,"), M.PreviouslyBlockedBecameQ1Q2OceanicCount);
		Json += FString::Printf(TEXT("\"deferred_nondivergent_gap_count\":%d,"), M.DeferredNondivergentGapCount);
		Json += FString::Printf(TEXT("\"hole_count_window0\":%d,"), M.HoleCountWindow0);
		Json += FString::Printf(TEXT("\"hole_count_final\":%d,"), M.HoleCountFinal);
		Json += FString::Printf(TEXT("\"hole_count_growth\":%d,"), M.HoleCountGrowth);
		Json += FString::Printf(TEXT("\"rebuilt_triangle_assignment_count\":%d,"), M.RebuiltTriangleAssignmentCount);
		Json += FString::Printf(TEXT("\"mixed_vertex_triangle_count\":%d,"), M.MixedVertexTriangleCount);
		Json += FString::Printf(TEXT("\"three_way_triangle_assignment_count\":%d,"), M.ThreeWayTriangleAssignmentCount);
		Json += FString::Printf(TEXT("\"unassigned_triangle_count\":%d,"), M.UnassignedTriangleCount);
		Json += FString::Printf(TEXT("\"unassigned_triangle_budget\":%d,"), M.UnassignedTriangleBudget);
		Json += FString::Printf(TEXT("\"unsupported_overlap_write_attempt_count\":%d,"), M.UnsupportedOverlapWriteAttemptCount);
		Json += FString::Printf(TEXT("\"prior_owner_read_count\":%d,"), M.PriorOwnerReadCount);
		Json += FString::Printf(TEXT("\"global_owner_read_count\":%d,"), M.GlobalOwnerReadCount);
		Json += FString::Printf(TEXT("\"centroid_primary_resolution_count\":%d,"), M.CentroidPrimaryResolutionCount);
		Json += FString::Printf(TEXT("\"random_primary_resolution_count\":%d,"), M.RandomPrimaryResolutionCount);
		Json += FString::Printf(TEXT("\"nearest_primary_resolution_count\":%d,"), M.NearestPrimaryResolutionCount);
		Json += FString::Printf(TEXT("\"post_cycle_authority_hash\":%s,"), *JsonString(M.PostCycleAuthorityHash));
		Json += FString::Printf(TEXT("\"contact_label_hash\":%s,"), *JsonString(M.ContactLabelHash));
		Json += FString::Printf(TEXT("\"resample_decision_hash\":%s,"), *JsonString(M.ResampleDecisionHash));
		Json += FString::Printf(TEXT("\"rebuilt_topology_hash\":%s,"), *JsonString(M.RebuiltTopologyHash));
		Json += FString::Printf(TEXT("\"replay_post_cycle_authority_hash\":%s,"), *JsonString(M.ReplayPostCycleAuthorityHash));
		Json += FString::Printf(TEXT("\"replay_contact_label_hash\":%s,"), *JsonString(M.ReplayContactLabelHash));
		Json += FString::Printf(TEXT("\"replay_resample_decision_hash\":%s,"), *JsonString(M.ReplayResampleDecisionHash));
		Json += FString::Printf(TEXT("\"replay_rebuilt_topology_hash\":%s,"), *JsonString(M.ReplayRebuiltTopologyHash));
		Json += FString::Printf(TEXT("\"pinned_m2_baseline_pass\":%s,"), M.bPinnedM2BaselinePass ? TEXT("true") : TEXT("false"));
		Json += FString::Printf(TEXT("\"hole_pump_tripwire_pass\":%s,"), M.bHolePumpTripwirePass ? TEXT("true") : TEXT("false"));
		Json += FString::Printf(TEXT("\"q1q2_divergence_pass\":%s,"), M.bQ1Q2DivergencePass ? TEXT("true") : TEXT("false"));
		Json += FString::Printf(TEXT("\"process_filter_evidence_pass\":%s,"), M.bProcessFilterEvidencePass ? TEXT("true") : TEXT("false"));
		Json += FString::Printf(TEXT("\"topology_budget_pass\":%s,"), M.bTopologyBudgetPass ? TEXT("true") : TEXT("false"));
		Json += FString::Printf(TEXT("\"no_forbidden_fallback_pass\":%s,"), M.bNoForbiddenFallbackPass ? TEXT("true") : TEXT("false"));
		Json += FString::Printf(TEXT("\"replay_deterministic\":%s,"), M.bReplayDeterministic ? TEXT("true") : TEXT("false"));
		Json += FString::Printf(TEXT("\"aabb_build_ms\":%.3f,"), M.AabbBuildMs);
		Json += FString::Printf(TEXT("\"motion_apply_ms\":%.3f,"), M.MotionApplyMs);
		Json += FString::Printf(TEXT("\"contact_evidence_ms\":%.3f,"), M.ContactEvidenceMs);
		Json += FString::Printf(TEXT("\"resample_filter_ms\":%.3f,"), M.ResampleFilterMs);
		Json += FString::Printf(TEXT("\"topology_rebuild_ms\":%.3f,"), M.TopologyRebuildMs);
		Json += FString::Printf(TEXT("\"process_lane_ms\":%.3f,"), M.ProcessLaneMs);
		Json += FString::Printf(TEXT("\"full_carrier_cycle_ms\":%.3f,"), M.FullCarrierCycleMs);
		Json += FString::Printf(TEXT("\"total_ms\":%.3f,"), M.TotalMs);
		Json += FString::Printf(TEXT("\"peak_memory_mb\":%.3f,"), M.PeakMemoryMb);
		Json += FString::Printf(TEXT("\"fixture_pass\":%s,"), M.bFixturePass ? TEXT("true") : TEXT("false"));
		Json += FString::Printf(TEXT("\"verdict\":%s"), *JsonString(M.Verdict));
		Json += TEXT("}");
		return Json;
	}

	FString FCarrierV2Milestone3::BuildCheckpointReport(
		const FCarrierV2Milestone3SuiteResult& Suite,
		const FString& CommandLine,
		const FString& CommitSha)
	{
		const double Start = FPlatformTime::Seconds();
		FString Report;
		Report += TEXT("# CarrierLab Milestone 3 Closeout Report\n\n");
		Report += TEXT("Status: generated by `CarrierLabV2Milestone3`.\n\n");
		Report += FString::Printf(TEXT("- Git HEAD at commandlet launch: `%s`\n"), CommitSha.IsEmpty() ? TEXT("unknown") : *CommitSha);
		Report += FString::Printf(TEXT("- Command: `%s`\n"), *CommandLine);
		Report += FString::Printf(TEXT("- Output root: `%s`\n"), *Suite.OutputRoot);
		Report += FString::Printf(TEXT("- Metrics JSONL: `%s`\n\n"), *Suite.MetricsPath);
		Report += TEXT("## Scope\n\n");
		Report += TEXT("Milestone 3 adds source-grounded contact evidence and conservative process-state labels as filters over the Milestone 2 carrier cycle. It does not add terrain, elevation, uplift, erosion, slab pull, rifting, persistent global ownership, prior-owner retention, or any winner resolver for unresolved multi-hit samples. Process labels are reset at rebuild boundaries and are consumed only as transient filters.\n\n");

		Report += TEXT("## Fixture Gates\n\n");
		Report += TEXT("| fixture | samples | windows | contacts | labels | filtered writes | exhausted | post-filter multi | q1/q2 accepted/rejected | holes w0/final/growth | unassigned/budget | pass | verdict |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|\n");
		for (const FCarrierV2Milestone3FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Milestone3Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %d | %d | %d | %d | %d | %d/%d | %d/%d/%d | %d/%d | %s | `%s` |\n"),
				*M.FixtureId,
				M.GlobalSampleCount,
				M.LifecycleWindowCount,
				M.ContactEvidenceCount,
				M.FilterActiveTriangleLabelCount,
				M.FilteredSingleSourceWriteCount,
				M.FilterExhaustedSampleCount,
				M.PostFilterUnresolvedMultihitCount,
				M.Q1Q2DivergentAcceptedCount,
				M.Q1Q2DivergenceRejectedCount,
				M.HoleCountWindow0,
				M.HoleCountFinal,
				M.HoleCountGrowth,
				M.UnassignedTriangleCount,
				M.UnassignedTriangleBudget,
				M.bFixturePass ? TEXT("pass") : TEXT("fail"),
				*M.Verdict);
		}

		Report += TEXT("\n## Contact And Polarity Evidence\n\n");
		Report += TEXT("| fixture | polarity mode | convergent | divergent | third-plate | O/C | O/O ambiguous | C/C candidate | subducting labels | colliding labels | contact hash |\n");
		Report += TEXT("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FCarrierV2Milestone3FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Milestone3Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | %d | %d | %d | %d | %d | %d | %d | %d | `%s` |\n"),
				*M.FixtureId,
				*M.PolarityMode,
				M.ConvergentContactCount,
				M.DivergentContactCount,
				M.ThirdPlateContactCount,
				M.OceanContinentContactCount,
				M.OceanOceanAmbiguousContactCount,
				M.ContinentalCollisionCandidateCount,
				M.SubductingTriangleLabelCount,
				M.CollidingTriangleLabelCount,
				*M.ContactLabelHash);
		}

		Report += TEXT("\n## Filter And Gap Decisions\n\n");
		Report += TEXT("| fixture | raw hits | filtered subducting | filtered colliding | valid writes | filtered single-source | filter exhausted | unresolved multihit | generated oceanic | prior blocked became oceanic | owner/resolver reads | policy pass |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FCarrierV2Milestone3FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Milestone3Metrics& M = Result.Metrics;
			const int32 OwnerResolvers = M.PriorOwnerReadCount + M.GlobalOwnerReadCount + M.CentroidPrimaryResolutionCount + M.RandomPrimaryResolutionCount + M.NearestPrimaryResolutionCount;
			Report += FString::Printf(
				TEXT("| `%s` | %lld | %d | %d | %d | %d | %d | %d | %d | %d | %d | %s |\n"),
				*M.FixtureId,
				M.RawHitCountTotal,
				M.FilteredSubductingHitCount,
				M.FilteredCollidingHitCount,
				M.ValidSingleHitWriteCount,
				M.FilteredSingleSourceWriteCount,
				M.FilterExhaustedSampleCount,
				M.PostFilterUnresolvedMultihitCount,
				M.GeneratedOceanicCount,
				M.PreviouslyBlockedBecameQ1Q2OceanicCount,
				OwnerResolvers,
				(M.bOverlapFilterPolicyPass && M.bNoForbiddenFallbackPass && M.bHolePumpTripwirePass) ? TEXT("pass") : TEXT("fail"));
		}
		Report += TEXT("\nPrior-blocked note: `prior blocked became oceanic` is diagnostic only. M3 does not use previous blocked sample ids as behavior or authority; q1/q2 generation is decided from the current boundary pair, current relative opening, and current process labels. Nonzero scale counts remain a Milestone 4 watch item for persistent ridge/contact identity, not an ownership fallback.\n");

		Report += TEXT("\n## Pinned M2 Baselines\n\n");
		Report += TEXT("| fixture | compared | mismatches | pass |\n|---|---:|---:|---|\n");
		for (const FCarrierV2Milestone3FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Milestone3Metrics& M = Result.Metrics;
			if (M.PinnedM2BaselineComparedCount > 0 || Result.Config.bRequirePinnedM2Baseline)
			{
				Report += FString::Printf(TEXT("| `%s` | %d | %d | %s |\n"), *M.FixtureId, M.PinnedM2BaselineComparedCount, M.PinnedM2BaselineMismatchCount, M.bPinnedM2BaselinePass ? TEXT("pass") : TEXT("fail"));
			}
		}

		Report += TEXT("\n## Replay A/B\n\n");
		Report += TEXT("| fixture | post authority | replay post | contact labels | replay labels | decisions | replay decisions | topology | replay topology | deterministic |\n|---|---|---|---|---|---|---|---|---|---|\n");
		for (const FCarrierV2Milestone3FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Milestone3Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | %s |\n"),
				*M.FixtureId,
				*M.PostCycleAuthorityHash,
				*M.ReplayPostCycleAuthorityHash,
				*M.ContactLabelHash,
				*M.ReplayContactLabelHash,
				*M.ResampleDecisionHash,
				*M.ReplayResampleDecisionHash,
				*M.RebuiltTopologyHash,
				*M.ReplayRebuiltTopologyHash,
				M.bReplayDeterministic ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Performance\n\n");
		Report += TEXT("| fixture | samples | aabb | motion | contact evidence | resample/filter | rebuild | process lane | full cycle | budget pass | total ms | peak mb |\n|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---:|\n");
		for (const FCarrierV2Milestone3FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Milestone3Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %s | %.3f | %.3f |\n"),
				*M.FixtureId,
				M.GlobalSampleCount,
				M.AabbBuildMs,
				M.MotionApplyMs,
				M.ContactEvidenceMs,
				M.ResampleFilterMs,
				M.TopologyRebuildMs,
				M.ProcessLaneMs,
				M.FullCarrierCycleMs,
				M.bPerformanceBudgetPass ? TEXT("pass") : TEXT("fail"),
				M.TotalMs,
				M.PeakMemoryMb);
		}
		Report += TEXT("\nPerformance note: `full cycle` is the paper-row resample-cycle comparison lane. `process lane` is contact/label evidence cost, separated from the global projection/resample pass so M3 does not claim the 1.24 s Table 2 total row as spare headroom.\n");

		Report += TEXT("\n## Gate Summary\n\n");
		Report += FString::Printf(TEXT("- Pinned M2 baseline gate: `%s`\n"), Suite.bPinnedM2BaselinePass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- Micro gate: `%s`\n"), Suite.bMicroGatePass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 50k scale gate: `%s`\n"), Suite.bScale50kPass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 250k attempted: `%s`, pass: `%s`\n"), Suite.bAttempted250k ? TEXT("yes") : TEXT("no"), Suite.bScale250kPass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(
			TEXT("- 500k optional characterization attempted: `%s`, pass: `%s`\n"),
			Suite.bAttempted500k ? TEXT("yes") : TEXT("no"),
			Suite.bAttempted500k ? (Suite.bScale500kPass ? TEXT("pass") : TEXT("fail")) : TEXT("n/a"));
		if (!Suite.bAttempted500k)
		{
			Report += FString::Printf(TEXT("- 500k not attempted reason: `%s`\n"), Suite.NotAttempted500kReason.IsEmpty() ? TEXT("not requested") : *Suite.NotAttempted500kReason);
		}
		Report += FString::Printf(TEXT("- Final verdict: `%s`\n\n"), *Suite.Verdict);
		Report += TEXT("## Next Gate\n\n");
		Report += TEXT("Stop here for explicit user go/no-go before Milestone 4. M3 validates process filters as carrier-cycle inputs; it does not validate elevation, terrain amplification, rifting, erosion, or persistent ridge identity.\n");

		const double ReportMs = (FPlatformTime::Seconds() - Start) * 1000.0;
		Report += FString::Printf(TEXT("\nReport generation ms: %.3f\n"), ReportMs);
		return Report;
	}

	TArray<FCarrierV2Stage1Config> FCarrierV2Stage1::MakeMicroFixtureConfigs()
	{
		TArray<FCarrierV2Stage1Config> Configs;

		FCarrierV2Stage1Config FX005;
		FX005.BaseConfig.FixtureId = TEXT("FX-005");
		FX005.BaseConfig.FixtureName = TEXT("ZeroMotionReplayBase");
		FX005.BaseConfig.SampleCount = 6;
		FX005.BaseConfig.PlateCount = 2;
		FX005.BaseConfig.FixtureKind = ECarrierV2FixtureKind::Positive;
		FX005.BaseConfig.ExpectedFailureReason = TEXT("none");
		FX005.FixtureId = TEXT("FX-005");
		FX005.FixtureName = TEXT("ZeroMotionReplay");
		FX005.ExpectedMotionClass = TEXT("stable");
		FX005.MotionStepCount = 4;
		FX005.DtMa = 1.0;
		FX005.bRequireNoMotionGapOrOverlap = true;
		FX005.PlateMotions.Add(FCarrierV2MotionSpec());
		FX005.PlateMotions.Add(FCarrierV2MotionSpec());
		Configs.Add(FX005);

		FCarrierV2Stage1Config FX006;
		FX006.BaseConfig.FixtureId = TEXT("FX-006");
		FX006.BaseConfig.FixtureName = TEXT("SinglePlateRotationBase");
		FX006.BaseConfig.SampleCount = 6;
		FX006.BaseConfig.PlateCount = 1;
		FX006.BaseConfig.FixtureKind = ECarrierV2FixtureKind::Positive;
		FX006.BaseConfig.ExpectedFailureReason = TEXT("none");
		FX006.FixtureId = TEXT("FX-006");
		FX006.FixtureName = TEXT("SinglePlateRotation");
		FX006.ExpectedMotionClass = TEXT("rigid_rotation");
		FX006.MotionStepCount = 1;
		FX006.DtMa = 1.0;
		FX006.bRequireNoMotionGapOrOverlap = true;
		FCarrierV2MotionSpec SingleMotion;
		SingleMotion.Axis = FVector3d(0.0, 0.0, 1.0);
		SingleMotion.AngularSpeedRadPerMa = 0.2;
		FX006.PlateMotions.Add(SingleMotion);
		Configs.Add(FX006);

		FCarrierV2Stage1Config FX009;
		FX009.BaseConfig.FixtureId = TEXT("FX-009");
		FX009.BaseConfig.FixtureName = TEXT("StableSeamBase");
		FX009.BaseConfig.SampleCount = 6;
		FX009.BaseConfig.PlateCount = 2;
		FX009.BaseConfig.FixtureKind = ECarrierV2FixtureKind::Positive;
		FX009.BaseConfig.ExpectedFailureReason = TEXT("none");
		FX009.FixtureId = TEXT("FX-009");
		FX009.FixtureName = TEXT("StableSeam");
		FX009.ExpectedMotionClass = TEXT("stable_seam");
		FX009.MotionStepCount = 2;
		FX009.DtMa = 1.0;
		FX009.bRequireNoMotionGapOrOverlap = true;
		FCarrierV2MotionSpec SeamMotion;
		SeamMotion.Axis = FVector3d(0.0, 0.0, 1.0);
		SeamMotion.AngularSpeedRadPerMa = 0.05;
		FX009.PlateMotions.Add(SeamMotion);
		FX009.PlateMotions.Add(SeamMotion);
		Configs.Add(FX009);

		FCarrierV2Stage1Config FX007;
		FX007.BaseConfig.FixtureId = TEXT("FX-007");
		FX007.BaseConfig.FixtureName = TEXT("ForcedDivergenceBase");
		FX007.BaseConfig.SampleCount = 6;
		FX007.BaseConfig.PlateCount = 2;
		FX007.BaseConfig.FixtureKind = ECarrierV2FixtureKind::Positive;
		FX007.BaseConfig.ExpectedFailureReason = TEXT("none");
		FX007.FixtureId = TEXT("FX-007");
		FX007.FixtureName = TEXT("ForcedDivergence");
		FX007.ExpectedMotionClass = TEXT("divergent_candidate");
		FX007.MotionStepCount = 1;
		FX007.DtMa = 1.0;
		FX007.bRequireDivergentCandidate = true;
		FCarrierV2MotionSpec DivergentA;
		DivergentA.Axis = FVector3d(1.0, 0.0, 0.0);
		DivergentA.AngularSpeedRadPerMa = 0.05;
		FCarrierV2MotionSpec DivergentB;
		DivergentB.Axis = FVector3d(1.0, 0.0, 0.0);
		DivergentB.AngularSpeedRadPerMa = -0.05;
		FX007.PlateMotions.Add(DivergentA);
		FX007.PlateMotions.Add(DivergentB);
		Configs.Add(FX007);

		FCarrierV2Stage1Config FX008;
		FX008.BaseConfig.FixtureId = TEXT("FX-008");
		FX008.BaseConfig.FixtureName = TEXT("ForcedConvergenceBase");
		FX008.BaseConfig.SampleCount = 6;
		FX008.BaseConfig.PlateCount = 2;
		FX008.BaseConfig.FixtureKind = ECarrierV2FixtureKind::Positive;
		FX008.BaseConfig.ExpectedFailureReason = TEXT("none");
		FX008.FixtureId = TEXT("FX-008");
		FX008.FixtureName = TEXT("ForcedConvergence");
		FX008.ExpectedMotionClass = TEXT("convergent_candidate");
		FX008.MotionStepCount = 1;
		FX008.DtMa = 1.0;
		FX008.bRequireConvergentCandidate = true;
		FCarrierV2MotionSpec ConvergentA;
		ConvergentA.Axis = FVector3d(1.0, 0.0, 0.0);
		ConvergentA.AngularSpeedRadPerMa = -0.4;
		FCarrierV2MotionSpec ConvergentB;
		ConvergentB.Axis = FVector3d(1.0, 1.0, 0.0);
		ConvergentB.AngularSpeedRadPerMa = -0.4;
		FX008.PlateMotions.Add(ConvergentA);
		FX008.PlateMotions.Add(ConvergentB);
		Configs.Add(FX008);

		FCarrierV2Stage1Config FX010;
		FX010.BaseConfig.FixtureId = TEXT("FX-010");
		FX010.BaseConfig.FixtureName = TEXT("ThirdPlateIntrusionBase");
		FX010.BaseConfig.SampleCount = 6;
		FX010.BaseConfig.PlateCount = 3;
		FX010.BaseConfig.FixtureKind = ECarrierV2FixtureKind::Positive;
		FX010.BaseConfig.ExpectedFailureReason = TEXT("none");
		FX010.FixtureId = TEXT("FX-010");
		FX010.FixtureName = TEXT("ThirdPlateIntrusion");
		FX010.ExpectedMotionClass = TEXT("third_plate_intrusion");
		FX010.MotionStepCount = 1;
		FX010.DtMa = 1.0;
		FX010.bRequireThirdPlateIntrusion = true;
		FCarrierV2MotionSpec ThirdA;
		ThirdA.Axis = FVector3d(0.0, 0.0, 1.0);
		ThirdA.AngularSpeedRadPerMa = 0.1;
		FCarrierV2MotionSpec ThirdB = ThirdA;
		FCarrierV2MotionSpec ThirdC = ThirdA;
		FX010.PlateMotions.Add(ThirdA);
		FX010.PlateMotions.Add(ThirdB);
		FX010.PlateMotions.Add(ThirdC);
		Configs.Add(FX010);

		FCarrierV2Stage1Config FX011;
		FX011.BaseConfig.FixtureId = TEXT("FX-011");
		FX011.BaseConfig.FixtureName = TEXT("PerturbedBoundarySliverBase");
		FX011.BaseConfig.SampleCount = 12;
		FX011.BaseConfig.PlateCount = 4;
		FX011.BaseConfig.FixtureKind = ECarrierV2FixtureKind::Positive;
		FX011.BaseConfig.ExpectedFailureReason = TEXT("none");
		FX011.BaseConfig.bUseFibonacciSubstrate = true;
		FX011.FixtureId = TEXT("FX-011");
		FX011.FixtureName = TEXT("PerturbedBoundarySliver");
		FX011.ExpectedMotionClass = TEXT("tolerance_stress");
		FX011.MotionStepCount = 1;
		FX011.DtMa = 1.0;
		for (int32 PlateId = 0; PlateId < FX011.BaseConfig.PlateCount; ++PlateId)
		{
			FCarrierV2MotionSpec StressMotion;
			StressMotion.Axis = FVector3d(
				PlateId == 0 ? 1.0 : 0.3 * static_cast<double>(PlateId),
				PlateId == 1 ? 1.0 : 0.2,
				PlateId == 2 ? 1.0 : 0.4).GetSafeNormal();
			StressMotion.AngularSpeedRadPerMa = (PlateId % 2 == 0 ? 1.0 : -1.0) * (0.02 + 0.005 * static_cast<double>(PlateId));
			FX011.PlateMotions.Add(StressMotion);
		}
		Configs.Add(FX011);

		FCarrierV2Stage1Config FX012;
		FX012.BaseConfig.FixtureId = TEXT("FX-012");
		FX012.BaseConfig.FixtureName = TEXT("AsymmetricOracleFrameSensitivityBase");
		FX012.BaseConfig.SampleCount = 64;
		FX012.BaseConfig.PlateCount = 3;
		FX012.BaseConfig.FixtureKind = ECarrierV2FixtureKind::Positive;
		FX012.BaseConfig.ExpectedFailureReason = TEXT("none");
		FX012.BaseConfig.FixtureSubstrateId = TEXT("fibonacci_spherical_delaunay_micro");
		FX012.BaseConfig.bUseFibonacciSubstrate = true;
		FX012.FixtureId = TEXT("FX-012");
		FX012.FixtureName = TEXT("AsymmetricOracleFrameSensitivity");
		FX012.ExpectedMotionClass = TEXT("oracle_frame_sensitivity");
		FX012.SourceStatus = TEXT("diagnostic_negative_control");
		FX012.MotionStepCount = 1;
		FX012.DtMa = 1.0;
		FX012.bRequireOracleFrameSensitivity = true;
		FCarrierV2MotionSpec FrameA;
		FrameA.Axis = FVector3d(1.0, 0.25, 0.15).GetSafeNormal();
		FrameA.AngularSpeedRadPerMa = 0.36;
		FCarrierV2MotionSpec FrameB;
		FrameB.Axis = FVector3d(-0.2, 1.0, 0.35).GetSafeNormal();
		FrameB.AngularSpeedRadPerMa = -0.28;
		FCarrierV2MotionSpec FrameC;
		FrameC.Axis = FVector3d(0.35, -0.45, 1.0).GetSafeNormal();
		FrameC.AngularSpeedRadPerMa = 0.21;
		FX012.PlateMotions.Add(FrameA);
		FX012.PlateMotions.Add(FrameB);
		FX012.PlateMotions.Add(FrameC);
		Configs.Add(FX012);

		return Configs;
	}

	FCarrierV2Stage1Config FCarrierV2Stage1::MakeScaleConfig(const int32 SampleCount, const bool bComparisonScale)
	{
		FCarrierV2Stage1Config Config;
		Config.BaseConfig = FCarrierV2Stage0::MakeScaleConfig(SampleCount, bComparisonScale);
		const FString SampleLabel = (SampleCount % 1000 == 0)
			? FString::Printf(TEXT("%dK"), SampleCount / 1000)
			: FString::Printf(TEXT("%d"), SampleCount);
		Config.BaseConfig.FixtureId = FString::Printf(TEXT("SCALE-%s-MOTION"), *SampleLabel);
		Config.BaseConfig.FixtureName = FString::Printf(
			TEXT("Scale%sRigidMotion%s"),
			*SampleLabel,
			bComparisonScale ? TEXT("Characterization") : TEXT("Gate"));
		Config.FixtureId = Config.BaseConfig.FixtureId;
		Config.FixtureName = Config.BaseConfig.FixtureName;
		Config.ExpectedMotionClass = TEXT("scale_motion");
		Config.SourceStatus = TEXT("source_explicit_motion_scale_characterization");
		Config.ProjectionCandidatePolicyId = bComparisonScale
			? TEXT("angular_cap_inverse_ray_static_aabb")
			: TEXT("angular_cap_inverse_ray_static_aabb_with_50k_all_plate_equivalence");
		Config.bUseFullTriangleScanForProjection = false;
		Config.bRunBruteforceProjectionOracle = false;
		Config.bUseAngularCapPlateBroadphase = true;
		Config.bRunAllPlateBroadphaseEquivalence = !bComparisonScale;
		Config.bRequireNoMotionGapOrOverlap = false;
		Config.bRequireDivergentCandidate = false;
		Config.bRequireConvergentCandidate = false;
		Config.MotionStepCount = 1;
		Config.DtMa = 1.0;
		Config.MotionToleranceKm = 1.0e-6;
		Config.UnitLengthTolerance = 1.0e-10;
		Config.MotionOracleToleranceKm = 1.0e-6;
		Config.ExpectedMaxStepKernelMs = 1240.0;

		Config.PlateMotions.Reset();
		for (int32 PlateId = 0; PlateId < Config.BaseConfig.PlateCount; ++PlateId)
		{
			const double Z = FMath::Clamp(-0.85 + 1.7 * (static_cast<double>(PlateId) + 0.5) / static_cast<double>(Config.BaseConfig.PlateCount), -0.95, 0.95);
			const double R = FMath::Sqrt(FMath::Max(0.0, 1.0 - Z * Z));
			const double Theta = GoldenAngle * static_cast<double>(PlateId + 3);
			FCarrierV2MotionSpec Motion;
			Motion.Axis = FVector3d(R * FMath::Cos(Theta), R * FMath::Sin(Theta), Z);
			const double Magnitude = 0.0005 + 0.00001 * static_cast<double>(PlateId % 5);
			Motion.AngularSpeedRadPerMa = (PlateId % 2 == 0) ? Magnitude : -Magnitude;
			Config.PlateMotions.Add(Motion);
		}

		return Config;
	}

	bool FCarrierV2Stage1::RunFixtureWithReplay(const FCarrierV2Stage1Config& Config, FCarrierV2Stage1FixtureResult& OutResult)
	{
		FCarrierV2Stage1FixtureResult Replay;
		const bool bPrimaryOk = RunStage1FixtureOnce(Config, OutResult);
		const bool bReplayOk = RunStage1FixtureOnce(Config, Replay);

		OutResult.Metrics.ReplayPostMotionAuthorityHash = Replay.Metrics.PostMotionAuthorityHash;
		OutResult.Metrics.ReplayProjectionOutputHash = Replay.Metrics.ProjectionOutputHash;
		OutResult.Metrics.ReplayMetricsHash = Replay.Metrics.MetricsHash;
		OutResult.Metrics.bReplayDeterministic =
			bPrimaryOk == bReplayOk &&
			OutResult.Metrics.PostMotionAuthorityHash == Replay.Metrics.PostMotionAuthorityHash &&
			OutResult.Metrics.ProjectionOutputHash == Replay.Metrics.ProjectionOutputHash &&
			OutResult.Metrics.Verdict == Replay.Metrics.Verdict &&
			OutResult.Metrics.bFixturePass == Replay.Metrics.bFixturePass;

		OutResult.Metrics.bFixturePass = OutResult.Metrics.bFixturePass && OutResult.Metrics.bReplayDeterministic;
		OutResult.Metrics.bStageGatePass = OutResult.Metrics.bFixturePass;
		if (!OutResult.Metrics.bReplayDeterministic)
		{
			OutResult.Metrics.Verdict = TEXT("FAIL_REPLAY_DETERMINISM");
			if (OutResult.Error.IsEmpty())
			{
				OutResult.Error = TEXT("Replay A/B post-motion authority, projection, or verdict differed.");
			}
		}
		OutResult.Metrics.ProjectionOutputHash = HashToString(HashStage1ProjectionMetrics(OutResult.Metrics, false));
		OutResult.Metrics.MetricsHash.Reset();
		OutResult.Metrics.MetricsHash = HashToString(HashStage1ProjectionMetrics(OutResult.Metrics, true));
		return bPrimaryOk && bReplayOk && OutResult.Metrics.bFixturePass;
	}

	FString FCarrierV2Stage1::MetricsToJson(const FCarrierV2Stage1FixtureResult& Result)
	{
		const FCarrierV2Stage1Metrics& M = Result.Metrics;
		return FString::Printf(
			TEXT("{")
			TEXT("\"run_id\":%s,")
			TEXT("\"stage_id\":%s,")
			TEXT("\"fixture_id\":%s,")
			TEXT("\"fixture_name\":%s,")
			TEXT("\"fixture_kind\":%s,")
			TEXT("\"expected_motion_class\":%s,")
			TEXT("\"source_status\":%s,")
			TEXT("\"projection_candidate_policy_id\":%s,")
			TEXT("\"sample_count\":%d,")
			TEXT("\"triangle_count\":%d,")
			TEXT("\"plate_count\":%d,")
			TEXT("\"motion_step_count\":%d,")
			TEXT("\"dt_ma\":%.12g,")
			TEXT("\"total_motion_ma\":%.12g,")
			TEXT("\"planet_radius_km\":%.12g,")
			TEXT("\"motion_tolerance_km\":%.12g,")
			TEXT("\"unit_length_tolerance\":%.12g,")
			TEXT("\"ray_epsilon\":%.12g,")
			TEXT("\"ray_parallel_tolerance\":%.12g,")
			TEXT("\"ray_t_min_tolerance\":%.12g,")
			TEXT("\"barycentric_slop\":%.12g,")
			TEXT("\"boundary_band\":%.12g,")
			TEXT("\"motion_oracle_tolerance_km\":%.12g,")
			TEXT("\"expected_max_step_kernel_ms\":%.3f,")
			TEXT("\"config_hash\":%s,")
			TEXT("\"pre_motion_authority_hash\":%s,")
			TEXT("\"post_motion_authority_hash\":%s,")
			TEXT("\"projection_output_hash\":%s,")
			TEXT("\"metrics_hash\":%s,")
			TEXT("\"global_sample_count\":%d,")
			TEXT("\"global_triangle_count\":%d,")
			TEXT("\"local_plate_vertex_count_sum\":%d,")
			TEXT("\"local_plate_triangle_count_sum\":%d,")
			TEXT("\"motion_vertex_count\":%d,")
			TEXT("\"analytic_motion_max_error_km\":%.12g,")
			TEXT("\"analytic_motion_mean_error_km\":%.12g,")
			TEXT("\"unit_length_max_error\":%.12g,")
			TEXT("\"rotated_vector_max_error\":%.12g,")
			TEXT("\"rotated_vector_count\":%d,")
			TEXT("\"material_attachment_error_count\":%d,")
			TEXT("\"raw_motion_miss_count\":%d,")
			TEXT("\"raw_motion_overlap_count\":%d,")
			TEXT("\"raw_motion_miss_fraction\":%.12g,")
			TEXT("\"raw_motion_overlap_fraction\":%.12g,")
			TEXT("\"top_miss_plate_pairs\":%s,")
			TEXT("\"top_overlap_plate_pairs\":%s,")
			TEXT("\"boundary_degenerate_count\":%d,")
			TEXT("\"boundary_policy_selected_count\":%d,")
			TEXT("\"divergent_candidate_count\":%d,")
			TEXT("\"convergent_candidate_count\":%d,")
			TEXT("\"third_plate_intrusion_count\":%d,")
			TEXT("\"material_interpolation_count\":%d,")
			TEXT("\"motion_repair_count\":%d,")
			TEXT("\"remesh_during_motion_count\":%d,")
			TEXT("\"primary_resolver_consumed_count\":%d,")
			TEXT("\"projection_reads_global_owner_count\":%d,")
			TEXT("\"tree_triangle_count_sum\":%d,")
			TEXT("\"ray_query_count\":%lld,")
			TEXT("\"aabb_hit_count_total\":%lld,")
			TEXT("\"bruteforce_hit_count_total\":%lld,")
			TEXT("\"aabb_bruteforce_classification_mismatch_count\":%d,")
			TEXT("\"legacy_moved_frame_bruteforce_mismatch_count\":%d,")
			TEXT("\"broadphase_candidate_query_count\":%lld,")
			TEXT("\"broadphase_skipped_plate_query_count\":%lld,")
			TEXT("\"all_plate_equivalence_ray_query_count\":%lld,")
			TEXT("\"broadphase_equivalence_mismatch_count\":%d,")
			TEXT("\"raw_hit_count_total\":%lld,")
			TEXT("\"ray_triangle_test_count\":%lld,")
			TEXT("\"aabb_bruteforce_equivalence_pass\":%s,")
			TEXT("\"oracle_frame_sensitivity_pass\":%s,")
			TEXT("\"broadphase_equivalence_pass\":%s,")
			TEXT("\"motion_oracle_pass\":%s,")
			TEXT("\"unit_length_pass\":%s,")
			TEXT("\"material_attachment_pass\":%s,")
			TEXT("\"performance_budget_pass\":%s,")
			TEXT("\"projection_expectation_pass\":%s,")
			TEXT("\"no_repair_or_remesh_pass\":%s,")
			TEXT("\"replay_deterministic\":%s,")
			TEXT("\"fixture_pass\":%s,")
			TEXT("\"stage_gate_pass\":%s,")
			TEXT("\"verdict\":%s,")
			TEXT("\"replay_post_motion_authority_hash\":%s,")
			TEXT("\"replay_projection_output_hash\":%s,")
			TEXT("\"replay_metrics_hash\":%s,")
			TEXT("\"build_substrate_ms\":%.3f,")
			TEXT("\"build_plate_local_ms\":%.3f,")
			TEXT("\"plate_aabb_build_ms\":%.3f,")
			TEXT("\"motion_apply_ms\":%.3f,")
			TEXT("\"inverse_ray_projection_kernel_ms\":%.3f,")
			TEXT("\"projection_kernel_ms\":%.3f,")
			TEXT("\"metrics_ms\":%.3f,")
			TEXT("\"step_kernel_ms\":%.3f,")
			TEXT("\"step_with_diagnostics_ms\":%.3f,")
			TEXT("\"total_ms\":%.3f,")
			TEXT("\"peak_memory_mb\":%.3f")
			TEXT("}"),
			*JsonString(M.RunId),
			*JsonString(M.StageId),
			*JsonString(M.FixtureId),
			*JsonString(M.FixtureName),
			*JsonString(M.FixtureKind),
			*JsonString(M.ExpectedMotionClass),
			*JsonString(M.SourceStatus),
			*JsonString(M.ProjectionCandidatePolicyId),
			M.SampleCount,
			M.TriangleCount,
			M.PlateCount,
			M.MotionStepCount,
			M.DtMa,
			M.TotalMotionMa,
			M.PlanetRadiusKm,
			M.MotionToleranceKm,
			M.UnitLengthTolerance,
			M.RayEpsilon,
			M.RayParallelTolerance,
			M.RayTMinTolerance,
			M.BarycentricSlop,
			M.BoundaryBand,
			M.MotionOracleToleranceKm,
			M.ExpectedMaxStepKernelMs,
			*JsonString(M.ConfigHash),
			*JsonString(M.PreMotionAuthorityHash),
			*JsonString(M.PostMotionAuthorityHash),
			*JsonString(M.ProjectionOutputHash),
			*JsonString(M.MetricsHash),
			M.GlobalSampleCount,
			M.GlobalTriangleCount,
			M.LocalPlateVertexCountSum,
			M.LocalPlateTriangleCountSum,
			M.MotionVertexCount,
			M.AnalyticMotionMaxErrorKm,
			M.AnalyticMotionMeanErrorKm,
			M.UnitLengthMaxError,
			M.RotatedVectorMaxError,
			M.RotatedVectorCount,
			M.MaterialAttachmentErrorCount,
			M.RawMotionMissCount,
			M.RawMotionOverlapCount,
			M.RawMotionMissFraction,
			M.RawMotionOverlapFraction,
			*JsonString(M.TopMissPlatePairs),
			*JsonString(M.TopOverlapPlatePairs),
			M.BoundaryDegenerateCount,
			M.BoundaryPolicySelectedCount,
			M.DivergentCandidateCount,
			M.ConvergentCandidateCount,
			M.ThirdPlateIntrusionCount,
			M.MaterialInterpolationCount,
			M.MotionRepairCount,
			M.RemeshDuringMotionCount,
			M.PrimaryResolverConsumedCount,
			M.ProjectionReadsGlobalOwnerCount,
			M.TreeTriangleCountSum,
			M.RayQueryCount,
			M.AabbHitCountTotal,
			M.BruteForceHitCountTotal,
			M.AabbBruteforceClassificationMismatchCount,
			M.LegacyMovedFrameBruteforceMismatchCount,
			M.BroadphaseCandidateQueryCount,
			M.BroadphaseSkippedPlateQueryCount,
			M.AllPlateEquivalenceRayQueryCount,
			M.BroadphaseEquivalenceMismatchCount,
			M.RawHitCountTotal,
			M.RayTriangleTestCount,
			M.bAabbBruteforceEquivalencePass ? TEXT("true") : TEXT("false"),
			M.bOracleFrameSensitivityPass ? TEXT("true") : TEXT("false"),
			M.bBroadphaseEquivalencePass ? TEXT("true") : TEXT("false"),
			M.bMotionOraclePass ? TEXT("true") : TEXT("false"),
			M.bUnitLengthPass ? TEXT("true") : TEXT("false"),
			M.bMaterialAttachmentPass ? TEXT("true") : TEXT("false"),
			M.bPerformanceBudgetPass ? TEXT("true") : TEXT("false"),
			M.bProjectionExpectationPass ? TEXT("true") : TEXT("false"),
			M.bNoRepairOrRemeshPass ? TEXT("true") : TEXT("false"),
			M.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			M.bFixturePass ? TEXT("true") : TEXT("false"),
			M.bStageGatePass ? TEXT("true") : TEXT("false"),
			*JsonString(M.Verdict),
			*JsonString(M.ReplayPostMotionAuthorityHash),
			*JsonString(M.ReplayProjectionOutputHash),
			*JsonString(M.ReplayMetricsHash),
			M.BuildSubstrateMs,
			M.BuildPlateLocalMs,
			M.PlateAabbBuildMs,
			M.MotionApplyMs,
			M.InverseRayProjectionKernelMs,
			M.ProjectionKernelMs,
			M.MetricsMs,
			M.StepKernelMs,
			M.StepWithDiagnosticsMs,
			M.TotalMs,
			M.PeakMemoryMb);
	}

	FString FCarrierV2Stage1::BuildCheckpointReport(
		const FCarrierV2Stage1SuiteResult& Suite,
		const FString& CommandLine,
		const FString& CommitSha)
	{
		const double Start = FPlatformTime::Seconds();
		FString Report;
		Report += TEXT("# CarrierLab Milestone 1 Closeout Report\n\n");
		Report += TEXT("Status: generated by `CarrierLabV2Stage1` for the Milestone 1 motion checkpoint.\n\n");
		Report += FString::Printf(TEXT("- Commit: `%s`\n"), CommitSha.IsEmpty() ? TEXT("unknown") : *CommitSha);
		Report += FString::Printf(TEXT("- Command: `%s`\n"), *CommandLine);
		Report += FString::Printf(TEXT("- Output root: `%s`\n"), *Suite.OutputRoot);
		Report += FString::Printf(TEXT("- Metrics JSONL: `%s`\n\n"), *Suite.MetricsPath);

		Report += TEXT("## Scope\n\n");
		Report += TEXT("Milestone 1 covers rigid plate-local authority motion, static per-plate AABB trees in plate-local coordinates, inverse-transformed center-ray projection, attached material record preservation, an independent analytic motion oracle, AABB-vs-brute-force micro equivalence, replay determinism, and raw post-motion projection classification. It does not cover remesh, gap filling, q1/q2, qGamma generation, contact resolution, subduction, collision, rifting, uplift, erosion, slab pull, editor UI, maps as gates, or ownership repair.\n\n");

		Report += TEXT("## Query Architecture\n\n");
		Report += TEXT("Each run builds one static `FDynamicMesh3` plus `FDynamicMeshAABBTree3` per plate from canonical plate-local triangles before motion. For each fixed global substrate sample, the center ray direction is inverse-rotated into candidate plate-local frames and queried against static plate trees. The brute-force micro oracle scans those same canonical plate-local triangles without the AABB tree; it does not scan already-moved vertices. Scale runs use a conservative moved angular-cap broadphase to skip impossible plate trees; the 50k scale gate compares broadphase classification against all-plate AABB classification before 250k relies on the optimized path. Raw hit sets are classified as miss, boundary-only, overlap, or third-plate intrusion without mutating authority or consuming an overlap resolver.\n\n");

		Report += TEXT("## Fixture Gates\n\n");
		Report += TEXT("| fixture | class | samples | triangles | policy | pass | verdict |\n");
		Report += TEXT("|---|---|---:|---:|---|---|---|\n");
		for (const FCarrierV2Stage1FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage1Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | %d | %d | `%s` | %s | `%s` |\n"),
				*M.FixtureId,
				*M.ExpectedMotionClass,
				M.GlobalSampleCount,
				M.GlobalTriangleCount,
				*M.ProjectionCandidatePolicyId,
				M.bFixturePass ? TEXT("pass") : TEXT("fail"),
				*M.Verdict);
		}

		Report += TEXT("\n## Tolerances\n\n");
		Report += TEXT("| fixture | ray parallel | ray t min | barycentric slop | boundary band | unit length | motion oracle km | max step kernel ms |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FCarrierV2Stage1FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage1Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %.12g | %.12g | %.12g | %.12g | %.12g | %.12g | %.3f |\n"),
				*M.FixtureId,
				M.RayParallelTolerance,
				M.RayTMinTolerance,
				M.BarycentricSlop,
				M.BoundaryBand,
				M.UnitLengthTolerance,
				M.MotionOracleToleranceKm,
				M.ExpectedMaxStepKernelMs);
		}

		Report += TEXT("\n## Motion Oracle\n\n");
		Report += TEXT("| fixture | moved vertices | max error km | mean error km | unit length max error | material attachment errors | oracle pass |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---|\n");
		for (const FCarrierV2Stage1FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage1Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %.12g | %.12g | %.12g | %d | %s |\n"),
				*M.FixtureId,
				M.MotionVertexCount,
				M.AnalyticMotionMaxErrorKm,
				M.AnalyticMotionMeanErrorKm,
				M.UnitLengthMaxError,
				M.MaterialAttachmentErrorCount,
				M.bMotionOraclePass ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Inverse-Ray Projection Classification\n\n");
		Report += TEXT("| fixture | ray queries | AABB hits | raw misses | miss % | raw overlaps | overlap % | boundary hits | third-plate | divergent | convergent | expectation |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FCarrierV2Stage1FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage1Metrics& M = Result.Metrics;
			const bool bHasProjectionExpectation =
				Result.Config.bRequireNoMotionGapOrOverlap ||
				Result.Config.bRequireDivergentCandidate ||
				Result.Config.bRequireConvergentCandidate ||
				Result.Config.bRequireThirdPlateIntrusion;
			const TCHAR* ExpectationText = bHasProjectionExpectation
				? (M.bProjectionExpectationPass ? TEXT("pass") : TEXT("fail"))
				: TEXT("n/a");
			Report += FString::Printf(
				TEXT("| `%s` | %lld | %lld | %d | %.4f | %d | %.4f | %d | %d | %d | %d | %s |\n"),
				*M.FixtureId,
				M.RayQueryCount,
				M.AabbHitCountTotal,
				M.RawMotionMissCount,
				M.RawMotionMissFraction * 100.0,
				M.RawMotionOverlapCount,
				M.RawMotionOverlapFraction * 100.0,
				M.BoundaryDegenerateCount,
				M.ThirdPlateIntrusionCount,
				M.DivergentCandidateCount,
				M.ConvergentCandidateCount,
				ExpectationText);
		}

		Report += TEXT("\n## AABB Versus Brute Force\n\n");
		Report += TEXT("| fixture | brute-force hits | brute-force triangle tests | classification mismatches | legacy moved-frame mismatches | equivalence pass | frame sensitivity |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---|---|\n");
		for (const FCarrierV2Stage1FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage1Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %lld | %lld | %d | %d | %s | %s |\n"),
				*M.FixtureId,
				M.BruteForceHitCountTotal,
				M.RayTriangleTestCount,
				M.AabbBruteforceClassificationMismatchCount,
				M.LegacyMovedFrameBruteforceMismatchCount,
				M.bAabbBruteforceEquivalencePass ? TEXT("pass") : TEXT("not-run/fail"),
				Result.Config.bRequireOracleFrameSensitivity
					? (M.bOracleFrameSensitivityPass ? TEXT("pass") : TEXT("fail"))
					: TEXT("n/a"));
		}

		Report += TEXT("\n## Miss/Overlap Attribution\n\n");
		Report += TEXT("Plate-pair attribution is diagnostic. Misses are attributed to the cold-start source-adjacent plate pair(s) around the sample; overlaps are attributed to the plate pair(s) present in the moved hit set.\n\n");
		Report += TEXT("| fixture | top miss plate pairs | top overlap plate pairs |\n");
		Report += TEXT("|---|---|---|\n");
		for (const FCarrierV2Stage1FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage1Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | `%s` |\n"),
				*M.FixtureId,
				*M.TopMissPlatePairs,
				*M.TopOverlapPlatePairs);
		}

		Report += TEXT("\n## Broadphase Equivalence\n\n");
		Report += TEXT("| fixture | broadphase candidates | broadphase skips | all-plate equivalence queries | mismatches | pass |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---|\n");
		for (const FCarrierV2Stage1FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage1Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %lld | %lld | %lld | %d | %s |\n"),
				*M.FixtureId,
				M.BroadphaseCandidateQueryCount,
				M.BroadphaseSkippedPlateQueryCount,
				M.AllPlateEquivalenceRayQueryCount,
				M.BroadphaseEquivalenceMismatchCount,
				M.bBroadphaseEquivalencePass ? TEXT("pass") : TEXT("not-run/fail"));
		}

		Report += TEXT("\n## Forbidden Fallback Counters\n\n");
		Report += TEXT("These counters are contract tripwires in the Milestone 1 path, not proof that future code cannot introduce a fallback. The closeout combines zero-valued counters with source inspection that projection takes the build state as const and never reads persistent global sample ownership as authority.\n\n");
		Report += TEXT("| fixture | global-owner reads | motion repair | remesh during motion | primary resolver consumed | material attachment errors | pass |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---|\n");
		for (const FCarrierV2Stage1FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage1Metrics& M = Result.Metrics;
			const bool bPass =
				M.ProjectionReadsGlobalOwnerCount == 0 &&
				M.MotionRepairCount == 0 &&
				M.RemeshDuringMotionCount == 0 &&
				M.PrimaryResolverConsumedCount == 0 &&
				M.MaterialAttachmentErrorCount == 0;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %d | %d | %d | %s |\n"),
				*M.FixtureId,
				M.ProjectionReadsGlobalOwnerCount,
				M.MotionRepairCount,
				M.RemeshDuringMotionCount,
				M.PrimaryResolverConsumedCount,
				M.MaterialAttachmentErrorCount,
				bPass ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Performance Ladder\n\n");
		Report += TEXT("| fixture | samples | substrate ms | plate-local ms | AABB build ms | motion ms | inverse-ray projection ms | metrics ms | step kernel ms | max step ms | budget pass | total ms | peak memory mb |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---:|\n");
		for (const FCarrierV2Stage1FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage1Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %s | %.3f | %.3f |\n"),
				*M.FixtureId,
				M.GlobalSampleCount,
				M.BuildSubstrateMs,
				M.BuildPlateLocalMs,
				M.PlateAabbBuildMs,
				M.MotionApplyMs,
				M.InverseRayProjectionKernelMs,
				M.MetricsMs,
				M.StepKernelMs,
				M.ExpectedMaxStepKernelMs,
				M.bPerformanceBudgetPass ? TEXT("pass") : TEXT("fail"),
				M.TotalMs,
				M.PeakMemoryMb);
		}

		Report += TEXT("\n## Replay A/B\n\n");
		Report += TEXT("| fixture | post-motion authority | replay authority | projection hash | replay projection hash | deterministic |\n");
		Report += TEXT("|---|---|---|---|---|---|\n");
		for (const FCarrierV2Stage1FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage1Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | `%s` | `%s` | `%s` | %s |\n"),
				*M.FixtureId,
				*M.PostMotionAuthorityHash,
				*M.ReplayPostMotionAuthorityHash,
				*M.ProjectionOutputHash,
				*M.ReplayProjectionOutputHash,
				M.bReplayDeterministic ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Policy Ledger\n\n");
		Report += TEXT("| policy | authority class | behavior | audit evidence |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += TEXT("| `M1-POL-ALL-PLATE-INVERSE-RAY` | source_explicit_query_architecture | every substrate ray is inverse-transformed into every plate-local static AABB tree for the first implementation | `projection_candidate_policy_id=all_plate_inverse_ray_static_aabb` |\n");
		Report += TEXT("| `M1-POL-ANGULAR-CAP-BROADPHASE` | diagnostic_optimization | scale runs may skip plate trees outside a moved conservative angular cap after 50k all-plate equivalence passes | `broadphase_equivalence_mismatch_count=0` |\n");
		Report += TEXT("| `M1-POL-BRUTE-FORCE-MICRO-ORACLE` | diagnostic_only | micro fixtures compare AABB classification against exhaustive canonical plate-local triangle scans in the same inverse-ray frame as the static trees | `aabb_bruteforce_classification_mismatch_count=0` |\n");
		Report += TEXT("| `M1-POL-LEGACY-MOVED-FRAME-SENTINEL` | diagnostic_negative_control | FX-012 requires the deliberately legacy moved-vertex brute-force frame to disagree at least once, showing the oracle regression would be visible | `legacy_moved_frame_bruteforce_mismatch_count>0` |\n");
		Report += TEXT("| `M1-POL-NO-REPAIR-NO-REMESH` | source_explicit_stop_condition | gaps and overlaps are counted but not repaired, resolved, or resampled | `motion_repair_count=0`, `remesh_during_motion_count=0`, `primary_resolver_consumed_count=0` |\n");
		Report += TEXT("| `V2-1-POL-INDEPENDENT-ROTATION-ORACLE` | source_explicit_formula | implementation applies incremental Rodrigues rotation plus unit normalization; oracle recomputes final position from pre-motion snapshots with axis-parallel/perpendicular decomposition | analytic error columns and unit-length column |\n\n");

		Report += TEXT("## Independent Oracle\n\n");
		Report += TEXT("Motion checks recompute expected final vertex positions from pre-motion snapshots and motion specs, not from mutated post-motion state. Material checks compare source sample ids and material records before and after motion. Projection classification reads static plate-local trees through inverse-transformed rays and never reads persistent global sample ownership as tectonic authority.\n\n");

		Report += TEXT("## Known Limitations\n\n");
		Report += TEXT("- Micro fixtures are correctness microscopes for motion and raw classification, not paper-scale terrain evidence.\n");
		Report += TEXT("- 50k is the minimum paper-scale carrier gate; 250k is a required Milestone 1 performance/comparison gate.\n");
		Report += TEXT("- 500k is a stretch gate and must be named as attempted or not attempted.\n");
		Report += TEXT("- The 1240 ms scale budget is the binding implementation gate for the 250k M1 step kernel; older planning notes rounded this to 1200 ms.\n");
		Report += TEXT("- Candidate plate selection starts from all plates; scale performance uses angular-cap broadphase only with an explicit 50k all-plate equivalence gate.\n");
		Report += TEXT("- Milestone 1 intentionally stops before resampling, q1/q2 gap fill, remesh, and process physics.\n\n");

		Report += TEXT("## Verdict\n\n");
		Report += FString::Printf(TEXT("Verdict: `%s`\n\n"), *Suite.Verdict);
		Report += FString::Printf(TEXT("- Micro gate: %s\n"), Suite.bMicroGatePass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 50k gate: %s\n"), Suite.bScale50kPass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 100k probe attempted: %s\n"), Suite.bAttempted100k ? TEXT("yes") : TEXT("no"));
		if (Suite.bAttempted100k)
		{
			Report += FString::Printf(TEXT("- 100k probe: %s\n"), Suite.bScale100kPass ? TEXT("pass") : TEXT("fail"));
		}
		Report += FString::Printf(TEXT("- 250k hard gate attempted: %s\n"), Suite.bAttempted250k ? TEXT("yes") : TEXT("no"));
		if (Suite.bAttempted250k)
		{
			Report += FString::Printf(TEXT("- 250k hard gate: %s\n"), Suite.bScale250kPass ? TEXT("pass") : TEXT("fail"));
		}
		Report += FString::Printf(TEXT("- 500k stretch attempted: %s"), Suite.bAttempted500k ? TEXT("yes") : TEXT("no"));
		if (!Suite.bAttempted500k)
		{
			Report += TEXT(" (`-Run500k` not requested)");
		}
		Report += TEXT("\n");
		if (Suite.bAttempted500k)
		{
			Report += FString::Printf(TEXT("- 500k stretch: %s\n"), Suite.bScale500kPass ? TEXT("pass") : TEXT("fail"));
			if (!Suite.bScale500kPass)
			{
				Report += TEXT("- 500k stretch note: non-blocking for the Milestone 1 verdict, but it is now a Milestone 2 performance watch because it exceeded the 1240 ms M1 250k-derived step-kernel cap.\n");
			}
		}
		Report += TEXT("\nRecommendation: prepare the Milestone 2 entry packet only if the verdict is `MILESTONE_1_PASS`; otherwise revise Milestone 1.\n");
		Report += TEXT("\nExplicit user go/no-go is required before Milestone 2 work.\n");

		const double ReportMs = (FPlatformTime::Seconds() - Start) * 1000.0;
		Report += FString::Printf(TEXT("\nReport generation ms: %.3f\n"), ReportMs);
		return Report;
	}

	TArray<FCarrierV2Stage2Config> FCarrierV2Stage2::MakeMicroFixtureConfigs()
	{
		TArray<FCarrierV2Stage2Config> Configs;
		const TArray<FCarrierV2Stage1Config> Stage1Configs = FCarrierV2Stage1::MakeMicroFixtureConfigs();

		for (const FCarrierV2Stage1Config& Stage1Config : Stage1Configs)
		{
			if (Stage1Config.FixtureId == TEXT("FX-007"))
			{
				FCarrierV2Stage2Config FX007;
				FX007.MotionConfig = Stage1Config;
				FX007.FixtureId = TEXT("FX-007");
				FX007.FixtureName = TEXT("ForcedDivergenceNoProcessDryRun");
				FX007.ExpectedProcessClass = TEXT("divergent_gap_no_process_dry_run");
				FX007.ContactPolicyId = TEXT("moved_geometry_full_scan_dry_run");
				FX007.bUseFullTriangleScanForContact = true;
				FX007.bRequireNoContactCandidates = true;
				Configs.Add(FX007);
			}
			else if (Stage1Config.FixtureId == TEXT("FX-008"))
			{
				FCarrierV2Stage2Config FX008;
				FX008.MotionConfig = Stage1Config;
				FX008.FixtureId = TEXT("FX-008");
				FX008.FixtureName = TEXT("ForcedConvergenceContactDryRun");
				FX008.ExpectedProcessClass = TEXT("convergent_contact_dry_run");
				FX008.ContactPolicyId = TEXT("moved_geometry_full_scan_dry_run");
				FX008.bUseFullTriangleScanForContact = true;
				FX008.bRequireContactCandidate = true;
				FX008.bRequirePolarityCandidate = true;
				FX008.ExpectedMinimumContactCandidates = 1;
				FX008.ExpectedMinimumPolarityCandidates = 1;
				Configs.Add(FX008);
			}
		}

		FCarrierV2Stage2Config FX009;
		FX009.MotionConfig.BaseConfig.FixtureId = TEXT("FX-009");
		FX009.MotionConfig.BaseConfig.FixtureName = TEXT("ThirdPlateIntrusionBase");
		FX009.MotionConfig.BaseConfig.SampleCount = 6;
		FX009.MotionConfig.BaseConfig.PlateCount = 3;
		FX009.MotionConfig.BaseConfig.FixtureKind = ECarrierV2FixtureKind::Positive;
		FX009.MotionConfig.BaseConfig.ExpectedFailureReason = TEXT("none");
		FX009.MotionConfig.FixtureId = TEXT("FX-009");
		FX009.MotionConfig.FixtureName = TEXT("ThirdPlateIntrusionMotion");
		FX009.MotionConfig.ExpectedMotionClass = TEXT("third_plate_intrusion_candidate");
		FX009.MotionConfig.SourceStatus = TEXT("source_explicit_contact_fixture_lab_policy");
		FX009.MotionConfig.MotionStepCount = 1;
		FX009.MotionConfig.DtMa = 1.0;
		FX009.MotionConfig.bRequireConvergentCandidate = true;
		FX009.MotionConfig.PlateMotions.Reset();
		FCarrierV2MotionSpec Plate0Motion;
		Plate0Motion.Axis = FVector3d(0.9426621464385799, -0.3175991106642196, 0.10256160381500525);
		Plate0Motion.AngularSpeedRadPerMa = 0.5923621410870896;
		FCarrierV2MotionSpec Plate1Motion;
		Plate1Motion.Axis = FVector3d(0.5899635663382427, -0.24654745832838693, -0.7688675706422232);
		Plate1Motion.AngularSpeedRadPerMa = -0.44091979709734674;
		FCarrierV2MotionSpec Plate2Motion;
		Plate2Motion.Axis = FVector3d(0.5600355667775193, 0.3593805462203835, 0.7464621805172192);
		Plate2Motion.AngularSpeedRadPerMa = 0.04244305037900342;
		FX009.MotionConfig.PlateMotions.Add(Plate0Motion);
		FX009.MotionConfig.PlateMotions.Add(Plate1Motion);
		FX009.MotionConfig.PlateMotions.Add(Plate2Motion);
		FX009.FixtureId = TEXT("FX-009");
		FX009.FixtureName = TEXT("ThirdPlateIntrusionDryRun");
		FX009.ExpectedProcessClass = TEXT("third_plate_intrusion_dry_run");
		FX009.ContactPolicyId = TEXT("moved_geometry_full_scan_dry_run");
		FX009.bUseFullTriangleScanForContact = true;
		FX009.bRequireContactCandidate = true;
		FX009.bRequireThirdPlateIntrusion = true;
		FX009.bRequirePolarityCandidate = true;
		FX009.ExpectedMinimumContactCandidates = 1;
		FX009.ExpectedMinimumThirdPlateIntrusions = 1;
		FX009.ExpectedMinimumPolarityCandidates = 1;
		Configs.Add(FX009);

		return Configs;
	}

	FCarrierV2Stage2Config FCarrierV2Stage2::MakeScaleConfig(const int32 SampleCount, const bool bComparisonScale)
	{
		FCarrierV2Stage2Config Config;
		Config.MotionConfig = FCarrierV2Stage1::MakeScaleConfig(SampleCount, bComparisonScale);
		Config.MotionConfig.BaseConfig.FixtureId = bComparisonScale ? TEXT("SCALE-250K-CONTACT") : TEXT("SCALE-50K-CONTACT");
		Config.MotionConfig.BaseConfig.FixtureName = bComparisonScale ? TEXT("Scale250kContactDryRunCharacterization") : TEXT("Scale50kContactDryRunGate");
		Config.MotionConfig.FixtureId = Config.MotionConfig.BaseConfig.FixtureId;
		Config.MotionConfig.FixtureName = Config.MotionConfig.BaseConfig.FixtureName;
		Config.MotionConfig.ExpectedMotionClass = TEXT("scale_motion_contact_readout");
		Config.FixtureId = Config.MotionConfig.FixtureId;
		Config.FixtureName = Config.MotionConfig.FixtureName;
		Config.ExpectedProcessClass = TEXT("scale_contact_dry_run");
		Config.ContactPolicyId = TEXT("moved_geometry_source_adjacent_contact_dry_run");
		Config.bUseFullTriangleScanForContact = false;
		Config.bRequireContactCandidate = true;
		Config.bRequirePolarityCandidate = true;
		Config.ExpectedMinimumContactCandidates = 1;
		Config.ExpectedMinimumPolarityCandidates = 1;
		return Config;
	}

	bool FCarrierV2Stage2::RunFixtureWithReplay(const FCarrierV2Stage2Config& Config, FCarrierV2Stage2FixtureResult& OutResult)
	{
		FCarrierV2Stage2FixtureResult Replay;
		const bool bPrimaryOk = RunStage2FixtureOnce(Config, OutResult);
		const bool bReplayOk = RunStage2FixtureOnce(Config, Replay);

		OutResult.Metrics.ReplayPostMotionAuthorityHash = Replay.Metrics.PostMotionAuthorityHash;
		OutResult.Metrics.ReplayProjectionOutputHash = Replay.Metrics.ProjectionOutputHash;
		OutResult.Metrics.ReplayProcessStateHash = Replay.Metrics.ProcessStateHash;
		OutResult.Metrics.ReplayMetricsHash = Replay.Metrics.MetricsHash;
		OutResult.Metrics.bReplayDeterministic =
			bPrimaryOk == bReplayOk &&
			OutResult.Metrics.PostMotionAuthorityHash == Replay.Metrics.PostMotionAuthorityHash &&
			OutResult.Metrics.ProjectionOutputHash == Replay.Metrics.ProjectionOutputHash &&
			OutResult.Metrics.ProcessStateHash == Replay.Metrics.ProcessStateHash &&
			OutResult.Metrics.Verdict == Replay.Metrics.Verdict &&
			OutResult.Metrics.bFixturePass == Replay.Metrics.bFixturePass;

		OutResult.Metrics.bFixturePass = OutResult.Metrics.bFixturePass && OutResult.Metrics.bReplayDeterministic;
		OutResult.Metrics.bStageGatePass = OutResult.Metrics.bFixturePass;
		if (!OutResult.Metrics.bReplayDeterministic)
		{
			OutResult.Metrics.Verdict = TEXT("FAIL_REPLAY_DETERMINISM");
			if (OutResult.Error.IsEmpty())
			{
				OutResult.Error = TEXT("Replay A/B post-motion, projection, process-state, or verdict differed.");
			}
		}
		OutResult.Metrics.MetricsHash = HashToString(HashStage2Metrics(OutResult.Metrics, true));
		return bPrimaryOk && bReplayOk && OutResult.Metrics.bFixturePass;
	}

	FString FCarrierV2Stage2::MetricsToJson(const FCarrierV2Stage2FixtureResult& Result)
	{
		const FCarrierV2Stage2Metrics& M = Result.Metrics;
		FString Json = FString::Printf(
			TEXT("{")
			TEXT("\"run_id\":%s,")
			TEXT("\"stage_id\":%s,")
			TEXT("\"fixture_id\":%s,")
			TEXT("\"fixture_name\":%s,")
			TEXT("\"fixture_kind\":%s,")
			TEXT("\"expected_motion_class\":%s,")
			TEXT("\"expected_process_class\":%s,")
			TEXT("\"source_status\":%s,")
			TEXT("\"projection_candidate_policy_id\":%s,")
			TEXT("\"contact_policy_id\":%s,")
			TEXT("\"sample_count\":%d,")
			TEXT("\"triangle_count\":%d,")
			TEXT("\"plate_count\":%d,")
			TEXT("\"motion_step_count\":%d,")
			TEXT("\"dt_ma\":%.12g,")
			TEXT("\"total_motion_ma\":%.12g,")
			TEXT("\"planet_radius_km\":%.12g,")
			TEXT("\"motion_tolerance_km\":%.12g,")
			TEXT("\"unit_length_tolerance\":%.12g,")
			TEXT("\"ray_epsilon\":%.12g,")
			TEXT("\"config_hash\":%s,")
			TEXT("\"pre_motion_authority_hash\":%s,")
			TEXT("\"post_motion_authority_hash\":%s,")
			TEXT("\"projection_output_hash\":%s,")
			TEXT("\"process_state_hash\":%s,")
			TEXT("\"metrics_hash\":%s,")
			TEXT("\"global_sample_count\":%d,")
			TEXT("\"global_triangle_count\":%d,")
			TEXT("\"local_plate_vertex_count_sum\":%d,")
			TEXT("\"local_plate_triangle_count_sum\":%d,")
			TEXT("\"motion_vertex_count\":%d,")
			TEXT("\"analytic_motion_max_error_km\":%.12g,")
			TEXT("\"analytic_motion_mean_error_km\":%.12g,")
			TEXT("\"unit_length_max_error\":%.12g,")
			TEXT("\"material_attachment_error_count\":%d,")
			TEXT("\"raw_motion_miss_count\":%d,")
			TEXT("\"raw_motion_overlap_count\":%d,")
			TEXT("\"boundary_degenerate_count\":%d,")
			TEXT("\"boundary_policy_selected_count\":%d,")
			TEXT("\"divergent_candidate_count\":%d,")
			TEXT("\"convergent_candidate_count\":%d,")
			TEXT("\"motion_repair_count\":%d,")
			TEXT("\"remesh_during_motion_count\":%d,")
			TEXT("\"projection_reads_global_owner_count\":%d,")
			TEXT("\"contact_candidate_count\":%d,")
			TEXT("\"accepted_convergence_evidence_count\":%d,")
			TEXT("\"rejected_contact_count\":%d,")
			TEXT("\"third_plate_intrusion_count\":%d,")
			TEXT("\"polarity_candidate_count\":%d,")
			TEXT("\"process_mutation_count\":%d,")
			TEXT("\"centroid_primary_resolution_count\":%d,")
			TEXT("\"random_primary_resolution_count\":%d,")
			TEXT("\"nearest_primary_resolution_count\":%d,")
			TEXT("\"projection_owner_label_evidence_count\":%d,")
			TEXT("\"overlap_consumed_before_process_state_count\":%d,")
			TEXT("\"material_mutation_count\":%d,")
			TEXT("\"remesh_during_process_dry_run_count\":%d,")
			TEXT("\"gap_fill_during_process_dry_run_count\":%d,")
			TEXT("\"raw_hit_count_total\":%lld,")
			TEXT("\"ray_triangle_test_count\":%lld,")
			TEXT("\"contact_raw_hit_count_total\":%lld,")
			TEXT("\"contact_ray_triangle_test_count\":%lld,")
			TEXT("\"motion_oracle_pass\":%s,")
			TEXT("\"unit_length_pass\":%s,")
			TEXT("\"material_attachment_pass\":%s,")
			TEXT("\"projection_expectation_pass\":%s,")
			TEXT("\"no_repair_or_remesh_pass\":%s,")
			TEXT("\"contact_evidence_pass\":%s,")
			TEXT("\"dry_run_no_mutation_pass\":%s,")
			TEXT("\"no_primary_resolver_pass\":%s,")
			TEXT("\"third_plate_pass\":%s,")
			TEXT("\"polarity_pass\":%s,")
			TEXT("\"replay_deterministic\":%s,")
			TEXT("\"fixture_pass\":%s,")
			TEXT("\"stage_gate_pass\":%s,")
			TEXT("\"verdict\":%s,")
			TEXT("\"replay_post_motion_authority_hash\":%s,")
			TEXT("\"replay_projection_output_hash\":%s,")
			TEXT("\"replay_process_state_hash\":%s,")
			TEXT("\"replay_metrics_hash\":%s,")
			TEXT("\"build_substrate_ms\":%.3f,")
			TEXT("\"build_plate_local_ms\":%.3f,")
			TEXT("\"motion_apply_ms\":%.3f,")
			TEXT("\"projection_kernel_ms\":%.3f,")
			TEXT("\"contact_detection_ms\":%.3f,")
			TEXT("\"metrics_ms\":%.3f,")
			TEXT("\"total_ms\":%.3f,")
			TEXT("\"peak_memory_mb\":%.3f,")
			TEXT("\"process_candidates\":["),
			*JsonString(M.RunId),
			*JsonString(M.StageId),
			*JsonString(M.FixtureId),
			*JsonString(M.FixtureName),
			*JsonString(M.FixtureKind),
			*JsonString(M.ExpectedMotionClass),
			*JsonString(M.ExpectedProcessClass),
			*JsonString(M.SourceStatus),
			*JsonString(M.ProjectionCandidatePolicyId),
			*JsonString(M.ContactPolicyId),
			M.SampleCount,
			M.TriangleCount,
			M.PlateCount,
			M.MotionStepCount,
			M.DtMa,
			M.TotalMotionMa,
			M.PlanetRadiusKm,
			M.MotionToleranceKm,
			M.UnitLengthTolerance,
			M.RayEpsilon,
			*JsonString(M.ConfigHash),
			*JsonString(M.PreMotionAuthorityHash),
			*JsonString(M.PostMotionAuthorityHash),
			*JsonString(M.ProjectionOutputHash),
			*JsonString(M.ProcessStateHash),
			*JsonString(M.MetricsHash),
			M.GlobalSampleCount,
			M.GlobalTriangleCount,
			M.LocalPlateVertexCountSum,
			M.LocalPlateTriangleCountSum,
			M.MotionVertexCount,
			M.AnalyticMotionMaxErrorKm,
			M.AnalyticMotionMeanErrorKm,
			M.UnitLengthMaxError,
			M.MaterialAttachmentErrorCount,
			M.RawMotionMissCount,
			M.RawMotionOverlapCount,
			M.BoundaryDegenerateCount,
			M.BoundaryPolicySelectedCount,
			M.DivergentCandidateCount,
			M.ConvergentCandidateCount,
			M.MotionRepairCount,
			M.RemeshDuringMotionCount,
			M.ProjectionReadsGlobalOwnerCount,
			M.ContactCandidateCount,
			M.AcceptedConvergenceEvidenceCount,
			M.RejectedContactCount,
			M.ThirdPlateIntrusionCount,
			M.PolarityCandidateCount,
			M.ProcessMutationCount,
			M.CentroidPrimaryResolutionCount,
			M.RandomPrimaryResolutionCount,
			M.NearestPrimaryResolutionCount,
			M.ProjectionOwnerLabelEvidenceCount,
			M.OverlapConsumedBeforeProcessStateCount,
			M.MaterialMutationCount,
			M.RemeshDuringProcessDryRunCount,
			M.GapFillDuringProcessDryRunCount,
			M.RawHitCountTotal,
			M.RayTriangleTestCount,
			M.ContactRawHitCountTotal,
			M.ContactRayTriangleTestCount,
			M.bMotionOraclePass ? TEXT("true") : TEXT("false"),
			M.bUnitLengthPass ? TEXT("true") : TEXT("false"),
			M.bMaterialAttachmentPass ? TEXT("true") : TEXT("false"),
			M.bProjectionExpectationPass ? TEXT("true") : TEXT("false"),
			M.bNoRepairOrRemeshPass ? TEXT("true") : TEXT("false"),
			M.bContactEvidencePass ? TEXT("true") : TEXT("false"),
			M.bDryRunNoMutationPass ? TEXT("true") : TEXT("false"),
			M.bNoPrimaryResolverPass ? TEXT("true") : TEXT("false"),
			M.bThirdPlatePass ? TEXT("true") : TEXT("false"),
			M.bPolarityPass ? TEXT("true") : TEXT("false"),
			M.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			M.bFixturePass ? TEXT("true") : TEXT("false"),
			M.bStageGatePass ? TEXT("true") : TEXT("false"),
			*JsonString(M.Verdict),
			*JsonString(M.ReplayPostMotionAuthorityHash),
			*JsonString(M.ReplayProjectionOutputHash),
			*JsonString(M.ReplayProcessStateHash),
			*JsonString(M.ReplayMetricsHash),
			M.BuildSubstrateMs,
			M.BuildPlateLocalMs,
			M.MotionApplyMs,
			M.ProjectionKernelMs,
			M.ContactDetectionMs,
			M.MetricsMs,
			M.TotalMs,
			M.PeakMemoryMb);

		for (int32 Index = 0; Index < Result.ProcessCandidates.Num(); ++Index)
		{
			if (Index > 0)
			{
				Json += TEXT(",");
			}
			const FCarrierV2ProcessCandidateRecord& Candidate = Result.ProcessCandidates[Index];
			Json += FString::Printf(
				TEXT("{\"candidate_id\":%d,\"sample_id\":%d,\"candidate_class\":%s,\"evidence_kind\":%s,\"plate_ids\":%s,\"local_triangle_ids\":%s,\"contact_hit_plate_ids\":%s,\"contact_hit_local_triangle_ids\":%s,\"third_plate_visible\":%s,\"polarity_candidate\":%s,\"accepted\":%s}"),
				Candidate.CandidateId,
				Candidate.SampleId,
				*JsonString(Candidate.CandidateClass),
				*JsonString(Candidate.EvidenceKind),
				*JsonIntArray(Candidate.PlateIds),
				*JsonIntArray(Candidate.LocalTriangleIds),
				*JsonIntArray(Candidate.ContactHitPlateIds),
				*JsonIntArray(Candidate.ContactHitLocalTriangleIds),
				Candidate.bThirdPlateVisible ? TEXT("true") : TEXT("false"),
				Candidate.bPolarityCandidate ? TEXT("true") : TEXT("false"),
				Candidate.bAccepted ? TEXT("true") : TEXT("false"));
		}
		Json += TEXT("]}");
		return Json;
	}

	FString FCarrierV2Stage2::BuildCheckpointReport(
		const FCarrierV2Stage2SuiteResult& Suite,
		const FString& CommandLine,
		const FString& CommitSha)
	{
		const double Start = FPlatformTime::Seconds();
		FString Report;
		Report += TEXT("# CarrierLab V2 Stage 2 Report\n\n");
		Report += TEXT("Status: generated by `CarrierLabV2Stage2`.\n\n");
		Report += FString::Printf(TEXT("- Commit: `%s`\n"), CommitSha.IsEmpty() ? TEXT("unknown") : *CommitSha);
		Report += FString::Printf(TEXT("- Command: `%s`\n"), *CommandLine);
		Report += FString::Printf(TEXT("- Output root: `%s`\n"), *Suite.OutputRoot);
		Report += FString::Printf(TEXT("- Metrics JSONL: `%s`\n\n"), *Suite.MetricsPath);

		Report += TEXT("## Scope\n\n");
		Report += TEXT("V2-2 covers contact/process dry-run evidence after rigid motion: moved-geometry multi-hit candidates, polarity candidate records, third-plate visibility, process-state hashing, and replay determinism. It does not mutate material, choose overlap winners, subduct, collide, remesh, fill gaps, repair ownership, or claim terrain generation.\n\n");

		Report += TEXT("## Fixture Gates\n\n");
		Report += TEXT("| fixture | process class | samples | triangles | policy | pass | verdict |\n");
		Report += TEXT("|---|---|---:|---:|---|---|---|\n");
		for (const FCarrierV2Stage2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage2Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | %d | %d | `%s` | %s | `%s` |\n"),
				*M.FixtureId,
				*M.ExpectedProcessClass,
				M.GlobalSampleCount,
				M.GlobalTriangleCount,
				*M.ContactPolicyId,
				M.bFixturePass ? TEXT("pass") : TEXT("fail"),
				*M.Verdict);
		}

		Report += TEXT("\n## Contact Evidence\n\n");
		Report += TEXT("| fixture | raw motion miss | raw motion overlap | contacts | accepted evidence | rejected contacts | third-plate | polarity | contact pass |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FCarrierV2Stage2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage2Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %d | %d | %d | %d | %d | %s |\n"),
				*M.FixtureId,
				M.RawMotionMissCount,
				M.RawMotionOverlapCount,
				M.ContactCandidateCount,
				M.AcceptedConvergenceEvidenceCount,
				M.RejectedContactCount,
				M.ThirdPlateIntrusionCount,
				M.PolarityCandidateCount,
				M.bContactEvidencePass ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Dry-Run Guardrails\n\n");
		Report += TEXT("| fixture | process mutations | material mutations | remeshes | gap fills | centroid resolver | random resolver | nearest resolver | owner-label evidence | guard pass |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FCarrierV2Stage2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage2Metrics& M = Result.Metrics;
			const bool bGuardPass = M.bDryRunNoMutationPass && M.bNoPrimaryResolverPass && M.ProjectionOwnerLabelEvidenceCount == 0;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %d | %d | %d | %d | %d | %d | %s |\n"),
				*M.FixtureId,
				M.ProcessMutationCount,
				M.MaterialMutationCount,
				M.RemeshDuringProcessDryRunCount,
				M.GapFillDuringProcessDryRunCount,
				M.CentroidPrimaryResolutionCount,
				M.RandomPrimaryResolutionCount,
				M.NearestPrimaryResolutionCount,
				M.ProjectionOwnerLabelEvidenceCount,
				bGuardPass ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Replay A/B\n\n");
		Report += TEXT("| fixture | post-motion authority | replay authority | projection hash | replay projection hash | process hash | replay process hash | deterministic |\n");
		Report += TEXT("|---|---|---|---|---|---|---|---|\n");
		for (const FCarrierV2Stage2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage2Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | %s |\n"),
				*M.FixtureId,
				*M.PostMotionAuthorityHash,
				*M.ReplayPostMotionAuthorityHash,
				*M.ProjectionOutputHash,
				*M.ReplayProjectionOutputHash,
				*M.ProcessStateHash,
				*M.ReplayProcessStateHash,
				M.bReplayDeterministic ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Performance Ladder\n\n");
		Report += TEXT("| fixture | samples | substrate ms | plate-local ms | motion ms | projection ms | contact ms | metrics ms | total ms | peak memory mb |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FCarrierV2Stage2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage2Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f |\n"),
				*M.FixtureId,
				M.GlobalSampleCount,
				M.BuildSubstrateMs,
				M.BuildPlateLocalMs,
				M.MotionApplyMs,
				M.ProjectionKernelMs,
				M.ContactDetectionMs,
				M.MetricsMs,
				M.TotalMs,
				M.PeakMemoryMb);
		}

		Report += TEXT("\n## Policy Ledger\n\n");
		Report += TEXT("| policy | authority class | behavior | audit evidence |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += TEXT("| `V2-2-POL-CONTACT-FULL-SCAN-MICRO` | diagnostic_only | FX-007 through FX-009 detect process candidates by exhaustive moved plate-local triangle scans | `contact_policy_id=moved_geometry_full_scan_dry_run` |\n");
		Report += TEXT("| `V2-2-POL-SOURCE-ADJACENT-SCALE-CONTACT` | source_silent_lab_policy | scale contact detection uses source-adjacent moved triangles as a performance readout, not final broadphase authority | `contact_policy_id=moved_geometry_source_adjacent_contact_dry_run` |\n");
		Report += TEXT("| `V2-2-POL-POLARITY-CANDIDATE-ONLY` | source_explicit_process_state | convergent contacts create polarity candidates but do not choose a subducting or colliding side | `polarity_candidate_count`, `process_mutation_count=0` |\n");
		Report += TEXT("| `V2-2-POL-NO-PRIMARY-RESOLVER` | source_explicit_stop_condition | raw overlap remains process evidence; centroid/random/nearest winner policies are not allowed | resolver count columns are zero |\n\n");

		Report += TEXT("## Independent Oracle\n\n");
		Report += TEXT("Motion is still checked by the V2-1 analytic oracle from pre-motion snapshots. Contact evidence is derived from moved plate-local triangle intersections and candidate records are hashed independently of replay output. The dry-run gate fails if contact evidence comes from projection owner labels or if any overlap resolver consumes the evidence before process state exists.\n\n");

		Report += TEXT("## Known Limitations\n\n");
		Report += TEXT("- V2-2 records process candidates only; it does not implement the paper's subduction, collision, material transfer, remesh, or elevation evolution.\n");
		Report += TEXT("- FX-009 is a deterministic micro fixture for third-plate visibility, not a production broadphase design.\n");
		Report += TEXT("- 50k is the minimum V2-2 scale gate; 250k is characterization unless explicitly made a hard gate.\n");
		Report += TEXT("- 500k is not attempted by this V2-2 commandlet goal.\n");
		Report += TEXT("- Scale contact detection still uses source-adjacent candidates, so it is a performance/readout sanity check rather than the final moved-surface spatial index.\n\n");

		Report += TEXT("## Verdict\n\n");
		Report += FString::Printf(TEXT("Verdict: `%s`\n\n"), *Suite.Verdict);
		Report += FString::Printf(TEXT("- Micro gate: %s\n"), Suite.bMicroGatePass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 50k gate: %s\n"), Suite.bScale50kPass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 250k characterization attempted: %s\n"), Suite.bAttempted250k ? TEXT("yes") : TEXT("no"));
		if (Suite.bAttempted250k)
		{
			Report += FString::Printf(TEXT("- 250k characterization: %s\n"), Suite.bScale250kPass ? TEXT("pass") : TEXT("fail"));
		}
		Report += TEXT("\nExplicit user go/no-go is required before V2-3 process mutation or Stage 1.5 resampling work.\n");

		const double ReportMs = (FPlatformTime::Seconds() - Start) * 1000.0;
		Report += FString::Printf(TEXT("\nReport generation ms: %.3f\n"), ReportMs);
		return Report;
	}

	TArray<FCarrierV2Stage3Config> FCarrierV2Stage3::MakeMicroFixtureConfigs()
	{
		TArray<FCarrierV2Stage3Config> Configs;
		FCarrierV2Stage2Config ConvergenceContact;
		for (const FCarrierV2Stage2Config& Stage2Config : FCarrierV2Stage2::MakeMicroFixtureConfigs())
		{
			if (Stage2Config.FixtureId == TEXT("FX-008"))
			{
				ConvergenceContact = Stage2Config;
				break;
			}
		}

		FCarrierV2Stage3Config FX010;
		FX010.ContactConfig = ConvergenceContact;
		FX010.ContactConfig.FixtureId = TEXT("FX-010");
		FX010.ContactConfig.FixtureName = TEXT("OceanOceanAgePolarityContact");
		FX010.ContactConfig.MotionConfig.BaseConfig.FixtureId = TEXT("FX-010");
		FX010.ContactConfig.MotionConfig.BaseConfig.FixtureName = TEXT("OceanOceanAgePolarityBase");
		FX010.ContactConfig.MotionConfig.FixtureId = TEXT("FX-010");
		FX010.ContactConfig.MotionConfig.FixtureName = TEXT("OceanOceanAgePolarityMotion");
		FX010.ContactConfig.ExpectedProcessClass = TEXT("ocean_ocean_age_polarity_contact");
		FX010.FixtureId = TEXT("FX-010");
		FX010.FixtureName = TEXT("OceanOceanAgePolarity");
		FX010.ExpectedProcessClass = TEXT("ocean_ocean_age_polarity");
		FX010.ProcessMutationPolicyId = TEXT("process_state_age_polarity_marks_no_remesh");
		FX010.bRequireSubductionMark = true;
		FX010.bRequireOceanicAgePolarity = true;
		FX010.bRequireProcessEvents = true;
		FX010.ExpectedMinimumSubductionEvents = 1;
		FX010.ExpectedMinimumSubductingTriangleMarks = 1;
		FX010.ExpectedMinimumProcessEvents = 1;
		AddPlateMaterialProfile(FX010, 0, ECarrierV2MaterialClass::Oceanic, 120.0, TEXT("fx_010_explicit_older_oceanic_plate"));
		AddPlateMaterialProfile(FX010, 1, ECarrierV2MaterialClass::Oceanic, 20.0, TEXT("fx_010_explicit_younger_oceanic_plate"));
		Configs.Add(FX010);

		FCarrierV2Stage3Config FX011;
		FX011.ContactConfig = ConvergenceContact;
		FX011.ContactConfig.FixtureId = TEXT("FX-011");
		FX011.ContactConfig.FixtureName = TEXT("ContinentalCollisionCandidateContact");
		FX011.ContactConfig.MotionConfig.BaseConfig.FixtureId = TEXT("FX-011");
		FX011.ContactConfig.MotionConfig.BaseConfig.FixtureName = TEXT("ContinentalCollisionCandidateBase");
		FX011.ContactConfig.MotionConfig.FixtureId = TEXT("FX-011");
		FX011.ContactConfig.MotionConfig.FixtureName = TEXT("ContinentalCollisionCandidateMotion");
		FX011.ContactConfig.ExpectedProcessClass = TEXT("continental_collision_candidate_contact");
		FX011.FixtureId = TEXT("FX-011");
		FX011.FixtureName = TEXT("ContinentalCollisionCandidate");
		FX011.ExpectedProcessClass = TEXT("continental_collision_candidate");
		FX011.ProcessMutationPolicyId = TEXT("process_state_collision_suture_candidates_no_topology_transfer");
		FX011.bRequireCollisionCandidate = true;
		FX011.bRequireProcessEvents = true;
		FX011.ExpectedMinimumCollisionCandidates = 1;
		FX011.ExpectedMinimumCollisionEvents = 1;
		FX011.ExpectedMinimumProcessEvents = 1;
		AddPlateMaterialProfile(FX011, 0, ECarrierV2MaterialClass::Continental, 0.0, TEXT("fx_011_explicit_continental_plate_a"));
		AddPlateMaterialProfile(FX011, 1, ECarrierV2MaterialClass::Continental, 0.0, TEXT("fx_011_explicit_continental_plate_b"));
		Configs.Add(FX011);

		return Configs;
	}

	FCarrierV2Stage3Config FCarrierV2Stage3::MakeScaleConfig(const int32 SampleCount, const bool bComparisonScale)
	{
		FCarrierV2Stage3Config Config;
		Config.ContactConfig = FCarrierV2Stage2::MakeScaleConfig(SampleCount, bComparisonScale);
		Config.ContactConfig.FixtureId = bComparisonScale ? TEXT("SCALE-250K-PROCESS") : TEXT("SCALE-50K-PROCESS");
		Config.ContactConfig.FixtureName = bComparisonScale ? TEXT("Scale250kProcessMutationCharacterization") : TEXT("Scale50kProcessMutationGate");
		Config.ContactConfig.MotionConfig.BaseConfig.FixtureId = Config.ContactConfig.FixtureId;
		Config.ContactConfig.MotionConfig.BaseConfig.FixtureName = Config.ContactConfig.FixtureName;
		Config.ContactConfig.MotionConfig.FixtureId = Config.ContactConfig.FixtureId;
		Config.ContactConfig.MotionConfig.FixtureName = Config.ContactConfig.FixtureName;
		Config.ContactConfig.ExpectedProcessClass = TEXT("scale_process_candidate_contact");
		Config.FixtureId = Config.ContactConfig.FixtureId;
		Config.FixtureName = Config.ContactConfig.FixtureName;
		Config.ExpectedProcessClass = TEXT("scale_process_mutation_fixture");
		Config.ProcessMutationPolicyId = TEXT("deterministic_plate_profile_process_marks_no_remesh");
		Config.bRequireSubductionMark = true;
		Config.bRequireCollisionCandidate = true;
		Config.bRequireProcessEvents = true;
		Config.ExpectedMinimumSubductionEvents = 1;
		Config.ExpectedMinimumSubductingTriangleMarks = 1;
		Config.ExpectedMinimumCollisionCandidates = 1;
		Config.ExpectedMinimumCollisionEvents = 1;
		Config.ExpectedMinimumProcessEvents = 1;
		for (int32 PlateId = 0; PlateId < Config.ContactConfig.MotionConfig.BaseConfig.PlateCount; ++PlateId)
		{
			Config.PlateMaterialProfiles.Add(MakeDefaultStage3PlateProfile(PlateId));
		}
		return Config;
	}

	bool FCarrierV2Stage3::RunFixtureWithReplay(const FCarrierV2Stage3Config& Config, FCarrierV2Stage3FixtureResult& OutResult)
	{
		FCarrierV2Stage3FixtureResult Replay;
		const bool bPrimaryOk = RunStage3FixtureOnce(Config, OutResult);
		const bool bReplayOk = RunStage3FixtureOnce(Config, Replay);

		OutResult.Metrics.ReplayStage2ProcessStateHash = Replay.Metrics.Stage2ProcessStateHash;
		OutResult.Metrics.ReplayProcessStateHash = Replay.Metrics.ProcessStateHash;
		OutResult.Metrics.ReplayMetricsHash = Replay.Metrics.MetricsHash;
		OutResult.Metrics.bReplayDeterministic =
			bPrimaryOk == bReplayOk &&
			OutResult.Metrics.Stage2ProcessStateHash == Replay.Metrics.Stage2ProcessStateHash &&
			OutResult.Metrics.ProcessStateHash == Replay.Metrics.ProcessStateHash &&
			OutResult.Metrics.Verdict == Replay.Metrics.Verdict &&
			OutResult.Metrics.bFixturePass == Replay.Metrics.bFixturePass;

		OutResult.Metrics.bFixturePass = OutResult.Metrics.bFixturePass && OutResult.Metrics.bReplayDeterministic;
		OutResult.Metrics.bStageGatePass = OutResult.Metrics.bFixturePass;
		if (!OutResult.Metrics.bReplayDeterministic)
		{
			OutResult.Metrics.Verdict = TEXT("FAIL_REPLAY_DETERMINISM");
			if (OutResult.Error.IsEmpty())
			{
				OutResult.Error = TEXT("Replay A/B stage2-process, process-state, verdict, or fixture pass differed.");
			}
		}
		OutResult.Metrics.MetricsHash = HashToString(HashStage3Metrics(OutResult.Metrics, true));
		return bPrimaryOk && bReplayOk && OutResult.Metrics.bFixturePass;
	}

	FString FCarrierV2Stage3::MetricsToJson(const FCarrierV2Stage3FixtureResult& Result)
	{
		const FCarrierV2Stage3Metrics& M = Result.Metrics;
		FString Json = FString::Printf(
			TEXT("{")
			TEXT("\"run_id\":%s,")
			TEXT("\"stage_id\":%s,")
			TEXT("\"fixture_id\":%s,")
			TEXT("\"fixture_name\":%s,")
			TEXT("\"fixture_kind\":%s,")
			TEXT("\"expected_motion_class\":%s,")
			TEXT("\"expected_process_class\":%s,")
			TEXT("\"source_status\":%s,")
			TEXT("\"projection_candidate_policy_id\":%s,")
			TEXT("\"contact_policy_id\":%s,")
			TEXT("\"process_mutation_policy_id\":%s,")
			TEXT("\"sample_count\":%d,")
			TEXT("\"triangle_count\":%d,")
			TEXT("\"plate_count\":%d,")
			TEXT("\"motion_step_count\":%d,")
			TEXT("\"dt_ma\":%.12g,")
			TEXT("\"total_motion_ma\":%.12g,")
			TEXT("\"planet_radius_km\":%.12g,")
			TEXT("\"motion_tolerance_km\":%.12g,")
			TEXT("\"unit_length_tolerance\":%.12g,")
			TEXT("\"ray_epsilon\":%.12g,")
			TEXT("\"config_hash\":%s,")
			TEXT("\"stage2_process_state_hash\":%s,")
			TEXT("\"process_state_hash\":%s,")
			TEXT("\"metrics_hash\":%s,")
			TEXT("\"global_sample_count\":%d,")
			TEXT("\"global_triangle_count\":%d,")
			TEXT("\"local_plate_vertex_count_sum\":%d,")
			TEXT("\"local_plate_triangle_count_sum\":%d,")
			TEXT("\"motion_vertex_count\":%d,")
			TEXT("\"analytic_motion_max_error_km\":%.12g,")
			TEXT("\"analytic_motion_mean_error_km\":%.12g,")
			TEXT("\"unit_length_max_error\":%.12g,")
			TEXT("\"material_attachment_error_count\":%d,")
			TEXT("\"raw_motion_miss_count\":%d,")
			TEXT("\"raw_motion_overlap_count\":%d,")
			TEXT("\"divergent_candidate_count\":%d,")
			TEXT("\"convergent_candidate_count\":%d,")
			TEXT("\"contact_candidate_count\":%d,")
			TEXT("\"accepted_convergence_evidence_count\":%d,")
			TEXT("\"third_plate_intrusion_count\":%d,")
			TEXT("\"polarity_candidate_count\":%d,")
			TEXT("\"third_plate_passthrough_count\":%d,")
			TEXT("\"process_event_count\":%d,")
			TEXT("\"process_event_with_provenance_count\":%d,")
			TEXT("\"process_event_without_provenance_count\":%d,")
			TEXT("\"subduction_event_count\":%d,")
			TEXT("\"oceanic_age_polarity_decision_count\":%d,")
			TEXT("\"older_oceanic_subducting_pass_count\":%d,")
			TEXT("\"subducting_triangle_mark_count\":%d,")
			TEXT("\"collision_candidate_count\":%d,")
			TEXT("\"collision_event_count\":%d,")
			TEXT("\"terrane_detach_triangle_count\":%d,")
			TEXT("\"terrane_suture_triangle_count\":%d,")
			TEXT("\"material_destroyed_without_process_count\":%d,")
			TEXT("\"material_created_without_process_count\":%d,")
			TEXT("\"topology_mutation_without_event_count\":%d,")
			TEXT("\"material_mutation_count\":%d,")
			TEXT("\"remesh_during_process_mutation_count\":%d,")
			TEXT("\"topology_rebuild_during_process_mutation_count\":%d,")
			TEXT("\"gap_fill_during_process_mutation_count\":%d,")
			TEXT("\"divergent_generation_during_process_mutation_count\":%d,")
			TEXT("\"terrain_beauty_mutation_count\":%d,")
			TEXT("\"ownership_repair_during_process_mutation_count\":%d,")
			TEXT("\"centroid_primary_resolution_count\":%d,")
			TEXT("\"random_primary_resolution_count\":%d,")
			TEXT("\"nearest_primary_resolution_count\":%d,")
			TEXT("\"projection_owner_label_evidence_count\":%d,")
			TEXT("\"overlap_consumed_before_process_state_count\":%d,")
			TEXT("\"slab_pull_enabled\":%s,")
			TEXT("\"slab_pull_axis_delta_deg\":%.12g,")
			TEXT("\"inherited_stage2_pass\":%s,")
			TEXT("\"motion_oracle_pass\":%s,")
			TEXT("\"unit_length_pass\":%s,")
			TEXT("\"material_attachment_pass\":%s,")
			TEXT("\"projection_expectation_pass\":%s,")
			TEXT("\"contact_evidence_pass\":%s,")
			TEXT("\"process_event_provenance_pass\":%s,")
			TEXT("\"subduction_polarity_pass\":%s,")
			TEXT("\"collision_candidate_pass\":%s,")
			TEXT("\"process_event_expectation_pass\":%s,")
			TEXT("\"no_forbidden_mutation_pass\":%s,")
			TEXT("\"slab_pull_pass\":%s,")
			TEXT("\"replay_deterministic\":%s,")
			TEXT("\"fixture_pass\":%s,")
			TEXT("\"stage_gate_pass\":%s,")
			TEXT("\"verdict\":%s,")
			TEXT("\"replay_stage2_process_state_hash\":%s,")
			TEXT("\"replay_process_state_hash\":%s,")
			TEXT("\"replay_metrics_hash\":%s,")
			TEXT("\"build_substrate_ms\":%.3f,")
			TEXT("\"build_plate_local_ms\":%.3f,")
			TEXT("\"motion_apply_ms\":%.3f,")
			TEXT("\"projection_kernel_ms\":%.3f,")
			TEXT("\"contact_detection_ms\":%.3f,")
			TEXT("\"process_mutation_ms\":%.3f,")
			TEXT("\"metrics_ms\":%.3f,")
			TEXT("\"total_ms\":%.3f,")
			TEXT("\"peak_memory_mb\":%.3f,")
			TEXT("\"process_events\":["),
			*JsonString(M.RunId),
			*JsonString(M.StageId),
			*JsonString(M.FixtureId),
			*JsonString(M.FixtureName),
			*JsonString(M.FixtureKind),
			*JsonString(M.ExpectedMotionClass),
			*JsonString(M.ExpectedProcessClass),
			*JsonString(M.SourceStatus),
			*JsonString(M.ProjectionCandidatePolicyId),
			*JsonString(M.ContactPolicyId),
			*JsonString(M.ProcessMutationPolicyId),
			M.SampleCount,
			M.TriangleCount,
			M.PlateCount,
			M.MotionStepCount,
			M.DtMa,
			M.TotalMotionMa,
			M.PlanetRadiusKm,
			M.MotionToleranceKm,
			M.UnitLengthTolerance,
			M.RayEpsilon,
			*JsonString(M.ConfigHash),
			*JsonString(M.Stage2ProcessStateHash),
			*JsonString(M.ProcessStateHash),
			*JsonString(M.MetricsHash),
			M.GlobalSampleCount,
			M.GlobalTriangleCount,
			M.LocalPlateVertexCountSum,
			M.LocalPlateTriangleCountSum,
			M.MotionVertexCount,
			M.AnalyticMotionMaxErrorKm,
			M.AnalyticMotionMeanErrorKm,
			M.UnitLengthMaxError,
			M.MaterialAttachmentErrorCount,
			M.RawMotionMissCount,
			M.RawMotionOverlapCount,
			M.DivergentCandidateCount,
			M.ConvergentCandidateCount,
			M.ContactCandidateCount,
			M.AcceptedConvergenceEvidenceCount,
			M.ThirdPlateIntrusionCount,
			M.PolarityCandidateCount,
			M.ThirdPlatePassthroughCount,
			M.ProcessEventCount,
			M.ProcessEventWithProvenanceCount,
			M.ProcessEventWithoutProvenanceCount,
			M.SubductionEventCount,
			M.OceanicAgePolarityDecisionCount,
			M.OlderOceanicSubductingPassCount,
			M.SubductingTriangleMarkCount,
			M.CollisionCandidateCount,
			M.CollisionEventCount,
			M.TerraneDetachTriangleCount,
			M.TerraneSutureTriangleCount,
			M.MaterialDestroyedWithoutProcessCount,
			M.MaterialCreatedWithoutProcessCount,
			M.TopologyMutationWithoutEventCount,
			M.MaterialMutationCount,
			M.RemeshDuringProcessMutationCount,
			M.TopologyRebuildDuringProcessMutationCount,
			M.GapFillDuringProcessMutationCount,
			M.DivergentGenerationDuringProcessMutationCount,
			M.TerrainBeautyMutationCount,
			M.OwnershipRepairDuringProcessMutationCount,
			M.CentroidPrimaryResolutionCount,
			M.RandomPrimaryResolutionCount,
			M.NearestPrimaryResolutionCount,
			M.ProjectionOwnerLabelEvidenceCount,
			M.OverlapConsumedBeforeProcessStateCount,
			M.bSlabPullEnabled ? TEXT("true") : TEXT("false"),
			M.SlabPullAxisDeltaDeg,
			M.bInheritedStage2Pass ? TEXT("true") : TEXT("false"),
			M.bMotionOraclePass ? TEXT("true") : TEXT("false"),
			M.bUnitLengthPass ? TEXT("true") : TEXT("false"),
			M.bMaterialAttachmentPass ? TEXT("true") : TEXT("false"),
			M.bProjectionExpectationPass ? TEXT("true") : TEXT("false"),
			M.bContactEvidencePass ? TEXT("true") : TEXT("false"),
			M.bProcessEventProvenancePass ? TEXT("true") : TEXT("false"),
			M.bSubductionPolarityPass ? TEXT("true") : TEXT("false"),
			M.bCollisionCandidatePass ? TEXT("true") : TEXT("false"),
			M.bProcessEventExpectationPass ? TEXT("true") : TEXT("false"),
			M.bNoForbiddenMutationPass ? TEXT("true") : TEXT("false"),
			M.bSlabPullPass ? TEXT("true") : TEXT("false"),
			M.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			M.bFixturePass ? TEXT("true") : TEXT("false"),
			M.bStageGatePass ? TEXT("true") : TEXT("false"),
			*JsonString(M.Verdict),
			*JsonString(M.ReplayStage2ProcessStateHash),
			*JsonString(M.ReplayProcessStateHash),
			*JsonString(M.ReplayMetricsHash),
			M.BuildSubstrateMs,
			M.BuildPlateLocalMs,
			M.MotionApplyMs,
			M.ProjectionKernelMs,
			M.ContactDetectionMs,
			M.ProcessMutationMs,
			M.MetricsMs,
			M.TotalMs,
			M.PeakMemoryMb);

		for (int32 Index = 0; Index < Result.ProcessEvents.Num(); ++Index)
		{
			if (Index > 0)
			{
				Json += TEXT(",");
			}
			const FCarrierV2ProcessEventRecord& Event = Result.ProcessEvents[Index];
			Json += FString::Printf(
				TEXT("{\"event_id\":%d,\"source_candidate_id\":%d,\"sample_id\":%d,\"event_class\":%s,\"process_class\":%s,\"source_plate_id\":%d,\"destination_plate_id\":%d,\"source_local_triangle_id\":%d,\"destination_local_triangle_id\":%d,\"subducting_plate_id\":%d,\"overriding_plate_id\":%d,\"subducting_local_triangle_id\":%d,\"source_material_class\":%s,\"destination_material_class\":%s,\"source_oceanic_age_ma\":%.12g,\"destination_oceanic_age_ma\":%.12g,\"older_oceanic_subducts\":%s,\"has_provenance\":%s,\"provenance\":%s}"),
				Event.EventId,
				Event.SourceCandidateId,
				Event.SampleId,
				*JsonString(Event.EventClass),
				*JsonString(Event.ProcessClass),
				Event.SourcePlateId,
				Event.DestinationPlateId,
				Event.SourceLocalTriangleId,
				Event.DestinationLocalTriangleId,
				Event.SubductingPlateId,
				Event.OverridingPlateId,
				Event.SubductingLocalTriangleId,
				*JsonString(MaterialClassName(Event.SourceMaterialClass)),
				*JsonString(MaterialClassName(Event.DestinationMaterialClass)),
				Event.SourceOceanicAgeMa,
				Event.DestinationOceanicAgeMa,
				Event.bOlderOceanicSubducts ? TEXT("true") : TEXT("false"),
				Event.bHasProvenance ? TEXT("true") : TEXT("false"),
				*JsonString(Event.Provenance));
		}
		Json += TEXT("],\"triangle_marks\":[");
		for (int32 Index = 0; Index < Result.TriangleMarks.Num(); ++Index)
		{
			if (Index > 0)
			{
				Json += TEXT(",");
			}
			const FCarrierV2TriangleProcessMarkRecord& Mark = Result.TriangleMarks[Index];
			Json += FString::Printf(
				TEXT("{\"mark_id\":%d,\"event_id\":%d,\"source_candidate_id\":%d,\"sample_id\":%d,\"plate_id\":%d,\"local_triangle_id\":%d,\"mark_class\":%s,\"provenance\":%s}"),
				Mark.MarkId,
				Mark.EventId,
				Mark.SourceCandidateId,
				Mark.SampleId,
				Mark.PlateId,
				Mark.LocalTriangleId,
				*JsonString(Mark.MarkClass),
				*JsonString(Mark.Provenance));
		}
		Json += TEXT("]}");
		return Json;
	}

	FString FCarrierV2Stage3::BuildCheckpointReport(
		const FCarrierV2Stage3SuiteResult& Suite,
		const FString& CommandLine,
		const FString& CommitSha)
	{
		const double Start = FPlatformTime::Seconds();
		FString Report;
		Report += TEXT("# CarrierLab V2 Stage 3 Report\n\n");
		Report += TEXT("Status: generated by `CarrierLabV2Stage3`.\n\n");
		Report += FString::Printf(TEXT("- Commit: `%s`\n"), CommitSha.IsEmpty() ? TEXT("unknown") : *CommitSha);
		Report += FString::Printf(TEXT("- Command: `%s`\n"), *CommandLine);
		Report += FString::Printf(TEXT("- Output root: `%s`\n"), *Suite.OutputRoot);
		Report += FString::Printf(TEXT("- Metrics JSONL: `%s`\n\n"), *Suite.MetricsPath);

		Report += TEXT("## Scope\n\n");
		Report += TEXT("V2-3 consumes V2-2 accepted convergence candidates and records process-state events before any remesh. It covers ocean-ocean age polarity, subducting triangle marks, continental collision/suture candidate records, process provenance, replay-stable process hashes, a 50k gate, and optional 250k characterization. It does not rebuild topology, remesh, fill gaps, generate divergent crust, transfer terrain, mutate elevation, repair ownership, choose overlap winners, or implement slab pull.\n\n");

		Report += TEXT("## Fixture Gates\n\n");
		Report += TEXT("| fixture | process class | samples | contacts | events | subduction marks | collision events | pass | verdict |\n");
		Report += TEXT("|---|---|---:|---:|---:|---:|---:|---|---|\n");
		for (const FCarrierV2Stage3FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage3Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | %d | %d | %d | %d | %d | %s | `%s` |\n"),
				*M.FixtureId,
				*M.ExpectedProcessClass,
				M.GlobalSampleCount,
				M.ContactCandidateCount,
				M.ProcessEventCount,
				M.SubductingTriangleMarkCount,
				M.CollisionEventCount,
				M.bFixturePass ? TEXT("pass") : TEXT("fail"),
				*M.Verdict);
		}

		Report += TEXT("\n## Process State Evidence\n\n");
		Report += TEXT("| fixture | age polarity decisions | older oceanic subducted | subduction events | collision candidates | terrane detach marks | suture marks | third-plate passthrough |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FCarrierV2Stage3FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage3Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %d | %d | %d | %d | %d |\n"),
				*M.FixtureId,
				M.OceanicAgePolarityDecisionCount,
				M.OlderOceanicSubductingPassCount,
				M.SubductionEventCount,
				M.CollisionCandidateCount,
				M.TerraneDetachTriangleCount,
				M.TerraneSutureTriangleCount,
				M.ThirdPlatePassthroughCount);
		}

		Report += TEXT("\n## Hard Guards\n\n");
		Report += TEXT("| fixture | provenance holes | material no-process destroy | material no-process create | topology no-event | material mutations | remesh | rebuild | gap fill | divergent generation | terrain beauty | ownership repair | resolvers | guard pass |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FCarrierV2Stage3FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage3Metrics& M = Result.Metrics;
			const int32 ResolverTotal = M.CentroidPrimaryResolutionCount + M.RandomPrimaryResolutionCount + M.NearestPrimaryResolutionCount;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %s |\n"),
				*M.FixtureId,
				M.ProcessEventWithoutProvenanceCount,
				M.MaterialDestroyedWithoutProcessCount,
				M.MaterialCreatedWithoutProcessCount,
				M.TopologyMutationWithoutEventCount,
				M.MaterialMutationCount,
				M.RemeshDuringProcessMutationCount,
				M.TopologyRebuildDuringProcessMutationCount,
				M.GapFillDuringProcessMutationCount,
				M.DivergentGenerationDuringProcessMutationCount,
				M.TerrainBeautyMutationCount,
				M.OwnershipRepairDuringProcessMutationCount,
				ResolverTotal,
				M.bNoForbiddenMutationPass ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Replay A/B\n\n");
		Report += TEXT("| fixture | stage2 process hash | replay stage2 hash | v2-3 process hash | replay v2-3 hash | deterministic |\n");
		Report += TEXT("|---|---|---|---|---|---|\n");
		for (const FCarrierV2Stage3FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage3Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | `%s` | `%s` | `%s` | %s |\n"),
				*M.FixtureId,
				*M.Stage2ProcessStateHash,
				*M.ReplayStage2ProcessStateHash,
				*M.ProcessStateHash,
				*M.ReplayProcessStateHash,
				M.bReplayDeterministic ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Performance Ladder\n\n");
		Report += TEXT("| fixture | samples | substrate ms | plate-local ms | motion ms | projection ms | contact ms | process ms | total ms | peak memory mb |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FCarrierV2Stage3FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage3Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f |\n"),
				*M.FixtureId,
				M.GlobalSampleCount,
				M.BuildSubstrateMs,
				M.BuildPlateLocalMs,
				M.MotionApplyMs,
				M.ProjectionKernelMs,
				M.ContactDetectionMs,
				M.ProcessMutationMs,
				M.TotalMs,
				M.PeakMemoryMb);
		}

		Report += TEXT("\n## Policy Ledger\n\n");
		Report += TEXT("| policy | authority class | behavior | audit evidence |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += TEXT("| `V2-3-POL-PLATE-MATERIAL-PROFILES` | fixture_input_lab_policy | micro and scale fixtures use explicit plate material profiles so process polarity does not read global owner labels | `process_events`, `plate_id`, material class and age fields |\n");
		Report += TEXT("| `V2-3-POL-OLDER-OCEANIC-SUBDUCTS` | fixture_oracle | FX-010 requires the older oceanic profile to receive the subducting triangle mark | `older_oceanic_subducting_pass_count` |\n");
		Report += TEXT("| `V2-3-POL-COLLISION-AS-EVENT-NOT-REPAIR` | source_explicit_process_state | FX-011 records continental collision and suture candidates without topology transfer | `collision_event_count`, zero topology mutation counters |\n");
		Report += TEXT("| `V2-3-POL-SLAB-PULL-BYPASS` | source_explicit_deferred_feedback | slab pull remains disabled and must report zero axis delta | `slab_pull_enabled=false`, `slab_pull_axis_delta_deg=0` |\n\n");

		Report += TEXT("## Independent Oracle\n\n");
		Report += TEXT("V2-3 does not compare process output against itself. V2-2 independently supplies accepted moved-geometry contact candidates. V2-3 then applies fixture plate material profiles: oceanic-oceanic contacts mark the older oceanic side as subducting, mixed oceanic/continental contacts mark the oceanic side as subducting, and continental-continental contacts create collision/suture candidate events. Replay A/B must reproduce both the inherited V2-2 process hash and the V2-3 event/mark process hash.\n\n");

		Report += TEXT("## Known Limitations\n\n");
		Report += TEXT("- V2-3 records process state only; it does not perform Stage 1.5/V2-4 remesh filtering.\n");
		Report += TEXT("- Continental collision records are candidate/event ledgers, not completed terrane topology transfer.\n");
		Report += TEXT("- Slab pull is explicitly bypassed in this slice.\n");
		Report += TEXT("- Scale fixtures still inherit V2-2 source-adjacent contact detection, so they are performance and process-state gates, not the final spatial index.\n");
		Report += TEXT("- 500k is not attempted by this V2-3 commandlet goal.\n\n");

		Report += TEXT("## Verdict\n\n");
		Report += FString::Printf(TEXT("Verdict: `%s`\n\n"), *Suite.Verdict);
		Report += FString::Printf(TEXT("- Micro gate: %s\n"), Suite.bMicroGatePass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 50k gate: %s\n"), Suite.bScale50kPass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 250k characterization attempted: %s\n"), Suite.bAttempted250k ? TEXT("yes") : TEXT("no"));
		if (Suite.bAttempted250k)
		{
			Report += FString::Printf(TEXT("- 250k characterization: %s\n"), Suite.bScale250kPass ? TEXT("pass") : TEXT("fail"));
		}
		Report += TEXT("\nExplicit user go/no-go is required before V2-4 filtered overlap remesh / Stage 1.5 resampling integration work.\n");

		const double ReportMs = (FPlatformTime::Seconds() - Start) * 1000.0;
		Report += FString::Printf(TEXT("\nReport generation ms: %.3f\n"), ReportMs);
		return Report;
	}

	TArray<FCarrierV2Stage4Config> FCarrierV2Stage4::MakeMicroFixtureConfigs()
	{
		TArray<FCarrierV2Stage4Config> Configs;
		FCarrierV2Stage3Config SubductionProcess;
		FCarrierV2Stage3Config CollisionProcess;
		for (const FCarrierV2Stage3Config& Stage3Config : FCarrierV2Stage3::MakeMicroFixtureConfigs())
		{
			if (Stage3Config.FixtureId == TEXT("FX-010"))
			{
				SubductionProcess = Stage3Config;
			}
			else if (Stage3Config.FixtureId == TEXT("FX-011"))
			{
				CollisionProcess = Stage3Config;
			}
		}

		FCarrierV2Stage4Config FX012;
		FX012.ProcessConfig = SubductionProcess;
		FX012.FixtureId = TEXT("FX-012");
		FX012.FixtureName = TEXT("FilteredSubductionOverlapSampling");
		FX012.ExpectedRemeshClass = TEXT("global_tds_filtered_subduction_overlap");
		FX012.RemeshSamplingPolicyId = TEXT("global_tds_aabb_all_hit_filter_subducting_no_q1q2");
		FX012.bRequireFilteredSubductingHit = true;
		FX012.bRequireValidHitAfterFilter = true;
		FX012.ExpectedMinimumFilteredSubductingHits = 1;
		FX012.ExpectedMinimumValidHits = 1;
		Configs.Add(FX012);

		FCarrierV2Stage4Config FX013;
		FX013.ProcessConfig = CollisionProcess;
		FX013.FixtureId = TEXT("FX-013");
		FX013.FixtureName = TEXT("FilteredCollisionGapLedger");
		FX013.ExpectedRemeshClass = TEXT("global_tds_filtered_collision_gap_ledger");
		FX013.RemeshSamplingPolicyId = TEXT("global_tds_aabb_all_hit_filter_colliding_no_q1q2");
		FX013.bRequireFilteredCollidingHit = true;
		FX013.bRequireValidHitAfterFilter = true;
		FX013.ExpectedMinimumFilteredCollidingHits = 1;
		FX013.ExpectedMinimumValidHits = 1;
		Configs.Add(FX013);

		return Configs;
	}

	FCarrierV2Stage4Config FCarrierV2Stage4::MakeScaleConfig(const int32 SampleCount, const bool bComparisonScale)
	{
		FCarrierV2Stage4Config Config;
		Config.ProcessConfig = FCarrierV2Stage3::MakeScaleConfig(SampleCount, bComparisonScale);
		Config.FixtureId = bComparisonScale ? TEXT("SCALE-250K-FILTERED-SAMPLING") : TEXT("SCALE-50K-FILTERED-SAMPLING");
		Config.FixtureName = bComparisonScale ? TEXT("Scale250kFilteredGlobalSamplingCharacterization") : TEXT("Scale50kFilteredGlobalSamplingGate");
		Config.ExpectedRemeshClass = TEXT("scale_global_tds_filtered_sampling");
		Config.RemeshSamplingPolicyId = TEXT("global_tds_aabb_all_hit_filter_scale_no_q1q2");
		Config.bRequireFilteredSubductingHit = true;
		Config.bRequireFilteredCollidingHit = true;
		Config.bRequireValidHitAfterFilter = true;
		Config.ExpectedMinimumFilteredSubductingHits = 1;
		Config.ExpectedMinimumFilteredCollidingHits = 1;
		Config.ExpectedMinimumValidHits = 1;
		return Config;
	}

	bool FCarrierV2Stage4::RunFixtureWithReplay(const FCarrierV2Stage4Config& Config, FCarrierV2Stage4FixtureResult& OutResult)
	{
		FCarrierV2Stage4FixtureResult Replay;
		const bool bPrimaryOk = RunStage4FixtureOnce(Config, OutResult);
		const bool bReplayOk = RunStage4FixtureOnce(Config, Replay);

		OutResult.Metrics.ReplayStage3ProcessStateHash = Replay.Metrics.Stage3ProcessStateHash;
		OutResult.Metrics.ReplayGlobalSamplingHash = Replay.Metrics.GlobalSamplingHash;
		OutResult.Metrics.ReplayMetricsHash = Replay.Metrics.MetricsHash;
		OutResult.Metrics.bReplayDeterministic =
			bPrimaryOk == bReplayOk &&
			OutResult.Metrics.Stage3ProcessStateHash == Replay.Metrics.Stage3ProcessStateHash &&
			OutResult.Metrics.GlobalSamplingHash == Replay.Metrics.GlobalSamplingHash &&
			OutResult.Metrics.Verdict == Replay.Metrics.Verdict &&
			OutResult.Metrics.bFixturePass == Replay.Metrics.bFixturePass;

		OutResult.Metrics.bFixturePass = OutResult.Metrics.bFixturePass && OutResult.Metrics.bReplayDeterministic;
		OutResult.Metrics.bStageGatePass = OutResult.Metrics.bFixturePass;
		if (!OutResult.Metrics.bReplayDeterministic)
		{
			OutResult.Metrics.Verdict = TEXT("FAIL_REPLAY_DETERMINISM");
			if (OutResult.Error.IsEmpty())
			{
				OutResult.Error = TEXT("Replay A/B stage3-process hash, sampling hash, verdict, or fixture pass differed.");
			}
		}
		OutResult.Metrics.MetricsHash = HashToString(HashStage4Metrics(OutResult.Metrics, true));
		return bPrimaryOk && bReplayOk && OutResult.Metrics.bFixturePass;
	}

	FString FCarrierV2Stage4::MetricsToJson(const FCarrierV2Stage4FixtureResult& Result)
	{
		const FCarrierV2Stage4Metrics& M = Result.Metrics;
		FString Json = FString::Printf(
			TEXT("{")
			TEXT("\"run_id\":%s,")
			TEXT("\"stage_id\":%s,")
			TEXT("\"fixture_id\":%s,")
			TEXT("\"fixture_name\":%s,")
			TEXT("\"fixture_kind\":%s,")
			TEXT("\"expected_remesh_class\":%s,")
			TEXT("\"source_status\":%s,")
			TEXT("\"remesh_sampling_policy_id\":%s,")
			TEXT("\"remesh_trigger_reason\":%s,")
			TEXT("\"sample_count\":%d,")
			TEXT("\"triangle_count\":%d,")
			TEXT("\"plate_count\":%d,")
			TEXT("\"ray_epsilon\":%.12g,")
			TEXT("\"config_hash\":%s,")
			TEXT("\"stage3_process_state_hash\":%s,")
			TEXT("\"remesh_input_hash\":%s,")
			TEXT("\"global_sampling_hash\":%s,")
			TEXT("\"metrics_hash\":%s,")
			TEXT("\"global_sample_count\":%d,")
			TEXT("\"global_triangle_count\":%d,")
			TEXT("\"local_plate_vertex_count_sum\":%d,")
			TEXT("\"local_plate_triangle_count_sum\":%d,")
			TEXT("\"pretreated_plate_count\":%d,")
			TEXT("\"fully_subducted_plate_destroyed_count\":%d,")
			TEXT("\"aabb_mesh_triangle_count\":%d,")
			TEXT("\"aabb_ray_query_count\":%lld,")
			TEXT("\"raw_hit_count_total\":%lld,")
			TEXT("\"filtered_subducting_hit_count\":%d,")
			TEXT("\"filtered_colliding_hit_count\":%d,")
			TEXT("\"valid_hit_after_filter_count\":%d,")
			TEXT("\"zero_valid_hit_count\":%d,")
			TEXT("\"gap_fill_deferred_count\":%d,")
			TEXT("\"post_filter_unresolved_multihit_count\":%d,")
			TEXT("\"post_filter_boundary_only_multihit_count\":%d,")
			TEXT("\"boundary_degenerate_hit_count\":%d,")
			TEXT("\"material_interpolation_count\":%d,")
			TEXT("\"selected_filtered_hit_count\":%d,")
			TEXT("\"prior_owner_fallback_count\":%d,")
			TEXT("\"centroid_primary_resolution_count\":%d,")
			TEXT("\"random_primary_resolution_count\":%d,")
			TEXT("\"nearest_primary_resolution_count\":%d,")
			TEXT("\"ownership_repair_during_remesh_count\":%d,")
			TEXT("\"retention_hysteresis_anchor_count\":%d,")
			TEXT("\"generated_oceanic_count\":%d,")
			TEXT("\"q1q2_deferred_count\":%d,")
			TEXT("\"q1q2_discrete_approx_count\":%d,")
			TEXT("\"q1q2_prior_owner_lookup_count\":%d,")
			TEXT("\"topology_rebuild_during_sampling_count\":%d,")
			TEXT("\"process_state_reset_count\":%d,")
			TEXT("\"terrain_beauty_mutation_count\":%d,")
			TEXT("\"material_created_without_gap_fill_count\":%d,")
			TEXT("\"material_destroyed_without_process_count\":%d,")
			TEXT("\"inherited_stage3_pass\":%s,")
			TEXT("\"source_filter_pass\":%s,")
			TEXT("\"valid_selection_pass\":%s,")
			TEXT("\"no_forbidden_fallback_pass\":%s,")
			TEXT("\"deferred_gap_fill_pass\":%s,")
			TEXT("\"no_premature_topology_pass\":%s,")
			TEXT("\"replay_deterministic\":%s,")
			TEXT("\"fixture_pass\":%s,")
			TEXT("\"stage_gate_pass\":%s,")
			TEXT("\"verdict\":%s,")
			TEXT("\"replay_stage3_process_state_hash\":%s,")
			TEXT("\"replay_global_sampling_hash\":%s,")
			TEXT("\"replay_metrics_hash\":%s,")
			TEXT("\"stage3_ms\":%.3f,")
			TEXT("\"build_substrate_ms\":%.3f,")
			TEXT("\"build_plate_local_ms\":%.3f,")
			TEXT("\"motion_apply_ms\":%.3f,")
			TEXT("\"aabb_build_ms\":%.3f,")
			TEXT("\"global_sampling_ms\":%.3f,")
			TEXT("\"metrics_ms\":%.3f,")
			TEXT("\"total_ms\":%.3f,")
			TEXT("\"peak_memory_mb\":%.3f,")
			TEXT("\"sample_records_preview\":["),
			*JsonString(M.RunId),
			*JsonString(M.StageId),
			*JsonString(M.FixtureId),
			*JsonString(M.FixtureName),
			*JsonString(M.FixtureKind),
			*JsonString(M.ExpectedRemeshClass),
			*JsonString(M.SourceStatus),
			*JsonString(M.RemeshSamplingPolicyId),
			*JsonString(M.RemeshTriggerReason),
			M.SampleCount,
			M.TriangleCount,
			M.PlateCount,
			M.RayEpsilon,
			*JsonString(M.ConfigHash),
			*JsonString(M.Stage3ProcessStateHash),
			*JsonString(M.RemeshInputHash),
			*JsonString(M.GlobalSamplingHash),
			*JsonString(M.MetricsHash),
			M.GlobalSampleCount,
			M.GlobalTriangleCount,
			M.LocalPlateVertexCountSum,
			M.LocalPlateTriangleCountSum,
			M.PretreatedPlateCount,
			M.FullySubductedPlateDestroyedCount,
			M.AabbMeshTriangleCount,
			M.AabbRayQueryCount,
			M.RawHitCountTotal,
			M.FilteredSubductingHitCount,
			M.FilteredCollidingHitCount,
			M.ValidHitAfterFilterCount,
			M.ZeroValidHitCount,
			M.GapFillDeferredCount,
			M.PostFilterUnresolvedMultihitCount,
			M.PostFilterBoundaryOnlyMultihitCount,
			M.BoundaryDegenerateHitCount,
			M.MaterialInterpolationCount,
			M.SelectedFilteredHitCount,
			M.PriorOwnerFallbackCount,
			M.CentroidPrimaryResolutionCount,
			M.RandomPrimaryResolutionCount,
			M.NearestPrimaryResolutionCount,
			M.OwnershipRepairDuringRemeshCount,
			M.RetentionHysteresisAnchorCount,
			M.GeneratedOceanicCount,
			M.Q1Q2DeferredCount,
			M.Q1Q2DiscreteApproxCount,
			M.Q1Q2PriorOwnerLookupCount,
			M.TopologyRebuildDuringSamplingCount,
			M.ProcessStateResetCount,
			M.TerrainBeautyMutationCount,
			M.MaterialCreatedWithoutGapFillCount,
			M.MaterialDestroyedWithoutProcessCount,
			M.bInheritedStage3Pass ? TEXT("true") : TEXT("false"),
			M.bSourceFilterPass ? TEXT("true") : TEXT("false"),
			M.bValidSelectionPass ? TEXT("true") : TEXT("false"),
			M.bNoForbiddenFallbackPass ? TEXT("true") : TEXT("false"),
			M.bDeferredGapFillPass ? TEXT("true") : TEXT("false"),
			M.bNoPrematureTopologyPass ? TEXT("true") : TEXT("false"),
			M.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			M.bFixturePass ? TEXT("true") : TEXT("false"),
			M.bStageGatePass ? TEXT("true") : TEXT("false"),
			*JsonString(M.Verdict),
			*JsonString(M.ReplayStage3ProcessStateHash),
			*JsonString(M.ReplayGlobalSamplingHash),
			*JsonString(M.ReplayMetricsHash),
			M.Stage3Ms,
			M.BuildSubstrateMs,
			M.BuildPlateLocalMs,
			M.MotionApplyMs,
			M.AabbBuildMs,
			M.GlobalSamplingMs,
			M.MetricsMs,
			M.TotalMs,
			M.PeakMemoryMb);

		const int32 PreviewCount = FMath::Min(16, Result.SampleRecords.Num());
		for (int32 Index = 0; Index < PreviewCount; ++Index)
		{
			if (Index > 0)
			{
				Json += TEXT(",");
			}
			const FCarrierV2RemeshSampleRecord& Record = Result.SampleRecords[Index];
			Json += FString::Printf(
				TEXT("{\"sample_id\":%d,\"raw_hit_count\":%d,\"filtered_subducting_hit_count\":%d,\"filtered_colliding_hit_count\":%d,\"valid_hit_count\":%d,\"selected_plate_id\":%d,\"selected_local_triangle_id\":%d,\"selected_source_triangle_id\":%d,\"selected_continental_fraction\":%.12g,\"zero_valid_hit\":%s,\"post_filter_unresolved_multihit\":%s,\"selected_filtered_hit\":%s,\"selection_provenance\":%s}"),
				Record.SampleId,
				Record.RawHitCount,
				Record.FilteredSubductingHitCount,
				Record.FilteredCollidingHitCount,
				Record.ValidHitCount,
				Record.SelectedPlateId,
				Record.SelectedLocalTriangleId,
				Record.SelectedSourceTriangleId,
				Record.SelectedContinentalFraction,
				Record.bZeroValidHit ? TEXT("true") : TEXT("false"),
				Record.bPostFilterUnresolvedMultihit ? TEXT("true") : TEXT("false"),
				Record.bSelectedFilteredHit ? TEXT("true") : TEXT("false"),
				*JsonString(Record.SelectionProvenance));
		}
		Json += TEXT("]}");
		return Json;
	}

	FString FCarrierV2Stage4::BuildCheckpointReport(
		const FCarrierV2Stage4SuiteResult& Suite,
		const FString& CommandLine,
		const FString& CommitSha)
	{
		const double Start = FPlatformTime::Seconds();
		FString Report;
		Report += TEXT("# CarrierLab V2 Stage 4 Report\n\n");
		Report += TEXT("Status: generated by `CarrierLabV2Stage4`.\n\n");
		Report += FString::Printf(TEXT("- Commit: `%s`\n"), CommitSha.IsEmpty() ? TEXT("unknown") : *CommitSha);
		Report += FString::Printf(TEXT("- Command: `%s`\n"), *CommandLine);
		Report += FString::Printf(TEXT("- Output root: `%s`\n"), *Suite.OutputRoot);
		Report += FString::Printf(TEXT("- Metrics JSONL: `%s`\n\n"), *Suite.MetricsPath);

		Report += TEXT("## Scope\n\n");
		Report += TEXT("V2-4 is the first Stage 1.5 resampling consumer of V2-3 process marks. It samples every global TDS vertex by casting a center-to-vertex ray into an AABB tree built from the current moved plate-local triangles, filters subducting and colliding marked triangles before source selection, barycentrically transfers material only from a surviving single valid hit or boundary-only degenerate hit, and records zero-valid-hit samples as deferred q1/q2/qGamma gap-fill work. It does not create divergent crust, approximate q1/q2, rebuild plate-local topology, reset process state, repair ownership, retain prior owners, or choose non-boundary post-filter multihit winners.\n\n");

		Report += TEXT("## Fixture Gates\n\n");
		Report += TEXT("| fixture | remesh class | samples | raw hits | filtered subducting | filtered colliding | valid hits | zero valid | unresolved multihit | pass | verdict |\n");
		Report += TEXT("|---|---|---:|---:|---:|---:|---:|---:|---:|---|---|\n");
		for (const FCarrierV2Stage4FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage4Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | %d | %lld | %d | %d | %d | %d | %d | %s | `%s` |\n"),
				*M.FixtureId,
				*M.ExpectedRemeshClass,
				M.GlobalSampleCount,
				M.RawHitCountTotal,
				M.FilteredSubductingHitCount,
				M.FilteredCollidingHitCount,
				M.ValidHitAfterFilterCount,
				M.ZeroValidHitCount,
				M.PostFilterUnresolvedMultihitCount,
				M.bFixturePass ? TEXT("pass") : TEXT("fail"),
				*M.Verdict);
		}

		Report += TEXT("\n## Filter And Selection Evidence\n\n");
		Report += TEXT("| fixture | selected filtered hits | boundary-only multihits | material interpolations | gap-fill deferred | generated oceanic | q1/q2 approximations | source filter | valid selection |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---|---|\n");
		for (const FCarrierV2Stage4FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage4Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %d | %d | %d | %d | %s | %s |\n"),
				*M.FixtureId,
				M.SelectedFilteredHitCount,
				M.PostFilterBoundaryOnlyMultihitCount,
				M.MaterialInterpolationCount,
				M.GapFillDeferredCount,
				M.GeneratedOceanicCount,
				M.Q1Q2DiscreteApproxCount,
				M.bSourceFilterPass ? TEXT("pass") : TEXT("fail"),
				M.bValidSelectionPass ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Hard Guards\n\n");
		Report += TEXT("| fixture | prior owner fallback | centroid | random | nearest | ownership repair | retention/hysteresis | q1/q2 prior owner | topology rebuild | process reset | terrain beauty | fallback guard |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FCarrierV2Stage4FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage4Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %s |\n"),
				*M.FixtureId,
				M.PriorOwnerFallbackCount,
				M.CentroidPrimaryResolutionCount,
				M.RandomPrimaryResolutionCount,
				M.NearestPrimaryResolutionCount,
				M.OwnershipRepairDuringRemeshCount,
				M.RetentionHysteresisAnchorCount,
				M.Q1Q2PriorOwnerLookupCount,
				M.TopologyRebuildDuringSamplingCount,
				M.ProcessStateResetCount,
				M.TerrainBeautyMutationCount,
				M.bNoForbiddenFallbackPass ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Replay A/B\n\n");
		Report += TEXT("| fixture | v2-3 process hash | replay v2-3 hash | sampling hash | replay sampling hash | deterministic |\n");
		Report += TEXT("|---|---|---|---|---|---|\n");
		for (const FCarrierV2Stage4FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage4Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | `%s` | `%s` | `%s` | %s |\n"),
				*M.FixtureId,
				*M.Stage3ProcessStateHash,
				*M.ReplayStage3ProcessStateHash,
				*M.GlobalSamplingHash,
				*M.ReplayGlobalSamplingHash,
				M.bReplayDeterministic ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Performance Ladder\n\n");
		Report += TEXT("| fixture | samples | triangles | stage3 ms | build substrate ms | plate-local ms | motion ms | AABB build ms | sampling ms | total ms | peak memory mb |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FCarrierV2Stage4FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage4Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f |\n"),
				*M.FixtureId,
				M.GlobalSampleCount,
				M.AabbMeshTriangleCount,
				M.Stage3Ms,
				M.BuildSubstrateMs,
				M.BuildPlateLocalMs,
				M.MotionApplyMs,
				M.AabbBuildMs,
				M.GlobalSamplingMs,
				M.TotalMs,
				M.PeakMemoryMb);
		}

		Report += TEXT("\n## Policy Ledger\n\n");
		Report += TEXT("| policy | authority class | behavior | audit evidence |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += TEXT("| `V2-4-POL-GLOBAL-TDS-SAMPLING-TARGET` | source_explicit | every global TDS vertex is sampled by a center ray through moved plate-local triangles | `aabb_ray_query_count`, `global_sample_count` |\n");
		Report += TEXT("| `V2-4-POL-PROCESS-MARK-FILTER` | source_explicit | subducting and colliding triangle marks are filtered before source selection | `filtered_subducting_hit_count`, `filtered_colliding_hit_count`, `selected_filtered_hit_count=0` |\n");
		Report += TEXT("| `V2-4-POL-Q1Q2-DEFERRED` | approved_slice_limit | zero-valid-hit vertices are recorded as deferred gap-fill candidates; no generated oceanic crust is created in this slice | `gap_fill_deferred_count`, `generated_oceanic_count=0` |\n");
		Report += TEXT("| `V2-4-POL-NO-TOPOLOGY-REBUILD-YET` | approved_slice_limit | plate-local topology is not rebuilt until q1/q2 gap-fill is implemented | `topology_rebuild_during_sampling_count=0`, `process_state_reset_count=0` |\n\n");

		Report += TEXT("## Independent Oracle\n\n");
		Report += TEXT("V2-4 does not compare filtered sampling output against itself. The input process marks are inherited from the replay-checked V2-3 process state. The sampling oracle is structural: all moved plate-local triangles are inserted into a GeometryCore AABB tree, every global TDS vertex ray collects all intersections, marked subducting/colliding intersections are discarded before valid-hit selection, and any non-boundary multi-hit remaining after filtering is a hard failure instead of being resolved by centroid, nearest, prior owner, random order, retention, or repair.\n\n");

		Report += TEXT("## Known Limitations\n\n");
		Report += TEXT("- V2-4 intentionally does not implement q1/q2/qGamma divergent gap fill; zero-valid-hit samples are ledgered for V2-5.\n");
		Report += TEXT("- Because q1/q2 is deferred, V2-4 does not rebuild fresh plate-local topology or reset process state.\n");
		Report += TEXT("- Boundary-only multi-hits at exact global vertices are counted separately from non-boundary post-filter multihits.\n");
		Report += TEXT("- Scale fixtures inherit V2-3's deterministic plate material profile policy and are still carrier-kernel gates, not terrain/elevation validation.\n\n");

		Report += TEXT("## Verdict\n\n");
		Report += FString::Printf(TEXT("Verdict: `%s`\n\n"), *Suite.Verdict);
		Report += FString::Printf(TEXT("- Micro gate: %s\n"), Suite.bMicroGatePass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 50k gate: %s\n"), Suite.bScale50kPass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 250k characterization attempted: %s\n"), Suite.bAttempted250k ? TEXT("yes") : TEXT("no"));
		if (Suite.bAttempted250k)
		{
			Report += FString::Printf(TEXT("- 250k characterization: %s\n"), Suite.bScale250kPass ? TEXT("pass") : TEXT("fail"));
		}
		Report += TEXT("\nExplicit user go/no-go is required before V2-5 q1/q2/qGamma divergent gap-fill and topology rebuild work.\n");

		const double ReportMs = (FPlatformTime::Seconds() - Start) * 1000.0;
		Report += FString::Printf(TEXT("\nReport generation ms: %.3f\n"), ReportMs);
		return Report;
	}

	TArray<FCarrierV2Stage5Config> FCarrierV2Stage5::MakeMicroFixtureConfigs()
	{
		TArray<FCarrierV2Stage5Config> Configs;
		FCarrierV2Stage4Config SubductionSampling;
		FCarrierV2Stage4Config CollisionSampling;
		for (const FCarrierV2Stage4Config& Stage4Config : FCarrierV2Stage4::MakeMicroFixtureConfigs())
		{
			if (Stage4Config.FixtureId == TEXT("FX-012"))
			{
				SubductionSampling = Stage4Config;
			}
			else if (Stage4Config.FixtureId == TEXT("FX-013"))
			{
				CollisionSampling = Stage4Config;
			}
		}

		FCarrierV2Stage5Config FX014;
		FX014.SamplingConfig = SubductionSampling;
		FX014.FixtureId = TEXT("FX-014");
		FX014.FixtureName = TEXT("FilteredSamplingRebuildReset");
		FX014.ExpectedRemeshClass = TEXT("filtered_sampling_topology_rebuild_reset_no_gap");
		FX014.RemeshSamplingPolicyId = TEXT("global_tds_aabb_filter_rebuild_reset_no_gap");
		FX014.bRequireGeneratedOceanic = false;
		FX014.bRequireContinuousQ1Q2 = false;
		Configs.Add(FX014);

		FCarrierV2Stage5Config FX015;
		FX015.SamplingConfig = CollisionSampling;
		FX015.FixtureId = TEXT("FX-015");
		FX015.FixtureName = TEXT("ContinuousQ1Q2GapFillRebuildReset");
		FX015.ExpectedRemeshClass = TEXT("continuous_q1q2_qgamma_gap_fill_topology_rebuild_reset");
		FX015.RemeshSamplingPolicyId = TEXT("global_tds_aabb_filter_continuous_q1q2_rebuild_reset");
		FX015.bRequireGeneratedOceanic = true;
		FX015.bRequireContinuousQ1Q2 = true;
		FX015.ExpectedMinimumGeneratedOceanic = 1;
		FX015.ExpectedMinimumQ1Q2Pairs = 1;
		Configs.Add(FX015);

		return Configs;
	}

	FCarrierV2Stage5Config FCarrierV2Stage5::MakeScaleConfig(const int32 SampleCount, const bool bComparisonScale)
	{
		FCarrierV2Stage5Config Config;
		Config.SamplingConfig = FCarrierV2Stage4::MakeScaleConfig(SampleCount, bComparisonScale);
		Config.FixtureId = bComparisonScale ? TEXT("SCALE-250K-Q1Q2-REBUILD") : TEXT("SCALE-50K-Q1Q2-REBUILD");
		Config.FixtureName = bComparisonScale ? TEXT("Scale250kQ1Q2RebuildCharacterization") : TEXT("Scale50kQ1Q2RebuildGate");
		Config.ExpectedRemeshClass = TEXT("scale_continuous_q1q2_gap_fill_topology_rebuild_reset");
		Config.RemeshSamplingPolicyId = TEXT("global_tds_aabb_filter_scale_continuous_q1q2_rebuild_reset");
		Config.bRequireGeneratedOceanic = true;
		Config.bRequireContinuousQ1Q2 = true;
		Config.ExpectedMinimumGeneratedOceanic = 1;
		Config.ExpectedMinimumQ1Q2Pairs = 1;
		return Config;
	}

	bool FCarrierV2Stage5::RunFixtureWithReplay(const FCarrierV2Stage5Config& Config, FCarrierV2Stage5FixtureResult& OutResult)
	{
		FCarrierV2Stage5FixtureResult Replay;
		const bool bPrimaryOk = RunStage5FixtureOnce(Config, OutResult);
		const bool bReplayOk = RunStage5FixtureOnce(Config, Replay);

		OutResult.Metrics.ReplayStage3ProcessStateHash = Replay.Metrics.Stage3ProcessStateHash;
		OutResult.Metrics.ReplayGlobalSamplingHash = Replay.Metrics.GlobalSamplingHash;
		OutResult.Metrics.ReplayGapFillHash = Replay.Metrics.GapFillHash;
		OutResult.Metrics.ReplayRebuiltTopologyHash = Replay.Metrics.RebuiltTopologyHash;
		OutResult.Metrics.ReplayMetricsHash = Replay.Metrics.MetricsHash;
		OutResult.Metrics.bReplayDeterministic =
			bPrimaryOk == bReplayOk &&
			OutResult.Metrics.Stage3ProcessStateHash == Replay.Metrics.Stage3ProcessStateHash &&
			OutResult.Metrics.GlobalSamplingHash == Replay.Metrics.GlobalSamplingHash &&
			OutResult.Metrics.GapFillHash == Replay.Metrics.GapFillHash &&
			OutResult.Metrics.RebuiltTopologyHash == Replay.Metrics.RebuiltTopologyHash &&
			OutResult.Metrics.Verdict == Replay.Metrics.Verdict &&
			OutResult.Metrics.bFixturePass == Replay.Metrics.bFixturePass;

		OutResult.Metrics.bFixturePass = OutResult.Metrics.bFixturePass && OutResult.Metrics.bReplayDeterministic;
		OutResult.Metrics.bStageGatePass = OutResult.Metrics.bFixturePass;
		if (!OutResult.Metrics.bReplayDeterministic)
		{
			OutResult.Metrics.Verdict = TEXT("FAIL_REPLAY_DETERMINISM");
			if (OutResult.Error.IsEmpty())
			{
				OutResult.Error = TEXT("Replay A/B process hash, sampling hash, gap-fill hash, rebuilt topology hash, verdict, or fixture pass differed.");
			}
		}
		OutResult.Metrics.MetricsHash = HashToString(HashStage5Metrics(OutResult.Metrics, true));
		return bPrimaryOk && bReplayOk && OutResult.Metrics.bFixturePass;
	}

	FString FCarrierV2Stage5::MetricsToJson(const FCarrierV2Stage5FixtureResult& Result)
	{
		const FCarrierV2Stage5Metrics& M = Result.Metrics;
		FString Json = TEXT("{");
		Json += FString::Printf(TEXT("\"run_id\":%s,"), *JsonString(M.RunId));
		Json += FString::Printf(TEXT("\"stage_id\":%s,"), *JsonString(M.StageId));
		Json += FString::Printf(TEXT("\"fixture_id\":%s,"), *JsonString(M.FixtureId));
		Json += FString::Printf(TEXT("\"fixture_name\":%s,"), *JsonString(M.FixtureName));
		Json += FString::Printf(TEXT("\"fixture_kind\":%s,"), *JsonString(M.FixtureKind));
		Json += FString::Printf(TEXT("\"expected_remesh_class\":%s,"), *JsonString(M.ExpectedRemeshClass));
		Json += FString::Printf(TEXT("\"source_status\":%s,"), *JsonString(M.SourceStatus));
		Json += FString::Printf(TEXT("\"remesh_sampling_policy_id\":%s,"), *JsonString(M.RemeshSamplingPolicyId));
		Json += FString::Printf(TEXT("\"triangle_partition_policy_id\":%s,"), *JsonString(M.TrianglePartitionPolicyId));
		Json += FString::Printf(TEXT("\"remesh_trigger_reason\":%s,"), *JsonString(M.RemeshTriggerReason));
		Json += FString::Printf(TEXT("\"sample_count\":%d,\"triangle_count\":%d,\"plate_count\":%d,\"ray_epsilon\":%.12g,"), M.SampleCount, M.TriangleCount, M.PlateCount, M.RayEpsilon);
		Json += FString::Printf(TEXT("\"config_hash\":%s,\"stage3_process_state_hash\":%s,\"remesh_input_hash\":%s,\"global_sampling_hash\":%s,\"gap_fill_hash\":%s,\"rebuilt_topology_hash\":%s,\"metrics_hash\":%s,"),
			*JsonString(M.ConfigHash),
			*JsonString(M.Stage3ProcessStateHash),
			*JsonString(M.RemeshInputHash),
			*JsonString(M.GlobalSamplingHash),
			*JsonString(M.GapFillHash),
			*JsonString(M.RebuiltTopologyHash),
			*JsonString(M.MetricsHash));
		Json += FString::Printf(TEXT("\"global_sample_count\":%d,\"global_triangle_count\":%d,\"local_plate_vertex_count_sum\":%d,\"local_plate_triangle_count_sum\":%d,\"pretreated_plate_count\":%d,\"fully_subducted_plate_destroyed_count\":%d,\"boundary_edge_count\":%d,\"aabb_mesh_triangle_count\":%d,\"aabb_ray_query_count\":%lld,\"raw_hit_count_total\":%lld,"),
			M.GlobalSampleCount,
			M.GlobalTriangleCount,
			M.LocalPlateVertexCountSum,
			M.LocalPlateTriangleCountSum,
			M.PretreatedPlateCount,
			M.FullySubductedPlateDestroyedCount,
			M.BoundaryEdgeCount,
			M.AabbMeshTriangleCount,
			M.AabbRayQueryCount,
			M.RawHitCountTotal);
		Json += FString::Printf(TEXT("\"filtered_subducting_hit_count\":%d,\"filtered_colliding_hit_count\":%d,\"valid_hit_after_filter_count\":%d,\"zero_valid_hit_count\":%d,\"q1q2_gap_fill_count\":%d,\"q1q2_boundary_query_count\":%d,\"q1q2_boundary_pair_count\":%d,\"q1q2_different_plate_pair_count\":%d,\"qgamma_computed_count\":%d,\"generated_oceanic_count\":%d,\"gap_fill_no_boundary_pair_count\":%d,"),
			M.FilteredSubductingHitCount,
			M.FilteredCollidingHitCount,
			M.ValidHitAfterFilterCount,
			M.ZeroValidHitCount,
			M.Q1Q2GapFillCount,
			M.Q1Q2BoundaryQueryCount,
			M.Q1Q2BoundaryPairCount,
			M.Q1Q2DifferentPlatePairCount,
			M.QGammaComputedCount,
			M.GeneratedOceanicCount,
			M.GapFillNoBoundaryPairCount);
		Json += FString::Printf(TEXT("\"post_filter_unresolved_multihit_count\":%d,\"post_filter_boundary_only_multihit_count\":%d,\"boundary_degenerate_hit_count\":%d,\"material_interpolation_count\":%d,\"selected_filtered_hit_count\":%d,"),
			M.PostFilterUnresolvedMultihitCount,
			M.PostFilterBoundaryOnlyMultihitCount,
			M.BoundaryDegenerateHitCount,
			M.MaterialInterpolationCount,
			M.SelectedFilteredHitCount);
		Json += FString::Printf(TEXT("\"prior_owner_fallback_count\":%d,\"centroid_primary_resolution_count\":%d,\"random_primary_resolution_count\":%d,\"nearest_primary_resolution_count\":%d,\"ownership_repair_during_remesh_count\":%d,\"retention_hysteresis_anchor_count\":%d,\"q1q2_discrete_approx_count\":%d,\"q1q2_prior_owner_lookup_count\":%d,"),
			M.PriorOwnerFallbackCount,
			M.CentroidPrimaryResolutionCount,
			M.RandomPrimaryResolutionCount,
			M.NearestPrimaryResolutionCount,
			M.OwnershipRepairDuringRemeshCount,
			M.RetentionHysteresisAnchorCount,
			M.Q1Q2DiscreteApproxCount,
			M.Q1Q2PriorOwnerLookupCount);
		Json += FString::Printf(TEXT("\"topology_rebuild_count\":%d,\"rebuilt_plate_count\":%d,\"rebuilt_local_vertex_count_sum\":%d,\"rebuilt_local_triangle_count_sum\":%d,\"rebuilt_triangle_assignment_count\":%d,\"mixed_vertex_triangle_count\":%d,\"majority_triangle_assignment_count\":%d,\"three_way_triangle_assignment_count\":%d,\"unassigned_triangle_count\":%d,"),
			M.TopologyRebuildCount,
			M.RebuiltPlateCount,
			M.RebuiltLocalVertexCountSum,
			M.RebuiltLocalTriangleCountSum,
			M.RebuiltTriangleAssignmentCount,
			M.MixedVertexTriangleCount,
			M.MajorityTriangleAssignmentCount,
			M.ThreeWayTriangleAssignmentCount,
			M.UnassignedTriangleCount);
		Json += FString::Printf(TEXT("\"process_state_reset_count\":%d,\"pre_reset_triangle_mark_count\":%d,\"post_reset_triangle_mark_count\":%d,\"terrain_beauty_mutation_count\":%d,\"material_created_without_gap_fill_count\":%d,\"material_destroyed_without_process_count\":%d,"),
			M.ProcessStateResetCount,
			M.PreResetTriangleMarkCount,
			M.PostResetTriangleMarkCount,
			M.TerrainBeautyMutationCount,
			M.MaterialCreatedWithoutGapFillCount,
			M.MaterialDestroyedWithoutProcessCount);
		Json += FString::Printf(TEXT("\"inherited_stage3_pass\":%s,\"source_filter_pass\":%s,\"q1q2_gap_fill_pass\":%s,\"topology_rebuild_pass\":%s,\"process_reset_pass\":%s,\"no_forbidden_fallback_pass\":%s,\"replay_deterministic\":%s,\"fixture_pass\":%s,\"stage_gate_pass\":%s,\"verdict\":%s,"),
			M.bInheritedStage3Pass ? TEXT("true") : TEXT("false"),
			M.bSourceFilterPass ? TEXT("true") : TEXT("false"),
			M.bQ1Q2GapFillPass ? TEXT("true") : TEXT("false"),
			M.bTopologyRebuildPass ? TEXT("true") : TEXT("false"),
			M.bProcessResetPass ? TEXT("true") : TEXT("false"),
			M.bNoForbiddenFallbackPass ? TEXT("true") : TEXT("false"),
			M.bReplayDeterministic ? TEXT("true") : TEXT("false"),
			M.bFixturePass ? TEXT("true") : TEXT("false"),
			M.bStageGatePass ? TEXT("true") : TEXT("false"),
			*JsonString(M.Verdict));
		Json += FString::Printf(TEXT("\"replay_stage3_process_state_hash\":%s,\"replay_global_sampling_hash\":%s,\"replay_gap_fill_hash\":%s,\"replay_rebuilt_topology_hash\":%s,\"replay_metrics_hash\":%s,"),
			*JsonString(M.ReplayStage3ProcessStateHash),
			*JsonString(M.ReplayGlobalSamplingHash),
			*JsonString(M.ReplayGapFillHash),
			*JsonString(M.ReplayRebuiltTopologyHash),
			*JsonString(M.ReplayMetricsHash));
		Json += FString::Printf(TEXT("\"stage3_ms\":%.3f,\"build_substrate_ms\":%.3f,\"build_plate_local_ms\":%.3f,\"motion_apply_ms\":%.3f,\"aabb_build_ms\":%.3f,\"global_sampling_ms\":%.3f,\"topology_rebuild_ms\":%.3f,\"metrics_ms\":%.3f,\"total_ms\":%.3f,\"peak_memory_mb\":%.3f,\"sample_records_preview\":["),
			M.Stage3Ms,
			M.BuildSubstrateMs,
			M.BuildPlateLocalMs,
			M.MotionApplyMs,
			M.AabbBuildMs,
			M.GlobalSamplingMs,
			M.TopologyRebuildMs,
			M.MetricsMs,
			M.TotalMs,
			M.PeakMemoryMb);

		const int32 PreviewCount = FMath::Min(16, Result.SampleRecords.Num());
		for (int32 Index = 0; Index < PreviewCount; ++Index)
		{
			if (Index > 0)
			{
				Json += TEXT(",");
			}
			const FCarrierV2Stage5SampleRecord& Record = Result.SampleRecords[Index];
			Json += FString::Printf(
				TEXT("{\"sample_id\":%d,\"raw_hit_count\":%d,\"filtered_subducting_hit_count\":%d,\"filtered_colliding_hit_count\":%d,\"valid_hit_count\":%d,\"selected_plate_id\":%d,\"assigned_plate_id\":%d,\"selected_continental_fraction\":%.12g,\"zero_valid_hit\":%s,\"generated_oceanic\":%s,\"boundary_pair_found\":%s,\"q1_plate_id\":%d,\"q2_plate_id\":%d,\"q1_distance_rad\":%.12g,\"q2_distance_rad\":%.12g,\"qgamma_distance_rad\":%.12g,\"qgamma_alpha\":%.12g,\"selection_provenance\":%s}"),
				Record.SampleId,
				Record.RawHitCount,
				Record.FilteredSubductingHitCount,
				Record.FilteredCollidingHitCount,
				Record.ValidHitCount,
				Record.SelectedPlateId,
				Record.AssignedPlateId,
				Record.SelectedContinentalFraction,
				Record.bZeroValidHit ? TEXT("true") : TEXT("false"),
				Record.bGeneratedOceanic ? TEXT("true") : TEXT("false"),
				Record.bBoundaryPairFound ? TEXT("true") : TEXT("false"),
				Record.Q1PlateId,
				Record.Q2PlateId,
				Record.Q1DistanceRad,
				Record.Q2DistanceRad,
				Record.QGammaDistanceRad,
				Record.QGammaAlpha,
				*JsonString(Record.SelectionProvenance));
		}
		Json += TEXT("]}");
		return Json;
	}

	FString FCarrierV2Stage5::BuildCheckpointReport(
		const FCarrierV2Stage5SuiteResult& Suite,
		const FString& CommandLine,
		const FString& CommitSha)
	{
		const double Start = FPlatformTime::Seconds();
		FString Report;
		Report += TEXT("# CarrierLab V2 Stage 5 Report\n\n");
		Report += TEXT("Status: generated by `CarrierLabV2Stage5`.\n\n");
		Report += FString::Printf(TEXT("- Commit: `%s`\n"), CommitSha.IsEmpty() ? TEXT("unknown") : *CommitSha);
		Report += FString::Printf(TEXT("- Command: `%s`\n"), *CommandLine);
		Report += FString::Printf(TEXT("- Output root: `%s`\n"), *Suite.OutputRoot);
		Report += FString::Printf(TEXT("- Metrics JSONL: `%s`\n\n"), *Suite.MetricsPath);

		Report += TEXT("## Scope\n\n");
		Report += TEXT("V2-5 consumes V2-4 filtered global TDS sampling and completes the remaining Stage 1.5 remesh mechanics: zero-valid samples use continuous nearest boundary q1/q2 points from different plates, qGamma midpoint provenance is recorded, new oceanic carrier material is created only through that gap-fill path, and plate-local topology is rebuilt from the resampled global TDS assignments. Process triangle marks are reset after rebuild. This slice does not add elevation, erosion, slab pull, rifting, terrane transfer, editor UI, or terrain beauty.\n\n");

		Report += TEXT("## Fixture Gates\n\n");
		Report += TEXT("| fixture | remesh class | samples | raw hits | filtered subducting | filtered colliding | valid hits | zero valid | generated oceanic | unassigned triangles | pass | verdict |\n");
		Report += TEXT("|---|---|---:|---:|---:|---:|---:|---:|---:|---:|---|---|\n");
		for (const FCarrierV2Stage5FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage5Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | %d | %lld | %d | %d | %d | %d | %d | %d | %s | `%s` |\n"),
				*M.FixtureId,
				*M.ExpectedRemeshClass,
				M.GlobalSampleCount,
				M.RawHitCountTotal,
				M.FilteredSubductingHitCount,
				M.FilteredCollidingHitCount,
				M.ValidHitAfterFilterCount,
				M.ZeroValidHitCount,
				M.GeneratedOceanicCount,
				M.UnassignedTriangleCount,
				M.bFixturePass ? TEXT("pass") : TEXT("fail"),
				*M.Verdict);
		}

		Report += TEXT("\n## Q1/Q2 Evidence\n\n");
		Report += TEXT("| fixture | boundary edges | q1/q2 queries | q1/q2 pairs | different-plate pairs | qGamma computed | no-pair fallback | discrete q1/q2 | prior-owner q1/q2 | gap-fill pass |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FCarrierV2Stage5FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage5Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %d | %d | %d | %d | %d | %d | %s |\n"),
				*M.FixtureId,
				M.BoundaryEdgeCount,
				M.Q1Q2BoundaryQueryCount,
				M.Q1Q2BoundaryPairCount,
				M.Q1Q2DifferentPlatePairCount,
				M.QGammaComputedCount,
				M.GapFillNoBoundaryPairCount,
				M.Q1Q2DiscreteApproxCount,
				M.Q1Q2PriorOwnerLookupCount,
				M.bQ1Q2GapFillPass ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Rebuild And Reset Evidence\n\n");
		Report += TEXT("| fixture | rebuilds | rebuilt plates | rebuilt vertices | rebuilt triangles | mixed triangles | majority assignments | three-way assignments | process reset | pre-reset marks | post-reset marks | rebuild pass | reset pass |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|\n");
		for (const FCarrierV2Stage5FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage5Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %d | %d | %d | %d | %d | %d | %d | %d | %s | %s |\n"),
				*M.FixtureId,
				M.TopologyRebuildCount,
				M.RebuiltPlateCount,
				M.RebuiltLocalVertexCountSum,
				M.RebuiltLocalTriangleCountSum,
				M.MixedVertexTriangleCount,
				M.MajorityTriangleAssignmentCount,
				M.ThreeWayTriangleAssignmentCount,
				M.ProcessStateResetCount,
				M.PreResetTriangleMarkCount,
				M.PostResetTriangleMarkCount,
				M.bTopologyRebuildPass ? TEXT("pass") : TEXT("fail"),
				M.bProcessResetPass ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Hard Guards\n\n");
		Report += TEXT("| fixture | selected filtered hits | prior owner fallback | centroid | random | nearest | ownership repair | retention/hysteresis | material without gap fill | terrain beauty | guard pass |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FCarrierV2Stage5FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage5Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %d | %d | %d | %d | %d | %d | %d | %s |\n"),
				*M.FixtureId,
				M.SelectedFilteredHitCount,
				M.PriorOwnerFallbackCount,
				M.CentroidPrimaryResolutionCount,
				M.RandomPrimaryResolutionCount,
				M.NearestPrimaryResolutionCount,
				M.OwnershipRepairDuringRemeshCount,
				M.RetentionHysteresisAnchorCount,
				M.MaterialCreatedWithoutGapFillCount,
				M.TerrainBeautyMutationCount,
				M.bNoForbiddenFallbackPass ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Replay A/B\n\n");
		Report += TEXT("| fixture | v2-3 hash | replay v2-3 hash | sampling hash | replay sampling hash | topology hash | replay topology hash | deterministic |\n");
		Report += TEXT("|---|---|---|---|---|---|---|---|\n");
		for (const FCarrierV2Stage5FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage5Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | %s |\n"),
				*M.FixtureId,
				*M.Stage3ProcessStateHash,
				*M.ReplayStage3ProcessStateHash,
				*M.GlobalSamplingHash,
				*M.ReplayGlobalSamplingHash,
				*M.RebuiltTopologyHash,
				*M.ReplayRebuiltTopologyHash,
				M.bReplayDeterministic ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Performance Ladder\n\n");
		Report += TEXT("| fixture | samples | triangles | stage3 ms | build substrate ms | plate-local ms | motion ms | AABB build ms | sampling ms | rebuild ms | total ms | peak memory mb |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
		for (const FCarrierV2Stage5FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Stage5Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f |\n"),
				*M.FixtureId,
				M.GlobalSampleCount,
				M.GlobalTriangleCount,
				M.Stage3Ms,
				M.BuildSubstrateMs,
				M.BuildPlateLocalMs,
				M.MotionApplyMs,
				M.AabbBuildMs,
				M.GlobalSamplingMs,
				M.TopologyRebuildMs,
				M.TotalMs,
				M.PeakMemoryMb);
		}

		Report += TEXT("\n## Policy Ledger\n\n");
		Report += TEXT("| policy | authority class | behavior | audit evidence |\n");
		Report += TEXT("|---|---|---|---|\n");
		Report += TEXT("| `V2-5-POL-CONTINUOUS-Q1Q2` | source_explicit | zero-valid global TDS samples choose continuous nearest points on two different plate boundary arcs and compute qGamma midpoint provenance | `q1q2_boundary_pair_count`, `q1q2_different_plate_pair_count`, `qgamma_computed_count` |\n");
		Report += TEXT("| `V2-5-POL-OCEANIC-GAP-ONLY` | source_explicit_carrier_subset | generated oceanic carrier material is created only for zero-valid q1/q2 gap-fill samples | `generated_oceanic_count == zero_valid_hit_count`, `material_created_without_gap_fill_count=0` |\n");
		Report += TEXT("| `V2-5-POL-TOPOLOGY-REBUILD` | source_explicit | plate-local triangulations are rebuilt from global TDS assignments after resampling | `topology_rebuild_count`, `rebuilt_local_triangle_count_sum` |\n");
		Report += TEXT("| `V2-5-POL-MIXED-TRIANGLE-PARTITION` | lab_policy_thesis_gap | mixed-vertex global triangles use majority assignment, with plate sample-count then lower-id tiebreak for three-way cases | `mixed_vertex_triangle_count`, `three_way_triangle_assignment_count` |\n");
		Report += TEXT("| `V2-5-POL-PROCESS-RESET` | source_explicit | process triangle marks are reset after the rebuilt topology is produced | `pre_reset_triangle_mark_count`, `post_reset_triangle_mark_count=0` |\n\n");

		Report += TEXT("## Independent Oracle\n\n");
		Report += TEXT("V2-5 gates do not accept a self-described remesh. The sampling path must first inherit a replay-passing V2-3 process state, then independently collect all AABB ray intersections, filter marked subducting/colliding hits, and require every zero-valid sample in these fixtures and scale runs to receive a q1/q2 pair from two different boundary plates. The topology gate rebuilds plate-local triangles from the global TDS vertex assignments and hashes the rebuilt per-plate vertices, triangles, material provenance, and process-reset state.\n\n");

		Report += TEXT("## Known Limitations\n\n");
		Report += TEXT("- V2-5 implements carrier material generation only; elevation, zGamma ridge profile values, erosion, slab pull, rifting, and terrain beauty remain out of scope.\n");
		Report += TEXT("- The paper does not specify mixed-vertex global triangle partitioning after per-vertex plate assignment; V2-5 names and counts the deterministic lab policy instead of hiding it.\n");
		Report += TEXT("- Continental persistence still depends on future terrane transfer before remesh; V2-5 does not invent a resampler-side continental rescue path.\n\n");

		Report += TEXT("## Verdict\n\n");
		Report += FString::Printf(TEXT("Verdict: `%s`\n\n"), *Suite.Verdict);
		Report += FString::Printf(TEXT("- Micro gate: %s\n"), Suite.bMicroGatePass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 50k gate: %s\n"), Suite.bScale50kPass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 250k characterization attempted: %s\n"), Suite.bAttempted250k ? TEXT("yes") : TEXT("no"));
		if (Suite.bAttempted250k)
		{
			Report += FString::Printf(TEXT("- 250k characterization: %s\n"), Suite.bScale250kPass ? TEXT("pass") : TEXT("fail"));
		}
		Report += TEXT("\nExplicit user go/no-go is required before any V2-6 carrier-cycle, cadence, rifting, terrain, or editor work.\n");

		const double ReportMs = (FPlatformTime::Seconds() - Start) * 1000.0;
		Report += FString::Printf(TEXT("\nReport generation ms: %.3f\n"), ReportMs);
		return Report;
	}

	TArray<FCarrierV2Milestone2Config> FCarrierV2Milestone2::MakeMicroFixtureConfigs()
	{
		TArray<FCarrierV2Milestone2Config> Configs;

		FCarrierV2Milestone2Config FX001;
		FX001.MotionConfig.BaseConfig.FixtureId = TEXT("M2-FX-001");
		FX001.MotionConfig.BaseConfig.FixtureName = TEXT("M2NoMotionSinglePlateBase");
		FX001.MotionConfig.BaseConfig.SampleCount = 6;
		FX001.MotionConfig.BaseConfig.PlateCount = 1;
		FX001.MotionConfig.BaseConfig.FixtureKind = ECarrierV2FixtureKind::Positive;
		FX001.MotionConfig.FixtureId = TEXT("M2-FX-001");
		FX001.MotionConfig.FixtureName = TEXT("NoMotionResampleNoop");
		FX001.MotionConfig.ExpectedMotionClass = TEXT("no_motion_resample");
		FX001.MotionConfig.SourceStatus = TEXT("source_explicit_m2_noop");
		FX001.MotionConfig.MotionStepCount = 1;
		FX001.MotionConfig.DtMa = 1.0;
		FX001.MotionConfig.PlateMotions.Add(FCarrierV2MotionSpec());
		FX001.FixtureId = TEXT("M2-FX-001");
		FX001.FixtureName = TEXT("NoMotionResampleNoop");
		FX001.bRequireSingleHitWrites = true;
		FX001.ExpectedMinimumSingleHitWrites = 6;
		FX001.bRequireMaterialConservation = true;
		FX001.bRequireSharpnessPreservation = true;
		Configs.Add(FX001);

		FCarrierV2Milestone2Config FX002;
		FX002.MotionConfig.BaseConfig.FixtureId = TEXT("M2-FX-002");
		FX002.MotionConfig.BaseConfig.FixtureName = TEXT("M2SingleSourceTransferBase");
		FX002.MotionConfig.BaseConfig.SampleCount = 64;
		FX002.MotionConfig.BaseConfig.PlateCount = 1;
		FX002.MotionConfig.BaseConfig.FixtureKind = ECarrierV2FixtureKind::Positive;
		FX002.MotionConfig.BaseConfig.FixtureSubstrateId = TEXT("fibonacci_spherical_delaunay_micro");
		FX002.MotionConfig.BaseConfig.bUseFibonacciSubstrate = true;
		FX002.MotionConfig.FixtureId = TEXT("M2-FX-002");
		FX002.MotionConfig.FixtureName = TEXT("SingleSourceBarycentricTransfer");
		FX002.MotionConfig.ExpectedMotionClass = TEXT("single_source_rotation");
		FX002.MotionConfig.SourceStatus = TEXT("source_explicit_m2_single_source");
		FX002.MotionConfig.MotionStepCount = 1;
		FX002.MotionConfig.DtMa = 1.0;
		FCarrierV2MotionSpec FX002Motion;
		FX002Motion.Axis = FVector3d(0.2, 0.4, 1.0).GetSafeNormal();
		FX002Motion.AngularSpeedRadPerMa = 0.015;
		FX002.MotionConfig.PlateMotions.Add(FX002Motion);
		FX002.FixtureId = TEXT("M2-FX-002");
		FX002.FixtureName = TEXT("SingleSourceBarycentricTransfer");
		FX002.bRequireSingleHitWrites = true;
		FX002.ExpectedMinimumSingleHitWrites = 64;
		FX002.bRequireFullTopologyRebuild = true;
		Configs.Add(FX002);

		FCarrierV2Milestone2Config FX003;
		for (const FCarrierV2Stage1Config& Stage1Config : FCarrierV2Stage1::MakeMicroFixtureConfigs())
		{
			if (Stage1Config.FixtureId == TEXT("FX-007"))
			{
				FX003.MotionConfig = Stage1Config;
				break;
			}
		}
		FX003.MotionConfig.BaseConfig.FixtureId = TEXT("M2-FX-003");
		FX003.MotionConfig.FixtureId = TEXT("M2-FX-003");
		FX003.MotionConfig.FixtureName = TEXT("DivergentContinuousQ1Q2GapFill");
		FX003.MotionConfig.SourceStatus = TEXT("source_explicit_m2_divergent_gap");
		FX003.FixtureId = TEXT("M2-FX-003");
		FX003.FixtureName = TEXT("DivergentContinuousQ1Q2GapFill");
		FX003.bRequireDivergentGapFill = true;
		FX003.ExpectedMinimumGapFill = 1;
		FX003.bRequireFullTopologyRebuild = false;
		Configs.Add(FX003);

		FCarrierV2Milestone2Config FX004;
		for (const FCarrierV2Stage1Config& Stage1Config : FCarrierV2Stage1::MakeMicroFixtureConfigs())
		{
			if (Stage1Config.FixtureId == TEXT("FX-008"))
			{
				FX004.MotionConfig = Stage1Config;
				break;
			}
		}
		FX004.MotionConfig.BaseConfig.FixtureId = TEXT("M2-FX-004");
		FX004.MotionConfig.FixtureId = TEXT("M2-FX-004");
		FX004.MotionConfig.FixtureName = TEXT("OverlapBlockedBeforeProcessState");
		FX004.MotionConfig.SourceStatus = TEXT("source_explicit_m2_overlap_blocked");
		FX004.FixtureId = TEXT("M2-FX-004");
		FX004.FixtureName = TEXT("OverlapBlockedBeforeProcessState");
		FX004.bRequireOverlapBlocked = true;
		FX004.ExpectedMinimumOverlapBlocked = 1;
		FX004.bRequireFullTopologyRebuild = false;
		Configs.Add(FX004);

		FCarrierV2Milestone2Config FX005 = FX001;
		FX005.MotionConfig.BaseConfig.FixtureId = TEXT("M2-FX-005");
		FX005.MotionConfig.BaseConfig.FixtureName = TEXT("M2RepeatedLifecycleBase");
		FX005.MotionConfig.FixtureId = TEXT("M2-FX-005");
		FX005.MotionConfig.FixtureName = TEXT("RepeatedLifecycleConservation");
		FX005.MotionConfig.ExpectedMotionClass = TEXT("repeated_lifecycle_stationary_resample");
		FX005.MotionConfig.SourceStatus = TEXT("source_explicit_m2_lifecycle");
		FX005.FixtureId = TEXT("M2-FX-005");
		FX005.FixtureName = TEXT("RepeatedLifecycleConservation");
		FX005.LifecycleWindowCount = 3;
		FX005.bRequireSingleHitWrites = true;
		FX005.ExpectedMinimumSingleHitWrites = 6;
		FX005.bRequireMaterialConservation = true;
		FX005.bRequireSharpnessPreservation = true;
		Configs.Add(FX005);

		FCarrierV2Milestone2Config FX006 = FX001;
		FX006.MotionConfig.BaseConfig.FixtureId = TEXT("M2-FX-006");
		FX006.MotionConfig.BaseConfig.FixtureName = TEXT("M2PriorOwnerNegativeControlBase");
		FX006.MotionConfig.FixtureId = TEXT("M2-FX-006");
		FX006.MotionConfig.FixtureName = TEXT("PriorOwnerNegativeControl");
		FX006.MotionConfig.SourceStatus = TEXT("diagnostic_negative_control_prior_owner_present_unread");
		FX006.FixtureId = TEXT("M2-FX-006");
		FX006.FixtureName = TEXT("PriorOwnerNegativeControl");
		FX006.bInjectPriorOwnerLabelsForNegativeControl = true;
		Configs.Add(FX006);

		return Configs;
	}

	FCarrierV2Milestone2Config FCarrierV2Milestone2::MakeScaleConfig(const int32 SampleCount, const bool bComparisonScale)
	{
		FCarrierV2Milestone2Config Config;
		Config.MotionConfig = FCarrierV2Stage1::MakeScaleConfig(SampleCount, bComparisonScale);
		const FString SampleLabel = (SampleCount % 1000 == 0)
			? FString::Printf(TEXT("%dK"), SampleCount / 1000)
			: FString::Printf(TEXT("%d"), SampleCount);
		Config.MotionConfig.BaseConfig.FixtureId = FString::Printf(TEXT("SCALE-%s-M2"), *SampleLabel);
		Config.MotionConfig.BaseConfig.FixtureName = FString::Printf(TEXT("Scale%sMilestone2CarrierCycle"), *SampleLabel);
		Config.MotionConfig.FixtureId = Config.MotionConfig.BaseConfig.FixtureId;
		Config.MotionConfig.FixtureName = Config.MotionConfig.BaseConfig.FixtureName;
		Config.MotionConfig.ExpectedMotionClass = TEXT("scale_scheduled_motion_resample");
		Config.MotionConfig.SourceStatus = TEXT("source_explicit_m2_scale");
		Config.FixtureId = Config.MotionConfig.FixtureId;
		Config.FixtureName = Config.MotionConfig.FixtureName;
		Config.CarrierCycleClass = TEXT("scale_scheduled_motion_resample_writeback");
		Config.bScaleCharacterization = true;
		Config.bRequireSingleHitWrites = true;
		Config.ExpectedMinimumSingleHitWrites = 1;
		Config.bRequireDivergentGapFill = true;
		Config.ExpectedMinimumGapFill = 1;
		Config.bRequireOverlapBlocked = true;
		Config.ExpectedMinimumOverlapBlocked = 1;
		Config.bRequireFullTopologyRebuild = false;
		Config.ExpectedMaxStepKernelMs = SampleCount >= 500000 ? 2480.0 : 1240.0;
		return Config;
	}

	bool FCarrierV2Milestone2::RunFixtureWithReplay(const FCarrierV2Milestone2Config& Config, FCarrierV2Milestone2FixtureResult& OutResult)
	{
		FCarrierV2Milestone2FixtureResult Replay;
		const bool bPrimaryOk = RunMilestone2FixtureOnce(Config, OutResult);
		const bool bReplayOk = RunMilestone2FixtureOnce(Config, Replay);

		OutResult.Metrics.ReplayPreCycleAuthorityHash = Replay.Metrics.PreCycleAuthorityHash;
		OutResult.Metrics.ReplayPostCycleAuthorityHash = Replay.Metrics.PostCycleAuthorityHash;
		OutResult.Metrics.ReplayResampleOutputHash = Replay.Metrics.ResampleOutputHash;
		OutResult.Metrics.ReplayRebuiltTopologyHash = Replay.Metrics.RebuiltTopologyHash;
		OutResult.Metrics.ReplayMetricsHash = Replay.Metrics.MetricsHash;
		OutResult.Metrics.bReplayDeterministic =
			OutResult.Metrics.PreCycleAuthorityHash == Replay.Metrics.PreCycleAuthorityHash &&
			OutResult.Metrics.PostCycleAuthorityHash == Replay.Metrics.PostCycleAuthorityHash &&
			OutResult.Metrics.ResampleOutputHash == Replay.Metrics.ResampleOutputHash &&
			OutResult.Metrics.RebuiltTopologyHash == Replay.Metrics.RebuiltTopologyHash;

		OutResult.Metrics.bFixturePass = OutResult.Metrics.bFixturePass && OutResult.Metrics.bReplayDeterministic;
		OutResult.Metrics.bStageGatePass = OutResult.Metrics.bFixturePass;
		if (!OutResult.Metrics.bReplayDeterministic)
		{
			OutResult.Metrics.Verdict = TEXT("FAIL_REPLAY_DETERMINISM");
			if (OutResult.Error.IsEmpty())
			{
				OutResult.Error = TEXT("Replay A/B authority hash, resample hash, or rebuilt topology hash differed.");
			}
		}
		OutResult.Metrics.MetricsHash = HashToString(HashMilestone2Metrics(OutResult.Metrics, true));
		return bPrimaryOk && bReplayOk && OutResult.Metrics.bFixturePass;
	}

	FString FCarrierV2Milestone2::MetricsToJson(const FCarrierV2Milestone2FixtureResult& Result)
	{
		const FCarrierV2Milestone2Metrics& M = Result.Metrics;
		FString Json = TEXT("{");
		Json += FString::Printf(TEXT("\"run_id\":%s,"), *JsonString(M.RunId));
		Json += FString::Printf(TEXT("\"stage_id\":%s,"), *JsonString(M.StageId));
		Json += FString::Printf(TEXT("\"fixture_id\":%s,"), *JsonString(M.FixtureId));
		Json += FString::Printf(TEXT("\"fixture_name\":%s,"), *JsonString(M.FixtureName));
		Json += FString::Printf(TEXT("\"fixture_kind\":%s,"), *JsonString(M.FixtureKind));
		Json += FString::Printf(TEXT("\"carrier_cycle_class\":%s,"), *JsonString(M.CarrierCycleClass));
		Json += FString::Printf(TEXT("\"source_status\":%s,"), *JsonString(M.SourceStatus));
		Json += FString::Printf(TEXT("\"resample_policy_id\":%s,"), *JsonString(M.ResamplePolicyId));
		Json += FString::Printf(TEXT("\"sample_count\":%d,"), M.GlobalSampleCount);
		Json += FString::Printf(TEXT("\"triangle_count\":%d,"), M.GlobalTriangleCount);
		Json += FString::Printf(TEXT("\"plate_count\":%d,"), M.PlateCount);
		Json += FString::Printf(TEXT("\"lifecycle_windows\":%d,"), M.LifecycleWindowCount);
		Json += FString::Printf(TEXT("\"max_plate_motion_angle_rad\":%.12f,"), M.MaxPlateMotionAngleRad);
		Json += FString::Printf(TEXT("\"broadphase_margin_gate_pass\":%s,"), M.bBroadphaseMarginGatePass ? TEXT("true") : TEXT("false"));
		Json += FString::Printf(TEXT("\"broadphase_equivalence_mismatch_count\":%d,"), M.BroadphaseEquivalenceMismatchCount);
		Json += FString::Printf(TEXT("\"raw_hit_count_total\":%lld,"), M.RawHitCountTotal);
		Json += FString::Printf(TEXT("\"valid_single_hit_write_count\":%d,"), M.ValidSingleHitWriteCount);
		Json += FString::Printf(TEXT("\"divergent_zero_hit_count\":%d,"), M.DivergentZeroHitCount);
		Json += FString::Printf(TEXT("\"q1q2_gap_fill_count\":%d,"), M.Q1Q2GapFillCount);
		Json += FString::Printf(TEXT("\"q1q2_boundary_query_count\":%d,"), M.Q1Q2BoundaryQueryCount);
		Json += FString::Printf(TEXT("\"q1q2_boundary_pair_count\":%d,"), M.Q1Q2BoundaryPairCount);
		Json += FString::Printf(TEXT("\"q1q2_different_plate_pair_count\":%d,"), M.Q1Q2DifferentPlatePairCount);
		Json += FString::Printf(TEXT("\"qgamma_computed_count\":%d,"), M.QGammaComputedCount);
		Json += FString::Printf(TEXT("\"generated_oceanic_count\":%d,"), M.GeneratedOceanicCount);
		Json += FString::Printf(TEXT("\"gap_fill_no_boundary_pair_count\":%d,"), M.GapFillNoBoundaryPairCount);
		Json += FString::Printf(TEXT("\"nondegenerate_overlap_blocked_count\":%d,"), M.NondegenerateOverlapBlockedCount);
		Json += FString::Printf(TEXT("\"boundary_only_overlap_count\":%d,"), M.BoundaryOnlyOverlapCount);
		Json += FString::Printf(TEXT("\"cross_plate_boundary_only_overlap_count\":%d,"), M.CrossPlateBoundaryOnlyOverlapCount);
		Json += FString::Printf(TEXT("\"same_plate_boundary_only_multihit_count\":%d,"), M.SamePlateBoundaryOnlyMultihitCount);
		Json += FString::Printf(TEXT("\"deferred_overlap_sample_count\":%d,"), M.DeferredOverlapSampleCount);
		Json += FString::Printf(TEXT("\"deferred_overlap_area_weight\":%.12f,"), M.DeferredOverlapAreaWeight);
		Json += FString::Printf(TEXT("\"deferred_overlap_continental_mass_estimate\":%.12f,"), M.DeferredOverlapContinentalMassEstimate);
		Json += FString::Printf(TEXT("\"unsupported_overlap_write_attempt_count\":%d,"), M.UnsupportedOverlapWriteAttemptCount);
		Json += FString::Printf(TEXT("\"prior_owner_read_count\":%d,"), M.PriorOwnerReadCount);
		Json += FString::Printf(TEXT("\"global_owner_read_count\":%d,"), M.GlobalOwnerReadCount);
		Json += FString::Printf(TEXT("\"centroid_primary_resolution_count\":%d,"), M.CentroidPrimaryResolutionCount);
		Json += FString::Printf(TEXT("\"random_primary_resolution_count\":%d,"), M.RandomPrimaryResolutionCount);
		Json += FString::Printf(TEXT("\"nearest_primary_resolution_count\":%d,"), M.NearestPrimaryResolutionCount);
		Json += FString::Printf(TEXT("\"topology_rebuild_count\":%d,"), M.TopologyRebuildCount);
		Json += FString::Printf(TEXT("\"mixed_vertex_triangle_count\":%d,"), M.MixedVertexTriangleCount);
		Json += FString::Printf(TEXT("\"majority_triangle_assignment_count\":%d,"), M.MajorityTriangleAssignmentCount);
		Json += FString::Printf(TEXT("\"three_way_triangle_assignment_count\":%d,"), M.ThreeWayTriangleAssignmentCount);
		Json += FString::Printf(TEXT("\"rebuilt_triangle_assignment_count\":%d,"), M.RebuiltTriangleAssignmentCount);
		Json += FString::Printf(TEXT("\"unassigned_triangle_count\":%d,"), M.UnassignedTriangleCount);
		Json += FString::Printf(TEXT("\"unassigned_triangle_budget\":%d,"), M.UnassignedTriangleBudget);
		Json += FString::Printf(TEXT("\"unassigned_triangle_budget_pass\":%s,"), M.bUnassignedTriangleBudgetPass ? TEXT("true") : TEXT("false"));
		Json += FString::Printf(TEXT("\"material_conservation_delta\":%.12f,"), M.MaterialConservationDelta);
		Json += FString::Printf(TEXT("\"total_variation_delta\":%.12f,"), M.TotalVariationDelta);
		Json += FString::Printf(TEXT("\"top_miss_plate_pairs\":%s,"), *JsonString(M.TopMissPlatePairs));
		Json += FString::Printf(TEXT("\"top_overlap_plate_pairs\":%s,"), *JsonString(M.TopOverlapPlatePairs));
		Json += FString::Printf(TEXT("\"pre_cycle_authority_hash\":%s,"), *JsonString(M.PreCycleAuthorityHash));
		Json += FString::Printf(TEXT("\"post_cycle_authority_hash\":%s,"), *JsonString(M.PostCycleAuthorityHash));
		Json += FString::Printf(TEXT("\"resample_output_hash\":%s,"), *JsonString(M.ResampleOutputHash));
		Json += FString::Printf(TEXT("\"rebuilt_topology_hash\":%s,"), *JsonString(M.RebuiltTopologyHash));
		Json += FString::Printf(TEXT("\"replay_post_cycle_authority_hash\":%s,"), *JsonString(M.ReplayPostCycleAuthorityHash));
		Json += FString::Printf(TEXT("\"replay_resample_output_hash\":%s,"), *JsonString(M.ReplayResampleOutputHash));
		Json += FString::Printf(TEXT("\"replay_rebuilt_topology_hash\":%s,"), *JsonString(M.ReplayRebuiltTopologyHash));
		Json += FString::Printf(TEXT("\"replay_deterministic\":%s,"), M.bReplayDeterministic ? TEXT("true") : TEXT("false"));
		Json += FString::Printf(TEXT("\"step_kernel_ms\":%.3f,"), M.StepKernelMs);
		Json += FString::Printf(TEXT("\"full_carrier_cycle_ms\":%.3f,"), M.FullCarrierCycleMs);
		Json += FString::Printf(TEXT("\"paper_resample_cycle_budget_ms\":%.3f,"), M.PaperResampleCycleBudgetMs);
		Json += FString::Printf(TEXT("\"paper_resample_cycle_budget_pass\":%s,"), M.bPaperResampleCycleBudgetPass ? TEXT("true") : TEXT("false"));
		Json += FString::Printf(TEXT("\"total_ms\":%.3f,"), M.TotalMs);
		Json += FString::Printf(TEXT("\"peak_memory_mb\":%.3f,"), M.PeakMemoryMb);
		Json += FString::Printf(TEXT("\"fixture_pass\":%s,"), M.bFixturePass ? TEXT("true") : TEXT("false"));
		Json += FString::Printf(TEXT("\"verdict\":%s"), *JsonString(M.Verdict));
		Json += TEXT("}");
		return Json;
	}

	FString FCarrierV2Milestone2::BuildCheckpointReport(
		const FCarrierV2Milestone2SuiteResult& Suite,
		const FString& CommandLine,
		const FString& CommitSha)
	{
		const double Start = FPlatformTime::Seconds();
		FString Report;
		Report += TEXT("# CarrierLab Milestone 2 Closeout Report\n\n");
		Report += TEXT("Status: generated by `CarrierLabV2Milestone2`.\n\n");
		Report += FString::Printf(TEXT("- Git HEAD at commandlet launch: `%s`\n"), CommitSha.IsEmpty() ? TEXT("unknown") : *CommitSha);
		Report += FString::Printf(TEXT("- Command: `%s`\n"), *CommandLine);
		Report += FString::Printf(TEXT("- Output root: `%s`\n"), *Suite.OutputRoot);
		Report += FString::Printf(TEXT("- Metrics JSONL: `%s`\n\n"), *Suite.MetricsPath);

		Report += TEXT("## Scope\n\n");
		Report += TEXT("Milestone 2 implements the pre-process carrier cycle: scheduled rigid motion, projection only at the resample boundary, single-source barycentric write-back, divergent q1/q2/qGamma oceanic gap fill, and plate-local topology rebuild. Nondegenerate cross-plate overlaps are counted and blocked because subduction/collision process-state filters do not exist yet in the M2 contract. This milestone does not add contact physics, terrain, elevation, erosion, uplift, slab pull, rifting, or any overlap winner policy.\n\n");

		Report += TEXT("## Fixture Gates\n\n");
		Report += TEXT("| fixture | samples | plates | windows | single-source writes | gaps | q1/q2 pairs | overlap blocked | deferred overlap mass | unassigned tris | pass | verdict |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|\n");
		for (const FCarrierV2Milestone2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Milestone2Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %d | %d | %d | %d | %d | %.12f | %d | %s | `%s` |\n"),
				*M.FixtureId,
				M.GlobalSampleCount,
				M.PlateCount,
				M.LifecycleWindowCount,
				M.ValidSingleHitWriteCount,
				M.DivergentZeroHitCount,
				M.Q1Q2BoundaryPairCount,
				M.NondegenerateOverlapBlockedCount,
				M.DeferredOverlapContinentalMassEstimate,
				M.UnassignedTriangleCount,
				M.bFixturePass ? TEXT("pass") : TEXT("fail"),
				*M.Verdict);
		}

		Report += TEXT("\n## Write-Back Policy\n\n");
		Report += TEXT("| fixture | raw hits | material interpolation | generated oceanic | no q1/q2 pair | nondegenerate blocked | cross-boundary blocked | same-plate boundary multi-hit | unsupported overlap writes | prior/global owner reads | centroid/random/nearest resolvers | policy pass |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|\n");
		for (const FCarrierV2Milestone2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Milestone2Metrics& M = Result.Metrics;
			const int32 OwnerReads = M.PriorOwnerReadCount + M.GlobalOwnerReadCount;
			const int32 Resolvers = M.CentroidPrimaryResolutionCount + M.RandomPrimaryResolutionCount + M.NearestPrimaryResolutionCount;
			Report += FString::Printf(
				TEXT("| `%s` | %lld | %d | %d | %d | %d | %d | %d | %d | %d | %d | %s |\n"),
				*M.FixtureId,
				M.RawHitCountTotal,
				M.MaterialInterpolationCount,
				M.GeneratedOceanicCount,
				M.GapFillNoBoundaryPairCount,
				M.NondegenerateOverlapBlockedCount,
				M.CrossPlateBoundaryOnlyOverlapCount,
				M.SamePlateBoundaryOnlyMultihitCount,
				M.UnsupportedOverlapWriteAttemptCount,
				OwnerReads,
				Resolvers,
				(M.bOverlapPolicyPass && M.bNoForbiddenFallbackPass) ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## Lifecycle And Topology\n\n");
		Report += TEXT("| fixture | rebuilds | rebuilt plates | rebuilt vertices | rebuilt triangles | mixed tris | 3-way tris | unassigned/budget | material delta | tv delta | conservation status | topology status |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---|\n");
		for (const FCarrierV2Milestone2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Milestone2Metrics& M = Result.Metrics;
			const bool bConservationRequired =
				Result.Config.bRequireMaterialConservation || Result.Config.bRequireSharpnessPreservation;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %d | %d | %d/%d | %d | %d | %d/%d | %.12f | %.12f | %s | %s |\n"),
				*M.FixtureId,
				M.TopologyRebuildCount,
				M.RebuiltPlateCount,
				M.RebuiltLocalVertexCountSum,
				M.RebuiltTriangleAssignmentCount,
				M.GlobalTriangleCount,
				M.MixedVertexTriangleCount,
				M.ThreeWayTriangleAssignmentCount,
				M.UnassignedTriangleCount,
				M.UnassignedTriangleBudget,
				M.MaterialConservationDelta,
				M.TotalVariationDelta,
				Milestone2RequirementStatus(bConservationRequired, M.bLifecycleConservationPass),
				Milestone2TopologyStatus(Result.Config, M));
		}
		Report += TEXT("\nLifecycle note: material and total-variation deltas are gate-enforced only where the fixture explicitly requires conservation/sharpness. Scale rows are characterization until M3 process filters make repeated convergent windows legal. The unassigned-triangle budget is `6 * (nondegenerate overlap blocked + cross-plate boundary blocked + no-q1q2-pair gaps)` for rows that intentionally defer full topology.\n");

		Report += TEXT("\n## Attribution\n\n");
		Report += TEXT("| fixture | top miss plate pairs | top overlap plate pairs |\n");
		Report += TEXT("|---|---|---|\n");
		for (const FCarrierV2Milestone2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Milestone2Metrics& M = Result.Metrics;
			Report += FString::Printf(TEXT("| `%s` | `%s` | `%s` |\n"), *M.FixtureId, *M.TopMissPlatePairs, *M.TopOverlapPlatePairs);
		}

		Report += TEXT("\n## Replay A/B\n\n");
		Report += TEXT("| fixture | pre hash | post hash | replay post | resample hash | replay resample | topology hash | replay topology | deterministic |\n");
		Report += TEXT("|---|---|---|---|---|---|---|---|---|\n");
		for (const FCarrierV2Milestone2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Milestone2Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | `%s` | %s |\n"),
				*M.FixtureId,
				*M.PreCycleAuthorityHash,
				*M.PostCycleAuthorityHash,
				*M.ReplayPostCycleAuthorityHash,
				*M.ResampleOutputHash,
				*M.ReplayResampleOutputHash,
				*M.RebuiltTopologyHash,
				*M.ReplayRebuiltTopologyHash,
				M.bReplayDeterministic ? TEXT("pass") : TEXT("fail"));
		}

		Report += TEXT("\n## M2 Regression Baselines For M3\n\n");
		Report += TEXT("These hashes are the filters-off M2 carrier-cycle signatures that M3 must treat as regression baselines when process filters are disabled. Replay A/B checks within-run determinism; this table is the cross-commit pin.\n\n");
		Report += TEXT("| fixture | post-cycle authority | resample output | rebuilt topology |\n");
		Report += TEXT("|---|---|---|---|\n");
		for (const FCarrierV2Milestone2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Milestone2Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | `%s` | `%s` | `%s` |\n"),
				*M.FixtureId,
				*M.PostCycleAuthorityHash,
				*M.ResampleOutputHash,
				*M.RebuiltTopologyHash);
		}

		Report += TEXT("\n## Performance Ladder\n\n");
		Report += TEXT("| fixture | samples | build substrate ms | build plate ms | aabb build ms | motion ms | resample ms | rebuild ms | step kernel ms | full cycle ms | paper resample budget ms | budget pass | total ms | peak mb |\n");
		Report += TEXT("|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---|---:|---:|\n");
		for (const FCarrierV2Milestone2FixtureResult& Result : Suite.Results)
		{
			const FCarrierV2Milestone2Metrics& M = Result.Metrics;
			Report += FString::Printf(
				TEXT("| `%s` | %d | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %.3f | %s | %.3f | %.3f |\n"),
				*M.FixtureId,
				M.GlobalSampleCount,
				M.BuildSubstrateMs,
				M.BuildPlateLocalMs,
				M.AabbBuildMs,
				M.MotionApplyMs,
				M.ResampleMs,
				M.TopologyRebuildMs,
				M.StepKernelMs,
				M.FullCarrierCycleMs,
				M.PaperResampleCycleBudgetMs,
				(M.bPerformanceBudgetPass && M.bPaperResampleCycleBudgetPass) ? TEXT("pass") : TEXT("fail"),
				M.TotalMs,
				M.PeakMemoryMb);
		}
		Report += TEXT("\nBudget note: `step kernel ms` remains the legacy M2 no-pathology gate charged as motion + resample + topology rebuild. `full cycle ms` adds AABB build because M2 rebuilds trees every resample window. The paper-anchored resample-cycle comparison is only applied where the paper row is named here: 250k full cycle versus the 3.58 s oceanic-crust/resampling row. M3 must split per-step process timing from per-window resampling timing instead of treating the 1.24 s total-row cap as spare headroom.\n");

		Report += TEXT("\n## Gate Summary\n\n");
		Report += FString::Printf(TEXT("- Micro gate: `%s`\n"), Suite.bMicroGatePass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 50k scale gate: `%s`\n"), Suite.bScale50kPass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 100k reported: `%s`\n"), Suite.bScale100kReported ? TEXT("yes") : TEXT("no"));
		Report += FString::Printf(TEXT("- 250k attempted: `%s`, pass: `%s`\n"), Suite.bAttempted250k ? TEXT("yes") : TEXT("no"), Suite.bScale250kPass ? TEXT("pass") : TEXT("fail"));
		Report += FString::Printf(TEXT("- 500k optional characterization attempted: `%s`, pass: `%s`\n"), Suite.bAttempted500k ? TEXT("yes") : TEXT("no"), Suite.bScale500kPass ? TEXT("pass") : TEXT("fail"));
		if (!Suite.bAttempted500k)
		{
			Report += FString::Printf(TEXT("- 500k not attempted reason: `%s`\n"), Suite.NotAttempted500kReason.IsEmpty() ? TEXT("not requested") : *Suite.NotAttempted500kReason);
		}
		Report += FString::Printf(TEXT("- Final verdict: `%s`\n\n"), *Suite.Verdict);
		Report += TEXT("## Next Gate\n\n");
		Report += TEXT("Stop here for explicit user go/no-go before Milestone 3. M2 exercises the carrier cycle only for single-source transfer and divergent gap fill. Convergent overlap material selection remains intentionally deferred until source-grounded process-state filters exist.\n");

		const double ReportMs = (FPlatformTime::Seconds() - Start) * 1000.0;
		Report += FString::Printf(TEXT("\nReport generation ms: %.3f\n"), ReportMs);
		return Report;
	}
}
