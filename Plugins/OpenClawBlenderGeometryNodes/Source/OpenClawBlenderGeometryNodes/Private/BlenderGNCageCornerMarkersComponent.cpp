#include "BlenderGNCageCornerMarkersComponent.h"

#include "BlenderGeometryNodesComponent.h"
#include "BlenderGeometryNodesTypes.h"
#include "BlenderGNProceduralMeshUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "ProceduralMeshComponent.h"

namespace
{
	struct FCornerCluster
	{
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;

		explicit FCornerCluster(const FVector& Point)
			: Sum(Point), Count(1)
		{
		}

		void Add(const FVector& Point)
		{
			Sum += Point;
			++Count;
		}

		FVector Center() const
		{
			return Sum / FMath::Max(1, Count);
		}

		FVector Direction() const
		{
			const FVector ClusterCenter = Center();
			return ClusterCenter.IsNearlyZero() ? FVector::UpVector : ClusterCenter.GetSafeNormal();
		}
	};
}

UBlenderGNCageCornerMarkersComponent::UBlenderGNCageCornerMarkersComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBlenderGNCageCornerMarkersComponent::SetMarkerRadius(float Radius)
{
	MarkerRadius = FMath::Clamp(Radius, 2.0f, 18.0f);
	ApplyMarkerScale();
	RebuildMarkers();
}

void UBlenderGNCageCornerMarkersComponent::OnRegister()
{
	Super::OnRegister();
	BindRuntime();
}

void UBlenderGNCageCornerMarkersComponent::OnUnregister()
{
	UnbindRuntime();
	Super::OnUnregister();
}

void UBlenderGNCageCornerMarkersComponent::BeginPlay()
{
	Super::BeginPlay();
	BindRuntime();
	RebuildMarkers();
}

void UBlenderGNCageCornerMarkersComponent::HandleMeshUpdated(UProceduralMeshComponent* ProceduralMesh)
{
	RebuildMarkers();
}

void UBlenderGNCageCornerMarkersComponent::BindRuntime()
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
		BoundRuntime->OnMeshUpdated.AddUniqueDynamic(this, &UBlenderGNCageCornerMarkersComponent::HandleMeshUpdated);
	}
}

void UBlenderGNCageCornerMarkersComponent::UnbindRuntime()
{
	if (BoundRuntime)
	{
		BoundRuntime->OnMeshUpdated.RemoveDynamic(this, &UBlenderGNCageCornerMarkersComponent::HandleMeshUpdated);
		BoundRuntime = nullptr;
	}
}

void UBlenderGNCageCornerMarkersComponent::RebuildMarkers()
{
	BindRuntime();
	if (!BoundRuntime)
	{
		return;
	}

	const FBlenderGNMeshData& MeshData = BoundRuntime->GetLastMeshData();
	if (MeshData.Vertices.Num() == 0 || MeshData.Sections.Num() == 0)
	{
		ApplyMarkerScale();
		return;
	}

	const int32 SectionIndex = FMath::Clamp(CageSubmeshIndex, 0, MeshData.Sections.Num() - 1);
	const TArray<int32>& Triangles = MeshData.Sections[SectionIndex].Triangles;
	float MaxRadius = 0.0f;
	for (int32 VertexIndex : Triangles)
	{
		if (MeshData.Vertices.IsValidIndex(VertexIndex))
		{
			MaxRadius = FMath::Max(MaxRadius, static_cast<float>(MeshData.Vertices[VertexIndex].Length()));
		}
	}

	const float MinRadius = MaxRadius * SurfaceRadiusThreshold;
	TArray<FCornerCluster> Clusters;
	for (int32 VertexIndex : Triangles)
	{
		if (!MeshData.Vertices.IsValidIndex(VertexIndex))
		{
			continue;
		}

		const FVector Vertex = MeshData.Vertices[VertexIndex];
		if (Vertex.Length() < MinRadius)
		{
			continue;
		}

		const FVector Direction = Vertex.IsNearlyZero() ? FVector::UpVector : Vertex.GetSafeNormal();
		int32 BestIndex = INDEX_NONE;
		float BestDot = DirectionMergeDot;
		for (int32 ClusterIndex = 0; ClusterIndex < Clusters.Num(); ++ClusterIndex)
		{
			const float Dot = FVector::DotProduct(Direction, Clusters[ClusterIndex].Direction());
			if (Dot > BestDot)
			{
				BestDot = Dot;
				BestIndex = ClusterIndex;
			}
		}

		if (BestIndex == INDEX_NONE)
		{
			Clusters.Emplace(Vertex);
		}
		else
		{
			Clusters[BestIndex].Add(Vertex);
		}
	}

	EnsureMarkerCount(Clusters.Num());
	for (int32 Index = 0; Index < Markers.Num(); ++Index)
	{
		UStaticMeshComponent* Marker = Markers[Index];
		if (!Marker)
		{
			continue;
		}
		const bool bActive = Index < Clusters.Num();
		Marker->SetVisibility(bActive);
		Marker->SetHiddenInGame(!bActive);
		if (bActive)
		{
			Marker->SetRelativeLocation(Clusters[Index].Center());
			Marker->SetRelativeRotation(FRotator::ZeroRotator);
		}
	}
	ApplyMarkerScale();
}

void UBlenderGNCageCornerMarkersComponent::EnsureMarkerCount(int32 Count)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!MarkerMesh)
	{
		MarkerMesh = Cast<UStaticMesh>(StaticLoadObject(UStaticMesh::StaticClass(), nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")));
	}

	while (Markers.Num() < Count)
	{
		const FName ComponentName = MakeUniqueObjectName(Owner, UStaticMeshComponent::StaticClass(), TEXT("Cage_Corner_Marker"));
		UStaticMeshComponent* Marker = NewObject<UStaticMeshComponent>(Owner, ComponentName);
		Marker->SetMobility(EComponentMobility::Movable);
		Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Marker->SetStaticMesh(MarkerMesh);
		if (MarkerMaterial)
		{
			Marker->SetMaterial(0, MarkerMaterial);
		}
		Marker->SetupAttachment(Owner->GetRootComponent());
		Marker->RegisterComponent();
		Owner->AddInstanceComponent(Marker);
		Markers.Add(Marker);
	}
}

void UBlenderGNCageCornerMarkersComponent::ApplyMarkerScale()
{
	const FVector Scale(MarkerRadius * 0.02f);
	for (UStaticMeshComponent* Marker : Markers)
	{
		if (Marker)
		{
			if (MarkerMaterial)
			{
				Marker->SetMaterial(0, MarkerMaterial);
			}
			Marker->SetRelativeScale3D(Scale);
		}
	}
}
