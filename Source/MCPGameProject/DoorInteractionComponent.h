#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "DoorInteractionComponent.generated.h"

class USoundBase;
class UWidgetComponent;

/**
 * 对开门交互：按下切换键(默认 5)开/关门，门板 Door_L/Door_R 滑动，
 * 同步淡入/淡出主 UI 的 WidgetComponent，并播放开关门音效。默认关门态。
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class MCPGAMEPROJECT_API UDoorInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDoorInteractionComponent();

	/** 左门板 Actor（Door_L）。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	TObjectPtr<AActor> LeftDoor;

	/** 右门板 Actor（Door_R）。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	TObjectPtr<AActor> RightDoor;

	/** 开门时左门板相对关门位置的滑动偏移（门 Actor 本地空间）。拖动微调到 frame 左边界。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	FVector LeftDoorOpenOffset = FVector(0.f, -100.f, 0.f);

	/** 开门时右门板相对关门位置的滑动偏移（门 Actor 本地空间）。拖动微调到 frame 右边界。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	FVector RightDoorOpenOffset = FVector(0.f, 100.f, 0.f);

	/** 主 UI Actor（含 WidgetComponent）。开门时淡出、关门时淡入。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	TObjectPtr<AActor> MainUIActor;

	/** 开/关门动画时长（秒）。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door", meta=(ClampMin="0.05"))
	float OpenCloseDuration = 2.0f;

	/** 切换开关门的按键。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	FKey ToggleKey = EKeys::Five;

	/** 开门音效。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	TObjectPtr<USoundBase> OpenSound;

	/** 关门音效。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door")
	TObjectPtr<USoundBase> CloseSound;

	/**
	 * 性能优化：门关闭时冻结这些 Actor 的逐帧动画（CustomTimeDilation=0），保持可见。
	 * 用于玉石「C」(停止旋转)、印章池(停止动画)等——不隐藏，只停动。开门恢复。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door|Performance")
	TArray<TObjectPtr<AActor>> PauseActorsWhenClosed;

	/**
	 * 性能优化：门关闭时把这些 Actor 网格材质的速度参数置 0，暂停纯材质(shader)动效；开门还原。
	 * 用于 BG 这类靠材质参数(Speed/Rotation Speed × Time)驱动的动画。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door|Performance")
	TArray<TObjectPtr<AActor>> PauseShaderActorsWhenClosed;

	/** 上面 shader 暂停要置 0 的标量参数名（默认 Speed / Rotation Speed）。*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Door|Performance")
	TArray<FName> ShaderSpeedParameterNames = { FName("Speed"), FName("Rotation Speed") };

	/** 切换开/关门（也可被蓝图/其它逻辑调用）。*/
	UFUNCTION(BlueprintCallable, Category="Door")
	void ToggleDoor();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

private:
	void CacheClosedAndOpenLocations();
	void ApplyProgress();
	// 暂停/恢复被门挡住的动态内容（玉石/印章池冻结时间，BG 置零 shader 速度）
	void SetGatedContentActive(bool bActive);
	// BeginPlay 时为 BG 等创建动态材质实例并缓存原始速度参数
	void SetupShaderControls();

	bool bContentActive = true;

	// BG 等的动态材质实例及其原始速度参数值（用于暂停/还原）
	TArray<TWeakObjectPtr<class UMaterialInstanceDynamic>> ShaderDMIs;
	TArray<TArray<float>> ShaderOriginalValues;

	// true=目标为开门
	bool bTargetOpen = false;
	// 0=完全关，1=完全开
	float Progress = 0.f;
	bool bInitialized = false;

	FVector LeftClosedLocation = FVector::ZeroVector;
	FVector LeftOpenLocation = FVector::ZeroVector;
	FVector RightClosedLocation = FVector::ZeroVector;
	FVector RightOpenLocation = FVector::ZeroVector;

	TWeakObjectPtr<UWidgetComponent> CachedUIWidget;
};
