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

IButtonDevice* InputManager_SFML::get_button_device(unsigned id) const
{	
	return id < m_button_devices.size() ? m_button_devices[id] : nullptr;
}

IAxisDevice* InputManager_SFML::get_axis_device(unsigned id) const
{	
	return id < m_axis_devices.size() ? m_axis_devices[id] : nullptr;
}

ITouchDevice* InputManager_SFML::get_touch_device(unsigned id) const
{	
	return id < m_touch_devices.size() ? m_touch_devices[id] : nullptr;
}

void InputManager_SFML::register_component_factories(std::vector<IComponentFactory*>& factories)
{	
	factories.emplace_back(CreateComponentFactory<GamepadComponent, GamepadComponent>(this));
}

sf::Sensor::Type pakal_sensor_SFML(InputManager_SFML::Sensors sensor)
{	
	switch (sensor)
	{
	case InputManager_SFML:: Sensors::Accelerometer:
		return sf::Sensor::Accelerometer;
	case InputManager_SFML:: Sensors::Gyroscope:
		return sf::Sensor::Gyroscope;
	case InputManager_SFML:: Sensors::Magnetometer:
		return sf::Sensor::Magnetometer;
	case InputManager_SFML:: Sensors::Gravity:
		return sf::Sensor::Gravity;
	case InputManager_SFML:: Sensors::UserAcceleration:
		return sf::Sensor::UserAcceleration;
	case InputManager_SFML:: Sensors::Orientation:
		return sf::Sensor::Orientation;
	default: break;
	}
	return { sf::Sensor::Gravity };
}

tmath::vector3df InputManager_SFML::get_sensor_value(Sensors sensor) const
{	
	auto s = pakal_sensor_SFML(sensor);
	auto v = sf::Sensor::getValue(s);
	return{ v.x, v.y, v.z };
}

bool InputManager_SFML::is_sensor_available(Sensors sensor) const
{
	return sf::Sensor::isAvailable(pakal_sensor_SFML(sensor));
}

void InputManager_SFML::set_sensor_enabled(Sensors sensor, bool value)
{
	sf::Sensor::setEnabled(pakal_sensor_SFML(sensor), value);
}

tmath::vector2di InputManager_SFML::get_mouse_position() const
{
	auto pos = sf::Mouse::getPosition();
	return{ pos.x, pos.y };
}

bool InputManager_SFML::is_key_pressed(Pakal::Key key)
{
	auto k = (sf::Keyboard::Key)(key);
	return sf::Keyboard::isKeyPressed(k);
}
