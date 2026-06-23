#include "ExhibitionLightingRig.h"

#include "Components/PostProcessComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Scene.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	void ConfigureRectLight(
		URectLightComponent* Light,
		const FVector& Location,
		const FVector& Target,
		float Width,
		float Height)
	{
		Light->SetRelativeLocation(Location);
		Light->SetRelativeRotation((Target - Location).Rotation());
		Light->SetSourceWidth(Width);
		Light->SetSourceHeight(Height);
		Light->SetAttenuationRadius(1100.0f);
		Light->SetMobility(EComponentMobility::Movable);
		Light->SetCastShadows(true);
		Light->bUseTemperature = true;
		Light->IntensityUnits = ELightUnits::Lumens;
	}

	void ConfigureRoomSurface(
		UStaticMeshComponent* Surface,
		USceneComponent* Parent,
		UStaticMesh* Mesh,
		UMaterialInterface* Material,
		const FVector& Location,
		const FVector& Scale)
	{
		Surface->SetupAttachment(Parent);
		Surface->SetStaticMesh(Mesh);
		Surface->SetMaterial(0, Material);
		Surface->SetRelativeLocation(Location);
		Surface->SetRelativeScale3D(Scale);
		Surface->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Surface->SetCastShadow(true);
		Surface->SetMobility(EComponentMobility::Static);
	}
}

AExhibitionLightingRig::AExhibitionLightingRig()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	KeyLight = CreateDefaultSubobject<URectLightComponent>(TEXT("KeyLight_WarmSoftbox"));
	KeyLight->SetupAttachment(SceneRoot);
	FillLight = CreateDefaultSubobject<URectLightComponent>(TEXT("FillLight_CoolSoftbox"));
	FillLight->SetupAttachment(SceneRoot);
	TopLight = CreateDefaultSubobject<URectLightComponent>(TEXT("TopLight_GalleryPanel"));
	TopLight->SetupAttachment(SceneRoot);
	RimLight = CreateDefaultSubobject<USpotLightComponent>(TEXT("RimLight_Accent"));
	RimLight->SetupAttachment(SceneRoot);
	AmbientSkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("AmbientSkyLight"));
	AmbientSkyLight->SetupAttachment(SceneRoot);
	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("GalleryPostProcess"));
	PostProcess->SetupAttachment(SceneRoot);
	BackWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackWall"));
	LeftWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeftWall"));
	RightWall = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RightWall"));
	Ceiling = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ceiling"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> NeutralMaterial(
		TEXT("/Game/Sbsar/Plaster_MAT.Plaster_MAT"));

	ConfigureRoomSurface(
		BackWall, SceneRoot, CubeMesh.Object, NeutralMaterial.Object,
		FVector(460.0f, 0.0f, 250.0f), FVector(0.15f, 7.0f, 5.0f));
	ConfigureRoomSurface(
		LeftWall, SceneRoot, CubeMesh.Object, NeutralMaterial.Object,
		FVector(0.0f, -700.0f, 250.0f), FVector(9.0f, 0.15f, 5.0f));
	ConfigureRoomSurface(
		RightWall, SceneRoot, CubeMesh.Object, NeutralMaterial.Object,
		FVector(0.0f, 700.0f, 250.0f), FVector(9.0f, 0.15f, 5.0f));
	ConfigureRoomSurface(
		Ceiling, SceneRoot, CubeMesh.Object, NeutralMaterial.Object,
		FVector(0.0f, 0.0f, 500.0f), FVector(9.0f, 7.0f, 0.15f));

	KeyLight->SetRelativeLocation(FVector(-180.0f, -300.0f, 320.0f));
	FillLight->SetRelativeLocation(FVector(-80.0f, 350.0f, 240.0f));
	TopLight->SetRelativeLocation(FVector(0.0f, 0.0f, 445.0f));
	RimLight->SetRelativeLocation(FVector(350.0f, -120.0f, 290.0f));

	AmbientSkyLight->SetMobility(EComponentMobility::Movable);
	AmbientSkyLight->SetIntensity(0.32f);
	AmbientSkyLight->bRealTimeCapture = true;
	PostProcess->bUnbound = true;
	PostProcess->BlendWeight = 1.0f;
}

