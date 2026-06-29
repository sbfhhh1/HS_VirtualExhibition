#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "DoubleAccordionFoldingDoorActor.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UProceduralMeshComponent;
class USoundBase;
class UAudioComponent;

UCLASS(Blueprintable)
class MCPGAMEPROJECT_API ADoubleAccordionFoldingDoorActor : public AActor
{
	GENERATED_BODY()

public:
	ADoubleAccordionFoldingDoorActor();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Folding Door")
	TObjectPtr<UProceduralMeshComponent> ProceduralMesh;

	// 中间两根木条：独立子组件，靠 Transform 平移跟随开合（刚体移动有正确运动矢量，不被 TSR 糊）。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Folding Door")
	TObjectPtr<UProceduralMeshComponent> InnerBarLeft;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Folding Door")
	TObjectPtr<UProceduralMeshComponent> InnerBarRight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Folding Door")
	TObjectPtr<UAudioComponent> DoorAudio;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OpenAmount = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door", meta = (ClampMin = "1"))
	int32 FoldCount = 16;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door", meta = (ClampMin = "100.0"))
	float TotalWidth = 342.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door", meta = (ClampMin = "10.0"))
	float StripHeight = 187.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door", meta = (ClampMin = "0.02", ClampMax = "0.8"))
	float MinimumOpenAmount = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door", meta = (ClampMin = "0.0", ClampMax = "4.0"))
	float DepthScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door", meta = (ClampMin = "0.0"))
	float Thickness = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door", meta = (ClampMin = "1.0"))
	float EndStripWidth = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door", meta = (ClampMin = "1.0"))
	float EndStripDepth = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door", meta = (ClampMin = "2", ClampMax = "128"))
	int32 HeightVertices = 14;

	// 纹理平铺次数：UV 沿布料表面展开（与 Blender GN 版一致）。
	// 1.0 = 一张完整图铺满整个门表面（默认）；>1 则在表面上重复平铺。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door", meta = (ClampMin = "0.01"))
	float UVScale = 1.0f;

	// 端部木条的 box 映射平铺尺寸：每个纹理重复对应的世界尺寸（UU）。越小木纹越密。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door", meta = (ClampMin = "0.01"))
	float WoodUVTileSize = 100.0f;

	// 从全折叠到全展平的动画总时长（秒）。长按方向键时以此速度匀速过渡。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door|Interaction", meta = (ClampMin = "0.05"))
	float AnimationDuration = 2.0f;

	// 展平（OpenAmount→1，关闭遮挡）时播放的音效
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door|Interaction")
	TObjectPtr<USoundBase> FlattenSound;

	// 折叠（OpenAmount→0，打开通道）时播放的音效
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door|Interaction")
	TObjectPtr<USoundBase> FoldSound;

	// 是否调整音效播放速率，使其时长与本次动画剩余时长一致（会轻微改变音调）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door|Interaction")
	bool bSyncSoundToAnimation = true;

	// 是否在 BeginPlay 时自动绑定键盘 上/下 方向键（无手势设备调试用）
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door|Interaction")
	bool bBindArrowKeys = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door|Materials")
	TObjectPtr<UMaterialInterface> DoorMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door|Materials")
	TObjectPtr<UMaterialInterface> WoodMaterial;

	// 中间两根木条的材质。留空则默认使用木材主段(外框)同款材质；可在此自定义指定。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Folding Door|Materials")
	TObjectPtr<UMaterialInterface> InnerBarMaterial;

	UFUNCTION(BlueprintCallable, Category = "Folding Door")
	void SetOpenAmount(float Value);

	UFUNCTION(BlueprintCallable, Category = "Folding Door")
	void SetDoorParameters(
		float InOpenAmount,
		int32 InFoldCount,
		float InTotalWidth,
		float InStripHeight,
		float InMinimumOpenAmount,
		float InDepthScale,
		float InThickness,
		float InEndStripWidth,
		float InEndStripDepth);

	UFUNCTION(BlueprintCallable, Category = "Folding Door")
	void RebuildDoor();

	// 持续展平（OpenAmount→1，关闭遮挡）。对应长按上箭头；松开调用 StopDoorAnimation。
	UFUNCTION(BlueprintCallable, Category = "Folding Door")
	void StartFlattening();

