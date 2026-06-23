#include "BlenderGNFastDynamicMeshComponent.h"
#include "PrimitiveViewRelevance.h"
#include "RenderResource.h"
#include "RenderingThread.h"
#include "PrimitiveSceneProxy.h"
#include "SceneManagement.h"
#include "DynamicMeshBuilder.h"
#include "Materials/Material.h"
#include "Engine/Engine.h"
#include "SceneInterface.h"
#include "MaterialDomain.h"
#include "StaticMeshResources.h"
#include "LocalVertexFactory.h"

class FDynamicPositionVertexBuffer : public FVertexBuffer
{
public:
	TArray<FVector3f> Positions;
	FShaderResourceViewRHIRef SRV;

	void Init(const TArray<FVector3f>& InPositions)
	{
		Positions = InPositions;
	}

	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		const uint32 Size = Positions.Num() * sizeof(FVector3f);
		FRHIResourceCreateInfo CreateInfo(TEXT("BlenderGN_DynPosVB"));
		VertexBufferRHI = RHICmdList.CreateBuffer(Size, BUF_Dynamic | BUF_VertexBuffer | BUF_ShaderResource, sizeof(FVector3f), ERHIAccess::VertexOrIndexBuffer | ERHIAccess::SRVMask, CreateInfo);
		void* Data = RHICmdList.LockBuffer(VertexBufferRHI, 0, Size, RLM_WriteOnly);
		FMemory::Memcpy(Data, Positions.GetData(), Size);
		RHICmdList.UnlockBuffer(VertexBufferRHI);
		SRV = RHICmdList.CreateShaderResourceView(
			VertexBufferRHI,
			FRHIViewDesc::CreateBufferSRV()
				.SetType(FRHIViewDesc::EBufferType::Typed)
				.SetFormat(PF_R32_FLOAT));
	}

	virtual void ReleaseRHI() override
	{
		SRV.SafeRelease();
		FVertexBuffer::ReleaseRHI();
	}

	uint32 GetNumVertices() const { return Positions.Num(); }
};

class FBlenderGNFastMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	FBlenderGNFastMeshSceneProxy(UBlenderGNFastDynamicMeshComponent* Component,
		TArray<FVector3f>&& InPositions,
		TArray<FDynamicMeshVertex>&& InStaticVerts,
		TArray<uint32>&& InIndices,
		TArray<FBlenderGNFastIslandBatch>&& InIslandBatches,
		float InMotionAmplitude,
		float InMotionSpeed,
		float InMotionSmoothness)
		: FPrimitiveSceneProxy(Component)
		, Material(Component->GetMaterial(0) ? Component->GetMaterial(0) : UMaterial::GetDefaultMaterial(MD_Surface))
		, MaterialRelevance(Component->GetMaterialRelevance(GetScene().GetShaderPlatform()))
		, VertexFactory(GetScene().GetFeatureLevel(), "FBlenderGNFastMeshProxy")
		, Positions(MoveTemp(InPositions))
		, StaticVerts(MoveTemp(InStaticVerts))
		, Indices(MoveTemp(InIndices))
		, IslandBatches(MoveTemp(InIslandBatches))
		, MotionAmplitude(InMotionAmplitude)
		, MotionSpeed(InMotionSpeed)
		, MotionSmoothness(InMotionSmoothness)
		, NumVertices(Positions.Num())
		, NumPrimitives(Indices.Num() / 3)
	{
		bWillEverBeLit = true;
	}

	virtual ~FBlenderGNFastMeshSceneProxy() override
	{
		VertexFactory.ReleaseResource();
		DynPositionVB.ReleaseResource();
		StaticMeshVBs.StaticMeshVertexBuffer.ReleaseResource();
		StaticMeshVBs.ColorVertexBuffer.ReleaseResource();
		IB.ReleaseResource();
	}

	SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	virtual void CreateRenderThreadResources(FRHICommandListBase& RHICmdList) override
	{
		// Use InitFromDynamicVertex to set up static buffers (tangent, UV, color) + position
		// Pass nullptr for VertexFactory - we'll set it up ourselves after overriding position
		StaticMeshVBs.PositionVertexBuffer.Init(NumVertices);
		StaticMeshVBs.StaticMeshVertexBuffer.Init(NumVertices, 1);
		StaticMeshVBs.ColorVertexBuffer.Init(NumVertices);

		for (int32 i = 0; i < NumVertices; ++i)
		{
			const FDynamicMeshVertex& Vertex = StaticVerts[i];
			StaticMeshVBs.PositionVertexBuffer.VertexPosition(i) = Vertex.Position;
			StaticMeshVBs.StaticMeshVertexBuffer.SetVertexTangents(i, Vertex.TangentX.ToFVector3f(), Vertex.GetTangentY(), Vertex.TangentZ.ToFVector3f());
			StaticMeshVBs.StaticMeshVertexBuffer.SetVertexUV(i, 0, Vertex.TextureCoordinate[0]);
			StaticMeshVBs.ColorVertexBuffer.VertexColor(i) = Vertex.Color;
		}

		// Init static GPU resources (tangent, UV, color - BUF_Static by default)
		StaticMeshVBs.StaticMeshVertexBuffer.InitResource(RHICmdList);
		StaticMeshVBs.ColorVertexBuffer.InitResource(RHICmdList);

		// Create dynamic position buffer (BUF_Dynamic for fast per-frame updates)
		{
			DynPositionVB.Init(Positions);
			DynPositionVB.InitResource(RHICmdList);
		}

		// Index buffer
		IB.Indices = Indices;
		IB.InitResource(RHICmdList);

		// Set up vertex factory - bind tangent/UV/color from static buffers, position from dynamic buffer
		{
			FLocalVertexFactory::FDataType VFData;
			StaticMeshVBs.StaticMeshVertexBuffer.BindTangentVertexBuffer(&VertexFactory, VFData);
			StaticMeshVBs.StaticMeshVertexBuffer.BindPackedTexCoordVertexBuffer(&VertexFactory, VFData);
			StaticMeshVBs.ColorVertexBuffer.BindColorVertexBuffer(&VertexFactory, VFData);

			VFData.PositionComponent = FVertexStreamComponent(&DynPositionVB, 0, sizeof(FVector3f), VET_Float3);
			VFData.PositionComponentSRV = DynPositionVB.SRV;

			VertexFactory.SetData(RHICmdList, VFData);
		}
		VertexFactory.InitResource(RHICmdList);

		// Free CPU-side static vertex data
		StaticVerts.Empty();
	}

	void UpdatePositions_RenderThread(FRHICommandListImmediate& RHICmdList, const TArray<FVector3f>& NewPositions)
	{
		if (NewPositions.Num() != NumVertices || !DynPositionVB.VertexBufferRHI.IsValid())
		{
			return;
		}
		const uint32 Size = NumVertices * sizeof(FVector3f);
		void* Data = RHICmdList.LockBuffer(DynPositionVB.VertexBufferRHI, 0, Size, RLM_WriteOnly);
		FMemory::Memcpy(Data, NewPositions.GetData(), Size);
		RHICmdList.UnlockBuffer(DynPositionVB.VertexBufferRHI);
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
		if (NumVertices == 0 || NumPrimitives == 0)
		{
			return;
		}

		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ++ViewIndex)
		{
			if (!(VisibilityMap & (1 << ViewIndex)))
			{
				continue;
			}

			if (IslandBatches.Num() > 0)
			{
				for (const FBlenderGNFastIslandBatch& IslandBatch : IslandBatches)
				{
					if (IslandBatch.NumPrimitives == 0)
					{
						continue;
					}

					const float Wave = FMath::Sin(static_cast<float>(ViewFamily.Time.GetWorldTimeSeconds()) * MotionSpeed + IslandBatch.Phase);
					const float Shaped = FMath::Lerp(Wave, FMath::SmoothStep(-1.0f, 1.0f, Wave), MotionSmoothness);
					const float Blend = FMath::Lerp(1.0f, 0.35f, MotionSmoothness);
					const FVector Offset = FVector(IslandBatch.Normal) * (Shaped * MotionAmplitude * Blend);
					const FMatrix IslandLocalToWorld = FTranslationMatrix(Offset) * GetLocalToWorld();

					FMeshBatch& Mesh = Collector.AllocateMesh();
					Mesh.VertexFactory = &VertexFactory;
					Mesh.MaterialRenderProxy = Material->GetRenderProxy();
					Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
					Mesh.Type = PT_TriangleList;
					Mesh.DepthPriorityGroup = SDPG_World;
					Mesh.bCanApplyViewModeOverrides = true;
					Mesh.CastShadow = false;

					FMeshBatchElement& Element = Mesh.Elements[0];
					Element.IndexBuffer = &IB;
					Element.FirstIndex = IslandBatch.FirstIndex;
					Element.NumPrimitives = IslandBatch.NumPrimitives;
					Element.MinVertexIndex = 0;
					Element.MaxVertexIndex = NumVertices - 1;

					FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
					DynamicPrimitiveUniformBuffer.Set(Collector.GetRHICommandList(), IslandLocalToWorld, IslandLocalToWorld, GetBounds(), GetLocalBounds(), true, false, AlwaysHasVelocity());
					Element.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

					Collector.AddMesh(ViewIndex, Mesh);
				}
				continue;
			}

			FMeshBatch& Mesh = Collector.AllocateMesh();
			Mesh.VertexFactory = &VertexFactory;
			Mesh.MaterialRenderProxy = Material->GetRenderProxy();
			Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.bCanApplyViewModeOverrides = true;
			Mesh.CastShadow = IsShadowCast(Views[ViewIndex]);

			FMeshBatchElement& Element = Mesh.Elements[0];
			Element.IndexBuffer = &IB;
			Element.FirstIndex = 0;
			Element.NumPrimitives = NumPrimitives;
			Element.MinVertexIndex = 0;
			Element.MaxVertexIndex = NumVertices - 1;

			FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
			DynamicPrimitiveUniformBuffer.Set(Collector.GetRHICommandList(), GetLocalToWorld(), GetLocalToWorld(), GetBounds(), GetLocalBounds(), true, false, AlwaysHasVelocity());
			Element.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

			Collector.AddMesh(ViewIndex, Mesh);
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View) && IslandBatches.Num() == 0;
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		Result.bVelocityRelevance = false;
		return Result;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + Positions.GetAllocatedSize() + Indices.GetAllocatedSize();
	}

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

