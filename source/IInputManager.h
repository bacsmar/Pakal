#pragma once

#include "Config.h"
#include "IManager.h"
#include "IComponentProvider.h"
#include "InputDevice.h"
#include "Event.h"

namespace Pakal
{
	class _PAKALExport IInputManager : public IManager, public IComponentProvider
	{
	public:
		virtual ~IInputManager(){};

		virtual IButtonDevice*	get_button_device(unsigned id) = 0;
		virtual IAxisDevice*	get_axis_device(unsigned id) = 0;
		virtual ITouchDevice*	get_touch_device(unsigned id) = 0;

		Event<void> event_mouse_moved;
		Event<unsigned> event_button_pressed;
	};
}