#include "OpenClawCreateGradientBoxSceneCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/TaskGraphInterfaces.h"
#include "BlenderGeometryNodesActor.h"
#include "BlenderGeometryNodesComponent.h"
#include "BlenderGNGradientBoxMotionComponent.h"
#include "BlenderGNInertialDragRotateComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/SkyLight.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "ImageUtils.h"
#include "Materials/Material.h"
#include "Misc/PackageName.h"
#include "TimerManager.h"
#include "UObject/SavePackage.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(OpenClawCreateGradientBoxSceneCommandlet)

DEFINE_LOG_CATEGORY_STATIC(LogOpenClawGradientBoxScene, Log, All);

namespace
{
constexpr TCHAR GradientBoxFolder[] = TEXT("/Game/GradientBox");
constexpr TCHAR GradientBoxMapPath[] = TEXT("/Game/GradientBox/Procedural_GradientBox_GN_Runtime");
constexpr TCHAR GradientBoxTemplateMap[] = TEXT("/Engine/Maps/Templates/Template_Default");
constexpr TCHAR GradientBoxBlendFile[] = TEXT("C:/unity_project/Unity_Blender_GN/Assets/Generated/BlenderGN/Gradient Box/Gradient Box.blend");

bool SaveAssetPackage(UObject* Asset)
{
	if (!Asset)
	{
		return false;
	}

	UPackage* Package = Asset->GetOutermost();
	const FString PackageFileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(Package, Asset, *PackageFileName, SaveArgs);
}

UMaterial* CreateOrUpdateMaterial(const TCHAR* AssetName, const FLinearColor& Color)
{
	const FString PackageName = FString::Printf(TEXT("%s/%s"), GradientBoxFolder, AssetName);
	UMaterial* Material = LoadObject<UMaterial>(nullptr, *FString::Printf(TEXT("%s.%s"), *PackageName, AssetName));
	if (!Material)
	{
		UPackage* Package = CreatePackage(*PackageName);
		Material = NewObject<UMaterial>(Package, AssetName, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Material);
	}

	if (!Material)
	{
		return nullptr;
	}

	Material->PreEditChange(nullptr);
	UMaterialEditorOnlyData* EditorOnly = Material->GetEditorOnlyData();
	EditorOnly->BaseColor.UseConstant = true;
	EditorOnly->BaseColor.Constant = Color;
	EditorOnly->Roughness.UseConstant = true;
	EditorOnly->Roughness.Constant = 0.45f;
	Material->PostEditChange();
	Material->MarkPackageDirty();
	Material->ForceRecompileForRendering();
	SaveAssetPackage(Material);
	return Material;
}

FBlenderGNInput MakeIntInput(const FString& SocketName, const FString& DisplayName, int32 Value, float Min, float Max)
{
	FBlenderGNInput Input;
	Input.SocketName = SocketName;
	Input.DisplayName = DisplayName;
	Input.FallbackIdentifier = SocketName;
	Input.Type = EBlenderGNParameterType::Int;
	Input.IntValue = Value;
	Input.SliderMin = Min;
	Input.SliderMax = Max;
	Input.bShowInRuntimePanel = true;
	return Input;
}

FBlenderGNInput MakeFloatInput(const FString& SocketName, const FString& DisplayName, float Value, float Min, float Max)
{
	FBlenderGNInput Input;
	Input.SocketName = SocketName;
	Input.DisplayName = DisplayName;
	Input.FallbackIdentifier = SocketName;
	Input.Type = EBlenderGNParameterType::Float;
	Input.FloatValue = Value;
	Input.SliderMin = Min;
	Input.SliderMax = Max;
	Input.bShowInRuntimePanel = true;
	return Input;
}

void ConfigureRuntime(UBlenderGeometryNodesComponent* Runtime, UMaterialInterface* BlueMaterial, UMaterialInterface* PinkMaterial)
{
	if (!Runtime)
	{
		return;
	}

	Runtime->BlenderExecutable = UBlenderGeometryNodesComponent::FindDefaultBlenderPath();
	Runtime->BlendFile.FilePath = GradientBoxBlendFile;
	Runtime->ObjectName = TEXT("Cube");
	Runtime->ModifierName = TEXT("GradientBoxes");
	Runtime->SidecarScriptPath = UBlenderGeometryNodesComponent::GetDefaultSidecarPath();
	Runtime->RefreshMode = EBlenderGNRefreshMode::Manual;
	Runtime->BlenderTimeoutSeconds = 45;
	Runtime->MinimumRefreshInterval = 0.08f;
	Runtime->FixedRefreshInterval = 0.12f;
	Runtime->BlenderFrameRate = 60.0f;
	Runtime->BlenderFrameOffset = 1;
	Runtime->bRefreshOnBeginPlay = false;
	Runtime->bUseBinaryMeshCache = false;
	Runtime->bApplyImportedNormals = true;
	Runtime->bSmoothNormalsByPosition = false;
	Runtime->bEnablePreviewAnimation = false;
	Runtime->bCastShadows = true;
	Runtime->BlenderToUnrealScale = 100.0f;
	Runtime->Inputs.Reset();
	Runtime->Inputs.Add(MakeIntInput(TEXT("Socket_2"), TEXT("num"), 10, 1.0f, 20.0f));
	Runtime->Inputs.Add(MakeFloatInput(TEXT("Socket_3"), TEXT("size"), 1.1236745f, 0.1f, 3.0f));
	Runtime->Inputs.Add(MakeFloatInput(TEXT("Socket_8"), TEXT("gap"), 0.15f, 0.0f, 0.6f));
	Runtime->Inputs.Add(MakeFloatInput(TEXT("Socket_4"), TEXT("frame"), 1.0f, 0.5f, 5.0f));
	Runtime->Inputs.Add(MakeFloatInput(TEXT("Socket_5"), TEXT("speed"), 5.0f, 1.0f, 10.0f));
	Runtime->Inputs.Add(MakeFloatInput(TEXT("Socket_6"), TEXT("power"), 2.0f, 1.0f, 5.0f));
	Runtime->Inputs.Add(MakeFloatInput(TEXT("Socket_7"), TEXT("shift ang"), 180.0f, 0.0f, 360.0f));
	Runtime->MaterialOverrides.Reset();
	Runtime->MaterialOverrides.SetNum(3);
	Runtime->MaterialOverrides[1] = PinkMaterial;
	Runtime->MaterialOverrides[2] = BlueMaterial;
}

void ConfigureMotion(UBlenderGNGradientBoxMotionComponent* Motion, UBlenderGeometryNodesComponent* Runtime)
{
	if (!Motion)
	{
		return;
	}

	Motion->Runtime = Runtime;
	Motion->bLiveMotion = true;
	Motion->Amplitude = 1.0f;
	Motion->Speed = 0.49f;
	Motion->Smoothness = 1.0f;
	Motion->MaxAnimationFrameRate = 60.0f;
	Motion->bRecalculateAnimatedNormals = false;
	Motion->bUseFastDynamicMesh = false;
}

void AddFloor(UWorld* World)
{
	AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), FVector(0.0, 0.0, -85.0), FRotator::ZeroRotator);
	if (!Floor)
	{
		return;
	}

	Floor->SetActorLabel(TEXT("GradientBox_Neutral_Floor"));
	Floor->SetActorScale3D(FVector(12.0, 12.0, 0.05));
	if (UStaticMeshComponent* Mesh = Floor->GetStaticMeshComponent())
	{
		if (UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")))
		{
			Mesh->SetStaticMesh(Plane);
		}
		Mesh->SetMobility(EComponentMobility::Static);
	}
}

