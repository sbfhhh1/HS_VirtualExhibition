#include "BlenderGeometryNodesComponent.h"

#include "BlenderGNGradientBoxMotionComponent.h"
#include "BlenderGNProceduralMeshUtils.h"
#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Interfaces/IPluginManager.h"
#include "JsonObjectConverter.h"
#include "Materials/Material.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
constexpr int32 HexSphereRuntimeInputCount = 5;

const TCHAR* HexSphereRuntimeInputNames[HexSphereRuntimeInputCount] =
{
	TEXT("Hex Subdivisions"),
	TEXT("Tile Gap"),
	TEXT("Breath Amplitude"),
	TEXT("Breath Speed"),
	TEXT("Breath Noise Scale")
};

bool IsHexSphereRuntimeInputSet(const TArray<FBlenderGNInput>& Inputs)
{
	if (Inputs.Num() != HexSphereRuntimeInputCount)
	{
		return false;
	}

	int32 HexSubdivisionNameCount = 0;
	for (const FBlenderGNInput& Input : Inputs)
	{
		if (Input.SocketName.Equals(TEXT("Hex Subdivisions"), ESearchCase::IgnoreCase) ||
			Input.DisplayName.Equals(TEXT("Hex Subdivisions"), ESearchCase::IgnoreCase))
		{
			++HexSubdivisionNameCount;
		}
	}

	return HexSubdivisionNameCount > 0;
}
}

UBlenderGeometryNodesComponent::UBlenderGeometryNodesComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
#if WITH_EDITOR
	bTickInEditor = true;
#endif
	EnsureDefaults();
}

void UBlenderGeometryNodesComponent::EnsureDefaults()
{
	if (BlenderExecutable.IsEmpty())
	{
		BlenderExecutable = FindDefaultBlenderPath();
	}
	if (SidecarScriptPath.IsEmpty())
	{
		SidecarScriptPath = GetDefaultSidecarPath();
	}
	if (Inputs.Num() == 0)
	{
		FBlenderGNInput Subdivisions;
		Subdivisions.DisplayName = TEXT("Hex Subdivisions");
		Subdivisions.SocketName = TEXT("Hex Subdivisions");
		Subdivisions.Type = EBlenderGNParameterType::Int;
		Subdivisions.IntValue = 2;
		Subdivisions.SliderMin = 1.0f;
		Subdivisions.SliderMax = 2.0f;
		Inputs.Add(Subdivisions);

		FBlenderGNInput TileGap;
		TileGap.DisplayName = TEXT("Tile Gap");
		TileGap.SocketName = TEXT("Tile Gap");
		TileGap.Type = EBlenderGNParameterType::Float;
		TileGap.FloatValue = 0.42f;
		TileGap.SliderMin = 0.0f;
		TileGap.SliderMax = 0.72f;
		Inputs.Add(TileGap);

		FBlenderGNInput BreathAmplitude;
		BreathAmplitude.DisplayName = TEXT("Breath Amplitude");
		BreathAmplitude.SocketName = TEXT("Breath Amplitude");
		BreathAmplitude.Type = EBlenderGNParameterType::Float;
		BreathAmplitude.FloatValue = 0.12f;
		BreathAmplitude.SliderMin = 0.0f;
		BreathAmplitude.SliderMax = 0.35f;
		Inputs.Add(BreathAmplitude);

		FBlenderGNInput BreathSpeed;
		BreathSpeed.DisplayName = TEXT("Breath Speed");
		BreathSpeed.SocketName = TEXT("Breath Speed");
		BreathSpeed.Type = EBlenderGNParameterType::Float;
		BreathSpeed.FloatValue = 1.15f;
		BreathSpeed.SliderMin = 0.0f;
		BreathSpeed.SliderMax = 4.0f;
		Inputs.Add(BreathSpeed);

		FBlenderGNInput BreathNoiseScale;
		BreathNoiseScale.DisplayName = TEXT("Breath Noise Scale");
		BreathNoiseScale.SocketName = TEXT("Breath Noise Scale");
		BreathNoiseScale.Type = EBlenderGNParameterType::Float;
		BreathNoiseScale.FloatValue = 1.75f;
		BreathNoiseScale.SliderMin = 0.2f;
		BreathNoiseScale.SliderMax = 10.0f;
		Inputs.Add(BreathNoiseScale);
	}

	if (IsHexSphereRuntimeInputSet(Inputs))
	{
		for (int32 Index = 0; Index < HexSphereRuntimeInputCount; ++Index)
		{
			Inputs[Index].SocketName = HexSphereRuntimeInputNames[Index];
			Inputs[Index].DisplayName = HexSphereRuntimeInputNames[Index];
		}
	}

	for (FBlenderGNInput& Input : Inputs)
	{
		if (Input.SocketName.IsEmpty())
		{
			Input.SocketName = !Input.DisplayName.IsEmpty() ? Input.DisplayName : Input.FallbackIdentifier;
		}
		if (Input.DisplayName.IsEmpty())
		{
			Input.DisplayName = !Input.SocketName.IsEmpty() ? Input.SocketName : Input.FallbackIdentifier;
		}
	}
}

void UBlenderGeometryNodesComponent::OnRegister()
{
	Super::OnRegister();
	EnsureDefaults();
	SetComponentTickEnabled(true);

#if WITH_EDITOR
	UWorld* World = GetWorld();
	if (!HasAnyFlags(RF_ClassDefaultObject) && World && !World->IsGameWorld() && LastMeshData.Vertices.Num() == 0 && RefreshMode != EBlenderGNRefreshMode::Manual)
	{
		RequestRefresh(true);
	}
#endif
}

void UBlenderGeometryNodesComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsureDefaults();
	if (LastMeshData.Vertices.Num() == 0)
	{
		RestoreLastMeshDataFromProceduralMesh();
	}
	if (bRefreshOnBeginPlay)
	{
		RefreshNow();
	}
}

void UBlenderGeometryNodesComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UBlenderGeometryNodesComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const double Now = FPlatformTime::Seconds();
	if (bHasPendingRefresh && !bIsRunning && Now >= QueuedRefreshTime)
	{
		bHasPendingRefresh = false;
		RequestRefresh(true);
	}

	if (RefreshMode == EBlenderGNRefreshMode::FixedInterval && Now >= NextFixedRefreshTime)
	{
		NextFixedRefreshTime = Now + FMath::Max(0.1f, FixedRefreshInterval);
		RequestRefresh(true);
	}
	else if (RefreshMode == EBlenderGNRefreshMode::OnParameterChange)
	{
		const FString Hash = BuildRequestHash();
		if (!Hash.Equals(LastRequestHash, ESearchCase::CaseSensitive))
		{
			RequestRefresh(false);
		}
	}

	ApplyPreviewAnimation(Now);
}

#if WITH_EDITOR
void UBlenderGeometryNodesComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	EnsureDefaults();
	BlenderTimeoutSeconds = FMath::Clamp(BlenderTimeoutSeconds, 1, 120);
	MinimumRefreshInterval = FMath::Clamp(MinimumRefreshInterval, 0.02f, 2.0f);
	FixedRefreshInterval = FMath::Clamp(FixedRefreshInterval, 0.1f, 10.0f);
	BlenderFrameRate = FMath::Clamp(BlenderFrameRate, 1.0f, 60.0f);
	PreviewAnimationFrameRate = FMath::Clamp(PreviewAnimationFrameRate, 1.0f, 120.0f);

	if (RefreshMode == EBlenderGNRefreshMode::OnParameterChange)
	{
		NotifyParametersChanged();
	}
}

#endif

void UBlenderGeometryNodesComponent::RefreshNow()
{
	RequestRefresh(true);
}

void UBlenderGeometryNodesComponent::RequestRefresh(bool bForce)
{
	EnsureDefaults();

	const FString Hash = BuildRequestHash();
	if (!bForce && Hash.Equals(LastRequestHash, ESearchCase::CaseSensitive))
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (bIsRunning)
	{
		bHasPendingRefresh = true;
		QueuedRefreshTime = Now + FMath::Max(0.05f, MinimumRefreshInterval);
		LastStatus = TEXT("Refresh queued while Blender is still evaluating.");
		return;
	}

	if (!bForce)
	{
		const double Earliest = LastRefreshStartTime + MinimumRefreshInterval;
		if (Now < Earliest)
		{
			bHasPendingRefresh = true;
			QueuedRefreshTime = Earliest;
			LastStatus = TEXT("Refresh queued for rate limiting.");
			return;
		}
	}

	StartBackgroundRefresh(Hash);
}

void UBlenderGeometryNodesComponent::NotifyParametersChanged()
{
	RequestRefreshForParameterChange();
}

void UBlenderGeometryNodesComponent::RequestRefreshForParameterChange()
{
	if (RefreshMode == EBlenderGNRefreshMode::Manual)
	{
		RequestRefresh(true);
	}
	else if (RefreshMode == EBlenderGNRefreshMode::OnParameterChange)
	{
		RequestRefresh(false);
	}
}

