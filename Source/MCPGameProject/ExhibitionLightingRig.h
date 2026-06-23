#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ExhibitionLightingRig.generated.h"

class UPostProcessComponent;
class URectLightComponent;
class USceneComponent;
class USkyLightComponent;
class USpotLightComponent;
class UStaticMeshComponent;

UCLASS(BlueprintType)
class MCPGAMEPROJECT_API AExhibitionLightingRig : public AActor
{
	GENERATED_BODY()

public:
	AExhibitionLightingRig();
	virtual void OnConstruction(const FTransform& Transform) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exhibition|Lighting")
	float KeyIntensity = 7000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exhibition|Lighting")
	float FillIntensity = 3500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exhibition|Lighting")
	float TopIntensity = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exhibition|Lighting")
	float RimIntensity = 6000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exhibition|Lighting")
	float WarmTemperature = 4700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exhibition|Lighting")
	float CoolTemperature = 5900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exhibition|Post Process")
	float ExposureMinEV100 = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exhibition|Post Process")
	float ExposureMaxEV100 = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exhibition|Post Process")
	float ExposureCompensation = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exhibition|Post Process")
	float BloomIntensity = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exhibition|Post Process")
	float VignetteIntensity = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Exhibition|Post Process")
	float AmbientOcclusionIntensity = 0.65f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Exhibition|Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Exhibition|Components")
	TObjectPtr<URectLightComponent> KeyLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Exhibition|Components")
	TObjectPtr<URectLightComponent> FillLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Exhibition|Components")
	TObjectPtr<URectLightComponent> TopLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Exhibition|Components")
	TObjectPtr<USpotLightComponent> RimLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Exhibition|Components")
	TObjectPtr<USkyLightComponent> AmbientSkyLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Exhibition|Components")
	TObjectPtr<UPostProcessComponent> PostProcess;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Exhibition|Architecture")
	TObjectPtr<UStaticMeshComponent> BackWall;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Exhibition|Architecture")
	TObjectPtr<UStaticMeshComponent> LeftWall;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Exhibition|Architecture")
	TObjectPtr<UStaticMeshComponent> RightWall;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Exhibition|Architecture")
	TObjectPtr<UStaticMeshComponent> Ceiling;
};

