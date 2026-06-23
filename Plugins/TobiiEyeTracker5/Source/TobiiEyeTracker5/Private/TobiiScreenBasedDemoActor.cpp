#include "TobiiScreenBasedDemoActor.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMeshActor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "TobiiDataRecorderComponent.h"
#include "TobiiDemoWidget.h"
#include "TobiiEyeTrackerSubsystem.h"
#include "TobiiGazeTrailComponent.h"

namespace
{
	FString GetRuntimeActorName(const AActor* Actor)
	{
		if (!Actor)
		{
			return TEXT("none");
		}
#if WITH_EDITOR
		return Actor->GetActorLabel();
#else
		return Actor->GetName();
#endif
	}

	AActor* FindDemoTarget(UWorld* World, const FString& EditorLabel, const FString& RuntimeName)
	{
		TArray<AActor*> Actors;
		UGameplayStatics::GetAllActorsOfClass(World, AStaticMeshActor::StaticClass(), Actors);
		for (AActor* Actor : Actors)
		{
			if (!Actor)
			{
				continue;
			}
#if WITH_EDITOR
			if (Actor->GetActorLabel() == EditorLabel)
			{
				return Actor;
			}
#endif
			if (Actor->GetName() == RuntimeName)
			{
				return Actor;
			}
		}
		return nullptr;
	}
}

ATobiiScreenBasedDemoActor::ATobiiScreenBasedDemoActor()
{
	PrimaryActorTick.bCanEverTick = true;
	GazeTrail = CreateDefaultSubobject<UTobiiGazeTrailComponent>(TEXT("GazeTrail"));
	Recorder = CreateDefaultSubobject<UTobiiDataRecorderComponent>(TEXT("Recorder"));
}

void ATobiiScreenBasedDemoActor::BeginPlay()
{
	Super::BeginPlay();
	if (GazeTargets.Num() != 3 || GazeTargets.Contains(nullptr))
	{
		GazeTargets = {
			FindDemoTarget(GetWorld(), TEXT("CubeLeft"), TEXT("StaticMeshActor_1")),
			FindDemoTarget(GetWorld(), TEXT("CubeMiddle"), TEXT("StaticMeshActor_2")),
			FindDemoTarget(GetWorld(), TEXT("CubeRight"), TEXT("StaticMeshActor_3"))
		};
		UE_LOG(LogTemp, Log, TEXT("Tobii demo: restored %d/3 gaze target references at runtime."),
			GazeTargets.FilterByPredicate([](const AActor* Target) { return Target != nullptr; }).Num());
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UTobiiEyeTrackerSubsystem* Tobii = GI->GetSubsystem<UTobiiEyeTrackerSubsystem>())
		{
			Tobii->SetSimulationEnabled(bEnableSimulationAtStart || FParse::Param(FCommandLine::Get(), TEXT("TobiiSimulate")));
		}
	}
	if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
	{
		Widget = CreateWidget<UTobiiDemoWidget>(PC, UTobiiDemoWidget::StaticClass());
		if (Widget) Widget->AddToViewport();
	}
	AppendEvent(TEXT("HUD ready"));
	if (FParse::Param(FCommandLine::Get(), TEXT("TobiiAutoRecord")))
	{
		Recorder->StartRecording();
		AppendEvent(TEXT("Automatic smoke-test recording started"));
	}
	float AutoQuitSeconds = 0.0f;
	if (FParse::Value(FCommandLine::Get(), TEXT("TobiiAutoQuit="), AutoQuitSeconds) && AutoQuitSeconds > 0.0f)
	{
		AutoQuitAt = FPlatformTime::Seconds() + AutoQuitSeconds;
	}
}

void ATobiiScreenBasedDemoActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	HandleInput();
	UpdateTarget(DeltaTime);
	UpdateWidget();
	if (AutoQuitAt > 0.0 && FPlatformTime::Seconds() >= AutoQuitAt)
	{
		Recorder->StopRecording();
		UKismetSystemLibrary::QuitGame(this, GetWorld()->GetFirstPlayerController(), EQuitPreference::Quit, false);
	}
}

