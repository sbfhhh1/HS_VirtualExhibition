#include "BlenderGNGradientBoxMotionComponent.h"

#include "BlenderGNRuntimePanelWidget.h"
#include "BlenderGeometryNodesComponent.h"
#include "BlenderGeometryNodesTypes.h"
#include "BlenderGNProceduralMeshUtils.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Materials/Material.h"
#include "ProceduralMeshComponent.h"

namespace
{
	constexpr float MinimumScale = 0.22f;
	constexpr float MaximumScale = 1.45f;
	constexpr float ReliableImportedScale = 0.03f;
	constexpr float NeutralClampPadding = 1.15f;
	constexpr int32 ReferenceNum = 8;
	constexpr float ReferenceSize = 1.6788921f;
	constexpr float ReferenceGap = 0.15f;
	constexpr float ReferenceFrame = 1.442309f;
	constexpr float ReferenceGnSpeed = 5.0f;
	constexpr float ReferencePower = 2.0f;
	constexpr float ReferenceShiftAngle = 180.0f;
	constexpr float ReferenceFrameRate = 60.0f;

	FIntVector QuantizeGradientMotionCell(const FVector& Value, float CellSize)
	{
		const float SafeCellSize = FMath::Max(0.000001f, CellSize);
		return FIntVector(
			FMath::FloorToInt(Value.X / SafeCellSize),
			FMath::FloorToInt(Value.Y / SafeCellSize),
			FMath::FloorToInt(Value.Z / SafeCellSize));
	}

	void AddGradientMotionEdge(TArray<TArray<int32>>& Adjacency, int32 A, int32 B)
	{
		if (Adjacency.IsValidIndex(A) && Adjacency.IsValidIndex(B))
		{
			Adjacency[A].AddUnique(B);
			Adjacency[B].AddUnique(A);
		}
	}

	FVector AbsVector(const FVector& Value)
	{
		return FVector(FMath::Abs(Value.X), FMath::Abs(Value.Y), FMath::Abs(Value.Z));
	}

	FVector MaxVector(const FVector& A, const FVector& B)
	{
		return FVector(
			FMath::Max(A.X, B.X),
			FMath::Max(A.Y, B.Y),
			FMath::Max(A.Z, B.Z));
	}

	float MedianOrFallback(TArray<float>& Values, float Fallback)
	{
		if (Values.Num() == 0)
		{
			return Fallback;
		}

		Values.Sort();
		const int32 Mid = Values.Num() / 2;
		return (Values.Num() & 1) != 0 ? Values[Mid] : (Values[Mid - 1] + Values[Mid]) * 0.5f;
	}

	float SignedExtent(float Delta, float Normal, float Extent)
	{
		if (FMath::Abs(Delta) > 0.000001f)
		{
			return FMath::Sign(Delta) * Extent;
		}
		if (FMath::Abs(Normal) > 0.25f)
		{
			return FMath::Sign(Normal) * Extent;
		}
		return Extent;
	}

	bool IsUsableNeutralDelta(const FVector& Delta, const FVector& Limit)
	{
		if (Delta.SizeSquared() < 0.00000001)
		{
			return false;
		}

		return FMath::Abs(Delta.X) <= Limit.X * 4.0f
			&& FMath::Abs(Delta.Y) <= Limit.Y * 4.0f
			&& FMath::Abs(Delta.Z) <= Limit.Z * 4.0f;
	}

	FVector UnrealToSourcePosition(const FVector& UnrealPosition, float UnrealScale)
	{
		const float SafeScale = FMath::Max(0.0001f, UnrealScale);
		return FVector(UnrealPosition.X, -UnrealPosition.Y, UnrealPosition.Z) / SafeScale;
	}
}

UBlenderGNGradientBoxMotionComponent::UBlenderGNGradientBoxMotionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	bTickInEditor = true;
	UpdateTickInterval();
}

void UBlenderGNGradientBoxMotionComponent::SetLiveMotion(bool bEnabled)
{
	if (bLiveMotion == bEnabled)
	{
		return;
	}

	bLiveMotion = bEnabled;
	UpdateTickInterval();
	SetComponentTickEnabled(bLiveMotion);
	if (!bLiveMotion)
	{
		DestroyFastSectionMeshes();
		if (BoundRuntime)
		{
			if (UProceduralMeshComponent* ProceduralMesh = BoundRuntime->GetProceduralMesh())
			{
				ProceduralMesh->SetVisibility(true);
			}
		}
	}
}

void UBlenderGNGradientBoxMotionComponent::UpdateTickRate(float NewFPS)
{
	MaxAnimationFrameRate = FMath::Clamp(NewFPS, 1.0f, 120.0f);
	UpdateTickInterval();
}

void UBlenderGNGradientBoxMotionComponent::SetAmplitude(float Value)
{
	Amplitude = FMath::Clamp(Value, 0.0f, 1.0f);
	ApplyMotionImmediately();
}

void UBlenderGNGradientBoxMotionComponent::SetSpeed(float Value)
{
	Speed = FMath::Clamp(Value, 0.0f, 4.0f);
	ApplyMotionImmediately();
}

void UBlenderGNGradientBoxMotionComponent::SetSmoothness(float Value)
{
	Smoothness = FMath::Clamp(Value, 0.0f, 1.0f);
	ApplyMotionImmediately();
}

void UBlenderGNGradientBoxMotionComponent::SetReversePinkAnimation(bool bReverse)
{
	if (bReversePinkAnimation == bReverse)
	{
		return;
	}

	bReversePinkAnimation = bReverse;
	ApplyMotionImmediately();
}

void UBlenderGNGradientBoxMotionComponent::SetUseFastDynamicMesh(bool bEnabled)
{
	if (bUseFastDynamicMesh == bEnabled)
	{
		return;
	}

	bUseFastDynamicMesh = bEnabled;
	if (!bUseFastDynamicMesh)
	{
		DestroyFastSectionMeshes();
	}
	ApplyMotionImmediately();
}

void UBlenderGNGradientBoxMotionComponent::SetRecalculateAnimatedNormals(bool bEnabled)
{
	if (bRecalculateAnimatedNormals == bEnabled)
	{
		return;
	}

	bRecalculateAnimatedNormals = bEnabled;
	if (bRecalculateAnimatedNormals)
	{
		DestroyFastSectionMeshes();
	}
	ApplyMotionImmediately();
}

void UBlenderGNGradientBoxMotionComponent::OnRegister()
{
	Super::OnRegister();
	BindRuntime();
}

void UBlenderGNGradientBoxMotionComponent::OnUnregister()
{
	DestroyFastSectionMeshes();
	UnbindRuntime();
	Super::OnUnregister();
}

void UBlenderGNGradientBoxMotionComponent::BeginPlay()
{
	Super::BeginPlay();
	MotionStartTime = FPlatformTime::Seconds();
	BindRuntime();
	ApplyReferenceInputs();
	RebuildCache();
	UpdateTickInterval();
	SetComponentTickEnabled(bLiveMotion);
	CreateRuntimeControlPanel();
}

void UBlenderGNGradientBoxMotionComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bCreateRuntimeControlPanel && !RuntimeControlPanel)
	{
		CreateRuntimeControlPanel();
	}

	if (!bLiveMotion)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (MotionStartTime <= 0.0)
	{
		MotionStartTime = Now;
	}
	LastMotionUpdateTime = Now;
	ApplyMotion(static_cast<float>(Now - MotionStartTime));
}

void UBlenderGNGradientBoxMotionComponent::HandleMeshUpdated(UProceduralMeshComponent* ProceduralMesh)
{
	MotionStartTime = FPlatformTime::Seconds();
	RebuildCache();
	ApplyMotion(0.0f);
}

void UBlenderGNGradientBoxMotionComponent::BindRuntime()
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
		BoundRuntime->OnMeshUpdated.AddUniqueDynamic(this, &UBlenderGNGradientBoxMotionComponent::HandleMeshUpdated);
	}
}

void UBlenderGNGradientBoxMotionComponent::UnbindRuntime()
{
	if (BoundRuntime)
	{
		BoundRuntime->OnMeshUpdated.RemoveDynamic(this, &UBlenderGNGradientBoxMotionComponent::HandleMeshUpdated);
		BoundRuntime = nullptr;
	}
}

