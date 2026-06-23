#include "ChineseSealPoolAnimator.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
void BuildHardNormalSection(FChineseSealProceduralMesh& MeshData)
{
	MeshData.SectionVertices.Reset();
	MeshData.SectionTriangles.Reset();
	MeshData.SectionNormals.Reset();
	MeshData.SectionUV0.Reset();
	MeshData.SectionTangents.Reset();

	const int32 TriangleCount = MeshData.Triangles.Num() / 3;
	MeshData.SectionVertices.Reserve(TriangleCount * 3);
	MeshData.SectionTriangles.Reserve(TriangleCount * 3);
	MeshData.SectionNormals.Reserve(TriangleCount * 3);
	MeshData.SectionUV0.Reserve(TriangleCount * 3);
	MeshData.SectionTangents.Reserve(TriangleCount * 3);


	for (int32 TriangleIndex = 0; TriangleIndex + 2 < MeshData.Triangles.Num(); TriangleIndex += 3)
	{
		const int32 TriangleNumber = TriangleIndex / 3;
		const int32 IndexA = MeshData.Triangles[TriangleIndex];
		const int32 IndexB = MeshData.Triangles[TriangleIndex + 1];
		const int32 IndexC = MeshData.Triangles[TriangleIndex + 2];
		if (!MeshData.Vertices.IsValidIndex(IndexA) || !MeshData.Vertices.IsValidIndex(IndexB) || !MeshData.Vertices.IsValidIndex(IndexC))
		{
			continue;
		}

		const FVector& A = MeshData.Vertices[IndexA];
		const FVector& B = MeshData.Vertices[IndexB];
		const FVector& C = MeshData.Vertices[IndexC];
		// 用几何法线（缠绕 A, C, B 对应 Cross(C-A, B-A)）作为初值，再强制朝外。
		// 不信任源数据法线：base cube 朝内、文字/边框朝外，方向不一致。
		// 直接用 Blender 导出的朝外法线（.sealmesh 已转为 UE 坐标系）。不再用几何法线+强制朝外，
		// 那对凸起笔画的侧面会判错。Blender 端 normals_make_consistent 已保证含笔画侧面正确朝外。
		FVector FaceNormal = MeshData.TriangleNormals.IsValidIndex(TriangleNumber)
			? MeshData.TriangleNormals[TriangleNumber].GetSafeNormal()
			: FVector::CrossProduct(B - A, C - A).GetSafeNormal();
		if (FaceNormal.IsNearlyZero())
		{
			FaceNormal = FVector::UpVector;
		}

		FVector TangentX = FVector::CrossProduct(FVector::UpVector, FaceNormal).GetSafeNormal();
		if (TangentX.IsNearlyZero())
		{
			TangentX = FVector::CrossProduct(FVector::ForwardVector, FaceNormal).GetSafeNormal();
		}
		if (TangentX.IsNearlyZero())
		{
			TangentX = FVector::RightVector;
		}
		const FProcMeshTangent FaceTangent(TangentX, false);

		const int32 BaseIndex = MeshData.SectionVertices.Num();
		MeshData.SectionVertices.Add(A);
		MeshData.SectionVertices.Add(B);
		MeshData.SectionVertices.Add(C);
		MeshData.SectionTriangles.Add(BaseIndex);
		MeshData.SectionTriangles.Add(BaseIndex + 1);
		MeshData.SectionTriangles.Add(BaseIndex + 2);
		MeshData.SectionNormals.Add(FaceNormal);
		MeshData.SectionNormals.Add(FaceNormal);
		MeshData.SectionNormals.Add(FaceNormal);
		const int32 UVIndex = TriangleNumber * 3;
		if (MeshData.TriangleUVs.IsValidIndex(UVIndex + 2))
		{
			MeshData.SectionUV0.Add(MeshData.TriangleUVs[UVIndex]);
			MeshData.SectionUV0.Add(MeshData.TriangleUVs[UVIndex + 1]);
			MeshData.SectionUV0.Add(MeshData.TriangleUVs[UVIndex + 2]);
		}
		else
		{
			MeshData.SectionUV0.Add(FVector2D::ZeroVector);
			MeshData.SectionUV0.Add(FVector2D::UnitVector);
			MeshData.SectionUV0.Add(FVector2D(1.0f, 0.0f));
		}
		MeshData.SectionTangents.Add(FaceTangent);
		MeshData.SectionTangents.Add(FaceTangent);
		MeshData.SectionTangents.Add(FaceTangent);
	}
}
}