private:
	UMaterialInterface* Material;
	FMaterialRelevance MaterialRelevance;
	FLocalVertexFactory VertexFactory;
	FStaticMeshVertexBuffers StaticMeshVBs;
	FDynamicPositionVertexBuffer DynPositionVB;
	FDynamicMeshIndexBuffer32 IB;
	TArray<FVector3f> Positions;
	TArray<FDynamicMeshVertex> StaticVerts;
	TArray<uint32> Indices;
	TArray<FBlenderGNFastIslandBatch> IslandBatches;
	float MotionAmplitude;
	float MotionSpeed;
	float MotionSmoothness;
	int32 NumVertices;
	int32 NumPrimitives;
};

// ============================================================================

UBlenderGNFastDynamicMeshComponent::UBlenderGNFastDynamicMeshComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBlenderGNFastDynamicMeshComponent::InitMesh(
	const TArray<FVector>& Vertices,
	const TArray<int32>& Triangles,
	const TArray<FVector>& Normals,
	const TArray<FVector2D>& UVs,
	const TArray<FColor>& Colors,
	const TArray<FProcMeshTangent>& Tangents)
{
	const int32 NumVerts = Vertices.Num();
	CachedVertexCount = NumVerts;
	IslandBatches.Reset();
	MotionAmplitude = 0.0f;
	MotionSpeed = 1.0f;
	MotionSmoothness = 0.0f;

	// Build position buffer (FVector3f)
	PositionBuffer.SetNumUninitialized(NumVerts);
	FBox MeshBounds(ForceInit);
	for (int32 i = 0; i < NumVerts; ++i)
	{
		PositionBuffer[i] = FVector3f(Vertices[i]);
		MeshBounds += Vertices[i];
	}
	const float BoundsPadding = FMath::Max(200.0f, MeshBounds.GetExtent().GetMax() * 1.0f);
	LocalBounds = MeshBounds.ExpandBy(BoundsPadding);

	// Build normals
	NormalBuffer.SetNumUninitialized(NumVerts);
	for (int32 i = 0; i < NumVerts; ++i)
	{
		NormalBuffer[i] = (i < Normals.Num()) ? FVector3f(Normals[i]) : FVector3f(0, 0, 1);
	}

	// Build UVs
	UVBuffer.SetNumUninitialized(NumVerts);
	for (int32 i = 0; i < NumVerts; ++i)
	{
		UVBuffer[i] = (i < UVs.Num()) ? FVector2f(UVs[i]) : FVector2f::ZeroVector;
	}

	// Build colors
	ColorBuffer.SetNumUninitialized(NumVerts);
	for (int32 i = 0; i < NumVerts; ++i)
	{
		ColorBuffer[i] = (i < Colors.Num()) ? Colors[i] : FColor::White;
	}

	// Build tangents
	TangentXBuffer.SetNumUninitialized(NumVerts);
	TangentZBuffer.SetNumUninitialized(NumVerts);
	for (int32 i = 0; i < NumVerts; ++i)
	{
		if (i < Tangents.Num())
		{
			TangentXBuffer[i] = FVector3f(Tangents[i].TangentX);
			FVector3f N = NormalBuffer[i];
			TangentZBuffer[i] = FVector4f(N.X, N.Y, N.Z, Tangents[i].bFlipTangentY ? -1.0f : 1.0f);
		}
		else
		{
			TangentXBuffer[i] = FVector3f(1, 0, 0);
			FVector3f N = NormalBuffer[i];
			TangentZBuffer[i] = FVector4f(N.X, N.Y, N.Z, 1.0f);
		}
	}

	// Build index buffer
	IndexBuffer.SetNumUninitialized(Triangles.Num());
	for (int32 i = 0; i < Triangles.Num(); ++i)
	{
		IndexBuffer[i] = static_cast<uint32>(Triangles[i]);
	}

	MarkRenderStateDirty();
	UpdateBounds();
}

