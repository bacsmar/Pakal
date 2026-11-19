#pragma once
#include <mutex>

namespace Pakal
{
	// Whether the game's window has the focus. SFML reads the keyboard and the gamepads as the whole system has them, focused or not:
	// a player writing in another window would be playing too. The input manager follows the window's focus (OSManager's
	// event_window_focused) and its devices read nothing while the window does not have it
	namespace WindowFocus_SFML
	{
		bool focused();
		void set_focused(bool focused);
	}

	// SFML's joysticks are one state for the whole program, not safe from two threads at once: the window's events update it (pollEvent
	// looks for pads plugged in or out, opening and closing their DirectInput devices) while the input manager reads it on the game's
	// thread. Each use of sf::Joystick, and each pollEvent, holds this lock (a device closed by one thread while the other used it crashed
	// the game now and then, after many minutes)
	namespace JoystickLock_SFML
	{
		std::mutex& mutex();
	}
}
