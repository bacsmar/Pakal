#pragma once

#include "OSManager.h"
#include "IInputManager.h"

namespace Pakal
{
	// A window that does not exist: the engine without a platform (the core alone, a headless run). It reports the size it
	// was asked for.
	class _PAKALExport DummyWindowManager : public OSManager::IWindowManager
	{
	public:
		WindowArgs setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned int bitsPerPixel) override
		{
			m_size = dimensions;
			return WindowArgs(windowId == 0 ? 1 : windowId, dimensions.x, dimensions.y);
		}
		void close_window() override {}
		void process_window_events() override {}
		void set_title(const std::string& title) override {}
		tmath::vector2di get_position() override { return tmath::vector2di(0, 0); }
		tmath::vector2di get_size() override { return m_size; }

	private:
		tmath::vector2di m_size = tmath::vector2di(0, 0);
	};

	// Input with no devices: nothing is pressed, nothing moves
	class _PAKALExport DummyInputManager : public IInputManager
	{
	public:
		void initialize() override {}
		void terminate() override {}
		void register_component_factories(std::vector<IComponentFactory*>& factories) override {}

		IButtonDevice* get_button_device(unsigned id) const override { return nullptr; }
		IAxisDevice* get_axis_device(unsigned id) const override { return nullptr; }
		ITouchDevice* get_touch_device(unsigned id) const override { return nullptr; }
		tmath::vector2di get_mouse_position() const override { return tmath::vector2di(0, 0); }

		tmath::vector3df get_sensor_value(Sensors sensor) const override { return tmath::vector3df(0, 0, 0); }
		bool is_sensor_available(Sensors sensor) const override { return false; }
		void set_sensor_enabled(Sensors sensor, bool value) override {}

		bool is_key_pressed(Pakal::Key key) override { return false; }
	};
}