void UBlenderGeometryNodesComponent::ApplyPreviewAnimation(double TimeSeconds)
{
	if (AActor* Owner = GetOwner())
	{
		if (Owner->FindComponentByClass<UBlenderGNGradientBoxMotionComponent>())
		{
			LastPreviewAnimationStatus = TEXT("Gradient Box local motion component owns preview animation.");
			return;
		}
	}

	if (!bEnablePreviewAnimation || LastMeshData.Vertices.Num() == 0 || LastMeshData.Sections.Num() == 0)
	{
		return;
	}

	const double MinFrameInterval = PreviewAnimationFrameRate >= 59.0f
		? 0.0
		: 1.0 / FMath::Clamp(PreviewAnimationFrameRate, 1.0f, 120.0f);
	if (MinFrameInterval > 0.0 && TimeSeconds - LastPreviewAnimationTime < MinFrameInterval)
	{
		return;
	}
	LastPreviewAnimationTime = TimeSeconds;

	if (PreviewVertexIsland.Num() != LastMeshData.Vertices.Num() || PreviewIslandNormals.Num() == 0 || PreviewIslandPhases.Num() == 0)
	{
		BuildPreviewAnimationCache();
	}
	if (PreviewVertexIsland.Num() != LastMeshData.Vertices.Num() || PreviewIslandNormals.Num() == 0)
	{
		return;
	}

	const float Amplitude = GetFloatInput(TEXT("Breath Amplitude"), 0.12f) * BlenderToUnrealScale;
	const float Speed = GetFloatInput(TEXT("Breath Speed"), 1.15f);
	if (FMath::IsNearlyZero(Amplitude) || FMath::IsNearlyZero(Speed))
	{
		return;
	}

	if (FastDynamicMesh && FastDynamicMesh->IsIslandMotionEnabled() && FastDynamicMesh->GetVertexCount() == LastMeshData.Vertices.Num())
	{
		if (CachedProceduralMesh)
		{
			CachedProceduralMesh->SetVisibility(false);
		}
		DestroyPreviewIslandMeshes();
		FastDynamicMesh->SetVisibility(true);
		LastPreviewAnimationStatus = FString::Printf(
			TEXT("GPU island motion: %d islands for subdivision %d"),
			PreviewIslandNormals.Num(),
			GetIntInput(TEXT("Hex Subdivisions"), 2));
		return;
	}

	const float TimePhase = static_cast<float>(TimeSeconds) * Speed;
	const float Smoothness = FMath::Clamp(PreviewAnimationSmoothness, 0.0f, 1.0f);
	const float Blend = FMath::Lerp(1.0f, 0.35f, Smoothness);
	const float AmpBlend = Amplitude * Blend;

	if (PreviewIslandOffsets.Num() != PreviewIslandNormals.Num())
	{
		PreviewIslandOffsets.SetNumUninitialized(PreviewIslandNormals.Num());
	}
	for (int32 Island = 0; Island < PreviewIslandNormals.Num(); ++Island)
	{
		const float Wave = FMath::Sin(TimePhase + PreviewIslandPhases[Island]);
		const float Shaped = FMath::Lerp(Wave, FMath::SmoothStep(-1.0f, 1.0f, Wave), Smoothness);
		PreviewIslandOffsets[Island] = PreviewIslandNormals[Island] * (Shaped * AmpBlend);
	}

	if (!FastDynamicMesh || !FastDynamicMesh->IsIslandMotionEnabled() || FastDynamicMesh->GetVertexCount() != LastMeshData.Vertices.Num())
	{
		AActor* Owner = GetOwner();
		if (!Owner || !Owner->GetRootComponent())
		{
			return;
		}

		if (FastDynamicMesh)
		{
			FastDynamicMesh->DestroyComponent();
			FastDynamicMesh = nullptr;
		}

		FastDynamicMesh = NewObject<UBlenderGNFastDynamicMeshComponent>(Owner, TEXT("BlenderGN_GpuIslandMotion"), RF_Transient);
		FastDynamicMesh->SetupAttachment(Owner->GetRootComponent());

		TArray<FColor> Colors;
		Colors.Init(FColor(184, 184, 173, 255), LastMeshData.Vertices.Num());
		FastDynamicMesh->InitIslandMotionMesh(
			LastMeshData.Vertices,
			LastMeshData.Triangles,
			PreviewStableNormals.Num() == LastMeshData.Vertices.Num() ? PreviewStableNormals : LastMeshData.Normals,
			LastMeshData.UVs,
			Colors,
			PreviewStableTangents,
			PreviewVertexIsland,
			PreviewIslandNormals,
			PreviewIslandPhases,
			Amplitude,
			Speed,
			Smoothness);

		UMaterialInterface* Mat = nullptr;
		if (bForceDefaultMaterial)
		{
			Mat = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		else if (MaterialOverrides.Num() > 0 && MaterialOverrides[0])
		{
			Mat = MaterialOverrides[0];
		}
		else if (FallbackMaterial)
		{
			Mat = FallbackMaterial;
		}
		if (Mat)
		{
			FastDynamicMesh->SetMaterial(0, Mat);
		}

		FastDynamicMesh->SetCastShadow(false);
		FastDynamicMesh->SetVisibility(true);
		FastDynamicMesh->RegisterComponent();
		LastPreviewAnimationStatus = FString::Printf(
			TEXT("GPU island motion: %d islands for subdivision %d"),
			PreviewIslandNormals.Num(),
			GetIntInput(TEXT("Hex Subdivisions"), 2));
	}

	if (CachedProceduralMesh)
	{
		CachedProceduralMesh->SetVisibility(false);
	}
	DestroyPreviewIslandMeshes();
	if (FastDynamicMesh)
	{
		FastDynamicMesh->SetVisibility(true);
	}
}

void UBlenderGeometryNodesComponent::BuildPreviewAnimationCache()
{
	PreviewVertexIsland.Reset();
	PreviewIslandNormals.Reset();
	PreviewIslandPhases.Reset();
	LastPreviewIslandCount = 0;
	LastPreviewAnimationStatus = TEXT("No mesh");

	const int32 VertexCount = LastMeshData.Vertices.Num();
	if (VertexCount == 0 || LastMeshData.Triangles.Num() == 0)
	{
		return;
	}

	BlenderGNProcMesh::FIslandMotionCache Cache;
	const int32 HexSubdivisions = GetIntInput(TEXT("Hex Subdivisions"), 2);
	const float NoiseScale = FMath::Max(0.2f, GetFloatInput(TEXT("Breath Noise Scale"), 1.75f));
	if (BlenderGNProcMesh::BuildHexIslandMotionCache(LastMeshData, HexSubdivisions, NoiseScale, Cache))
	{
		PreviewVertexIsland = MoveTemp(Cache.VertexIsland);
		PreviewIslandNormals = MoveTemp(Cache.IslandNormals);
		PreviewIslandPhases = MoveTemp(Cache.IslandPhases);
		PreviewAnimatedVertices = LastMeshData.Vertices;
		PreviewAnimatedPositions.Reset();
		LastPreviewIslandCount = PreviewIslandNormals.Num();
		LastPreviewAnimationStatus = FString::Printf(
			TEXT("Hex island cache: %d islands for subdivision %d"),
			LastPreviewIslandCount,
			HexSubdivisions);
		return;
	}

	TArray<int32> Parent;
	TArray<int32> Rank;
	Parent.SetNumUninitialized(VertexCount);
	Rank.Init(0, VertexCount);
	for (int32 Index = 0; Index < VertexCount; ++Index)
	{
		Parent[Index] = Index;
	}

	TFunction<int32(int32)> FindRoot = [&Parent, &FindRoot](int32 Index) -> int32
	{
		if (Parent[Index] != Index)
		{
			Parent[Index] = FindRoot(Parent[Index]);
		}
		return Parent[Index];
	};

	auto Union = [&Parent, &Rank, &FindRoot](int32 A, int32 B)
	{
		int32 RootA = FindRoot(A);
		int32 RootB = FindRoot(B);
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
	};

	TMap<FIntVector, int32> PositionRepresentative;
	constexpr float PositionQuantization = 1000.0f;
	for (int32 Index = 0; Index < VertexCount; ++Index)
	{
		const FVector& Vertex = LastMeshData.Vertices[Index];
		const FIntVector Key(
			FMath::RoundToInt(Vertex.X * PositionQuantization),
			FMath::RoundToInt(Vertex.Y * PositionQuantization),
			FMath::RoundToInt(Vertex.Z * PositionQuantization));
		if (int32* Existing = PositionRepresentative.Find(Key))
		{
			Union(Index, *Existing);
		}
		else
		{
			PositionRepresentative.Add(Key, Index);
		}
	}

	TMap<FIntVector, int32> DirectionRepresentative;
	constexpr float DirectionQuantization = 16.0f;
	for (int32 Index = 0; Index < VertexCount; ++Index)
	{
		const FVector Direction = (LastMeshData.Vertices[Index] - LastMeshCenter).GetSafeNormal();
		if (Direction.IsNearlyZero())
		{
			continue;
		}

		const FIntVector Key(
			FMath::RoundToInt(Direction.X * DirectionQuantization),
			FMath::RoundToInt(Direction.Y * DirectionQuantization),
			FMath::RoundToInt(Direction.Z * DirectionQuantization));
		if (int32* Existing = DirectionRepresentative.Find(Key))
		{
			Union(Index, *Existing);
		}
		else
		{
			DirectionRepresentative.Add(Key, Index);
		}
	}

	for (int32 Index = 0; Index + 2 < LastMeshData.Triangles.Num(); Index += 3)
	{
		const int32 A = LastMeshData.Triangles[Index];
		const int32 B = LastMeshData.Triangles[Index + 1];
		const int32 C = LastMeshData.Triangles[Index + 2];
		if (LastMeshData.Vertices.IsValidIndex(A) && LastMeshData.Vertices.IsValidIndex(B) && LastMeshData.Vertices.IsValidIndex(C))
		{
			Union(A, B);
			Union(B, C);
			Union(C, A);
		}
	}

	TMap<int32, int32> RootToIsland;
	TArray<FVector> IslandCenters;
	TArray<int32> IslandCounts;
	PreviewVertexIsland.SetNumUninitialized(VertexCount);
	for (int32 Index = 0; Index < VertexCount; ++Index)
	{
		const int32 Root = FindRoot(Index);
		int32* IslandPtr = RootToIsland.Find(Root);
		if (!IslandPtr)
		{
			const int32 NewIsland = IslandCenters.Num();
			RootToIsland.Add(Root, NewIsland);
			IslandCenters.Add(FVector::ZeroVector);
			IslandCounts.Add(0);
			IslandPtr = RootToIsland.Find(Root);
		}

		const int32 Island = *IslandPtr;
		PreviewVertexIsland[Index] = Island;
		IslandCenters[Island] += LastMeshData.Vertices[Index];
		++IslandCounts[Island];
	}

	PreviewIslandNormals.SetNum(IslandCenters.Num());
	PreviewIslandPhases.SetNum(IslandCenters.Num());
	for (int32 Island = 0; Island < IslandCenters.Num(); ++Island)
	{
		const FVector Center = IslandCenters[Island] / static_cast<float>(FMath::Max(1, IslandCounts[Island]));
		const FVector Normal = (Center - LastMeshCenter).GetSafeNormal();
		PreviewIslandNormals[Island] = Normal.IsNearlyZero() ? FVector::UpVector : Normal;
		PreviewIslandPhases[Island] = Hash01((Island + 1) * 37) * 2.0f * PI * NoiseScale;
	}

	PreviewAnimatedVertices = LastMeshData.Vertices;
	LastPreviewIslandCount = PreviewIslandNormals.Num();
	LastPreviewAnimationStatus = FString::Printf(TEXT("Fallback cache: %d connected components"), LastPreviewIslandCount);
}

void UBlenderGeometryNodesComponent::StartBackgroundRefresh(const FString& RequestHash)
{
	FString ScriptPath;
	const FString Error = ValidatePaths(ScriptPath);
	if (!Error.IsEmpty())
	{
		LastStatus = Error;
		bIsRunning = false;
		return;
	}

	bIsRunning = true;
	bHasPendingRefresh = false;
	ActiveRequestHash = RequestHash;
	LastRefreshStartTime = FPlatformTime::Seconds();
	LastStatus = TEXT("Starting Blender...");

	const FString WorkDir = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("OpenClawBlenderGeometryNodes"));
	IFileManager::Get().MakeDirectory(*WorkDir, true);
	const FString Guid = FGuid::NewGuid().ToString(EGuidFormats::Digits);
	const FString RequestPath = FPaths::Combine(WorkDir, FString::Printf(TEXT("gn_request_%s.json"), *Guid));
	const FString MeshPath = FPaths::Combine(WorkDir, FString::Printf(TEXT("gn_mesh_%s.%s"), *Guid, bUseBinaryMeshCache ? TEXT("bin") : TEXT("json")));
	FFileHelper::SaveStringToFile(BuildRequestJson(MeshPath), *RequestPath);

	const FString Arguments = BuildBlenderArguments(ScriptPath, RequestPath);
	const FString WorkingDirectory = FPaths::GetPath(BlendFile.FilePath);
	const FString BlenderExe = BlenderExecutable;
	const int32 TimeoutSeconds = BlenderTimeoutSeconds;
	const float Scale = BlenderToUnrealScale;
	const bool bShouldSmoothNormals = bSmoothNormalsByPosition;
	const float SmoothQuantization = NormalSmoothQuantization;
	TWeakObjectPtr<UBlenderGeometryNodesComponent> WeakThis(this);

	Async(EAsyncExecution::ThreadPool, [WeakThis, BlenderExe, Arguments, WorkingDirectory, MeshPath, TimeoutSeconds, Scale, bShouldSmoothNormals, SmoothQuantization]()
	{
		UBlenderGeometryNodesComponent::FEvalResult Result;
		const double Start = FPlatformTime::Seconds();
		FProcHandle Process = FPlatformProcess::CreateProc(*BlenderExe, *Arguments, true, true, true, nullptr, 0, *WorkingDirectory, nullptr);
		if (!Process.IsValid())
		{
			Result.Status = TEXT("Failed to start Blender.");
		}
		else
		{
			bool bTimedOut = false;
			while (FPlatformProcess::IsProcRunning(Process))
			{
				if (FPlatformTime::Seconds() - Start > TimeoutSeconds)
				{
					FPlatformProcess::TerminateProc(Process, true);
					bTimedOut = true;
					break;
				}
				FPlatformProcess::Sleep(0.015f);
			}

			int32 ReturnCode = 0;
			FPlatformProcess::GetProcReturnCode(Process, &ReturnCode);
			FPlatformProcess::CloseProc(Process);
			Result.ElapsedSeconds = static_cast<float>(FPlatformTime::Seconds() - Start);

			if (bTimedOut)
			{
				Result.Status = FString::Printf(TEXT("Blender timed out after %ds."), TimeoutSeconds);
			}
			else if (ReturnCode != 0 || !FPaths::FileExists(MeshPath))
			{
				Result.Status = FString::Printf(TEXT("Blender export failed. ExitCode=%d"), ReturnCode);
			}
			else
			{
				FString Error;
				bool bReadOk = false;
				if (MeshPath.EndsWith(TEXT(".bin")))
				{
					bReadOk = UBlenderGeometryNodesComponent::ReadBinaryMeshCache(MeshPath, Scale, Result.MeshData, Error);
				}
				else
				{
					FString JsonText;
					FFileHelper::LoadFileToString(JsonText, *MeshPath);
					bReadOk = UBlenderGeometryNodesComponent::ReadJsonMeshCache(JsonText, Scale, Result.MeshData, Error);
				}
				Result.bSuccess = bReadOk;
				if (Result.bSuccess && bShouldSmoothNormals && !Result.MeshData.bImportedNormals)
				{
					UBlenderGeometryNodesComponent::SmoothNormalsByPosition(Result.MeshData, SmoothQuantization);
				}
				Result.Status = bReadOk ? TEXT("Blender Geometry Nodes mesh updated.") : Error;
			}
		}

		AsyncTask(ENamedThreads::GameThread, [WeakThis, Result]()
		{
			if (!WeakThis.IsValid())
			{
				return;
			}
			UBlenderGeometryNodesComponent* Component = WeakThis.Get();
			Component->bIsRunning = false;
			Component->LastEvaluationSeconds = Result.ElapsedSeconds;
			Component->LastStatus = Result.Status;
			if (Result.bSuccess)
			{
				Component->LastRequestHash = Component->ActiveRequestHash;
				Component->ApplyMeshData(Result.MeshData);
				if (Component->LastRequestHash.Equals(Component->BuildRequestHash(), ESearchCase::CaseSensitive))
				{
					Component->bHasPendingRefresh = false;
				}
				else if (Component->bHasPendingRefresh)
				{
					Component->bHasPendingRefresh = false;
					Component->RequestRefresh(true);
				}
			}
		});
	});
}

