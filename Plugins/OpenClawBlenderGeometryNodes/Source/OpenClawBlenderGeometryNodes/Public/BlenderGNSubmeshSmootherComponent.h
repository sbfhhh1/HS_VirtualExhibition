#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BlenderGNSubmeshSmootherComponent.generated.h"

class UBlenderGeometryNodesComponent;
class UProceduralMeshComponent;

UCLASS(ClassGroup = (OpenClaw), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class OPENCLAWBLENDERGEOMETRYNODES_API UBlenderGNSubmeshSmootherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBlenderGNSubmeshSmootherComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	TObjectPtr<UBlenderGeometryNodesComponent> Runtime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing")
	int32 SubmeshIndex = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing", meta = (ClampMin = "0", ClampMax = "24"))
	int32 Iterations = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Strength = 0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing", meta = (ClampMin = "-1.0", ClampMax = "0.0"))
	float InflateStrength = -0.48f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Smoothing", meta = (ClampMin = "100.0", ClampMax = "10000.0"))
	float WeldQuantization = 850.0f;

	UFUNCTION(BlueprintCallable, Category = "Smoothing")
	void SmoothCurrentMesh();

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UBlenderGeometryNodesComponent> BoundRuntime;

	bool bApplyingSmoothedMesh = false;

	UFUNCTION()
	void HandleMeshUpdated(UProceduralMeshComponent* ProceduralMesh);

	void BindRuntime();
	void UnbindRuntime();
};