void AddLight(UWorld* World, const TCHAR* Label, const FRotator& Rotation, float Intensity, const FLinearColor& Color)
{
	ADirectionalLight* Light = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), FVector::ZeroVector, Rotation);
	if (!Light)
	{
		return;
	}

	Light->SetActorLabel(Label);
	if (UDirectionalLightComponent* Component = Light->GetComponent())
	{
		Component->SetIntensity(Intensity);
		Component->SetLightColor(Color);
		Component->SetCastShadows(true);
	}
}

void AddTemplateFallbackEnvironment(UWorld* World)
{
	if (!World)
	{
		return;
	}

	if (!World->SpawnActor<ASkyAtmosphere>(ASkyAtmosphere::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator))
	{
		UE_LOG(LogOpenClawGradientBoxScene, Warning, TEXT("Failed to spawn SkyAtmosphere fallback."));
	}

	ASkyLight* SkyLight = World->SpawnActor<ASkyLight>(ASkyLight::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	if (SkyLight && SkyLight->GetLightComponent())
	{
		SkyLight->SetActorLabel(TEXT("GradientBox_SkyLight"));
		SkyLight->GetLightComponent()->SetIntensity(1.0f);
		SkyLight->GetLightComponent()->bRealTimeCapture = true;
	}

	AddFloor(World);
	AddLight(World, TEXT("Directional_Light_Key"), FRotator(-42.0, -35.0, 0.0), 4.0f, FLinearColor(1.0f, 0.95f, 0.88f, 1.0f));
	AddLight(World, TEXT("Directional_Light_Fill"), FRotator(-18.0, 120.0, 0.0), 1.15f, FLinearColor(0.70f, 0.82f, 1.0f, 1.0f));
}

void AddGradientBoxPostProcess(UWorld* World)
{
	if (!World)
	{
		return;
	}

	APostProcessVolume* Volume = World->SpawnActor<APostProcessVolume>(APostProcessVolume::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator);
	if (!Volume)
	{
		return;
	}

	Volume->SetActorLabel(TEXT("GradientBox_Crisp_PostProcess"));
	Volume->bUnbound = true;
	Volume->BlendWeight = 1.0f;
	Volume->Settings.bOverride_MotionBlurAmount = true;
	Volume->Settings.MotionBlurAmount = 0.0f;
	Volume->Settings.bOverride_MotionBlurMax = true;
	Volume->Settings.MotionBlurMax = 0.0f;
	Volume->Settings.bOverride_MotionBlurPerObjectSize = true;
	Volume->Settings.MotionBlurPerObjectSize = 0.0f;
}

bool CaptureGradientBoxFrame(UWorld* World, const FString& FileName, const FVector& Location, const FRotator& Rotation)
{
	if (!World)
	{
		return false;
	}

	ASceneCapture2D* CaptureActor = World->SpawnActor<ASceneCapture2D>(ASceneCapture2D::StaticClass(), Location, Rotation);
	if (!CaptureActor || !CaptureActor->GetCaptureComponent2D())
	{
		return false;
	}

	UTextureRenderTarget2D* RenderTarget = NewObject<UTextureRenderTarget2D>();
	RenderTarget->RenderTargetFormat = RTF_RGBA8;
	RenderTarget->ClearColor = FLinearColor(0.045f, 0.052f, 0.060f, 1.0f);
	RenderTarget->InitAutoFormat(1280, 720);
	RenderTarget->UpdateResourceImmediate(true);

	USceneCaptureComponent2D* Capture = CaptureActor->GetCaptureComponent2D();
	Capture->TextureTarget = RenderTarget;
	Capture->FOVAngle = 42.0f;
	Capture->bCaptureEveryFrame = false;
	Capture->bCaptureOnMovement = false;
	Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
	Capture->CaptureScene();
	FlushRenderingCommands();

	TArray<FColor> Pixels;
	FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
	const bool bReadPixels = Resource && Resource->ReadPixels(Pixels);
	CaptureActor->Destroy();
	if (!bReadPixels || Pixels.Num() == 0)
	{
		return false;
	}

	TArray64<uint8> PngData;
	FImageUtils::PNGCompressImageArray(1280, 720, Pixels, PngData);
	return FFileHelper::SaveArrayToFile(PngData, *FileName);
}

void CaptureVerificationFrames(UWorld* World, UBlenderGNGradientBoxMotionComponent* Motion)
{
	const FString OutputDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GradientBoxVerify"));
	IFileManager::Get().MakeDirectory(*OutputDir, true);

	const FVector CaptureLocation(700.0, -900.0, 520.0);
	const FVector CaptureTarget(0.0, 0.0, 120.0);
	const FRotator CaptureRotation = (CaptureTarget - CaptureLocation).Rotation();
	const FString StaticPath = FPaths::Combine(OutputDir, TEXT("ue_gradient_box_static.png"));
	const FString Frame07Path = FPaths::Combine(OutputDir, TEXT("ue_gradient_box_0.70s.png"));
	const FString Frame14Path = FPaths::Combine(OutputDir, TEXT("ue_gradient_box_1.40s.png"));

	if (CaptureGradientBoxFrame(World, StaticPath, CaptureLocation, CaptureRotation))
	{
		UE_LOG(LogOpenClawGradientBoxScene, Display, TEXT("Captured %s"), *StaticPath);
	}
	else
	{
		UE_LOG(LogOpenClawGradientBoxScene, Warning, TEXT("Could not capture static Gradient Box frame."));
	}

	if (Motion)
	{
		Motion->SetLiveMotion(true);
	}

	World->Tick(ELevelTick::LEVELTICK_All, 0.7f);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	if (CaptureGradientBoxFrame(World, Frame07Path, CaptureLocation, CaptureRotation))
	{
		UE_LOG(LogOpenClawGradientBoxScene, Display, TEXT("Captured %s"), *Frame07Path);
	}
	else
	{
		UE_LOG(LogOpenClawGradientBoxScene, Warning, TEXT("Could not capture 0.70s Gradient Box frame."));
	}

	World->Tick(ELevelTick::LEVELTICK_All, 0.7f);
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	if (CaptureGradientBoxFrame(World, Frame14Path, CaptureLocation, CaptureRotation))
	{
		UE_LOG(LogOpenClawGradientBoxScene, Display, TEXT("Captured %s"), *Frame14Path);
	}
	else
	{
		UE_LOG(LogOpenClawGradientBoxScene, Warning, TEXT("Could not capture 1.40s Gradient Box frame."));
	}
}
}

