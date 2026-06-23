#include "BlenderGNProceduralMeshUtils.h"

#include "BlenderGeometryNodesComponent.h"
#include "BlenderGeometryNodesTypes.h"
#include "ProceduralMeshComponent.h"

namespace
{
	FIntVector QuantizePosition(const FVector& Value, float Quantization)
	{
		const float SafeQuantization = FMath::Max(1.0f, Quantization);
		return FIntVector(
			FMath::RoundToInt(Value.X * SafeQuantization),
			FMath::RoundToInt(Value.Y * SafeQuantization),
			FMath::RoundToInt(Value.Z * SafeQuantization));
	}

	FIntVector QuantizeProceduralMeshCell(const FVector& Value, float CellSize)
	{
		const float SafeCellSize = FMath::Max(0.001f, CellSize);
		return FIntVector(
			FMath::FloorToInt(Value.X / SafeCellSize),
			FMath::FloorToInt(Value.Y / SafeCellSize),
			FMath::FloorToInt(Value.Z / SafeCellSize));
	}

	struct FTriangleUnionFind
	{
		TArray<int32> Parent;
		TArray<int32> Rank;

		explicit FTriangleUnionFind(int32 Count)
		{
			Parent.SetNumUninitialized(Count);
			Rank.Init(0, Count);
			for (int32 Index = 0; Index < Count; ++Index)
			{
				Parent[Index] = Index;
			}
		}

		int32 Find(int32 Value)
		{
			while (Parent.IsValidIndex(Value) && Parent[Value] != Value)
			{
				Parent[Value] = Parent[Parent[Value]];
				Value = Parent[Value];
			}
			return Value;
		}

		void Union(int32 A, int32 B)
		{
			int32 RootA = Find(A);
			int32 RootB = Find(B);
			if (RootA == RootB)
			{
				return;
			}
			if (Rank[RootA] < Rank[RootB])
			{
				Swap(RootA, RootB);
			}
			Parent[RootB] = RootA;
			if (Rank[RootA] == Rank[RootB])
			{
				++Rank[RootA];
			}
		}
	};

	struct FWeldedTriangleComponent
	{
		TArray<int32> Triangles;
		FVector SumVertices = FVector::ZeroVector;
		FVector SumNormals = FVector::ZeroVector;
		double Area = 0.0;
		int32 VertexSampleCount = 0;

		FVector Center() const
		{
			return VertexSampleCount > 0 ? SumVertices / static_cast<double>(VertexSampleCount) : FVector::ZeroVector;
		}

		void AddTriangle(int32 TriangleIndex, int32 A, int32 B, int32 C, const TArray<FVector>& Vertices)
		{
			if (!Vertices.IsValidIndex(A) || !Vertices.IsValidIndex(B) || !Vertices.IsValidIndex(C))
			{
				return;
			}

			Triangles.Add(TriangleIndex);
			const FVector& VA = Vertices[A];
			const FVector& VB = Vertices[B];
			const FVector& VC = Vertices[C];
			SumVertices += VA + VB + VC;
			VertexSampleCount += 3;

			const FVector Normal = FVector::CrossProduct(VB - VA, VC - VA);
			Area += Normal.Size();
			SumNormals += Normal;
		}

		FVector CalculateNormal(const FVector& MeshCenter) const
		{
			const FVector Radial = Center() - MeshCenter;
			FVector Normal = !SumNormals.IsNearlyZero() ? SumNormals.GetSafeNormal() : Radial.GetSafeNormal();
			if (!Radial.IsNearlyZero() && FVector::DotProduct(Normal, Radial) < 0.0)
			{
				Normal *= -1.0;
			}
			return Normal.IsNearlyZero() ? FVector::UpVector : Normal;
		}
	};

	void CollectTriangles(const FBlenderGNMeshData& MeshData, TArray<int32>& OutTriangles)
	{
		OutTriangles.Reset();
		if (MeshData.Triangles.Num() > 0)
		{
			OutTriangles = MeshData.Triangles;
			return;
		}

		for (const FBlenderGNMeshSection& Section : MeshData.Sections)
		{
			OutTriangles.Append(Section.Triangles);
		}
	}

	int32 GetTargetIslandCount(int32 HexSubdivisions)
	{
		const int32 Subdivision = FMath::Max(1, HexSubdivisions);
		return Subdivision <= 1 ? 12 : FMath::Max(1, 10 * Subdivision * Subdivision - 10);
	}

