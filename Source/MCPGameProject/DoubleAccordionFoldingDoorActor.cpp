#include "DoubleAccordionFoldingDoorActor.h"

#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ProceduralMeshComponent.h"
#include "Sound/SoundBase.h"

ADoubleAccordionFoldingDoorActor::ADoubleAccordionFoldingDoorActor()
{
	PrimaryActorTick.bCanEverTick = true;

	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	RootComponent = ProceduralMesh;
	ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProceduralMesh->bUseAsyncCooking = true;

	// 中间两根木条做成独立子组件，靠 Transform 平移跟随开合。刚体平移天然产生运动矢量，
	// 不会被 TSR 糊，且无需给 Substance 木材材质加 WPO（保持原木纹外观）。
	InnerBarLeft = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("InnerBarLeft"));
	InnerBarLeft->SetupAttachment(ProceduralMesh);
	InnerBarLeft->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InnerBarRight = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("InnerBarRight"));
	InnerBarRight->SetupAttachment(ProceduralMesh);
	InnerBarRight->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	DoorAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("DoorAudio"));
	DoorAudio->SetupAttachment(ProceduralMesh);
	DoorAudio->bAutoActivate = false;
}

void ADoubleAccordionFoldingDoorActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RebuildDoor();
}

void ADoubleAccordionFoldingDoorActor::BeginPlay()
{
	Super::BeginPlay();
	RebuildDoor();

	// 绑定键盘 上/下 方向键，供无手势设备调试。手势/蓝图可直接调用同一组
	// StartFlattening / StartFolding / StopDoorAnimation，保证多种输入始终同步。
	if (!bBindArrowKeys)
	{
		return;
	}

	if (APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0))
	{
		EnableInput(PlayerController);
		if (InputComponent)
		{
			InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &ADoubleAccordionFoldingDoorActor::StartFlattening);
			InputComponent->BindKey(EKeys::Up, IE_Released, this, &ADoubleAccordionFoldingDoorActor::StopDoorAnimation);
			InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &ADoubleAccordionFoldingDoorActor::StartFolding);
			InputComponent->BindKey(EKeys::Down, IE_Released, this, &ADoubleAccordionFoldingDoorActor::StopDoorAnimation);
		}
	}
}

void ADoubleAccordionFoldingDoorActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (FMath::IsNearlyZero(AnimationDirection))
	{
		return;
	}

	const float Speed = 1.0f / FMath::Max(KINDA_SMALL_NUMBER, AnimationDuration);
	const float NewOpen = FMath::Clamp(OpenAmount + AnimationDirection * Speed * DeltaSeconds, 0.0f, 1.0f);
	if (!FMath::IsNearlyEqual(NewOpen, OpenAmount))
	{
		OpenAmount = NewOpen;
		// 不重建网格：只把开合量推给材质 WPO，保留稳定拓扑与正确运动矢量。
		ApplyOpenAmountToMaterial();
	}

	// 到达端点自动停止，并停止音效（即使按键仍按住）。
	if ((AnimationDirection > 0.0f && OpenAmount >= 1.0f) ||
		(AnimationDirection < 0.0f && OpenAmount <= 0.0f))
	{
		StopDoorAnimation();
	}
}

void ADoubleAccordionFoldingDoorActor::StartFlattening()
{
	if (OpenAmount >= 1.0f)
	{
		return; // 已完全展平
	}
	AnimationDirection = 1.0f;
	PlayDoorSound(FlattenSound, (1.0f - OpenAmount) * AnimationDuration);
}

void ADoubleAccordionFoldingDoorActor::StartFolding()
{
	if (OpenAmount <= 0.0f)
	{
		return; // 已完全折叠
	}
	AnimationDirection = -1.0f;
	PlayDoorSound(FoldSound, OpenAmount * AnimationDuration);
}

void ADoubleAccordionFoldingDoorActor::StopDoorAnimation()
{
	AnimationDirection = 0.0f;
	if (DoorAudio)
	{
		DoorAudio->Stop();
	}

	// 停止时把材质“上一帧”对齐到“当前帧”，让 WPO 运动矢量归零。否则 EffectiveOpenPrev≠EffectiveOpen
	// 会被冻结，速度 pass 每帧都算出恒定假位移，TSR 误判表面仍在运动而持续糊（表现为松开方向键后反而模糊）。
	LastEffectiveOpen = GetEffectiveOpenAmount();
	ApplyOpenAmountToMaterial();
}

