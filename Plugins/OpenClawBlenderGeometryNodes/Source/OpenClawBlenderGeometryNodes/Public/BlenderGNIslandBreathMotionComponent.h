#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "BlenderGNIslandBreathMotionComponent.generated.h"

class UBlenderGeometryNodesComponent;
class UProceduralMeshComponent;

UCLASS(ClassGroup = (OpenClaw), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class OPENCLAWBLENDERGEOMETRYNODES_API UBlenderGNIslandBreathMotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBlenderGNIslandBreathMotionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	TObjectPtr<UBlenderGeometryNodesComponent> Runtime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	bool bLiveMotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0.0", ClampMax = "35.0"))
	float Amplitude = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float Speed = 1.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0.2", ClampMax = "10.0"))
	float NoiseScale = 1.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Smoothness = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "1.0", ClampMax = "120.0"))
	float MaxAnimationFrameRate = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance")
	bool bRecalculateAnimatedNormals = false;

	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetLiveMotion(bool bEnabled);

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UBlenderGeometryNodesComponent> BoundRuntime;

	TArray<FVector> BaseVertices;
	TArray<FVector> AnimatedVertices;
	TArray<FVector> AnimatedNormals;
	TArray<FLinearColor> ScratchVertexColors;
	TArray<FProcMeshTangent> ScratchTangents;
	TArray<FVector> IslandNormals;
	TArray<FVector> IslandOffsets;
	TArray<float> IslandPhases;
	TArray<int32> VertexIsland;
	int32 CachedVertexCount = -1;
	double LastMotionUpdateTime = -999.0;

	UFUNCTION()
	void HandleMeshUpdated(UProceduralMeshComponent* ProceduralMesh);

	void BindRuntime();
	void UnbindRuntime();
	void RebuildCache();
	void BuildIslands();
	void UpdateTickInterval();
	void ApplyMotion(float WorldTimeSeconds);
};
