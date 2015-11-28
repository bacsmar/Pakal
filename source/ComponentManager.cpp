#include "ComponentManager.h"

#include "ComponentFactory.h"
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

	if(m_factories.find( componentTypename ) != m_factories.end() && replacePreviousFactory == false)
	{
		LOG_ERROR("[ComponentManager] Factory Already registered");
		ASSERT(false);
	}

	m_factories[ componentTypename ] = factory;
	LOG_DEBUG("[ComponentManager] registered factory for: '%s' component type.", componentTypename.c_str() );
}

Component* ComponentManager::create_component(const std::string& typeName )
{
	const auto &it = m_factories.find(typeName);

	if(it == m_factories.end())
	{
		LOG_WARNING("[ComponentManager] error: '%s' unknown component type.", typeName );
		return nullptr;
	}	

	Component * newComponent = it->second->create(); //call the factory create component
	return newComponent;
}

ComponentManager::~ComponentManager()
{
	for( auto it = m_factories.begin() ; it != m_factories.end() ; )
	{
		delete it->second;
		m_factories.erase(it++);	
	}
}
