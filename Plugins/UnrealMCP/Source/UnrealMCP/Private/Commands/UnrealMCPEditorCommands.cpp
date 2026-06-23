#include "Commands/UnrealMCPEditorCommands.h"
#include "Commands/UnrealMCPCommonUtils.h"
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#include "ImageUtils.h"
#include "HighResScreenshot.h"
#include "Engine/GameViewportClient.h"
#include "Misc/FileHelper.h"
#include "GameFramework/Actor.h"
#include "FileHelpers.h"
#include "Engine/Selection.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/DirectionalLight.h"
#include "Engine/PointLight.h"
#include "Engine/SpotLight.h"
#include "Camera/CameraActor.h"
#include "Components/StaticMeshComponent.h"
#include "BlenderGeometryNodesComponent.h"
#include "EditorSubsystem.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "EditorAssetLibrary.h"
#include "IPythonScriptPlugin.h"

FUnrealMCPEditorCommands::FUnrealMCPEditorCommands()
{
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleCommand(const FString& CommandType, const TSharedPtr<FJsonObject>& Params)
{
    // Actor manipulation commands
    if (CommandType == TEXT("get_actors_in_level"))
    {
        return HandleGetActorsInLevel(Params);
    }
    else if (CommandType == TEXT("find_actors_by_name"))
    {
        return HandleFindActorsByName(Params);
    }
    else if (CommandType == TEXT("spawn_actor") || CommandType == TEXT("create_actor"))
    {
        if (CommandType == TEXT("create_actor"))
        {
            UE_LOG(LogTemp, Warning, TEXT("'create_actor' command is deprecated and will be removed in a future version. Please use 'spawn_actor' instead."));
        }
        return HandleSpawnActor(Params);
    }
    else if (CommandType == TEXT("delete_actor"))
    {
        return HandleDeleteActor(Params);
    }
    else if (CommandType == TEXT("set_actor_transform"))
    {
        return HandleSetActorTransform(Params);
    }
    else if (CommandType == TEXT("get_actor_properties"))
    {
        return HandleGetActorProperties(Params);
    }
    else if (CommandType == TEXT("set_actor_property"))
    {
        return HandleSetActorProperty(Params);
    }
    // Blueprint actor spawning
    else if (CommandType == TEXT("spawn_blueprint_actor"))
    {
        return HandleSpawnBlueprintActor(Params);
    }
    // Editor viewport commands
    else if (CommandType == TEXT("focus_viewport"))
    {
        return HandleFocusViewport(Params);
    }
    else if (CommandType == TEXT("take_screenshot"))
    {
        return HandleTakeScreenshot(Params);
    }
    else if (CommandType == TEXT("rerun_construction_scripts"))
    {
        return HandleRerunConstructionScripts(Params);
    }
    else if (CommandType == TEXT("openclaw_gn_configure_refresh"))
    {
        return HandleOpenClawGNConfigureRefresh(Params);
    }
    else if (CommandType == TEXT("openclaw_gn_get_status"))
    {
        return HandleOpenClawGNGetStatus(Params);
    }
    else if (CommandType == TEXT("openclaw_gn_dump_mesh_debug"))
    {
        return HandleOpenClawGNDumpMeshDebug(Params);
    }
    else if (CommandType == TEXT("save_all"))
    {
        return HandleSaveAll(Params);
    }
    else if (CommandType == TEXT("execute_python_file"))
    {
        return HandleExecutePythonFile(Params);
    }
    else if (CommandType == TEXT("open_level"))
    {
        return HandleOpenLevel(Params);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown editor command: %s"), *CommandType));
}

namespace
{
    AActor* FindActorByExactName(const FString& ActorName)
    {
        UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
        if (!World)
        {
            return nullptr;
        }

        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
        for (AActor* Actor : AllActors)
        {
            if (Actor && (Actor->GetName() == ActorName || Actor->GetActorLabel() == ActorName))
            {
                return Actor;
            }
        }
        return nullptr;
    }

    FLevelEditorViewportClient* GetUsableLevelViewportClient()
    {
        if (!GEditor)
        {
            return nullptr;
        }

        if (FViewport* ActiveViewport = GEditor->GetActiveViewport())
        {
            if (FViewportClient* ActiveClient = ActiveViewport->GetClient())
            {
                return static_cast<FLevelEditorViewportClient*>(ActiveClient);
            }
        }

        for (FLevelEditorViewportClient* ViewportClient : GEditor->GetLevelViewportClients())
        {
            if (ViewportClient && ViewportClient->Viewport)
            {
                return ViewportClient;
            }
        }

        return nullptr;
    }

    void ApplyOpenClawGNInputValue(FBlenderGNInput& Input, const TSharedPtr<FJsonObject>& InputObject)
    {
        if (!InputObject.IsValid())
        {
            return;
        }

        FString Type;
        InputObject->TryGetStringField(TEXT("type"), Type);
        if (InputObject->HasField(TEXT("display_name")))
        {
            Input.DisplayName = InputObject->GetStringField(TEXT("display_name"));
        }
        if (InputObject->HasField(TEXT("socket_name")))
        {
            Input.SocketName = InputObject->GetStringField(TEXT("socket_name"));
        }
        else if (InputObject->HasField(TEXT("name")))
        {
            Input.SocketName = InputObject->GetStringField(TEXT("name"));
        }
        if (Input.SocketName.IsEmpty())
        {
            Input.SocketName = Input.DisplayName;
        }
        if (!Input.SocketName.IsEmpty())
        {
            Input.DisplayName = Input.SocketName;
        }
        if (InputObject->HasField(TEXT("fallback_identifier")))
        {
            Input.FallbackIdentifier = InputObject->GetStringField(TEXT("fallback_identifier"));
        }
        if (InputObject->HasField(TEXT("slider_min")))
        {
            Input.SliderMin = InputObject->GetNumberField(TEXT("slider_min"));
        }
        if (InputObject->HasField(TEXT("slider_max")))
        {
            Input.SliderMax = InputObject->GetNumberField(TEXT("slider_max"));
        }

        if (Type.Equals(TEXT("int"), ESearchCase::IgnoreCase))
        {
            Input.Type = EBlenderGNParameterType::Int;
            Input.IntValue = static_cast<int32>(InputObject->GetNumberField(TEXT("value")));
        }
        else if (Type.Equals(TEXT("bool"), ESearchCase::IgnoreCase))
        {
            Input.Type = EBlenderGNParameterType::Bool;
            Input.BoolValue = InputObject->GetBoolField(TEXT("value"));
        }
        else if (Type.Equals(TEXT("vector3"), ESearchCase::IgnoreCase) || Type.Equals(TEXT("vector"), ESearchCase::IgnoreCase))
        {
            Input.Type = EBlenderGNParameterType::Vector3;
            const TSharedPtr<FJsonObject>* VectorObject = nullptr;
            if (InputObject->TryGetObjectField(TEXT("value"), VectorObject) && VectorObject && VectorObject->IsValid())
            {
                Input.VectorValue = FVector(
                    (*VectorObject)->GetNumberField(TEXT("x")),
                    (*VectorObject)->GetNumberField(TEXT("y")),
                    (*VectorObject)->GetNumberField(TEXT("z")));
            }
            else
            {
                const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
                if (InputObject->TryGetArrayField(TEXT("value"), Values) && Values && Values->Num() >= 3)
                {
                    Input.VectorValue = FVector((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), (*Values)[2]->AsNumber());
                }
            }
        }
        else
        {
            Input.Type = EBlenderGNParameterType::Float;
            Input.FloatValue = InputObject->GetNumberField(TEXT("value"));
        }
    }

    TArray<TSharedPtr<FJsonValue>> MakeFlatVectorArray(const TArray<FVector>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        Result.Reserve(Values.Num() * 3);
        for (const FVector& Value : Values)
        {
            Result.Add(MakeShared<FJsonValueNumber>(Value.X));
            Result.Add(MakeShared<FJsonValueNumber>(Value.Y));
            Result.Add(MakeShared<FJsonValueNumber>(Value.Z));
        }
        return Result;
    }

    TArray<TSharedPtr<FJsonValue>> MakeFlatVector2DArray(const TArray<FVector2D>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        Result.Reserve(Values.Num() * 2);
        for (const FVector2D& Value : Values)
        {
            Result.Add(MakeShared<FJsonValueNumber>(Value.X));
            Result.Add(MakeShared<FJsonValueNumber>(Value.Y));
        }
        return Result;
    }

    TArray<TSharedPtr<FJsonValue>> MakeIntArray(const TArray<int32>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        Result.Reserve(Values.Num());
        for (const int32 Value : Values)
        {
            Result.Add(MakeShared<FJsonValueNumber>(Value));
        }
        return Result;
    }

    TArray<TSharedPtr<FJsonValue>> MakeTangentArray(const TArray<FProcMeshTangent>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> Result;
        Result.Reserve(Values.Num() * 4);
        for (const FProcMeshTangent& Value : Values)
        {
            Result.Add(MakeShared<FJsonValueNumber>(Value.TangentX.X));
            Result.Add(MakeShared<FJsonValueNumber>(Value.TangentX.Y));
            Result.Add(MakeShared<FJsonValueNumber>(Value.TangentX.Z));
            Result.Add(MakeShared<FJsonValueBoolean>(Value.bFlipTangentY));
        }
        return Result;
    }

    TSharedPtr<FJsonObject> BuildOpenClawGNMeshStats(const FBlenderGNMeshData& MeshData)
    {
        const bool bHasImportedNormals = MeshData.bImportedNormals && MeshData.Normals.Num() == MeshData.Vertices.Num();
        int32 InvalidTriangles = 0;
        int32 DegenerateTriangles = 0;
        int32 ComparedTriangles = 0;
        int32 NegativeDotTriangles = 0;
        int32 LowDotTriangles = 0;
        double MinDot = 1.0;
        double MaxDot = -1.0;
        double SumDot = 0.0;

        for (int32 Index = 0; Index + 2 < MeshData.Triangles.Num(); Index += 3)
        {
            const int32 A = MeshData.Triangles[Index];
            const int32 B = MeshData.Triangles[Index + 1];
            const int32 C = MeshData.Triangles[Index + 2];
            if (!MeshData.Vertices.IsValidIndex(A) || !MeshData.Vertices.IsValidIndex(B) || !MeshData.Vertices.IsValidIndex(C))
            {
                ++InvalidTriangles;
                continue;
            }

            const FVector FaceNormal = FVector::CrossProduct(MeshData.Vertices[B] - MeshData.Vertices[A], MeshData.Vertices[C] - MeshData.Vertices[A]).GetSafeNormal();
            if (FaceNormal.IsNearlyZero())
            {
                ++DegenerateTriangles;
                continue;
            }
            if (!bHasImportedNormals)
            {
                continue;
            }

            const FVector ImportedNormal = (MeshData.Normals[A] + MeshData.Normals[B] + MeshData.Normals[C]).GetSafeNormal();
            if (ImportedNormal.IsNearlyZero())
            {
                continue;
            }

            const double Dot = FVector::DotProduct(FaceNormal, ImportedNormal);
            MinDot = FMath::Min(MinDot, Dot);
            MaxDot = FMath::Max(MaxDot, Dot);
            SumDot += Dot;
            ++ComparedTriangles;
            if (Dot < 0.0)
            {
                ++NegativeDotTriangles;
            }
            if (Dot < 0.2)
            {
                ++LowDotTriangles;
            }
        }

        TSharedPtr<FJsonObject> Stats = MakeShared<FJsonObject>();
        Stats->SetNumberField(TEXT("vertices"), MeshData.Vertices.Num());
        Stats->SetNumberField(TEXT("triangles"), MeshData.Triangles.Num() / 3);
        Stats->SetNumberField(TEXT("normals"), MeshData.Normals.Num());
        Stats->SetNumberField(TEXT("uvs"), MeshData.UVs.Num());
        Stats->SetNumberField(TEXT("sections"), MeshData.Sections.Num());
        Stats->SetBoolField(TEXT("has_imported_normals"), bHasImportedNormals);
        Stats->SetNumberField(TEXT("invalid_triangles"), InvalidTriangles);
        Stats->SetNumberField(TEXT("degenerate_triangles"), DegenerateTriangles);
        Stats->SetNumberField(TEXT("compared_normal_triangles"), ComparedTriangles);
        Stats->SetNumberField(TEXT("negative_dot_triangles"), NegativeDotTriangles);
        Stats->SetNumberField(TEXT("low_dot_triangles"), LowDotTriangles);
        Stats->SetNumberField(TEXT("min_face_imported_dot"), ComparedTriangles > 0 ? MinDot : 0.0);
        Stats->SetNumberField(TEXT("max_face_imported_dot"), ComparedTriangles > 0 ? MaxDot : 0.0);
        Stats->SetNumberField(TEXT("average_face_imported_dot"), ComparedTriangles > 0 ? SumDot / ComparedTriangles : 0.0);
        return Stats;
    }
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorsInLevel(const TSharedPtr<FJsonObject>& Params)
{
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> ActorArray;
    for (AActor* Actor : AllActors)
    {
        if (Actor)
        {
            ActorArray.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), ActorArray);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFindActorsByName(const TSharedPtr<FJsonObject>& Params)
{
    FString Pattern;
    if (!Params->TryGetStringField(TEXT("pattern"), Pattern))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'pattern' parameter"));
    }
    
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    TArray<TSharedPtr<FJsonValue>> MatchingActors;
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName().Contains(Pattern))
        {
            MatchingActors.Add(FUnrealMCPCommonUtils::ActorToJson(Actor));
        }
    }
    
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetArrayField(TEXT("actors"), MatchingActors);
    
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString ActorType;
    if (!Params->TryGetStringField(TEXT("type"), ActorType))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'type' parameter"));
    }

    // Get actor name (required parameter)
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Get optional transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Create the actor based on type
    AActor* NewActor = nullptr;
    UWorld* World = GEditor->GetEditorWorldContext().World();

    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    // Check if an actor with this name already exists
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor with name '%s' already exists"), *ActorName));
        }
    }

    FActorSpawnParameters SpawnParams;

    if (ActorType == TEXT("StaticMeshActor"))
    {
        NewActor = World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("PointLight"))
    {
        NewActor = World->SpawnActor<APointLight>(APointLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("SpotLight"))
    {
        NewActor = World->SpawnActor<ASpotLight>(ASpotLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("DirectionalLight"))
    {
        NewActor = World->SpawnActor<ADirectionalLight>(ADirectionalLight::StaticClass(), Location, Rotation, SpawnParams);
    }
    else if (ActorType == TEXT("CameraActor"))
    {
        NewActor = World->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), Location, Rotation, SpawnParams);
    }
    else
    {
        // Generic fallback: try to find the class by name (supports custom C++ actors)
        UClass* FoundClass = nullptr;

        // Strategy 1: Search all UClass objects for a matching short name
        for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
        {
            if (ClassIt->GetName() == ActorType || ClassIt->GetFName().ToString() == ActorType)
            {
                if (ClassIt->IsChildOf<AActor>() && !ClassIt->HasAnyClassFlags(CLASS_Abstract))
                {
                    FoundClass = *ClassIt;
                    break;
                }
            }
        }

        // Strategy 2: Try as full path (e.g. "/Script/MCPGameProject.MetalSphereGrid")
        if (!FoundClass)
        {
            FoundClass = LoadObject<UClass>(nullptr, *ActorType);
        }

        // Validate it's an Actor subclass
        if (FoundClass && FoundClass->IsChildOf<AActor>())
        {
            NewActor = World->SpawnActor<AActor>(FoundClass, Location, Rotation, SpawnParams);
        }
        else
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Unknown actor type: %s"), *ActorType));
        }
    }

    if (NewActor)
    {
        NewActor->SetActorLabel(*ActorName);

        // Set scale (since SpawnActor only takes location and rotation)
        FTransform Transform = NewActor->GetTransform();
        Transform.SetScale3D(Scale);
        NewActor->SetActorTransform(Transform);

        // Rerun construction scripts so OnConstruction executes (e.g. for procedural geometry)
        NewActor->RerunConstructionScripts();

        // Return the created actor's details
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to create actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleDeleteActor(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            // Store actor info before deletion for the response
            TSharedPtr<FJsonObject> ActorInfo = FUnrealMCPCommonUtils::ActorToJsonObject(Actor);
            
            // Delete the actor
            Actor->Destroy();
            
            TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
            ResultObj->SetObjectField(TEXT("deleted_actor"), ActorInfo);
            return ResultObj;
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorTransform(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get transform parameters
    FTransform NewTransform = TargetActor->GetTransform();

    if (Params->HasField(TEXT("location")))
    {
        NewTransform.SetLocation(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location")));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        NewTransform.SetRotation(FQuat(FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"))));
    }
    if (Params->HasField(TEXT("scale")))
    {
        NewTransform.SetScale3D(FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale")));
    }

    // Set the new transform
    TargetActor->SetActorTransform(NewTransform);

    // Return updated actor info
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleGetActorProperties(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Always return detailed properties for this command
    return FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true);
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSetActorProperty(const TSharedPtr<FJsonObject>& Params)
{
    // Get actor name
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'name' parameter"));
    }

    // Find the actor
    AActor* TargetActor = nullptr;
    TArray<AActor*> AllActors;
    UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
    
    for (AActor* Actor : AllActors)
    {
        if (Actor && Actor->GetName() == ActorName)
        {
            TargetActor = Actor;
            break;
        }
    }

    if (!TargetActor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    // Get property name
    FString PropertyName;
    if (!Params->TryGetStringField(TEXT("property_name"), PropertyName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_name' parameter"));
    }

    // Get property value
    if (!Params->HasField(TEXT("property_value")))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'property_value' parameter"));
    }
    
    TSharedPtr<FJsonValue> PropertyValue = Params->Values.FindRef(TEXT("property_value"));
    
    // Set the property using our utility function
    FString ErrorMessage;
    if (FUnrealMCPCommonUtils::SetObjectProperty(TargetActor, PropertyName, PropertyValue, ErrorMessage))
    {
        // Property set successfully
        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetStringField(TEXT("actor"), ActorName);
        ResultObj->SetStringField(TEXT("property"), PropertyName);
        ResultObj->SetBoolField(TEXT("success"), true);
        
        // Also include the full actor details
        ResultObj->SetObjectField(TEXT("actor_details"), FUnrealMCPCommonUtils::ActorToJsonObject(TargetActor, true));
        return ResultObj;
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(ErrorMessage);
    }
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSpawnBlueprintActor(const TSharedPtr<FJsonObject>& Params)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    // Find the blueprint
    if (BlueprintName.IsEmpty())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Blueprint name is empty"));
    }

    FString AssetPath = BlueprintName.StartsWith(TEXT("/Game/"))
        ? BlueprintName
        : TEXT("/Game/Blueprints/") + BlueprintName;

    if (!FPackageName::IsValidLongPackageName(AssetPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(
            FString::Printf(TEXT("Invalid blueprint asset path: %s"), *AssetPath));
    }

    // Try to load the blueprint directly (works even if not saved to disk yet)
    UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    
    // If not found by direct load, check if package exists on disk
    if (!Blueprint && FPackageName::DoesPackageExist(AssetPath))
    {
        Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath);
    }

    if (!Blueprint)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    // Get transform parameters
    FVector Location(0.0f, 0.0f, 0.0f);
    FRotator Rotation(0.0f, 0.0f, 0.0f);
    FVector Scale(1.0f, 1.0f, 1.0f);

    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
    }
    if (Params->HasField(TEXT("rotation")))
    {
        Rotation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("rotation"));
    }
    if (Params->HasField(TEXT("scale")))
    {
        Scale = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("scale"));
    }

    // Spawn the actor
    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(Location);
    SpawnTransform.SetRotation(FQuat(Rotation));
    SpawnTransform.SetScale3D(Scale);

    FActorSpawnParameters SpawnParams;

    AActor* NewActor = World->SpawnActor<AActor>(Blueprint->GeneratedClass, SpawnTransform, SpawnParams);
    if (NewActor)
    {
        NewActor->SetActorLabel(*ActorName);
        return FUnrealMCPCommonUtils::ActorToJsonObject(NewActor, true);
    }

    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to spawn blueprint actor"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleFocusViewport(const TSharedPtr<FJsonObject>& Params)
{
    // Get target actor name if provided
    FString TargetActorName;
    bool HasTargetActor = Params->TryGetStringField(TEXT("target"), TargetActorName);

    // Get location if provided
    FVector Location(0.0f, 0.0f, 0.0f);
    bool HasLocation = false;
    if (Params->HasField(TEXT("location")))
    {
        Location = FUnrealMCPCommonUtils::GetVectorFromJson(Params, TEXT("location"));
        HasLocation = true;
    }

    // Get distance
    float Distance = 1000.0f;
    if (Params->HasField(TEXT("distance")))
    {
        Distance = Params->GetNumberField(TEXT("distance"));
    }

    // Get orientation if provided
    FRotator Orientation(0.0f, 0.0f, 0.0f);
    bool HasOrientation = false;
    if (Params->HasField(TEXT("orientation")))
    {
        Orientation = FUnrealMCPCommonUtils::GetRotatorFromJson(Params, TEXT("orientation"));
        HasOrientation = true;
    }

    FLevelEditorViewportClient* ViewportClient = GetUsableLevelViewportClient();
    if (!ViewportClient)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get active viewport"));
    }

    const bool bHasRealtime = Params->HasField(TEXT("realtime"));
    const FText MCPRealtimeOverrideName = FText::FromString(TEXT("UnrealMCP Realtime Viewport"));
    if (bHasRealtime)
    {
        const bool bRealtime = Params->GetBoolField(TEXT("realtime"));
        for (FLevelEditorViewportClient* Client : GEditor->GetLevelViewportClients())
        {
            if (!Client || !Client->Viewport)
            {
                continue;
            }
            Client->RemoveRealtimeOverride(MCPRealtimeOverrideName, false);
            Client->SetRealtime(bRealtime);
            Client->AddRealtimeOverride(bRealtime, MCPRealtimeOverrideName);
        }
    }

    // If we have a target actor, focus on it
    if (HasTargetActor)
    {
        // Find the actor
        AActor* TargetActor = nullptr;
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(GWorld, AActor::StaticClass(), AllActors);
        
        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetName() == TargetActorName)
            {
                TargetActor = Actor;
                break;
            }
        }

        if (!TargetActor)
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *TargetActorName));
        }

        // Focus on the actor
        ViewportClient->SetViewLocation(TargetActor->GetActorLocation() - FVector(Distance, 0.0f, 0.0f));
    }
    // Otherwise use the provided location
    else if (HasLocation)
    {
        ViewportClient->SetViewLocation(Location - FVector(Distance, 0.0f, 0.0f));
    }
    else
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Either 'target' or 'location' must be provided"));
    }

    // Set orientation if provided
    if (HasOrientation)
    {
        ViewportClient->SetViewRotation(Orientation);
    }

    // Force viewport to redraw
    ViewportClient->Invalidate();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("realtime"), ViewportClient->IsRealtime());
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleTakeScreenshot(const TSharedPtr<FJsonObject>& Params)
{
    // Get file path parameter
    FString FilePath;
    if (!Params->TryGetStringField(TEXT("filepath"), FilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'filepath' parameter"));
    }
    
    // Ensure the file path has a proper extension
    if (!FilePath.EndsWith(TEXT(".png")))
    {
        FilePath += TEXT(".png");
    }

    FLevelEditorViewportClient* ViewportClient = GetUsableLevelViewportClient();
    if (ViewportClient && ViewportClient->Viewport)
    {
        FViewport* Viewport = ViewportClient->Viewport;
        TArray<FColor> Bitmap;
        FIntRect ViewportRect(0, 0, Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y);

        if (Viewport->ReadPixels(Bitmap, FReadSurfaceDataFlags(), ViewportRect))
        {
            TArray64<uint8> CompressedBitmap;
            FImageUtils::PNGCompressImageArray(Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, TArrayView64<const FColor>(Bitmap.GetData(), Bitmap.Num()), CompressedBitmap);
            
            if (FFileHelper::SaveArrayToFile(CompressedBitmap, *FilePath))
            {
                TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
                ResultObj->SetStringField(TEXT("filepath"), FilePath);
                return ResultObj;
            }
        }
    }
    
    return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to take screenshot"));
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleRerunConstructionScripts(const TSharedPtr<FJsonObject>& Params)
{
    // 获取 Actor 名称参数
    FString ActorName;
    bool bHasActorName = Params->TryGetStringField(TEXT("name"), ActorName);

    UWorld* World = GEditor->GetEditorWorldContext().World();
    if (!World)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Failed to get editor world"));
    }

    if (bHasActorName)
    {
        // 对指定 Actor 重新运行构造脚本
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

        for (AActor* Actor : AllActors)
        {
            if (Actor && Actor->GetName() == ActorName)
            {
                Actor->RerunConstructionScripts();
                TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
                ResultObj->SetBoolField(TEXT("success"), true);
                ResultObj->SetStringField(TEXT("actor"), ActorName);
    return ResultObj;
}

        }
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }
    else
    {
        // 对所有 Actor 重新运行构造脚本
        TArray<AActor*> AllActors;
        UGameplayStatics::GetAllActorsOfClass(World, AActor::StaticClass(), AllActors);

        int32 Count = 0;
        for (AActor* Actor : AllActors)
        {
            if (Actor)
            {
                Actor->RerunConstructionScripts();
                ++Count;
            }
        }

        TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
        ResultObj->SetBoolField(TEXT("success"), true);
        ResultObj->SetNumberField(TEXT("count"), Count);
        return ResultObj;
    }
} 

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleOpenClawGNConfigureRefresh(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) && !Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    AActor* Actor = FindActorByExactName(ActorName);
    if (!Actor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UBlenderGeometryNodesComponent* Component = Actor->FindComponentByClass<UBlenderGeometryNodesComponent>();
    if (!Component)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor has no UBlenderGeometryNodesComponent: %s"), *ActorName));
    }

    Component->Modify();
    FString StringValue;
    if (Params->TryGetStringField(TEXT("blender_executable"), StringValue))
    {
        Component->BlenderExecutable = StringValue;
    }
    if (Params->TryGetStringField(TEXT("blend_file"), StringValue))
    {
        Component->BlendFile.FilePath = StringValue;
    }
    if (Params->TryGetStringField(TEXT("object_name"), StringValue))
    {
        Component->ObjectName = StringValue;
    }
    if (Params->TryGetStringField(TEXT("modifier_name"), StringValue))
    {
        Component->ModifierName = StringValue;
    }
    if (Params->TryGetStringField(TEXT("sidecar_script"), StringValue))
    {
        Component->SidecarScriptPath = StringValue;
    }
    if (Params->TryGetStringField(TEXT("fallback_material"), StringValue))
    {
        Component->FallbackMaterial = Cast<UMaterialInterface>(UEditorAssetLibrary::LoadAsset(StringValue));
    }
    if (Params->HasField(TEXT("clear_fallback_material")) && Params->GetBoolField(TEXT("clear_fallback_material")))
    {
        Component->FallbackMaterial = nullptr;
    }
    if (Params->HasField(TEXT("clear_material_overrides")) && Params->GetBoolField(TEXT("clear_material_overrides")))
    {
        Component->MaterialOverrides.Reset();
    }
    if (Params->HasField(TEXT("timeout_seconds")))
    {
        Component->BlenderTimeoutSeconds = FMath::Clamp(static_cast<int32>(Params->GetNumberField(TEXT("timeout_seconds"))), 1, 120);
    }
    if (Params->HasField(TEXT("refresh_mode")))
    {
        const TSharedPtr<FJsonValue> RefreshModeValue = Params->Values.FindRef(TEXT("refresh_mode"));
        if (RefreshModeValue.IsValid() && RefreshModeValue->Type == EJson::String)
        {
            const FString RefreshModeString = RefreshModeValue->AsString();
            if (RefreshModeString.Equals(TEXT("manual"), ESearchCase::IgnoreCase))
            {
                Component->RefreshMode = EBlenderGNRefreshMode::Manual;
            }
            else if (RefreshModeString.Equals(TEXT("on_parameter_change"), ESearchCase::IgnoreCase) ||
                     RefreshModeString.Equals(TEXT("onparameterchange"), ESearchCase::IgnoreCase))
            {
                Component->RefreshMode = EBlenderGNRefreshMode::OnParameterChange;
            }
            else if (RefreshModeString.Equals(TEXT("fixed_interval"), ESearchCase::IgnoreCase) ||
                     RefreshModeString.Equals(TEXT("fixedinterval"), ESearchCase::IgnoreCase))
            {
                Component->RefreshMode = EBlenderGNRefreshMode::FixedInterval;
            }
        }
        else if (RefreshModeValue.IsValid() && RefreshModeValue->Type == EJson::Number)
        {
            const int32 RefreshModeInt = FMath::Clamp(static_cast<int32>(RefreshModeValue->AsNumber()), 0, 2);
            Component->RefreshMode = static_cast<EBlenderGNRefreshMode>(RefreshModeInt);
        }
    }
    if (Params->HasField(TEXT("fixed_refresh_interval")))
    {
        Component->FixedRefreshInterval = FMath::Clamp(static_cast<float>(Params->GetNumberField(TEXT("fixed_refresh_interval"))), 0.1f, 10.0f);
    }
    if (Params->HasField(TEXT("minimum_refresh_interval")))
    {
        Component->MinimumRefreshInterval = FMath::Clamp(static_cast<float>(Params->GetNumberField(TEXT("minimum_refresh_interval"))), 0.02f, 2.0f);
    }
    if (Params->HasField(TEXT("blender_frame_rate")))
    {
        Component->BlenderFrameRate = FMath::Clamp(static_cast<float>(Params->GetNumberField(TEXT("blender_frame_rate"))), 1.0f, 60.0f);
    }
    if (Params->HasField(TEXT("apply_imported_normals")))
    {
        Component->bApplyImportedNormals = Params->GetBoolField(TEXT("apply_imported_normals"));
    }
    if (Params->HasField(TEXT("smooth_normals_by_position")))
    {
        Component->bSmoothNormalsByPosition = Params->GetBoolField(TEXT("smooth_normals_by_position"));
    }
    if (Params->HasField(TEXT("normal_smooth_quantization")))
    {
        Component->NormalSmoothQuantization = FMath::Clamp(static_cast<float>(Params->GetNumberField(TEXT("normal_smooth_quantization"))), 1000.0f, 100000.0f);
    }
    if (Params->HasField(TEXT("cast_shadows")))
    {
        Component->bCastShadows = Params->GetBoolField(TEXT("cast_shadows"));
    }
    if (Params->HasField(TEXT("force_default_material")))
    {
        Component->bForceDefaultMaterial = Params->GetBoolField(TEXT("force_default_material"));
        if (Component->bForceDefaultMaterial)
        {
            Component->FallbackMaterial = nullptr;
            Component->MaterialOverrides.Reset();
        }
    }
    if (Params->HasField(TEXT("invert_shading_normals")))
    {
        Component->bInvertShadingNormals = Params->GetBoolField(TEXT("invert_shading_normals"));
    }
    if (Params->HasField(TEXT("use_binary_mesh_cache")))
    {
        Component->bUseBinaryMeshCache = Params->GetBoolField(TEXT("use_binary_mesh_cache"));
    }
    if (Params->HasField(TEXT("enable_preview_animation")))
    {
        Component->bEnablePreviewAnimation = Params->GetBoolField(TEXT("enable_preview_animation"));
    }
    if (Params->HasField(TEXT("preview_animation_frame_rate")))
    {
        Component->PreviewAnimationFrameRate = FMath::Clamp(static_cast<float>(Params->GetNumberField(TEXT("preview_animation_frame_rate"))), 1.0f, 120.0f);
    }
    if (Params->HasField(TEXT("preview_animation_smoothness")))
    {
        Component->PreviewAnimationSmoothness = FMath::Clamp(static_cast<float>(Params->GetNumberField(TEXT("preview_animation_smoothness"))), 0.0f, 1.0f);
    }

    const TArray<TSharedPtr<FJsonValue>>* InputValues = nullptr;
    if (Params->TryGetArrayField(TEXT("inputs"), InputValues) && InputValues)
    {
        Component->Inputs.Reset();
        for (const TSharedPtr<FJsonValue>& Value : *InputValues)
        {
            const TSharedPtr<FJsonObject> InputObject = Value.IsValid() ? Value->AsObject() : nullptr;
            if (!InputObject.IsValid())
            {
                continue;
            }
            FBlenderGNInput Input;
            ApplyOpenClawGNInputValue(Input, InputObject);
            Component->Inputs.Add(Input);
        }
    }

    Component->RefreshNow();

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetStringField(TEXT("status"), Component->LastStatus);
    ResultObj->SetBoolField(TEXT("is_running"), Component->bIsRunning);
    ResultObj->SetNumberField(TEXT("input_count"), Component->Inputs.Num());
    ResultObj->SetStringField(TEXT("object_name"), Component->ObjectName);
    ResultObj->SetStringField(TEXT("modifier_name"), Component->ModifierName);
    ResultObj->SetStringField(TEXT("blend_file"), Component->BlendFile.FilePath);
    ResultObj->SetNumberField(TEXT("refresh_mode"), static_cast<uint8>(Component->RefreshMode));
    ResultObj->SetBoolField(TEXT("apply_imported_normals"), Component->bApplyImportedNormals);
    ResultObj->SetBoolField(TEXT("smooth_normals_by_position"), Component->bSmoothNormalsByPosition);
    ResultObj->SetBoolField(TEXT("cast_shadows"), Component->bCastShadows);
    ResultObj->SetBoolField(TEXT("force_default_material"), Component->bForceDefaultMaterial);
    ResultObj->SetBoolField(TEXT("invert_shading_normals"), Component->bInvertShadingNormals);
    ResultObj->SetBoolField(TEXT("enable_preview_animation"), Component->bEnablePreviewAnimation);
    ResultObj->SetNumberField(TEXT("preview_animation_frame_rate"), Component->PreviewAnimationFrameRate);
    ResultObj->SetNumberField(TEXT("hex_subdivisions"), Component->GetIntInput(TEXT("Hex Subdivisions"), 2));
    ResultObj->SetNumberField(TEXT("preview_islands"), Component->LastPreviewIslandCount);
    ResultObj->SetStringField(TEXT("preview_animation_status"), Component->LastPreviewAnimationStatus);
    TArray<TSharedPtr<FJsonValue>> InputArray;
    for (const FBlenderGNInput& Input : Component->Inputs)
    {
        TSharedPtr<FJsonObject> InputObj = MakeShared<FJsonObject>();
        InputObj->SetStringField(TEXT("display_name"), Input.DisplayName);
        InputObj->SetStringField(TEXT("socket_name"), Input.SocketName);
        InputObj->SetNumberField(TEXT("type"), static_cast<int32>(Input.Type));
        InputArray.Add(MakeShared<FJsonValueObject>(InputObj));
    }
    ResultObj->SetArrayField(TEXT("inputs"), InputArray);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleOpenClawGNGetStatus(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) && !Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    AActor* Actor = FindActorByExactName(ActorName);
    if (!Actor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UBlenderGeometryNodesComponent* Component = Actor->FindComponentByClass<UBlenderGeometryNodesComponent>();
    if (!Component)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor has no UBlenderGeometryNodesComponent: %s"), *ActorName));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetStringField(TEXT("status"), Component->LastStatus);
    ResultObj->SetBoolField(TEXT("is_running"), Component->bIsRunning);
    ResultObj->SetBoolField(TEXT("has_pending_refresh"), Component->bHasPendingRefresh);
    ResultObj->SetNumberField(TEXT("last_eval_seconds"), Component->LastEvaluationSeconds);
    ResultObj->SetNumberField(TEXT("vertices"), Component->LastVertexCount);
    ResultObj->SetNumberField(TEXT("triangles"), Component->LastTriangleCount);
    ResultObj->SetNumberField(TEXT("input_count"), Component->Inputs.Num());
    ResultObj->SetStringField(TEXT("object_name"), Component->ObjectName);
    ResultObj->SetStringField(TEXT("modifier_name"), Component->ModifierName);
    ResultObj->SetStringField(TEXT("blend_file"), Component->BlendFile.FilePath);
    ResultObj->SetNumberField(TEXT("refresh_mode"), static_cast<uint8>(Component->RefreshMode));
    ResultObj->SetNumberField(TEXT("fixed_refresh_interval"), Component->FixedRefreshInterval);
    ResultObj->SetBoolField(TEXT("apply_imported_normals"), Component->bApplyImportedNormals);
    ResultObj->SetBoolField(TEXT("smooth_normals_by_position"), Component->bSmoothNormalsByPosition);
    ResultObj->SetBoolField(TEXT("mesh_has_imported_normals"), Component->GetLastMeshData().bImportedNormals);
    ResultObj->SetBoolField(TEXT("cast_shadows"), Component->bCastShadows);
    ResultObj->SetBoolField(TEXT("force_default_material"), Component->bForceDefaultMaterial);
    ResultObj->SetBoolField(TEXT("invert_shading_normals"), Component->bInvertShadingNormals);
    ResultObj->SetBoolField(TEXT("enable_preview_animation"), Component->bEnablePreviewAnimation);
    ResultObj->SetNumberField(TEXT("preview_animation_frame_rate"), Component->PreviewAnimationFrameRate);
    ResultObj->SetNumberField(TEXT("hex_subdivisions"), Component->GetIntInput(TEXT("Hex Subdivisions"), 2));
    ResultObj->SetNumberField(TEXT("preview_islands"), Component->LastPreviewIslandCount);
    ResultObj->SetStringField(TEXT("preview_animation_status"), Component->LastPreviewAnimationStatus);
    TArray<TSharedPtr<FJsonValue>> StatusInputArray;
    for (const FBlenderGNInput& Input : Component->Inputs)
    {
        TSharedPtr<FJsonObject> InputObj = MakeShared<FJsonObject>();
        InputObj->SetStringField(TEXT("display_name"), Input.DisplayName);
        InputObj->SetStringField(TEXT("socket_name"), Input.SocketName);
        InputObj->SetNumberField(TEXT("type"), static_cast<int32>(Input.Type));
        StatusInputArray.Add(MakeShared<FJsonValueObject>(InputObj));
    }
    ResultObj->SetArrayField(TEXT("inputs"), StatusInputArray);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleOpenClawGNDumpMeshDebug(const TSharedPtr<FJsonObject>& Params)
{
    FString ActorName;
    if (!Params->TryGetStringField(TEXT("actor_name"), ActorName) && !Params->TryGetStringField(TEXT("name"), ActorName))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'actor_name' parameter"));
    }

    AActor* Actor = FindActorByExactName(ActorName);
    if (!Actor)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor not found: %s"), *ActorName));
    }

    UBlenderGeometryNodesComponent* Component = Actor->FindComponentByClass<UBlenderGeometryNodesComponent>();
    if (!Component)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Actor has no UBlenderGeometryNodesComponent: %s"), *ActorName));
    }

    const FBlenderGNMeshData& MeshData = Component->GetLastMeshData();
    const TArray<FProcMeshTangent>& Tangents = Component->GetLastTangents();
    FString OutputPath;
    Params->TryGetStringField(TEXT("output_path"), OutputPath);
    const bool bWriteFullDump = !OutputPath.IsEmpty();
    const bool bIncludeArrays = bWriteFullDump || (Params->HasField(TEXT("include_arrays")) && Params->GetBoolField(TEXT("include_arrays")));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("actor"), ActorName);
    ResultObj->SetStringField(TEXT("object_name"), Component->ObjectName);
    ResultObj->SetStringField(TEXT("modifier_name"), Component->ModifierName);
    ResultObj->SetStringField(TEXT("blend_file"), Component->BlendFile.FilePath);
    ResultObj->SetStringField(TEXT("status"), Component->LastStatus);
    ResultObj->SetBoolField(TEXT("apply_imported_normals"), Component->bApplyImportedNormals);
    ResultObj->SetBoolField(TEXT("smooth_normals_by_position"), Component->bSmoothNormalsByPosition);
    ResultObj->SetBoolField(TEXT("cast_shadows"), Component->bCastShadows);
    ResultObj->SetBoolField(TEXT("force_default_material"), Component->bForceDefaultMaterial);
    ResultObj->SetBoolField(TEXT("invert_shading_normals"), Component->bInvertShadingNormals);
    ResultObj->SetObjectField(TEXT("mesh_stats"), BuildOpenClawGNMeshStats(MeshData));
    ResultObj->SetNumberField(TEXT("tangents"), Tangents.Num());

    TArray<TSharedPtr<FJsonValue>> SectionArray;
    SectionArray.Reserve(MeshData.Sections.Num());
    for (int32 SectionIndex = 0; SectionIndex < MeshData.Sections.Num(); ++SectionIndex)
    {
        TSharedPtr<FJsonObject> SectionObj = MakeShared<FJsonObject>();
        SectionObj->SetNumberField(TEXT("section_index"), SectionIndex);
        SectionObj->SetNumberField(TEXT("triangles"), MeshData.Sections[SectionIndex].Triangles.Num() / 3);
        if (MeshData.Materials.IsValidIndex(SectionIndex))
        {
            SectionObj->SetStringField(TEXT("material_name"), MeshData.Materials[SectionIndex].Name);
        }
        if (bIncludeArrays)
        {
            SectionObj->SetArrayField(TEXT("triangle_indices"), MakeIntArray(MeshData.Sections[SectionIndex].Triangles));
        }
        SectionArray.Add(MakeShared<FJsonValueObject>(SectionObj));
    }
    ResultObj->SetArrayField(TEXT("sections"), SectionArray);

    if (bIncludeArrays)
    {
        ResultObj->SetArrayField(TEXT("vertices"), MakeFlatVectorArray(MeshData.Vertices));
        ResultObj->SetArrayField(TEXT("normals"), MakeFlatVectorArray(MeshData.Normals));
        ResultObj->SetArrayField(TEXT("uvs"), MakeFlatVector2DArray(MeshData.UVs));
        ResultObj->SetArrayField(TEXT("triangles"), MakeIntArray(MeshData.Triangles));
        ResultObj->SetArrayField(TEXT("material_indices"), MakeIntArray(MeshData.MaterialIndices));
        ResultObj->SetArrayField(TEXT("tangent_x_and_flip_y"), MakeTangentArray(Tangents));
    }

    if (bWriteFullDump)
    {
        FString OutputJson;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputJson);
        FJsonSerializer::Serialize(ResultObj.ToSharedRef(), Writer);
        if (!FFileHelper::SaveStringToFile(OutputJson, *OutputPath))
        {
            return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to write GN mesh debug dump: %s"), *OutputPath));
        }
        ResultObj->SetStringField(TEXT("output_path"), FPaths::ConvertRelativePathToFull(OutputPath));
    }

    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleSaveAll(const TSharedPtr<FJsonObject>& Params)
{
    bool bNeededSaving = false;
    const bool bSuccess = FEditorFileUtils::SaveDirtyPackages(false, true, true, false, false, false, &bNeededSaving);
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bSuccess);
    ResultObj->SetBoolField(TEXT("needed_saving"), bNeededSaving);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleExecutePythonFile(const TSharedPtr<FJsonObject>& Params)
{
    FString FilePath;
    if (!Params->TryGetStringField(TEXT("path"), FilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    FilePath = FPaths::ConvertRelativePathToFull(FilePath);
    if (!FPaths::FileExists(FilePath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Python file not found: %s"), *FilePath));
    }

    IPythonScriptPlugin* PythonPlugin = IPythonScriptPlugin::Get();
    if (!PythonPlugin || !PythonPlugin->IsPythonAvailable())
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("PythonScriptPlugin is not available"));
    }

    const bool bSuccess = PythonPlugin->ExecPythonCommand(*FilePath);
    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), bSuccess);
    ResultObj->SetStringField(TEXT("path"), FilePath);
    return ResultObj;
}

TSharedPtr<FJsonObject> FUnrealMCPEditorCommands::HandleOpenLevel(const TSharedPtr<FJsonObject>& Params)
{
    FString LevelPath;
    if (!Params->TryGetStringField(TEXT("path"), LevelPath))
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(TEXT("Missing 'path' parameter"));
    }

    if (!FPackageName::DoesPackageExist(LevelPath))
    {
        const FString ContentRelative = FString::Printf(TEXT("/Game/%s"), *LevelPath);
        if (FPackageName::DoesPackageExist(ContentRelative))
        {
            LevelPath = ContentRelative;
        }
    }

    bool bSuccess = FEditorFileUtils::LoadMap(LevelPath, false, true);
    if (!bSuccess)
    {
        return FUnrealMCPCommonUtils::CreateErrorResponse(FString::Printf(TEXT("Failed to open level: %s"), *LevelPath));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShareable(new FJsonObject);
    ResultObj->SetStringField(TEXT("status"), TEXT("success"));
    TSharedPtr<FJsonObject> ResultData = MakeShareable(new FJsonObject);
    ResultData->SetStringField(TEXT("level"), LevelPath);
    ResultObj->SetObjectField(TEXT("result"), ResultData);
    return ResultObj;
}
