#include  "InputManager_SFML.h"
#include "LogMgr.h"

#include "components/GamepadComponent.h"
#include "SFML/Keyboard_SFML.h"
#include "SFML/Gamepad_SFML.h"


using namespace Pakal;

InputManager_SFML::InputManager_SFML() 
{	
}

void InputManager_SFML::initialize()
{
	// register internal button,axis and touch devices
	auto keyboard = new Keyboard_SFML();
	m_devices.emplace_back(keyboard);
	m_button_devices.emplace_back(keyboard);

	for (unsigned i = 0; i < sf::Joystick::Count; i++)
	{
		//if (sf::Joystick::isConnected(0) == false)
		//	continue;
		auto identification = sf::Joystick::getIdentification(i);
		auto gamepad = new Gamepad_SFML();
		gamepad->set_device_id( i + 1 );
		m_devices.emplace_back(gamepad);
		m_button_devices.emplace_back(gamepad);
		m_axis_devices.emplace_back(gamepad);
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
	ASSERT(id < m_button_devices.size());
	return m_button_devices[id];
}

IAxisDevice* InputManager_SFML::get_axis_device(unsigned id)
{
	ASSERT(id < m_axis_devices.size());
	return m_axis_devices[id];
}

ITouchDevice* InputManager_SFML::get_touch_device(unsigned id)
{
	ASSERT(id < m_touch_devices.size());
	return m_touch_devices[id];
}

void InputManager_SFML::register_component_factories(std::vector<IComponentFactory*>& factories)
{
	//factories.emplace_back(CreateComponentFactory<GamepadComponent, GamepadComponent_SFML>());
	factories.emplace_back(CreateComponentFactory<GamepadComponent, GamepadComponent>(this));
}