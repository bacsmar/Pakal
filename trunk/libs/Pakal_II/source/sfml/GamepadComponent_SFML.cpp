
#include "GamepadComponent_SFML.h"

bool Pakal::GamepadComponent_SFML::is_button_pressed(Buttons button)
{
	// std::map<Buttons, equivalent>
	auto key = m_buttons[button];

	return sf::Keyboard::isKeyPressed( key);
}

Pakal::GamepadComponent_SFML::GamepadComponent_SFML()
{
	m_buttons.resize( static_cast<unsigned>(Buttons::count));
	m_buttons[Buttons::A] = sf::Keyboard::Key::A;
	m_buttons[Buttons::B] = sf::Keyboard::Key::B;
	m_buttons[Buttons::X] = sf::Keyboard::Key::X;
	m_buttons[Buttons::Y] = sf::Keyboard::Key::Y;	
	
}