#include "BlenderGNIslandBreathMotionComponent.h"

#include "BlenderGeometryNodesComponent.h"
#include "BlenderGeometryNodesTypes.h"
#include "BlenderGNProceduralMeshUtils.h"
#include "ProceduralMeshComponent.h"

UBlenderGNIslandBreathMotionComponent::UBlenderGNIslandBreathMotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	UpdateTickInterval();
}

void UBlenderGNIslandBreathMotionComponent::SetLiveMotion(bool bEnabled)
{
	bLiveMotion = bEnabled;
	UpdateTickInterval();
	SetComponentTickEnabled(bLiveMotion);
}

void UBlenderGNIslandBreathMotionComponent::OnRegister()
{
	Super::OnRegister();
	BindRuntime();
}

void UBlenderGNIslandBreathMotionComponent::OnUnregister()
{
	UnbindRuntime();
	Super::OnUnregister();
}

void UBlenderGNIslandBreathMotionComponent::BeginPlay()
{
	Super::BeginPlay();
	BindRuntime();
	RebuildCache();
	UpdateTickInterval();
	SetComponentTickEnabled(bLiveMotion);
}

void UBlenderGNIslandBreathMotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!bLiveMotion)
	{
		return;
	}

	const float WorldTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const double UpdateInterval = 1.0 / FMath::Max(1.0f, MaxAnimationFrameRate);
	if (WorldTimeSeconds - LastMotionUpdateTime < UpdateInterval)
	{
		return;
	}

	LastMotionUpdateTime = WorldTimeSeconds;
	ApplyMotion(WorldTimeSeconds);
}

void UBlenderGNIslandBreathMotionComponent::HandleMeshUpdated(UProceduralMeshComponent* ProceduralMesh)
{
	RebuildCache();
}

void UBlenderGNIslandBreathMotionComponent::BindRuntime()
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
		BoundRuntime->OnMeshUpdated.AddUniqueDynamic(this, &UBlenderGNIslandBreathMotionComponent::HandleMeshUpdated);
	}
}

void UBlenderGNIslandBreathMotionComponent::UnbindRuntime()
{
	if (BoundRuntime)
	{
		BoundRuntime->OnMeshUpdated.RemoveDynamic(this, &UBlenderGNIslandBreathMotionComponent::HandleMeshUpdated);
		BoundRuntime = nullptr;
	}
}

void UBlenderGNIslandBreathMotionComponent::RebuildCache()
{
	BindRuntime();
	if (!BoundRuntime)
	{
		return;
	}

	BaseVertices = BoundRuntime->GetLastMeshData().Vertices;
	CachedVertexCount = BaseVertices.Num();
	AnimatedVertices.SetNumUninitialized(CachedVertexCount);
	AnimatedNormals.Reset();
	if (bRecalculateAnimatedNormals)
	{
		AnimatedNormals.Reserve(CachedVertexCount);
	}
	BuildIslands();
	IslandOffsets.SetNumUninitialized(IslandNormals.Num());
}

