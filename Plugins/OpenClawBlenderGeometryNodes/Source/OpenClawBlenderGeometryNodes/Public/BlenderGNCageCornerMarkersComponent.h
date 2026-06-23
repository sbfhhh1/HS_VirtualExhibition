#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BlenderGNCageCornerMarkersComponent.generated.h"

class UBlenderGeometryNodesComponent;
class UMaterialInterface;
class UProceduralMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(ClassGroup = (OpenClaw), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class OPENCLAWBLENDERGEOMETRYNODES_API UBlenderGNCageCornerMarkersComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBlenderGNCageCornerMarkersComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	TObjectPtr<UBlenderGeometryNodesComponent> Runtime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers")
	TObjectPtr<UMaterialInterface> MarkerMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers")
	int32 CageSubmeshIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers", meta = (ClampMin = "2.0", ClampMax = "18.0"))
	float MarkerRadius = 10.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers", meta = (ClampMin = "0.985", ClampMax = "0.9998"))
	float DirectionMergeDot = 0.994f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Markers", meta = (ClampMin = "0.7", ClampMax = "1.3"))
	float SurfaceRadiusThreshold = 0.92f;

	UFUNCTION(BlueprintCallable, Category = "Markers")
	void RebuildMarkers();

	UFUNCTION(BlueprintCallable, Category = "Markers")
	void SetMarkerRadius(float Radius);

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UBlenderGeometryNodesComponent> BoundRuntime;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMesh> MarkerMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> Markers;

	UFUNCTION()
	void HandleMeshUpdated(UProceduralMeshComponent* ProceduralMesh);

	void BindRuntime();
	void UnbindRuntime();
	void EnsureMarkerCount(int32 Count);
	void ApplyMarkerScale();
};
