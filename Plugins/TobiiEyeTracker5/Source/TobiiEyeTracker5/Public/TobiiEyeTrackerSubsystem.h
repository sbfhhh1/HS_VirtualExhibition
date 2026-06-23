#pragma once

#include "CoreMinimal.h"
#include "Async/Future.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "TobiiEyeTrackerTypes.h"
#include "TobiiEyeTrackerSubsystem.generated.h"

UCLASS(BlueprintType)
class TOBIIEYETRACKER5_API UTobiiEyeTrackerSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickable() const override { return !IsTemplate(); }

	UFUNCTION(BlueprintCallable, Category = "Tobii")
	bool StartTracking();

	UFUNCTION(BlueprintCallable, Category = "Tobii")
	void StopTracking();

	UFUNCTION(BlueprintPure, Category = "Tobii")
	FTobiiGazeSample GetLatestGaze() const { return LatestGaze; }

	UFUNCTION(BlueprintPure, Category = "Tobii")
	FTobiiHeadPoseSample GetLatestHeadPose() const { return LatestHeadPose; }

	UFUNCTION(BlueprintPure, Category = "Tobii")
	ETobiiUserPresence GetUserPresence() const { return Presence; }

	UFUNCTION(BlueprintPure, Category = "Tobii")
	bool IsConnected() const { return bConnected; }

	UFUNCTION(BlueprintPure, Category = "Tobii")
	bool IsAvailable() const { return bAvailable; }

	UFUNCTION(BlueprintPure, Category = "Tobii")
	int32 GetGazeSampleCount() const { return GazeSampleCount; }

	UFUNCTION(BlueprintPure, Category = "Tobii")
	float GetLastGazeAgeSeconds() const { return LastGazeAgeSeconds; }

	UFUNCTION(BlueprintCallable, Category = "Tobii")
	bool LaunchSystemCalibration();

	UFUNCTION(BlueprintCallable, Category = "Tobii|Debug")
	void SetSimulationEnabled(bool bEnabled) { bSimulationEnabled = bEnabled; }

	UFUNCTION(BlueprintPure, Category = "Tobii|Debug")
	bool IsSimulationEnabled() const { return bSimulationEnabled; }

	UPROPERTY(BlueprintAssignable, Category = "Tobii")
	FTobiiConnectionChanged OnConnectionChanged;

	UPROPERTY(BlueprintAssignable, Category = "Tobii")
	FTobiiGazeUpdated OnGazeUpdated;

	UPROPERTY(BlueprintAssignable, Category = "Tobii")
	FTobiiPresenceChanged OnPresenceChanged;

	UPROPERTY(BlueprintAssignable, Category = "Tobii")
	FTobiiError OnError;

private:
	void* DllHandle = nullptr;
	bool bStarted = false;
	bool bStartAttempted = false;
	bool bStartInProgress = false;
	bool bStopRequested = false;
	bool bAvailable = false;
	bool bConnected = false;
	bool bSimulationEnabled = false;
	bool bHasSmoothedPoint = false;
	bool bLoggedFirstGaze = false;
	int32 GazeSampleCount = 0;
	float LastGazeAgeSeconds = -1.0f;
	double LastDiagnosticLogSeconds = 0.0;
	void* LastBoundWindow = nullptr;
	TFuture<bool> StartFuture;
	ETobiiUserPresence Presence = ETobiiUserPresence::Unknown;
	FTobiiGazeSample LatestGaze;
	FTobiiHeadPoseSample LatestHeadPose;

	bool LoadIntegration();
	void UnloadIntegration();
	void CompleteStartTracking();
	void BindGameWindow();
	void UpdateDevice(float DeltaTime);
	void UpdateSimulation(float DeltaTime);
	void ReportError(const FString& Message);
};