	int32 FindNearestDirection(const FVector& Direction, const TArray<FVector>& Candidates)
	{
		int32 BestIndex = 0;
		double BestDot = -DBL_MAX;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const double Dot = FVector::DotProduct(Direction, Candidates[Index]);
			if (Dot <= BestDot)
			{
				continue;
			}

			BestDot = Dot;
			BestIndex = Index;
		}
		return BestIndex;
	}

	void BuildVirtualWeldMap(const TArray<FVector>& Vertices, float Tolerance, TArray<int32>& OutWeldedVertex, int32& OutWeldedCount)
	{
		OutWeldedVertex.SetNumUninitialized(Vertices.Num());
		OutWeldedCount = 0;

		TMap<FIntVector, TArray<int32>> Buckets;
		Buckets.Reserve(Vertices.Num());
		const float ToleranceSqr = Tolerance * Tolerance;
		for (int32 VertexIndex = 0; VertexIndex < Vertices.Num(); ++VertexIndex)
		{
			const FVector& Position = Vertices[VertexIndex];
			const FIntVector Cell = QuantizeProceduralMeshCell(Position, Tolerance);
			int32 Found = INDEX_NONE;
			for (int32 X = -1; X <= 1 && Found == INDEX_NONE; ++X)
			{
				for (int32 Y = -1; Y <= 1 && Found == INDEX_NONE; ++Y)
				{
					for (int32 Z = -1; Z <= 1 && Found == INDEX_NONE; ++Z)
					{
						const FIntVector Neighbor(Cell.X + X, Cell.Y + Y, Cell.Z + Z);
						if (const TArray<int32>* Candidates = Buckets.Find(Neighbor))
						{
							for (int32 Candidate : *Candidates)
							{
								if (FVector::DistSquared(Vertices[Candidate], Position) <= ToleranceSqr)
								{
									Found = OutWeldedVertex[Candidate];
									break;
								}
							}
						}
					}
				}
			}

			if (Found == INDEX_NONE)
			{
				Found = OutWeldedCount++;
				Buckets.FindOrAdd(Cell).Add(VertexIndex);
			}
			OutWeldedVertex[VertexIndex] = Found;
		}
	}

	TArray<FWeldedTriangleComponent> BuildWeldedTriangleComponents(const FBlenderGNMeshData& MeshData, const TArray<int32>& Triangles, float Tolerance)
	{
		const int32 TriangleCount = Triangles.Num() / 3;
		TArray<int32> WeldedVertex;
		int32 WeldedCount = 0;
		BuildVirtualWeldMap(MeshData.Vertices, Tolerance, WeldedVertex, WeldedCount);

		TMap<int32, TArray<int32>> WeldedToTriangles;
		WeldedToTriangles.Reserve(WeldedCount);
		for (int32 Triangle = 0; Triangle < TriangleCount; ++Triangle)
		{
			const int32 Offset = Triangle * 3;
			for (int32 Corner = 0; Corner < 3; ++Corner)
			{
				const int32 VertexIndex = Triangles[Offset + Corner];
				if (WeldedVertex.IsValidIndex(VertexIndex))
				{
					WeldedToTriangles.FindOrAdd(WeldedVertex[VertexIndex]).Add(Triangle);
				}
			}
		}

		FTriangleUnionFind UnionFind(TriangleCount);
		for (const TPair<int32, TArray<int32>>& Pair : WeldedToTriangles)
		{
			const TArray<int32>& SharedTriangles = Pair.Value;
			for (int32 Index = 1; Index < SharedTriangles.Num(); ++Index)
			{
				UnionFind.Union(SharedTriangles[0], SharedTriangles[Index]);
			}
		}

		TMap<int32, int32> RootToComponent;
		TArray<FWeldedTriangleComponent> Components;
		for (int32 Triangle = 0; Triangle < TriangleCount; ++Triangle)
		{
			const int32 Root = UnionFind.Find(Triangle);
			int32* ComponentIndexPtr = RootToComponent.Find(Root);
			if (!ComponentIndexPtr)
			{
				const int32 NewIndex = Components.Num();
				RootToComponent.Add(Root, NewIndex);
				Components.AddDefaulted();
				ComponentIndexPtr = RootToComponent.Find(Root);
			}

			const int32 Offset = Triangle * 3;
			Components[*ComponentIndexPtr].AddTriangle(Triangle, Triangles[Offset], Triangles[Offset + 1], Triangles[Offset + 2], MeshData.Vertices);
		}
		return Components;
	}

	float ScoreMotionComponentCandidate(const TArray<FWeldedTriangleComponent>& Components, int32 TargetIslandCount)
	{
		if (Components.Num() < TargetIslandCount)
		{
			return FLT_MAX;
		}

		double TotalArea = 0.0;
		double MaxArea = 0.0;
		for (const FWeldedTriangleComponent& Component : Components)
		{
			const double SafeArea = FMath::Max(0.000001, Component.Area);
			TotalArea += SafeArea;
			MaxArea = FMath::Max(MaxArea, SafeArea);
		}

		const double AverageArea = TotalArea / FMath::Max(1, Components.Num());
		const double MaxRatio = MaxArea / FMath::Max(0.000001, AverageArea);
		const double IdealCount = FMath::Max(static_cast<double>(TargetIslandCount), static_cast<double>(TargetIslandCount) * 14.0);
		const double CountPenalty = FMath::Abs(static_cast<double>(Components.Num()) - IdealCount) / FMath::Max(1.0, static_cast<double>(TargetIslandCount));
		const double MergePenalty = FMath::Max(0.0, MaxRatio - 24.0) * 4.0;
		const double TinyPenalty = FMath::Max(0.0, static_cast<double>(Components.Num() - TargetIslandCount * 35)) / FMath::Max(1.0, static_cast<double>(TargetIslandCount));
		return static_cast<float>(CountPenalty + MergePenalty + TinyPenalty);
	}

	bool TryBuildMotionComponents(const FBlenderGNMeshData& MeshData, const TArray<int32>& Triangles, int32 TargetIslandCount, TArray<FWeldedTriangleComponent>& OutComponents)
	{
		FBox Bounds(ForceInit);
		for (const FVector& Vertex : MeshData.Vertices)
		{
			Bounds += Vertex;
		}
		if (!Bounds.IsValid)
		{
			return false;
		}

		const FVector Extent = Bounds.GetExtent();
		const float BaseSize = FMath::Max(0.001f, static_cast<float>(FMath::Min3(Extent.X, Extent.Y, Extent.Z)));
		const float Tolerances[] =
		{
			BaseSize * 0.008f,
			BaseSize * 0.006f,
			BaseSize * 0.004f,
			BaseSize * 0.011f,
			BaseSize * 0.003f,
			BaseSize * 0.014f,
			BaseSize * 0.002f
		};

		float BestScore = FLT_MAX;
		for (float Tolerance : Tolerances)
		{
			TArray<FWeldedTriangleComponent> Candidate = BuildWeldedTriangleComponents(MeshData, Triangles, Tolerance);
			if (Candidate.Num() < TargetIslandCount)
			{
				continue;
			}

			const float Score = ScoreMotionComponentCandidate(Candidate, TargetIslandCount);
			if (Score >= BestScore)
			{
				continue;
			}

			BestScore = Score;
			OutComponents = MoveTemp(Candidate);
		}

		return OutComponents.Num() >= TargetIslandCount;
	}

	bool TryBuildDirectionSeeds(const TArray<FVector>& Directions, const TArray<float>& Weights, int32 TargetCount, TArray<FVector>& OutSeeds)
	{
		OutSeeds.Reset();
		if (Directions.Num() < TargetCount || Weights.Num() != Directions.Num() || TargetCount <= 0)
		{
			return false;
		}

		OutSeeds.SetNum(TargetCount);
		int32 First = 0;
		double BestZ = -DBL_MAX;
		for (int32 Index = 0; Index < Directions.Num(); ++Index)
		{
			if (Directions[Index].Z <= BestZ)
			{
				continue;
			}

			BestZ = Directions[Index].Z;
			First = Index;
		}

		OutSeeds[0] = Directions[First];
		TArray<float> NearestDistances;
		NearestDistances.SetNumUninitialized(Directions.Num());
		for (int32 Index = 0; Index < Directions.Num(); ++Index)
		{
			NearestDistances[Index] = 1.0f - FVector::DotProduct(Directions[Index], OutSeeds[0]);
		}

		for (int32 Seed = 1; Seed < TargetCount; ++Seed)
		{
			int32 Next = 0;
			float BestScore = -FLT_MAX;
			for (int32 Index = 0; Index < Directions.Num(); ++Index)
			{
				const float Score = NearestDistances[Index] * FMath::Sqrt(FMath::Max(0.000001f, Weights[Index]));
				if (Score <= BestScore)
				{
					continue;
				}

				BestScore = Score;
				Next = Index;
			}

			OutSeeds[Seed] = Directions[Next];
			for (int32 Index = 0; Index < Directions.Num(); ++Index)
			{
				NearestDistances[Index] = FMath::Min(NearestDistances[Index], 1.0f - FVector::DotProduct(Directions[Index], OutSeeds[Seed]));
			}
		}

		TArray<FVector> AccumDirections;
		TArray<float> AccumWeights;
		AccumDirections.SetNum(TargetCount);
		AccumWeights.SetNum(TargetCount);
		constexpr int32 IterationCount = 10;
		for (int32 Iteration = 0; Iteration < IterationCount; ++Iteration)
		{
			for (int32 Seed = 0; Seed < TargetCount; ++Seed)
			{
				AccumDirections[Seed] = FVector::ZeroVector;
				AccumWeights[Seed] = 0.0f;
			}

			for (int32 Index = 0; Index < Directions.Num(); ++Index)
			{
				const int32 Nearest = FindNearestDirection(Directions[Index], OutSeeds);
				const float Weight = FMath::Max(0.000001f, Weights[Index]);
				AccumDirections[Nearest] += Directions[Index] * Weight;
				AccumWeights[Nearest] += Weight;
			}

			for (int32 Seed = 0; Seed < TargetCount; ++Seed)
			{
				if (AccumWeights[Seed] <= 0.000001f || AccumDirections[Seed].IsNearlyZero())
				{
					continue;
				}

				OutSeeds[Seed] = AccumDirections[Seed].GetSafeNormal();
			}
		}

		return true;
	}
}

