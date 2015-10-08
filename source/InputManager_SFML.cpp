#include  "InputManager_SFML.h"
#include "LogMgr.h"

#include "sfml/GamepadComponent_SFML.h"


using namespace Pakal;

InputManager_SFML::InputManager_SFML() 
{
	
}

void InputManager_SFML::initialize()
{
	
}

void InputManager_SFML::terminate()
{	
}

void InputManager_SFML::register_component_factories(std::vector<IComponentFactory*>& factories)
{
	factories.emplace_back(CreateComponentFactory<GamepadComponent, GamepadComponent_SFML>());
}