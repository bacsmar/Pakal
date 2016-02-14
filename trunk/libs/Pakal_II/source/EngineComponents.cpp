#include "Engine.h"
#include "ComponentManager.h"

#include "Components/AutomataComponent.h"
#include "Components/ScriptComponent.h"
#if PAKAL_USE_SCRIPTS == 1
	#include "components/ScriptComponent_Lua.h"
#endif

using namespace Pakal;

void Engine::register_default_components()
{
	LOG_INFO("[Engine] Registering Basic components");
	m_component_manager->register_factory<AutomataComponent>([]()
	{
		return new AutomataComponent();
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