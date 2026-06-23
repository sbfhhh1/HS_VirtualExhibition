#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FistDisplayToggleComponent.generated.h"

struct FLeapFrameData;

/**
 * 握拳手势切换显示：每次握拳（Leap GrabStrength 越过阈值）在若干"显示分组"之间轮流，
 * 显示当前组的 Actor、隐藏其它组。默认在 BG 与 Chinese_Seal_Pool_Animator 之间轮流。
 */
UCLASS(ClassGroup=(TransformationVFX), meta=(BlueprintSpawnableComponent))
class MCPGAMEPROJECT_API UFistDisplayToggleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFistDisplayToggleComponent();

	// 轮流显示的分组标识。匹配规则（任一即算该组）：Actor 拥有同名 Tag；或（编辑器/PIE）Actor 标签等于它；或 Actor 名包含它。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fist Toggle")
	TArray<FName> DisplayGroupTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fist Toggle")
	bool bEnableFistToggle = true;

	// 握拳判定阈值（GrabStrength，0~1，1=完全握拳）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fist Toggle", meta=(ClampMin="0.1", ClampMax="1.0"))
	float FistGrabThreshold = 0.9f;

	// 松开阈值：握拳触发后必须降到该值以下才能再次触发（防止一次握拳连切）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fist Toggle", meta=(ClampMin="0.0", ClampMax="1.0"))
	float ReleaseThreshold = 0.5f;

	// 两次触发最小间隔（秒）。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fist Toggle", meta=(ClampMin="0.0"))
	float TriggerCooldownSeconds = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fist Toggle", meta=(ClampMin="0.0"))
	float GesturePollInterval = 0.033f;

	// 手部跟踪置信度低于此值时忽略。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fist Toggle", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinHandConfidence = 0.2f;

	// 起始显示的分组索引。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fist Toggle")
	int32 InitialGroupIndex = 0;

	// 显示指定分组（隐藏其它受管理分组），带环绕。
	UFUNCTION(BlueprintCallable, Category="Fist Toggle")
	void ShowGroup(int32 GroupIndex);

	// 切到下一组（轮流）。
	UFUNCTION(BlueprintCallable, Category="Fist Toggle")
	void AdvanceGroup();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(
		float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void OnLeapTrackingData(const FLeapFrameData& Frame);
	void ResolveGroups();
	bool ActorMatchesGroup(const AActor* Actor, FName GroupTag) const;

	// 受管理的 Actor 及其所属分组索引（并行数组）。
	UPROPERTY(Transient)
	TArray<TObjectPtr<AActor>> ManagedActors;
	TArray<int32> ManagedActorGroups;

	int32 CurrentGroupIndex = 0;

	// Leap 帧回调缓存（游戏线程写、Tick 读）。
	float MaxGrabStrength = 0.0f;
	bool bHandTracked = false;
	float TimeSinceLeapFrame = 1000.0f;

	bool bFistLatched = false;   // 已触发、等待松开再触发
	float TriggerCooldown = 0.0f;

	FDelegateHandle LeapFrameDelegateHandle;
};