void UBlenderGNGradientBoxMotionComponent::ApplyReferenceInputs()
{
	if (!bApplyReferenceInputsOnBeginPlay || !BoundRuntime)
	{
		return;
	}

	BoundRuntime->BlenderFrameRate = ReferenceFrameRate;
	auto EnsureFloatInput = [this](const FString& SocketName, const FString& DisplayName, float Value, float Min, float Max)
	{
		for (FBlenderGNInput& Input : BoundRuntime->Inputs)
		{
			if (Input.MatchesName(SocketName) || Input.MatchesName(DisplayName))
			{
				Input.SocketName = SocketName;
				Input.DisplayName = DisplayName;
				Input.FallbackIdentifier = SocketName;
				Input.Type = EBlenderGNParameterType::Float;
				Input.FloatValue = Value;
				Input.SliderMin = Min;
				Input.SliderMax = Max;
				Input.bShowInRuntimePanel = true;
				return;
			}
		}

		FBlenderGNInput& Input = BoundRuntime->Inputs.AddDefaulted_GetRef();
		Input.SocketName = SocketName;
		Input.DisplayName = DisplayName;
		Input.FallbackIdentifier = SocketName;
		Input.Type = EBlenderGNParameterType::Float;
		Input.FloatValue = Value;
		Input.SliderMin = Min;
		Input.SliderMax = Max;
		Input.bShowInRuntimePanel = true;
	};

	BoundRuntime->SetIntInput(TEXT("Socket_2"), ReferenceNum, false);
	BoundRuntime->SetFloatInput(TEXT("Socket_3"), ReferenceSize, false);
	EnsureFloatInput(TEXT("Socket_8"), TEXT("gap"), ReferenceGap, 0.0f, 0.6f);
	BoundRuntime->SetFloatInput(TEXT("Socket_4"), ReferenceFrame, false);
	BoundRuntime->SetFloatInput(TEXT("Socket_5"), ReferenceGnSpeed, false);
	BoundRuntime->SetFloatInput(TEXT("Socket_6"), ReferencePower, false);
	BoundRuntime->SetFloatInput(TEXT("Socket_7"), ReferenceShiftAngle, false);

	if (bRefreshReferenceMeshOnBeginPlay)
	{
		BoundRuntime->RequestRefresh(true);
	}
}

void UBlenderGNGradientBoxMotionComponent::RebuildCache()
{
	BindRuntime();
	if (!BoundRuntime)
	{
		return;
	}

	if (BoundRuntime->GetLastMeshData().Vertices.Num() == 0)
	{
		BoundRuntime->RestoreLastMeshDataFromProceduralMesh();
	}

	const FBlenderGNMeshData& MeshData = BoundRuntime->GetLastMeshData();
	BaseVertices = MeshData.Vertices;
	BaseNormals = MeshData.Normals;
	CachedVertexCount = BaseVertices.Num();
	AnimatedVertices.SetNumUninitialized(CachedVertexCount);
	AnimatedNormals.Reset();

	BaseBounds = FBox(ForceInit);
	for (const FVector& Vertex : BaseVertices)
	{
		BaseBounds += Vertex;
	}

	CacheRuntimeParameters();
	BuildIslands(MeshData);
	NeutralHalfExtents = EstimateNeutralHalfExtents();
	IslandNeutralHalfExtents = EstimateIslandNeutralHalfExtents();
	NormalGroups = BlenderGNProcMesh::BuildPositionGroups(BaseVertices, NormalMergeQuantization);
	BoundRuntime->DisablePreviewAnimationMeshes();
	DestroyFastSectionMeshes();
	if (UProceduralMeshComponent* ProceduralMesh = BoundRuntime->GetProceduralMesh())
	{
		ProceduralMesh->SetVisibility(true);
		ProceduralMesh->SetHiddenInGame(false);
	}
}

void UBlenderGNGradientBoxMotionComponent::CacheRuntimeParameters()
{
	CachedFrameInput = 1.0f;
	CachedGnSpeedDegreesPerFrame = 5.0f;
	CachedShiftAngleDegrees = 180.0f;
	CachedPower = 2.0f;
	CachedBlenderFrameRate = 60.0f;

	if (!BoundRuntime)
	{
		return;
	}

	CachedFrameInput = BoundRuntime->GetFloatInput(TEXT("Socket_4"), BoundRuntime->GetFloatInput(TEXT("frame"), CachedFrameInput));
	CachedGnSpeedDegreesPerFrame = BoundRuntime->GetFloatInput(TEXT("Socket_5"), BoundRuntime->GetFloatInput(TEXT("speed"), CachedGnSpeedDegreesPerFrame));
	CachedPower = BoundRuntime->GetFloatInput(TEXT("Socket_6"), BoundRuntime->GetFloatInput(TEXT("power"), CachedPower));
	CachedShiftAngleDegrees = BoundRuntime->GetFloatInput(TEXT("Socket_7"), BoundRuntime->GetFloatInput(TEXT("shift ang"), CachedShiftAngleDegrees));
	CachedBlenderFrameRate = FMath::Max(1.0f, BoundRuntime->BlenderFrameRate);
}

void UBlenderGNGradientBoxMotionComponent::BuildIslands(const FBlenderGNMeshData& MeshData)
{
	VertexIsland.Init(INDEX_NONE, BaseVertices.Num());
	IslandCenters.Reset();
	IslandPhases.Reset();
	IslandScales.Reset();
	IslandImportedScales.Reset();
	IslandMaterialIndices.Reset();
	IslandNeutralHalfExtents.Reset();
	if (BaseVertices.Num() == 0 || MeshData.Sections.Num() == 0)
	{
		return;
	}

	const FVector Extent = BaseBounds.IsValid ? BaseBounds.GetExtent() : FVector(100.0);
	const float GridSize = static_cast<float>(FMath::Max3(Extent.X, Extent.Y, Extent.Z));
	const float WeldTolerance = FMath::Max(0.0001f, GridSize * 0.005f);
	const int32 TargetIslandCount = GetTargetIslandCount(MeshData);
	const TArray<int32> VertexMaterial = BuildVertexMaterialMap(MeshData);
	TArray<int32> BuiltVertexIsland;
	TArray<FVector> BuiltIslandCenters;
	TArray<int32> BuiltIslandMaterials;
	bool bUsedOrderedPath = false;
	if (!bForceConnectedComponentIslands)
	{
		bUsedOrderedPath = BuildOrderedCubeIslands(MeshData, VertexMaterial, BuiltVertexIsland, BuiltIslandCenters, BuiltIslandMaterials)
			&& BuiltIslandCenters.Num() > 0;
	}
	if (!bUsedOrderedPath)
	{
		if (!BuildConnectedIslandsForTolerance(MeshData, WeldTolerance, VertexMaterial, BuiltVertexIsland, BuiltIslandCenters, BuiltIslandMaterials) || BuiltIslandCenters.Num() == 0)
		{
			return;
		}
	}

	VertexIsland = MoveTemp(BuiltVertexIsland);
	IslandCenters = MoveTemp(BuiltIslandCenters);
	IslandMaterialIndices = MoveTemp(BuiltIslandMaterials);
	IslandPhases.SetNumZeroed(IslandCenters.Num());
	IslandScales.Init(1.0f, IslandCenters.Num());
	IslandImportedScales.Init(1.0f, IslandCenters.Num());
	RefreshIslandPhasesAndImportedScales();

	if (bLogIslandStatistics)
	{
		int32 AssignedVertexCount = 0;
		TArray<int32> VertexCountPerIsland;
		VertexCountPerIsland.Init(0, IslandCenters.Num());
		for (int32 Vertex = 0; Vertex < VertexIsland.Num(); ++Vertex)
		{
			const int32 Island = VertexIsland[Vertex];
			if (VertexCountPerIsland.IsValidIndex(Island))
			{
				++VertexCountPerIsland[Island];
				++AssignedVertexCount;
			}
		}

		int32 MinVerts = TNumericLimits<int32>::Max();
		int32 MaxVerts = 0;
		double SumVerts = 0.0;
		for (int32 Count : VertexCountPerIsland)
		{
			MinVerts = FMath::Min(MinVerts, Count);
			MaxVerts = FMath::Max(MaxVerts, Count);
			SumVerts += Count;
		}
		const float AverageVerts = VertexCountPerIsland.Num() > 0 ? static_cast<float>(SumVerts / VertexCountPerIsland.Num()) : 0.0f;
		const int32 UnassignedVerts = VertexIsland.Num() - AssignedVertexCount;
		const TCHAR* PathLabel = bUsedOrderedPath ? TEXT("OrderedCube") : TEXT("ConnectedComponents");

		UE_LOG(LogTemp, Display,
			TEXT("[GradientBoxMotion] verts=%d tris=%d islands=%d target=%d tolerance=%.4f extent=(%.2f, %.2f, %.2f) path=%s vertsPerIsland(min/avg/max)=%d/%.1f/%d unassigned=%d"),
			BaseVertices.Num(),
			MeshData.GetTriangleCount(),
			IslandCenters.Num(),
			TargetIslandCount,
			WeldTolerance,
			Extent.X,
			Extent.Y,
			Extent.Z,
			PathLabel,
			MinVerts == TNumericLimits<int32>::Max() ? 0 : MinVerts,
			AverageVerts,
			MaxVerts,
			UnassignedVerts);

		// Per-cube Blender export typically yields 24 face-corner verts per cube. If average is much
		// larger (e.g. hundreds), the algorithm has merged cubes into super-islands and animation
		// will deform geometry, producing flipped faces.
		if (AverageVerts > 64.0f)
		{
			UE_LOG(LogTemp, Warning,
				TEXT("[GradientBoxMotion] Average verts per island (%.1f) is much higher than expected (~24). Cubes likely merged into super-islands. Try enabling bForceConnectedComponentIslands or check Blender output (gap > weld tolerance)."),
				AverageVerts);
		}
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("[GradientBoxMotion] verts=%d tris=%d islands=%d target=%d tolerance=%.4f extent=(%.2f, %.2f, %.2f) path=%s"),
			BaseVertices.Num(),
			MeshData.GetTriangleCount(),
			IslandCenters.Num(),
			TargetIslandCount,
			WeldTolerance,
			Extent.X,
			Extent.Y,
			Extent.Z,
			bUsedOrderedPath ? TEXT("OrderedCube") : TEXT("ConnectedComponents"));
	}
}