FString UBlenderGeometryNodesComponent::ValidatePaths(FString& OutScriptPath) const
{
	OutScriptPath = SidecarScriptPath;
	if (OutScriptPath.IsEmpty())
	{
		OutScriptPath = GetDefaultSidecarPath();
	}
	if (BlenderExecutable.IsEmpty() || !FPaths::FileExists(BlenderExecutable))
	{
		return TEXT("Blender executable is missing or invalid.");
	}
	if (BlendFile.FilePath.IsEmpty() || !FPaths::FileExists(BlendFile.FilePath))
	{
		return TEXT("Blend file is missing or invalid.");
	}
	if (OutScriptPath.IsEmpty() || !FPaths::FileExists(OutScriptPath))
	{
		return TEXT("Blender sidecar script is missing or invalid.");
	}
	if (ObjectName.IsEmpty())
	{
		return TEXT("Object Name is empty.");
	}
	return FString();
}

FString UBlenderGeometryNodesComponent::BuildBlenderArguments(const FString& ScriptPath, const FString& RequestPath) const
{
	return FString::Printf(TEXT("-b \"%s\" --python \"%s\" -- \"%s\""), *BlendFile.FilePath, *ScriptPath, *RequestPath);
}

FString UBlenderGeometryNodesComponent::BuildRequestJson(const FString& MeshOutputPath) const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("objectName"), ObjectName);
	Root->SetStringField(TEXT("modifierName"), ModifierName);
	Root->SetStringField(TEXT("outputPath"), MeshOutputPath);
	Root->SetStringField(TEXT("meshFormat"), bUseBinaryMeshCache ? TEXT("binary") : TEXT("json"));
	Root->SetNumberField(TEXT("frameRate"), BlenderFrameRate);
	const double WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
	Root->SetNumberField(TEXT("unityTime"), WorldTime);
	Root->SetNumberField(TEXT("frame"), BlenderFrameOffset + FMath::RoundToInt(WorldTime * BlenderFrameRate));

	TArray<TSharedPtr<FJsonValue>> InputValues;
	for (const FBlenderGNInput& Input : Inputs)
	{
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Input.SocketName);
		Item->SetStringField(TEXT("fallbackIdentifier"), Input.FallbackIdentifier);
		switch (Input.Type)
		{
		case EBlenderGNParameterType::Int:
			Item->SetNumberField(TEXT("value"), Input.IntValue);
			break;
		case EBlenderGNParameterType::Bool:
			Item->SetBoolField(TEXT("value"), Input.BoolValue);
			break;
		case EBlenderGNParameterType::Vector3:
			{
				TArray<TSharedPtr<FJsonValue>> Vector;
				Vector.Add(MakeShared<FJsonValueNumber>(Input.VectorValue.X));
				Vector.Add(MakeShared<FJsonValueNumber>(Input.VectorValue.Y));
				Vector.Add(MakeShared<FJsonValueNumber>(Input.VectorValue.Z));
				Item->SetArrayField(TEXT("value"), Vector);
			}
			break;
		default:
			Item->SetNumberField(TEXT("value"), Input.FloatValue);
			break;
		}
		InputValues.Add(MakeShared<FJsonValueObject>(Item));
	}
	Root->SetArrayField(TEXT("inputs"), InputValues);

	FString Output;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root, Writer);
	return Output;
}

