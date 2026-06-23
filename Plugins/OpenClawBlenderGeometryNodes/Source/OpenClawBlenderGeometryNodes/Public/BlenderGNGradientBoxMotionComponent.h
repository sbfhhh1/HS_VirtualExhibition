#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BlenderGNFastDynamicMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "BlenderGNGradientBoxMotionComponent.generated.h"

class UBlenderGeometryNodesComponent;
class UProceduralMeshComponent;
class UBlenderGNRuntimePanelWidget;
struct FBlenderGNMeshData;

UCLASS(ClassGroup = (OpenClaw), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class OPENCLAWBLENDERGEOMETRYNODES_API UBlenderGNGradientBoxMotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBlenderGNGradientBoxMotionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	TObjectPtr<UBlenderGeometryNodesComponent> Runtime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	bool bLiveMotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Amplitude = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float Speed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Smoothness = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	bool bReversePinkAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "1.0", ClampMax = "120.0"))
	float MaxAnimationFrameRate = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
	bool bRecalculateAnimatedNormals = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
	bool bUseFastDynamicMesh = false;

	// Debug: force the connected-components fallback even if the ordered-cube heuristic accepts the
	// mesh. Use this to compare island grouping behavior against Unity's reference implementation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bForceConnectedComponentIslands = false;

	// Debug: log island statistics (count, target, average verts per island, tolerance) on every
	// RebuildCache. Helpful when faces appear flipped during animation.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bLogIslandStatistics = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime UI")
	bool bCreateRuntimeControlPanel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime UI")
	bool bShowMouseCursorForRuntimePanel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gradient Box|Runtime")
	bool bApplyReferenceInputsOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gradient Box|Runtime")
	bool bRefreshReferenceMeshOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Normals", meta = (ClampMin = "1000.0", ClampMax = "100000.0"))
	float NormalMergeQuantization = 10000.0f;

	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetLiveMotion(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Motion")
	void UpdateTickRate(float NewFPS);

	UFUNCTION(BlueprintCallable, Category = "Motion")
	void UpdateTickInterval();

	UFUNCTION(BlueprintCallable, Category = "Motion")
	int32 GetCachedIslandCount() const { return IslandCenters.Num(); }

	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetAmplitude(float Value);

	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetSpeed(float Value);

	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetSmoothness(float Value);

	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetReversePinkAnimation(bool bReverse);

	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetUseFastDynamicMesh(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetRecalculateAnimatedNormals(bool bEnabled);

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UBlenderGeometryNodesComponent> BoundRuntime;

	TArray<FVector> BaseVertices;
	TArray<FVector> BaseNormals;
	TArray<FVector> AnimatedVertices;
	TArray<FVector> AnimatedNormals;
	TArray<FLinearColor> ScratchVertexColors;
	TArray<FProcMeshTangent> ScratchTangents;
	TArray<FVector> IslandCenters;
	TArray<float> IslandPhases;
	TArray<float> IslandScales;
	TArray<float> IslandImportedScales;
	TArray<int32> VertexIsland;
	TArray<int32> IslandMaterialIndices;
	TArray<FVector> IslandNeutralHalfExtents;
	TArray<TArray<int32>> NormalGroups;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBlenderGNFastDynamicMeshComponent>> FastSectionMeshes;
	TArray<TArray<int32>> FastSectionGlobalVertices;
	TArray<TArray<FVector>> FastSectionAnimatedVertices;
	FVector NeutralHalfExtents = FVector(1.0);
	FBox BaseBounds;
	int32 CachedVertexCount = -1;
	double LastMotionUpdateTime = -999.0;
	double MotionStartTime = 0.0;

	UPROPERTY(Transient)
	TObjectPtr<UBlenderGNRuntimePanelWidget> RuntimeControlPanel;

	float CachedFrameInput = 1.0f;
	float CachedGnSpeedDegreesPerFrame = 5.0f;
	float CachedShiftAngleDegrees = 180.0f;
	float CachedPower = 2.0f;
	float CachedBlenderFrameRate = 60.0f;

	UFUNCTION()
	void HandleMeshUpdated(UProceduralMeshComponent* ProceduralMesh);

	void BindRuntime();
	void UnbindRuntime();
	void ApplyReferenceInputs();
	void RebuildCache();
	void CacheRuntimeParameters();
	void BuildIslands(const FBlenderGNMeshData& MeshData);
	void RefreshIslandPhasesAndImportedScales();
	void ApplyMotion(float WorldTimeSeconds);
	void ApplyMotionImmediately();
	void ApplyAnimatedVertices();
	void CreateRuntimeControlPanel();
	bool EnsureFastSectionMeshes();
	void DestroyFastSectionMeshes();
	bool ApplyAnimatedVerticesFast();

	int32 GetTargetIslandCount(const FBlenderGNMeshData& MeshData) const;
	TArray<int32> BuildVertexMaterialMap(const FBlenderGNMeshData& MeshData) const;
	bool BuildOrderedCubeIslands(const FBlenderGNMeshData& MeshData, const TArray<int32>& VertexMaterial, TArray<int32>& OutVertexIsland, TArray<FVector>& OutIslandCenters, TArray<int32>& OutIslandMaterials) const;
	TArray<int32> BuildWeldMap(float Tolerance, const TArray<int32>& VertexMaterial) const;
	bool BuildConnectedIslandsForTolerance(const FBlenderGNMeshData& MeshData, float Tolerance, const TArray<int32>& VertexMaterial, TArray<int32>& OutVertexIsland, TArray<FVector>& OutIslandCenters, TArray<int32>& OutIslandMaterials) const;
	float ScoreIslandCandidate(int32 IslandCount, int32 TargetIslandCount) const;
	float EstimateAverageEdgeLength(const FBlenderGNMeshData& MeshData) const;
	FVector EstimateNeutralHalfExtents() const;
	TArray<FVector> EstimateIslandNeutralHalfExtents() const;
	FVector GetNeutralHalfExtentsForIsland(int32 Island) const;
	FVector GetNeutralDelta(int32 VertexIndex, int32 Island) const;
	FVector BuildFallbackNeutralDelta(int32 VertexIndex, int32 Island, const FVector& Delta) const;
	float CalculateBlenderScale(float Phase) const;
	bool IsPinkMaterial(int32 MaterialIndex) const;
};