UOpenClawCreateGradientBoxSceneCommandlet::UOpenClawCreateGradientBoxSceneCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UOpenClawCreateGradientBoxSceneCommandlet::Main(const FString& Params)
{
	UE_LOG(LogOpenClawGradientBoxScene, Display, TEXT("Creating Gradient Box runtime scene..."));

	if (!FPaths::FileExists(GradientBoxBlendFile))
	{
		UE_LOG(LogOpenClawGradientBoxScene, Error, TEXT("Blend file not found: %s"), GradientBoxBlendFile);
		return 1;
	}

	IFileManager::Get().MakeDirectory(*FPackageName::LongPackageNameToFilename(GradientBoxFolder), true);

	UMaterial* BlueMaterial = CreateOrUpdateMaterial(TEXT("M_GradientBox_Blue"), FLinearColor(0.0f, 0.08f, 0.95f, 1.0f));
	UMaterial* PinkMaterial = CreateOrUpdateMaterial(TEXT("M_GradientBox_Pink"), FLinearColor(1.0f, 0.08f, 0.48f, 1.0f));
	if (!BlueMaterial || !PinkMaterial)
	{
		UE_LOG(LogOpenClawGradientBoxScene, Error, TEXT("Failed to create Gradient Box materials."));
		return 1;
	}

	UWorld* World = UEditorLoadingAndSavingUtils::NewMapFromTemplate(GradientBoxTemplateMap, false);
	if (!World)
	{
		World = UEditorLoadingAndSavingUtils::NewBlankMap(false);
		if (World)
		{
			AddTemplateFallbackEnvironment(World);
		}
	}
	if (!World)
	{
		UE_LOG(LogOpenClawGradientBoxScene, Error, TEXT("Failed to create an editor map."));
		return 1;
	}

	ABlenderGeometryNodesActor* RuntimeActor = World->SpawnActor<ABlenderGeometryNodesActor>(
		ABlenderGeometryNodesActor::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator);
	if (!RuntimeActor)
	{
		UE_LOG(LogOpenClawGradientBoxScene, Error, TEXT("Failed to spawn Gradient Box runtime actor."));
		return 1;
	}

	RuntimeActor->SetActorLabel(TEXT("GradientBoxGN_Runtime"));
	RuntimeActor->SetActorLocation(FVector(0.0, 0.0, 120.0));
	ConfigureRuntime(RuntimeActor->GeometryNodes, BlueMaterial, PinkMaterial);
	RuntimeActor->ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	UBlenderGNGradientBoxMotionComponent* Motion = NewObject<UBlenderGNGradientBoxMotionComponent>(RuntimeActor, TEXT("GradientBoxMotion"));
	RuntimeActor->AddInstanceComponent(Motion);
	Motion->RegisterComponent();
	ConfigureMotion(Motion, RuntimeActor->GeometryNodes);

	UBlenderGNInertialDragRotateComponent* Drag = NewObject<UBlenderGNInertialDragRotateComponent>(RuntimeActor, TEXT("GradientBoxDragRotate"));
	RuntimeActor->AddInstanceComponent(Drag);
	Drag->Target = RuntimeActor->GetRootComponent();
	Drag->RegisterComponent();

	const FVector CameraLocation(760.0, -980.0, 560.0);
	const FVector CameraTarget(0.0, 0.0, 120.0);
	ACameraActor* Camera = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), CameraLocation, (CameraTarget - CameraLocation).Rotation());
	if (Camera && Camera->GetCameraComponent())
	{
		Camera->SetActorLabel(TEXT("GradientBox_Camera"));
		Camera->GetCameraComponent()->SetFieldOfView(42.0f);
		Camera->GetCameraComponent()->PostProcessBlendWeight = 1.0f;
		Camera->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount = true;
		Camera->GetCameraComponent()->PostProcessSettings.MotionBlurAmount = 0.0f;
		Camera->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurMax = true;
		Camera->GetCameraComponent()->PostProcessSettings.MotionBlurMax = 0.0f;
		Camera->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurPerObjectSize = true;
		Camera->GetCameraComponent()->PostProcessSettings.MotionBlurPerObjectSize = 0.0f;
	}

	AddGradientBoxPostProcess(World);

	const bool bSavedMap = UEditorLoadingAndSavingUtils::SaveMap(World, GradientBoxMapPath);
	UEditorLoadingAndSavingUtils::SaveDirtyPackages(false, true);
	if (!bSavedMap)
	{
		UE_LOG(LogOpenClawGradientBoxScene, Error, TEXT("Failed to save map: %s"), GradientBoxMapPath);
		return 1;
	}

	UE_LOG(LogOpenClawGradientBoxScene, Display, TEXT("Created %s with actor GradientBoxGN_Runtime."), GradientBoxMapPath);
	return 0;
}

