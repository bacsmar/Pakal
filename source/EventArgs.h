///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 The PakalEngine Open Source Project
// File: EventArgs.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: 03-sep-2015
//
// Purpose: This file contains common  arguments used in Events
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "config.h"

namespace Pakal
{
	enum class MouseButton
	{
		Left,
		Right,
		Middle,
		XButton1,
		XButton2,
	};

	enum class Key
	{
		Unknown = -1, ///< Unhandled key
		A = 0,        ///< The A key
		B,            ///< The B key
		C,            ///< The C key
		D,            ///< The D key
		E,            ///< The E key
		F,            ///< The F key
		G,            ///< The G key
		H,            ///< The H key
		I,            ///< The I key
		J,            ///< The J key
		K,            ///< The K key
		L,            ///< The L key
		M,            ///< The M key
		N,            ///< The N key
		O,            ///< The O key
		P,            ///< The P key
		Q,            ///< The Q key
		R,            ///< The R key
		S,            ///< The S key
		T,            ///< The T key
		U,            ///< The U key
		V,            ///< The V key
		W,            ///< The W key
		X,            ///< The X key
		Y,            ///< The Y key
		Z,            ///< The Z key
		Num0,         ///< The 0 key
		Num1,         ///< The 1 key
		Num2,         ///< The 2 key
		Num3,         ///< The 3 key
		Num4,         ///< The 4 key
		Num5,         ///< The 5 key
		Num6,         ///< The 6 key
		Num7,         ///< The 7 key
		Num8,         ///< The 8 key
		Num9,         ///< The 9 key
		Escape,       ///< The Escape key
		LControl,     ///< The left Control key
		LShift,       ///< The left Shift key
		LAlt,         ///< The left Alt key
		LSystem,      ///< The left OS specific key: window (Windows and Linux), apple (MacOS X), ...
		RControl,     ///< The right Control key
		RShift,       ///< The right Shift key
		RAlt,         ///< The right Alt key
		RSystem,      ///< The right OS specific key: window (Windows and Linux), apple (MacOS X), ...
		Menu,         ///< The Menu key
		LBracket,     ///< The [ key
		RBracket,     ///< The ] key
		SemiColon,    ///< The ; key
		Comma,        ///< The , key
		Period,       ///< The . key
		Quote,        ///< The ' key
		Slash,        ///< The / key
		BackSlash,    ///< The \ key
		Tilde,        ///< The ~ key
		Equal,        ///< The = key
		Dash,         ///< The - key
		Space,        ///< The Space key
		Return,       ///< The Return key
		BackSpace,    ///< The Backspace key
		Tab,          ///< The Tabulation key
		PageUp,       ///< The Page up key
		PageDown,     ///< The Page down key
		End,          ///< The End key
		Home,         ///< The Home key
		Insert,       ///< The Insert key
		Delete,       ///< The Delete key
		Add,          ///< The + key
		Subtract,     ///< The - key
		Multiply,     ///< The * key
		Divide,       ///< The / key
		Left,         ///< Left arrow
		Right,        ///< Right arrow
		Up,           ///< Up arrow
		Down,         ///< Down arrow
		Numpad0,      ///< The numpad 0 key
		Numpad1,      ///< The numpad 1 key
		Numpad2,      ///< The numpad 2 key
		Numpad3,      ///< The numpad 3 key
		Numpad4,      ///< The numpad 4 key
		Numpad5,      ///< The numpad 5 key
		Numpad6,      ///< The numpad 6 key
		Numpad7,      ///< The numpad 7 key
		Numpad8,      ///< The numpad 8 key
		Numpad9,      ///< The numpad 9 key
		F1,           ///< The F1 key
		F2,           ///< The F2 key
		F3,           ///< The F3 key
		F4,           ///< The F4 key
		F5,           ///< The F5 key
		F6,           ///< The F6 key
		F7,           ///< The F7 key
		F8,           ///< The F8 key
		F9,           ///< The F9 key
		F10,          ///< The F10 key
		F11,          ///< The F11 key
		F12,          ///< The F12 key
		F13,          ///< The F13 key
		F14,          ///< The F14 key
		F15,          ///< The F15 key
		Pause,        ///< The Pause key

		KeyCount      ///< Keep last -- the total number of keyboard keys
	};

