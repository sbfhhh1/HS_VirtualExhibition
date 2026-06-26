#include "DoorInteractionComponent.h"

#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UDoorInteractionComponent::UDoorInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDoorInteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	CacheClosedAndOpenLocations();

	// 找主 UI 的 WidgetComponent
	if (MainUIActor)
	{
		CachedUIWidget = MainUIActor->FindComponentByClass<UWidgetComponent>();
	}

	// 默认关门态
	bTargetOpen = false;
	Progress = 0.f;
	ApplyProgress();

	// 为 BG 等创建动态材质实例、缓存原始速度参数
	SetupShaderControls();

	// 默认关门 -> 暂停被门挡住的动态内容
	bContentActive = true; // 强制下面真正执行一次暂停
	SetGatedContentActive(false);
}

void UDoorInteractionComponent::CacheClosedAndOpenLocations()
{
	const FQuat OwnerRot = GetOwner() ? GetOwner()->GetActorQuat() : FQuat::Identity;

	if (LeftDoor)
	{
		LeftClosedLocation = LeftDoor->GetActorLocation();
		LeftOpenLocation = LeftClosedLocation + OwnerRot.RotateVector(LeftDoorOpenOffset);
	}
	if (RightDoor)
	{
		RightClosedLocation = RightDoor->GetActorLocation();
		RightOpenLocation = RightClosedLocation + OwnerRot.RotateVector(RightDoorOpenOffset);
	}
	bInitialized = true;
}

void UDoorInteractionComponent::ToggleDoor()
{
	bTargetOpen = !bTargetOpen;

	// 开门：立即恢复动态内容，随门开显现并重新旋转/跑 shader
	if (bTargetOpen)
	{
		SetGatedContentActive(true);
	}

	USoundBase* Sound = bTargetOpen ? OpenSound : CloseSound;
	if (Sound)
	{
		const FVector Loc = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;

		// 让音效时长适应开关门动画时长：pitch = 音效原长 / 门动画时长。
		// 门 2s、音效 1.5s -> pitch 0.75（放慢拉到 2s）；音效 3s -> pitch 1.5（加快压到 2s）。
		float PitchMultiplier = 1.f;
		const float SoundDuration = Sound->GetDuration();
		if (SoundDuration > 0.f && SoundDuration < 1.0e8f && OpenCloseDuration > 0.05f)
		{
			PitchMultiplier = SoundDuration / OpenCloseDuration;
		}
		UGameplayStatics::PlaySoundAtLocation(this, Sound, Loc, 1.f, PitchMultiplier);
	}

	UE_LOG(LogTemp, Display, TEXT("DoorInteraction: toggle -> %s"),
		bTargetOpen ? TEXT("open") : TEXT("close"));
}

void UDoorInteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bInitialized)
	{
		return;
	}

	// 轮询切换键（边沿触发）
	if (const UWorld* World = GetWorld())
	{
		if (const APlayerController* PC = World->GetFirstPlayerController())
		{
			if (ToggleKey.IsValid() && PC->WasInputKeyJustPressed(ToggleKey))
			{
				ToggleDoor();
			}
		}
	}

	// 朝目标推进进度
	const float Target = bTargetOpen ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(Progress, Target))
	{
		const float Step = DeltaTime / FMath::Max(OpenCloseDuration, 0.05f);
		Progress = (Progress < Target) ? FMath::Min(Progress + Step, Target)
		                               : FMath::Max(Progress - Step, Target);
		ApplyProgress();

		// 完全关闭后暂停动态内容（隐藏+停 Tick）
		if (!bTargetOpen && Progress <= 0.f)
		{
			SetGatedContentActive(false);
		}
	}
}

void UDoorInteractionComponent::SetGatedContentActive(bool bActive)
{
	if (bContentActive == bActive)
	{
		return;
	}
	bContentActive = bActive;

	// 玉石/印章池：冻结逐帧动画(停转/停动)，保持可见
	for (AActor* Actor : PauseActorsWhenClosed)
	{
		if (Actor)
		{
			Actor->CustomTimeDilation = bActive ? 1.f : 0.f;
		}
	}

	// BG 等：置零/还原材质速度参数，暂停/恢复纯 shader 动效
	for (int32 d = 0; d < ShaderDMIs.Num(); ++d)
	{
		UMaterialInstanceDynamic* DMI = ShaderDMIs[d].Get();
		if (!DMI)
		{
			continue;
		}
		for (int32 p = 0; p < ShaderSpeedParameterNames.Num(); ++p)
		{
			const float Value = (bActive && ShaderOriginalValues.IsValidIndex(d)
				&& ShaderOriginalValues[d].IsValidIndex(p))
				? ShaderOriginalValues[d][p] : 0.f;
			DMI->SetScalarParameterValue(ShaderSpeedParameterNames[p], Value);
		}
	}
}

void UDoorInteractionComponent::SetupShaderControls()
{
	ShaderDMIs.Reset();
	ShaderOriginalValues.Reset();

	for (AActor* Actor : PauseShaderActorsWhenClosed)
	{
		if (!Actor)
		{
			continue;
		}
		TArray<UStaticMeshComponent*> Meshes;
		Actor->GetComponents(Meshes);
		for (UStaticMeshComponent* Mesh : Meshes)
		{
			if (!Mesh)
			{
				continue;
			}
			const int32 NumMats = Mesh->GetNumMaterials();
			for (int32 i = 0; i < NumMats; ++i)
			{
				UMaterialInterface* Src = Mesh->GetMaterial(i);
				if (!Src)
				{
					continue;
				}
				UMaterialInstanceDynamic* DMI =
					Mesh->CreateAndSetMaterialInstanceDynamicFromMaterial(i, Src);
				if (!DMI)
				{
					continue;
				}
				TArray<float> Originals;
				for (const FName& ParamName : ShaderSpeedParameterNames)
				{
					Originals.Add(DMI->K2_GetScalarParameterValue(ParamName));
				}
				ShaderDMIs.Add(DMI);
				ShaderOriginalValues.Add(Originals);
			}
		}
	}
}

void UDoorInteractionComponent::ApplyProgress()
{
	// 缓动，开关门更自然
	const float A = FMath::SmoothStep(0.f, 1.f, Progress);

	if (LeftDoor)
	{
		LeftDoor->SetActorLocation(FMath::Lerp(LeftClosedLocation, LeftOpenLocation, A));
	}
	if (RightDoor)
	{
		RightDoor->SetActorLocation(FMath::Lerp(RightClosedLocation, RightOpenLocation, A));
	}

	// 主 UI：关门(A=0)不透明，开门(A=1)淡出
	if (UWidgetComponent* Widget = CachedUIWidget.Get())
	{
		Widget->SetTintColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, 1.f - A));
	}
}
