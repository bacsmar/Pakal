#pragma once

#include "Config.h"
#include "IManager.h"
#include "IComponentProvider.h"
#include "InputDevice.h"
#include "Event.h"

#include "EventArgs.h"

namespace Pakal
{
	class _PAKALExport IInputManager : public IManager, public IComponentProvider
	{
	public:
		virtual ~IInputManager(){}

		virtual IButtonDevice*	get_button_device(unsigned id) = 0;
		virtual IAxisDevice*	get_axis_device(unsigned id) = 0;
		virtual ITouchDevice*	get_touch_device(unsigned id) = 0;

		Event<MouseArgs> event_mouse_moved;
		Event<MouseArgs> event_mouse_pressed;
		Event<MouseArgs> event_mouse_released;
		Event<MouseArgs>  event_mouse_wheel;
		Event<TextArgs>  event_text;
		Event<KeyArgs>  event_key_down;
		Event<KeyArgs>  event_key_up;
	};
}