#pragma once

namespace Pakal
{
	class InputDevice
	{
	public:
		virtual ~InputDevice() {}
		virtual void		set_device_id(unsigned deviceId) = 0;
		virtual unsigned	get_device_id() const = 0;
	};

	struct IButtonDevice : public virtual InputDevice
	{
		virtual ~IButtonDevice() {}
		virtual bool get_button_state(unsigned v_button) = 0;
		virtual void map_button(unsigned v_button, unsigned real_button) = 0;
	};

	struct IAxisDevice : public virtual InputDevice
	{
		virtual ~IAxisDevice() {}
		virtual float get_axis_state(unsigned v_axis) = 0;
		virtual void map_axis( unsigned v_axis, unsigned real_axis ) = 0;		
	};

	struct ITouchDevice : public virtual InputDevice
	{
		virtual ~ITouchDevice() {}
		virtual bool get_touch_state(unsigned v_touch) = 0;
		virtual void map_touch(unsigned v_touch, unsigned real_touch) = 0;
	};	

}