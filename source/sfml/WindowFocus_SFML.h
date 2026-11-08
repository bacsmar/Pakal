#pragma once

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
}
