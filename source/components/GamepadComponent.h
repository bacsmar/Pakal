#pragma once
#include <Event.h>
#include "Component.h"
#include "InputDevice.h"
#include <set>

namespace Pakal
{	
	class IInputManager;
	class GamepadComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(GamepadComponent, Component);
	public:
		friend class InputManager;
		enum  Buttons 
		{
			Button_A, Button_B, Button_X, Button_Y,
			Button_LEFT, Button_RIGHT, Button_UP, Button_DOWN,
			Button_count,
		};

		enum Axis
		{
			Axis_X,    
			Axis_Y,    
			Axis_Z,    
			Axis_R,
			Axis_U,
			Axis_V,
			PovX, 
			PovY,  
			Axis_count,
		};
		enum Touches{};

		struct ButtonArgs
		{
			Buttons button;
			bool pressed;
		};
		struct AxisArgs
		{
			Axis axis;
			float value;
		};
		struct TouchArgs
		{
			Touches touch;
			unsigned x, y;
		};

		Event<GamepadComponent, ButtonArgs> event_button_pressed;
		Event<GamepadComponent, ButtonArgs> event_button_released;
		Event<GamepadComponent, AxisArgs>	event_axis_moved;
		Event<GamepadComponent, TouchArgs>	event_touch;

		virtual bool is_button_pressed(Buttons button);
		virtual float get_axis_state(Axis axis);

		virtual void connect();

		explicit GamepadComponent(IInputManager *input_manager) : m_input_manager(input_manager){}
		~GamepadComponent();

	protected:
		// methods to be fired by derived classes
		inline void fire_event_button_pressed(ButtonArgs &args)		{ event_button_pressed.notify(args); }
		inline void fire_event_button_released(ButtonArgs &args)	{ event_button_released.notify(args); }
		inline void fire_event_axis_moved(AxisArgs &args)			{ event_axis_moved.notify(args); }
		inline void fire_event_touch(TouchArgs &args)				{ event_touch.notify(args); }
				
		std::vector<IButtonDevice*>	m_buttonDevices;
		std::vector<IAxisDevice*>	m_axisDevices;
		IInputManager*				m_input_manager;
	};

}