#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TobiiScreenBasedDemoActor.generated.h"

class UTobiiDataRecorderComponent;
class UTobiiDemoWidget;
class UTobiiGazeTrailComponent;

UCLASS(Blueprintable)
class TOBIIEYETRACKER5_API ATobiiScreenBasedDemoActor : public AActor
{
	GENERATED_BODY()

public:
	ATobiiScreenBasedDemoActor();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tobii|Demo")
	TArray<TObjectPtr<AActor>> GazeTargets;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tobii|Demo")
	bool bEnableSimulationAtStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tobii|Demo")
	float DwellDurationSeconds = 2.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tobii|Demo")
	TObjectPtr<UTobiiGazeTrailComponent> GazeTrail;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tobii|Demo")
	TObjectPtr<UTobiiDataRecorderComponent> Recorder;

private:
	UPROPERTY(Transient) TObjectPtr<UTobiiDemoWidget> Widget;
	UPROPERTY(Transient) TObjectPtr<AActor> ActiveTarget;
	TArray<FString> Events;
	bool bGuideVisible = false;
	bool bLastGazeValid = false;
	double GazeLostAt = -1.0;
	float DwellSeconds = 0.0f;
	bool bDwellTriggered = false;
	double AutoQuitAt = -1.0;

	void HandleInput();
	void UpdateTarget(float DeltaTime);
	void UpdateWidget();
	void AppendEvent(const FString& Event);
	AActor* FindTargetForGaze(const FVector2D& DisplayPoint) const;
};
