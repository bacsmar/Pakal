
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
	auto b = m_buttons[button];
	return sf::Joystick::isButtonPressed(m_device_id , b);
}

void Pakal::Gamepad_SFML::map_button(unsigned virtualButton, unsigned realButton)
{
	m_buttons[virtualButton] = realButton;
}

Pakal::Gamepad_SFML::Gamepad_SFML()
{
	// default mapping
	m_buttons.resize( static_cast<unsigned>(GamepadComponent::Buttons::Button_count), -1 );
	m_buttons[GamepadComponent::Buttons::Button_A] = 0;
	m_buttons[GamepadComponent::Buttons::Button_B] = 1;
	m_buttons[GamepadComponent::Buttons::Button_X] = 2;
	m_buttons[GamepadComponent::Buttons::Button_Y] = 3;

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
}

Pakal::Gamepad_SFML::~Gamepad_SFML()
{
}