void ATobiiScreenBasedDemoActor::HandleInput()
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	UTobiiEyeTrackerSubsystem* Tobii = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTobiiEyeTrackerSubsystem>() : nullptr;
	if (!PC || !Tobii) return;

	if (PC->WasInputKeyJustPressed(EKeys::C))
	{
		AppendEvent(Tobii->LaunchSystemCalibration() ? TEXT("Opened Tobii system calibration") : TEXT("Calibration tool unavailable"));
	}
	if (PC->WasInputKeyJustPressed(EKeys::T))
	{
		bGuideVisible = !bGuideVisible;
		AppendEvent(bGuideVisible ? TEXT("Position guide shown") : TEXT("Position guide hidden"));
	}
	if (PC->WasInputKeyJustPressed(EKeys::S))
	{
		if (Recorder->IsRecording()) Recorder->StopRecording(); else Recorder->StartRecording();
		AppendEvent(Recorder->IsRecording() ? TEXT("Recording started") : TEXT("Recording stopped"));
	}
	if (PC->WasInputKeyJustPressed(EKeys::F8))
	{
		Tobii->SetSimulationEnabled(!Tobii->IsSimulationEnabled());
		AppendEvent(Tobii->IsSimulationEnabled() ? TEXT("Simulation enabled") : TEXT("Simulation disabled"));
	}
	if (PC->WasInputKeyJustPressed(EKeys::Escape))
	{
		UKismetSystemLibrary::QuitGame(this, PC, EQuitPreference::Quit, false);
	}
}

void ATobiiScreenBasedDemoActor::UpdateTarget(float DeltaTime)
{
	UTobiiEyeTrackerSubsystem* Tobii = GetGameInstance() ? GetGameInstance()->GetSubsystem<UTobiiEyeTrackerSubsystem>() : nullptr;
	if (!Tobii) return;
	const FTobiiGazeSample Gaze = Tobii->GetLatestGaze();

	if (Gaze.bValid != bLastGazeValid)
	{
		if (Gaze.bValid)
		{
			AppendEvent(TEXT("Gaze resumed"));
			if (GazeLostAt >= 0.0 && FPlatformTime::Seconds() - GazeLostAt < 0.35) AppendEvent(TEXT("Blink inferred"));
		}
		else
		{
			GazeLostAt = FPlatformTime::Seconds();
			AppendEvent(TEXT("Gaze lost"));
		}
		bLastGazeValid = Gaze.bValid;
	}

	AActor* NewTarget = Gaze.bValid ? FindTargetForGaze(Gaze.DisplayPoint) : nullptr;
	if (NewTarget != ActiveTarget)
	{
		ActiveTarget = NewTarget;
		DwellSeconds = 0.0f;
		bDwellTriggered = false;
		AppendEvent(ActiveTarget ? FString::Printf(TEXT("Focus on %s"), *GetRuntimeActorName(ActiveTarget)) : TEXT("Focus cleared"));
	}
	if (ActiveTarget)
	{
		ActiveTarget->AddActorWorldRotation(FRotator(0, 120.0f * DeltaTime, 0));
		DwellSeconds += DeltaTime;
		if (!bDwellTriggered && DwellSeconds >= DwellDurationSeconds)
		{
			bDwellTriggered = true;
			AppendEvent(FString::Printf(TEXT("Dwell trigger: %s"), *GetRuntimeActorName(ActiveTarget)));
		}
	}
	Recorder->CurrentTarget = ActiveTarget ? GetRuntimeActorName(ActiveTarget) : TEXT("");
	Recorder->DwellProgress = FMath::Clamp(DwellSeconds / DwellDurationSeconds, 0.0f, 1.0f);
}

