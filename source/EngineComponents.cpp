#include "Engine.h"
#include "ComponentManager.h"
#include "EntityManager.h"
#include "GenericEntity.h"
#include "Factory.h"

#include "components/AutomataComponent.h"
#include "components/ScriptComponent.h"
#include "components/CharacterComponent.h"
#if PAKAL_USE_SCRIPTS == 1
	#include "components/ScriptComponent_Lua.h"
#endif

using namespace Pakal;

void Engine::register_default_components() const
{
	LOG_INFO("[Engine] Registering Basic components");
	m_component_manager->register_factory<AutomataComponent>([]()
	{
		return new AutomataComponent();
	});
	m_component_manager->register_factory<CharacterComponent>([]()
	{
		return new CharacterComponent();
	});
#if PAKAL_USE_SCRIPTS == 1
	m_component_manager->register_factory<ScriptComponent>([]()
	{
		return new ScriptComponentLua();
	});
#else
	m_component_manager->register_factory<ScriptComponent>([]()
	{
		return new DummyScriptComponent();
	});
#endif

}

void Engine::register_default_entities() const
{
	LOG_INFO("[Engine] Registering default entities");
	
	// Capture entity manager to pass to GenericEntity constructor
	auto* entity_mgr = m_entity_manager;
	auto* generic_entity_factory = CreateFactory<Entity>("Pakal::GenericEntity", [entity_mgr]() -> Entity* {
		return new GenericEntity(entity_mgr);
	});
	
	m_entity_manager->add_entity_factory(generic_entity_factory);
	
	LOG_INFO("[Engine] GenericEntity factory registered");
}
