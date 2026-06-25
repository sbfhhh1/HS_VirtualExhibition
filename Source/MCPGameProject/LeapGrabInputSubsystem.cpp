#include "LeapGrabInputSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "LeapSubsystem.h"

namespace
{
const FKey LeapGrabLeftKey(TEXT("LeapGrabL"));
const FKey LeapPinchLeftKey(TEXT("LeapPinchL"));
const FKey LeapGrabRightKey(TEXT("LeapGrabR"));
const FKey LeapPinchRightKey(TEXT("LeapPinchR"));
} // namespace

void ULeapGrabInputSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (ULeapSubsystem* LeapSubsystem = ULeapSubsystem::Get())
	{
		LeapFrameDelegateHandle =
			LeapSubsystem->OnLeapFrameMulti.AddUObject(this, &ULeapGrabInputSubsystem::OnLeapTrackingData);
	}
}

void ULeapGrabInputSubsystem::Deinitialize()
{
	if (LeapFrameDelegateHandle.IsValid())
	{
		if (ULeapSubsystem* LeapSubsystem = ULeapSubsystem::Get())
		{
			LeapSubsystem->OnLeapFrameMulti.Remove(LeapFrameDelegateHandle);
		}
		LeapFrameDelegateHandle.Reset();
	}

	Super::Deinitialize();
}

void ULeapGrabInputSubsystem::Tick(float DeltaTime)
{
	const UGameInstance* GameInstance = GetGameInstance();
	const UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World || World->WorldType != EWorldType::Game)
	{
		return;
	}

	UpdateHand(DeltaTime, LeftHand, LeapGrabLeftKey, LeapPinchLeftKey);
	UpdateHand(DeltaTime, RightHand, LeapGrabRightKey, LeapPinchRightKey);
}

void ULeapGrabInputSubsystem::OnLeapTrackingData(const FLeapFrameData& Frame)
{
	LeftHand.GrabStrength = 0.0f;
	LeftHand.PinchStrength = 0.0f;
	RightHand.GrabStrength = 0.0f;
	RightHand.PinchStrength = 0.0f;

	for (const FLeapHandData& Hand : Frame.Hands)
	{
		if (Hand.Confidence <= 0.0f)
		{
			continue;
		}

		FHandInputState* State = nullptr;
		if (Hand.HandType == EHandType::LEAP_HAND_LEFT)
		{
			State = &LeftHand;
		}
		else if (Hand.HandType == EHandType::LEAP_HAND_RIGHT)
		{
			State = &RightHand;
		}

		if (State)
		{
			State->GrabStrength = FMath::Max(State->GrabStrength, Hand.GrabStrength);
			State->PinchStrength = FMath::Max(State->PinchStrength, Hand.PinchStrength);
			State->TimeSinceSeen = 0.0f;
		}
	}
}

void ULeapGrabInputSubsystem::UpdateHand(float DeltaTime, FHandInputState& State, const FKey& GrabKey, const FKey& PinchKey)
{
	State.TimeSinceSeen += DeltaTime;
	if (State.TimeSinceSeen > HandStaleTimeout)
	{
		State.GrabStrength = 0.0f;
		State.PinchStrength = 0.0f;
	}

	if (!State.bGrabPressed && State.GrabStrength >= StartGrabThreshold)
	{
		State.bGrabPressed = true;
		SendKey(GrabKey, IE_Pressed);
	}
	else if (State.bGrabPressed && State.GrabStrength <= EndGrabThreshold)
	{
		State.bGrabPressed = false;
		SendKey(GrabKey, IE_Released);
	}

	if (!State.bPinchPressed && State.PinchStrength >= StartPinchThreshold)
	{
		State.bPinchPressed = true;
		SendKey(PinchKey, IE_Pressed);
	}
	else if (State.bPinchPressed && State.PinchStrength <= EndPinchThreshold)
	{
		State.bPinchPressed = false;
		SendKey(PinchKey, IE_Released);
	}
}

void ULeapGrabInputSubsystem::SendKey(const FKey& Key, EInputEvent Event)
{
	if (!Key.IsValid())
	{
		return;
	}

	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (APlayerController* PlayerController = GameInstance->GetFirstLocalPlayerController())
		{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
			PlayerController->InputKey(FInputKeyParams(Key, Event, 1.0));
PRAGMA_ENABLE_DEPRECATION_WARNINGS
		}
	}
}

TStatId ULeapGrabInputSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(ULeapGrabInputSubsystem, STATGROUP_Tickables);
}

bool ULeapGrabInputSubsystem::IsTickable() const
{
	return !IsTemplate() && GetGameInstance() != nullptr;
}