void ADoubleAccordionFoldingDoorActor::PlayDoorSound(USoundBase* Sound, float RemainingAnimTime)
{
	if (!Sound || !DoorAudio)
	{
		return;
	}

	DoorAudio->Stop();
	DoorAudio->SetSound(Sound);

	// 调整播放速率，使音效自然时长被压缩/拉伸到本次动画剩余时长，实现音画同步。
	float Pitch = 1.0f;
	if (bSyncSoundToAnimation && RemainingAnimTime > KINDA_SMALL_NUMBER)
	{
		const float SoundDuration = Sound->GetDuration();
		if (SoundDuration > KINDA_SMALL_NUMBER && SoundDuration < INDEFINITELY_LOOPING_DURATION)
		{
			Pitch = SoundDuration / RemainingAnimTime;
		}
	}
	DoorAudio->SetPitchMultiplier(Pitch);
	DoorAudio->Play();
}

void ADoubleAccordionFoldingDoorActor::SetOpenAmount(float Value)
{
	OpenAmount = Value;
	ApplyOpenAmountToMaterial();
}

void ADoubleAccordionFoldingDoorActor::SetDoorParameters(
	float InOpenAmount,
	int32 InFoldCount,
	float InTotalWidth,
	float InStripHeight,
	float InMinimumOpenAmount,
	float InDepthScale,
	float InThickness,
	float InEndStripWidth,
	float InEndStripDepth)
{
	OpenAmount = InOpenAmount;
	FoldCount = InFoldCount;
	TotalWidth = InTotalWidth;
	StripHeight = InStripHeight;
	MinimumOpenAmount = InMinimumOpenAmount;
	DepthScale = InDepthScale;
	Thickness = InThickness;
	EndStripWidth = InEndStripWidth;
	EndStripDepth = InEndStripDepth;
	RebuildDoor();
}

void ADoubleAccordionFoldingDoorActor::RebuildDoor()
{
	ClampParameters();
	if (!ProceduralMesh)
	{
		return;
	}

	UMaterialInterface* ResolvedDoorMaterial = ResolveSectionMaterial(0, DoorMaterial, LastAppliedDoorMaterial);
	UMaterialInterface* ResolvedWoodMaterial = ResolveSectionMaterial(1, WoodMaterial, LastAppliedWoodMaterial);

	// 只构建一次「静止位姿」(EffectiveOpen=1，全展平、无折叠深度) 的稳定拓扑。折叠动画由材质
	// WPO 完成：逐帧只改 EffectiveOpen 标量，顶点位置/拓扑保持不变。deforming 表面因此拥有正确
	// 的运动矢量（WPO 速度输出），TSR/TAA 不会再把布料纹理在动画中糊成一团。
	const float TextileWidth = GetTextileWidthPerSide();
	const int32 PanelCount = FMath::Max(2, FoldCount * 2);
	const float PanelWidth = TextileWidth / static_cast<float>(PanelCount);

	FMeshBuildData DoorData;
	const int32 LeftStart = DoorData.Vertices.Num();
	AppendDoorLeaf(DoorData, true, 1.0f, TextileWidth, PanelCount, PanelWidth, 0.0f);
	const int32 LeftEnd = DoorData.Vertices.Num();
	AppendDoorLeaf(DoorData, false, 1.0f, TextileWidth, PanelCount, PanelWidth, 0.0f);
	const int32 RightEnd = DoorData.Vertices.Num();

	BakeClothFoldCoefficients(DoorData, LeftStart, LeftEnd, true, PanelWidth, PanelCount);
	BakeClothFoldCoefficients(DoorData, LeftEnd, RightEnd, false, PanelWidth, PanelCount);

	FMeshBuildData WoodData;
	const float HalfStrip = EndStripWidth * 0.5f;
	const float HalfHeight = StripHeight * 1.08f * 0.5f;
	const float HalfDepth = EndStripDepth * 0.5f;
	const FVector StripExtent(HalfStrip, HalfDepth, HalfHeight);

	// 外侧两根木条固定不动，留在主网格 section 1。
	auto AppendWoodStrip = [this, &WoodData](const FVector& Center, const FVector& Extent)
	{
		const int32 Start = WoodData.Vertices.Num();
		AppendBox(WoodData, Center, Extent);
		WoodData.UV1.SetNumZeroed(WoodData.Vertices.Num());
		WoodData.UV2.SetNumZeroed(WoodData.Vertices.Num());
	};

	AppendWoodStrip(FVector(HalfStrip, 0.0f, 0.0f), StripExtent);
	AppendWoodStrip(FVector(TotalWidth - HalfStrip, 0.0f, 0.0f), StripExtent);

	UpdateOrCreateMeshSection(0, DoorData);
	UpdateOrCreateMeshSection(1, WoodData);

	if (ResolvedDoorMaterial)
	{
		DoorMID = ProceduralMesh->CreateAndSetMaterialInstanceDynamicFromMaterial(0, ResolvedDoorMaterial);
		LastAppliedDoorMaterial = ResolvedDoorMaterial;
	}
	if (ResolvedWoodMaterial)
	{
		WoodMID = ProceduralMesh->CreateAndSetMaterialInstanceDynamicFromMaterial(1, ResolvedWoodMaterial);
		LastAppliedWoodMaterial = ResolvedWoodMaterial;
	}

	// 中间两根木条：独立子组件，居中建一次盒体，靠 Transform 平移跟随开合。
	// 材质优先用用户指定的 InnerBarMaterial；留空则默认复用木材主段槽位材质（与外框木纹一致）。
	UMaterialInterface* WoodSlotMaterial = ProceduralMesh->GetMaterial(1);
	UMaterialInterface* BarMaterial = InnerBarMaterial
		? InnerBarMaterial.Get()
		: (WoodSlotMaterial ? WoodSlotMaterial : ResolvedWoodMaterial);
	BuildInnerBars(StripExtent, BarMaterial);
	UpdateInnerBarPositions();

	// 重建后用当前开合量初始化材质参数（编辑器里也能按 OpenAmount 预览折叠）。
	ApplyOpenAmountToMaterial();
}

