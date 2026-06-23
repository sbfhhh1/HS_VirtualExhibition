#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BlenderGNInertialDragRotateComponent.generated.h"

UCLASS(ClassGroup = (OpenClaw), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class OPENCLAWBLENDERGEOMETRYNODES_API UBlenderGNInertialDragRotateComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBlenderGNInertialDragRotateComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	TObjectPtr<USceneComponent> Target;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input")
	bool bIgnoreDragOverUi = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.02", ClampMax = "1.0"))
	float RotationSensitivity = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.5", ClampMax = "12.0"))
	float InertiaDamping = 4.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxAngularSpeed = 120.0f;

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool bDragging = false;
	bool bWasPressed = false;
	FVector2D PreviousPointerPosition = FVector2D::ZeroVector;
	FVector2D AngularVelocity = FVector2D::ZeroVector;

	USceneComponent* GetRotateTarget() const;
	bool IsPointerOverUi() const;
	void ApplyInertia(float DeltaTime);
	void ApplyRotation(const FVector2D& Step);
};