void UBlenderGNGradientBoxMotionComponent::RefreshIslandPhasesAndImportedScales()
{
	if (IslandCenters.Num() == 0)
	{
		return;
	}

	const FVector Diagonal = FVector(1.0, 1.0, 1.0).GetSafeNormal();
	const float Shift = FMath::DegreesToRadians(CachedShiftAngleDegrees);
	const float ImportedTimePhase = CachedFrameInput * FMath::DegreesToRadians(CachedGnSpeedDegreesPerFrame);
	const float UnrealScale = BoundRuntime ? BoundRuntime->BlenderToUnrealScale : 1.0f;

	for (int32 Island = 0; Island < IslandCenters.Num(); ++Island)
	{
		// Convert back from UE's X,-Y,Z import convention before matching Unity's source-space wave.
		const FVector SourceCenter = UnrealToSourcePosition(IslandCenters[Island], UnrealScale);
		IslandPhases[Island] = static_cast<float>(FVector::DotProduct(SourceCenter, Diagonal)) * Shift;
		float Phase = ImportedTimePhase + IslandPhases[Island];
		if (bReversePinkAnimation && IsPinkMaterial(IslandMaterialIndices.IsValidIndex(Island) ? IslandMaterialIndices[Island] : 0))
		{
			Phase += PI;
		}
		IslandImportedScales[Island] = CalculateBlenderScale(Phase);
	}
}

void UBlenderGNGradientBoxMotionComponent::UpdateTickInterval()
{
	PrimaryComponentTick.TickInterval = 1.0f / FMath::Max(1.0f, MaxAnimationFrameRate);
}

void UBlenderGNGradientBoxMotionComponent::ApplyMotion(float WorldTimeSeconds)
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
	if (BaseVertices.Num() == 0 || VertexIsland.Num() != BaseVertices.Num())
	{
		return;
	}

	CacheRuntimeParameters();
	RefreshIslandPhasesAndImportedScales();

	const float SimulatedFrame = CachedFrameInput + WorldTimeSeconds * CachedBlenderFrameRate * FMath::Max(0.0f, Speed);
	const float TimePhase = SimulatedFrame * FMath::DegreesToRadians(CachedGnSpeedDegreesPerFrame);
	const float Blend = FMath::SmoothStep(0.0f, 1.0f, FMath::Clamp(Amplitude, 0.0f, 1.0f));
	const float SmoothedBlend = FMath::Lerp(Blend, FMath::SmoothStep(0.0f, 1.0f, Blend), FMath::Clamp(Smoothness, 0.0f, 1.0f));

	for (int32 Island = 0; Island < IslandCenters.Num(); ++Island)
	{
		float Phase = TimePhase + IslandPhases[Island];
		if (bReversePinkAnimation && IsPinkMaterial(IslandMaterialIndices.IsValidIndex(Island) ? IslandMaterialIndices[Island] : 0))
		{
			Phase += PI;
		}

		const float TargetScale = FMath::Lerp(MinimumScale, MaximumScale, CalculateBlenderScale(Phase));
		IslandScales[Island] = FMath::Lerp(1.0f, TargetScale, SmoothedBlend);
	}

	for (int32 VertexIndex = 0; VertexIndex < BaseVertices.Num(); ++VertexIndex)
	{
		const int32 Island = VertexIsland[VertexIndex];
		if (!IslandCenters.IsValidIndex(Island))
		{
			AnimatedVertices[VertexIndex] = BaseVertices[VertexIndex];
			continue;
		}

		AnimatedVertices[VertexIndex] = IslandCenters[Island] + GetNeutralDelta(VertexIndex, Island) * IslandScales[Island];
	}

	ApplyAnimatedVertices();
}

void UBlenderGNGradientBoxMotionComponent::ApplyMotionImmediately()
{
	const double Now = FPlatformTime::Seconds();
	if (MotionStartTime <= 0.0)
	{
		MotionStartTime = Now;
	}
	ApplyMotion(static_cast<float>(Now - MotionStartTime));
}

void UBlenderGNGradientBoxMotionComponent::CreateRuntimeControlPanel()
{
	if (!bCreateRuntimeControlPanel || RuntimeControlPanel || !GetWorld() || GetWorld()->IsNetMode(NM_DedicatedServer))
	{
		return;
	}

	APlayerController* Controller = GetWorld()->GetFirstPlayerController();
	if (!Controller)
	{
		return;
	}

	RuntimeControlPanel = CreateWidget<UBlenderGNRuntimePanelWidget>(Controller, UBlenderGNRuntimePanelWidget::StaticClass());
	if (!RuntimeControlPanel)
	{
		return;
	}

	RuntimeControlPanel->ResolveReferences(GetOwner());
	RuntimeControlPanel->AddToViewport(20);

	if (bShowMouseCursorForRuntimePanel)
	{
		Controller->bShowMouseCursor = true;
		Controller->bEnableClickEvents = true;
		Controller->bEnableMouseOverEvents = true;
		Controller->SetIgnoreLookInput(false);
		Controller->SetIgnoreMoveInput(false);
		UWidgetBlueprintLibrary::SetInputMode_GameAndUIEx(Controller, nullptr, EMouseLockMode::DoNotLock, false);
	}
}

