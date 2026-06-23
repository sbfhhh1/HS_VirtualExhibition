#pragma once

#include "CoreMinimal.h"
#include "BlenderGeometryNodesTypes.generated.h"

UENUM(BlueprintType)
enum class EBlenderGNRefreshMode : uint8
{
	Manual,
	OnParameterChange,
	FixedInterval
};

UENUM(BlueprintType)
enum class EBlenderGNParameterType : uint8
{
	Float,
	Int,
	Bool,
	Vector3
};

USTRUCT(BlueprintType)
struct OPENCLAWBLENDERGEOMETRYNODES_API FBlenderGNInput
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry Nodes")
	FString DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry Nodes")
	FString SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry Nodes")
	FString FallbackIdentifier;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry Nodes")
	EBlenderGNParameterType Type = EBlenderGNParameterType::Int;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry Nodes", meta = (ClampMin = "0"))
	int32 IntValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry Nodes")
	float FloatValue = 0.16f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry Nodes")
	bool BoolValue = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry Nodes")
	FVector VectorValue = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry Nodes")
	bool bShowInRuntimePanel = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry Nodes")
	float SliderMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry Nodes")
	float SliderMax = 1.0f;

	FString GetBestName() const;
	bool MatchesName(const FString& Query) const;
};

USTRUCT()
struct OPENCLAWBLENDERGEOMETRYNODES_API FBlenderGNMeshSection
{
	GENERATED_BODY()

	UPROPERTY()
	TArray<int32> Triangles;
};

USTRUCT(BlueprintType)
struct OPENCLAWBLENDERGEOMETRYNODES_API FBlenderGNMaterialInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	FString Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	FLinearColor BaseColor = FLinearColor(0.72f, 0.72f, 0.68f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	float Metallic = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Material")
	float Roughness = 0.45f;
};

USTRUCT(BlueprintType)
struct OPENCLAWBLENDERGEOMETRYNODES_API FBlenderGNMeshData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	FString ObjectName;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<FVector> Vertices;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<FVector> Normals;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	bool bImportedNormals = false;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<FVector2D> UVs;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<int32> Triangles;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<int32> MaterialIndices;

	UPROPERTY(BlueprintReadOnly, Category = "Mesh")
	TArray<FBlenderGNMaterialInfo> Materials;

	UPROPERTY()
	TArray<FBlenderGNMeshSection> Sections;

	void BuildSections();
	int32 GetTriangleCount() const { return Triangles.Num() / 3; }
	void Reset();
};
