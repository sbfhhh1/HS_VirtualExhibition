#include "BlenderGNParasiteMotionComponent.h"

#include "BlenderGeometryNodesComponent.h"
#include "BlenderGeometryNodesTypes.h"
#include "BlenderGNProceduralMeshUtils.h"
#include "ProceduralMeshComponent.h"

UBlenderGNParasiteMotionComponent::UBlenderGNParasiteMotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBlenderGNParasiteMotionComponent::SetLiveMotion(bool bEnabled)
{
	bLiveMotion = bEnabled;
	SetComponentTickEnabled(bLiveMotion || PreviewGooSize != 1.0f || PreviewCageRadius != 1.0f || PreviewWireRadius != 1.0f || PreviewGooNoiseScale != 1.0f);
}

void UBlenderGNParasiteMotionComponent::SetGeometryPreview(const FString& SocketName, float Value)
{
	if (SocketName.Equals(TEXT("Goo Size"), ESearchCase::IgnoreCase))
	{
		PreviewGooSize = FMath::Clamp(Value / FMath::Max(0.0001f, BaseGooSize), 0.1f, 4.0f);
	}
	else if (SocketName.Equals(TEXT("Cage Radius"), ESearchCase::IgnoreCase))
	{
		PreviewCageRadius = FMath::Clamp(Value / FMath::Max(0.0001f, BaseCageRadius), 0.25f, 4.0f);
	}
	else if (SocketName.Equals(TEXT("Wire Radius"), ESearchCase::IgnoreCase))
	{
		PreviewWireRadius = FMath::Clamp(Value / FMath::Max(0.0001f, BaseWireRadius), 0.2f, 5.0f);
	}
	else if (SocketName.Equals(TEXT("Goo Noise Scale"), ESearchCase::IgnoreCase))
	{
		PreviewGooNoiseScale = FMath::Clamp(Value / FMath::Max(0.0001f, BaseGooNoiseScale), 0.2f, 5.0f);
	}
	else if (SocketName.Equals(TEXT("Goo Threshold"), ESearchCase::IgnoreCase))
	{
		GooAmplitude = FMath::Clamp(FMath::Lerp(3.5f, 8.5f, FMath::GetMappedRangeValueClamped(FVector2D(0.42f, 0.16f), FVector2D(0.0f, 1.0f), Value)), 0.0f, 18.0f);
	}
	else if (SocketName.Equals(TEXT("Goo Contrast"), ESearchCase::IgnoreCase))
	{
		Smoothness = FMath::Clamp(FMath::Lerp(0.98f, 0.82f, Value), 0.0f, 1.0f);
		RebuildPhases();
	}

	ApplyCurrentFrame(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f, true);
	SetComponentTickEnabled(true);
}

void UBlenderGNParasiteMotionComponent::OnRegister()
{
	Super::OnRegister();
	BindRuntime();
}

void UBlenderGNParasiteMotionComponent::OnUnregister()
{
	UnbindRuntime();
	Super::OnUnregister();
}

void UBlenderGNParasiteMotionComponent::BeginPlay()
{
	Super::BeginPlay();
	BindRuntime();
	RebuildCache();
	SetComponentTickEnabled(bLiveMotion);
}

void UBlenderGNParasiteMotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	ApplyCurrentFrame(GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f, false);
}

void UBlenderGNParasiteMotionComponent::HandleMeshUpdated(UProceduralMeshComponent* ProceduralMesh)
{
	CapturePreviewBaselines();
	ResetGeometryPreview();
	RebuildCache();
}

void UBlenderGNParasiteMotionComponent::BindRuntime()
{
	UBlenderGeometryNodesComponent* Resolved = BlenderGNProcMesh::ResolveRuntime(GetOwner(), Runtime);
	if (Resolved == BoundRuntime)
	{
		return;
	}
	UnbindRuntime();
	BoundRuntime = Resolved;
	if (BoundRuntime)
	{
		BoundRuntime->OnMeshUpdated.AddUniqueDynamic(this, &UBlenderGNParasiteMotionComponent::HandleMeshUpdated);
	}
}

void UBlenderGNParasiteMotionComponent::UnbindRuntime()
{
	if (BoundRuntime)
	{
		BoundRuntime->OnMeshUpdated.RemoveDynamic(this, &UBlenderGNParasiteMotionComponent::HandleMeshUpdated);
		BoundRuntime = nullptr;
	}
}

void UBlenderGNParasiteMotionComponent::CapturePreviewBaselines()
{
	if (!BoundRuntime)
	{
		return;
	}

	BaseGooSize = FMath::Max(0.0001f, BoundRuntime->GetFloatInput(TEXT("Goo Size"), BaseGooSize));
	BaseCageRadius = FMath::Max(0.0001f, BoundRuntime->GetFloatInput(TEXT("Cage Radius"), BaseCageRadius));
	BaseWireRadius = FMath::Max(0.0001f, BoundRuntime->GetFloatInput(TEXT("Wire Radius"), BaseWireRadius));
	BaseGooNoiseScale = FMath::Max(0.0001f, BoundRuntime->GetFloatInput(TEXT("Goo Noise Scale"), BaseGooNoiseScale));
}

void UBlenderGNParasiteMotionComponent::ResetGeometryPreview()
{
	PreviewGooSize = 1.0f;
	PreviewCageRadius = 1.0f;
	PreviewWireRadius = 1.0f;
	PreviewGooNoiseScale = 1.0f;
}

