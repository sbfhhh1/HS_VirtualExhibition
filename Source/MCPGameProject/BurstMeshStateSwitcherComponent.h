#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BurstMeshStateSwitcherComponent.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;
class AActor;
class APlayerController;
class UAudioComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USoundBase;
class UStaticMeshComponent;
struct FLeapHandData;
struct FLeapFrameData;

USTRUCT(BlueprintType)
struct FBurstInfoPanelClassPair
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Info Panel")
	TSubclassOf<AActor> LeftPanelClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Info Panel")
	TSubclassOf<AActor> RightPanelClass;
};

USTRUCT()
struct FBurstInfoPanelActorPair
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<AActor> LeftPanelActor;

	UPROPERTY(Transient)
	TObjectPtr<AActor> RightPanelActor;
};

USTRUCT()
struct FBurstInfoPanelAnimation
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<AActor> Actor;

	FVector StartLocation = FVector::ZeroVector;
	FVector TargetLocation = FVector::ZeroVector;
	FQuat StartRotation = FQuat::Identity;
	FQuat TargetRotation = FQuat::Identity;
	float Elapsed = 0.0f;
	float Duration = 0.0f;
	bool bHideWhenDone = false;
};

UCLASS(ClassGroup=(TransformationVFX), meta=(BlueprintSpawnableComponent))
class MCPGAMEPROJECT_API UBurstMeshStateSwitcherComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBurstMeshStateSwitcherComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Materials")
	TArray<TObjectPtr<UMaterialInterface>> StateParticleMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Timing", meta=(ClampMin="0.1"))
	float FrontHalfDuration = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Timing", meta=(ClampMin="0.1"))
	float ModelFadeInDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Timing", meta=(ClampMin="0.0"))
	float SourceHideDelay = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Timing", meta=(ClampMin="0.0"))
	float ParticleModelOverlapDuration = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Timing", meta=(ClampMin="0.0"))
	float SourceFadeOutDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Transform", meta=(ClampMin="0.01"))
	float ParticleEffectScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Transform", meta=(ClampMin="0.1"))
	float InhalationSpreadScale = 1.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Transform")
	FVector ParticleNonUniformScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Transform")
	FVector ParticleLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Transform")
	FRotator ParticleRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Motion")
	FVector AttractorPosition = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Materials")
	bool bUseStateParticleMaterials = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Materials")
	TObjectPtr<UMaterialInterface> ParticleMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|System")
	TObjectPtr<UNiagaraSystem> ParticleSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|System")
	TObjectPtr<UNiagaraSystem> MorphParticleSystem;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Morph")
	bool bOverrideMorphParameters = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Morph", meta=(EditCondition="bOverrideMorphParameters"))
	FLinearColor MorphEmissiveColor = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Morph", meta=(EditCondition="bOverrideMorphParameters"))
	float MorphNoiseForce = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Morph", meta=(EditCondition="bOverrideMorphParameters"))
	float MorphSpawnRate = 30000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Morph", meta=(EditCondition="bOverrideMorphParameters"))
	float MorphSpringForce = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|System")
	int32 ParticleCurrentMode = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Motion", meta=(ClampMin="0.0", ClampMax="1.0"))
	float InitialTransformationAlpha = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Motion", meta=(ClampMin="0.0", ClampMax="1.0"))
	float FirstHalfEndTransformationAlpha = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Motion", meta=(ClampMin="0.1"))
	float TransformationAlphaEaseExponent = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Fade")
	bool bUseNaturalParticleFadeOut = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Fade", meta=(ClampMin="0.0"))
	float ParticleFadeOutDuration = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Fade", meta=(ClampMin="0.1"))
	float ParticleFadeOutEaseExponent = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Particles|Fade")
	FName ParticleFadeOpacityParameter = TEXT("Fade Opacity");

	// ---------------- Leap Motion 鎸ユ墜鎵嬪娍 ----------------
	// 鍙虫墜鍚戝乏鎽嗗姩 鈫?妯″瀷 ID 鍙嶅悜鏇存柊锛涘乏鎵嬪悜鍙虫尌鍔?鈫?妯″瀷 ID 姝ｅ悜鏇存柊銆?
	// 閫氳繃閫熷害闃堝€?+ 姘村钩鍒嗛噺涓诲 + 姣忔墜鍐峰嵈 + 杩囨浮鏈熷睆钄藉洓閲嶉槻鎶栵紝閬垮厤鏁版嵁鎶栧姩瀵艰嚧妯″瀷闂儊鍒囨崲銆?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Leap Gesture")
	bool bEnableLeapSwipe = true;

	// 瑙﹀彂鎸ユ墜鎵€闇€鐨勬渶灏忔帉蹇冩按骞抽€熷害锛圲E 绌洪棿 cm/s锛夈€備綆浜庢鍊肩殑寰皬绉诲姩瑙嗕负鎶栧姩骞跺拷鐣ャ€?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Leap Gesture", meta=(ClampMin="1.0"))
	float SwipeVelocityThreshold = 80.0f;

	// 涓€娆℃尌鎵嬭Е鍙戝悗锛岃鎵嬬殑鍐峰嵈鏃堕棿锛堢锛夈€傞槻姝㈠崟娆℃尌鍔ㄥ湪澶氬抚鍐呴噸澶嶈Е鍙戯紝鏄槻闂儊鐨勫叧閿€?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Leap Gesture", meta=(ClampMin="0.0"))
	float SwipeCooldownSeconds = 0.8f;

	// 鎵嬮儴璺熻釜缃俊搴︿綆浜庢鍊兼椂蹇界暐锛岄伩鍏嶈窡韪笉绋冲畾鏃惰瑙﹀彂銆?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Leap Gesture", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinHandConfidence = 0.2f;

	// 鑻ヨ澶囧畨瑁呮湞鍚戝鑷村乏鍙虫柟鍚戯紙Y 杞达級鐩稿弽锛屽嬀閫夋椤圭炕杞垽瀹氭柟鍚戙€?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Leap Gesture")
	bool bInvertSwipeAxis = false;

	// 榛樿鍦ㄥ垏鎹㈣繃娓¤繘琛屼腑蹇界暐鏂扮殑鎸ユ墜锛岀‘淇濇ā鍨嬪畬鏁存垚褰㈠悗鍐嶅搷搴斾笅涓€娆★紝杩涗竴姝ラ槻闂儊銆?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Leap Gesture")
	bool bAllowSwipeDuringTransition = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Leap Gesture", meta=(ClampMin="0.0"))
	float LeapFrameStaleRebindSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Leap Gesture")
	bool bDebugLeapSwipe = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Leap Gesture")
	bool bEnableLeapGrabCycle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Leap Gesture", meta=(ClampMin="0.0", ClampMax="1.0"))
	float GrabTriggerThreshold = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Leap Gesture", meta=(ClampMin="0.0", ClampMax="1.0"))
	float GrabReleaseThreshold = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Leap Gesture", meta=(ClampMin="0.0"))
	float GrabCooldownSeconds = 0.9f;

	// ---------------- 妯″瀷鑷浆 ----------------
	// 妯″瀷瀹屾暣鏄剧ず鏃剁粫绔栫洿杞达紙涓栫晫 Z 杞达級浠庡綋鍓嶈搴︾紦鎱㈣嚜杞紱闅愯棌鎴栧垏鎹㈣繃娓′腑涓嶈浆銆?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Rotation")
	bool bEnableModelRotation = true;

	// 鑷浆閫熷害锛堝害/绉掞級銆傛鍊奸€嗘椂閽堛€佽礋鍊奸『鏃堕拡锛? 绛夊悓鍏抽棴銆?
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Rotation")
	float RotationSpeedDegreesPerSecond = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Info Panels")
	TArray<FBurstInfoPanelClassPair> InfoPanelClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Info Panels", meta=(ClampMin="0.0"))
	float PanelRevealDelaySeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Info Panels", meta=(ClampMin="0.0"))
	float PanelHiddenOutsideMargin = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Info Panels", meta=(ClampMin="1.0"))
	float PanelSlideInSpeed = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Info Panels", meta=(ClampMin="1.0"))
	float PanelSlideOutSpeed = 620.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Info Panels", meta=(ClampMin="0.1"))
	float PanelEaseExponent = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Info Panels")
	FName FrameActorName = TEXT("frame");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Info Panels")
	bool bAnimateInitialPanelsIn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Info Panels")
	FVector LeftPanelFallbackTargetLocation = FVector(235.233857f, -146.0f, 170.116791f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Info Panels")
	FVector RightPanelFallbackTargetLocation = FVector(235.233857f, 138.190721f, 170.116792f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Info Panels")
	FRotator PanelFallbackTargetRotation = FRotator(0.0f, 179.999999f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Audio")
	TObjectPtr<USoundBase> PanelSlideSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Audio", meta=(ClampMin="0.0"))
	float PanelSlideSoundVolume = 0.50f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Audio", meta=(ClampMin="0.1"))
	float PanelSlideSoundPitch = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Audio", meta=(ClampMin="0.0"))
	float PanelSlideSoundMinInterval = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Audio")
	TObjectPtr<USoundBase> TransformationSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Audio", meta=(ClampMin="0.0"))
	float TransformationSoundVolume = 0.86f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Audio", meta=(ClampMin="0.1"))
	float TransformationSoundPitch = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Audio", meta=(ClampMin="0.0"))
	float TransformationSoundPostRevealHoldSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Audio", meta=(ClampMin="0.0"))
	float TransformationSoundFadeOutSeconds = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Audio")
	bool bStopPreviousTransformationSound = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Audio")
	TObjectPtr<USoundBase> BackgroundMusic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Audio")
	bool bAutoStartBackgroundMusic = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Audio")
	bool bLoopBackgroundMusic = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Audio", meta=(ClampMin="0.0"))
	float BackgroundMusicVolume = 0.24f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Burst Switcher|Audio", meta=(ClampMin="0.1"))
	float BackgroundMusicPitch = 1.0f;

	UFUNCTION(BlueprintCallable, Category="Burst Switcher|Models")
	void SwitchToState(int32 StateIndex);

	// 鎸夋柟鍚戝惊鐜垏鎹㈡ā鍨嬶紙+1 姝ｅ悜 / -1 鍙嶅悜锛夛紝甯︾幆缁曘€?
	UFUNCTION(BlueprintCallable, Category="Burst Switcher|Models")
	void CycleModel(int32 Direction);

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void CacheOwnerComponents();
	void ApplyInitialStateMaterials();
	void CaptureInitialStateMaterials();
	void RestoreStateInitialMaterials(int32 StateIndex);
	UMaterialInterface* GetStateInitialMaterial(int32 StateIndex, int32 MaterialIndex = 0) const;
	void DisableLegacyTriggerAndUI();
	void SetRuntimeStateVisibility(int32 VisibleStateIndex);
	void ApplyParticleSettings();
	void ApplyMorphParameters();
	void SetTransformationAlpha(float Alpha);
	UMaterialInterface* ResolveParticleMaterial() const;
	void HideSourceMesh();
	void BeginSourceFadeOut();
	void BeginModelReveal();
	void FinishParticlesAndRevealTarget();
	void BeginTargetModelFadeIn();
	void BeginParticleFadeOut();
	void StopAndHideParticles();
	void CompleteTransition();
	void SwitchToStateOne();
	void SwitchToStateTwo();
	void SwitchToStateThree();
	void PollLeapGrabGestures(float DeltaTime);
	void PollLeapSwipeGestures(float DeltaTime);
	void UpdateModelRotation(float DeltaTime);
	void InitializeInfoPanels();
	void BeginPanelSwitch(int32 TargetStateIndex);
	void SchedulePanelReveal(int32 StateIndex);
	void RevealCurrentStatePanels();
	void UpdateInfoPanelAnimations(float DeltaTime);
	void StartPanelAnimation(AActor* Actor, const FVector& TargetLocation, const FQuat& TargetRotation, float Speed, bool bHideWhenDone);
	void MovePanelToHiddenLocation(int32 StateIndex, bool bLeftPanel, bool bKeepVisible);
	void HideInactiveInfoPanels();
	void ResolveInfoPanelTargets();
	void ResolveInfoPanelHiddenLocations();
	AActor* FindExistingPanelActor(TSubclassOf<AActor> PanelClass) const;
	AActor* SpawnPanelActor(TSubclassOf<AActor> PanelClass, const FTransform& InitialTransform) const;
	AActor* GetInfoPanelActor(int32 StateIndex, bool bLeftPanel) const;
	FTransform GetInfoPanelTargetTransform(bool bLeftPanel) const;
	FVector GetInfoPanelHiddenLocation(bool bLeftPanel) const;
	void SetInfoPanelVisible(AActor* Actor, bool bVisible) const;
	bool HasActivePanelAnimation(AActor* Actor) const;
	void PlayPanelSlideSound();
	void PlayTransformationSound();
	void FadeOutTransformationSound();
	void StartBackgroundMusic();
	void StopBackgroundMusic();
	void UpdateBackgroundMusicLoop();
	bool IsTransitionInProgress() const;
	void BindLeapFrameDelegate(bool bForceRebind);
	void RemoveLeapFrameDelegate();
	void RefreshLeapFrameDelegate(float DeltaTime);
	// 鍒ゅ畾鎺屽績閫熷害鏄惁鏋勬垚涓€娆℃湁鏁堢殑姘村钩鎸ユ墜锛堥槇鍊?+ 姘村钩鍒嗛噺涓诲锛夈€?
	bool IsLateralSwipe(const FVector& Velocity) const;
	float EstimateGripStrength(const FLeapHandData& Hand) const;
	// Leap 甯у洖璋冿紙娓告垙绾跨▼锛夛細浠呯紦瀛樺乏鍙虫墜鎺屽績閫熷害/缃俊搴︼紝瀹為檯鍒ゅ畾鏀惧湪 Tick 涓寜 DeltaTime 璧板喎鍗淬€?
	void OnLeapTrackingData(const FLeapFrameData& Frame);

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SourceMeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> TargetMeshComponent;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> StateMeshComponents;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> TransformationNiagara;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> PlayerController;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ActiveFadeMaterials;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> ActiveSourceFadeMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ActiveParticleMaterial;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> InitialStateMaterials;

	UPROPERTY(Transient)
	TArray<int32> InitialStateMaterialOffsets;

	UPROPERTY(Transient)
	TArray<int32> InitialStateMaterialCounts;

	FVector NiagaraBaseLocation = FVector::ZeroVector;
	FVector NiagaraBaseScale = FVector::OneVector;
	FRotator NiagaraBaseRotation = FRotator::ZeroRotator;
	int32 CurrentStateIndex = 2;
	int32 PendingStateIndex = INDEX_NONE;
	float TransitionElapsed = 0.0f;
	float ActiveTransitionDuration = 1.0f;
	float FadeInElapsed = 0.0f;
	float SourceFadeOutElapsed = 0.0f;
	bool bSourceFadeOutActive = false;
	float ParticleFadeOutElapsed = 0.0f;
	bool bTransitionActive = false;
	bool bFadeInActive = false;
	bool bFadeRevealPending = false;
	bool bParticleFadeOutActive = false;
	bool bInitialStateMaterialsApplied = false;
	FTimerHandle HideSourceTimer;
	FTimerHandle BeginRevealTimer;
	FTimerHandle ParticleFadeOutTimer;
	FTimerHandle PanelRevealTimer;
	FTimerHandle BackgroundMusicLoopTimer;
	FTimerHandle TransformationSoundFadeOutTimer;

	UPROPERTY(Transient)
	TArray<FBurstInfoPanelActorPair> InfoPanelActors;

	UPROPERTY(Transient)
	TArray<FBurstInfoPanelAnimation> InfoPanelAnimations;

	FTransform LeftPanelTargetTransform = FTransform::Identity;
	FTransform RightPanelTargetTransform = FTransform::Identity;
	FVector LeftPanelHiddenLocation = FVector::ZeroVector;
	FVector RightPanelHiddenLocation = FVector::ZeroVector;
	bool bInfoPanelsInitialized = false;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BackgroundMusicComponent;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> TransformationSoundComponent;

	float LastPanelSlideSoundTime = -1000.0f;

	// 姣忔墜鎸ユ墜鍐峰嵈鍓╀綑鏃堕棿锛堢锛夛紝宸﹀彸鎵嬬嫭绔嬭鏃躲€?
	float LeftHandSwipeCooldown = 0.0f;
	float RightHandSwipeCooldown = 0.0f;
	float LeftHandGrabCooldown = 0.0f;
	float RightHandGrabCooldown = 0.0f;

	// Leap 甯у洖璋冪紦瀛樼殑鏈€鏂版墜閮ㄦ暟鎹紙娓告垙绾跨▼鍐欏叆锛孴ick 璇诲彇锛夈€?
	FVector LeftPalmVelocity = FVector::ZeroVector;
	FVector RightPalmVelocity = FVector::ZeroVector;
	float LeftHandConfidence = 0.0f;
	float RightHandConfidence = 0.0f;
	float LeftGripStrength = 0.0f;
	float RightGripStrength = 0.0f;
	bool bLeftHandPresent = false;
	bool bRightHandPresent = false;
	bool bLeftGrabLatched = false;
	bool bRightGrabLatched = false;
	float TimeSinceLeapFrame = 1000.0f;
	bool bWasLeapFrameStale = true;
	FDelegateHandle LeapFrameDelegateHandle;
};