FString UBlenderGeometryNodesComponent::BuildRequestHash() const
{
	FString InputHash;
	for (const FBlenderGNInput& Input : Inputs)
	{
		InputHash += FString::Printf(
			TEXT("|%s|%s|%s|%d|%d|%.9g|%d|%.9g|%.9g|%.9g"),
			*Input.DisplayName,
			*Input.SocketName,
			*Input.FallbackIdentifier,
			static_cast<int32>(Input.Type),
			Input.IntValue,
			Input.FloatValue,
			Input.BoolValue ? 1 : 0,
			Input.VectorValue.X,
			Input.VectorValue.Y,
			Input.VectorValue.Z);
	}
	return FString::Printf(TEXT("%s|%s|%s|%s|%d%s"), *BlenderExecutable, *BlendFile.FilePath, *ObjectName, *ModifierName, bUseBinaryMeshCache ? 1 : 0, *InputHash);
}

void UBlenderGeometryNodesComponent::ApplyMeshData(const FBlenderGNMeshData& MeshData)
{
	const int32 PreviousVertexCount = LastVertexCount;
	const int32 PreviousTriangleCount = LastTriangleCount;
	const int32 PreviousSectionCount = LastMeshData.Sections.Num();

	LastMeshData = MeshData;
	LastVertexCount = LastMeshData.Vertices.Num();
	LastTriangleCount = LastMeshData.GetTriangleCount();
	LastMeshCenter = FVector::ZeroVector;
	LastPreviewAnimationTime = -999.0;
	DestroyPreviewIslandMeshes();
	if (LastMeshData.Vertices.Num() > 0)
	{
		for (const FVector& Vertex : LastMeshData.Vertices)
		{
			LastMeshCenter += Vertex;
		}
		LastMeshCenter /= static_cast<float>(LastMeshData.Vertices.Num());
	}

	const bool bTopologyUnchanged = (LastVertexCount == PreviousVertexCount)
		&& (LastTriangleCount == PreviousTriangleCount)
		&& (LastMeshData.Sections.Num() == PreviousSectionCount)
		&& (PreviousVertexCount > 0);

	if (FastDynamicMesh)
	{
		FastDynamicMesh->DestroyComponent();
		FastDynamicMesh = nullptr;
	}

	UProceduralMeshComponent* ProceduralMesh = EnsureProceduralMesh();
	if (ProceduralMesh)
	{
		ProceduralMesh->SetVisibility(!FastDynamicMesh);
	}
	if (!ProceduralMesh)
	{
		LastStatus = TEXT("Could not create ProceduralMeshComponent.");
		return;
	}

	if (LastMeshData.bImportedNormals && LastMeshData.Normals.Num() == LastMeshData.Vertices.Num())
	{
		PreviewStableNormals = LastMeshData.Normals;
	}
	else
	{
		FBlenderGNMeshData MeshWithRecalculatedNormals = LastMeshData;
		RecalculateNormalsForTriangleWinding(MeshWithRecalculatedNormals);
		PreviewStableNormals = MoveTemp(MeshWithRecalculatedNormals.Normals);
	}
	if (bInvertShadingNormals)
	{
		for (FVector& Normal : PreviewStableNormals)
		{
			Normal *= -1.0;
		}
	}
	CalculateTangentsAlignedWithNormals(LastMeshData, PreviewStableNormals, PreviewStableTangents);
	BuildPreviewAnimationCache();

	if (bTopologyUnchanged)
	{
		for (int32 SectionIndex = 0; SectionIndex < LastMeshData.Sections.Num(); ++SectionIndex)
		{
			ProceduralMesh->UpdateMeshSection_LinearColor(
				SectionIndex,
				LastMeshData.Vertices,
				PreviewStableNormals,
				LastMeshData.UVs,
				PreviewVertexColors,
				PreviewStableTangents);
		}
	}
	else
	{
		PreviewVertexColors.Reset();
		PreviewVertexColors.Init(FLinearColor(0.72f, 0.72f, 0.68f, 1.0f), LastMeshData.Vertices.Num());

		ProceduralMesh->SetCastShadow(bCastShadows);
		ProceduralMesh->ClearAllMeshSections();
		for (int32 SectionIndex = 0; SectionIndex < LastMeshData.Sections.Num(); ++SectionIndex)
		{
			const FBlenderGNMeshSection& Section = LastMeshData.Sections[SectionIndex];
			ProceduralMesh->CreateMeshSection_LinearColor(
				SectionIndex,
				LastMeshData.Vertices,
				Section.Triangles,
				PreviewStableNormals,
				LastMeshData.UVs,
				PreviewVertexColors,
				PreviewStableTangents,
				false);

			if (bForceDefaultMaterial)
			{
				if (UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface))
				{
					ProceduralMesh->SetMaterial(SectionIndex, DefaultMaterial);
				}
			}
			else if (MaterialOverrides.IsValidIndex(SectionIndex) && MaterialOverrides[SectionIndex])
			{
				ProceduralMesh->SetMaterial(SectionIndex, MaterialOverrides[SectionIndex]);
			}
			else if (FallbackMaterial)
			{
				ProceduralMesh->SetMaterial(SectionIndex, FallbackMaterial);
			}
			else if (UMaterialInterface* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface))
			{
				ProceduralMesh->SetMaterial(SectionIndex, DefaultMaterial);
			}
		}
	}

	OnMeshUpdated.Broadcast(ProceduralMesh);
	ApplyPreviewAnimation(FPlatformTime::Seconds());
}

void UBlenderGeometryNodesComponent::ReapplyLastMeshData()
{
	if (LastMeshData.Vertices.Num() == 0 || LastMeshData.Triangles.Num() == 0)
	{
		return;
	}
	ApplyMeshData(LastMeshData);
}

bool UBlenderGeometryNodesComponent::RestoreLastMeshDataFromProceduralMesh()
{
	UProceduralMeshComponent* ProceduralMesh = GetProceduralMesh();
	if (!ProceduralMesh)
	{
		return false;
	}

	FBlenderGNMeshData RestoredMeshData;
	if (!BuildMeshDataFromProceduralMesh(ProceduralMesh, RestoredMeshData))
	{
		return false;
	}

	LastMeshData = RestoredMeshData;
	LastVertexCount = LastMeshData.Vertices.Num();
	LastTriangleCount = LastMeshData.GetTriangleCount();
	LastMeshCenter = FVector::ZeroVector;
	for (const FVector& Vertex : LastMeshData.Vertices)
	{
		LastMeshCenter += Vertex;
	}
	if (LastMeshData.Vertices.Num() > 0)
	{
		LastMeshCenter /= static_cast<float>(LastMeshData.Vertices.Num());
	}

	if (LastMeshData.Normals.Num() == LastMeshData.Vertices.Num())
	{
		PreviewStableNormals = LastMeshData.Normals;
	}
	CalculateTangentsAlignedWithNormals(LastMeshData, PreviewStableNormals, PreviewStableTangents);
	BuildPreviewAnimationCache();
	OnMeshUpdated.Broadcast(ProceduralMesh);
	LastStatus = TEXT("Restored mesh cache from saved ProceduralMesh.");
	return true;
}

void UBlenderGeometryNodesComponent::DisablePreviewAnimationMeshes()
{
	DestroyPreviewIslandMeshes();
	if (FastDynamicMesh)
	{
		FastDynamicMesh->DestroyComponent();
		FastDynamicMesh = nullptr;
	}
}

