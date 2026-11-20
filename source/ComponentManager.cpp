#include "ComponentManager.h"

#include "ComponentFactory.h"
#include "GenericEntity.h"
#include "IComponentProvider.h"
#include "LogMgr.h"

using namespace Pakal;


void ComponentManager::register_provider(IComponentProvider& provider )
{
	std::vector<IComponentFactory*> factories;
	provider.register_component_factories(factories);

	for (auto & factory : factories)
	{
		register_factory(factory);
	}
}

void ComponentManager::register_factory(IComponentFactory* factory, bool replacePreviousFactory)
{
	ASSERT(factory);

	std::string componentTypename = factory->get_typename();

	auto previous = m_factories.find( componentTypename );
	if(previous != m_factories.end())
	{
		if (replacePreviousFactory == false)
		{
			LOG_ERROR("[ComponentManager] Factory Already registered");
			ASSERT(false);
		}
		// the one replaced is the manager's too (it leaked)
		if (previous->second != factory)
		{
			delete previous->second;
		}
	}

	m_factories[ componentTypename ] = factory;
	LOG_DEBUG("[ComponentManager] registered factory for: '%s' component type.", componentTypename.c_str() );
}

AsyncComponent* ComponentManager::create_component(const std::string& typeName )
{
	const auto &it = m_factories.find(typeName);

	if(it == m_factories.end())
	{
		LOG_WARNING("[ComponentManager] error: '%s' unknown component type.", typeName.c_str() );
		return nullptr;
	}	

	AsyncComponent * newComponent = it->second->create(); //call the factory create component
	return newComponent;
}

void ComponentManager::request_dispose(AsyncComponent* component)
{
	if (!component || component->m_pendingDispose)
	{
		return;
	}

	component->m_pendingDispose = true;
	m_pendingDispose.insert(component);
}

size_t ComponentManager::process_pending_disposals()
{
	if (m_pendingDispose.empty())
	{
		return 0;
	}

	std::vector<AsyncComponent*> pending(m_pendingDispose.begin(), m_pendingDispose.end());
	m_pendingDispose.clear();

	size_t disposedCount = 0;
	for (auto* component : pending)
	{
		if (!component)
		{
			continue;
		}

		component->prepare_dispose();

		auto* genericParent = dynamic_cast<GenericEntity*>(component->get_parent_entity());
		if (genericParent)
		{
			if (genericParent->destroy_component(component))
			{
				disposedCount++;
			}
			continue;
		}

		delete component;
		disposedCount++;
	}

	return disposedCount;
}

void ComponentManager::terminate()
{
	process_pending_disposals();

	// The factories go when the engine ends its run, as the entities' do (EntityManager::terminate): a game module's
	// factories are made by its own code, and the module is unloaded right after the run, before the engine is deleted
	// (deleting them in ~ComponentManager then ran code no longer loaded: the desktop player crashed on exit with a
	// module that registers components). Like those, they are registered for one run.
	release_factories();
}

void ComponentManager::release_factories()
{
	for (auto& pair : m_factories)
	{
		delete pair.second;
	}
	m_factories.clear();
}

ComponentManager::~ComponentManager()
{
	release_factories();
}
