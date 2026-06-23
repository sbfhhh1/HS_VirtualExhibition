#include "TobiiPositionGuideComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TobiiEyeTrackerSubsystem.h"

FString UTobiiPositionGuideComponent::GetGuideText() const
{
	if (!GetWorld() || !GetWorld()->GetGameInstance())
	{
		return TEXT("Position guide unavailable");
	}
	const UTobiiEyeTrackerSubsystem* Tobii = GetWorld()->GetGameInstance()->GetSubsystem<UTobiiEyeTrackerSubsystem>();
	if (!Tobii)
	{
		return TEXT("Position guide unavailable");
	}
	const FTobiiHeadPoseSample Head = Tobii->GetLatestHeadPose();
	const FString Presence = StaticEnum<ETobiiUserPresence>()->GetNameStringByValue(static_cast<int64>(Tobii->GetUserPresence()));
	return FString::Printf(TEXT("Eye Tracker 5 position guide\nPresence: %s\nHead position: %.1f, %.1f, %.1f\nHead rotation: P %.1f Y %.1f R %.1f\nKeep your head centered and gaze data valid."),
		*Presence, Head.Position.X, Head.Position.Y, Head.Position.Z, Head.Rotation.Pitch, Head.Rotation.Yaw, Head.Rotation.Roll);
}

bool UTobiiPositionGuideComponent::HasUsablePositionData() const
{
	if (!GetWorld() || !GetWorld()->GetGameInstance())
	{
		return false;
	}
	const UTobiiEyeTrackerSubsystem* Tobii = GetWorld()->GetGameInstance()->GetSubsystem<UTobiiEyeTrackerSubsystem>();
	return Tobii && Tobii->GetLatestHeadPose().bValid;
}