UBlenderGeometryNodesComponent* BlenderGNProcMesh::ResolveRuntime(const AActor* Owner, UBlenderGeometryNodesComponent* ExplicitRuntime)
{
	if (IsValid(ExplicitRuntime))
	{
		return ExplicitRuntime;
	}
	return Owner ? Owner->FindComponentByClass<UBlenderGeometryNodesComponent>() : nullptr;
}

bool BlenderGNProcMesh::ReadSectionVertices(const UProceduralMeshComponent* Mesh, TArray<FVector>& OutVertices)
{
	OutVertices.Reset();
	if (!Mesh)
	{
		return false;
	}

	const FProcMeshSection* Section = const_cast<UProceduralMeshComponent*>(Mesh)->GetProcMeshSection(0);
	if (!Section || Section->ProcVertexBuffer.Num() == 0)
	{
		return false;
	}

	OutVertices.Reserve(Section->ProcVertexBuffer.Num());
	for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
	{
		OutVertices.Add(Vertex.Position);
	}
	return true;
}

bool BlenderGNProcMesh::ApplyVertices(UBlenderGeometryNodesComponent* Runtime, const TArray<FVector>& Vertices, const TArray<FVector>& Normals)
{
	TArray<FLinearColor> VertexColors;
	TArray<FProcMeshTangent> Tangents;
	return ApplyVertices(Runtime, Vertices, Normals, VertexColors, Tangents);
}

