#include "TobiiGazeTrailComponent.h"

#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TobiiEyeTrackerSubsystem.h"

UTobiiGazeTrailComponent::UTobiiGazeTrailComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTobiiGazeTrailComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	LatestHitActor = nullptr;
	if (!bEnabled || !GetWorld() || !GetWorld()->GetGameInstance())
	{
		return;
	}

	const UTobiiEyeTrackerSubsystem* Tobii = GetWorld()->GetGameInstance()->GetSubsystem<UTobiiEyeTrackerSubsystem>();
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!Tobii || !PC)
	{
		return;
	}

	const FTobiiGazeSample Gaze = Tobii->GetLatestGaze();
	if (!Gaze.bValid)
	{
		return;
	}

	int32 Width = 0;
	int32 Height = 0;
	PC->GetViewportSize(Width, Height);
	FVector Origin;
	FVector Direction;
	// Tobii Game Integration returns normalized coords with Y increasing upward (Y=0 at bottom,
	// Y=1 at top), while UE screen coords have Y increasing downward (Y=0 at top).  Invert Y.
	const FVector2D ScreenPoint(Gaze.DisplayPoint.X * Width, (1.0 - Gaze.DisplayPoint.Y) * Height);
	if (!PC->DeprojectScreenPositionToWorld(ScreenPoint.X, ScreenPoint.Y, Origin, Direction))
	{
		return;
	}

	// Debug: log gaze mapping every 120 frames (~2 seconds at 60fps)
	static int32 DebugFrameCounter = 0;
	if (++DebugFrameCounter % 120 == 0)
	{
		UE_LOG(LogTemp, Log, TEXT("TobiiGazeTrail: norm=(%.3f,%.3f) viewport=%dx%d screen=(%.0f,%.0f)"),
			Gaze.DisplayPoint.X, Gaze.DisplayPoint.Y, Width, Height, ScreenPoint.X, ScreenPoint.Y);
	}

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TobiiGazeTrail), true, GetOwner());
	if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Direction * TraceDistance, ECC_Visibility, Params))
	{
		LatestHitActor = Hit.GetActor();
		LatestHitLocation = Hit.ImpactPoint;
		DrawDebugSphere(GetWorld(), LatestHitLocation, TrailPointSize, 10, FColor::Green, false, 0.12f, 0, 1.5f);
	}
}
