#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "ChineseSealPoolAnimator.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UProceduralMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

struct FChineseSealProceduralMesh
{
	FString Name;
	TArray<FVector> Vertices;
	TArray<int32> Triangles;
	TArray<FVector> TriangleNormals;
	TArray<FVector2D> TriangleUVs;
	TArray<FVector> SectionVertices;
	TArray<int32> SectionTriangles;
	TArray<FVector> SectionNormals;
	TArray<FVector2D> SectionUV0;
	TArray<FProcMeshTangent> SectionTangents;
};

UCLASS(Blueprintable, meta = (DisplayName = "Chinese Seal Pool Animator"))
class MCPGAMEPROJECT_API AChineseSealPoolAnimator : public AActor
{
	GENERATED_BODY()

public:
	AChineseSealPoolAnimator();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seal Meshes")
	TArray<TObjectPtr<UStaticMesh>> SealMeshes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seal Meshes")
	TObjectPtr<UMaterialInterface> OverrideMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seal Meshes")
	bool bUseImportedStaticMeshes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "1", ClampMax = "30"))
	int32 Rows = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "1", ClampMax = "30"))
	int32 Columns = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "10.0", ClampMax = "500.0"))
	float RowSpacing = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "10.0", ClampMax = "500.0"))
	float ColumnSpacing = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.01", ClampMax = "20.0"))
	float SealScale = 0.32f;

	/** Seal height is generated as cross-section side length multiplied by this value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.05", ClampMax = "10.0"))
	float SealHeightToSectionRatio = 2.0f;

	/** Actor origin is treated as the water surface; seals are held this far above it before animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0.0", ClampMax = "200.0"))
	float HeightAboveWater = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seal Material")
	FLinearColor DefaultSealColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seal Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultSealMetallic = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seal Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultSealRoughness = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Seal Material", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefaultSealSpecular = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bEnableAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bAnimateInEditor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float AnimationAmplitude = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float AnimationSpeed = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float AnimationFrequency = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AnimationUpdateInterval = 0.033f;

	/** 0 is regular sine motion; 1 adds per-cell phase, speed, amplitude and Perlin drift. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Irregularity = 0.68f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.01", ClampMax = "5.0"))
	float NoiseScale = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	int32 RandomSeed = 1937;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaTime) override;
	virtual bool ShouldTickIfViewportsOnly() const override;

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> SealComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> ProceduralSealComponents;

	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> MotionComponents;

	TArray<FVector> BaseRelativeLocations;
	TArray<float> Phases;
	TArray<float> SpeedMultipliers;
	TArray<float> AmplitudeMultipliers;
	TArray<FChineseSealProceduralMesh> ProceduralMeshes;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DefaultSealMaterial;

	float RunningTime = 0.0f;

	void RebuildSeals();
	void ClearSealComponents();
	void LoadDefaultMeshesIfNeeded();
	bool LoadProceduralMeshesIfNeeded();
	UMaterialInterface* ResolveSealMaterial();
	FVector GetScaleForStaticMesh(UStaticMesh* Mesh) const;
	FVector GetScaleForProceduralMesh(const FChineseSealProceduralMesh& MeshData) const;
	void UpdateSealMotion(float Time);
};

