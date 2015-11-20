#include "Engine.h"
#include "ComponentManager.h"

#include "Components/AutomataComponent.h"
#include "Components/ScriptComponent.h"


using namespace Pakal;

void Engine::register_default_components()
{
	m_component_manager->register_factory<AutomataComponent>([]()
	{
		return new AutomataComponent();
	});

	m_component_manager->register_factory<ScriptComponent>([]()
	{
		return new ScriptComponent();
	});

}