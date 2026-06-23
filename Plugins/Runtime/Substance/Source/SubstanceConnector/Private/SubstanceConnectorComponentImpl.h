#pragma once

#include "SubstanceConnectorComponent.h"
#include <memory>
#include <string>
#include <substance/connector/types.h>
#include "Containers/UnrealString.h"
#include <substance/connector/framework/schemas/connectionschema.h>
#include <substance/connector/framework/schemas/sendmeshschema.h>
#include <substance/connector/framework/schemas/sendtoschema.h>

struct SubstanceConnectorContext;

namespace Substance
{
namespace Connector
{
namespace Framework
{
class SbsarApplication;
class MeshApplication;
class System;
} // namespace Connector
}
namespace Unreal
{
namespace Connector
{
//! @brief Implementation of the component class, containing the API instances
class Component::Impl
{
public:
	//! @brief Default constructor
	Impl();

	//! @brief Destructor
	~Impl();

	//! @brief Method to finalize Connector setup after object construction
	//! @return True on success, false on failure
	bool postInitialization();

	//! @brief Method to finalize Connector shutdown prior to unloading the module
	//! @return True on success, false on failure
	bool shutdown();

	//! @brief Send a load sbsar message to another application
	//! @param context The context associated with the connection
	//! @param message The message containing the path to the sbsar file
	void sendLoadSbsar(unsigned int context, const std::string& message);

    //! @brief Send an event to another connection to load a mesh
	//! @param context Integer representing the id of the connection to send to
	//! @param message schema containing all of the details of the asset to import
	void sendLoadMesh(unsigned int context, Substance::Connector::Framework::Schemas::send_to_schema& schema);

	//! @brief Send an event to another connection respond to a mesh export config request
	//! @param context Integer representing the id of the connection to send to
	//! @param schema Instance containing all of the config details for how an importing applications expects
	//! a mesh to be exported
	void sendMeshExportConfig(unsigned int context, Substance::Connector::Framework::Schemas::mesh_export_schema& schema);

	//! @brief Send a request the expected mesh export config from an importing application
	//! @param context Integer representing the id of the connection to send to this request to
	//! @param message schema instance containing initial context information of the asset that is to be
	//! exported
	void sendRequestMeshConfig(unsigned int context, Substance::Connector::Framework::Schemas::send_mesh_config_request_schema& config_schema);

	//! @brief Slot used to connect to application exit to handle cleanup
	void onApplicationQuit();

	void setContextHandle(SubstanceConnectorContext* context);

private:

	//! @brief Application instance for handling system events, such as on
	//!     establish and closing of a connection
	std::unique_ptr<Substance::Connector::Framework::System> mConnectorSystem;

	//! @brief The open context for this application, used for others to
	//!     connect to when they establish the connection
	unsigned int mOpenContext;
};
} // namespace Connector
} // namespace Unreal
} // namespace Substance
