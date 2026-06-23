#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TobiiDataRecorderComponent.generated.h"

class FArchive;

UCLASS(ClassGroup = "Tobii", BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class TOBIIEYETRACKER5_API UTobiiDataRecorderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTobiiDataRecorderComponent();
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Tobii|Recording")
	bool StartRecording();

	UFUNCTION(BlueprintCallable, Category = "Tobii|Recording")
	void StopRecording();

	UFUNCTION(BlueprintCallable, Category = "Tobii|Recording")
	void RecordEvent(const FString& EventName, const FString& Value = TEXT(""));

	UFUNCTION(BlueprintPure, Category = "Tobii|Recording")
	bool IsRecording() const { return bRecording; }

	UFUNCTION(BlueprintPure, Category = "Tobii|Recording")
	FString GetOutputPath() const { return OutputPath; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tobii|Recording")
	FString OutputFolder = TEXT("TobiiData");

	UPROPERTY(BlueprintReadWrite, Category = "Tobii|Recording")
	FString CurrentTarget;

	UPROPERTY(BlueprintReadWrite, Category = "Tobii|Recording")
	float DwellProgress = 0.0f;

private:
	TUniquePtr<FArchive> Writer;
	FString OutputPath;
	bool bRecording = false;

	void WriteLine(const FString& Line);
	static FString EscapeXml(const FString& Value);
};
