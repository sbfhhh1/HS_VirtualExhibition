#include "BlenderGNSubmeshSmootherComponent.h"

#include "BlenderGeometryNodesComponent.h"
#include "BlenderGeometryNodesTypes.h"
#include "BlenderGNProceduralMeshUtils.h"
#include "ProceduralMeshComponent.h"

namespace
{
	FIntVector Quantize(const FVector& V, float Quantization)
	{
		return FIntVector(
			FMath::RoundToInt(V.X * Quantization),
			FMath::RoundToInt(V.Y * Quantization),
			FMath::RoundToInt(V.Z * Quantization));
	}

	void AddSubmeshSmoothingEdge(TArray<TArray<int32>>& Adjacency, int32 A, int32 B)
	{
		if (Adjacency.IsValidIndex(A) && Adjacency.IsValidIndex(B))
		{
			Adjacency[A].AddUnique(B);
			Adjacency[B].AddUnique(A);
		}
	}

	void SmoothPass(const TArray<FVector>& Source, TArray<FVector>& Target, const TArray<bool>& Selected, const TArray<TArray<int32>>& Adjacency, float PassStrength)
	{
		Target = Source;
		for (int32 Index = 0; Index < Source.Num(); ++Index)
		{
			if (!Selected.IsValidIndex(Index) || !Selected[Index] || !Adjacency.IsValidIndex(Index) || Adjacency[Index].Num() == 0)
			{
				continue;
			}

			FVector Average = FVector::ZeroVector;
			for (int32 Neighbor : Adjacency[Index])
			{
				Average += Source[Neighbor];
			}
			Average /= Adjacency[Index].Num();
			Target[Index] = Source[Index] + (Average - Source[Index]) * PassStrength;
		}
	}
}

UBlenderGNSubmeshSmootherComponent::UBlenderGNSubmeshSmootherComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBlenderGNSubmeshSmootherComponent::OnRegister()
{
	Super::OnRegister();
	BindRuntime();
}

void UBlenderGNSubmeshSmootherComponent::OnUnregister()
{
	UnbindRuntime();
	Super::OnUnregister();
}

void UBlenderGNSubmeshSmootherComponent::BeginPlay()
{
	Super::BeginPlay();
	BindRuntime();
}

void UBlenderGNSubmeshSmootherComponent::HandleMeshUpdated(UProceduralMeshComponent* ProceduralMesh)
{
	if (!bApplyingSmoothedMesh)
	{
		SmoothCurrentMesh();
	}
}

void UBlenderGNSubmeshSmootherComponent::BindRuntime()
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
		BoundRuntime->OnMeshUpdated.AddUniqueDynamic(this, &UBlenderGNSubmeshSmootherComponent::HandleMeshUpdated);
	}
}

void UBlenderGNSubmeshSmootherComponent::UnbindRuntime()
{
	if (BoundRuntime)
	{
		BoundRuntime->OnMeshUpdated.RemoveDynamic(this, &UBlenderGNSubmeshSmootherComponent::HandleMeshUpdated);
		BoundRuntime = nullptr;
	}
}

void UBlenderGNSubmeshSmootherComponent::SmoothCurrentMesh()
{
	BindRuntime();
	if (!BoundRuntime || Iterations <= 0 || Strength <= 0.0f)
	{
		return;
	}

	FBlenderGNMeshData MeshData = BoundRuntime->GetLastMeshData();
	if (MeshData.Vertices.Num() == 0 || MeshData.Sections.Num() == 0)
	{
		return;
	}

	const int32 SectionIndex = FMath::Clamp(SubmeshIndex, 0, MeshData.Sections.Num() - 1);
	const TArray<int32>& Triangles = MeshData.Sections[SectionIndex].Triangles;
	if (Triangles.Num() == 0)
	{
		return;
	}

	TMap<FIntVector, TArray<int32>> WeldGroups;
	for (int32 VertexIndex : Triangles)
	{
		if (MeshData.Vertices.IsValidIndex(VertexIndex))
		{
			WeldGroups.FindOrAdd(Quantize(MeshData.Vertices[VertexIndex], FMath::Max(1.0f, WeldQuantization))).Add(VertexIndex);
		}
	}
	for (const TPair<FIntVector, TArray<int32>>& Pair : WeldGroups)
	{
		FVector Average = FVector::ZeroVector;
		for (int32 VertexIndex : Pair.Value)
		{
			Average += MeshData.Vertices[VertexIndex];
		}
		Average /= FMath::Max(1, Pair.Value.Num());
		for (int32 VertexIndex : Pair.Value)
		{
			MeshData.Vertices[VertexIndex] = Average;
		}
	}

	TArray<bool> Selected;
	Selected.Init(false, MeshData.Vertices.Num());
	TArray<TArray<int32>> Adjacency;
	Adjacency.SetNum(MeshData.Vertices.Num());
	for (int32 Index = 0; Index + 2 < Triangles.Num(); Index += 3)
	{
		const int32 A = Triangles[Index];
		const int32 B = Triangles[Index + 1];
		const int32 C = Triangles[Index + 2];
		if (!Selected.IsValidIndex(A) || !Selected.IsValidIndex(B) || !Selected.IsValidIndex(C))
		{
			continue;
		}
		Selected[A] = Selected[B] = Selected[C] = true;
		AddSubmeshSmoothingEdge(Adjacency, A, B);
		AddSubmeshSmoothingEdge(Adjacency, B, C);
		AddSubmeshSmoothingEdge(Adjacency, C, A);
	}

	TArray<FVector> Source = MeshData.Vertices;
	TArray<FVector> Target;
	for (int32 Pass = 0; Pass < Iterations; ++Pass)
	{
		SmoothPass(Source, Target, Selected, Adjacency, Strength);
		SmoothPass(Target, Source, Selected, Adjacency, InflateStrength);
	}
	MeshData.Vertices = MoveTemp(Source);
	BlenderGNProcMesh::RecalculateNormals(MeshData, MeshData.Vertices, MeshData.Normals);

	TGuardValue<bool> Guard(bApplyingSmoothedMesh, true);
	BoundRuntime->ApplyMeshData(MeshData);
}
