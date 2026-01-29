#pragma once
#include "InputDevice.h"
#include <vector>
#include <SFML/Window/Keyboard.hpp>

namespace Pakal
{
	class Keyboard_SFML : public IButtonDevice
	{
	public:
		void set_device_id(unsigned deviceId) override { m_device_id = deviceId; };
		unsigned get_device_id() const override { return m_device_id; };
		bool get_button_state(unsigned button) override;
		void map_button(unsigned virtualButton, unsigned realButton) override;	
		Keyboard_SFML();
		~Keyboard_SFML() override;

	private:
		std::vector<sf::Keyboard::Key>		m_buttons;
		unsigned m_device_id = 0;
	};	
}