AChineseSealPoolAnimator::AChineseSealPoolAnimator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickInterval = AnimationUpdateInterval;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	RootComponent = SceneRoot;
}

void AChineseSealPoolAnimator::BeginPlay()
{
	Super::BeginPlay();
	RunningTime = 0.0f;
	PrimaryActorTick.TickInterval = FMath::Max(0.0f, AnimationUpdateInterval);
	SetActorTickEnabled(true);
	LoadDefaultMeshesIfNeeded();
	RebuildSeals();
	UpdateSealMotion(RunningTime);
}

void AChineseSealPoolAnimator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Force procedural meshes to reload from JSON every time OnConstruction runs,
	// so that code changes (e.g. coordinate transforms) take effect without
	// needing to restart the editor.
	ProceduralMeshes.Reset();
	LoadDefaultMeshesIfNeeded();
	RebuildSeals();
	UpdateSealMotion(RunningTime);
}

void AChineseSealPoolAnimator::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bEnableAnimation)
	{
		return;
	}

	RunningTime += DeltaTime * AnimationSpeed;
	if (IsHidden())
	{
		return;
	}

	UpdateSealMotion(RunningTime);
}

bool AChineseSealPoolAnimator::ShouldTickIfViewportsOnly() const
{
	return bAnimateInEditor;
}

void AChineseSealPoolAnimator::LoadDefaultMeshesIfNeeded()
{
	if (!bUseImportedStaticMeshes)
	{
		return;
	}

	if (!SealMeshes.IsEmpty())
	{
		return;
	}

	static const TCHAR* DefaultMeshPaths[] = {
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Chuan.SM_Seal_Chuan"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Di.SM_Seal_Di"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Dian.SM_Seal_Dian"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Feng.SM_Seal_Feng"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Shui.SM_Seal_Shui"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Guang.SM_Seal_Guang"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Hai.SM_Seal_Hai"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Huo.SM_Seal_Huo"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Lei.SM_Seal_Lei"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Lin.SM_Seal_Lin"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Ri.SM_Seal_Ri"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Shan.SM_Seal_Shan"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Shi.SM_Seal_Shi"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Tian.SM_Seal_Tian"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Xing.SM_Seal_Xing"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Xue.SM_Seal_Xue"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Ying.SM_Seal_Ying"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Yu.SM_Seal_Yu"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Yue.SM_Seal_Yue"),
		TEXT("/Game/TransformationVFX/SM2SM/ChineseSeals/SM_Seal_Yun.SM_Seal_Yun"),
	};

	for (const TCHAR* MeshPath : DefaultMeshPaths)
	{
		FString PackageName = MeshPath;
		PackageName.Split(TEXT("."), &PackageName, nullptr);
		if (!FPackageName::DoesPackageExist(PackageName))
		{
			continue;
		}

		if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, MeshPath))
		{
			SealMeshes.Add(Mesh);
		}
	}
}

void AChineseSealPoolAnimator::ClearSealComponents()
{
	// 不依赖 Transient 数组（OnConstruction 重建时这俩数组是空的，旧组件不会被销毁→重叠）。
	// 直接销毁 SceneRoot 下所有印章网格组件，彻底清理。
	if (SceneRoot)
	{
		TArray<USceneComponent*> SealChildren;
		SceneRoot->GetChildrenComponents(true, SealChildren);
		for (USceneComponent* Child : SealChildren)
		{
			if (Child && (Child->IsA<UStaticMeshComponent>() || Child->IsA<UProceduralMeshComponent>()))
			{
				Child->DestroyComponent();
			}
		}
	}

	SealComponents.Empty();
	ProceduralSealComponents.Empty();
	MotionComponents.Empty();
	BaseRelativeLocations.Empty();
	Phases.Empty();
	SpeedMultipliers.Empty();
	AmplitudeMultipliers.Empty();
}