void UBlenderGeometryNodesComponent::EnsureFastDynamicMesh()
{
	if (FastDynamicMesh && FastDynamicMesh->GetVertexCount() == LastMeshData.Vertices.Num())
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || LastMeshData.Vertices.Num() == 0)
	{
		return;
	}

	const bool bNeedsRegister = !FastDynamicMesh;
	if (!FastDynamicMesh)
	{
		FastDynamicMesh = NewObject<UBlenderGNFastDynamicMeshComponent>(Owner, TEXT("BlenderGN_FastDynMesh"), RF_Transient);
		FastDynamicMesh->SetupAttachment(Owner->GetRootComponent());
	}
	TArray<FColor> Colors;
	Colors.Init(FColor(184, 184, 173, 255), LastMeshData.Vertices.Num());

	FastDynamicMesh->InitMesh(
		LastMeshData.Vertices,
		LastMeshData.Triangles,
		PreviewStableNormals.Num() == LastMeshData.Vertices.Num() ? PreviewStableNormals : LastMeshData.Normals,
		LastMeshData.UVs,
		Colors,
		PreviewStableTangents);

	UMaterialInterface* Mat = nullptr;
	if (bForceDefaultMaterial)
	{
		Mat = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	else if (MaterialOverrides.Num() > 0 && MaterialOverrides[0])
	{
		Mat = MaterialOverrides[0];
	}
	else if (FallbackMaterial)
	{
		Mat = FallbackMaterial;
	}
	if (Mat)
	{
		FastDynamicMesh->SetMaterial(0, Mat);
	}

	FastDynamicMesh->SetCastShadow(bCastShadows);
	FastDynamicMesh->SetVisibility(true);
	if (bNeedsRegister)
	{
		FastDynamicMesh->RegisterComponent();
	}
	else
	{
		FastDynamicMesh->MarkRenderStateDirty();
		FastDynamicMesh->UpdateBounds();
	}

	if (CachedProceduralMesh)
	{
		CachedProceduralMesh->SetVisibility(false);
	}
}

bool UBlenderGeometryNodesComponent::EnsurePreviewIslandMeshes()
{
	const int32 IslandCount = PreviewIslandNormals.Num();
	if (IslandCount <= 0 || PreviewVertexIsland.Num() != LastMeshData.Vertices.Num())
	{
		return false;
	}
	if (PreviewIslandMeshes.Num() == IslandCount)
	{
		bool bAllValid = true;
		for (UProceduralMeshComponent* IslandMesh : PreviewIslandMeshes)
		{
			bAllValid &= IsValid(IslandMesh);
		}
		if (bAllValid)
		{
			return true;
		}
		DestroyPreviewIslandMeshes();
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Owner->GetRootComponent())
	{
		return false;
	}

	TArray<TMap<int32, int32>> GlobalToLocal;
	TArray<TArray<FVector>> IslandVertices;
	TArray<TArray<int32>> IslandTriangles;
	TArray<TArray<FVector>> IslandNormals;
	TArray<TArray<FVector2D>> IslandUVs;
	TArray<TArray<FLinearColor>> IslandColors;
	TArray<TArray<FProcMeshTangent>> IslandTangents;
	GlobalToLocal.SetNum(IslandCount);
	IslandVertices.SetNum(IslandCount);
	IslandTriangles.SetNum(IslandCount);
	IslandNormals.SetNum(IslandCount);
	IslandUVs.SetNum(IslandCount);
	IslandColors.SetNum(IslandCount);
	IslandTangents.SetNum(IslandCount);

	auto AddVertexToIsland = [&](int32 Island, int32 GlobalIndex) -> int32
	{
		if (int32* Existing = GlobalToLocal[Island].Find(GlobalIndex))
		{
			return *Existing;
		}

		const int32 LocalIndex = IslandVertices[Island].Num();
		GlobalToLocal[Island].Add(GlobalIndex, LocalIndex);
		IslandVertices[Island].Add(LastMeshData.Vertices[GlobalIndex]);
		IslandNormals[Island].Add(PreviewStableNormals.IsValidIndex(GlobalIndex) ? PreviewStableNormals[GlobalIndex] : FVector::UpVector);
		IslandUVs[Island].Add(LastMeshData.UVs.IsValidIndex(GlobalIndex) ? LastMeshData.UVs[GlobalIndex] : FVector2D::ZeroVector);
		IslandColors[Island].Add(FLinearColor(0.72f, 0.72f, 0.68f, 1.0f));
		IslandTangents[Island].Add(PreviewStableTangents.IsValidIndex(GlobalIndex) ? PreviewStableTangents[GlobalIndex] : FProcMeshTangent(1.0f, 0.0f, 0.0f));
		return LocalIndex;
	};

	for (int32 Index = 0; Index + 2 < LastMeshData.Triangles.Num(); Index += 3)
	{
		const int32 A = LastMeshData.Triangles[Index];
		const int32 B = LastMeshData.Triangles[Index + 1];
		const int32 C = LastMeshData.Triangles[Index + 2];
		if (!PreviewVertexIsland.IsValidIndex(A) || !PreviewVertexIsland.IsValidIndex(B) || !PreviewVertexIsland.IsValidIndex(C))
		{
			continue;
		}

		const int32 Island = PreviewVertexIsland[A];
		if (!IslandVertices.IsValidIndex(Island))
		{
			continue;
		}

		IslandTriangles[Island].Add(AddVertexToIsland(Island, A));
		IslandTriangles[Island].Add(AddVertexToIsland(Island, B));
		IslandTriangles[Island].Add(AddVertexToIsland(Island, C));
	}

	UMaterialInterface* Material = nullptr;
	if (bForceDefaultMaterial)
	{
		Material = UMaterial::GetDefaultMaterial(MD_Surface);
	}
	else if (MaterialOverrides.Num() > 0 && MaterialOverrides[0])
	{
		Material = MaterialOverrides[0];
	}
	else if (FallbackMaterial)
	{
		Material = FallbackMaterial;
	}
	else
	{
		Material = UMaterial::GetDefaultMaterial(MD_Surface);
	}

	PreviewIslandMeshes.SetNum(IslandCount);
	for (int32 Island = 0; Island < IslandCount; ++Island)
	{
		if (IslandVertices[Island].Num() == 0 || IslandTriangles[Island].Num() == 0)
		{
			DestroyPreviewIslandMeshes();
			return false;
		}

		const FName ComponentName = MakeUniqueObjectName(Owner, UProceduralMeshComponent::StaticClass(), *FString::Printf(TEXT("BlenderGN_Island_%02d"), Island));
		UProceduralMeshComponent* IslandMesh = NewObject<UProceduralMeshComponent>(Owner, ComponentName, RF_Transient);
		IslandMesh->SetupAttachment(Owner->GetRootComponent());
		IslandMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		IslandMesh->SetCastShadow(bCastShadows);
		IslandMesh->CreateMeshSection_LinearColor(
			0,
			IslandVertices[Island],
			IslandTriangles[Island],
			IslandNormals[Island],
			IslandUVs[Island],
			IslandColors[Island],
			IslandTangents[Island],
			false);
		if (Material)
		{
			IslandMesh->SetMaterial(0, Material);
		}
		IslandMesh->RegisterComponent();
		PreviewIslandMeshes[Island] = IslandMesh;
	}

	if (CachedProceduralMesh)
	{
		CachedProceduralMesh->SetVisibility(false);
	}
	if (FastDynamicMesh)
	{
		FastDynamicMesh->SetVisibility(false);
	}
	return true;
}

void UBlenderGeometryNodesComponent::DestroyPreviewIslandMeshes()
{
	for (UProceduralMeshComponent* IslandMesh : PreviewIslandMeshes)
	{
		if (IsValid(IslandMesh))
		{
			IslandMesh->DestroyComponent();
		}
	}
	PreviewIslandMeshes.Reset();
}

bool UBlenderGeometryNodesComponent::BuildMeshDataFromProceduralMesh(const UProceduralMeshComponent* ProceduralMesh, FBlenderGNMeshData& OutMeshData) const
{
	if (!ProceduralMesh)
	{
		return false;
	}

	OutMeshData.Reset();
	OutMeshData.ObjectName = ObjectName;
	OutMeshData.bImportedNormals = true;

	auto CopySectionVertices = [&OutMeshData](const FProcMeshSection* Section)
	{
		OutMeshData.Vertices.Reset();
		OutMeshData.Normals.Reset();
		OutMeshData.UVs.Reset();
		if (!Section)
		{
			return;
		}

		OutMeshData.Vertices.Reserve(Section->ProcVertexBuffer.Num());
		OutMeshData.Normals.Reserve(Section->ProcVertexBuffer.Num());
		OutMeshData.UVs.Reserve(Section->ProcVertexBuffer.Num());
		for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
		{
			OutMeshData.Vertices.Add(Vertex.Position);
			OutMeshData.Normals.Add(Vertex.Normal);
			OutMeshData.UVs.Add(Vertex.UV0);
		}
	};

	auto MatchesExistingVertices = [&OutMeshData](const FProcMeshSection* Section)
	{
		if (!Section || Section->ProcVertexBuffer.Num() != OutMeshData.Vertices.Num())
		{
			return false;
		}

		for (int32 Index = 0; Index < Section->ProcVertexBuffer.Num(); ++Index)
		{
			if (!Section->ProcVertexBuffer[Index].Position.Equals(OutMeshData.Vertices[Index], 0.01))
			{
				return false;
			}
		}
		return true;
	};

	const int32 SectionCount = ProceduralMesh->GetNumSections();
	for (int32 SectionIndex = 0; SectionIndex < SectionCount; ++SectionIndex)
	{
		const FProcMeshSection* Section = const_cast<UProceduralMeshComponent*>(ProceduralMesh)->GetProcMeshSection(SectionIndex);
		if (!Section || Section->ProcVertexBuffer.Num() == 0 || Section->ProcIndexBuffer.Num() < 3)
		{
			continue;
		}

		FBlenderGNMeshSection& MeshSection = OutMeshData.Sections.AddDefaulted_GetRef();
		if (OutMeshData.Vertices.Num() == 0)
		{
			CopySectionVertices(Section);
			for (auto Index : Section->ProcIndexBuffer)
			{
				MeshSection.Triangles.Add(static_cast<int32>(Index));
			}
		}
		else if (MatchesExistingVertices(Section))
		{
			for (auto Index : Section->ProcIndexBuffer)
			{
				MeshSection.Triangles.Add(static_cast<int32>(Index));
			}
		}
		else
		{
			const int32 VertexOffset = OutMeshData.Vertices.Num();
			OutMeshData.Vertices.Reserve(OutMeshData.Vertices.Num() + Section->ProcVertexBuffer.Num());
			OutMeshData.Normals.Reserve(OutMeshData.Normals.Num() + Section->ProcVertexBuffer.Num());
			OutMeshData.UVs.Reserve(OutMeshData.UVs.Num() + Section->ProcVertexBuffer.Num());
			for (const FProcMeshVertex& Vertex : Section->ProcVertexBuffer)
			{
				OutMeshData.Vertices.Add(Vertex.Position);
				OutMeshData.Normals.Add(Vertex.Normal);
				OutMeshData.UVs.Add(Vertex.UV0);
			}

			MeshSection.Triangles.Reserve(Section->ProcIndexBuffer.Num());
			for (auto Index : Section->ProcIndexBuffer)
			{
				MeshSection.Triangles.Add(VertexOffset + static_cast<int32>(Index));
			}
		}

		OutMeshData.Triangles.Append(MeshSection.Triangles);
		OutMeshData.MaterialIndices.Add(SectionIndex);

		FBlenderGNMaterialInfo MaterialInfo;
		if (UMaterialInterface* Material = ProceduralMesh->GetMaterial(SectionIndex))
		{
			MaterialInfo.Name = Material->GetName();
		}
		else
		{
			MaterialInfo.Name = FString::Printf(TEXT("Section_%d"), SectionIndex);
		}
		OutMeshData.Materials.Add(MaterialInfo);
	}

	return OutMeshData.Vertices.Num() > 0 && OutMeshData.Triangles.Num() >= 3 && OutMeshData.Sections.Num() > 0;
}

