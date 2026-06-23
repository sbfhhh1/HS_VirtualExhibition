#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TobiiPositionGuideComponent.generated.h"

UCLASS(ClassGroup = "Tobii", BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class TOBIIEYETRACKER5_API UTobiiPositionGuideComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Tobii|Position Guide")
	FString GetGuideText() const;

	UFUNCTION(BlueprintPure, Category = "Tobii|Position Guide")
	bool HasUsablePositionData() const;
};
