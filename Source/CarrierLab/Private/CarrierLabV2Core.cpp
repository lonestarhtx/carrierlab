// Copyright Epic Games, Inc. All Rights Reserved.

#include "CarrierLabV2Core.h"

#include "CompGeom/ConvexHull3.h"
#include "HAL/PlatformMemory.h"
#include "HAL/PlatformTime.h"

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
			AddTriangle(State, 0, 2, 4, 0);
			AddTriangle(State, 2, 1, 4, bSinglePlate ? 0 : 0);
			AddTriangle(State, 1, 3, 4, bSinglePlate ? 0 : 1);
			AddTriangle(State, 3, 0, 4, bSinglePlate ? 0 : 1);
			AddTriangle(State, 2, 0, 5, 0);
			AddTriangle(State, 1, 2, 5, bSinglePlate ? 0 : 0);
			AddTriangle(State, 3, 1, 5, bSinglePlate ? 0 : 1);
			AddTriangle(State, 0, 3, 5, bSinglePlate ? 0 : 1);

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
}