UProceduralMeshComponent* UBlenderGeometryNodesComponent::EnsureProceduralMesh()
{
	if (CachedProceduralMesh)
	{
		return CachedProceduralMesh;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	CachedProceduralMesh = Owner->FindComponentByClass<UProceduralMeshComponent>();
	if (!CachedProceduralMesh)
	{
		CachedProceduralMesh = NewObject<UProceduralMeshComponent>(Owner, TEXT("BlenderGN_ProceduralMesh"));
		CachedProceduralMesh->SetupAttachment(Owner->GetRootComponent());
		CachedProceduralMesh->RegisterComponent();
		Owner->AddInstanceComponent(CachedProceduralMesh);
	}
	if (!Owner->GetRootComponent())
	{
		Owner->SetRootComponent(CachedProceduralMesh);
	}
	return CachedProceduralMesh;
}

UProceduralMeshComponent* UBlenderGeometryNodesComponent::GetProceduralMesh() const
{
	if (CachedProceduralMesh)
	{
		return CachedProceduralMesh;
	}
	return GetOwner() ? GetOwner()->FindComponentByClass<UProceduralMeshComponent>() : nullptr;
}

FBlenderGNInput* UBlenderGeometryNodesComponent::FindInput(const FString& SocketName)
{
	for (FBlenderGNInput& Input : Inputs)
	{
		if (Input.MatchesName(SocketName))
		{
			return &Input;
		}
	}
	return nullptr;
}

const FBlenderGNInput* UBlenderGeometryNodesComponent::FindInput(const FString& SocketName) const
{
	for (const FBlenderGNInput& Input : Inputs)
	{
		if (Input.MatchesName(SocketName))
		{
			return &Input;
		}
	}
	return nullptr;
}

void UBlenderGeometryNodesComponent::SetFloatInput(const FString& SocketName, float Value, bool bRefresh)
{
	if (FBlenderGNInput* Input = FindInput(SocketName))
	{
		Input->Type = EBlenderGNParameterType::Float;
		Input->FloatValue = Value;
		if (bRefresh)
		{
			RequestRefreshForParameterChange();
		}
	}
}

void UBlenderGeometryNodesComponent::SetIntInput(const FString& SocketName, int32 Value, bool bRefresh)
{
	if (FBlenderGNInput* Input = FindInput(SocketName))
	{
		Input->Type = EBlenderGNParameterType::Int;
		Input->IntValue = Input->MatchesName(TEXT("Hex Subdivisions")) ? FMath::Clamp(Value, 1, 2) : Value;
		if (bRefresh)
		{
			RequestRefreshForParameterChange();
		}
	}
}

void UBlenderGeometryNodesComponent::SetBoolInput(const FString& SocketName, bool Value, bool bRefresh)
{
	if (FBlenderGNInput* Input = FindInput(SocketName))
	{
		Input->Type = EBlenderGNParameterType::Bool;
		Input->BoolValue = Value;
		if (bRefresh)
		{
			RequestRefreshForParameterChange();
		}
	}
}

void UBlenderGeometryNodesComponent::SetVectorInput(const FString& SocketName, FVector Value, bool bRefresh)
{
	if (FBlenderGNInput* Input = FindInput(SocketName))
	{
		Input->Type = EBlenderGNParameterType::Vector3;
		Input->VectorValue = Value;
		if (bRefresh)
		{
			RequestRefreshForParameterChange();
		}
	}
}

float UBlenderGeometryNodesComponent::GetFloatInput(const FString& SocketName, float Fallback) const
{
	const FBlenderGNInput* Input = FindInput(SocketName);
	return Input ? Input->FloatValue : Fallback;
}

int32 UBlenderGeometryNodesComponent::GetIntInput(const FString& SocketName, int32 Fallback) const
{
	const FBlenderGNInput* Input = FindInput(SocketName);
	return Input ? Input->IntValue : Fallback;
}

bool UBlenderGeometryNodesComponent::GetBoolInput(const FString& SocketName, bool Fallback) const
{
	const FBlenderGNInput* Input = FindInput(SocketName);
	return Input ? Input->BoolValue : Fallback;
}

FVector UBlenderGeometryNodesComponent::GetVectorInput(const FString& SocketName, FVector Fallback) const
{
	const FBlenderGNInput* Input = FindInput(SocketName);
	return Input ? Input->VectorValue : Fallback;
}

bool UBlenderGeometryNodesComponent::ReadJsonMeshCache(const FString& JsonText, float Scale, FBlenderGNMeshData& OutMeshData, FString& OutError)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		OutError = TEXT("Failed to parse Blender mesh cache JSON.");
		return false;
	}

	OutMeshData.Reset();
	OutMeshData.ObjectName = Root->GetStringField(TEXT("objectName"));

	const TArray<TSharedPtr<FJsonValue>>* Vertices = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Normals = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* UVs = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Triangles = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* MaterialIndices = nullptr;
	Root->TryGetArrayField(TEXT("vertices"), Vertices);
	Root->TryGetArrayField(TEXT("normals"), Normals);
	Root->TryGetArrayField(TEXT("uvs"), UVs);
	Root->TryGetArrayField(TEXT("triangles"), Triangles);
	Root->TryGetArrayField(TEXT("materialIndices"), MaterialIndices);

	if (!Vertices || !Triangles || Vertices->Num() % 3 != 0 || Triangles->Num() % 3 != 0)
	{
		OutError = TEXT("Blender mesh cache is missing vertices or triangles.");
		return false;
	}

	for (int32 Index = 0; Index + 2 < Vertices->Num(); Index += 3)
	{
		OutMeshData.Vertices.Add(ConvertPosition((*Vertices)[Index]->AsNumber(), (*Vertices)[Index + 1]->AsNumber(), (*Vertices)[Index + 2]->AsNumber(), Scale));
	}
	if (Normals && Normals->Num() == Vertices->Num())
	{
		OutMeshData.bImportedNormals = true;
		for (int32 Index = 0; Index + 2 < Normals->Num(); Index += 3)
		{
			OutMeshData.Normals.Add(ConvertNormal((*Normals)[Index]->AsNumber(), (*Normals)[Index + 1]->AsNumber(), (*Normals)[Index + 2]->AsNumber()));
		}
	}
	if (UVs)
	{
		for (int32 Index = 0; Index + 1 < UVs->Num(); Index += 2)
		{
			OutMeshData.UVs.Add(FVector2D((*UVs)[Index]->AsNumber(), 1.0 - (*UVs)[Index + 1]->AsNumber()));
		}
	}
	while (OutMeshData.UVs.Num() < OutMeshData.Vertices.Num())
	{
		OutMeshData.UVs.Add(FVector2D::ZeroVector);
	}

	for (int32 Index = 0; Index + 2 < Triangles->Num(); Index += 3)
	{
		OutMeshData.Triangles.Add((*Triangles)[Index]->AsNumber());
		OutMeshData.Triangles.Add((*Triangles)[Index + 2]->AsNumber());
		OutMeshData.Triangles.Add((*Triangles)[Index + 1]->AsNumber());
	}
	if (MaterialIndices)
	{
		for (const TSharedPtr<FJsonValue>& Value : *MaterialIndices)
		{
			OutMeshData.MaterialIndices.Add(static_cast<int32>(Value->AsNumber()));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* Names = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Colors = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Metallic = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Roughness = nullptr;
	Root->TryGetArrayField(TEXT("materialNames"), Names);
	Root->TryGetArrayField(TEXT("materialBaseColors"), Colors);
	Root->TryGetArrayField(TEXT("materialMetallic"), Metallic);
	Root->TryGetArrayField(TEXT("materialRoughness"), Roughness);
	const int32 MaterialCount = Names ? Names->Num() : 0;
	for (int32 Index = 0; Index < MaterialCount; ++Index)
	{
		FBlenderGNMaterialInfo Info;
		Info.Name = (*Names)[Index]->AsString();
		const int32 ColorOffset = Index * 4;
		if (Colors && Colors->IsValidIndex(ColorOffset + 3))
		{
			Info.BaseColor = FLinearColor((*Colors)[ColorOffset]->AsNumber(), (*Colors)[ColorOffset + 1]->AsNumber(), (*Colors)[ColorOffset + 2]->AsNumber(), (*Colors)[ColorOffset + 3]->AsNumber());
		}
		if (Metallic && Metallic->IsValidIndex(Index))
		{
			Info.Metallic = (*Metallic)[Index]->AsNumber();
		}
		if (Roughness && Roughness->IsValidIndex(Index))
		{
			Info.Roughness = (*Roughness)[Index]->AsNumber();
		}
		OutMeshData.Materials.Add(Info);
	}

	if (OutMeshData.Normals.Num() != OutMeshData.Vertices.Num())
	{
		OutMeshData.Normals.Reset();
	}
	NormalizeTriangleWindingAndNormals(OutMeshData);
	OutMeshData.BuildSections();
	return true;
}

bool UBlenderGeometryNodesComponent::ReadBinaryMeshCache(const FString& FilePath, float Scale, FBlenderGNMeshData& OutMeshData, FString& OutError)
{
	TArray<uint8> Bytes;
	if (!FFileHelper::LoadFileToArray(Bytes, *FilePath) || Bytes.Num() < 16)
	{
		OutError = TEXT("Failed to read Blender binary mesh cache.");
		return false;
	}

	const uint8 Magic[] = { 'O', 'C', 'G', 'N', 'M', 'E', 'S', 'H' };
	int32 Offset = 0;
	auto Need = [&Bytes, &Offset](int32 Count) { return Count >= 0 && Offset + Count <= Bytes.Num(); };
	auto ReadU32 = [&Bytes, &Offset, &Need](uint32& OutValue) -> bool
	{
		if (!Need(4))
		{
			return false;
		}
		OutValue = static_cast<uint32>(Bytes[Offset]) |
			(static_cast<uint32>(Bytes[Offset + 1]) << 8) |
			(static_cast<uint32>(Bytes[Offset + 2]) << 16) |
			(static_cast<uint32>(Bytes[Offset + 3]) << 24);
		Offset += 4;
		return true;
	};
	auto ReadString = [&Bytes, &Offset, &Need, &ReadU32](FString& OutValue) -> bool
	{
		uint32 Length = 0;
		if (!ReadU32(Length) || !Need(static_cast<int32>(Length)))
		{
			return false;
		}
		FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Bytes.GetData() + Offset), Length);
		OutValue = FString(Converted.Length(), Converted.Get());
		Offset += Length;
		return true;
	};
	auto ReadFloatArray = [&Bytes, &Offset, &Need](TArray<float>& OutValues, int32 Count) -> bool
	{
		if (!Need(Count * 4))
		{
			return false;
		}
		OutValues.SetNum(Count);
		FMemory::Memcpy(OutValues.GetData(), Bytes.GetData() + Offset, Count * 4);
		Offset += Count * 4;
		return true;
	};
	auto ReadIntArray = [&Bytes, &Offset, &Need](TArray<int32>& OutValues, int32 Count) -> bool
	{
		if (!Need(Count * 4))
		{
			return false;
		}
		OutValues.SetNum(Count);
		FMemory::Memcpy(OutValues.GetData(), Bytes.GetData() + Offset, Count * 4);
		Offset += Count * 4;
		return true;
	};

	if (!Need(8) || FMemory::Memcmp(Bytes.GetData(), Magic, 8) != 0)
	{
		OutError = TEXT("Unsupported Blender GN binary cache magic.");
		return false;
	}
	Offset += 8;
	uint32 Version = 0;
	if (!ReadU32(Version) || (Version != 1 && Version != 2 && Version != 3))
	{
		OutError = TEXT("Unsupported Blender GN binary cache version.");
		return false;
	}

	OutMeshData.Reset();
	if (!ReadString(OutMeshData.ObjectName))
	{
		OutError = TEXT("Malformed Blender GN binary object name.");
		return false;
	}

	uint32 VertexFloatCount = 0;
	uint32 NormalFloatCount = 0;
	uint32 UvFloatCount = 0;
	uint32 TriangleCount = 0;
	uint32 MaterialIndexCount = 0;
	if (!ReadU32(VertexFloatCount) || !ReadU32(NormalFloatCount) || !ReadU32(UvFloatCount) || !ReadU32(TriangleCount) || !ReadU32(MaterialIndexCount))
	{
		OutError = TEXT("Malformed Blender GN binary counts.");
		return false;
	}

	TArray<float> RawVertices;
	TArray<float> RawNormals;
	TArray<float> RawUvs;
	TArray<int32> RawTriangles;
	TArray<int32> RawMaterialIndices;
	if (!ReadFloatArray(RawVertices, VertexFloatCount) ||
		!ReadFloatArray(RawNormals, NormalFloatCount) ||
		!ReadFloatArray(RawUvs, UvFloatCount) ||
		!ReadIntArray(RawTriangles, TriangleCount) ||
		!ReadIntArray(RawMaterialIndices, MaterialIndexCount))
	{
		OutError = TEXT("Malformed Blender GN binary array data.");
		return false;
	}

	for (int32 Index = 0; Index + 2 < RawVertices.Num(); Index += 3)
	{
		OutMeshData.Vertices.Add(ConvertPosition(RawVertices[Index], RawVertices[Index + 1], RawVertices[Index + 2], Scale));
	}
	for (int32 Index = 0; Index + 2 < RawNormals.Num(); Index += 3)
	{
		OutMeshData.Normals.Add(ConvertNormal(RawNormals[Index], RawNormals[Index + 1], RawNormals[Index + 2]));
	}
	OutMeshData.bImportedNormals = OutMeshData.Normals.Num() == OutMeshData.Vertices.Num();
	for (int32 Index = 0; Index + 1 < RawUvs.Num(); Index += 2)
	{
		OutMeshData.UVs.Add(FVector2D(RawUvs[Index], 1.0f - RawUvs[Index + 1]));
	}
	while (OutMeshData.UVs.Num() < OutMeshData.Vertices.Num())
	{
		OutMeshData.UVs.Add(FVector2D::ZeroVector);
	}
	for (int32 Index = 0; Index + 2 < RawTriangles.Num(); Index += 3)
	{
		OutMeshData.Triangles.Add(RawTriangles[Index]);
		OutMeshData.Triangles.Add(RawTriangles[Index + 2]);
		OutMeshData.Triangles.Add(RawTriangles[Index + 1]);
	}
	OutMeshData.MaterialIndices = MoveTemp(RawMaterialIndices);

	uint32 MaterialNameCount = 0;
	if (!ReadU32(MaterialNameCount))
	{
		OutError = TEXT("Malformed Blender GN binary material names.");
		return false;
	}
	for (uint32 Index = 0; Index < MaterialNameCount; ++Index)
	{
		FBlenderGNMaterialInfo Info;
		if (!ReadString(Info.Name))
		{
			OutError = TEXT("Malformed Blender GN binary material name.");
			return false;
		}
		OutMeshData.Materials.Add(Info);
	}
	if (Version >= 3 && MaterialNameCount > 0)
	{
		TArray<float> RawMaterialColors;
		TArray<float> RawMaterialMetallic;
		TArray<float> RawMaterialRoughness;
		if (ReadFloatArray(RawMaterialColors, static_cast<int32>(MaterialNameCount) * 4) &&
			ReadFloatArray(RawMaterialMetallic, static_cast<int32>(MaterialNameCount)) &&
			ReadFloatArray(RawMaterialRoughness, static_cast<int32>(MaterialNameCount)))
		{
			for (uint32 Index = 0; Index < MaterialNameCount; ++Index)
			{
				FBlenderGNMaterialInfo& Info = OutMeshData.Materials[Index];
				const int32 ColorOffset = static_cast<int32>(Index) * 4;
				Info.BaseColor = FLinearColor(
					RawMaterialColors[ColorOffset],
					RawMaterialColors[ColorOffset + 1],
					RawMaterialColors[ColorOffset + 2],
					RawMaterialColors[ColorOffset + 3]);
				Info.Metallic = RawMaterialMetallic[Index];
				Info.Roughness = RawMaterialRoughness[Index];
			}
		}
	}
	if (OutMeshData.Normals.Num() != OutMeshData.Vertices.Num())
	{
		OutMeshData.Normals.Reset();
	}
	NormalizeTriangleWindingAndNormals(OutMeshData);
	OutMeshData.BuildSections();
	return true;
}