void UBlenderGNGradientBoxMotionComponent::ApplyAnimatedVertices()
{
	if (!BoundRuntime)
	{
		return;
	}

	if (bUseFastDynamicMesh && !bRecalculateAnimatedNormals && ApplyAnimatedVerticesFast())
	{
		return;
	}

	DestroyFastSectionMeshes();
	if (UProceduralMeshComponent* ProceduralMesh = BoundRuntime->GetProceduralMesh())
	{
		ProceduralMesh->SetVisibility(true);
		ProceduralMesh->SetHiddenInGame(false);
	}

	if (bRecalculateAnimatedNormals)
	{
		BlenderGNProcMesh::RecalculateNormals(BoundRuntime->GetLastMeshData(), AnimatedVertices, AnimatedNormals);
		// The PMC convention used by this plugin is such that triangle winding produces an INWARD
		// cross-product after NormalizeTriangleWindingAndNormals. RecalculateNormals() blindly sums
		// CrossProduct(B-A, C-A), so its output is the opposite sign of the imported (outward)
		// normals. Align each recalculated normal back to the imported direction so lighting is
		// consistent regardless of how the cube has been deformed.
		const TArray<FVector>& ImportedNormals = BaseNormals;
		if (ImportedNormals.Num() == AnimatedNormals.Num())
		{
			for (int32 NormalIndex = 0; NormalIndex < AnimatedNormals.Num(); ++NormalIndex)
			{
				const FVector& Imported = ImportedNormals[NormalIndex];
				if (Imported.IsNearlyZero())
				{
					AnimatedNormals[NormalIndex] = -AnimatedNormals[NormalIndex];
					continue;
				}
				if (FVector::DotProduct(AnimatedNormals[NormalIndex], Imported) < 0.0)
				{
					AnimatedNormals[NormalIndex] = -AnimatedNormals[NormalIndex];
				}
			}
		}
		else
		{
			// Fallback: if we cannot align, flip wholesale — empirically the right sign for this PMC.
			for (FVector& Normal : AnimatedNormals)
			{
				Normal = -Normal;
			}
		}
		BlenderGNProcMesh::SmoothNormalsByGroups(NormalGroups, 35.0f, AnimatedNormals);
		BlenderGNProcMesh::ApplyVertices(BoundRuntime, AnimatedVertices, AnimatedNormals, ScratchVertexColors, ScratchTangents);
	}
	else
	{
		BlenderGNProcMesh::ApplyVertices(BoundRuntime, AnimatedVertices, AnimatedNormals, ScratchVertexColors, ScratchTangents);
	}
}

bool UBlenderGNGradientBoxMotionComponent::EnsureFastSectionMeshes()
{
	if (!BoundRuntime || !GetOwner() || !GetOwner()->GetRootComponent())
	{
		return false;
	}

	const FBlenderGNMeshData& MeshData = BoundRuntime->GetLastMeshData();
	if (MeshData.Vertices.Num() == 0 || MeshData.Sections.Num() == 0)
	{
		return false;
	}

	if (FastSectionMeshes.Num() == MeshData.Sections.Num())
	{
		bool bAllValid = true;
		for (const UBlenderGNFastDynamicMeshComponent* SectionMesh : FastSectionMeshes)
		{
			bAllValid &= IsValid(SectionMesh);
		}
		if (bAllValid)
		{
			if (UProceduralMeshComponent* ProceduralMesh = BoundRuntime->GetProceduralMesh())
			{
				ProceduralMesh->SetVisibility(false);
				ProceduralMesh->SetHiddenInGame(true);
			}
			for (UBlenderGNFastDynamicMeshComponent* SectionMesh : FastSectionMeshes)
			{
				if (IsValid(SectionMesh))
				{
					SectionMesh->SetVisibility(true);
					SectionMesh->SetHiddenInGame(false);
				}
			}
			return true;
		}
	}

	DestroyFastSectionMeshes();

	const TArray<FProcMeshTangent>& RuntimeTangents = BoundRuntime->GetLastTangents();
	FastSectionMeshes.SetNum(MeshData.Sections.Num());
	FastSectionGlobalVertices.SetNum(MeshData.Sections.Num());
	FastSectionAnimatedVertices.SetNum(MeshData.Sections.Num());

	for (int32 SectionIndex = 0; SectionIndex < MeshData.Sections.Num(); ++SectionIndex)
	{
		const FBlenderGNMeshSection& Section = MeshData.Sections[SectionIndex];
		if (Section.Triangles.Num() == 0)
		{
			continue;
		}

		// Skip sections with no named material and no explicit material override (empty/default material)
		const bool bHasNamedMaterial = MeshData.Materials.IsValidIndex(SectionIndex) && !MeshData.Materials[SectionIndex].Name.IsEmpty();
		const bool bHasMaterialOverride = !BoundRuntime->bForceDefaultMaterial && BoundRuntime->MaterialOverrides.IsValidIndex(SectionIndex) && BoundRuntime->MaterialOverrides[SectionIndex];
		if (!bHasNamedMaterial && !bHasMaterialOverride)
		{
			continue;
		}

		TArray<int32>& GlobalVertices = FastSectionGlobalVertices[SectionIndex];
		TMap<int32, int32> GlobalToLocal;
		GlobalVertices.Reserve(Section.Triangles.Num());
		GlobalToLocal.Reserve(Section.Triangles.Num());

		TArray<int32> LocalTriangles;
		LocalTriangles.Reserve(Section.Triangles.Num());
		auto AddLocalVertex = [&](int32 GlobalIndex) -> int32
		{
			if (const int32* Existing = GlobalToLocal.Find(GlobalIndex))
			{
				return *Existing;
			}

			const int32 LocalIndex = GlobalVertices.Num();
			GlobalToLocal.Add(GlobalIndex, LocalIndex);
			GlobalVertices.Add(GlobalIndex);
			return LocalIndex;
		};

		for (int32 GlobalIndex : Section.Triangles)
		{
			if (!MeshData.Vertices.IsValidIndex(GlobalIndex))
			{
				continue;
			}
			LocalTriangles.Add(AddLocalVertex(GlobalIndex));
		}

		if (GlobalVertices.Num() == 0 || LocalTriangles.Num() < 3)
		{
			continue;
		}

		TArray<FVector> SectionVertices;
		TArray<FVector> SectionNormals;
		TArray<FVector2D> SectionUVs;
		TArray<FColor> SectionColors;
		TArray<FProcMeshTangent> SectionTangents;
		SectionVertices.Reserve(GlobalVertices.Num());
		SectionNormals.Reserve(GlobalVertices.Num());
		SectionUVs.Reserve(GlobalVertices.Num());
		SectionColors.Reserve(GlobalVertices.Num());
		SectionTangents.Reserve(GlobalVertices.Num());

		for (int32 GlobalIndex : GlobalVertices)
		{
			SectionVertices.Add(MeshData.Vertices[GlobalIndex]);
			SectionNormals.Add(MeshData.Normals.IsValidIndex(GlobalIndex) ? MeshData.Normals[GlobalIndex] : FVector::UpVector);
			SectionUVs.Add(MeshData.UVs.IsValidIndex(GlobalIndex) ? MeshData.UVs[GlobalIndex] : FVector2D::ZeroVector);
			SectionColors.Add(FColor::White);
			SectionTangents.Add(RuntimeTangents.IsValidIndex(GlobalIndex) ? RuntimeTangents[GlobalIndex] : FProcMeshTangent(1.0f, 0.0f, 0.0f));
		}

		const FName ComponentName = MakeUniqueObjectName(GetOwner(), UBlenderGNFastDynamicMeshComponent::StaticClass(), *FString::Printf(TEXT("GradientBoxFastSection_%02d"), SectionIndex));
		UBlenderGNFastDynamicMeshComponent* SectionMesh = NewObject<UBlenderGNFastDynamicMeshComponent>(GetOwner(), ComponentName, RF_Transient);
		if (!SectionMesh)
		{
			continue;
		}

		SectionMesh->SetupAttachment(GetOwner()->GetRootComponent());
		SectionMesh->InitMesh(SectionVertices, LocalTriangles, SectionNormals, SectionUVs, SectionColors, SectionTangents);
		SectionMesh->SetCastShadow(BoundRuntime->bCastShadows);
		if (BoundRuntime->bForceDefaultMaterial)
		{
			if (UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface))
			{
				SectionMesh->SetMaterial(0, DefaultMaterial);
			}
		}
		else if (BoundRuntime->MaterialOverrides.IsValidIndex(SectionIndex) && BoundRuntime->MaterialOverrides[SectionIndex])
		{
			SectionMesh->SetMaterial(0, BoundRuntime->MaterialOverrides[SectionIndex]);
		}
		else if (BoundRuntime->FallbackMaterial)
		{
			SectionMesh->SetMaterial(0, BoundRuntime->FallbackMaterial);
		}
		SectionMesh->SetVisibility(true);
		SectionMesh->SetHiddenInGame(false);
		SectionMesh->RegisterComponent();
		FastSectionMeshes[SectionIndex] = SectionMesh;
		FastSectionAnimatedVertices[SectionIndex].SetNumUninitialized(GlobalVertices.Num());
	}

	if (UProceduralMeshComponent* ProceduralMesh = BoundRuntime->GetProceduralMesh())
	{
		if (FastSectionMeshes.ContainsByPredicate([](const UBlenderGNFastDynamicMeshComponent* Mesh)
		{
			return IsValid(Mesh);
		}))
		{
			ProceduralMesh->SetVisibility(false);
			ProceduralMesh->SetHiddenInGame(true);
		}
		else
		{
			ProceduralMesh->SetVisibility(true);
			ProceduralMesh->SetHiddenInGame(false);
		}
	}

	return FastSectionMeshes.ContainsByPredicate([](const UBlenderGNFastDynamicMeshComponent* Mesh)
	{
		return IsValid(Mesh);
	});
}

