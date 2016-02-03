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
			Pov_X, 
			Pov_Y,  
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

		Event<ButtonArgs>	event_button_pressed;
		Event<ButtonArgs>	event_button_released;
		Event<AxisArgs>		event_axis_moved;
		Event<TouchArgs>	event_touch;

		virtual bool	is_button_pressed(Buttons button);
		virtual float	get_axis_state(Axis axis);

		//virtual void connect();
		virtual void connectButtonDevice(unsigned deviceId);

		explicit GamepadComponent(IInputManager *input_manager) : m_input_manager(input_manager){}
		~GamepadComponent();

	protected:
		std::vector<IButtonDevice*>	m_buttonDevices;
		std::vector<IAxisDevice*>	m_axisDevices;
		IInputManager*				m_input_manager;
	};

}