void UBlenderGNParasiteMotionComponent::RebuildCache()
{
	BindRuntime();
	if (!BoundRuntime)
	{
		return;
	}

	const FBlenderGNMeshData& MeshData = BoundRuntime->GetLastMeshData();
	BaseVertices = MeshData.Vertices;
	BaseNormals = MeshData.Normals;
	if (BaseNormals.Num() != BaseVertices.Num())
	{
		BlenderGNProcMesh::RecalculateNormals(MeshData, BaseVertices, BaseNormals);
	}

	CachedVertexCount = BaseVertices.Num();
	AnimatedVertices.SetNum(CachedVertexCount);
	GooVertices.Init(false, CachedVertexCount);
	MarkGooVertices();
	RebuildPhases();
}

void UBlenderGNParasiteMotionComponent::MarkGooVertices()
{
	if (!BoundRuntime)
	{
		return;
	}

	const FBlenderGNMeshData& MeshData = BoundRuntime->GetLastMeshData();
	const int32 SectionIndex = MeshData.Sections.Num() > 0 ? FMath::Clamp(GooSubmeshIndex, 0, MeshData.Sections.Num() - 1) : 0;
	if (!MeshData.Sections.IsValidIndex(SectionIndex))
	{
		return;
	}

	for (int32 VertexIndex : MeshData.Sections[SectionIndex].Triangles)
	{
		if (GooVertices.IsValidIndex(VertexIndex))
		{
			GooVertices[VertexIndex] = true;
		}
	}
}

void UBlenderGNParasiteMotionComponent::RebuildPhases()
{
	Phases.SetNum(BaseVertices.Num());
	const float ClampedSmoothness = FMath::Clamp(Smoothness, 0.0f, 1.0f);
	for (int32 Index = 0; Index < BaseVertices.Num(); ++Index)
	{
		const FVector V = BaseVertices[Index];
		const float Radial = V.Length() * NoiseScale;
		const float Coherent = (FMath::PerlinNoise2D(FVector2D(V.X * NoiseScale + 19.7f, V.Z * NoiseScale - 7.3f)) + 1.0f) * 0.5f;
		const float Vertical = (FMath::PerlinNoise2D(FVector2D(V.Y * NoiseScale + 3.1f, V.X * NoiseScale + 11.9f)) + 1.0f) * 0.5f;
		const float RandomBlend = GooVertices.IsValidIndex(Index) && GooVertices[Index] ? FMath::Lerp(0.08f, 0.0f, ClampedSmoothness) : 0.18f;
		Phases[Index] = Radial + (Coherent * 0.7f + Vertical * 0.3f) * PI * 2.0f + BlenderGNProcMesh::Hash01(Index * 97 + 13) * PI * 2.0f * RandomBlend;
	}
}

void UBlenderGNParasiteMotionComponent::ApplyCurrentFrame(float WorldTimeSeconds, bool bForce)
{
	BindRuntime();
	if (!BoundRuntime)
	{
		return;
	}

	if (BoundRuntime->GetLastMeshData().Vertices.Num() != CachedVertexCount || BaseVertices.Num() == 0)
	{
		RebuildCache();
	}
	if (BaseVertices.Num() == 0 || BaseNormals.Num() != BaseVertices.Num() || GooVertices.Num() != BaseVertices.Num())
	{
		return;
	}

	const bool bHasPreview =
		!FMath::IsNearlyEqual(PreviewGooSize, 1.0f) ||
		!FMath::IsNearlyEqual(PreviewCageRadius, 1.0f) ||
		!FMath::IsNearlyEqual(PreviewWireRadius, 1.0f) ||
		!FMath::IsNearlyEqual(PreviewGooNoiseScale, 1.0f);
	if (!bForce && !bLiveMotion && !bHasPreview)
	{
		SetComponentTickEnabled(false);
		return;
	}

	const float T = WorldTimeSeconds * PulseSpeed;
	const float ClampedSmoothness = FMath::Clamp(Smoothness, 0.0f, 1.0f);
	const float SmoothBlend = FMath::Lerp(1.0f, 0.45f, ClampedSmoothness);
	for (int32 Index = 0; Index < BaseVertices.Num(); ++Index)
	{
		const bool bIsGoo = GooVertices[Index];
		FVector Vertex = BaseVertices[Index];
		const FVector FallbackNormal = Vertex.IsNearlyZero() ? FVector::UpVector : Vertex.GetSafeNormal();
		const FVector Normal = BaseNormals[Index].IsNearlyZero() ? FallbackNormal : BaseNormals[Index].GetSafeNormal();
		if (bIsGoo)
		{
			Vertex *= PreviewGooSize;
		}
		else
		{
			Vertex *= PreviewCageRadius;
			Vertex += Normal * ((PreviewWireRadius - 1.0f) * 1.8f);
		}

		const float Phase = Phases[Index] * (bIsGoo ? PreviewGooNoiseScale : 1.0f);
		const float Radial = FMath::Sin(T + Phase);
		const float Secondary = FMath::Sin(T * 0.57f + Phase * 1.73f);
		const float Wave = FMath::Lerp(Radial, FMath::SmoothStep(-1.0f, 1.0f, Radial), ClampedSmoothness);
		const float Offset = (Wave * 0.82f + Secondary * 0.18f) * (bIsGoo ? GooAmplitude : CageAmplitude) * SmoothBlend;
		AnimatedVertices[Index] = Vertex + Normal * Offset;
	}

	TArray<FVector> Normals;
	if (bRecalculateAnimatedNormals)
	{
		BlenderGNProcMesh::RecalculateNormals(BoundRuntime->GetLastMeshData(), AnimatedVertices, Normals);
	}
	BlenderGNProcMesh::ApplyVertices(BoundRuntime, AnimatedVertices, Normals);
}
