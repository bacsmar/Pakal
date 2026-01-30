#include "Engine.h"
#include "ComponentManager.h"

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
