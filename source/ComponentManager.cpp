#include "ComponentManager.h"

#include "IComponentFactory.h"
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

void ComponentManager::register_factory( IComponentFactory* factory, const std::string& name)
{
	ASSERT(factory);

	std::string factoryName = name.empty() ? factory->get_component_type().getName() : name;

	if(m_factories.find( factoryName ) != m_factories.end())
	{
		LOG_ERROR("[ComponentManager] Factory Already registered");
		ASSERT(false);
	}

	m_factories[ factoryName ] = factory;
	LOG_DEBUG("[ComponentManager] registered factory for: '%s' component type.", factoryName.c_str() );
}

IComponent* ComponentManager::create_component(const char* typeName )
{
	const auto &it = m_factories.find(typeName);

	if(it == m_factories.end())
	{
		LOG_WARNING("[ComponentManager] error: '%s' unknown component type.", typeName );
		return nullptr;
	}	

	IComponent * newComponent = it->second->create(); //call the factory create component
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
