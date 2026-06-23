#include "BlenderGNInertialDragRotateComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

UBlenderGNInertialDragRotateComponent::UBlenderGNInertialDragRotateComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBlenderGNInertialDragRotateComponent::BeginPlay()
{
	Super::BeginPlay();
	if (!Target && GetOwner())
	{
		Target = GetOwner()->GetRootComponent();
	}
}

void UBlenderGNInertialDragRotateComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	if (!Controller)
	{
		ApplyInertia(DeltaTime);
		return;
	}

	float MouseX = 0.0f;
	float MouseY = 0.0f;
	Controller->GetMousePosition(MouseX, MouseY);
	const FVector2D PointerPosition(MouseX, MouseY);
	const bool bPressed = Controller->IsInputKeyDown(EKeys::LeftMouseButton);
	const bool bDown = bPressed && !bWasPressed;
	const bool bUp = !bPressed && bWasPressed;
	bWasPressed = bPressed;

	if (bDown)
	{
		bDragging = !IsPointerOverUi();
		PreviousPointerPosition = PointerPosition;
		if (bDragging)
		{
			AngularVelocity = FVector2D::ZeroVector;
		}
	}

	if (bDragging && bPressed)
	{
		const FVector2D Delta = PointerPosition - PreviousPointerPosition;
		PreviousPointerPosition = PointerPosition;
		const float SafeDeltaTime = FMath::Max(DeltaTime, 0.0001f);
		const FVector2D Step = Delta * RotationSensitivity;
		ApplyRotation(Step);
		AngularVelocity = Step / SafeDeltaTime;
		AngularVelocity = AngularVelocity.GetClampedToMaxSize(MaxAngularSpeed);
	}

	if (bUp)
	{
		bDragging = false;
	}

	if (!bDragging)
	{
		ApplyInertia(DeltaTime);
	}
}

USceneComponent* UBlenderGNInertialDragRotateComponent::GetRotateTarget() const
{
	if (Target)
	{
		return Target;
	}
	return GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
}

bool UBlenderGNInertialDragRotateComponent::IsPointerOverUi() const
{
	// TODO: Wire this to the project UI hit-test layer if the runtime panel is built as native Slate/UMG.
	return false;
}

void UBlenderGNInertialDragRotateComponent::ApplyInertia(float DeltaTime)
{
	if (AngularVelocity.SizeSquared() < 0.001f)
	{
		return;
	}

	ApplyRotation(AngularVelocity * DeltaTime);
	AngularVelocity = FMath::Vector2DInterpTo(AngularVelocity, FVector2D::ZeroVector, DeltaTime, InertiaDamping);
}

void UBlenderGNInertialDragRotateComponent::ApplyRotation(const FVector2D& Step)
{
	USceneComponent* RotateTarget = GetRotateTarget();
	if (!RotateTarget)
	{
		return;
	}

	RotateTarget->AddWorldRotation(FRotator(0.0f, -Step.X, 0.0f));
	RotateTarget->AddWorldRotation(FRotator(Step.Y, 0.0f, 0.0f));
}