void AChineseSealPoolAnimator::RebuildSeals()
{
	ClearSealComponents();

	const bool bUseStaticMeshes = !SealMeshes.IsEmpty();
	const bool bUseProceduralMeshes = !bUseStaticMeshes && LoadProceduralMeshesIfNeeded();
	if (!bUseStaticMeshes && !bUseProceduralMeshes)
	{
		return;
	}

	const int32 SafeRows = FMath::Max(1, Rows);
	const int32 SafeColumns = FMath::Max(1, Columns);
	const float OffsetX = (SafeColumns - 1) * ColumnSpacing * 0.5f;
	const float OffsetY = (SafeRows - 1) * RowSpacing * 0.5f;

	FRandomStream Random(RandomSeed);
	UMaterialInterface* FallbackMaterial = ResolveSealMaterial();

	for (int32 Row = 0; Row < SafeRows; ++Row)
	{
		for (int32 Column = 0; Column < SafeColumns; ++Column)
		{
			const int32 TileIndex = Row * SafeColumns + Column;

			const FVector BaseLocation(
				Column * ColumnSpacing - OffsetX,
				Row * RowSpacing - OffsetY,
				HeightAboveWater);

			USceneComponent* MotionComponent = nullptr;
			// 用自动唯一名（NAME_None）。固定名 Seal_RR_CC 在程序网格<->静态网格切换时，
			// 旧同名组件 DestroyComponent 延迟未释放名字，新建同名会触发 UObject 名冲突 Fatal。
			const FName ComponentName = NAME_None;

			if (bUseStaticMeshes)
			{
				UStaticMesh* Mesh = SealMeshes[TileIndex % SealMeshes.Num()];
				if (!Mesh)
				{
					continue;
				}

				UStaticMeshComponent* SealComponent = NewObject<UStaticMeshComponent>(this, ComponentName);
				SealComponent->SetupAttachment(SceneRoot);
				SealComponent->SetStaticMesh(Mesh);
				SealComponent->SetMobility(EComponentMobility::Movable);
				SealComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				SealComponent->SetCastShadow(true);
				if (FallbackMaterial)
				{
					SealComponent->SetMaterial(0, FallbackMaterial);
					SealComponent->SetMaterial(1, FallbackMaterial);
				}
				SealComponent->SetRelativeLocation(BaseLocation);
				SealComponent->SetRelativeRotation(FRotator::ZeroRotator);
				SealComponent->SetRelativeScale3D(GetScaleForStaticMesh(Mesh));
				SealComponent->RegisterComponent();
				// 不调用 AddInstanceComponent：OnConstruction 动态组件一旦持久化到关卡，
				// 每次重建都会累积，导致印章成倍重叠。改为只 RegisterComponent，由
				// ClearSealComponents 在每次重建时清理。

				SealComponents.Add(SealComponent);
				MotionComponent = SealComponent;
			}
			else
			{
				const FChineseSealProceduralMesh& MeshData = ProceduralMeshes[TileIndex % ProceduralMeshes.Num()];
				UProceduralMeshComponent* SealComponent = NewObject<UProceduralMeshComponent>(this, ComponentName);
				SealComponent->SetupAttachment(SceneRoot);
				SealComponent->SetMobility(EComponentMobility::Movable);
				SealComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				SealComponent->SetCastShadow(true);
				SealComponent->CreateMeshSection(
					0,
					MeshData.SectionVertices,
					MeshData.SectionTriangles,
					MeshData.SectionNormals,
					MeshData.SectionUV0,
					TArray<FColor>(),
					MeshData.SectionTangents,
					false);
				if (FallbackMaterial)
				{
					SealComponent->SetMaterial(0, FallbackMaterial);
				}
				SealComponent->SetRelativeLocation(BaseLocation);
				SealComponent->SetRelativeRotation(FRotator::ZeroRotator);
				SealComponent->SetRelativeScale3D(GetScaleForProceduralMesh(MeshData));
				SealComponent->RegisterComponent();
				// 不调用 AddInstanceComponent：OnConstruction 动态组件一旦持久化到关卡，
				// 每次重建都会累积，导致印章成倍重叠。改为只 RegisterComponent，由
				// ClearSealComponents 在每次重建时清理。

				ProceduralSealComponents.Add(SealComponent);
				MotionComponent = SealComponent;
			}

			MotionComponents.Add(MotionComponent);
			BaseRelativeLocations.Add(BaseLocation);
			Phases.Add(Random.FRandRange(0.0f, UE_TWO_PI));
			SpeedMultipliers.Add(Random.FRandRange(0.74f, 1.34f));
			AmplitudeMultipliers.Add(Random.FRandRange(0.72f, 1.24f));
		}
	}
}

