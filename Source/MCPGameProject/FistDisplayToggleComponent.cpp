#include "FistDisplayToggleComponent.h"

#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "LeapSubsystem.h"
#include "UltraleapTrackingData.h"

namespace
{
FString NormalizeGroupToken(FString Value)
{
	Value.ToLowerInline();
	Value.ReplaceInline(TEXT("_"), TEXT(""));
	Value.ReplaceInline(TEXT("-"), TEXT(""));
	Value.ReplaceInline(TEXT(" "), TEXT(""));
	return Value;
}

bool IsFingerCurlingFallback(const FLeapHandData& Hand)
{
	if (Hand.GrabStrength > 0.0f || Hand.PinchStrength > 0.0f)
	{
		return false;
	}

	int32 KnownFingerCount = 0;
	int32 CurledFingerCount = 0;
	for (const FLeapDigitData& Digit : Hand.Digits)
	{
		++KnownFingerCount;
		if (!Digit.IsExtended)
		{
			++CurledFingerCount;
		}
	}

	return KnownFingerCount >= 4 && CurledFingerCount >= 4;
}

float EstimateFingerCurlStrength(const FLeapHandData& Hand)
{
	int32 KnownFingerCount = 0;
	int32 CurledFingerCount = 0;
	for (const FLeapDigitData& Digit : Hand.Digits)
	{
		++KnownFingerCount;
		if (!Digit.IsExtended)
		{
			++CurledFingerCount;
		}
	}

	if (KnownFingerCount <= 0)
	{
		return IsFingerCurlingFallback(Hand) ? 1.0f : 0.0f;
	}

	return static_cast<float>(CurledFingerCount) / static_cast<float>(KnownFingerCount);
}
} // namespace

UFistDisplayToggleComponent::UFistDisplayToggleComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = GesturePollInterval;
	// 默认在 BG 与中国印章池之间轮流；BG 通过标签匹配（StaticMeshActor 标签为 BG），
	// 印章池 Actor 带有 "ChineseSealPool" Tag（set_chinese_seal_defaults.py 设置）。
	DisplayGroupTags = {TEXT("BG"), TEXT("ChineseSealPool")};
}

void UFistDisplayToggleComponent::BeginPlay()
{
	Super::BeginPlay();

	// 每帧 Tick：保证键盘(WasInputKeyJustPressed 逐帧边沿触发)不被漏按；
	// 手势仍靠 bFistLatched + 冷却防重复，每帧轮询缓存的 Leap 数据开销可忽略。
	PrimaryComponentTick.TickInterval = 0.0f;
	ResolveGroups();

	if (ULeapSubsystem* LeapSubsystem = ULeapSubsystem::Get())
	{
		LeapFrameDelegateHandle =
			LeapSubsystem->OnLeapFrameMulti.AddUObject(this, &UFistDisplayToggleComponent::OnLeapTrackingData);
	}

	// 起始显示初始分组。
	ShowGroup(InitialGroupIndex);
}

void UFistDisplayToggleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (LeapFrameDelegateHandle.IsValid())
	{
		if (ULeapSubsystem* LeapSubsystem = ULeapSubsystem::Get())
		{
			LeapSubsystem->OnLeapFrameMulti.Remove(LeapFrameDelegateHandle);
		}
		LeapFrameDelegateHandle.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void UFistDisplayToggleComponent::ResolveGroups()
{
	ManagedActors.Reset();
	ManagedActorGroups.Reset();

	UWorld* World = GetWorld();
	if (!World || DisplayGroupTags.Num() == 0)
	{
		return;
	}

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor == GetOwner())
		{
			continue;
		}
		for (int32 GroupIndex = 0; GroupIndex < DisplayGroupTags.Num(); ++GroupIndex)
		{
			if (ActorMatchesGroup(Actor, DisplayGroupTags[GroupIndex]))
			{
				ManagedActors.Add(Actor);
				ManagedActorGroups.Add(GroupIndex);
				UE_LOG(LogTemp, Display, TEXT("FistDisplayToggle: group %d (%s) manages actor %s class %s."),
					GroupIndex,
					*DisplayGroupTags[GroupIndex].ToString(),
					*GetNameSafe(Actor),
					*GetNameSafe(Actor->GetClass()));
				break;
			}
		}
	}

	UE_LOG(LogTemp, Display, TEXT("FistDisplayToggle: resolved %d managed actor(s) across %d group(s)."),
		ManagedActors.Num(), DisplayGroupTags.Num());
}

bool UFistDisplayToggleComponent::ActorMatchesGroup(const AActor* Actor, FName GroupTag) const
{
	if (!Actor || GroupTag.IsNone())
	{
		return false;
	}
	if (Actor->ActorHasTag(GroupTag))
	{
		return true;
	}
	const FString GroupString = GroupTag.ToString();
	const FString NormalizedGroup = NormalizeGroupToken(GroupString);
	for (const FName& ActorTag : Actor->Tags)
	{
		if (NormalizeGroupToken(ActorTag.ToString()) == NormalizedGroup)
		{
			return true;
		}
	}
	// 不再用 GetActorLabel() 匹配：label 仅在带编辑器的构建(WITH_EDITOR)下有效，
	// 打包后会被编译掉导致匹配不到（编辑器/Standalone 正常、打包失效）。
	// 统一靠真 Tag 或对象名/类名匹配，保证编辑器与打包行为一致。
	const FString NormalizedActorName = NormalizeGroupToken(Actor->GetName());
	const FString NormalizedClassName = NormalizeGroupToken(GetNameSafe(Actor->GetClass()));
	return NormalizedActorName.Contains(NormalizedGroup) || NormalizedClassName.Contains(NormalizedGroup);
}