void ADoubleAccordionFoldingDoorActor::BuildInnerBars(const FVector& Extent, UMaterialInterface* WoodMat)
{
	const TArray<FVector2D> EmptyUV;
	const TArray<FLinearColor> EmptyColors;
	for (UProceduralMeshComponent* Bar : {InnerBarLeft.Get(), InnerBarRight.Get()})
	{
		if (!Bar)
		{
			continue;
		}
		FMeshBuildData Data;
		AppendBox(Data, FVector::ZeroVector, Extent); // 居中盒体，世界位置由组件 Transform 决定
		Bar->CreateMeshSection_LinearColor(0, Data.Vertices, Data.Triangles, Data.Normals,
			Data.UVs, EmptyUV, EmptyUV, EmptyUV, EmptyColors, Data.Tangents, false);
		if (WoodMat)
		{
			Bar->SetMaterial(0, WoodMat);
		}
	}
}

void ADoubleAccordionFoldingDoorActor::UpdateInnerBarPositions()
{
	const float EffectiveOpen = GetEffectiveOpenAmount();
	const float TextileWidth = GetTextileWidthPerSide();
	const float HalfStrip = EndStripWidth * 0.5f;
	const float ProjectedTextileWidth = TextileWidth * EffectiveOpen;

	if (InnerBarLeft)
	{
		InnerBarLeft->SetRelativeLocation(FVector(EndStripWidth + ProjectedTextileWidth + HalfStrip, 0.0f, 0.0f));
	}
	if (InnerBarRight)
	{
		InnerBarRight->SetRelativeLocation(FVector(TotalWidth - EndStripWidth - ProjectedTextileWidth - HalfStrip, 0.0f, 0.0f));
	}
}

void ADoubleAccordionFoldingDoorActor::ClampParameters()
{
	OpenAmount = FMath::Clamp(OpenAmount, 0.0f, 1.0f);
	FoldCount = FMath::Max(1, FoldCount);
	TotalWidth = FMath::Max(100.0f, TotalWidth);
	StripHeight = FMath::Max(10.0f, StripHeight);
	MinimumOpenAmount = FMath::Clamp(MinimumOpenAmount, 0.02f, 0.8f);
	DepthScale = FMath::Clamp(DepthScale, 0.0f, 4.0f);
	Thickness = FMath::Max(0.0f, Thickness);
	EndStripWidth = FMath::Max(1.0f, EndStripWidth);
	EndStripDepth = FMath::Max(1.0f, EndStripDepth);
	HeightVertices = FMath::Clamp(HeightVertices, 2, 128);

	const float MaxStripWidth = FMath::Max(1.0f, TotalWidth * 0.20f);
	EndStripWidth = FMath::Min(EndStripWidth, MaxStripWidth);
}

float ADoubleAccordionFoldingDoorActor::GetEffectiveOpenAmount() const
{
	return MinimumOpenAmount + OpenAmount * (1.0f - MinimumOpenAmount);
}

float ADoubleAccordionFoldingDoorActor::GetTextileWidthPerSide() const
{
	return FMath::Max(1.0f, TotalWidth * 0.5f - EndStripWidth * 2.0f);
}

