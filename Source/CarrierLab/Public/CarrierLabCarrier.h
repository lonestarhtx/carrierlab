// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace CarrierLab
{
	struct FStage0Config
	{
		int32 SampleCount = 1024;
		int32 PlateCount = 40;
		int32 Seed = 42;
		double ContinentalPlateFraction = 0.30;
	};

	struct FSphereSample
	{
		int32 Id = INDEX_NONE;
		FVector3d UnitPosition = FVector3d::ZeroVector;
		double AreaWeight = 0.0;
		int32 PlateId = INDEX_NONE;
		bool bContinental = false;
	};

	struct FSphereTriangle
	{
		int32 A = INDEX_NONE;
		int32 B = INDEX_NONE;
		int32 C = INDEX_NONE;
		int32 PlateId = INDEX_NONE;
		bool bBoundary = false;
	};

	struct FCarrierVertex
	{
		int32 GlobalSampleId = INDEX_NONE;
		FVector3d UnitPosition = FVector3d::ZeroVector;
		double AreaWeight = 0.0;
		bool bContinental = false;
	};

	struct FCarrierPlateTriangle
	{
		int32 A = INDEX_NONE;
		int32 B = INDEX_NONE;
		int32 C = INDEX_NONE;
		int32 SourceTriangleId = INDEX_NONE;
		bool bBoundary = false;
	};

	struct FCarrierRayTriangleRef
	{
		int32 PlateId = INDEX_NONE;
		int32 LocalTriangleId = INDEX_NONE;
	};

	struct FCarrierPlate
	{
		int32 PlateId = INDEX_NONE;
		FVector3d InitialCenter = FVector3d::ZeroVector;
		bool bContinental = false;
		TArray<int32> SampleIds;
		TArray<int32> TriangleIds;
		TArray<FCarrierVertex> Vertices;
		TArray<FCarrierPlateTriangle> LocalTriangles;
		TMap<int32, int32> GlobalSampleIdToLocalVertexId;
	};

	struct FCarrierState
	{
		FStage0Config Config;
		TArray<FSphereSample> Samples;
		TArray<FSphereTriangle> Triangles;
		TArray<FCarrierPlate> Plates;
		TArray<TArray<int32>> SampleIncidentTriangleIds;
		TArray<TArray<int32>> SampleNeighborIds;
		TArray<TArray<FCarrierRayTriangleRef>> SampleRayCandidateTriangles;
	};

	enum class EStage0MissClass : uint8
	{
		None = 0,
		NumericMiss = 1,
		TopologyHole = 2,
		OutOfDomain = 3
	};

	enum class EStage0OverlapClass : uint8
	{
		None = 0,
		BoundaryDegenerate = 1,
		NonDegenerate = 2,
		ThirdPlateBoundary = 3,
		ThirdPlateNonDegenerate = 4
	};

	struct FStage0ProjectionBuffers
	{
		TArray<int32> ResolvedPlateIds;
		TArray<uint8> MissClasses;
		TArray<uint8> OverlapClasses;
		TArray<uint8> BoundaryMask;
		TArray<uint8> ContinentalMask;
	};

	struct FStage0Metrics
	{
		int32 SampleCount = 0;
		int32 PlateCount = 0;
		int32 TriangleCount = 0;
		int32 PlateLocalVertexCount = 0;
		int32 PlateLocalTriangleCount = 0;
		int32 EdgeCount = 0;
		int32 EulerCharacteristic = 0;
		int32 BoundaryTriangleCount = 0;
		int32 RawHitCount = 0;
		int32 RawMissCount = 0;
		int32 RawMultiHitCount = 0;
		int32 BoundaryDegenerateOverlapCount = 0;
		int32 NonDegenerateMissCount = 0;
		int32 NonDegenerateOverlapCount = 0;
		int32 ThirdPlateIntrusionCount = 0;
		int32 ThirdPlateBoundaryDegenerateCount = 0;
		int32 ThirdPlateNonDegenerateCount = 0;
		int32 NaNOrInfCount = 0;
		int64 RayCandidateCount = 0;
		int64 RayTriangleTestCount = 0;
		double AuthoritativeContinentalAreaFraction = 0.0;
		double ProjectedContinentalAreaFraction = 0.0;
		double TotalMaterialArea = 0.0;
		double BuildWallClockSeconds = 0.0;
		double WallClockSeconds = 0.0;
		double TotalWallClockSeconds = 0.0;
		uint64 MemoryUsedBytes = 0;
		FString DeterminismHash;
	};

	class FCarrierLabStage0
	{
	public:
		static bool BuildColdStartCarrier(const FStage0Config& Config, FCarrierState& OutState, FString& OutError);
		static FStage0Metrics ProjectColdStart(const FCarrierState& State, FStage0ProjectionBuffers* OutProjectionBuffers = nullptr);
		static FString HashStateAndMetrics(const FCarrierState& State, const FStage0Metrics& Metrics);

	private:
		static void GenerateFibonacciSamples(int32 SampleCount, TArray<FSphereSample>& OutSamples);
		static void GeneratePlateCenters(const FStage0Config& Config, TArray<FVector3d>& OutCenters);
		static bool BuildSphericalDelaunayConvexHull(const TArray<FSphereSample>& Samples, TArray<FSphereTriangle>& OutTriangles, FString& OutError);
		static void AssignPlatesAndTriangles(FCarrierState& State);
		static void BuildAdjacency(FCarrierState& State);
		static void BuildPlateLocalTriangulations(FCarrierState& State);
		static bool IntersectRayWithPlateTriangle(const FVector3d& RayDirection, const FVector3d& A, const FVector3d& B, const FVector3d& C, FVector3d& OutBarycentric, bool& bOutBoundaryDegenerate);
		static uint64 ComputeTopologyHash(const FCarrierState& State, const FStage0Metrics& Metrics);
	};
}