void UBlenderGNGradientBoxMotionComponent::DestroyFastSectionMeshes()
{
	for (UBlenderGNFastDynamicMeshComponent* SectionMesh : FastSectionMeshes)
	{
		if (IsValid(SectionMesh))
		{
			SectionMesh->DestroyComponent();
		}
	}
	FastSectionMeshes.Reset();
	FastSectionGlobalVertices.Reset();
	FastSectionAnimatedVertices.Reset();
}

bool UBlenderGNGradientBoxMotionComponent::ApplyAnimatedVerticesFast()
{
	if (!EnsureFastSectionMeshes())
	{
		return false;
	}

	for (int32 SectionIndex = 0; SectionIndex < FastSectionMeshes.Num(); ++SectionIndex)
	{
		UBlenderGNFastDynamicMeshComponent* SectionMesh = FastSectionMeshes[SectionIndex];
		if (!IsValid(SectionMesh) || !FastSectionGlobalVertices.IsValidIndex(SectionIndex) || !FastSectionAnimatedVertices.IsValidIndex(SectionIndex))
		{
			continue;
		}

		const TArray<int32>& GlobalVertices = FastSectionGlobalVertices[SectionIndex];
		TArray<FVector>& SectionVertices = FastSectionAnimatedVertices[SectionIndex];
		if (SectionVertices.Num() != GlobalVertices.Num())
		{
			SectionVertices.SetNumUninitialized(GlobalVertices.Num());
		}

		for (int32 LocalIndex = 0; LocalIndex < GlobalVertices.Num(); ++LocalIndex)
		{
			const int32 GlobalIndex = GlobalVertices[LocalIndex];
			SectionVertices[LocalIndex] = AnimatedVertices.IsValidIndex(GlobalIndex) ? AnimatedVertices[GlobalIndex] : BaseVertices[GlobalIndex];
		}

		SectionMesh->UpdatePositionsOnly(SectionVertices);
	}

	return true;
}

int32 UBlenderGNGradientBoxMotionComponent::GetTargetIslandCount(const FBlenderGNMeshData& MeshData) const
{
	const int32 TriangleCubeEstimate = MeshData.GetTriangleCount() / 12;
	if (TriangleCubeEstimate > 0)
	{
		return TriangleCubeEstimate;
	}

	const int32 Num = BoundRuntime ? BoundRuntime->GetIntInput(TEXT("Socket_2"), BoundRuntime->GetIntInput(TEXT("num"), 10)) : 10;
	int32 MaterialSections = 0;
	for (int32 SectionIndex = 0; SectionIndex < MeshData.Sections.Num(); ++SectionIndex)
	{
		if (MeshData.Sections[SectionIndex].Triangles.Num() > 0)
		{
			++MaterialSections;
		}
	}
	return FMath::Max(1, Num * Num * Num * FMath::Clamp(MaterialSections, 1, 2));
}

TArray<int32> UBlenderGNGradientBoxMotionComponent::BuildVertexMaterialMap(const FBlenderGNMeshData& MeshData) const
{
	TArray<int32> VertexMaterial;
	VertexMaterial.Init(0, BaseVertices.Num());
	for (int32 SectionIndex = 0; SectionIndex < MeshData.Sections.Num(); ++SectionIndex)
	{
		for (int32 VertexIndex : MeshData.Sections[SectionIndex].Triangles)
		{
			if (VertexMaterial.IsValidIndex(VertexIndex))
			{
				VertexMaterial[VertexIndex] = SectionIndex;
			}
		}
	}
	return VertexMaterial;
}

TArray<int32> UBlenderGNGradientBoxMotionComponent::BuildWeldMap(float Tolerance, const TArray<int32>& VertexMaterial) const
{
	TArray<int32> WeldedId;
	WeldedId.SetNumUninitialized(BaseVertices.Num());

	TMap<FIntVector, TArray<int32>> Buckets;
	Buckets.Reserve(BaseVertices.Num());
	const float ToleranceSqr = Tolerance * Tolerance;
	int32 NextId = 0;

	for (int32 VertexIndex = 0; VertexIndex < BaseVertices.Num(); ++VertexIndex)
	{
		const FVector& Position = BaseVertices[VertexIndex];
		const FIntVector Cell = QuantizeGradientMotionCell(Position, Tolerance);
		int32 Found = INDEX_NONE;

		for (int32 X = -1; X <= 1 && Found == INDEX_NONE; ++X)
		{
			for (int32 Y = -1; Y <= 1 && Found == INDEX_NONE; ++Y)
			{
				for (int32 Z = -1; Z <= 1 && Found == INDEX_NONE; ++Z)
				{
					const FIntVector Neighbor(Cell.X + X, Cell.Y + Y, Cell.Z + Z);
					const TArray<int32>* Candidates = Buckets.Find(Neighbor);
					if (!Candidates)
					{
						continue;
					}

					for (int32 Candidate : *Candidates)
					{
						if (VertexMaterial.IsValidIndex(VertexIndex) &&
							VertexMaterial.IsValidIndex(Candidate) &&
							VertexMaterial[VertexIndex] != VertexMaterial[Candidate])
						{
							continue;
						}

						if (FVector::DistSquared(BaseVertices[Candidate], Position) <= ToleranceSqr)
						{
							Found = WeldedId[Candidate];
							break;
						}
					}
				}
			}
		}

		if (Found == INDEX_NONE)
		{
			Found = NextId++;
			Buckets.FindOrAdd(Cell).Add(VertexIndex);
		}

		WeldedId[VertexIndex] = Found;
	}

	return WeldedId;
}

