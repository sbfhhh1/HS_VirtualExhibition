#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "BlenderGNWallNoiseMotionComponent.generated.h"

class UBlenderGeometryNodesComponent;
class UProceduralMeshComponent;

UENUM(BlueprintType)
enum class EBlenderGNDisplacementAxis : uint8
{
	X,
	Y,
	Z
};

UCLASS(ClassGroup = (OpenClaw), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class OPENCLAWBLENDERGEOMETRYNODES_API UBlenderGNWallNoiseMotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBlenderGNWallNoiseMotionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	TObjectPtr<UBlenderGeometryNodesComponent> Runtime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	bool bLiveMotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	EBlenderGNDisplacementAxis DisplacementAxis = EBlenderGNDisplacementAxis::Z;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0.0", ClampMax = "150.0"))
	float Strength = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "-5.0", ClampMax = "5.0"))
	float Speed = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0.1", ClampMax = "12.0"))
	float Scale = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Smoothness = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Performance", meta = (ClampMin = "1.0", ClampMax = "120.0"))
	float MaxAnimationFrameRate = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Normals")
	bool bRecalculateAnimatedNormals = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Normals")
	bool bSmoothSplitNormals = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Normals", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float SmoothingAngle = 72.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Normals", meta = (ClampMin = "1000.0", ClampMax = "100000.0"))
	float PositionMergeQuantization = 10000.0f;

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
	TArray<TArray<int32>> NormalGroups;
	int32 CachedVertexCount = -1;
	double LastMotionUpdateTime = -999.0;

	UFUNCTION()
	void HandleMeshUpdated(UProceduralMeshComponent* ProceduralMesh);

	void BindRuntime();
	void UnbindRuntime();
	void RebuildCache();
	void SyncFromRuntimeInputs();
	void UpdateTickInterval();
	void ApplyMotion(float WorldTimeSeconds);
};
