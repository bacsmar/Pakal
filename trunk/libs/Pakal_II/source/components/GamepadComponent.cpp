#include "GamepadComponent.h"
#include  "IInputManager.h"

bool Pakal::GamepadComponent::is_button_pressed(Buttons button)
{
	ASSERT(button < Buttons::Button_count);	
	
	bool pressed = false;
	for( auto device : m_buttonDevices)
	{
		pressed |= device->get_button_state(button);
	}
	return pressed;
}

float Pakal::GamepadComponent::get_axis_state(Axis axis)
{
	ASSERT(false);
	return 0.f;	// not implemented
}

//void Pakal::GamepadComponent::connect()
//{
//	m_buttonDevices.clear();
//	m_axisDevices.clear();
//
//	m_buttonDevices.emplace_back( m_input_manager->get_button_device(0) );	// keyboard
//
//	if( auto buttonDev = m_input_manager->get_button_device(1))
//	{
//		m_buttonDevices.emplace_back(buttonDev);
//	}
//
//	if( auto axisDev = m_input_manager->get_axis_device(1))
//	{
//		m_axisDevices.emplace_back(axisDev);
//	}
//}

void Pakal::GamepadComponent::connectButtonDevice(unsigned deviceId)
{
	if (auto buttonDev = m_input_manager->get_button_device(deviceId))
	{
		buttonDev->button_pressed_event.add_listener([](unsigned )
		{

		});
		buttonDev->button_released_event.add_listener([](unsigned )
		{

		});
		m_buttonDevices.emplace_back(buttonDev);
	}
}

Pakal::GamepadComponent::~GamepadComponent()
{
}