bool AChineseSealPoolAnimator::LoadProceduralMeshesIfNeeded()
{
	if (!ProceduralMeshes.IsEmpty())
	{
		return true;
	}

	const FString JsonPath = FPaths::ProjectDir() / TEXT("SourceArt/ChineseSeals/chinese_seals_meshes.sealmesh");
	FString JsonText;
	if (!FFileHelper::LoadFileToString(JsonText, *JsonPath))
	{
		return false;
	}

	TSharedPtr<FJsonObject> RootObject;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* MeshArray = nullptr;
	if (!RootObject->TryGetArrayField(TEXT("meshes"), MeshArray))
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& MeshValue : *MeshArray)
	{
		const TSharedPtr<FJsonObject>* MeshObjectPtr = nullptr;
		if (!MeshValue.IsValid() || !MeshValue->TryGetObject(MeshObjectPtr) || !MeshObjectPtr || !MeshObjectPtr->IsValid())
		{
			continue;
		}

		const TSharedPtr<FJsonObject>& MeshObject = *MeshObjectPtr;
		FChineseSealProceduralMesh MeshData;
		MeshObject->TryGetStringField(TEXT("name"), MeshData.Name);

		const TArray<TSharedPtr<FJsonValue>>* Vertices = nullptr;
		if (MeshObject->TryGetArrayField(TEXT("vertices"), Vertices))
		{
			for (const TSharedPtr<FJsonValue>& VertexValue : *Vertices)
			{
				const TArray<TSharedPtr<FJsonValue>>* Coords = nullptr;
				if (VertexValue.IsValid() && VertexValue->TryGetArray(Coords) && Coords && Coords->Num() >= 3)
				{
					MeshData.Vertices.Add(FVector(
						static_cast<float>((*Coords)[0]->AsNumber()),
						static_cast<float>((*Coords)[1]->AsNumber()),
						static_cast<float>((*Coords)[2]->AsNumber())));
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* Triangles = nullptr;
		if (MeshObject->TryGetArrayField(TEXT("triangles"), Triangles))
		{
			for (const TSharedPtr<FJsonValue>& IndexValue : *Triangles)
			{
				if (IndexValue.IsValid())
				{
					MeshData.Triangles.Add(static_cast<int32>(IndexValue->AsNumber()));
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* Normals = nullptr;
		if (MeshObject->TryGetArrayField(TEXT("normals"), Normals))
		{
			for (const TSharedPtr<FJsonValue>& NormalValue : *Normals)
			{
				const TArray<TSharedPtr<FJsonValue>>* Coords = nullptr;
				if (NormalValue.IsValid() && NormalValue->TryGetArray(Coords) && Coords && Coords->Num() >= 3)
				{
					MeshData.TriangleNormals.Add(FVector(
						static_cast<float>((*Coords)[0]->AsNumber()),
						static_cast<float>((*Coords)[1]->AsNumber()),
						static_cast<float>((*Coords)[2]->AsNumber())));
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* UVs = nullptr;
		if (MeshObject->TryGetArrayField(TEXT("uvs"), UVs))
		{
			for (const TSharedPtr<FJsonValue>& UVValue : *UVs)
			{
				const TArray<TSharedPtr<FJsonValue>>* Coords = nullptr;
				if (UVValue.IsValid() && UVValue->TryGetArray(Coords) && Coords && Coords->Num() >= 2)
				{
					MeshData.TriangleUVs.Add(FVector2D(
						static_cast<float>((*Coords)[0]->AsNumber()),
						static_cast<float>((*Coords)[1]->AsNumber())));
				}
			}
		}

		if (!MeshData.Vertices.IsEmpty() && MeshData.Triangles.Num() >= 3)
		{
			BuildHardNormalSection(MeshData);
			ProceduralMeshes.Add(MoveTemp(MeshData));
		}
	}

	return !ProceduralMeshes.IsEmpty();
}

UMaterialInterface* AChineseSealPoolAnimator::ResolveSealMaterial()
{
	// 选基材质：优先 OverrideMaterial（如 M_ChineseSeal_DefaultWhite），否则用引擎 BasicShapeMaterial。
	// 关键：即使有 OverrideMaterial，也包成动态实例后应用 DefaultSealColor 等参数，
	// 否则 Default Seal Color 调了不生效（之前直接 return OverrideMaterial 跳过了所有参数）。
	UMaterialInterface* BaseMaterial = OverrideMaterial.Get();
	if (!BaseMaterial)
	{
		BaseMaterial = LoadObject<UMaterialInterface>(
			nullptr, TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	}
	if (!BaseMaterial)
	{
		return nullptr;
	}

	// 基材质变了（例如改了 OverrideMaterial）就重建缓存的动态实例。
	if (!DefaultSealMaterial || DefaultSealMaterial->Parent != BaseMaterial)
	{
		DefaultSealMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
	}

	if (DefaultSealMaterial)
	{
		// 覆盖材质用参数名 BaseColor，BasicShapeMaterial 用 Color；两个都设，材质里有哪个就生效哪个。
		DefaultSealMaterial->SetVectorParameterValue(TEXT("BaseColor"), DefaultSealColor);
		DefaultSealMaterial->SetVectorParameterValue(TEXT("Color"), DefaultSealColor);
		DefaultSealMaterial->SetScalarParameterValue(TEXT("Metallic"), DefaultSealMetallic);
		DefaultSealMaterial->SetScalarParameterValue(TEXT("Roughness"), DefaultSealRoughness);
		DefaultSealMaterial->SetScalarParameterValue(TEXT("Specular"), DefaultSealSpecular);
	}

	return DefaultSealMaterial.Get();
}

FVector AChineseSealPoolAnimator::GetScaleForStaticMesh(UStaticMesh* Mesh) const
{
	if (!Mesh)
	{
		return FVector(SealScale);
	}

	const FBoxSphereBounds Bounds = Mesh->GetBounds();
	const FVector Extent = Bounds.BoxExtent;
	const float SectionLength = FMath::Max(Extent.X, Extent.Y) * 2.0f;
	const float SourceHeight = FMath::Max(Extent.Z * 2.0f, 0.01f);
	const float DesiredHeight = SectionLength * FMath::Max(SealHeightToSectionRatio, 0.01f);
	const float ZScale = SealScale * DesiredHeight / SourceHeight;
	return FVector(SealScale, SealScale, ZScale);
}

FVector AChineseSealPoolAnimator::GetScaleForProceduralMesh(const FChineseSealProceduralMesh& MeshData) const
{
	if (MeshData.Vertices.IsEmpty())
	{
		return FVector(SealScale);
	}

	FBox Bounds(ForceInit);
	for (const FVector& Vertex : MeshData.Vertices)
	{
		Bounds += Vertex;
	}

	const FVector Size = Bounds.GetSize();
	const float SectionLength = FMath::Max(Size.X, Size.Y);
	const float SourceHeight = FMath::Max(Size.Z, 0.01f);
	const float DesiredHeight = SectionLength * FMath::Max(SealHeightToSectionRatio, 0.01f);
	const float ZScale = SealScale * DesiredHeight / SourceHeight;
	return FVector(SealScale, SealScale, ZScale);
}

void AChineseSealPoolAnimator::UpdateSealMotion(float Time)
{
	const int32 Count = FMath::Min(MotionComponents.Num(), BaseRelativeLocations.Num());
	for (int32 Index = 0; Index < Count; ++Index)
	{
		USceneComponent* MotionComponent = MotionComponents[Index];
		if (!MotionComponent)
		{
			continue;
		}

		const FVector BaseLocation = BaseRelativeLocations[Index];
		const float Phase = Phases.IsValidIndex(Index) ? Phases[Index] : 0.0f;
		const float SpeedMul = SpeedMultipliers.IsValidIndex(Index) ? SpeedMultipliers[Index] : 1.0f;
		const float AmpMul = AmplitudeMultipliers.IsValidIndex(Index) ? AmplitudeMultipliers[Index] : 1.0f;
		const float LocalTime = Time * AnimationFrequency * SpeedMul;

		const float SineA = FMath::Sin(LocalTime + Phase);
		const float SineB = FMath::Sin(LocalTime * 0.43f + Phase * 1.71f + BaseLocation.X * 0.018f);
		const float SineC = FMath::Cos(LocalTime * 0.29f - Phase * 0.63f + BaseLocation.Y * 0.014f);
		const float Noise = FMath::PerlinNoise2D(FVector2D(
			BaseLocation.X * 0.01f * NoiseScale + LocalTime * 0.09f,
			BaseLocation.Y * 0.01f * NoiseScale - LocalTime * 0.07f));

		const float RegularWave = SineA;
		const float IrregularWave = SineA * 0.45f + SineB * 0.25f + SineC * 0.18f + Noise * 0.35f;
		const float BlendedWave = FMath::Lerp(RegularWave, IrregularWave, FMath::Clamp(Irregularity, 0.0f, 1.0f));

		FVector NewLocation = BaseLocation;
		NewLocation.Z = FMath::Max(0.5f, HeightAboveWater + BlendedWave * AnimationAmplitude * AmpMul);
		MotionComponent->SetRelativeLocation(NewLocation);
	}
}
