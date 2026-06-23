#pragma once

#include "CoreMinimal.h"
#include "TobiiEyeTrackerTypes.generated.h"

UENUM(BlueprintType)
enum class ETobiiUserPresence : uint8
{
	Unknown,
	Away,
	Present
};

USTRUCT(BlueprintType)
struct TOBIIEYETRACKER5_API FTobiiGazeSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tobii")
	int64 TimestampMicroseconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tobii")
	FVector2D RawDisplayPoint = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Tobii")
	FVector2D DisplayPoint = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Tobii")
	bool bValid = false;
};

USTRUCT(BlueprintType)
struct TOBIIEYETRACKER5_API FTobiiHeadPoseSample
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Tobii")
	int64 TimestampMicroseconds = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Tobii")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Tobii")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Tobii")
	bool bValid = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTobiiConnectionChanged, bool, bConnected);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTobiiGazeUpdated, const FTobiiGazeSample&, Sample);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTobiiPresenceChanged, ETobiiUserPresence, Presence);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FTobiiError, const FString&, Message);