void UBlenderGNIslandBreathMotionComponent::BuildIslands()
{
	IslandNormals.Reset();
	IslandPhases.Reset();
	VertexIsland.Init(INDEX_NONE, BaseVertices.Num());
	if (!BoundRuntime || BaseVertices.Num() == 0)
	{
		return;
	}

	const FBlenderGNMeshData& MeshData = BoundRuntime->GetLastMeshData();
	BlenderGNProcMesh::FIslandMotionCache Cache;
	const int32 HexSubdivisions = BoundRuntime->GetIntInput(TEXT("Hex Subdivisions"), 2);
	const float RuntimeNoiseScale = BoundRuntime->GetFloatInput(TEXT("Breath Noise Scale"), NoiseScale);
	if (BlenderGNProcMesh::BuildHexIslandMotionCache(MeshData, HexSubdivisions, RuntimeNoiseScale, Cache))
	{
		VertexIsland = MoveTemp(Cache.VertexIsland);
		IslandNormals = MoveTemp(Cache.IslandNormals);
		IslandPhases = MoveTemp(Cache.IslandPhases);
		return;
	}

	// Fallback for non-hex meshes: standard connected components on explicit triangle indices.
	TArray<TArray<int32>> Adjacency;
	Adjacency.SetNum(BaseVertices.Num());
	for (const FBlenderGNMeshSection& Section : MeshData.Sections)
	{
		for (int32 Index = 0; Index + 2 < Section.Triangles.Num(); Index += 3)
		{
			const int32 A = Section.Triangles[Index];
			const int32 B = Section.Triangles[Index + 1];
			const int32 C = Section.Triangles[Index + 2];
			if (!Adjacency.IsValidIndex(A) || !Adjacency.IsValidIndex(B) || !Adjacency.IsValidIndex(C))
			{
				continue;
			}
			Adjacency[A].AddUnique(B);
			Adjacency[B].AddUnique(A);
			Adjacency[B].AddUnique(C);
			Adjacency[C].AddUnique(B);
			Adjacency[C].AddUnique(A);
			Adjacency[A].AddUnique(C);
		}
	}

	TQueue<int32> Queue;
	for (int32 Start = 0; Start < BaseVertices.Num(); ++Start)
	{
		if (VertexIsland[Start] != INDEX_NONE)
		{
			continue;
		}

		const int32 IslandId = IslandNormals.Num();
		FVector Center = FVector::ZeroVector;
		int32 Count = 0;
		VertexIsland[Start] = IslandId;
		Queue.Enqueue(Start);
		int32 Current = INDEX_NONE;
		while (Queue.Dequeue(Current))
		{
			Center += BaseVertices[Current];
			++Count;
			for (int32 Next : Adjacency[Current])
			{
				if (VertexIsland.IsValidIndex(Next) && VertexIsland[Next] == INDEX_NONE)
				{
					VertexIsland[Next] = IslandId;
					Queue.Enqueue(Next);
				}
			}
		}

		Center /= FMath::Max(1, Count);
		IslandNormals.Add(Center.IsNearlyZero() ? FVector::UpVector : Center.GetSafeNormal());
		IslandPhases.Add(BlenderGNProcMesh::Hash01((IslandId + 1) * 37) * PI * 2.0f * NoiseScale);
	}
}

void UBlenderGNIslandBreathMotionComponent::UpdateTickInterval()
{
	PrimaryComponentTick.TickInterval = 1.0f / FMath::Max(1.0f, MaxAnimationFrameRate);
}

void UBlenderGNIslandBreathMotionComponent::ApplyMotion(float WorldTimeSeconds)
{
	BindRuntime();
	if (!BoundRuntime)
	{
		return;
	}
	if (BoundRuntime->bEnablePreviewAnimation)
	{
		SetComponentTickEnabled(false);
		return;
	}

	if (BoundRuntime->GetLastMeshData().Vertices.Num() != CachedVertexCount || BaseVertices.Num() == 0)
	{
		RebuildCache();
	}
	if (BaseVertices.Num() == 0 || VertexIsland.Num() != BaseVertices.Num())
	{
		return;
	}
	if (IslandOffsets.Num() != IslandNormals.Num())
	{
		IslandOffsets.SetNumUninitialized(IslandNormals.Num());
	}

	const float T = WorldTimeSeconds * FMath::Max(0.0f, Speed);
	const float Blend = FMath::Lerp(1.0f, 0.35f, FMath::Clamp(Smoothness, 0.0f, 1.0f));
	for (int32 Island = 0; Island < IslandNormals.Num(); ++Island)
	{
		const float Wave = FMath::Sin(T + IslandPhases[Island]);
		const float Shaped = FMath::Lerp(Wave, FMath::SmoothStep(-1.0f, 1.0f, Wave), FMath::Clamp(Smoothness, 0.0f, 1.0f));
		IslandOffsets[Island] = IslandNormals[Island] * (Shaped * Amplitude * Blend);
	}

	for (int32 Index = 0; Index < BaseVertices.Num(); ++Index)
	{
		const int32 Island = VertexIsland[Index];
		if (!IslandOffsets.IsValidIndex(Island))
		{
			AnimatedVertices[Index] = BaseVertices[Index];
			continue;
		}
		AnimatedVertices[Index] = BaseVertices[Index] + IslandOffsets[Island];
	}

	if (bRecalculateAnimatedNormals)
	{
		BlenderGNProcMesh::RecalculateNormals(BoundRuntime->GetLastMeshData(), AnimatedVertices, AnimatedNormals);
		BlenderGNProcMesh::ApplyVertices(BoundRuntime, AnimatedVertices, AnimatedNormals, ScratchVertexColors, ScratchTangents);
	}
	else
	{
		BlenderGNProcMesh::ApplyVertices(BoundRuntime, AnimatedVertices, AnimatedNormals, ScratchVertexColors, ScratchTangents);
	}
}