void UFistDisplayToggleComponent::ShowGroup(int32 GroupIndex)
{
	const int32 GroupCount = DisplayGroupTags.Num();
	if (GroupCount <= 0)
	{
		return;
	}
	CurrentGroupIndex = ((GroupIndex % GroupCount) + GroupCount) % GroupCount;

	for (int32 Index = 0; Index < ManagedActors.Num(); ++Index)
	{
		AActor* Actor = ManagedActors[Index];
		if (!Actor)
		{
			continue;
		}
		const bool bVisible = (ManagedActorGroups[Index] == CurrentGroupIndex);
		Actor->SetActorHiddenInGame(!bVisible);
		Actor->SetActorEnableCollision(bVisible);

		TArray<USceneComponent*> SceneComponents;
		Actor->GetComponents(SceneComponents);
		for (USceneComponent* SceneComponent : SceneComponents)
		{
			if (!SceneComponent)
			{
				continue;
			}
			SceneComponent->SetHiddenInGame(!bVisible, true);
			SceneComponent->SetVisibility(bVisible, true);
		}
	}
	UE_LOG(LogTemp, Display, TEXT("FistDisplayToggle: showing group %d (%s)."),
		CurrentGroupIndex, *DisplayGroupTags[CurrentGroupIndex].ToString());
}

void UFistDisplayToggleComponent::AdvanceGroup()
{
	ShowGroup(CurrentGroupIndex + 1);
}

void UFistDisplayToggleComponent::OnLeapTrackingData(const FLeapFrameData& Frame)
{
	TimeSinceLeapFrame = 0.0f;
	bHandTracked = false;
	MaxGrabStrength = 0.0f;
	MaxPinchStrength = 0.0f;
	MaxFingerCurlStrength = 0.0f;
	for (const FLeapHandData& Hand : Frame.Hands)
	{
		if (Hand.Confidence < MinHandConfidence)
		{
			continue;
		}
		bHandTracked = true;
		MaxGrabStrength = FMath::Max(MaxGrabStrength, Hand.GrabStrength);
		MaxPinchStrength = FMath::Max(MaxPinchStrength, Hand.PinchStrength);
		MaxFingerCurlStrength = FMath::Max(MaxFingerCurlStrength, EstimateFingerCurlStrength(Hand));
	}
}

void UFistDisplayToggleComponent::TickComponent(
	float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TriggerCooldown = FMath::Max(0.0f, TriggerCooldown - DeltaTime);

	// 数据新鲜度兜底：Leap 停止推帧时视为无手，避免陈旧 GrabStrength 误触发。
	constexpr float LeapStaleTimeout = 0.25f;
	TimeSinceLeapFrame += DeltaTime;
	if (TimeSinceLeapFrame > LeapStaleTimeout)
	{
		bHandTracked = false;
		MaxGrabStrength = 0.0f;
		MaxPinchStrength = 0.0f;
		MaxFingerCurlStrength = 0.0f;
	}

	if (!bEnableFistToggle)
	{
		return;
	}

	// 边沿检测：从"未握拳"变为"握拳"且冷却结束时切到下一组；需松开（降到 ReleaseThreshold 以下）才能再次触发。
	const float GestureStrength = FMath::Max3(MaxGrabStrength, MaxPinchStrength, MaxFingerCurlStrength);
	const bool bFistClosed = bHandTracked && GestureStrength >= FistGrabThreshold;
	if (bFistClosed && !bFistLatched && TriggerCooldown <= 0.0f)
	{
		UE_LOG(LogTemp, Display,
			TEXT("FistDisplayToggle: grab trigger strength=%.2f grab=%.2f pinch=%.2f curl=%.2f."),
			GestureStrength,
			MaxGrabStrength,
			MaxPinchStrength,
			MaxFingerCurlStrength);
		AdvanceGroup();
		bFistLatched = true;
		TriggerCooldown = TriggerCooldownSeconds;
	}
	else if (GestureStrength < ReleaseThreshold)
	{
		bFistLatched = false;
	}

	// 调试用：数字 4 键等价于抓取手势，触发印章/背景轮回（与冷却共享，防重复触发）。
	if (const UWorld* World = GetWorld())
	{
		if (APlayerController* PlayerController = World->GetFirstPlayerController())
		{
			// 键盘 4 键：边沿触发本身就是一次一切，不加冷却，确保每次按下都稳定 toggle。
			if (PlayerController->WasInputKeyJustPressed(EKeys::Four))
			{
				UE_LOG(LogTemp, Display, TEXT("FistDisplayToggle: key '4' -> AdvanceGroup."));
				AdvanceGroup();
			}
		}
	}
}