FVector UBlenderGeometryNodesComponent::ConvertPosition(float X, float Y, float Z, float Scale)
{
	return FVector(X * Scale, -Y * Scale, Z * Scale);
}

FVector UBlenderGeometryNodesComponent::ConvertNormal(float X, float Y, float Z)
{
	return FVector(X, -Y, Z).GetSafeNormal();
}

namespace
{
void FlipTriangleWinding(TArray<int32>& Triangles, int32 TriangleIndex)
{
	const int32 Offset = TriangleIndex * 3;
	Swap(Triangles[Offset + 1], Triangles[Offset + 2]);
}
}

void UBlenderGeometryNodesComponent::NormalizeTriangleWindingAndNormals(FBlenderGNMeshData& MeshData)
{
	const int32 TriangleCount = MeshData.Triangles.Num() / 3;
	if (MeshData.Vertices.Num() == 0 || TriangleCount == 0)
	{
		return;
	}

	const bool bHasImportedLoopNormals = MeshData.bImportedNormals && MeshData.Normals.Num() == MeshData.Vertices.Num();
	if (bHasImportedLoopNormals)
	{
		for (FVector& Normal : MeshData.Normals)
		{
			Normal = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
		}
		for (int32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
		{
			const int32 Offset = TriangleIndex * 3;
			const int32 A = MeshData.Triangles[Offset];
			const int32 B = MeshData.Triangles[Offset + 1];
			const int32 C = MeshData.Triangles[Offset + 2];
			if (!MeshData.Vertices.IsValidIndex(A) || !MeshData.Vertices.IsValidIndex(B) || !MeshData.Vertices.IsValidIndex(C))
			{
				continue;
			}

			const FVector FaceNormal = FVector::CrossProduct(MeshData.Vertices[B] - MeshData.Vertices[A], MeshData.Vertices[C] - MeshData.Vertices[A]).GetSafeNormal();
			const FVector ImportedNormal = (MeshData.Normals[A] + MeshData.Normals[B] + MeshData.Normals[C]).GetSafeNormal();
			if (!FaceNormal.IsNearlyZero() && !ImportedNormal.IsNearlyZero() && FVector::DotProduct(FaceNormal, ImportedNormal) < 0.0f)
			{
				FlipTriangleWinding(MeshData.Triangles, TriangleIndex);
			}
		}
		return;
	}

	RecalculateNormalsForTriangleWinding(MeshData);
}

void UBlenderGeometryNodesComponent::RecalculateNormalsForTriangleWinding(FBlenderGNMeshData& MeshData)
{
	if (MeshData.Vertices.Num() == 0 || MeshData.Triangles.Num() == 0)
	{
		return;
	}

	MeshData.Normals.Reset();
	MeshData.bImportedNormals = false;
	MeshData.Normals.SetNumZeroed(MeshData.Vertices.Num());

	for (int32 Index = 0; Index + 2 < MeshData.Triangles.Num(); Index += 3)
	{
		const int32 A = MeshData.Triangles[Index];
		const int32 B = MeshData.Triangles[Index + 1];
		const int32 C = MeshData.Triangles[Index + 2];
		if (!MeshData.Vertices.IsValidIndex(A) || !MeshData.Vertices.IsValidIndex(B) || !MeshData.Vertices.IsValidIndex(C))
		{
			continue;
		}

		const FVector FaceNormal = FVector::CrossProduct(MeshData.Vertices[B] - MeshData.Vertices[A], MeshData.Vertices[C] - MeshData.Vertices[A]).GetSafeNormal();
		if (FaceNormal.IsNearlyZero())
		{
			continue;
		}

		MeshData.Normals[A] += FaceNormal;
		MeshData.Normals[B] += FaceNormal;
		MeshData.Normals[C] += FaceNormal;
	}

	for (FVector& Normal : MeshData.Normals)
	{
		Normal = Normal.IsNearlyZero() ? FVector::UpVector : Normal.GetSafeNormal();
	}
}

void UBlenderGeometryNodesComponent::CalculateTangentsAlignedWithNormals(const FBlenderGNMeshData& MeshData, const TArray<FVector>& Normals, TArray<FProcMeshTangent>& OutTangents)
{
	OutTangents.Reset();
	const int32 VertexCount = MeshData.Vertices.Num();
	if (VertexCount == 0)
	{
		return;
	}

	OutTangents.SetNum(VertexCount);
	TArray<FVector> TangentSums;
	TArray<FVector> BitangentSums;
	TangentSums.Init(FVector::ZeroVector, VertexCount);
	BitangentSums.Init(FVector::ZeroVector, VertexCount);

	for (int32 Index = 0; Index + 2 < MeshData.Triangles.Num(); Index += 3)
	{
		const int32 A = MeshData.Triangles[Index];
		const int32 B = MeshData.Triangles[Index + 1];
		const int32 C = MeshData.Triangles[Index + 2];
		if (!MeshData.Vertices.IsValidIndex(A) || !MeshData.Vertices.IsValidIndex(B) || !MeshData.Vertices.IsValidIndex(C) ||
			!MeshData.UVs.IsValidIndex(A) || !MeshData.UVs.IsValidIndex(B) || !MeshData.UVs.IsValidIndex(C))
		{
			continue;
		}

		const FVector Edge1 = MeshData.Vertices[B] - MeshData.Vertices[A];
		const FVector Edge2 = MeshData.Vertices[C] - MeshData.Vertices[A];
		const FVector2D DeltaUV1 = MeshData.UVs[B] - MeshData.UVs[A];
		const FVector2D DeltaUV2 = MeshData.UVs[C] - MeshData.UVs[A];
		const double Denominator = DeltaUV1.X * DeltaUV2.Y - DeltaUV2.X * DeltaUV1.Y;
		if (FMath::IsNearlyZero(Denominator, 1.0e-8))
		{
			continue;
		}

		const double InvDenominator = 1.0 / Denominator;
		const FVector Tangent = (Edge1 * DeltaUV2.Y - Edge2 * DeltaUV1.Y) * InvDenominator;
		const FVector Bitangent = (Edge2 * DeltaUV1.X - Edge1 * DeltaUV2.X) * InvDenominator;
		TangentSums[A] += Tangent;
		TangentSums[B] += Tangent;
		TangentSums[C] += Tangent;
		BitangentSums[A] += Bitangent;
		BitangentSums[B] += Bitangent;
		BitangentSums[C] += Bitangent;
	}

	for (int32 Index = 0; Index < VertexCount; ++Index)
	{
		const FVector Normal = Normals.IsValidIndex(Index) && !Normals[Index].IsNearlyZero() ? Normals[Index].GetSafeNormal() : FVector::UpVector;
		FVector Tangent = TangentSums[Index] - Normal * FVector::DotProduct(Normal, TangentSums[Index]);
		if (Tangent.IsNearlyZero())
		{
			const FVector ReferenceAxis = FMath::Abs(Normal.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
			Tangent = ReferenceAxis - Normal * FVector::DotProduct(Normal, ReferenceAxis);
		}
		Tangent = Tangent.IsNearlyZero() ? FVector::RightVector : Tangent.GetSafeNormal();

		const bool bFlipTangentY = FVector::DotProduct(FVector::CrossProduct(Normal, Tangent), BitangentSums[Index]) < 0.0f;
		OutTangents[Index] = FProcMeshTangent(Tangent, bFlipTangentY);
	}
}

void UBlenderGeometryNodesComponent::SmoothNormalsByPosition(FBlenderGNMeshData& MeshData, float Quantization)
{
	if (MeshData.Vertices.Num() == 0 || MeshData.Normals.Num() != MeshData.Vertices.Num())
	{
		return;
	}

	TMap<FIntVector, FVector> Sums;
	const float SafeQuantization = FMath::Max(1.0f, Quantization);
	for (int32 Index = 0; Index < MeshData.Vertices.Num(); ++Index)
	{
		const FVector& V = MeshData.Vertices[Index];
		const FIntVector Key(
			FMath::RoundToInt(V.X * SafeQuantization),
			FMath::RoundToInt(V.Y * SafeQuantization),
			FMath::RoundToInt(V.Z * SafeQuantization));
		Sums.FindOrAdd(Key) += MeshData.Normals[Index];
	}

	for (int32 Index = 0; Index < MeshData.Normals.Num(); ++Index)
	{
		const FVector& V = MeshData.Vertices[Index];
		const FIntVector Key(
			FMath::RoundToInt(V.X * SafeQuantization),
			FMath::RoundToInt(V.Y * SafeQuantization),
			FMath::RoundToInt(V.Z * SafeQuantization));
		if (const FVector* Sum = Sums.Find(Key))
		{
			MeshData.Normals[Index] = Sum->IsNearlyZero() ? MeshData.Normals[Index] : Sum->GetSafeNormal();
		}
	}
}

float UBlenderGeometryNodesComponent::Hash01(int32 Seed)
{
	uint32 X = static_cast<uint32>(Seed);
	X ^= X >> 16;
	X *= 0x7feb352d;
	X ^= X >> 15;
	X *= 0x846ca68b;
	X ^= X >> 16;
	return static_cast<float>(X & 0x00FFFFFF) / 16777215.0f;
}

FString UBlenderGeometryNodesComponent::FindDefaultBlenderPath()
{
	const TCHAR* Candidates[] =
	{
		TEXT("C:/Program Files/Blender Foundation/Blender 5.1/blender.exe"),
		TEXT("C:/Program Files/Blender Foundation/Blender 5.0/blender.exe"),
		TEXT("C:/Program Files/Blender Foundation/Blender 4.4/blender.exe"),
		TEXT("C:/Program Files/Blender Foundation/Blender 4.3/blender.exe")
	};
	for (const TCHAR* Candidate : Candidates)
	{
		if (FPaths::FileExists(Candidate))
		{
			return Candidate;
		}
	}
	return FString();
}

FString UBlenderGeometryNodesComponent::GetDefaultSidecarPath()
{
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("OpenClawBlenderGeometryNodes"));
	return Plugin.IsValid() ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Content/Python/unreal_gn_eval.py")) : FString();
}
