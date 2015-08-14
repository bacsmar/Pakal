#include "ComponentManager.h"

#include "IComponentFactory.h"
#include "IComponentProvider.h"
#include "LogMgr.h"

using namespace Pakal;


void ComponentManager::registerProvider(IComponentProvider& provider )
{
	std::vector<IComponentFactory*> factories;
	provider.register_component_factories(factories);

	for (auto & factory : factories)
	{
		registerFactory(factory);
	}
}

void ComponentManager::registerFactory( IComponentFactory* factory, const std::string& name)
{
	ASSERT(factory);

	std::string factoryName = name.empty() ? factory->getComponentType().getName() : name;

	if(m_ComponentFactories.find( factoryName ) != m_ComponentFactories.end())
	{
		LOG_ERROR("[ComponentManager] Factory Already registered");
		ASSERT(false);
	}

	m_ComponentFactories[ factoryName ] = factory;
	LOG_DEBUG("[ComponentManager] registered factory for: '%s' component type.", factoryName.c_str() );
}

IComponent* ComponentManager::createComponent(const char* typeName )
{
	const auto &it = m_ComponentFactories.find(typeName);

	if(it == m_ComponentFactories.end())
	{
		LOG_WARNING("[ComponentManager] error: '%s' unknown component type.", typeName );
		return nullptr;
	}	

	IComponent * newComponent = it->second->create(); //call the factory create component
	return newComponent;		
}

void ComponentManager::dropComponent(IComponent* component)
{
	delete component;
}

ComponentManager::~ComponentManager()
{
	for( auto it = m_ComponentFactories.begin() ; it != m_ComponentFactories.end() ; )
	{
		delete it->second;
		m_ComponentFactories.erase(it++);	
	}
}