float ADoubleAccordionFoldingDoorActor::EvaluateFoldDepth(float DistanceFromOuterHinge, float PanelWidth, int32 PanelCount, float FoldDepth) const
{
	if (PanelWidth <= KINDA_SMALL_NUMBER || PanelCount <= 0 || FMath::IsNearlyZero(FoldDepth))
	{
		return 0.0f;
	}

	const float Phase = FMath::Frac(DistanceFromOuterHinge / (PanelWidth * 2.0f));
	const float Triangle01 = 1.0f - FMath::Abs(Phase * 2.0f - 1.0f);
	const float SignedTriangle = Triangle01 * 2.0f - 1.0f;

	const float CreaseIndex = DistanceFromOuterHinge / PanelWidth;
	const float DistanceToNearestEnd = FMath::Min(CreaseIndex, static_cast<float>(PanelCount) - CreaseIndex);
	const float EndpointTaper = FMath::Clamp(DistanceToNearestEnd, 0.0f, 1.0f);
	return SignedTriangle * FoldDepth * EndpointTaper;
}

float ADoubleAccordionFoldingDoorActor::FoldDepthCoefficient(float DistanceFromOuterHinge, float PanelWidth, int32 PanelCount) const
{
	// 与 EvaluateFoldDepth 同一套折面公式，但剥离随开合变化的 FoldDepth 项，仅返回与顶点位置相关
	// 的常量系数。完整深度 = 本系数 * sqrt(1-eo^2)，由材质 WPO 在运行时乘上 sqrt 项。
	if (PanelWidth <= KINDA_SMALL_NUMBER || PanelCount <= 0)
	{
		return 0.0f;
	}

	const float Phase = FMath::Frac(DistanceFromOuterHinge / (PanelWidth * 2.0f));
	const float Triangle01 = 1.0f - FMath::Abs(Phase * 2.0f - 1.0f);
	const float SignedTriangle = Triangle01 * 2.0f - 1.0f;

	const float CreaseIndex = DistanceFromOuterHinge / PanelWidth;
	const float DistanceToNearestEnd = FMath::Min(CreaseIndex, static_cast<float>(PanelCount) - CreaseIndex);
	const float EndpointTaper = FMath::Clamp(DistanceToNearestEnd, 0.0f, 1.0f);

	// FoldDepth = PanelWidth * sqrt(1-eo^2) * DepthScale，这里去掉 sqrt 项，保留其余常量。
	return SignedTriangle * EndpointTaper * PanelWidth * DepthScale;
}

void ADoubleAccordionFoldingDoorActor::BakeClothFoldCoefficients(
	FMeshBuildData& Data,
	int32 StartIndex,
	int32 EndIndex,
	bool bLeftLeaf,
	float PanelWidth,
	int32 PanelCount) const
{
	if (Data.UV1.Num() != Data.Vertices.Num())
	{
		Data.UV1.SetNumZeroed(Data.Vertices.Num());
	}

	// 静止位姿(eo=1)下：左叶 X = EndStripWidth + Distance，右叶 X = TotalWidth - EndStripWidth - Distance。
	// 据此从顶点 X 反推每个顶点沿表面到外铰链的距离 Distance，再算折叠系数。
	for (int32 Index = StartIndex; Index < EndIndex && Index < Data.Vertices.Num(); ++Index)
	{
		const float X = Data.Vertices[Index].X;
		const float Distance = bLeftLeaf
			? (X - EndStripWidth)
			: ((TotalWidth - EndStripWidth) - X);
		const float ClampedDistance = FMath::Max(0.0f, Distance);

		// Cx：material WPO 的 X 偏移 = Cx*(eo-1)。左叶向中缝收拢取 +Distance，右叶镜像取 -Distance。
		const float Cx = bLeftLeaf ? ClampedDistance : -ClampedDistance;
		const float Cy = FoldDepthCoefficient(ClampedDistance, PanelWidth, PanelCount);
		Data.UV1[Index] = FVector2D(Cx, Cy);
	}
}

void ADoubleAccordionFoldingDoorActor::ApplyOpenAmountToMaterial()
{
	OpenAmount = FMath::Clamp(OpenAmount, 0.0f, 1.0f);
	const float EffectiveOpen = GetEffectiveOpenAmount();
	// 上一帧的 EffectiveOpen：供材质 PreviousFrameSwitch 计算 WPO 运动矢量（TSR 去模糊的关键）。
	// 仅靠 DMI 标量参数驱动的 WPO，引擎默认算不出运动矢量（上一帧用的是当前参数值，差值为 0）；
	// 把上一帧值单独喂给 PreviousFrameSwitch 的“上一帧”分支，速度 pass 才能得到正确位移。
	const float PrevEffectiveOpen = (LastEffectiveOpen < 0.0f) ? EffectiveOpen : LastEffectiveOpen;

	static const FName EffectiveOpenParam(TEXT("EffectiveOpen"));
	static const FName EffectiveOpenPrevParam(TEXT("EffectiveOpenPrev"));
	if (DoorMID)
	{
		DoorMID->SetScalarParameterValue(EffectiveOpenPrevParam, PrevEffectiveOpen);
		DoorMID->SetScalarParameterValue(EffectiveOpenParam, EffectiveOpen);
	}
	if (WoodMID)
	{
		WoodMID->SetScalarParameterValue(EffectiveOpenPrevParam, PrevEffectiveOpen);
		WoodMID->SetScalarParameterValue(EffectiveOpenParam, EffectiveOpen);
	}

	// 中间木条靠 Transform 平移跟随开合（与布料 WPO 同步）。
	UpdateInnerBarPositions();

	LastEffectiveOpen = EffectiveOpen;
}

