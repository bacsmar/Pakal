#pragma once
#include "Config.h"
#include "IInputManager.h"

namespace Pakal
{
	class OSManager;
	class _PAKALExport InputManager_SFML : public IInputManager
	{
	public:
		InputManager_SFML(OSManager* os_manager);

		void initialize() override;
		void terminate() override;

		virtual IButtonDevice*	get_button_device(unsigned id) override;
		virtual IAxisDevice*	get_axis_device(unsigned id) override;
		virtual ITouchDevice*	get_touch_device(unsigned id) override;

		void register_component_factories(std::vector<IComponentFactory*>& factories) override;
	private:
		std::vector<IButtonDevice*>	m_button_devices;
		std::vector<IAxisDevice*>	m_axis_devices;
		std::vector<ITouchDevice*>	m_touch_devices;
		OSManager*					m_os_manager;
		std::vector<InputDevice*>	m_devices;
	};
}