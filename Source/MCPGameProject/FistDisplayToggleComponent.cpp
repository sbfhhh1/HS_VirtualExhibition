#include "FistDisplayToggleComponent.h"

#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "LeapSubsystem.h"
#include "UltraleapTrackingData.h"

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

	PrimaryComponentTick.TickInterval = FMath::Max(0.0f, GesturePollInterval);
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
#if WITH_EDITOR
	if (Actor->GetActorLabel() == GroupString)
	{
		return true;
	}
#endif
	return Actor->GetName().Contains(GroupString);
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
	for (const FLeapHandData& Hand : Frame.Hands)
	{
		if (Hand.Confidence < MinHandConfidence)
		{
			continue;
		}
		bHandTracked = true;
		MaxGrabStrength = FMath::Max(MaxGrabStrength, Hand.GrabStrength);
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
	}

	if (!bEnableFistToggle)
	{
		return;
	}

	// 边沿检测：从"未握拳"变为"握拳"且冷却结束时切到下一组；需松开（降到 ReleaseThreshold 以下）才能再次触发。
	const bool bFistClosed = bHandTracked && MaxGrabStrength >= FistGrabThreshold;
	if (bFistClosed && !bFistLatched && TriggerCooldown <= 0.0f)
	{
		AdvanceGroup();
		bFistLatched = true;
		TriggerCooldown = TriggerCooldownSeconds;
	}
	else if (MaxGrabStrength < ReleaseThreshold)
	{
		bFistLatched = false;
	}
}