bool BlenderGNProcMesh::ApplyVertices(UBlenderGeometryNodesComponent* Runtime, const TArray<FVector>& Vertices, const TArray<FVector>& Normals, TArray<FLinearColor>& ScratchVertexColors, TArray<FProcMeshTangent>& ScratchTangents)
{
	if (!Runtime)
	{
		return false;
	}

	UProceduralMeshComponent* Mesh = Runtime->GetProceduralMesh();
	const FBlenderGNMeshData& MeshData = Runtime->GetLastMeshData();
	if (!Mesh || Vertices.Num() == 0 || MeshData.Sections.Num() == 0)
	{
		return false;
	}

	const TArray<FVector>& SafeNormals = Normals.Num() == Vertices.Num() ? Normals : MeshData.Normals;
	ScratchVertexColors.Reset();
	ScratchTangents.Reset();
	for (int32 SectionIndex = 0; SectionIndex < MeshData.Sections.Num(); ++SectionIndex)
	{
		Mesh->UpdateMeshSection_LinearColor(
			SectionIndex,
			Vertices,
			SafeNormals.Num() == Vertices.Num() ? SafeNormals : TArray<FVector>(),
			MeshData.UVs,
			ScratchVertexColors,
			ScratchTangents);
	}
	return true;
}

bool BlenderGNProcMesh::BuildHexIslandMotionCache(const FBlenderGNMeshData& MeshData, int32 HexSubdivisions, float NoiseScale, FIslandMotionCache& OutCache)
{
	OutCache.VertexIsland.Reset();
	OutCache.IslandNormals.Reset();
	OutCache.IslandPhases.Reset();

	TArray<int32> Triangles;
	CollectTriangles(MeshData, Triangles);
	const int32 VertexCount = MeshData.Vertices.Num();
	const int32 TriangleCount = Triangles.Num() / 3;
	const int32 TargetIslandCount = GetTargetIslandCount(HexSubdivisions);
	if (VertexCount == 0 || TriangleCount < TargetIslandCount)
	{
		return false;
	}

	TArray<FWeldedTriangleComponent> Components;
	if (!TryBuildMotionComponents(MeshData, Triangles, TargetIslandCount, Components))
	{
		return false;
	}

	FVector MeshCenter = FVector::ZeroVector;
	for (const FVector& Vertex : MeshData.Vertices)
	{
		MeshCenter += Vertex;
	}
	MeshCenter /= static_cast<double>(VertexCount);

	TArray<FVector> ComponentDirections;
	TArray<float> ComponentWeights;
	ComponentDirections.SetNumUninitialized(Components.Num());
	ComponentWeights.SetNumUninitialized(Components.Num());
	for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ++ComponentIndex)
	{
		const FVector Direction = (Components[ComponentIndex].Center() - MeshCenter).GetSafeNormal();
		ComponentDirections[ComponentIndex] = Direction.IsNearlyZero() ? FVector::UpVector : Direction;
		ComponentWeights[ComponentIndex] = FMath::Max(0.000001f, static_cast<float>(Components[ComponentIndex].Area));
	}

	TArray<FVector> SeedDirections;
	if (!TryBuildDirectionSeeds(ComponentDirections, ComponentWeights, TargetIslandCount, SeedDirections))
	{
		return false;
	}

	OutCache.VertexIsland.Init(0, VertexCount);
	OutCache.IslandNormals.SetNum(TargetIslandCount);
	OutCache.IslandPhases.SetNum(TargetIslandCount);

	TArray<FVector> IslandNormalSums;
	TArray<FVector> IslandCenterSums;
	TArray<double> IslandWeights;
	IslandNormalSums.Init(FVector::ZeroVector, TargetIslandCount);
	IslandCenterSums.Init(FVector::ZeroVector, TargetIslandCount);
	IslandWeights.Init(0.0, TargetIslandCount);

	const float SafeNoiseScale = FMath::Max(0.2f, NoiseScale);
	for (int32 Island = 0; Island < TargetIslandCount; ++Island)
	{
		OutCache.IslandPhases[Island] = Hash01((Island + 1) * 37) * PI * 2.0f * SafeNoiseScale;
	}

	for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ++ComponentIndex)
	{
		const FWeldedTriangleComponent& Component = Components[ComponentIndex];
		const int32 Island = FindNearestDirection(ComponentDirections[ComponentIndex], SeedDirections);
		for (int32 Triangle : Component.Triangles)
		{
			const int32 Offset = Triangle * 3;
			if (Offset + 2 >= Triangles.Num())
			{
				continue;
			}

			const int32 A = Triangles[Offset];
			const int32 B = Triangles[Offset + 1];
			const int32 C = Triangles[Offset + 2];
			if (OutCache.VertexIsland.IsValidIndex(A))
			{
				OutCache.VertexIsland[A] = Island;
			}
			if (OutCache.VertexIsland.IsValidIndex(B))
			{
				OutCache.VertexIsland[B] = Island;
			}
			if (OutCache.VertexIsland.IsValidIndex(C))
			{
				OutCache.VertexIsland[C] = Island;
			}
		}

		const double Weight = ComponentWeights[ComponentIndex];
		IslandNormalSums[Island] += Component.CalculateNormal(MeshCenter) * Weight;
		IslandCenterSums[Island] += Component.Center() * Weight;
		IslandWeights[Island] += Weight;
	}

	for (int32 Island = 0; Island < TargetIslandCount; ++Island)
	{
		const FVector Center = IslandWeights[Island] > 0.0
			? IslandCenterSums[Island] / IslandWeights[Island]
			: MeshCenter + SeedDirections[Island];
		const FVector Radial = Center - MeshCenter;
		FVector Normal = !IslandNormalSums[Island].IsNearlyZero() ? IslandNormalSums[Island].GetSafeNormal() : SeedDirections[Island];
		if (!Radial.IsNearlyZero() && FVector::DotProduct(Normal, Radial) < 0.0)
		{
			Normal *= -1.0;
		}
		OutCache.IslandNormals[Island] = Normal.IsNearlyZero() ? SeedDirections[Island] : Normal.GetSafeNormal();
	}

	return true;
}