bool UBlenderGNGradientBoxMotionComponent::BuildOrderedCubeIslands(const FBlenderGNMeshData& MeshData, const TArray<int32>& VertexMaterial, TArray<int32>& OutVertexIsland, TArray<FVector>& OutIslandCenters, TArray<int32>& OutIslandMaterials) const
{
	constexpr int32 CubeTriangleIndexCount = 36;
	// Per-cube unique vertex count depends on how Blender exports the cube:
	//   8  - shared corners (merge-by-distance applied across the whole cube)
	//   24 - face-corner verts (per-face split, default for hard normals)
	//   36 - per-triangle split (no sharing at all; common when GN realizes instances without dedup)
	// We accept anything up to 40 unique to cover all three; rejecting at 28 wrongly forced the
	// connected-components fallback for the GradientBox GN graph (it emits 36 verts per cube).
	constexpr int32 MaxUniqueVerticesPerCube = 40;
	// Span: a single cube's 36 raw indices should be contiguous (Blender outputs cubes one after
	// another). 64 leaves slack for any minor gaps Blender might introduce between instances.
	constexpr int32 MaxVertexIndexSpanPerCube = 64;
	if (MeshData.Triangles.Num() == 0 || MeshData.Triangles.Num() % CubeTriangleIndexCount != 0)
	{
		return false;
	}

	// Sanity-check ordering: if MeshData.Triangles was reconstructed from per-material sections
	// (e.g. via RestoreLastMeshDataFromProceduralMesh), chunks of 36 indices will straddle multiple
	// real cubes and produce broken islands. Each chunk should reference a tight, contiguous block
	// of vertex indices that all share one material. Detect that up-front and bail out so the caller
	// can fall back to BuildConnectedIslandsForTolerance, matching Unity's behavior.
	for (int32 ChunkStart = 0; ChunkStart + CubeTriangleIndexCount <= MeshData.Triangles.Num(); ChunkStart += CubeTriangleIndexCount)
	{
		int32 MinIndex = TNumericLimits<int32>::Max();
		int32 MaxIndex = TNumericLimits<int32>::Min();
		int32 ChunkMaterial = INDEX_NONE;
		bool bMaterialIsConsistent = true;
		TSet<int32> UniqueIndices;
		UniqueIndices.Reserve(CubeTriangleIndexCount);
		for (int32 Offset = 0; Offset < CubeTriangleIndexCount; ++Offset)
		{
			const int32 VertexIndex = MeshData.Triangles[ChunkStart + Offset];
			if (!BaseVertices.IsValidIndex(VertexIndex))
			{
				continue;
			}
			MinIndex = FMath::Min(MinIndex, VertexIndex);
			MaxIndex = FMath::Max(MaxIndex, VertexIndex);
			UniqueIndices.Add(VertexIndex);
			if (VertexMaterial.IsValidIndex(VertexIndex))
			{
				const int32 Material = VertexMaterial[VertexIndex];
				if (ChunkMaterial == INDEX_NONE)
				{
					ChunkMaterial = Material;
				}
				else if (ChunkMaterial != Material)
				{
					bMaterialIsConsistent = false;
				}
			}
		}
		if (MaxIndex < MinIndex)
		{
			return false;
		}
		const int32 IndexSpan = MaxIndex - MinIndex;
		if (IndexSpan >= MaxVertexIndexSpanPerCube
			|| UniqueIndices.Num() > MaxUniqueVerticesPerCube
			|| !bMaterialIsConsistent)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GradientBoxMotion] Ordered cube heuristic rejected (span=%d unique=%d materialConsistent=%d). Falling back to connected-components."),
				IndexSpan,
				UniqueIndices.Num(),
				bMaterialIsConsistent ? 1 : 0);
			return false;
		}
	}

	OutVertexIsland.Init(INDEX_NONE, BaseVertices.Num());
	OutIslandCenters.Reset();
	OutIslandMaterials.Reset();

	for (int32 ChunkStart = 0; ChunkStart + CubeTriangleIndexCount <= MeshData.Triangles.Num(); ChunkStart += CubeTriangleIndexCount)
	{
		const int32 Island = OutIslandCenters.Num();
		FVector Sum = FVector::ZeroVector;
		int32 Count = 0;
		TMap<int32, int32> MaterialVotes;

		for (int32 Offset = 0; Offset < CubeTriangleIndexCount; ++Offset)
		{
			const int32 VertexIndex = MeshData.Triangles[ChunkStart + Offset];
			if (!BaseVertices.IsValidIndex(VertexIndex))
			{
				continue;
			}

			OutVertexIsland[VertexIndex] = Island;
			Sum += BaseVertices[VertexIndex];
			++Count;
			if (VertexMaterial.IsValidIndex(VertexIndex))
			{
				int32& VoteCount = MaterialVotes.FindOrAdd(VertexMaterial[VertexIndex], 0);
				++VoteCount;
			}
		}

		if (Count == 0)
		{
			continue;
		}

		int32 BestMaterial = 0;
		int32 BestVotes = 0;
		for (const TPair<int32, int32>& Pair : MaterialVotes)
		{
			if (Pair.Value > BestVotes || (Pair.Value == BestVotes && Pair.Key > BestMaterial))
			{
				BestVotes = Pair.Value;
				BestMaterial = Pair.Key;
			}
		}

		OutIslandCenters.Add(Sum / static_cast<double>(Count));
		OutIslandMaterials.Add(BestMaterial);
	}

	return OutIslandCenters.Num() > 0;
}

