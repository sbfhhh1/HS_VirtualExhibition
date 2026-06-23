#include "SubstanceConnectorComponentImpl.h"
#include "SubstanceConnectorComponent.h"
#include <substance/connector/framework/features/sendsbsar.h>
#include <substance/connector/framework/features/sendmesh.h>
#include <substance/connector/framework/system.h>
#include <substance/connector/framework/core.h>
#include <substance/connector/framework/details/callbacks.h>

#include <map>
#include <functional>
#include <string>
#include "Serialization/JsonSerializer.h"

#include "Async/Async.h"
#include "AssetToolsModule.h"
#include "SubstanceFactory.h"
#include "AssetImportTask.h"
#include "USDStageImportContext.h"
#include "USDStageImporter.h"
#include "USDStageImporterModule.h"
#include "USDStageImportOptions.h"
#include "ContentBrowserModule.h"
#include "IContentBrowserSingleton.h"
#include "Factories/FbxFactory.h"
#include "Engine/RendererSettings.h"
#include "Misc/MessageDialog.h"
#include "UnrealEd.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/Texture.h"
#include "UnrealUSDWrapper.h"

namespace Substance
{
namespace Unreal
{
namespace Connector
{
//! @brief The application name of this program
static const char* const sConnectorApplicationName = "Unreal Engine";

//! @brief Application instance for handling importing sbsar files
std::unique_ptr<Substance::Connector::Framework::SbsarApplication> mConnectorSbsar (new Substance::Connector::Framework::SbsarApplication);
std::unique_ptr<Substance::Connector::Framework::MeshApplication> mConnectorMesh (new Substance::Connector::Framework::MeshApplication);

//! @brief Map of the application name to its context
static std::map<std::string, std::pair<Substance::Connector::Framework::Schemas::connection_schema, int>> sApplicationList;
static std::map<std::string, std::pair<Substance::Connector::Framework::Schemas::mesh_export_schema, int>> sMeshSchemaList;

static std::map<std::string, int> sAssetList;

static SubstanceConnectorContext* sContextHandle = nullptr;

static std::vector<substance_connector_uuid_t> feature_ids;

Substance::Connector::Framework::Schemas::mesh_export_schema unrealMeshSchema;

DEFINE_LOG_CATEGORY_STATIC(LogSubstanceConnectorModule, Log, All);

unsigned int getApplicationContext(const std::string& application)
{
    unsigned int context = 0u;

    if (sApplicationList.find(application) != sApplicationList.end())
    {
        context = sApplicationList[application].second;
    }

    return context;
}

void onRecvLoadSbsar(unsigned int context, const substance_connector_uuid_t* uuid, const char* message)
{
	if (sContextHandle == nullptr)
		return;

	FString messageString = message;

	TSharedPtr<FJsonObject> JsonParsed;
	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(messageString);
	if (FJsonSerializer::Deserialize(JsonReader, JsonParsed))
	{
        if(JsonParsed->HasField(FString("type"))) {
            if (JsonParsed->GetStringField(FString("type")) == "material") {

                FString path = JsonParsed->GetStringField(FString("path"));

                AsyncTask(ENamedThreads::GameThread, [=]() {
                    FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
                    FString mContentBrowserPath = FString("/Content");
                    ContentBrowserModule.Get().FocusPrimaryContentBrowser(false);
                    if(ContentBrowserModule.Get().GetCurrentPath().HasInternalPath()) {
                        mContentBrowserPath = ContentBrowserModule.Get().GetCurrentPath().GetInternalPathString();
                    } else {
                        mContentBrowserPath = ContentBrowserModule.Get().GetCurrentPath().GetVirtualPathString();
                    }

                    FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

                    USubstanceFactory* importFactory = NewObject<USubstanceFactory>();
                    importFactory->SuppressImportOverwriteDialog();
                    TArray<FString> SourcePaths;
                    SourcePaths.Add(path);

                    UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
                    ImportData->bReplaceExisting = true;
                    ImportData->Filenames = SourcePaths;
                    
                    ImportData->DestinationPath = mContentBrowserPath;
                    ImportData->bSkipReadOnly = false;
                    ImportData->Factory = importFactory;

                    AssetToolsModule.Get().ImportAssetsAutomated(ImportData);

                    importFactory->ConditionalBeginDestroy();
                    importFactory = nullptr;
                    UE_LOG(LogSubstanceConnectorModule, Warning, TEXT("Import Sbsar: %s"), *path);
                });
		    }
        }
	}
}

void onRecvConnectionEstablished(unsigned int context, const substance_connector_uuid_t* uuid, const char* message)
{
	sApplicationList[message].second = context;

    Substance::Connector::Framework::System::connectionEstablished(context, &Substance::Connector::Framework::System::sConnectionUpdateContextId, Substance::Connector::Framework::getConnectionContext().c_str());
}

void onRecvConnectionClosed(unsigned int context, const substance_connector_uuid_t* uuid, const char* message)
{
	const auto& it = sApplicationList.find(message);
	if (it != sApplicationList.end())
	{
		sApplicationList.erase(it);
	}
}

void onRecvConnectionUpdateContext(unsigned int context, const substance_connector_uuid_t* uuid, const char* message) {
    Substance::Connector::Framework::Schemas::connection_schema schema;
    schema.Deserialize(std::string(message));
    const auto& it = sApplicationList.find(schema.id_name);
    if (it != sApplicationList.end())
	{
		sApplicationList[schema.id_name].first = schema;
	}
}

void onRecvLoadMesh(unsigned int context, const substance_connector_uuid_t* uuid, const char* message) {

    if (sContextHandle == nullptr)
		return;
    
    if(!UTexture::IsVirtualTexturingEnabled()) {
        FText DialogText = FText::FromString("Substance Connector requires virtual textures to be enabled to receive mesh files. Would you like to enable this? The editor will require a restart.");
        FText MessageTitle = FText::FromString("Enable Virtual Textures");
        EAppReturnType::Type ReturnType = FMessageDialog::Open(EAppMsgType::YesNo, DialogText, MessageTitle);
        if (ReturnType == EAppReturnType::Yes) {
            URendererSettings* Settings = GetMutableDefault<URendererSettings>();
            Settings->bVirtualTextures = true;
            Settings->Modify();
            Settings->TryUpdateDefaultConfigFile();
            AsyncTask(ENamedThreads::GameThread, [=]() {
                FUnrealEdMisc::Get().RestartEditor(true);
            });
        }
        else {
            UE_LOG(LogSubstanceConnectorModule, Warning, TEXT("Importing mesh files through Substance Connector requires virtual textures to be enabled."));
        }
        return;
    }

    FString messageString = message;
    TSharedPtr<FJsonObject> JsonParsed;
    TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(messageString);
    if (FJsonSerializer::Deserialize(JsonReader, JsonParsed)) {
        if(JsonParsed->HasField(FString("type"))) {
            if (JsonParsed->GetStringField(FString("type")) == "mesh") {
                FString path = JsonParsed->GetStringField(FString("path"));
                FString extensionString = FPaths::GetExtension(path, false);

                AsyncTask(ENamedThreads::GameThread, [=]() {
                    FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
                    FString mContentBrowserPath = FString("/Content");
                    ContentBrowserModule.Get().FocusPrimaryContentBrowser(false);
                    if(ContentBrowserModule.Get().GetCurrentPath().HasInternalPath()) {
                        mContentBrowserPath = ContentBrowserModule.Get().GetCurrentPath().GetInternalPathString();
                    } else {
                        mContentBrowserPath = ContentBrowserModule.Get().GetCurrentPath().GetVirtualPathString();
                    }

                    FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");

                    if(extensionString == "usd" || extensionString == "usdz") {
                        UUsdStageImporter* USDImporter = IUsdStageImporterModule::Get().GetImporter();
                        FUsdStageImportContext ImportContext;
                        
                        if (ImportContext.Init(FString("ConnectorImport"), path, mContentBrowserPath, EObjectFlags::RF_NoFlags, false)) {
                            if (context == getApplicationContext("Modeler Connector Client")) {
                                ImportContext.ImportOptions->StageOptions.MetersPerUnit = 1.0f;
                                ImportContext.ImportOptions->bOverrideStageOptions = true;
                            }
                            USDImporter->ImportFromFile(ImportContext);
                            UE_LOG(LogSubstanceConnectorModule, Warning, TEXT("Import USD: %s"), *path);
                        }
                    
                    } else if(extensionString == "fbx" || extensionString == "obj") {
                        FString FilePath = path;
                        FString Filename = FPaths::GetBaseFilename(FilePath);

                        UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
                        ImportData->bReplaceExisting = true;
                        ImportData->Filenames = {FilePath};
                        
                        ImportData->DestinationPath = mContentBrowserPath;
                        ImportData->bSkipReadOnly = false;

                        // Set factory (using FBX factory as it supports OBJ)
                        UFbxFactory* Factory = NewObject<UFbxFactory>();
                        ImportData->Factory = Factory;

                        AssetToolsModule.Get().ImportAssetsAutomated(ImportData);
                    }
                });
		    }
        }
	}
}

void onRecvUpdateMesh(unsigned int context, const substance_connector_uuid_t* uuid, const char* message) {
    
}

void onRecvMeshConfig(unsigned int context, const substance_connector_uuid_t* uuid, const char* message) {
    Substance::Connector::Framework::Schemas::mesh_export_schema inputSchema;
    inputSchema.Deserialize(std::string(message));
}

void onRecvMeshConfigRequest(unsigned int context, const substance_connector_uuid_t* uuid, const char* message) {
    
    Substance::Connector::Framework::Schemas::mesh_export_schema outputSchema;
    outputSchema.supportedFormats = {
                                    Substance::Connector::Framework::Schemas::mesh_export_schema::FileFormat::usd,
                                    Substance::Connector::Framework::Schemas::mesh_export_schema::FileFormat::usdz,
                                    Substance::Connector::Framework::Schemas::mesh_export_schema::FileFormat::obj,
                                    Substance::Connector::Framework::Schemas::mesh_export_schema::FileFormat::fbx
                                    };

    //outputSchema.colorFormat = Substance::Connector::Framework::Schemas::mesh_export_schema::ColorFormat::None;
    outputSchema.colorEncoding = Substance::Connector::Framework::Schemas::mesh_export_schema::ColorEncoding::SRGB;
    outputSchema.allowNegativeTransforms = false;
    outputSchema.allowInstances = false;
    outputSchema.flattenHierarchy = true;
    outputSchema.axisConvention = Substance::Connector::Framework::Schemas::mesh_export_schema::AxisConvention::ForceZUp_XRight_YForward;
    outputSchema.unit = Substance::Connector::Framework::Schemas::mesh_export_schema::Unit::Centimeters;
    outputSchema.topology = Substance::Connector::Framework::Schemas::mesh_export_schema::ExportTopology::Triangles;
    outputSchema.requestUv = false;
    outputSchema.enableUdims = true;
    mConnectorMesh->sendMeshExportConfig(context, outputSchema);
}

Component::Impl::Impl()
	: mConnectorSystem(new Substance::Connector::Framework::System)
	, mOpenContext(0u)
{
    feature_ids = mConnectorSystem->getFeatureIds();

	Substance::Connector::Framework::registerApplication(mConnectorSbsar.get());
	Substance::Connector::Framework::registerApplication(mConnectorMesh.get());
	Substance::Connector::Framework::registerApplication(mConnectorSystem.get());

	mConnectorSbsar->mRecvLoadSbsar = onRecvLoadSbsar;
	mConnectorSbsar->mRecvUpdateSbsar = onRecvLoadSbsar;
	mConnectorSbsar->preInit();

    mConnectorMesh->mRecvLoadMesh = onRecvLoadMesh;
    mConnectorMesh->mRecvUpdateMesh = onRecvUpdateMesh;
    mConnectorMesh->mRecvMeshConfig = onRecvMeshConfig;
    mConnectorMesh->mRecvMeshConfigRequest = onRecvMeshConfigRequest;
    mConnectorMesh->preInit();

	mConnectorSystem->mRecvConnectionEstablished = onRecvConnectionEstablished;
	mConnectorSystem->mRecvConnectionClosed = onRecvConnectionClosed;
    mConnectorSystem->mRecvConnectionContext = onRecvConnectionUpdateContext;
	mConnectorSystem->preInit();

	Substance::Connector::Framework::init(sConnectorApplicationName);
}

Component::Impl::~Impl()
{
}

bool Component::Impl::postInitialization()
{
	bool result = Substance::Connector::Framework::openDefaultTcp(&mOpenContext);

	if (result)
	{
		result = Substance::Connector::Framework::broadcastTcp();
	}

	return result;
}

bool Component::Impl::shutdown()
{
	bool result = false;

	result = Substance::Connector::Framework::shutdown();

	return result;
}

void Component::Impl::sendLoadSbsar(unsigned int context, const std::string& message)
{
	mConnectorSbsar->sendLoadSbsar(context, message.c_str());
}

void Component::Impl::sendLoadMesh(unsigned int context, Substance::Connector::Framework::Schemas::send_to_schema &schema)
{
    mConnectorMesh->sendLoadMesh(context, schema);
}

void Component::Impl::sendMeshExportConfig(unsigned int context, Substance::Connector::Framework::Schemas::mesh_export_schema &schema)
{
    mConnectorMesh->sendMeshExportConfig(context, schema);
}

void Component::Impl::sendRequestMeshConfig(unsigned int context, Substance::Connector::Framework::Schemas::send_mesh_config_request_schema &config_schema)
{
    mConnectorMesh->sendRequestMeshConfig(context, config_schema);
}

void Component::Impl::onApplicationQuit()
{
	sContextHandle = nullptr;
	shutdown();
}

void Component::Impl::setContextHandle(SubstanceConnectorContext* context)
{
	sContextHandle = context;
}

} // namespace Connector
} // namespace Photoshop
} // namespace Alg