void ADoubleAccordionFoldingDoorActor::AppendDoorLeaf(
	FMeshBuildData& Data,
	bool bLeftLeaf,
	float EffectiveOpen,
	float TextileWidth,
	int32 PanelCount,
	float PanelWidth,
	float FoldDepth) const
{
	const int32 Columns = PanelCount + 1;
	const int32 Rows = FMath::Max(2, HeightVertices);
	const float HalfHeight = StripHeight * 0.5f;
	const float HalfThickness = Thickness * 0.5f;

	auto CenterPosition = [this, bLeftLeaf, EffectiveOpen, PanelWidth, PanelCount, FoldDepth, HalfHeight](int32 Column, int32 Row, int32 Rows)
	{
		const float Distance = static_cast<float>(Column) * PanelWidth;
		const float CenterDepth = EvaluateFoldDepth(Distance, PanelWidth, PanelCount, FoldDepth);
		const float X = bLeftLeaf
			? EndStripWidth + Distance * EffectiveOpen
			: TotalWidth - EndStripWidth - Distance * EffectiveOpen;
		const float V = Rows > 1 ? static_cast<float>(Row) / static_cast<float>(Rows - 1) : 0.0f;
		const float Z = -HalfHeight + V * StripHeight;
		return FVector(X, CenterDepth, Z);
	};

	auto SurfacePosition = [&CenterPosition](int32 Side, int32 Column, int32 Row, int32 Rows, float HalfThickness)
	{
		FVector Position = CenterPosition(Column, Row, Rows);
		Position.Y += Side == 0 ? HalfThickness : -HalfThickness;
		return Position;
	};

	auto UV = [this, bLeftLeaf, TextileWidth, PanelWidth](int32 Column, int32 Row, int32 Rows)
	{
		// 与 Blender GN 版本一致：UV 沿布料表面展开（用表面宽度 PanelWidth 累积，不随折叠投影），
		// 一张完整纹理铺满整个门表面，折叠时纹理随 zigzag 折面连续弯折、贴合表面。
		const float SurfaceDistance = static_cast<float>(Column) * PanelWidth;
		const float TotalSurfaceWidth = TextileWidth * 2.0f; // 两片叶子合计的展开宽度
		// 左叶 U 从外铰链 0 → 中缝 0.5，右叶镜像 0.5 → 1，整幅图横跨两叶铺满。
		const float NormalizedU = bLeftLeaf
			? SurfaceDistance / TotalSurfaceWidth
			: 1.0f - SurfaceDistance / TotalSurfaceWidth;
		const float NormalizedV = Rows > 1 ? static_cast<float>(Row) / static_cast<float>(Rows - 1) : 0.0f;
		// UVScale 为平铺次数：1.0 = 一张图铺满整个门（默认，匹配 Blender）；>1 则重复平铺。
		return FVector2D(NormalizedU * UVScale, NormalizedV * UVScale);
	};

	auto OutwardNormal = [&CenterPosition, Rows](int32 Side, int32 Column)
	{
		const int32 NextColumn = Column + 1;
		const FVector Tangent = (CenterPosition(NextColumn, 0, Rows) - CenterPosition(Column, 0, Rows)).GetSafeNormal();
		const FVector Candidate = FVector::CrossProduct(Tangent, FVector::ZAxisVector).GetSafeNormal();
		const FVector SurfaceOffsetDirection = Side == 0 ? FVector::YAxisVector : -FVector::YAxisVector;
		return FVector::DotProduct(Candidate, SurfaceOffsetDirection) >= 0.0f ? Candidate : -Candidate;
	};

	// 折面坡度系数：d(深度系数)/d(表面距离)，并把左右叶方向折进符号。
	// material 端实际坡度 = 本系数 * sqrt(1-eo^2)/eo，正反面再乘 ny(±1) 得解析法线。
	const float LeafDir = bLeftLeaf ? 1.0f : -1.0f;
	auto FacetSlopeCoeff = [this, PanelWidth, PanelCount, LeafDir](int32 Column) -> float
	{
		const float CyHere = FoldDepthCoefficient(static_cast<float>(Column) * PanelWidth, PanelWidth, PanelCount);
		const float CyNext = FoldDepthCoefficient(static_cast<float>(Column + 1) * PanelWidth, PanelWidth, PanelCount);
		return LeafDir * (CyNext - CyHere) / FMath::Max(KINDA_SMALL_NUMBER, PanelWidth);
	};

	for (int32 Column = 0; Column < PanelCount; ++Column)
	{
		for (int32 Row = 0; Row < Rows - 1; ++Row)
		{
			AppendSplitQuad(
				Data,
				SurfacePosition(0, Column, Row, Rows, HalfThickness),
				SurfacePosition(0, Column + 1, Row, Rows, HalfThickness),
				SurfacePosition(0, Column + 1, Row + 1, Rows, HalfThickness),
				SurfacePosition(0, Column, Row + 1, Rows, HalfThickness),
				UV(Column, Row, Rows),
				UV(Column + 1, Row, Rows),
				UV(Column + 1, Row + 1, Rows),
				UV(Column, Row + 1, Rows),
				OutwardNormal(0, Column),
				FVector2D(FacetSlopeCoeff(Column), 1.0f));   // 正面 ny=+1

			AppendSplitQuad(
				Data,
				SurfacePosition(1, Column, Row, Rows, HalfThickness),
				SurfacePosition(1, Column + 1, Row, Rows, HalfThickness),
				SurfacePosition(1, Column + 1, Row + 1, Rows, HalfThickness),
				SurfacePosition(1, Column, Row + 1, Rows, HalfThickness),
				UV(Column, Row, Rows),
				UV(Column + 1, Row, Rows),
				UV(Column + 1, Row + 1, Rows),
				UV(Column, Row + 1, Rows),
				OutwardNormal(1, Column),
				FVector2D(FacetSlopeCoeff(Column), -1.0f));  // 背面 ny=-1
		}
	}

	for (int32 SideColumn : {0, PanelCount})
	{
		for (int32 Row = 0; Row < Rows - 1; ++Row)
		{
			const FVector DesiredNormal = (bLeftLeaf == (SideColumn == 0)) ? -FVector::XAxisVector : FVector::XAxisVector;
			AppendSplitQuad(
				Data,
				SurfacePosition(0, SideColumn, Row, Rows, HalfThickness),
				SurfacePosition(1, SideColumn, Row, Rows, HalfThickness),
				SurfacePosition(1, SideColumn, Row + 1, Rows, HalfThickness),
				SurfacePosition(0, SideColumn, Row + 1, Rows, HalfThickness),
				UV(SideColumn, Row, Rows),
				UV(SideColumn, Row, Rows),
				UV(SideColumn, Row + 1, Rows),
				UV(SideColumn, Row + 1, Rows),
				DesiredNormal);
		}
	}

	for (int32 Column = 0; Column < PanelCount; ++Column)
	{
		AppendSplitQuad(
			Data,
			SurfacePosition(0, Column, 0, Rows, HalfThickness),
			SurfacePosition(0, Column + 1, 0, Rows, HalfThickness),
			SurfacePosition(1, Column + 1, 0, Rows, HalfThickness),
			SurfacePosition(1, Column, 0, Rows, HalfThickness),
			UV(Column, 0, Rows),
			UV(Column + 1, 0, Rows),
			UV(Column + 1, 0, Rows),
			UV(Column, 0, Rows),
			-FVector::ZAxisVector);
		AppendSplitQuad(
			Data,
			SurfacePosition(0, Column + 1, Rows - 1, Rows, HalfThickness),
			SurfacePosition(0, Column, Rows - 1, Rows, HalfThickness),
			SurfacePosition(1, Column, Rows - 1, Rows, HalfThickness),
			SurfacePosition(1, Column + 1, Rows - 1, Rows, HalfThickness),
			UV(Column + 1, Rows - 1, Rows),
			UV(Column, Rows - 1, Rows),
			UV(Column, Rows - 1, Rows),
			UV(Column + 1, Rows - 1, Rows),
			FVector::ZAxisVector);
	}
}