bool UBlenderGNGradientBoxMotionComponent::BuildConnectedIslandsForTolerance(const FBlenderGNMeshData& MeshData, float Tolerance, const TArray<int32>& VertexMaterial, TArray<int32>& OutVertexIsland, TArray<FVector>& OutIslandCenters, TArray<int32>& OutIslandMaterials) const
{
	OutVertexIsland.Init(INDEX_NONE, BaseVertices.Num());
	OutIslandCenters.Reset();
	OutIslandMaterials.Reset();

	const TArray<int32> WeldedId = BuildWeldMap(Tolerance, VertexMaterial);
	TArray<int32> TrianglesStorage;
	const TArray<int32>* TrianglesPtr = &MeshData.Triangles;
	if (MeshData.Triangles.Num() == 0)
	{
		for (const FBlenderGNMeshSection& Section : MeshData.Sections)
		{
			TrianglesStorage.Append(Section.Triangles);
		}
		TrianglesPtr = &TrianglesStorage;
	}

	const TArray<int32>& Triangles = *TrianglesPtr;
	const int32 TriangleCount = Triangles.Num() / 3;
	if (TriangleCount == 0)
	{
		return false;
	}

	struct FTriangleEdgeRef
	{
		int32 Triangle = INDEX_NONE;
		int32 RawA = INDEX_NONE;
		int32 RawB = INDEX_NONE;
	};

	auto MakeEdgeKey = [](int32 A, int32 B) -> uint64
	{
		const uint32 MinId = static_cast<uint32>(FMath::Min(A, B));
		const uint32 MaxId = static_cast<uint32>(FMath::Max(A, B));
		return (static_cast<uint64>(MinId) << 32) | static_cast<uint64>(MaxId);
	};

	TArray<FVector> TriangleNormals;
	TriangleNormals.Init(FVector::ZeroVector, TriangleCount);
	TArray<float> EdgeLengths;
	EdgeLengths.Reserve(TriangleCount * 3);
	TMap<uint64, TArray<FTriangleEdgeRef>> EdgeToTriangles;
	EdgeToTriangles.Reserve(TriangleCount * 3);

	for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
	{
		const int32 Offset = TriangleIndex * 3;
		const int32 RawA = Triangles[Offset];
		const int32 RawB = Triangles[Offset + 1];
		const int32 RawC = Triangles[Offset + 2];
		if (!BaseVertices.IsValidIndex(RawA) || !BaseVertices.IsValidIndex(RawB) || !BaseVertices.IsValidIndex(RawC) ||
			!WeldedId.IsValidIndex(RawA) || !WeldedId.IsValidIndex(RawB) || !WeldedId.IsValidIndex(RawC))
		{
			continue;
		}

		const FVector AB = BaseVertices[RawB] - BaseVertices[RawA];
		const FVector AC = BaseVertices[RawC] - BaseVertices[RawA];
		TriangleNormals[TriangleIndex] = FVector::CrossProduct(AB, AC).GetSafeNormal();

		const int32 RawEdges[3][2] = { { RawA, RawB }, { RawB, RawC }, { RawC, RawA } };
		for (const int32 (&RawEdge)[2] : RawEdges)
		{
			const int32 WeldA = WeldedId[RawEdge[0]];
			const int32 WeldB = WeldedId[RawEdge[1]];
			if (WeldA == WeldB)
			{
				continue;
			}

			const float EdgeLength = static_cast<float>(FVector::Distance(BaseVertices[RawEdge[0]], BaseVertices[RawEdge[1]]));
			EdgeLengths.Add(EdgeLength);
			EdgeToTriangles.FindOrAdd(MakeEdgeKey(WeldA, WeldB)).Add({ TriangleIndex, RawEdge[0], RawEdge[1] });
		}
	}

	EdgeLengths.Sort();
	const float MedianEdgeLength = EdgeLengths.Num() > 0 ? EdgeLengths[EdgeLengths.Num() / 2] : EstimateAverageEdgeLength(MeshData);
	const float SameNormalDiagonalThreshold = MedianEdgeLength * 1.18f;

	TArray<TArray<int32>> Adjacency;
	Adjacency.SetNum(TriangleCount);
	for (const TPair<uint64, TArray<FTriangleEdgeRef>>& Pair : EdgeToTriangles)
	{
		const TArray<FTriangleEdgeRef>& EdgeRefs = Pair.Value;
		for (int32 I = 0; I < EdgeRefs.Num(); ++I)
		{
			for (int32 J = I + 1; J < EdgeRefs.Num(); ++J)
			{
				const int32 TriangleA = EdgeRefs[I].Triangle;
				const int32 TriangleB = EdgeRefs[J].Triangle;
				if (!TriangleNormals.IsValidIndex(TriangleA) || !TriangleNormals.IsValidIndex(TriangleB))
				{
					continue;
				}

				const float NormalDot = static_cast<float>(FVector::DotProduct(TriangleNormals[TriangleA], TriangleNormals[TriangleB]));
				if (NormalDot < -0.25f)
				{
					continue;
				}

				const float EdgeLength = static_cast<float>(FVector::Distance(BaseVertices[EdgeRefs[I].RawA], BaseVertices[EdgeRefs[I].RawB]));
				if (NormalDot > 0.85f && EdgeLength < SameNormalDiagonalThreshold)
				{
					continue;
				}

				AddGradientMotionEdge(Adjacency, TriangleA, TriangleB);
			}
		}
	}

	TArray<int32> TriangleIsland;
	TriangleIsland.Init(INDEX_NONE, TriangleCount);
	TQueue<int32> Queue;
	for (int32 Start = 0; Start < TriangleCount; ++Start)
	{
		if (TriangleIsland[Start] != INDEX_NONE)
		{
			continue;
		}

		const int32 Island = OutIslandCenters.Num();
		TriangleIsland[Start] = Island;
		Queue.Enqueue(Start);
		int32 Current = INDEX_NONE;
		while (Queue.Dequeue(Current))
		{
			for (int32 Next : Adjacency[Current])
			{
				if (TriangleIsland.IsValidIndex(Next) && TriangleIsland[Next] == INDEX_NONE)
				{
					TriangleIsland[Next] = Island;
					Queue.Enqueue(Next);
				}
			}
		}
		OutIslandCenters.Add(FVector::ZeroVector);
		OutIslandMaterials.Add(0);
	}

	for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
	{
		const int32 Island = TriangleIsland[TriangleIndex];
		if (!OutIslandCenters.IsValidIndex(Island))
		{
			continue;
		}

		const int32 Offset = TriangleIndex * 3;
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			const int32 VertexIndex = Triangles[Offset + Corner];
			if (OutVertexIsland.IsValidIndex(VertexIndex) && OutVertexIsland[VertexIndex] == INDEX_NONE)
			{
				OutVertexIsland[VertexIndex] = Island;
			}
		}
	}

	TArray<int32> Counts;
	Counts.Init(0, OutIslandCenters.Num());
	TArray<TMap<int32, int32>> MaterialVotes;
	MaterialVotes.SetNum(OutIslandCenters.Num());
	for (int32 VertexIndex = 0; VertexIndex < BaseVertices.Num(); ++VertexIndex)
	{
		const int32 Island = OutVertexIsland[VertexIndex];
		if (!OutIslandCenters.IsValidIndex(Island))
		{
			continue;
		}

		OutVertexIsland[VertexIndex] = Island;
		OutIslandCenters[Island] += BaseVertices[VertexIndex];
		++Counts[Island];
		if (VertexMaterial.IsValidIndex(VertexIndex))
		{
			int32& VoteCount = MaterialVotes[Island].FindOrAdd(VertexMaterial[VertexIndex], 0);
			++VoteCount;
		}
	}

	// Determine island material by majority vote
	for (int32 Island = 0; Island < OutIslandCenters.Num(); ++Island)
	{
		int32 BestMaterial = 0;
		int32 BestVotes = 0;
		for (const TPair<int32, int32>& Pair : MaterialVotes[Island])
		{
			if (Pair.Value > BestVotes || (Pair.Value == BestVotes && Pair.Key > BestMaterial))
			{
				BestVotes = Pair.Value;
				BestMaterial = Pair.Key;
			}
		}
		OutIslandMaterials[Island] = BestMaterial;
	}

	for (int32 Island = 0; Island < OutIslandCenters.Num(); ++Island)
	{
		OutIslandCenters[Island] /= static_cast<double>(FMath::Max(1, Counts[Island]));
	}

	for (int32 VertexIndex = 0; VertexIndex < OutVertexIsland.Num(); ++VertexIndex)
	{
		if (OutVertexIsland[VertexIndex] != INDEX_NONE)
		{
			continue;
		}

		const int32 Island = OutIslandCenters.Num();
		OutVertexIsland[VertexIndex] = Island;
		OutIslandCenters.Add(BaseVertices[VertexIndex]);
		OutIslandMaterials.Add(0);
	}
	return OutIslandCenters.Num() > 0;
}

float UBlenderGNGradientBoxMotionComponent::ScoreIslandCandidate(int32 IslandCount, int32 TargetIslandCount) const
{
	if (IslandCount <= 0)
	{
		return TNumericLimits<float>::Max();
	}
	const float CountPenalty = FMath::Abs(static_cast<float>(IslandCount - TargetIslandCount)) / FMath::Max(1.0f, static_cast<float>(TargetIslandCount));
	const float UnderMergePenalty = IslandCount < TargetIslandCount ? 3.0f : 0.0f;
	const float TinyOverSplitPenalty = FMath::Max(0.0f, static_cast<float>(IslandCount - TargetIslandCount * 3)) / FMath::Max(1.0f, static_cast<float>(TargetIslandCount));
	return CountPenalty + UnderMergePenalty + TinyOverSplitPenalty;
}

float UBlenderGNGradientBoxMotionComponent::EstimateAverageEdgeLength(const FBlenderGNMeshData& MeshData) const
{
	double Sum = 0.0;
	int32 Count = 0;
	for (const FBlenderGNMeshSection& Section : MeshData.Sections)
	{
		const int32 MaxIndex = FMath::Min(Section.Triangles.Num(), 3000);
		for (int32 Index = 0; Index + 2 < MaxIndex; Index += 3)
		{
			const int32 A = Section.Triangles[Index];
			const int32 B = Section.Triangles[Index + 1];
			const int32 C = Section.Triangles[Index + 2];
			if (!BaseVertices.IsValidIndex(A) || !BaseVertices.IsValidIndex(B) || !BaseVertices.IsValidIndex(C))
			{
				continue;
			}
			Sum += FVector::Distance(BaseVertices[A], BaseVertices[B]);
			Sum += FVector::Distance(BaseVertices[B], BaseVertices[C]);
			Sum += FVector::Distance(BaseVertices[C], BaseVertices[A]);
			Count += 3;
		}
	}
	return Count > 0 ? static_cast<float>(Sum / Count) : 1.0f;
}

FVector UBlenderGNGradientBoxMotionComponent::EstimateNeutralHalfExtents() const
{
	if (BaseVertices.Num() == 0 || VertexIsland.Num() != BaseVertices.Num() || IslandImportedScales.Num() == 0)
	{
		return FVector(1.0);
	}

	TArray<FVector> IslandExtents;
	IslandExtents.Init(FVector::ZeroVector, IslandImportedScales.Num());
	for (int32 VertexIndex = 0; VertexIndex < BaseVertices.Num(); ++VertexIndex)
	{
		const int32 Island = VertexIsland[VertexIndex];
		if (!IslandExtents.IsValidIndex(Island) || !IslandCenters.IsValidIndex(Island))
		{
			continue;
		}
		IslandExtents[Island] = MaxVector(IslandExtents[Island], AbsVector(BaseVertices[VertexIndex] - IslandCenters[Island]));
	}

	TArray<float> Xs;
	TArray<float> Ys;
	TArray<float> Zs;
	for (int32 Island = 0; Island < IslandExtents.Num(); ++Island)
	{
		const float ImportedScale = IslandImportedScales.IsValidIndex(Island) ? IslandImportedScales[Island] : 1.0f;
		if (ImportedScale < ReliableImportedScale)
		{
			continue;
		}

		const FVector Candidate = IslandExtents[Island] / ImportedScale;
		if (Candidate.X > 0.0001f) { Xs.Add(static_cast<float>(Candidate.X)); }
		if (Candidate.Y > 0.0001f) { Ys.Add(static_cast<float>(Candidate.Y)); }
		if (Candidate.Z > 0.0001f) { Zs.Add(static_cast<float>(Candidate.Z)); }
	}

	const float Fallback = FMath::Max(1.0f, static_cast<float>(BaseBounds.GetExtent().Size()) * 0.02f);
	return FVector(
		MedianOrFallback(Xs, Fallback),
		MedianOrFallback(Ys, Fallback),
		MedianOrFallback(Zs, Fallback));
}

