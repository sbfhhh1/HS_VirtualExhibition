#include "SubstanceConnectorComponent.h"
#include "SubstanceConnectorComponentImpl.h"

namespace Substance
{
namespace Unreal
{
namespace Connector
{
Component::Component() : mImpl(new Component::Impl)
{
}

Component::~Component()
{
}

bool Component::postInitialization()
{
	return mImpl->postInitialization();
}

bool Component::shutdown()
{
	return mImpl->shutdown();
}

void Component::setContextHandle(SubstanceConnectorContext* context)
{
	mImpl->setContextHandle(context);
}

void Component::sendLoadSbsar(unsigned int context, const std::string& message)
{
	mImpl->sendLoadSbsar(context, message);
}
} // namespace Connector
} // namespace Unreal
} // namespace Substance