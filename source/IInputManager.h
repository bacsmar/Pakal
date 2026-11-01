#pragma once

#include "Config.h"
#include "IManager.h"
#include "IComponentProvider.h"
#include "InputDevice.h"
#include "Event.h"
#include "EventArgs.h"
#include "PakalMath.h"

namespace Pakal
{
	struct _PAKALExport ISensor
	{
		virtual ~ISensor()
		{
		}

		enum class Sensors
		{
			Accelerometer,
			Gyroscope,
			Magnetometer,
			Gravity,
			UserAcceleration,
			Orientation,
		};
		virtual tmath::vector3df get_sensor_value(Sensors sensor) const = 0;
		virtual bool is_sensor_available(Sensors sensor) const = 0;
		virtual void set_sensor_enabled(Sensors sensor, bool value) = 0;
	};

	struct _PAKALExport IKeyboard
	{
		virtual ~IKeyboard()
		{
		}

		virtual bool is_key_pressed(Pakal::Key key) = 0;
	};

	class _PAKALExport IInputManager : public IManager, public IComponentProvider, public ISensor, public IKeyboard
	{
	public:
		virtual ~IInputManager(){}

		virtual IButtonDevice*	get_button_device(unsigned id) const = 0;
		virtual IAxisDevice*	get_axis_device(unsigned id) const = 0;
		virtual ITouchDevice*	get_touch_device(unsigned id) const = 0;

		Event<MouseArgs> event_mouse_moved;
		Event<MouseArgs> event_mouse_pressed;
		Event<MouseArgs> event_mouse_released;
		Event<MouseArgs>  event_mouse_wheel;
		Event<TextArgs>  event_text;
		Event<KeyArgs>  event_key_down;
		Event<KeyArgs>  event_key_up;	

		Event<TouchArgs> event_touch_began;
		Event<TouchArgs> event_touch_moved;
		Event<TouchArgs> event_touch_ended;


		virtual tmath::vector2di get_mouse_position() const = 0;

		// The touch controls drawn over the picture (a phone's sticks) are the game's while it is played: on in a level, off in
		// its menus and viewers (off at the start). A platform without them ignores it.
		virtual void set_touch_controls(bool shown) { (void)shown; }
		// Whether the game aims with the right half of a touch screen (on at the start): a game that does not (a brawler,
		// whose buttons are there) turns it off, and that half is neither drawn as a stick nor read as one
		virtual void set_touch_aim(bool aims) { (void)aims; }
		// A game that reads the touch screen itself (its own sticks, drawn by its interface): every finger off the interface
		// goes to event_touch_began/moved/ended, and the platform's sticks are neither read nor drawn
		virtual void set_game_touches(bool on) { (void)on; }
		// The screen's size in the pixels event_touch_* come in ({0, 0}: no touch screen)
		virtual tmath::vector2di get_touch_area() const { return { 0, 0 }; }
	};
}