void ADoubleAccordionFoldingDoorActor::AppendBox(FMeshBuildData& Data, const FVector& Center, const FVector& Extent) const
{
	const FVector Min = Center - Extent;
	const FVector Max = Center + Extent;
	const float Tile = FMath::Max(KINDA_SMALL_NUMBER, WoodUVTileSize);

	// Box 映射：按面法线主轴选用对应的两个世界坐标轴投影，纹理按世界尺度等比平铺，
	// 避免细长木条上的纹理被拉伸。
	auto BoxUV = [Tile](const FVector& P, const FVector& N) -> FVector2D
	{
		const float AbsX = FMath::Abs(N.X);
		const float AbsY = FMath::Abs(N.Y);
		const float AbsZ = FMath::Abs(N.Z);
		if (AbsX >= AbsY && AbsX >= AbsZ)
		{
			return FVector2D(P.Y / Tile, P.Z / Tile); // ±X 面 → (Y, Z)
		}
		if (AbsY >= AbsX && AbsY >= AbsZ)
		{
			return FVector2D(P.X / Tile, P.Z / Tile); // ±Y 面 → (X, Z)
		}
		return FVector2D(P.X / Tile, P.Y / Tile);     // ±Z 面 → (X, Y)
	};

	auto AppendFace = [&Data, &BoxUV](const FVector& A, const FVector& B, const FVector& C, const FVector& D, const FVector& DesiredNormal)
	{
		AppendSplitQuad(
			Data,
			A, B, C, D,
			BoxUV(A, DesiredNormal),
			BoxUV(B, DesiredNormal),
			BoxUV(C, DesiredNormal),
			BoxUV(D, DesiredNormal),
			DesiredNormal);
	};

	AppendFace(FVector(Min.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Min.Z), FVector(Max.X, Max.Y, Min.Z), FVector(Min.X, Max.Y, Min.Z), -FVector::ZAxisVector);
	AppendFace(FVector(Min.X, Min.Y, Max.Z), FVector(Min.X, Max.Y, Max.Z), FVector(Max.X, Max.Y, Max.Z), FVector(Max.X, Min.Y, Max.Z), FVector::ZAxisVector);
	AppendFace(FVector(Min.X, Min.Y, Min.Z), FVector(Min.X, Min.Y, Max.Z), FVector(Max.X, Min.Y, Max.Z), FVector(Max.X, Min.Y, Min.Z), -FVector::YAxisVector);
	AppendFace(FVector(Max.X, Min.Y, Min.Z), FVector(Max.X, Min.Y, Max.Z), FVector(Max.X, Max.Y, Max.Z), FVector(Max.X, Max.Y, Min.Z), FVector::XAxisVector);
	AppendFace(FVector(Max.X, Max.Y, Min.Z), FVector(Max.X, Max.Y, Max.Z), FVector(Min.X, Max.Y, Max.Z), FVector(Min.X, Max.Y, Min.Z), FVector::YAxisVector);
	AppendFace(FVector(Min.X, Max.Y, Min.Z), FVector(Min.X, Max.Y, Max.Z), FVector(Min.X, Min.Y, Max.Z), FVector(Min.X, Min.Y, Min.Z), -FVector::XAxisVector);
}

