#include  "InputManager_SFML.h"
#include "LogMgr.h"

#include "components/GamepadComponent.h"
#include "SFML/Window.hpp"
#include "SFML/Keyboard_SFML.h"
#include "SFML/Gamepad_SFML.h"
#include "OSManager.h"


using namespace Pakal;

InputManager_SFML::InputManager_SFML(OSManager* os_manager) : m_os_manager(os_manager)
{
}

void InputManager_SFML::initialize()
{
	auto keyboard = new Keyboard_SFML();
	m_devices.emplace_back(keyboard);
	m_button_devices.emplace_back(keyboard);

	sf::Joystick::update();

	for (unsigned i = 0; i < sf::Joystick::Count; i++)
	{
		if(sf::Joystick::isConnected(i))
		{
			auto identification = sf::Joystick::getIdentification(i);
			LOG_INFO("[InputManager] joystick found: %s", identification.name.toAnsiString().c_str());
			auto gamepad = new Gamepad_SFML();
			gamepad->set_device_id(i);
			m_devices.emplace_back(gamepad);
			m_button_devices.emplace_back(gamepad);
			m_axis_devices.emplace_back(gamepad);
		}		
	}
	
}

void InputManager_SFML::terminate()
{	
	for(auto device : m_devices)
	{
		delete device;
	}
	m_devices.clear();
	m_button_devices.clear();	
	m_axis_devices.clear();	
	m_touch_devices.clear();
}

IButtonDevice* InputManager_SFML::get_button_device(unsigned id)
{	
	return id < m_button_devices.size() ? m_button_devices[id] : nullptr;
}

IAxisDevice* InputManager_SFML::get_axis_device(unsigned id)
{	
	return id < m_axis_devices.size() ? m_axis_devices[id] : nullptr;
}

ITouchDevice* InputManager_SFML::get_touch_device(unsigned id)
{	
	return id < m_touch_devices.size() ? m_touch_devices[id] : nullptr;
}

void InputManager_SFML::register_component_factories(std::vector<IComponentFactory*>& factories)
{	
	factories.emplace_back(CreateComponentFactory<GamepadComponent, GamepadComponent>(this));
}