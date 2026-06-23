#pragma once

#include "Modules/ModuleManager.h"

class FOpenClawBlenderGeometryNodesModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
