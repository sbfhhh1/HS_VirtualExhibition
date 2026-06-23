#include "BlenderGeometryNodesActor.h"

#include "BlenderGeometryNodesComponent.h"
#include "ProceduralMeshComponent.h"

ABlenderGeometryNodesActor::ABlenderGeometryNodesActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ProceduralMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMesh"));
	RootComponent = ProceduralMesh;
	ProceduralMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GeometryNodes = CreateDefaultSubobject<UBlenderGeometryNodesComponent>(TEXT("GeometryNodes"));
}
