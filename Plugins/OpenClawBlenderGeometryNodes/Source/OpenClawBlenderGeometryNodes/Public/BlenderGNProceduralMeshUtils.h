#pragma once

#include "CoreMinimal.h"
#include "ProceduralMeshComponent.h"

class UBlenderGeometryNodesComponent;
class UProceduralMeshComponent;
struct FBlenderGNMeshData;

namespace BlenderGNProcMesh
{
	struct FIslandMotionCache
	{
		TArray<int32> VertexIsland;
		TArray<FVector> IslandNormals;
		TArray<float> IslandPhases;
	};

	UBlenderGeometryNodesComponent* ResolveRuntime(const AActor* Owner, UBlenderGeometryNodesComponent* ExplicitRuntime);
	bool ReadSectionVertices(const UProceduralMeshComponent* Mesh, TArray<FVector>& OutVertices);
	bool ApplyVertices(UBlenderGeometryNodesComponent* Runtime, const TArray<FVector>& Vertices, const TArray<FVector>& Normals);
	bool ApplyVertices(UBlenderGeometryNodesComponent* Runtime, const TArray<FVector>& Vertices, const TArray<FVector>& Normals, TArray<FLinearColor>& ScratchVertexColors, TArray<FProcMeshTangent>& ScratchTangents);
	bool BuildHexIslandMotionCache(const FBlenderGNMeshData& MeshData, int32 HexSubdivisions, float NoiseScale, FIslandMotionCache& OutCache);
	void RecalculateNormals(const FBlenderGNMeshData& MeshData, const TArray<FVector>& Vertices, TArray<FVector>& OutNormals);
	TArray<TArray<int32>> BuildPositionGroups(const TArray<FVector>& Vertices, float Quantization);
	void SmoothNormalsByGroups(const TArray<TArray<int32>>& Groups, float SmoothingAngleDegrees, TArray<FVector>& Normals);
	float Hash01(int32 Seed);
	FVector AxisVector(int32 Axis);
	FVector ApplyAxisOffset(FVector Vertex, int32 Axis, float Offset);
	FVector2D AxisSampleCoordinates(const FVector& Vertex, int32 Axis);
}
