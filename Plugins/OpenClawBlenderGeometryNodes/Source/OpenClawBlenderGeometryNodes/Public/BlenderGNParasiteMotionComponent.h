#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BlenderGNParasiteMotionComponent.generated.h"

class UBlenderGeometryNodesComponent;
class UProceduralMeshComponent;

UCLASS(ClassGroup = (OpenClaw), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class OPENCLAWBLENDERGEOMETRYNODES_API UBlenderGNParasiteMotionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBlenderGNParasiteMotionComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	TObjectPtr<UBlenderGeometryNodesComponent> Runtime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
	bool bLiveMotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0.0", ClampMax = "18.0"))
	float GooAmplitude = 5.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0.0", ClampMax = "8.0"))
	float CageAmplitude = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float PulseSpeed = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0.2", ClampMax = "12.0"))
	float NoiseScale = 3.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Smoothness = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
	int32 GooSubmeshIndex = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Normals")
	bool bRecalculateAnimatedNormals = false;

	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetLiveMotion(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetGeometryPreview(const FString& SocketName, float Value);

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
	TArray<FVector> BaseNormals;
	TArray<bool> GooVertices;
	TArray<float> Phases;
	int32 CachedVertexCount = -1;
	float PreviewGooSize = 1.0f;
	float PreviewCageRadius = 1.0f;
	float PreviewWireRadius = 1.0f;
	float PreviewGooNoiseScale = 1.0f;
	float BaseGooSize = 1.0f;
	float BaseCageRadius = 1.06f;
	float BaseWireRadius = 0.022f;
	float BaseGooNoiseScale = 1.7f;

	UFUNCTION()
	void HandleMeshUpdated(UProceduralMeshComponent* ProceduralMesh);

	void BindRuntime();
	void UnbindRuntime();
	void CapturePreviewBaselines();
	void ResetGeometryPreview();
	void RebuildCache();
	void MarkGooVertices();
	void RebuildPhases();
	void ApplyCurrentFrame(float WorldTimeSeconds, bool bForce);
};
