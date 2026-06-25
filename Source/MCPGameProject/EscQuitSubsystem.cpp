#include "EscQuitSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"

void UEscQuitSubsystem::Tick(float DeltaTime)
{
	if (bQuitRequested)
	{
		return;
	}

	const UGameInstance* GameInstance = GetGameInstance();
	UWorld* World = GameInstance ? GameInstance->GetWorld() : nullptr;
	if (!World || World->WorldType != EWorldType::Game)
	{
		return;
	}

	APlayerController* PlayerController = GameInstance->GetFirstLocalPlayerController();
	if (PlayerController && PlayerController->WasInputKeyJustPressed(EKeys::Escape))
	{
		bQuitRequested = true;
		UKismetSystemLibrary::QuitGame(World, PlayerController, EQuitPreference::Quit, false);
	}
}

TStatId UEscQuitSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UEscQuitSubsystem, STATGROUP_Tickables);
}

bool UEscQuitSubsystem::IsTickable() const
{
	return !IsTemplate() && GetGameInstance() != nullptr;
}
