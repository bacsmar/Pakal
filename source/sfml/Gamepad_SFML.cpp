
#include "Gamepad_SFML.h"
#include "components/GamepadComponent.h"

float Pakal::Gamepad_SFML::get_axis_state(unsigned v_axis)
{
	auto axis = m_axis[v_axis];
	return sf::Joystick::getAxisPosition(m_device_id, axis);
}

void Pakal::Gamepad_SFML::map_axis(unsigned v_axis, unsigned real_axis)
{
	m_axis[v_axis] = static_cast<sf::Joystick::Axis>(real_axis);
}

bool Pakal::Gamepad_SFML::get_button_state(unsigned button)
{
	sf::Joystick::update();
	auto b = m_buttons[button];
	return b();
}

void Pakal::Gamepad_SFML::map_button(unsigned virtualButton, unsigned realButton)
{
	m_buttons[virtualButton] = [this, realButton]()->bool { return sf::Joystick::isButtonPressed(m_device_id, realButton); };;
}

Pakal::Gamepad_SFML::Gamepad_SFML()
{
	// default mapping
	auto lambda = []() { return false; };
	m_buttons.resize(static_cast<unsigned>(GamepadComponent::Buttons::Button_count), lambda);
	m_buttons[GamepadComponent::Buttons::Button_A] = [this]()->bool { return sf::Joystick::isButtonPressed(m_device_id, 0); };
	m_buttons[GamepadComponent::Buttons::Button_B] = [this]()->bool { return sf::Joystick::isButtonPressed(m_device_id, 1); };
	m_buttons[GamepadComponent::Buttons::Button_X] = [this]()->bool { return sf::Joystick::isButtonPressed(m_device_id, 2); };
	m_buttons[GamepadComponent::Buttons::Button_Y] = [this]()->bool { return sf::Joystick::isButtonPressed(m_device_id, 3); };

	//auto ypos = sf::Joystick::getAxisPosition(m_device_id, sf::Joystick::Pov_X);
	//auto zpos = sf::Joystick::getAxisPosition(m_device_id, sf::Joystick::Pov_Y);

	m_buttons[GamepadComponent::Buttons::Button_LEFT] = [this]()->bool { return sf::Joystick::getAxisPosition(m_device_id, sf::Joystick::X) < -2; };
	m_buttons[GamepadComponent::Buttons::Button_RIGHT] = [this]()->bool { return sf::Joystick::getAxisPosition(m_device_id, sf::Joystick::X) > 2; };
	m_buttons[GamepadComponent::Buttons::Button_UP] = [this]()->bool { return sf::Joystick::getAxisPosition(m_device_id, sf::Joystick::Y) < -2; };
	m_buttons[GamepadComponent::Buttons::Button_DOWN] = [this]()->bool { return sf::Joystick::getAxisPosition(m_device_id, sf::Joystick::Y) > 2; };

	/*m_buttons[GamepadComponent::Buttons::Button_] = 5;
	m_buttons[GamepadComponent::Buttons::Button_Y] = 6;
	m_buttons[GamepadComponent::Buttons::Button_Y] = 7;
	m_buttons[GamepadComponent::Buttons::Button_Y] = 8;
	m_buttons[GamepadComponent::Buttons::Button_Y] = 9;
	m_buttons[GamepadComponent::Buttons::Button_Y] = 10;
	*/
	m_axis.resize( GamepadComponent::Axis::Axis_count);
	m_axis[GamepadComponent::Axis_X] = sf::Joystick::X;
	m_axis[GamepadComponent::Axis_Y] = sf::Joystick::Y;	
	m_axis[GamepadComponent::Axis_Z] = sf::Joystick::Z;	
}

Pakal::Gamepad_SFML::~Gamepad_SFML()
{
}