#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TobiiGazeTrailComponent.generated.h"

UCLASS(ClassGroup = "Tobii", BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class TOBIIEYETRACKER5_API UTobiiGazeTrailComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTobiiGazeTrailComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tobii")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tobii")
	float TraceDistance = 100000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tobii")
	float TrailPointSize = 8.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Tobii")
	TObjectPtr<AActor> LatestHitActor;

	UPROPERTY(BlueprintReadOnly, Category = "Tobii")
	FVector LatestHitLocation = FVector::ZeroVector;
};