AActor* ATobiiScreenBasedDemoActor::FindTargetForGaze(const FVector2D& DisplayPoint) const
{
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC) return nullptr;
	int32 Width = 0, Height = 0;
	PC->GetViewportSize(Width, Height);

	// DisplayPoint is Tobii-native (Y=0 bottom, Y=1 top).
	// Convert to UE screen-normalized coords (Y=0 top, Y=1 bottom) for comparison.
	const double GazeUENormX = DisplayPoint.X;
	const double GazeUENormY = 1.0 - DisplayPoint.Y;

	AActor* Best = nullptr;
	double BestDistance = DBL_MAX;
	for (AActor* Target : GazeTargets)
	{
		if (!Target) continue;
		FVector2D Screen;
		if (PC->ProjectWorldLocationToScreen(Target->GetActorLocation(), Screen, true))
		{
			const double ProjNormX = Width > 0 ? Screen.X / Width : 0.0;
			const double ProjNormY = Height > 0 ? Screen.Y / Height : 0.0;
			const double Distance = FMath::Sqrt(FMath::Square(GazeUENormX - ProjNormX) + FMath::Square(GazeUENormY - ProjNormY));
			// Debug log every ~4 seconds
			static int32 DiagFrame = 0;
			if (++DiagFrame % 240 == 0)
			{
				UE_LOG(LogTemp, Log, TEXT("FindTarget: %s proj=(%.3f,%.3f) gaze=(%.3f,%.3f) dist=%.4f"),
					*GetRuntimeActorName(Target), ProjNormX, ProjNormY, GazeUENormX, GazeUENormY, Distance);
			}
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = Target;
			}
		}
	}
	if (BestDistance <= 0.20 && GazeUENormY >= 0.05 && GazeUENormY <= 0.95)
	{
		return Best;
	}

	// Fallback: match left/middle/right screen regions using UE-normalized coordinates.
	if (GazeTargets.Num() == 3 && GazeUENormY >= 0.05 && GazeUENormY <= 0.95)
	{
		const double CanonicalX[] = { 0.25, 0.50, 0.75 };
		int32 BestIndex = 0;
		double CanonicalDistance = DBL_MAX;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const double Distance = FMath::Abs(GazeUENormX - CanonicalX[Index]);
			if (Distance < CanonicalDistance)
			{
				CanonicalDistance = Distance;
				BestIndex = Index;
			}
		}
		return CanonicalDistance <= 0.20 ? GazeTargets[BestIndex] : nullptr;
	}
	return nullptr;
}

void ATobiiScreenBasedDemoActor::UpdateWidget()
{
	if (!Widget || !GetGameInstance()) return;
	UTobiiEyeTrackerSubsystem* Tobii = GetGameInstance()->GetSubsystem<UTobiiEyeTrackerSubsystem>();
	if (!Tobii) return;
	const FTobiiGazeSample Gaze = Tobii->GetLatestGaze();
	const FTobiiHeadPoseSample Head = Tobii->GetLatestHeadPose();
	const FString PresenceText = StaticEnum<ETobiiUserPresence>()->GetNameStringByValue(static_cast<int64>(Tobii->GetUserPresence()));
	const FString TargetText = GetRuntimeActorName(ActiveTarget);
	Widget->SetStatus(FString::Printf(
		TEXT("TOBII EYE TRACKER 5 / GAME INTEGRATION\nConnected: %s  Available: %s  Simulation: %s\nGaze: %s  Point: %.3f, %.3f  Samples: %d  Age: %.2fs\nTarget: %s  Dwell: %.0f%%\nPresence: %s\nHead pose: %s  P %.1f  Y %.1f  R %.1f\nRecording: %s\n\nC Calibration   T Position Guide   S Record XML   F8 Simulation   Esc Quit"),
		Tobii->IsConnected() ? TEXT("yes") : TEXT("no"), Tobii->IsAvailable() ? TEXT("yes") : TEXT("no"),
		Tobii->IsSimulationEnabled() ? TEXT("yes") : TEXT("no"), Gaze.bValid ? TEXT("valid") : TEXT("invalid"),
		Gaze.DisplayPoint.X, Gaze.DisplayPoint.Y, Tobii->GetGazeSampleCount(), Tobii->GetLastGazeAgeSeconds(),
		*TargetText, FMath::Clamp(DwellSeconds / DwellDurationSeconds, 0.0f, 1.0f) * 100.0f,
		*PresenceText, Head.bValid ? TEXT("valid") : TEXT("invalid"), Head.Rotation.Pitch, Head.Rotation.Yaw, Head.Rotation.Roll,
		Recorder->IsRecording() ? TEXT("yes") : TEXT("no")));
	Widget->SetEventLog(FString::Join(Events, TEXT("\n")));
	Widget->SetGuide(FString::Printf(TEXT("POSITION GUIDE (ET5 equivalent)\nPresence: %s\nHead position: %.1f, %.1f, %.1f\nKeep your head centered and gaze data valid."),
		*PresenceText, Head.Position.X, Head.Position.Y, Head.Position.Z), bGuideVisible);
	Widget->SetGazeCursor(Gaze.DisplayPoint, FMath::Clamp(DwellSeconds / DwellDurationSeconds, 0.0f, 1.0f), Gaze.bValid);
}

void ATobiiScreenBasedDemoActor::AppendEvent(const FString& Event)
{
	Events.Add(FString::Printf(TEXT("[%s] %s"), *FDateTime::Now().ToString(TEXT("%H:%M:%S")), *Event));
	while (Events.Num() > 9) Events.RemoveAt(0);
	if (Recorder) Recorder->RecordEvent(Event);
	UE_LOG(LogTemp, Log, TEXT("Tobii demo: %s"), *Event);
}
