#pragma once

#include "CoreMinimal.h"
#include "BlenderGeometryNodesTypes.h"
#include "Components/ActorComponent.h"
#include "ProceduralMeshComponent.h"
#include "BlenderGNFastDynamicMeshComponent.h"
#include "BlenderGeometryNodesComponent.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBlenderGNMeshUpdatedSignature, UProceduralMeshComponent*, ProceduralMesh);

UCLASS(ClassGroup = (OpenClaw), BlueprintType, Blueprintable, meta = (BlueprintSpawnableComponent))
class OPENCLAWBLENDERGEOMETRYNODES_API UBlenderGeometryNodesComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBlenderGeometryNodesComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blender Source")
	FString BlenderExecutable;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blender Source", meta = (FilePathFilter = "blend"))
	FFilePath BlendFile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blender Source")
	FString ObjectName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blender Source")
	FString ModifierName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Blender Source")
	FString SidecarScriptPath;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refresh")
	EBlenderGNRefreshMode RefreshMode = EBlenderGNRefreshMode::Manual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refresh", meta = (ClampMin = "1", ClampMax = "120"))
	int32 BlenderTimeoutSeconds = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refresh", meta = (ClampMin = "0.02", ClampMax = "2.0"))
	float MinimumRefreshInterval = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refresh", meta = (ClampMin = "0.1", ClampMax = "10.0"))
	float FixedRefreshInterval = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refresh", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float BlenderFrameRate = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refresh")
	int32 BlenderFrameOffset = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refresh")
	bool bRefreshOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Refresh")
	bool bUseBinaryMeshCache = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry Nodes", meta = (TitleProperty = "DisplayName"))
	TArray<FBlenderGNInput> Inputs;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation")
	bool bEnablePreviewAnimation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PreviewAnimationSmoothness = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Animation", meta = (ClampMin = "1.0", ClampMax = "120.0"))
	float PreviewAnimationFrameRate = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	TObjectPtr<UMaterialInterface> FallbackMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	TArray<TObjectPtr<UMaterialInterface>> MaterialOverrides;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	bool bApplyImportedNormals = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	bool bSmoothNormalsByPosition = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	bool bCastShadows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bForceDefaultMaterial = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bInvertShadingNormals = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output", meta = (ClampMin = "1000", ClampMax = "100000"))
	float NormalSmoothQuantization = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Output")
	float BlenderToUnrealScale = 100.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	bool bIsRunning = false;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	bool bHasPendingRefresh = false;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	FString LastStatus = TEXT("Idle");

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	float LastEvaluationSeconds = 0.0f;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	int32 LastVertexCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	int32 LastTriangleCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	int32 LastPreviewIslandCount = 0;

	UPROPERTY(Transient, VisibleAnywhere, BlueprintReadOnly, Category = "Status")
	FString LastPreviewAnimationStatus = TEXT("Not built");

	UPROPERTY(BlueprintAssignable, Category = "Geometry Nodes")
	FBlenderGNMeshUpdatedSignature OnMeshUpdated;

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	void RefreshNow();

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	void RequestRefresh(bool bForce);

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	void NotifyParametersChanged();

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	void SetFloatInput(const FString& SocketName, float Value, bool bRefresh = true);

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	void SetIntInput(const FString& SocketName, int32 Value, bool bRefresh = true);

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	void SetBoolInput(const FString& SocketName, bool Value, bool bRefresh = true);

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	void SetVectorInput(const FString& SocketName, FVector Value, bool bRefresh = true);

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	float GetFloatInput(const FString& SocketName, float Fallback = 0.0f) const;

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	int32 GetIntInput(const FString& SocketName, int32 Fallback = 0) const;

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	bool GetBoolInput(const FString& SocketName, bool Fallback = false) const;

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	FVector GetVectorInput(const FString& SocketName, FVector Fallback) const;

	UFUNCTION(BlueprintCallable, Category = "Geometry Nodes")
	UProceduralMeshComponent* GetProceduralMesh() const;

	const FBlenderGNMeshData& GetLastMeshData() const { return LastMeshData; }
	const TArray<FProcMeshTangent>& GetLastTangents() const { return PreviewStableTangents; }
	FBlenderGNMeshData& GetMutableLastMeshData() { return LastMeshData; }
	void ApplyMeshData(const FBlenderGNMeshData& MeshData);
	void ReapplyLastMeshData();
	bool RestoreLastMeshDataFromProceduralMesh();
	void DisablePreviewAnimationMeshes();
	struct FEvalResult
	{
		bool bSuccess = false;
		FString Status;
		float ElapsedSeconds = 0.0f;
		FBlenderGNMeshData MeshData;
	};
	static FString FindDefaultBlenderPath();
	static FString GetDefaultSidecarPath();

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> CachedProceduralMesh;

	UPROPERTY(Transient)
	TObjectPtr<UBlenderGNFastDynamicMeshComponent> FastDynamicMesh;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> PreviewIslandMeshes;

	UPROPERTY(Transient)
	FBlenderGNMeshData LastMeshData;

	UPROPERTY(Transient)
	FVector LastMeshCenter = FVector::ZeroVector;

	UPROPERTY(Transient)
	TArray<int32> PreviewVertexIsland;

	UPROPERTY(Transient)
	TArray<FVector> PreviewIslandNormals;

	UPROPERTY(Transient)
	TArray<float> PreviewIslandPhases;

	UPROPERTY(Transient)
	TArray<FVector> PreviewIslandOffsets;

	UPROPERTY(Transient)
	TArray<FVector> PreviewAnimatedVertices;

	TArray<FVector3f> PreviewAnimatedPositions;

	UPROPERTY(Transient)
	TArray<FVector> PreviewStableNormals;

	UPROPERTY(Transient)
	TArray<FProcMeshTangent> PreviewStableTangents;

	UPROPERTY(Transient)
	TArray<FLinearColor> PreviewVertexColors;

	FString LastRequestHash;
	FString ActiveRequestHash;
	double LastRefreshStartTime = -999.0;
	double LastPreviewAnimationTime = -999.0;
	double NextFixedRefreshTime = 0.0;
	double QueuedRefreshTime = 0.0;

	void EnsureDefaults();
	bool BuildMeshDataFromProceduralMesh(const UProceduralMeshComponent* ProceduralMesh, FBlenderGNMeshData& OutMeshData) const;
	void RequestRefreshForParameterChange();
	void ApplyPreviewAnimation(double TimeSeconds);
	void BuildPreviewAnimationCache();
	void EnsureFastDynamicMesh();
	bool EnsurePreviewIslandMeshes();
	void DestroyPreviewIslandMeshes();
	UProceduralMeshComponent* EnsureProceduralMesh();
	FBlenderGNInput* FindInput(const FString& SocketName);
	const FBlenderGNInput* FindInput(const FString& SocketName) const;
	FString BuildRequestHash() const;
	FString BuildRequestJson(const FString& MeshOutputPath) const;
	FString ValidatePaths(FString& OutScriptPath) const;
	void StartBackgroundRefresh(const FString& RequestHash);
	FString BuildBlenderArguments(const FString& ScriptPath, const FString& RequestPath) const;
	static bool ReadJsonMeshCache(const FString& JsonText, float Scale, FBlenderGNMeshData& OutMeshData, FString& OutError);
	static bool ReadBinaryMeshCache(const FString& FilePath, float Scale, FBlenderGNMeshData& OutMeshData, FString& OutError);
	static FVector ConvertPosition(float X, float Y, float Z, float Scale);
	static FVector ConvertNormal(float X, float Y, float Z);
	static void NormalizeTriangleWindingAndNormals(FBlenderGNMeshData& MeshData);
	static void RecalculateNormalsForTriangleWinding(FBlenderGNMeshData& MeshData);
	static void CalculateTangentsAlignedWithNormals(const FBlenderGNMeshData& MeshData, const TArray<FVector>& Normals, TArray<FProcMeshTangent>& OutTangents);
	static void SmoothNormalsByPosition(FBlenderGNMeshData& MeshData, float Quantization);
	static float Hash01(int32 Seed);
};
