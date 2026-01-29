#pragma once
#include "InputDevice.h"
#include <vector>
#include <SFML/Window/Joystick.hpp>
#include <functional>

namespace Pakal
{
	class Gamepad_SFML : public  IButtonDevice, public  IAxisDevice
	{
	public:
		// IDevice
		void set_device_id(unsigned deviceId) override { m_device_id = deviceId; };
		unsigned get_device_id() const override { return m_device_id; };
		// IButtonDevice
		float get_axis_state(unsigned v_axis) override;
		void map_axis(unsigned v_axis, unsigned real_axis) override;
		// IAxisDevice
		bool get_button_state(unsigned button) override;
		void map_button(unsigned virtualButton, unsigned realButton) override;	

		Gamepad_SFML();
		~Gamepad_SFML() override;
	private:
		std::vector< std::function<bool()> >			m_buttons;
		std::vector<sf::Joystick::Axis>	m_axis;
		unsigned m_device_id = 0;
	};	
}