UOpenClawVerifyGradientBoxSceneCommandlet::UOpenClawVerifyGradientBoxSceneCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UOpenClawVerifyGradientBoxSceneCommandlet::Main(const FString& Params)
{
	const FString MapFilename = FPackageName::LongPackageNameToFilename(GradientBoxMapPath, FPackageName::GetMapPackageExtension());
	UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(MapFilename);
	if (!World)
	{
		UE_LOG(LogOpenClawGradientBoxScene, Error, TEXT("Failed to load map: %s"), GradientBoxMapPath);
		return 1;
	}

	ABlenderGeometryNodesActor* RuntimeActor = nullptr;
	for (TActorIterator<ABlenderGeometryNodesActor> It(World); It; ++It)
	{
		if (It->GetActorLabel() == TEXT("GradientBoxGN_Runtime"))
		{
			RuntimeActor = *It;
			break;
		}
	}

	if (!RuntimeActor || !RuntimeActor->GeometryNodes)
	{
		UE_LOG(LogOpenClawGradientBoxScene, Error, TEXT("GradientBoxGN_Runtime actor or GeometryNodes component was not found."));
		return 1;
	}

	UBlenderGNGradientBoxMotionComponent* Motion = RuntimeActor->FindComponentByClass<UBlenderGNGradientBoxMotionComponent>();
	RuntimeActor->GeometryNodes->RefreshNow();
	const double Start = FPlatformTime::Seconds();
	while ((RuntimeActor->GeometryNodes->bIsRunning || RuntimeActor->GeometryNodes->LastVertexCount == 0)
		&& FPlatformTime::Seconds() - Start < 90.0)
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
		World->Tick(ELevelTick::LEVELTICK_All, 0.016f);
		FPlatformProcess::Sleep(0.016f);
	}

	if (RuntimeActor->GeometryNodes->bIsRunning)
	{
		UE_LOG(LogOpenClawGradientBoxScene, Error, TEXT("Timed out waiting for Blender refresh. Status: %s"), *RuntimeActor->GeometryNodes->LastStatus);
		return 1;
	}

	if (RuntimeActor->GeometryNodes->LastVertexCount <= 0 || RuntimeActor->GeometryNodes->LastTriangleCount <= 0)
	{
		UE_LOG(LogOpenClawGradientBoxScene, Error, TEXT("Blender refresh produced no mesh. Status: %s"), *RuntimeActor->GeometryNodes->LastStatus);
		return 1;
	}

	if (Motion)
	{
		Motion->SetLiveMotion(false);
	}

	const FBlenderGNMeshData& MeshData = RuntimeActor->GeometryNodes->GetLastMeshData();
	UE_LOG(LogOpenClawGradientBoxScene, Display, TEXT("Gradient Box refresh status: %s"), *RuntimeActor->GeometryNodes->LastStatus);
	UE_LOG(LogOpenClawGradientBoxScene, Display, TEXT("Vertices=%d Triangles=%d Sections=%d Materials=%d ImportedNormals=%s MotionIslands=%d"),
		RuntimeActor->GeometryNodes->LastVertexCount,
		RuntimeActor->GeometryNodes->LastTriangleCount,
		MeshData.Sections.Num(),
		MeshData.Materials.Num(),
		MeshData.bImportedNormals ? TEXT("true") : TEXT("false"),
		Motion ? Motion->GetCachedIslandCount() : -1);

	if (MeshData.Sections.Num() < 2)
	{
		UE_LOG(LogOpenClawGradientBoxScene, Error, TEXT("Expected pink/blue material sections, but only found %d sections."), MeshData.Sections.Num());
		return 1;
	}

	if (!MeshData.bImportedNormals)
	{
		UE_LOG(LogOpenClawGradientBoxScene, Warning, TEXT("Mesh did not report imported normals; visual normals may rely on generated normals."));
	}

	CaptureVerificationFrames(World, Motion);
	UEditorLoadingAndSavingUtils::SaveMap(World, GradientBoxMapPath);
	return 0;
}