void UBlenderGNFastDynamicMeshComponent::InitIslandMotionMesh(
	const TArray<FVector>& Vertices,
	const TArray<int32>& Triangles,
	const TArray<FVector>& Normals,
	const TArray<FVector2D>& UVs,
	const TArray<FColor>& Colors,
	const TArray<FProcMeshTangent>& Tangents,
	const TArray<int32>& VertexIsland,
	const TArray<FVector>& IslandNormals,
	const TArray<float>& IslandPhases,
	float Amplitude,
	float Speed,
	float Smoothness)
{
	InitMesh(Vertices, Triangles, Normals, UVs, Colors, Tangents);

	const int32 IslandCount = IslandNormals.Num();
	if (IslandCount <= 0 || VertexIsland.Num() != Vertices.Num())
	{
		return;
	}

	TArray<TArray<uint32>> IndicesByIsland;
	IndicesByIsland.SetNum(IslandCount);
	for (int32 Index = 0; Index + 2 < Triangles.Num(); Index += 3)
	{
		const int32 A = Triangles[Index];
		const int32 B = Triangles[Index + 1];
		const int32 C = Triangles[Index + 2];
		if (!VertexIsland.IsValidIndex(A) || !VertexIsland.IsValidIndex(B) || !VertexIsland.IsValidIndex(C))
		{
			continue;
		}

		const int32 Island = VertexIsland[A];
		if (!IndicesByIsland.IsValidIndex(Island))
		{
			continue;
		}

		IndicesByIsland[Island].Add(static_cast<uint32>(A));
		IndicesByIsland[Island].Add(static_cast<uint32>(B));
		IndicesByIsland[Island].Add(static_cast<uint32>(C));
	}

	IndexBuffer.Reset();
	IslandBatches.Reset();
	for (int32 Island = 0; Island < IslandCount; ++Island)
	{
		const TArray<uint32>& IslandIndices = IndicesByIsland[Island];
		if (IslandIndices.Num() < 3)
		{
			continue;
		}

		FBlenderGNFastIslandBatch& Batch = IslandBatches.AddDefaulted_GetRef();
		Batch.FirstIndex = static_cast<uint32>(IndexBuffer.Num());
		Batch.NumPrimitives = static_cast<uint32>(IslandIndices.Num() / 3);
		Batch.Normal = FVector3f(IslandNormals[Island].GetSafeNormal());
		Batch.Phase = IslandPhases.IsValidIndex(Island) ? IslandPhases[Island] : 0.0f;
		IndexBuffer.Append(IslandIndices);
	}

	MotionAmplitude = Amplitude;
	MotionSpeed = FMath::Max(0.0f, Speed);
	MotionSmoothness = FMath::Clamp(Smoothness, 0.0f, 1.0f);
	MarkRenderStateDirty();
	UpdateBounds();
}

