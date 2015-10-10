#pragma once

#include "Config.h"
#include "IManager.h"
#include "IComponentProvider.h"
#include "InputDevice.h"

namespace Pakal
{
	//class InputDevice;	

	class _PAKALExport IInputManager : public IManager, public IComponentProvider
	{
	public:
		virtual ~IInputManager(){};

		virtual IButtonDevice*	get_button_device(unsigned id) = 0;
		virtual IAxisDevice*	get_axis_device(unsigned id) = 0;
		virtual ITouchDevice*	get_touch_device(unsigned id) = 0;
	};
}