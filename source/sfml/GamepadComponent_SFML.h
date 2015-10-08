#pragma once
#include "components/GamepadComponent.h"
#include <vector>
#include <SFML/Window/Keyboard.hpp>

namespace Pakal
{
	class GamepadComponent_SFML : public GamepadComponent
	{
		DECLARE_RTTI_WITH_BASE(GamepadComponent_SFML, GamepadComponent);
	public:
		virtual bool is_button_pressed(Buttons button) override;

		GamepadComponent_SFML();

	private:
		std::vector<sf::Keyboard::Key>		m_buttons;
	};

	
}