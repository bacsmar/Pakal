#include "Engine.h"
#include "ComponentManager.h"

#include "Components/AutomataComponent.h"

using namespace Pakal;

void Engine::register_default_components()
{
	m_component_manager->register_factory<AutomataComponent>([]()
	{
		return new AutomataComponent();
	});

}