	// 持续折叠（OpenAmount→0，打开通道）。对应长按下箭头；松开调用 StopDoorAnimation。
	UFUNCTION(BlueprintCallable, Category = "Folding Door")
	void StartFolding();

	// 停止动画并停止音效。对应方向键抬起。
	UFUNCTION(BlueprintCallable, Category = "Folding Door")
	void StopDoorAnimation();

protected:
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

private:
	struct FMeshBuildData
	{
		TArray<FVector> Vertices;
		TArray<int32> Triangles;
		TArray<FVector> Normals;
		TArray<FVector2D> UVs;   // UV0：纹理坐标
		TArray<FVector2D> UV1;   // 折叠系数 (Cx, Cy)，供材质 WPO 使用
		TArray<FVector2D> UV2;   // 折叠法线数据 (折面坡度系数, 正反面符号 ny)，供材质解析法线使用
		TArray<FColor> Colors;
		TArray<FProcMeshTangent> Tangents;
	};

	void ClampParameters();
	float GetEffectiveOpenAmount() const;
	float GetTextileWidthPerSide() const;
	float EvaluateFoldDepth(float DistanceFromOuterHinge, float PanelWidth, int32 PanelCount, float FoldDepth) const;
	// 折叠深度系数（不含随开合变化的 sqrt(1-eo^2) 项）：material WPO 的 Y 偏移 = 本系数 * sqrt(1-eo^2)。
	float FoldDepthCoefficient(float DistanceFromOuterHinge, float PanelWidth, int32 PanelCount) const;
	// 把布料折叠系数烘焙进顶点 UV1：Cx 用于 X 压缩，Cy 用于折叠深度。
	void BakeClothFoldCoefficients(FMeshBuildData& Data, int32 StartIndex, int32 EndIndex, bool bLeftLeaf, float PanelWidth, int32 PanelCount) const;
	// 每帧仅把 EffectiveOpen 推给动态材质实例，不重建网格（折叠交给材质 WPO）。
	void ApplyOpenAmountToMaterial();

	// 在两个木条子组件上各建一个居中的盒体网格（只建一次，之后靠 Transform 移动）。
	void BuildInnerBars(const FVector& Extent, UMaterialInterface* WoodMat);
	// 按当前开合量更新两根中间木条的相对位置（X 平移）。
	void UpdateInnerBarPositions();
	void AppendDoorLeaf(FMeshBuildData& Data, bool bLeftLeaf, float EffectiveOpen, float TextileWidth, int32 PanelCount, float PanelWidth, float FoldDepth) const;
	void AppendBox(FMeshBuildData& Data, const FVector& Center, const FVector& Extent) const;
	void UpdateOrCreateMeshSection(int32 SectionIndex, const FMeshBuildData& Data);
	UMaterialInterface* ResolveSectionMaterial(int32 SectionIndex, TObjectPtr<UMaterialInterface>& ConfiguredMaterial, TObjectPtr<UMaterialInterface>& LastAppliedMaterial) const;
	static void AppendQuad(FMeshBuildData& Data, int32 A, int32 B, int32 C, int32 D);
	static void AppendQuadFacing(FMeshBuildData& Data, int32 A, int32 B, int32 C, int32 D, const FVector& DesiredNormal);
	static void AppendSplitQuad(FMeshBuildData& Data, const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector2D& UVA, const FVector2D& UVB, const FVector2D& UVC, const FVector2D& UVD, const FVector& DesiredNormal, const FVector2D& FoldNormalData = FVector2D::ZeroVector);
	static void RecalculateNormals(FMeshBuildData& Data);

	// 播放门音效；RemainingAnimTime 为本次动画剩余秒数，用于将音效时长同步到动画时长。
	void PlayDoorSound(USoundBase* Sound, float RemainingAnimTime);

	// 当前动画方向：+1 展平（→1），-1 折叠（→0），0 停止
	float AnimationDirection = 0.0f;

	// 上一帧应用的 EffectiveOpen，用于驱动材质 PreviousFrameSwitch 输出 WPO 运动矢量。
	// <0 表示尚未初始化（首帧令 prev=current，避免假位移）。
	float LastEffectiveOpen = -1.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LastAppliedDoorMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LastAppliedWoodMaterial;

	// 折叠动画通过这两个动态材质实例的 EffectiveOpen 标量驱动（WPO），不再逐帧重建网格。
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DoorMID;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> WoodMID;
};
