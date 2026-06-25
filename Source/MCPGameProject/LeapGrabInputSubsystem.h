#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "UltraleapTrackingData.h"
#include "LeapGrabInputSubsystem.generated.h"

struct FLeapFrameData;

UCLASS()
class MCPGAMEPROJECT_API ULeapGrabInputSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override;

private:
	struct FHandInputState
	{
		bool bGrabPressed = false;
		bool bPinchPressed = false;
		float GrabStrength = 0.0f;
		float PinchStrength = 0.0f;
		float TimeSinceSeen = 1000.0f;
	};

	void OnLeapTrackingData(const FLeapFrameData& Frame);
	void UpdateHand(float DeltaTime, FHandInputState& State, const FKey& GrabKey, const FKey& PinchKey);
	void SendKey(const FKey& Key, EInputEvent Event);

	FDelegateHandle LeapFrameDelegateHandle;
	FHandInputState LeftHand;
	FHandInputState RightHand;

	static constexpr float StartGrabThreshold = 0.8f;
	static constexpr float EndGrabThreshold = 0.5f;
	static constexpr float StartPinchThreshold = 0.8f;
	static constexpr float EndPinchThreshold = 0.5f;
	static constexpr float HandStaleTimeout = 0.25f;
};