void ADoubleAccordionFoldingDoorActor::UpdateOrCreateMeshSection(int32 SectionIndex, const FMeshBuildData& Data)
{
	if (!ProceduralMesh)
	{
		return;
	}

	// 网格只在结构参数变化（首次构建 / 改 FoldCount、HeightVertices 等）时重建。折叠动画不再走这里，
	// 而是由材质 WPO 驱动。用 _LinearColor 重载是为了携带 UV1（WPO 折叠系数）与 UV2（解析法线数据）。
	const TArray<FVector2D> EmptyUV;
	const TArray<FLinearColor> EmptyColors;
	ProceduralMesh->CreateMeshSection_LinearColor(
		SectionIndex,
		Data.Vertices,
		Data.Triangles,
		Data.Normals,
		Data.UVs,   // UV0：纹理
		Data.UV1,   // UV1：WPO 折叠系数 (Cx, Cy)
		Data.UV2,   // UV2：解析法线数据 (折面坡度系数, ny)
		EmptyUV,    // UV3
		EmptyColors,
		Data.Tangents,
		false);
}

UMaterialInterface* ADoubleAccordionFoldingDoorActor::ResolveSectionMaterial(
	int32 SectionIndex,
	TObjectPtr<UMaterialInterface>& ConfiguredMaterial,
	TObjectPtr<UMaterialInterface>& LastAppliedMaterial) const
{
	UMaterialInterface* CurrentSlotMaterial = ProceduralMesh ? ProceduralMesh->GetMaterial(SectionIndex) : nullptr;

	if (CurrentSlotMaterial && CurrentSlotMaterial != LastAppliedMaterial)
	{
		ConfiguredMaterial = CurrentSlotMaterial;
		return CurrentSlotMaterial;
	}

	if (ConfiguredMaterial)
	{
		return ConfiguredMaterial;
	}

	return CurrentSlotMaterial;
}

void ADoubleAccordionFoldingDoorActor::AppendQuad(FMeshBuildData& Data, int32 A, int32 B, int32 C, int32 D)
{
	Data.Triangles.Append({A, B, C, A, C, D});
}

