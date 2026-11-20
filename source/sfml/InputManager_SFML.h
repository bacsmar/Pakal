#pragma once
#include "Config.h"
#include "IInputManager.h"
#include "PakalMath.h"

namespace Pakal
{
	class OSManager;
	class _PAKALExport InputManager_SFML : public IInputManager
	{
		std::vector<IButtonDevice*>	m_button_devices;
		std::vector<IAxisDevice*>	m_axis_devices;
		std::vector<ITouchDevice*>	m_touch_devices;
		OSManager*					m_os_manager;
		std::vector<InputDevice*>	m_devices;
		std::vector<PadBinding>		m_pad_bindings;
		unsigned					m_pad_state[kPadSlots] = {};   // the pad inputs each slot holds (a bit each), read at most every few milliseconds
		int							m_pad_slot[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };   // each joystick's slot (sf::Joystick::Count), -1 until it presses
		int							m_pad_slots_given = 0;
		long long					m_pad_polled_at = 0;
		unsigned					m_focus_listener = 0;   // OSManager::event_window_focused
		void poll_pads();

	public:
		explicit InputManager_SFML(OSManager* os_manager);

		void initialize() override;
		void terminate() override;

		virtual IButtonDevice*	get_button_device(unsigned id) const override;
		virtual IAxisDevice*	get_axis_device(unsigned id) const override;
		virtual ITouchDevice*	get_touch_device(unsigned id) const override;

		void register_component_factories(std::vector<IComponentFactory*>& factories) override;

		tmath::vector3df get_sensor_value(Sensors sensor) const override;
		bool is_sensor_available(Sensors sensor) const override;
		void set_sensor_enabled(Sensors sensor, bool value) override;

		tmath::vector2di get_mouse_position() const override;
		bool is_key_pressed(Pakal::Key key) override;
		void set_pad_bindings(const std::vector<PadBinding>& bindings) override { m_pad_bindings = bindings; }
		bool any_pad_pressed() override;
	};
}