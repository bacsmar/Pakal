#pragma once
#include <Event.h>
#include "Component.h"


namespace Pakal
{
	class GamepadComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(GamepadComponent, Component);	
	public:

		enum  Buttons 
		{
			A,
			B,
			X,
			Y,
			count,
		};

		struct ButtonArgs{};
		struct AxisArgs{};
		struct TouchArgs{};

		Event<GamepadComponent, ButtonArgs> event_button_pressed;
		Event<GamepadComponent, ButtonArgs> event_button_released;
		Event<GamepadComponent, AxisArgs> event_axis_moved;
		Event<GamepadComponent, TouchArgs> event_touch;

		virtual bool is_button_pressed(Buttons button) = 0;

	protected:
		// methods to be fired by derived classes
		inline void fire_event_button_pressed(ButtonArgs &args) { event_button_pressed.notify(args); }
		inline void fire_event_button_released(ButtonArgs &args) { event_button_released.notify(args); }
		inline void fire_event_axis_moved(AxisArgs &args) { event_axis_moved.notify(args); }
		inline void fire_event_touch(TouchArgs &args) { event_touch.notify(args); }		

		struct Button
		{
			char name;					// ButtonA
			const char* friendly_name;	// Boton disparo
		};

		struct Axis
		{
			float value;
		};
		struct Touchpad
		{
			char index;
			unsigned x, y;
		};
		
		std::vector<Axis>		m_axis;
		std::vector<Touchpad>	m_touchpads;
	};

}