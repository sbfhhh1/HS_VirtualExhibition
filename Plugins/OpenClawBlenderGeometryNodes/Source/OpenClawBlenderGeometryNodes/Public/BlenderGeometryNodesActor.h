#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlenderGeometryNodesActor.generated.h"

class UBlenderGeometryNodesComponent;
class UProceduralMeshComponent;

UCLASS(Blueprintable, meta = (DisplayName = "Blender Geometry Nodes Actor"))
class OPENCLAWBLENDERGEOMETRYNODES_API ABlenderGeometryNodesActor : public AActor
{
	GENERATED_BODY()

public:
	ABlenderGeometryNodesActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Geometry Nodes")
	TObjectPtr<UProceduralMeshComponent> ProceduralMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Geometry Nodes")
	TObjectPtr<UBlenderGeometryNodesComponent> GeometryNodes;
};
