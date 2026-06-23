#include "OpenClawBlenderGeometryNodesEditorModule.h"

#include "BlenderGeometryNodesActor.h"
#include "BlenderGeometryNodesComponent.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Framework/Notifications/NotificationManager.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "OpenClawBlenderGeometryNodesComponentDetails.h"
#include "PropertyEditorModule.h"
#include "ToolMenus.h"
#include "Widgets/Notifications/SNotificationList.h"

#define LOCTEXT_NAMESPACE "OpenClawBlenderGeometryNodesEditor"

namespace
{
void NotifyOpenClaw(const FText& Text, SNotificationItem::ECompletionState State = SNotificationItem::CS_None)
{
	FNotificationInfo Info(Text);
	Info.ExpireDuration = 4.0f;
	TSharedPtr<SNotificationItem> Item = FSlateNotificationManager::Get().AddNotification(Info);
	if (Item.IsValid())
	{
		Item->SetCompletionState(State);
	}
}

UWorld* GetEditorWorld()
{
	return GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
}

ABlenderGeometryNodesActor* SpawnGeometryNodesActor(const FString& Label, bool bRuntime)
{
	UWorld* World = GetEditorWorld();
	if (!World)
	{
		NotifyOpenClaw(LOCTEXT("NoEditorWorld", "No editor world is available."), SNotificationItem::CS_Fail);
		return nullptr;
	}

	const FScopedTransaction Transaction(LOCTEXT("CreateGNActorTransaction", "Create Blender Geometry Nodes Actor"));
	FActorSpawnParameters Params;
	Params.Name = MakeUniqueObjectName(World->GetCurrentLevel(), ABlenderGeometryNodesActor::StaticClass(), *Label);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ABlenderGeometryNodesActor* Actor = World->SpawnActor<ABlenderGeometryNodesActor>(ABlenderGeometryNodesActor::StaticClass(), FTransform::Identity, Params);
	if (!Actor)
	{
		NotifyOpenClaw(LOCTEXT("SpawnFailed", "Failed to create Blender Geometry Nodes actor."), SNotificationItem::CS_Fail);
		return nullptr;
	}

	Actor->Modify();
	Actor->SetActorLabel(Label);
	if (Actor->GeometryNodes)
	{
		Actor->GeometryNodes->Modify();
		Actor->GeometryNodes->RefreshMode = EBlenderGNRefreshMode::Manual;
		Actor->GeometryNodes->bRefreshOnBeginPlay = bRuntime;
	}

	GEditor->SelectNone(false, true);
	GEditor->SelectActor(Actor, true, true);
	NotifyOpenClaw(FText::Format(LOCTEXT("ActorCreated", "Created {0}."), FText::FromString(Label)), SNotificationItem::CS_Success);
	return Actor;
}

void CreateRuntimeActor()
{
	SpawnGeometryNodesActor(TEXT("Blender GN Runtime Actor"), true);
}

void CreateBakerActor()
{
	SpawnGeometryNodesActor(TEXT("Blender GN Baker Actor"), false);
}

void InstallSidecar()
{
	const FString SourcePath = UBlenderGeometryNodesComponent::GetDefaultSidecarPath();
	if (SourcePath.IsEmpty() || !FPaths::FileExists(SourcePath))
	{
		NotifyOpenClaw(LOCTEXT("SidecarMissing", "Plugin sidecar script was not found."), SNotificationItem::CS_Fail);
		return;
	}

	const FString TargetDir = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("OpenClaw/BlenderGeometryNodes"));
	IFileManager::Get().MakeDirectory(*TargetDir, true);
	const FString TargetPath = FPaths::Combine(TargetDir, TEXT("unreal_gn_eval.py"));
	if (!IFileManager::Get().Copy(*TargetPath, *SourcePath, true, true))
	{
		NotifyOpenClaw(FText::Format(LOCTEXT("SidecarInstalled", "Installed sidecar to {0}."), FText::FromString(TargetPath)), SNotificationItem::CS_Success);
	}
	else
	{
		NotifyOpenClaw(LOCTEXT("SidecarInstallFailed", "Failed to install sidecar script."), SNotificationItem::CS_Fail);
	}
}
}

void FOpenClawBlenderGeometryNodesEditorModule::StartupModule()
{
	FPropertyEditorModule& PropertyEditor = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	PropertyEditor.RegisterCustomClassLayout(
		UBlenderGeometryNodesComponent::StaticClass()->GetFName(),
		FOnGetDetailCustomizationInstance::CreateStatic(&FOpenClawBlenderGeometryNodesComponentDetails::MakeInstance));
	PropertyEditor.NotifyCustomizationModuleChanged();

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FOpenClawBlenderGeometryNodesEditorModule::RegisterMenus));
}

void FOpenClawBlenderGeometryNodesEditorModule::ShutdownModule()
{
	if (UObjectInitialized())
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

	if (FModuleManager::Get().IsModuleLoaded("PropertyEditor"))
	{
		FPropertyEditorModule& PropertyEditor = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyEditor.UnregisterCustomClassLayout(UBlenderGeometryNodesComponent::StaticClass()->GetFName());
		PropertyEditor.NotifyCustomizationModuleChanged();
	}
}

void FOpenClawBlenderGeometryNodesEditorModule::RegisterMenus()
{
	FToolMenuOwnerScoped OwnerScoped(this);
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
	FToolMenuSection& Section = Menu->FindOrAddSection("OpenClaw");
	Section.Label = LOCTEXT("OpenClawSection", "OpenClaw");

	Section.AddSubMenu(
		"OpenClawBlenderGeometryNodes",
		LOCTEXT("BlenderGNMenu", "Blender Geometry Nodes"),
		LOCTEXT("BlenderGNMenuTooltip", "OpenClaw Blender Geometry Nodes editor utilities."),
		FNewToolMenuDelegate::CreateLambda([](UToolMenu* SubMenu)
		{
			FToolMenuSection& Actions = SubMenu->FindOrAddSection("Actions");
			Actions.AddMenuEntry(
				"CreateRuntimeActor",
				LOCTEXT("CreateRuntimeActor", "Create Runtime Actor"),
				LOCTEXT("CreateRuntimeActorTooltip", "Create a Blender Geometry Nodes actor configured for runtime refresh."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&CreateRuntimeActor)));
			Actions.AddMenuEntry(
				"CreateBakerActor",
				LOCTEXT("CreateBakerActor", "Create Baker Actor"),
				LOCTEXT("CreateBakerActorTooltip", "Create a Blender Geometry Nodes actor configured for editor baking."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&CreateBakerActor)));
			Actions.AddMenuEntry(
				"InstallSidecar",
				LOCTEXT("InstallSidecar", "Install Sidecar"),
				LOCTEXT("InstallSidecarTooltip", "Copy the bundled Blender Python sidecar into project Content/OpenClaw/BlenderGeometryNodes."),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateStatic(&InstallSidecar)));
		}));
}

IMPLEMENT_MODULE(FOpenClawBlenderGeometryNodesEditorModule, OpenClawBlenderGeometryNodesEditor)

#undef LOCTEXT_NAMESPACE