TArray<FVector> UBlenderGNGradientBoxMotionComponent::EstimateIslandNeutralHalfExtents() const
{
	TArray<FVector> Result;
	Result.Init(NeutralHalfExtents, IslandCenters.Num());
	if (BaseVertices.Num() == 0 || VertexIsland.Num() != BaseVertices.Num() || IslandImportedScales.Num() == 0)
	{
		return Result;
	}

	TArray<FVector> IslandExtents;
	IslandExtents.Init(FVector::ZeroVector, IslandImportedScales.Num());
	for (int32 VertexIndex = 0; VertexIndex < BaseVertices.Num(); ++VertexIndex)
	{
		const int32 Island = VertexIsland[VertexIndex];
		if (!IslandExtents.IsValidIndex(Island) || !IslandCenters.IsValidIndex(Island))
		{
			continue;
		}
		IslandExtents[Island] = MaxVector(IslandExtents[Island], AbsVector(BaseVertices[VertexIndex] - IslandCenters[Island]));
	}

	TMap<int32, TArray<float>> XsByMaterial;
	TMap<int32, TArray<float>> YsByMaterial;
	TMap<int32, TArray<float>> ZsByMaterial;
	for (int32 Island = 0; Island < IslandExtents.Num(); ++Island)
	{
		const float ImportedScale = IslandImportedScales.IsValidIndex(Island) ? IslandImportedScales[Island] : 1.0f;
		if (ImportedScale < ReliableImportedScale)
		{
			continue;
		}

		const int32 Material = IslandMaterialIndices.IsValidIndex(Island) ? IslandMaterialIndices[Island] : 0;
		const FVector Candidate = IslandExtents[Island] / ImportedScale;
		if (Candidate.X > 0.0001f) { XsByMaterial.FindOrAdd(Material).Add(static_cast<float>(Candidate.X)); }
		if (Candidate.Y > 0.0001f) { YsByMaterial.FindOrAdd(Material).Add(static_cast<float>(Candidate.Y)); }
		if (Candidate.Z > 0.0001f) { ZsByMaterial.FindOrAdd(Material).Add(static_cast<float>(Candidate.Z)); }
	}

	TMap<int32, FVector> ExtentsByMaterial;
	for (const TPair<int32, TArray<float>>& Pair : XsByMaterial)
	{
		TArray<float> Xs = Pair.Value;
		TArray<float> Ys = YsByMaterial.FindRef(Pair.Key);
		TArray<float> Zs = ZsByMaterial.FindRef(Pair.Key);
		ExtentsByMaterial.Add(Pair.Key, FVector(
			MedianOrFallback(Xs, static_cast<float>(NeutralHalfExtents.X)),
			MedianOrFallback(Ys, static_cast<float>(NeutralHalfExtents.Y)),
			MedianOrFallback(Zs, static_cast<float>(NeutralHalfExtents.Z))));
	}

	for (int32 Island = 0; Island < Result.Num(); ++Island)
	{
		const int32 Material = IslandMaterialIndices.IsValidIndex(Island) ? IslandMaterialIndices[Island] : 0;
		if (const FVector* MaterialExtents = ExtentsByMaterial.Find(Material))
		{
			Result[Island] = *MaterialExtents;
		}
	}
	return Result;
}

FVector UBlenderGNGradientBoxMotionComponent::GetNeutralHalfExtentsForIsland(int32 Island) const
{
	return IslandNeutralHalfExtents.IsValidIndex(Island) ? IslandNeutralHalfExtents[Island] : NeutralHalfExtents;
}

FVector UBlenderGNGradientBoxMotionComponent::GetNeutralDelta(int32 VertexIndex, int32 Island) const
{
	const FVector Delta = BaseVertices[VertexIndex] - IslandCenters[Island];
	const FVector HalfExtents = GetNeutralHalfExtentsForIsland(Island);
	const FVector Limits = HalfExtents * NeutralClampPadding;
	const float ImportedScale = IslandImportedScales.IsValidIndex(Island) ? IslandImportedScales[Island] : 1.0f;

	FVector NeutralDelta = ImportedScale >= ReliableImportedScale
		? Delta / ImportedScale
		: BuildFallbackNeutralDelta(VertexIndex, Island, Delta);

	if (!IsUsableNeutralDelta(NeutralDelta, Limits))
	{
		NeutralDelta = BuildFallbackNeutralDelta(VertexIndex, Island, Delta);
	}

	return FVector(
		FMath::Clamp(NeutralDelta.X, -Limits.X, Limits.X),
		FMath::Clamp(NeutralDelta.Y, -Limits.Y, Limits.Y),
		FMath::Clamp(NeutralDelta.Z, -Limits.Z, Limits.Z));
}

FVector UBlenderGNGradientBoxMotionComponent::BuildFallbackNeutralDelta(int32 VertexIndex, int32 Island, const FVector& Delta) const
{
	const FVector HalfExtents = GetNeutralHalfExtentsForIsland(Island);
	const FVector Normal = BaseNormals.IsValidIndex(VertexIndex) ? BaseNormals[VertexIndex] : FVector::ZeroVector;
	return FVector(
		SignedExtent(static_cast<float>(Delta.X), static_cast<float>(Normal.X), static_cast<float>(HalfExtents.X)),
		SignedExtent(static_cast<float>(Delta.Y), static_cast<float>(Normal.Y), static_cast<float>(HalfExtents.Y)),
		SignedExtent(static_cast<float>(Delta.Z), static_cast<float>(Normal.Z), static_cast<float>(HalfExtents.Z)));
}

float UBlenderGNGradientBoxMotionComponent::CalculateBlenderScale(float Phase) const
{
	const float Mapped = FMath::Clamp((FMath::Sin(Phase) + 1.0f) * 0.5f, 0.0f, 1.0f);
	return FMath::Pow(Mapped, FMath::Max(0.1f, CachedPower));
}

bool UBlenderGNGradientBoxMotionComponent::IsPinkMaterial(int32 MaterialIndex) const
{
	if (!BoundRuntime)
	{
		return MaterialIndex >= 2;
	}

	const FBlenderGNMeshData& MeshData = BoundRuntime->GetLastMeshData();
	if (MeshData.Materials.IsValidIndex(MaterialIndex))
	{
		const FString& Name = MeshData.Materials[MaterialIndex].Name;
		if (Name.Contains(TEXT("Pink"), ESearchCase::IgnoreCase))
		{
			return true;
		}
		if (Name.Contains(TEXT("Blue"), ESearchCase::IgnoreCase))
		{
			return false;
		}

		// Check material override for nameless/empty materials
		if (Name.IsEmpty() && BoundRuntime->MaterialOverrides.IsValidIndex(MaterialIndex))
		{
			if (UMaterialInterface* OverrideMat = BoundRuntime->MaterialOverrides[MaterialIndex])
			{
				const FString MatName = OverrideMat->GetName();
				if (MatName.Contains(TEXT("Pink"), ESearchCase::IgnoreCase))
				{
					return true;
				}
				if (MatName.Contains(TEXT("Blue"), ESearchCase::IgnoreCase))
				{
					return false;
				}
			}
		}
	}

	return MaterialIndex >= 2;
}
