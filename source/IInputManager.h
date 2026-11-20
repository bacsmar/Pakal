#pragma once

#include "Config.h"
#include "IManager.h"
#include "IComponentProvider.h"
#include "InputDevice.h"
#include "Event.h"
#include "EventArgs.h"
#include "PakalMath.h"

#include <atomic>
#include <mutex>
#include <vector>

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

	// A standard gamepad (an Xbox's, or any that has the same layout): its buttons by their names, and its left stick and d-pad as four directions
	// (either gives them) and its triggers as buttons
	enum class PadInput
	{
		A, B, X, Y, LeftBumper, RightBumper, Back, Start, LeftStick, RightStick,
		Left, Right, Up, Down, LeftTrigger, RightTrigger
	};

	// What a game makes of the pad: each input reads as a key, so that what a game reads from the keyboard (is_key_pressed) a pad can give. An
	// input may be bound to several keys (a button that does one thing in the fight and another in the menus). With several pads, each takes a
	// slot the first time it presses something (0, then 1, up to kPadSlots): a binding with a slot hears only that pad, one without (-1) any
	struct PadBinding
	{
		PadInput input;
		Pakal::Key key;
		int pad = -1;
	};
	constexpr int kPadSlots = 4;

	class _PAKALExport IInputManager : public IManager, public IComponentProvider, public ISensor, public IKeyboard
	{
	public:
		IInputManager() = default;
		IInputManager(const IInputManager&) = delete;
		IInputManager& operator=(const IInputManager&) = delete;
		virtual ~IInputManager();

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
		// How many fingers are on the screen, told each time it changes: all of them, those the platform keeps (its sticks, the
		// interface's buttons) as well. What a number of fingers means (a gesture) is the game's to decide
		Event<unsigned> event_touch_count;


		virtual tmath::vector2di get_mouse_position() const = 0;

		// The gamepad's inputs the game reads as keys (is_key_pressed gives true for a key while the keyboard has it down, or a pad holds an input
		// bound to it). A platform without gamepads ignores it
		virtual void set_pad_bindings(const std::vector<PadBinding>& bindings) { (void)bindings; }
		// Whether any gamepad holds any button now, bound or not (a splash any press skips). False without pads
		virtual bool any_pad_pressed() { return false; }

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

		// --- Synthetic input: what the engine's QA tools (source/mcp/QaTools) press, so that a program drives the game ---
		// Queued from any thread; the platform delivers it where it reads its own input (OSManager::process_window_events,
		// on the thread that handles the window's events) and through the same path: the same events, the same keys held,
		// the same touches (on a phone the touch screen's own code: the interface, the sticks). A game cannot tell it apart.
		struct SyntheticEvent
		{
			enum class Kind { KeyDown, KeyUp, PadDown, PadUp, PadStick, TouchDown, TouchMove, TouchUp, MouseMove, MouseDown, MouseUp, MouseWheel };
			Kind kind = Kind::KeyDown;
			Key key = Key::Unknown;
			PadInput pad = PadInput::A;
			int slot = 0;                   // a pad's seat (0 .. kPadSlots - 1)
			unsigned stick = 0;             // PadStick: 0 the left stick, 1 the right one; x right, y up, -1..1
			float x = 0.0f, y = 0.0f;       // pixels of the window (the touch screen's on a phone), or 0..1 when normalized
			bool normalized = false;
			int finger = 0;                 // a touch's id
			MouseButton button = MouseButton::Left;
			float delta = 0.0f;             // the wheel
			long long due_ms = 0;           // not delivered before this (steady clock, milliseconds; 0: at once)
		};
		// Queues one; returns its number (synthetic_delivered() reaches it once it has been delivered)
		unsigned long long queue_synthetic(const SyntheticEvent& event);
		unsigned long long synthetic_delivered() const { return m_synthetic_delivered.load(); }
		bool synthetic_pending() const { std::lock_guard<std::mutex> lock(m_synthetic_mutex); return !m_synthetic_queue.empty(); }
		// The platform: delivers the events that are due ('area': the size of the window or touch screen, in the pixels its
		// events come in, for the normalized ones). Called on the thread that handles the window's events.
		void deliver_synthetic(const tmath::vector2di& area);
		// What synthetic input holds now: a key, the pad inputs of a seat (a bit per PadInput; the left stick past half its way
		// counts as Left/Right/Up/Down, as a real pad's does), a stick's axis (0-1 the left stick, 2-3 the right; y up)
		bool synthetic_key_held(Key key) const;
		unsigned synthetic_pad_held(int slot) const;
		float synthetic_axis(int slot, unsigned axis) const;
		// Whether a synthetic key, or a synthetic pad input bound to the key, holds it: what is_key_pressed adds on every platform
		bool synthetic_pressed(Key key, const std::vector<PadBinding>& bindings) const;
		// Axis device 0 as a pad's sticks while synthetic sticks are pushed (null otherwise): what get_axis_device(0) gives then
		IAxisDevice* synthetic_sticks() const;
		static long long synthetic_now_ms();

	protected:
		// How an event reaches the game, once its held state is set: the desktop's way by default (keys as event_key_down/up,
		// the mouse as event_mouse_*, a touch as the mouse, as SFML gives a touch). A platform with other input (a phone's touch
		// screen) overrides it. The pad's inputs and sticks are states only (no events on any platform).
		virtual void on_synthetic(const SyntheticEvent& event);

	private:
		class SyntheticSticks;
		void apply_synthetic_state(const SyntheticEvent& event);
		mutable std::mutex m_synthetic_mutex;
		std::vector<SyntheticEvent> m_synthetic_queue;
		std::vector<unsigned long long> m_synthetic_numbers;
		unsigned long long m_synthetic_queued = 0;
		std::atomic<unsigned long long> m_synthetic_delivered{ 0 };
		std::atomic<bool> m_synthetic_keys[static_cast<int>(Key::KeyCount)] = {};
		std::atomic<unsigned> m_synthetic_pad[kPadSlots] = {};
		std::atomic<float> m_synthetic_stick[kPadSlots][4] = {};
		mutable SyntheticSticks* m_synthetic_sticks = nullptr;   // made the first time it is asked for
	};
}