#include "Component.h"
#include "Entity.h"
#include "GenericEntity.h"
#include "ComponentManager.h"

using namespace Pakal;

void Component::request_dispose()
{
	if (m_pendingDispose)
	{
		return;
	}

	auto* genericParent = dynamic_cast<GenericEntity*>(m_parent);
	if (!genericParent)
	{
		return;
	}

	auto* componentManager = genericParent->get_component_manager();
	if (!componentManager)
	{
		return;
	}

	componentManager->request_dispose(this);
}