	inline const char* KeyName(Key key) {		
		static const char *names[] = 
		{
			"Unknown", ///< Unhandled key
			"A",        ///< The A key
			"B",            ///< The B key
			"C",            ///< The C key
			"D",            ///< The D key
			"E",            ///< The E key
			"F",            ///< The F key
			"G",            ///< The G key
			"H",            ///< The H key
			"I",            ///< The I key
			"J",            ///< The J key
			"K",            ///< The K key
			"L",            ///< The L key
			"M",            ///< The M key
			"N",            ///< The N key
			"O",            ///< The O key
			"P",            ///< The P key
			"Q",            ///< The Q key
			"R",            ///< The R key
			"S",            ///< The S key
			"T",            ///< The T key
			"U",            ///< The U key
			"V",            ///< The V key
			"W",            ///< The W key
			"X",            ///< The X key
			"Y",            ///< The Y key
			"Z",            ///< The Z key
			"Num0",         ///< The 0 key
			"Num1",         ///< The 1 key
			"Num2",         ///< The 2 key
			"Num3",         ///< The 3 key
			"Num4",         ///< The 4 key
			"Num5",         ///< The 5 key
			"Num6",         ///< The 6 key
			"Num7",         ///< The 7 key
			"Num8",         ///< The 8 key
			"Num9",         ///< The 9 key
			"Escape",       ///< The Escape key
			"LControl",     ///< The left Control key
			"LShift",       ///< The left Shift key
			"LAlt",         ///< The left Alt key
			"LSystem",      ///< The left OS specific key: window (Windows and Linux), apple (MacOS X), ...
			"RControl",     ///< The right Control key
			"RShift",       ///< The right Shift key
			"RAlt",         ///< The right Alt key
			"RSystem",      ///< The right OS specific key: window (Windows and Linux), apple (MacOS X), ...
			"Menu",         ///< The Menu key
			"LBracket",     ///< The [ key
			"RBracket",     ///< The ] key
			"SemiColon",    ///< The ; key
			"Comma",        ///< The , key
			"Period",       ///< The . key
			"Quote",        ///< The ' key
			"Slash",        ///< The / key
			"BackSlash",    ///< The \ key
			"Tilde",        ///< The ~ key
			"Equal",        ///< The = key
			"Dash",         ///< The - key
			"Space",        ///< The Space key
			"Return",       ///< The Return key
			"BackSpace",    ///< The Backspace key
			"Tab",          ///< The Tabulation key
			"PageUp",       ///< The Page up key
			"PageDown",     ///< The Page down key
			"End",          ///< The End key
			"Home",         ///< The Home key
			"Insert",       ///< The Insert key
			"Delete",       ///< The Delete key
			"Add",          ///< The + key
			"Subtract",     ///< The - key
			"Multiply",     ///< The * key
			"Divide",       ///< The / key
			"Left",         ///< Left arrow
			"Right",        ///< Right arrow
			"Up",           ///< Up arrow
			"Down",         ///< Down arrow
			"Numpad0",      ///< The numpad 0 key
			"Numpad1",      ///< The numpad 1 key
			"Numpad2",      ///< The numpad 2 key
			"Numpad3",      ///< The numpad 3 key
			"Numpad4",      ///< The numpad 4 key
			"Numpad5",      ///< The numpad 5 key
			"Numpad6",      ///< The numpad 6 key
			"Numpad7",      ///< The numpad 7 key
			"Numpad8",      ///< The numpad 8 key
			"Numpad9",      ///< The numpad 9 key
			"F1",           ///< The F1 key
			"F2",           ///< The F2 key
			"F3",           ///< The F3 key
			"F4",           ///< The F4 key
			"F5",           ///< The F5 key
			"F6",           ///< The F6 key
			"F7",           ///< The F7 key
			"F8",           ///< The F8 key
			"F9",           ///< The F9 key
			"F10",          ///< The F10 key
			"F11",          ///< The F11 key
			"F12",          ///< The F12 key
			"F13",          ///< The F13 key
			"F14",          ///< The F14 key
			"F15",          ///< The F15 key
			"Pause",        ///< The Pause key

			"KeyCount"      ///< Keep last -- the total number of keyboard keys
		};
		return names[static_cast<int>(key)];
	}

	struct MouseArgs
	{
		MouseButton button_id;
		unsigned x, y;
		float delta;
	};
	struct WindowArgs
	{
		unsigned windowId = 0;
		unsigned size_x = 0;
		unsigned size_y = 0;
		WindowArgs(unsigned window_id, unsigned sizeX, unsigned sizeY) : 
			windowId(window_id), size_x(sizeX), size_y(sizeY){}
		WindowArgs():
			windowId(0), size_x(0), size_y(0) {}
	};

	struct TouchArgs
	{
		unsigned int finger; ///< Index of the finger in case of multi-touch events
		int x;               ///< X position of the touch, relative to the left of the owner window
		int y;               ///< Y position of the touch, relative to the top of the owner window
	};

	struct TextArgs
	{
		unsigned unicode;
	};

	struct KeyArgs
	{
		enum KeyModifier
		{
			KM_CTRL = 1 << 0,		// Set if at least one Ctrl key is depressed.
			KM_SHIFT = 1 << 1,		// Set if at least one Shift key is depressed.
			KM_ALT = 1 << 2,		// Set if at least one Alt key is depressed.
			KM_META = 1 << 3,		// Set if at least one Meta key (the command key) is depressed.
			KM_CAPSLOCK = 1 << 4,	// Set if caps lock is enabled.
			KM_NUMLOCK = 1 << 5,	// Set if num lock is enabled.
			KM_SCROLLLOCK = 1 << 6	// Set if scroll lock is enabled.
		};
		Key key;
		unsigned key_modifier;
	};

}