void ADoubleAccordionFoldingDoorActor::AppendQuadFacing(FMeshBuildData& Data, int32 A, int32 B, int32 C, int32 D, const FVector& DesiredNormal)
{
	if (!Data.Vertices.IsValidIndex(A) || !Data.Vertices.IsValidIndex(B) || !Data.Vertices.IsValidIndex(C) || DesiredNormal.IsNearlyZero())
	{
		AppendQuad(Data, A, B, C, D);
		return;
	}

	const FVector FaceNormal = FVector::CrossProduct(Data.Vertices[B] - Data.Vertices[A], Data.Vertices[C] - Data.Vertices[A]).GetSafeNormal();
	if (FVector::DotProduct(FaceNormal, DesiredNormal.GetSafeNormal()) < 0.0f)
	{
		AppendQuad(Data, A, D, C, B);
		return;
	}

	AppendQuad(Data, A, B, C, D);
}

void ADoubleAccordionFoldingDoorActor::AppendSplitQuad(
	FMeshBuildData& Data,
	const FVector& A,
	const FVector& B,
	const FVector& C,
	const FVector& D,
	const FVector2D& UVA,
	const FVector2D& UVB,
	const FVector2D& UVC,
	const FVector2D& UVD,
	const FVector& DesiredNormal,
	const FVector2D& FoldNormalData)
{
	const int32 Base = Data.Vertices.Num();
	const FVector Normal = DesiredNormal.GetSafeNormal();
	FVector TangentVector = (B - A).GetSafeNormal();
	if (TangentVector.IsNearlyZero())
	{
		TangentVector = FVector::XAxisVector;
	}

	Data.Vertices.Append({A, B, C, D});
	Data.UVs.Append({UVA, UVB, UVC, UVD});
	// UV2 每面恒定：(折面坡度系数, 正反面符号 ny)。非布料面传零 → 材质回退到静止顶点法线。
	Data.UV2.Append({FoldNormalData, FoldNormalData, FoldNormalData, FoldNormalData});
	Data.Normals.Append({Normal, Normal, Normal, Normal});
	Data.Colors.Append({FColor::White, FColor::White, FColor::White, FColor::White});
	Data.Tangents.Append({
		FProcMeshTangent(TangentVector, false),
		FProcMeshTangent(TangentVector, false),
		FProcMeshTangent(TangentVector, false),
		FProcMeshTangent(TangentVector, false)
	});

	// UE 渲染/光照以 Cross(C-A, B-A) 作为三角形正面法线（与引擎 CalculateTangentsForMesh 的
	// Edge02 ^ Edge01 一致），与数学右手叉积 Cross(B-A, C-A) 符号相反。必须按 UE 约定判断绕序，
	// 否则生成的渲染正面会整体朝内，导致单面材质（如木条）漆黑、双面布料背光发暗。
	const FVector RenderFaceNormal = FVector::CrossProduct(C - A, B - A).GetSafeNormal();
	if (!RenderFaceNormal.IsNearlyZero() && FVector::DotProduct(RenderFaceNormal, Normal) < 0.0f)
	{
		Data.Triangles.Append({Base, Base + 3, Base + 2, Base, Base + 2, Base + 1});
		return;
	}

	Data.Triangles.Append({Base, Base + 1, Base + 2, Base, Base + 2, Base + 3});
}

void ADoubleAccordionFoldingDoorActor::RecalculateNormals(FMeshBuildData& Data)
{
	Data.Normals.Init(FVector::ZeroVector, Data.Vertices.Num());
	for (int32 Index = 0; Index + 2 < Data.Triangles.Num(); Index += 3)
	{
		const int32 A = Data.Triangles[Index];
		const int32 B = Data.Triangles[Index + 1];
		const int32 C = Data.Triangles[Index + 2];
		if (!Data.Vertices.IsValidIndex(A) || !Data.Vertices.IsValidIndex(B) || !Data.Vertices.IsValidIndex(C))
		{
			continue;
		}

		const FVector Normal = FVector::CrossProduct(Data.Vertices[B] - Data.Vertices[A], Data.Vertices[C] - Data.Vertices[A]).GetSafeNormal();
		Data.Normals[A] += Normal;
		Data.Normals[B] += Normal;
		Data.Normals[C] += Normal;
	}

	for (FVector& Normal : Data.Normals)
	{
		Normal = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
	}

	Data.Colors.Init(FColor::White, Data.Vertices.Num());
	Data.Tangents.Reset();
	Data.Tangents.SetNum(Data.Vertices.Num());
	for (FProcMeshTangent& Tangent : Data.Tangents)
	{
		Tangent = FProcMeshTangent(1.0f, 0.0f, 0.0f);
	}
}