void AExhibitionLightingRig::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	const FVector Target(0.0f, 0.0f, 130.0f);
	ConfigureRectLight(KeyLight, FVector(-180.0f, -300.0f, 320.0f), Target, 320.0f, 220.0f);
	ConfigureRectLight(FillLight, FVector(-80.0f, 350.0f, 240.0f), Target, 400.0f, 280.0f);
	ConfigureRectLight(TopLight, FVector(0.0f, 0.0f, 445.0f), Target, 500.0f, 380.0f);
	KeyLight->SetupAttachment(SceneRoot);
	FillLight->SetupAttachment(SceneRoot);
	TopLight->SetupAttachment(SceneRoot);

	KeyLight->SetIntensity(KeyIntensity);
	KeyLight->SetTemperature(WarmTemperature);
	KeyLight->SetLightColor(FLinearColor(1.0f, 0.92f, 0.82f));
	FillLight->SetIntensity(FillIntensity);
	FillLight->SetTemperature(CoolTemperature);
	FillLight->SetLightColor(FLinearColor(0.78f, 0.88f, 1.0f));
	TopLight->SetIntensity(TopIntensity);
	TopLight->SetTemperature(5200.0f);
	TopLight->SetLightColor(FLinearColor(1.0f, 0.97f, 0.92f));

	RimLight->SetRelativeLocation(FVector(350.0f, -120.0f, 290.0f));
	RimLight->SetRelativeRotation((Target - RimLight->GetRelativeLocation()).Rotation());
	RimLight->SetMobility(EComponentMobility::Movable);
	RimLight->IntensityUnits = ELightUnits::Lumens;
	RimLight->bUseTemperature = true;
	RimLight->SetTemperature(CoolTemperature);
	RimLight->SetLightColor(FLinearColor(0.72f, 0.84f, 1.0f));
	RimLight->SetIntensity(RimIntensity);
	RimLight->SetAttenuationRadius(900.0f);
	RimLight->SetInnerConeAngle(22.0f);
	RimLight->SetOuterConeAngle(42.0f);
	RimLight->SetCastShadows(true);

	FPostProcessSettings& Settings = PostProcess->Settings;
	Settings.bOverride_AutoExposureMethod = true;
	Settings.AutoExposureMethod = AEM_Histogram;
	Settings.bOverride_AutoExposureMinBrightness = true;
	Settings.bOverride_AutoExposureMaxBrightness = true;
	Settings.AutoExposureMinBrightness = ExposureMinEV100;
	Settings.AutoExposureMaxBrightness = ExposureMaxEV100;
	Settings.bOverride_AutoExposureBias = true;
	Settings.AutoExposureBias = ExposureCompensation;
	Settings.bOverride_AutoExposureSpeedUp = true;
	Settings.AutoExposureSpeedUp = 2.0f;
	Settings.bOverride_AutoExposureSpeedDown = true;
	Settings.AutoExposureSpeedDown = 1.0f;
	Settings.bOverride_BloomIntensity = true;
	Settings.BloomIntensity = BloomIntensity;
	Settings.bOverride_VignetteIntensity = true;
	Settings.VignetteIntensity = VignetteIntensity;
	Settings.bOverride_WhiteTemp = true;
	Settings.WhiteTemp = 5350.0f;
	Settings.bOverride_ColorSaturation = true;
	Settings.ColorSaturation = FVector4(1.03f, 1.01f, 0.98f, 1.0f);
	Settings.bOverride_AmbientOcclusionIntensity = true;
	Settings.AmbientOcclusionIntensity = AmbientOcclusionIntensity;
	Settings.bOverride_AmbientOcclusionRadius = true;
	Settings.AmbientOcclusionRadius = 140.0f;
	Settings.bOverride_DynamicGlobalIlluminationMethod = true;
	Settings.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
	Settings.bOverride_ReflectionMethod = true;
	Settings.ReflectionMethod = EReflectionMethod::Lumen;
	Settings.bOverride_LumenSceneLightingQuality = true;
	Settings.LumenSceneLightingQuality = 1.5f;
	Settings.bOverride_LumenFinalGatherQuality = true;
	Settings.LumenFinalGatherQuality = 1.5f;
	Settings.bOverride_LumenReflectionQuality = true;
	Settings.LumenReflectionQuality = 1.5f;

}