void BlenderGNProcMesh::RecalculateNormals(const FBlenderGNMeshData& MeshData, const TArray<FVector>& Vertices, TArray<FVector>& OutNormals)
{
	OutNormals.SetNumZeroed(Vertices.Num());
	for (const FBlenderGNMeshSection& Section : MeshData.Sections)
	{
		for (int32 Index = 0; Index + 2 < Section.Triangles.Num(); Index += 3)
		{
			const int32 A = Section.Triangles[Index];
			const int32 B = Section.Triangles[Index + 1];
			const int32 C = Section.Triangles[Index + 2];
			if (!Vertices.IsValidIndex(A) || !Vertices.IsValidIndex(B) || !Vertices.IsValidIndex(C))
			{
				continue;
			}

			const FVector Normal = FVector::CrossProduct(Vertices[B] - Vertices[A], Vertices[C] - Vertices[A]).GetSafeNormal();
			OutNormals[A] += Normal;
			OutNormals[B] += Normal;
			OutNormals[C] += Normal;
		}
	}

	for (FVector& Normal : OutNormals)
	{
		Normal = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
	}
}

TArray<TArray<int32>> BlenderGNProcMesh::BuildPositionGroups(const TArray<FVector>& Vertices, float Quantization)
{
	TMap<FIntVector, TArray<int32>> Buckets;
	Buckets.Reserve(Vertices.Num());
	for (int32 Index = 0; Index < Vertices.Num(); ++Index)
	{
		Buckets.FindOrAdd(QuantizePosition(Vertices[Index], Quantization)).Add(Index);
	}

	TArray<TArray<int32>> Groups;
	for (TPair<FIntVector, TArray<int32>>& Pair : Buckets)
	{
		if (Pair.Value.Num() > 1)
		{
			Groups.Add(MoveTemp(Pair.Value));
		}
	}
	return Groups;
}

