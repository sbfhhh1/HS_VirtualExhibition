#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "BlenderGNRuntimePanelWidget.generated.h"

class UBlenderGNCageCornerMarkersComponent;
class UBlenderGNGradientBoxMotionComponent;
class UBlenderGNIslandBreathMotionComponent;
class UBlenderGNParasiteMotionComponent;
class UBlenderGNWallNoiseMotionComponent;
class UBlenderGeometryNodesComponent;
class UCheckBox;
class USlider;
class UTextBlock;

UCLASS(BlueprintType, Blueprintable)
class OPENCLAWBLENDERGEOMETRYNODES_API UBlenderGNRuntimePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	TObjectPtr<UBlenderGeometryNodesComponent> Target;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	TObjectPtr<UBlenderGNIslandBreathMotionComponent> IslandMotion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	TObjectPtr<UBlenderGNParasiteMotionComponent> ParasiteMotion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	TObjectPtr<UBlenderGNWallNoiseMotionComponent> WallNoiseMotion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	TObjectPtr<UBlenderGNGradientBoxMotionComponent> GradientBoxMotion;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Binding")
	TObjectPtr<UBlenderGNCageCornerMarkersComponent> CageCornerMarkers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime")
	bool bLiveMotion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime")
	bool bLiveBlenderRebuild = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime")
	bool bDriveBlenderAnimation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (ClampMin = "0.02", ClampMax = "1.25"))
	float RebuildDebounceSeconds = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Runtime", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float BlenderFrameRate = 60.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Status")
	bool bRebuildQueued = false;

	UPROPERTY(BlueprintReadOnly, Category = "Status")
	FString RebuildStatusText = TEXT("Blender Rebuild: Manual");

	UPROPERTY(BlueprintReadOnly, Category = "Status")
	FString RuntimeStatusText = TEXT("Idle");

	UFUNCTION(BlueprintCallable, Category = "Runtime")
	void ResolveReferences(AActor* SearchActor);

	UFUNCTION(BlueprintCallable, Category = "Runtime")
	void ApplyRuntimeSettings();

	UFUNCTION(BlueprintCallable, Category = "Runtime")
	void SetLiveMotion(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Runtime")
	void SetLiveBlenderRebuild(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Runtime")
	void QueueBlenderRebuild(bool bForce = true, float DelayOverride = -1.0f);

	UFUNCTION(BlueprintCallable, Category = "Runtime")
	void RefreshNow();

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	void SetFloatInput(const FString& SocketName, float Value, bool bPreviewParasite = true);

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	void SetIntInput(const FString& SocketName, int32 Value);

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	void SetBoolInput(const FString& SocketName, bool bValue);

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	void SetVectorInput(const FString& SocketName, FVector Value);

	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetWallNoiseStrength(float Value);

	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetWallNoiseSpeed(float Value);

	UFUNCTION(BlueprintCallable, Category = "Motion")
	void SetWallNoiseScale(float Value);

	UFUNCTION(BlueprintCallable, Category = "GradientBox")
	void SetGradientBoxAmplitude(float Value);

	UFUNCTION(BlueprintCallable, Category = "GradientBox")
	void SetGradientBoxSpeed(float Value);

	UFUNCTION(BlueprintCallable, Category = "GradientBox")
	void SetGradientBoxSmoothness(float Value);

	UFUNCTION(BlueprintCallable, Category = "GradientBox")
	void SetGradientBoxMaxFPS(float Value);

	UFUNCTION(BlueprintCallable, Category = "GradientBox")
	void SetGradientBoxReversePink(bool bReverse);

	UFUNCTION(BlueprintCallable, Category = "GradientBox")
	void SetGradientBoxUseFastMesh(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "GradientBox")
	void SetGradientBoxRecalcNormals(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "Status")
	void UpdateStatusText();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<USlider> AmplitudeSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> SpeedSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> SmoothnessSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> MaxFPSSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> NumSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> SizeSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> GapSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> FrameSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> GNSpeedSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> PowerSlider;

	UPROPERTY(Transient)
	TObjectPtr<USlider> ShiftAngleSlider;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> LiveMotionCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> LiveBlenderRebuildCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> DriveBlenderAnimationCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> ReversePinkCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> FastMeshCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<UCheckBox> RecalculateNormalsCheckBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> AmplitudeValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpeedValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SmoothnessValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MaxFPSValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NumValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SizeValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GapValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> FrameValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GNSpeedValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PowerValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ShiftAngleValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusTextBlock;

	bool bForceQueuedRebuild = false;
	double RebuildAtSeconds = 0.0;
	bool bSynchronizingControls = false;

	void PumpQueuedRebuild();
	void BuildDefaultGradientBoxPanel();
	void SynchronizeGradientBoxControls();
	void UpdateGradientBoxValueText();
	void UpdateGeometryNodeValueText();

	UFUNCTION()
	void HandleAmplitudeChanged(float Value);

	UFUNCTION()
	void HandleSpeedChanged(float Value);

	UFUNCTION()
	void HandleSmoothnessChanged(float Value);

	UFUNCTION()
	void HandleMaxFPSChanged(float Value);

	UFUNCTION()
	void HandleNumChanged(float Value);

	UFUNCTION()
	void HandleSizeChanged(float Value);

	UFUNCTION()
	void HandleGapChanged(float Value);

	UFUNCTION()
	void HandleFrameChanged(float Value);

	UFUNCTION()
	void HandleGNSpeedChanged(float Value);

	UFUNCTION()
	void HandlePowerChanged(float Value);

	UFUNCTION()
	void HandleShiftAngleChanged(float Value);

	UFUNCTION()
	void HandleLiveMotionChanged(bool bIsChecked);

	UFUNCTION()
	void HandleLiveBlenderRebuildChanged(bool bIsChecked);

	UFUNCTION()
	void HandleDriveBlenderAnimationChanged(bool bIsChecked);

	UFUNCTION()
	void HandleReversePinkChanged(bool bIsChecked);

	UFUNCTION()
	void HandleUseFastMeshChanged(bool bIsChecked);

	UFUNCTION()
	void HandleRecalculateNormalsChanged(bool bIsChecked);
};
