#pragma once

#include "CoreMinimal.h"
#include "Components/MeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "BlenderGNFastDynamicMeshComponent.generated.h"

struct FBlenderGNFastIslandBatch
{
	uint32 FirstIndex = 0;
	uint32 NumPrimitives = 0;
	FVector3f Normal = FVector3f(0.0f, 0.0f, 1.0f);
	float Phase = 0.0f;
};

UCLASS(ClassGroup = (OpenClaw), meta = (BlueprintSpawnableComponent))
class OPENCLAWBLENDERGEOMETRYNODES_API UBlenderGNFastDynamicMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	UBlenderGNFastDynamicMeshComponent();

	void InitMesh(
		const TArray<FVector>& Vertices,
		const TArray<int32>& Triangles,
		const TArray<FVector>& Normals,
		const TArray<FVector2D>& UVs,
		const TArray<FColor>& Colors,
		const TArray<FProcMeshTangent>& Tangents);

	void UpdatePositionsOnly(const TArray<FVector>& NewPositions);
	void UpdatePositionsOnly(const TArray<FVector3f>& NewPositions);
	void InitIslandMotionMesh(
		const TArray<FVector>& Vertices,
		const TArray<int32>& Triangles,
		const TArray<FVector>& Normals,
		const TArray<FVector2D>& UVs,
		const TArray<FColor>& Colors,
		const TArray<FProcMeshTangent>& Tangents,
		const TArray<int32>& VertexIsland,
		const TArray<FVector>& IslandNormals,
		const TArray<float>& IslandPhases,
		float Amplitude,
		float Speed,
		float Smoothness);

	int32 GetVertexCount() const { return CachedVertexCount; }
	bool IsIslandMotionEnabled() const { return IslandBatches.Num() > 0; }

	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	virtual int32 GetNumMaterials() const override;
	virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;

private:
	TArray<FVector3f> PositionBuffer;
	TArray<FVector3f> NormalBuffer;
	TArray<FVector2f> UVBuffer;
	TArray<FColor> ColorBuffer;
	TArray<uint32> IndexBuffer;
	TArray<FBlenderGNFastIslandBatch> IslandBatches;
	TArray<FVector3f> TangentXBuffer;
	TArray<FVector4f> TangentZBuffer;
	FBox LocalBounds;
	float MotionAmplitude = 0.0f;
	float MotionSpeed = 1.0f;
	float MotionSmoothness = 0.0f;
	int32 CachedVertexCount = 0;
};