void BlenderGNProcMesh::SmoothNormalsByGroups(const TArray<TArray<int32>>& Groups, float SmoothingAngleDegrees, TArray<FVector>& Normals)
{
	if (SmoothingAngleDegrees <= 0.0f || Normals.Num() == 0)
	{
		return;
	}

	const float DotThreshold = FMath::Cos(FMath::DegreesToRadians(SmoothingAngleDegrees));
	TArray<FVector> Smoothed = Normals;
	for (const TArray<int32>& Group : Groups)
	{
		for (int32 VertexIndex : Group)
		{
			if (!Normals.IsValidIndex(VertexIndex))
			{
				continue;
			}

			FVector Sum = FVector::ZeroVector;
			for (int32 OtherIndex : Group)
			{
				if (Normals.IsValidIndex(OtherIndex) && FVector::DotProduct(Normals[VertexIndex], Normals[OtherIndex]) >= DotThreshold)
				{
					Sum += Normals[OtherIndex];
				}
			}
			if (!Sum.IsNearlyZero())
			{
				Smoothed[VertexIndex] = Sum.GetSafeNormal();
			}
		}
	}
	Normals = MoveTemp(Smoothed);
}

float BlenderGNProcMesh::Hash01(int32 Seed)
{
	uint32 X = static_cast<uint32>(Seed);
	X ^= X >> 16;
	X *= 0x7feb352d;
	X ^= X >> 15;
	X *= 0x846ca68b;
	X ^= X >> 16;
	return static_cast<float>(X & 0x00FFFFFF) / 16777215.0f;
}

FVector BlenderGNProcMesh::AxisVector(int32 Axis)
{
	switch (Axis)
	{
	case 0:
		return FVector::XAxisVector;
	case 1:
		return FVector::YAxisVector;
	default:
		return FVector::ZAxisVector;
	}
}

FVector BlenderGNProcMesh::ApplyAxisOffset(FVector Vertex, int32 Axis, float Offset)
{
	Vertex += AxisVector(Axis) * Offset;
	return Vertex;
}

FVector2D BlenderGNProcMesh::AxisSampleCoordinates(const FVector& Vertex, int32 Axis)
{
	switch (Axis)
	{
	case 0:
		return FVector2D(Vertex.Y, Vertex.Z);
	case 1:
		return FVector2D(Vertex.X, Vertex.Z);
	default:
		return FVector2D(Vertex.X, Vertex.Y);
	}
}
