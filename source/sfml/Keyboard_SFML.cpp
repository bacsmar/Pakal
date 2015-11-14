
#include "Keyboard_SFML.h"
#include "components/GamepadComponent.h"

bool Pakal::Keyboard_SFML::get_button_state(unsigned button)
{
	auto key = m_buttons[button];
	return sf::Keyboard::isKeyPressed(key);
}

void Pakal::Keyboard_SFML::map_button(unsigned virtualButton, unsigned realButton)
{
	m_buttons[virtualButton] = static_cast<sf::Keyboard::Key>(realButton);
}

Pakal::Keyboard_SFML::Keyboard_SFML()
{
	m_buttons.resize(static_cast<unsigned>(GamepadComponent::Buttons::Button_count));
	m_buttons[GamepadComponent::Buttons::Button_A] = sf::Keyboard::Key::A;
	m_buttons[GamepadComponent::Buttons::Button_B] = sf::Keyboard::Key::B;
	m_buttons[GamepadComponent::Buttons::Button_X] = sf::Keyboard::Key::X;
	m_buttons[GamepadComponent::Buttons::Button_Y] = sf::Keyboard::Key::Y;

	m_buttons[GamepadComponent::Buttons::Button_DOWN] = sf::Keyboard::Key::Down;
	m_buttons[GamepadComponent::Buttons::Button_RIGHT] = sf::Keyboard::Key::Right;
	m_buttons[GamepadComponent::Buttons::Button_LEFT] = sf::Keyboard::Key::Left;
	m_buttons[GamepadComponent::Buttons::Button_UP] = sf::Keyboard::Key::Up;
}

Pakal::Keyboard_SFML::~Keyboard_SFML()
{
}