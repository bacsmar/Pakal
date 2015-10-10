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
	return 0.f;
}

void Pakal::GamepadComponent::connect()
{
	m_buttonDevices.clear();
	m_axisDevices.clear();

	m_buttonDevices.emplace_back( m_input_manager->get_button_device(0) );	// keyboard

	m_buttonDevices.emplace_back( m_input_manager->get_button_device(1) );
	m_axisDevices.emplace_back(m_input_manager->get_axis_device(1));
}

Pakal::GamepadComponent::~GamepadComponent()
{
}