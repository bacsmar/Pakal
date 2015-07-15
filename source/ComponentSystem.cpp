#include "ComponentSystem.h"
#include "Poco/Foundation.h"

#include "IComponentFactory.h"
#include "IComponentProvider.h"
#include "LogMgr.h"

using namespace Pakal;

void Pakal::ComponentSystem::registerFactory( IComponentFactory *factory )
{
	registerFactory(factory, factory->getTypeName() );
}

void Pakal::ComponentSystem::registerFactories( IComponentProvider &provider )
{

	std::vector<IComponentFactory*> factories;
	provider.registerComponentFactories(factories);

	for (auto & factory : factories)
	{
		registerFactory(factory);
	}
}

void Pakal::ComponentSystem::registerFactory( IComponentFactory* factory, const std::string & name )
{
	ASSERT(factory);

	if(m_ComponentFactories.find( name ) != m_ComponentFactories.end())
	{
		LOG_ERROR("[ComponentManager] Factory Already registered");
		ASSERT(false);
	}

	m_ComponentFactories[ name ] = factory;
	LOG_DEBUG("[ComponentManager] registered factory for: '%s' component type.", name.c_str() );
}

IComponent * Pakal::ComponentSystem::createComponent( const char * typeName )
{
	const auto &it = m_ComponentFactories.find(typeName);
	if(it == m_ComponentFactories.end())
	{
		LOG_ERROR("[ComponentManager] error: '%s' unknown component type.", typeName );
		return (0);
	}	
	IComponent * newComponent = it->second->create(); //call the creator function		
	return (newComponent);		
}

Pakal::ComponentSystem::~ComponentSystem()
{
	for( auto it = m_ComponentFactories.begin() ; it != m_ComponentFactories.end() ; )
	{
		delete it->second;
		m_ComponentFactories.erase(it++);	
	}
}
