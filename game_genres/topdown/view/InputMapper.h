///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: InputMapper.h
// Purpose: The player's input for each step (PlayerInput) from the keyboard, the mouse and the sticks
//          (a gamepad's, the touch screen's). WASD and the sticks are relative to the camera; the mouse
//          aims where its ray meets the floor at chest height; IJKL aim until the mouse moves again.
//          Mouse and focus events arrive on the window's thread.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/TopDownGame.h"
#include "topdown/view/CameraRig.h"
#include "EventArgs.h"
#include "InputDevice.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

namespace Pakal
{
	class IInputManager;
	class OSManager;
}

namespace Pakal::TopDown
{
	class InputMapper
	{
	public:
		struct Settings
		{
			float cameraAngle = 0.0f;         // camera_input_angle() of the camera's offset: "up" on the screen
			float stickAimMinimum = 0.2f;     // aim stick: below this the player keeps facing where it walks
			float stickFireThreshold = 0.41f; // aim stick: past this it fires
			float aimPlaneHeight = 0.9f;      // the mouse aims at chest height, where the bodies are
			bool ignoreKeys = false;          // a bot plays: the keys do nothing
			bool aimKeys = true;              // IJKL aim (off: those keys are free for a game's buttons)
			// The keys of a game's buttons: each one holds that bit of PlayerInput::held (and of pressed, the step it goes down)
			std::vector<std::pair<Pakal::Key, int>> buttons;
		};

		explicit InputMapper(const Settings& settings) : m_settings(settings) {}
		~InputMapper() { stop(); }
		InputMapper(const InputMapper&) = delete;
		InputMapper& operator=(const InputMapper&) = delete;

		// Listens to the mouse and the window's focus. 'clickTaken' says when a press at (x, y) is for a panel (the
		// development panel, a button of the interface), not the game: called on the window's thread.
		void listen(Pakal::IInputManager* input, Pakal::OSManager* os, std::function<bool(int x, int y)> clickTaken);
		void stop();

		bool key_down(Pakal::Key key) const;
		// A game's button pressed from its interface (a touch screen's button): held for the next 'reads' reads
		void tap(int bit, int reads = 6);
		bool mouse_left() const { return m_mouse.left; }

		// This step's input; the camera is the drawn one (width and height: the screen in pixels)
		PlayerInput read(const CameraRig& camera, float width, float height);
		// Where the mouse aims on the floor (false: it is not aiming, or misses the floor)
		bool mouse_aim(const CameraRig& camera, float width, float height, b2Vec2& at) const;
		float aim_height() const { return m_settings.aimPlaneHeight; }

	private:
		b2Vec2 camera_relative(const b2Vec2& v) const;

		struct Mouse
		{
			std::atomic<int> x{ 0 }, y{ 0 };
			std::atomic<bool> seen{ false };
			std::atomic<bool> left{ false };
		};

		Settings m_settings;
		Mouse m_mouse;
		Pakal::IInputManager* m_input = nullptr;
		Pakal::OSManager* m_os = nullptr;
		std::atomic<bool> m_focused{ true };   // the window has the focus
		unsigned m_focusId = 0;
		unsigned m_mouseMovedId = 0, m_mousePressedId = 0, m_mouseReleasedId = 0; // Pakal::EventId
		bool m_keyboardAim = false;   // IJKL were used last (the mouse takes over again when it moves)
		int m_lastMouseX = -1, m_lastMouseY = -1;
		uint32_t m_lastHeld = 0;      // the buttons at the last read (what goes down is 'pressed')
		std::vector<std::pair<int, int>> m_taps;   // a button tapped on the interface, and the reads it is still held
	};
}