void UBlenderGNFastDynamicMeshComponent::UpdatePositionsOnly(const TArray<FVector>& NewPositions)
{
	if (NewPositions.Num() != CachedVertexCount)
	{
		return;
	}

	for (int32 i = 0; i < CachedVertexCount; ++i)
	{
		PositionBuffer[i] = FVector3f(NewPositions[i]);
	}

	// Send only positions to render thread
	FBlenderGNFastMeshSceneProxy* Proxy = static_cast<FBlenderGNFastMeshSceneProxy*>(SceneProxy);
	if (Proxy)
	{
		TArray<FVector3f> PositionsCopy = PositionBuffer;
		ENQUEUE_RENDER_COMMAND(UpdateBlenderGNPositions)(
			[Proxy, Positions = MoveTemp(PositionsCopy)](FRHICommandListImmediate& RHICmdList)
			{
				Proxy->UpdatePositions_RenderThread(RHICmdList, Positions);
			});
	}
}

void UBlenderGNFastDynamicMeshComponent::UpdatePositionsOnly(const TArray<FVector3f>& NewPositions)
{
	if (NewPositions.Num() != CachedVertexCount)
	{
		return;
	}

	FMemory::Memcpy(PositionBuffer.GetData(), NewPositions.GetData(), CachedVertexCount * sizeof(FVector3f));

	FBlenderGNFastMeshSceneProxy* Proxy = static_cast<FBlenderGNFastMeshSceneProxy*>(SceneProxy);
	if (Proxy)
	{
		TArray<FVector3f> PositionsCopy = NewPositions;
		ENQUEUE_RENDER_COMMAND(UpdateBlenderGNPositions)(
			[Proxy, Positions = MoveTemp(PositionsCopy)](FRHICommandListImmediate& RHICmdList)
			{
				Proxy->UpdatePositions_RenderThread(RHICmdList, Positions);
			});
	}
}

FPrimitiveSceneProxy* UBlenderGNFastDynamicMeshComponent::CreateSceneProxy()
{
	if (CachedVertexCount == 0 || IndexBuffer.Num() == 0)
	{
		return nullptr;
	}

	// Build DynamicMeshVertex array for static buffers initialization
	TArray<FDynamicMeshVertex> DynVerts;
	DynVerts.SetNumUninitialized(CachedVertexCount);
	for (int32 i = 0; i < CachedVertexCount; ++i)
	{
		FDynamicMeshVertex& V = DynVerts[i];
		V.Position = PositionBuffer[i];
		V.TangentX = FVector3f(TangentXBuffer[i]);
		V.TangentZ = FVector3f(TangentZBuffer[i]);
		V.TangentZ.Vector.W = (TangentZBuffer[i].W < 0.0f) ? -128 : 127;
		V.TextureCoordinate[0] = UVBuffer[i];
		V.Color = ColorBuffer[i];
	}

	TArray<FVector3f> PosCopy = PositionBuffer;
	TArray<uint32> IdxCopy = IndexBuffer;

	TArray<FBlenderGNFastIslandBatch> IslandBatchCopy = IslandBatches;
	return new FBlenderGNFastMeshSceneProxy(
		this,
		MoveTemp(PosCopy),
		MoveTemp(DynVerts),
		MoveTemp(IdxCopy),
		MoveTemp(IslandBatchCopy),
		MotionAmplitude,
		MotionSpeed,
		MotionSmoothness);
}

FBoxSphereBounds UBlenderGNFastDynamicMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	if (LocalBounds.IsValid)
	{
		return FBoxSphereBounds(LocalBounds.TransformBy(LocalToWorld));
	}
	return FBoxSphereBounds(LocalToWorld.GetLocation(), FVector::ZeroVector, 0.0f);
}

int32 UBlenderGNFastDynamicMeshComponent::GetNumMaterials() const
{
	return 1;
}

UMaterialInterface* UBlenderGNFastDynamicMeshComponent::GetMaterial(int32 ElementIndex) const
{
	return (ElementIndex == 0) ? Super::GetMaterial(0) : nullptr;
}
