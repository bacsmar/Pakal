#include "InputManager_Polling.h"
#include "IInputManager.h"
#include "LogMgr.h"
#include "InputDevice.h"

namespace Pakal
{
	InputManager_Polling* InputManager_Polling::ms_instance = nullptr;

	InputManager_Polling::InputManager_Polling()
		: m_input_manager(nullptr)
	{
	}

	InputManager_Polling& InputManager_Polling::instance()
	{
		if (ms_instance == nullptr)
		{
			ms_instance = new InputManager_Polling();
		}
		return *ms_instance;
	}

	void InputManager_Polling::initialize(IInputManager* input_manager)
	{
		if (ms_instance == nullptr)
		{
			ms_instance = new InputManager_Polling();
		}
		ms_instance->m_input_manager = input_manager;
		if (input_manager != nullptr)
		{
			LOG_INFO("InputManager_Polling initialized with IInputManager");
		}
		else
		{
			LOG_ERROR("InputManager_Polling::initialize() called with nullptr");
		}
	}

	bool InputManager_Polling::poll_key_down(Key key) const
	{
		if (m_input_manager == nullptr)
		{
			return false;
		}
		return m_input_manager->is_key_pressed(key);
	}

	float InputManager_Polling::get_axis_value(unsigned axis_id) const
	{
		if (m_input_manager == nullptr)
		{
			return 0.0f;
		}
		IAxisDevice* axis_device = m_input_manager->get_axis_device(axis_id);
		if (axis_device != nullptr)
		{
			return axis_device->get_axis_state(0); // Get first virtual axis
		}
		return 0.0f;
	}

	tmath::vector2di InputManager_Polling::get_mouse_position() const
	{
		if (m_input_manager == nullptr)
		{
			return tmath::vector2di(0, 0);
		}
		return m_input_manager->get_mouse_position();
	}
}
