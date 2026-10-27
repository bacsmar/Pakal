///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: InputMapper.cpp
// Purpose: The player's input for each step - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/InputMapper.h"
#include "topdown/CameraInput.h"
#include "topdown/MouseAim.h"

#include "EventArgs.h"
#include "IInputManager.h"
#include "OSManager.h"

#include <algorithm>

namespace Pakal::TopDown
{
	void InputMapper::listen(Pakal::IInputManager* input, Pakal::OSManager* os, std::function<bool(int x, int y)> clickTaken)
	{
		m_input = input;
		if (!input)
		{
			return;
		}
		if (os)
		{
			m_os = os;
			m_focusId = os->event_window_focused.add_listener([this](const bool& focused)
			{
				m_focused = focused;
				if (!focused) m_mouse.left = false;
			});
		}
		m_mouseMovedId = input->event_mouse_moved.add_listener([this](const MouseArgs& args)
		{
			m_mouse.x = static_cast<int>(args.x);
			m_mouse.y = static_cast<int>(args.y);
			m_mouse.seen = true;
		});
		m_mousePressedId = input->event_mouse_pressed.add_listener([this, clickTaken](const MouseArgs& args)
		{
			const bool taken = clickTaken && clickTaken(int(args.x), int(args.y));
			if (args.button_id == MouseButton::Left && !taken) m_mouse.left = true;
		});
		m_mouseReleasedId = input->event_mouse_released.add_listener([this](const MouseArgs& args)
		{
			if (args.button_id == MouseButton::Left) m_mouse.left = false;
		});
	}

	void InputMapper::stop()
	{
		if (m_focusId != 0 && m_os)
		{
			m_os->event_window_focused.remove_listener(m_focusId);
			m_focusId = 0;
		}
		if (!m_input || m_mouseMovedId == 0)
		{
			return;
		}
		m_input->event_mouse_moved.remove_listener(m_mouseMovedId);
		m_input->event_mouse_pressed.remove_listener(m_mousePressedId);
		m_input->event_mouse_released.remove_listener(m_mouseReleasedId);
		m_mouseMovedId = m_mousePressedId = m_mouseReleasedId = 0;
	}

	bool InputMapper::key_down(Pakal::Key key) const
	{
		return !m_settings.ignoreKeys && m_input && m_focused && m_input->is_key_pressed(key);
	}

	void InputMapper::tap(int bit, int reads)
	{
		m_taps.emplace_back(bit, reads);
	}

	b2Vec2 InputMapper::camera_relative(const b2Vec2& v) const
	{
		float x, y;
		Pakal::TopDown::camera_relative(v.x, v.y, m_settings.cameraAngle, x, y);
		return b2Vec2(x, y);
	}

	bool InputMapper::mouse_aim(const CameraRig& camera, float width, float height, b2Vec2& at) const
	{
		if (m_keyboardAim || !m_mouse.seen || camera.panning())
		{
			return false;
		}
		float ax = 0.0f, ay = 0.0f;
		if (!mouse_on_plane(camera.eye(), camera.target(), camera.fov_y(), static_cast<float>(m_mouse.x.load()), static_cast<float>(m_mouse.y.load()),
			width, height, m_settings.aimPlaneHeight, ax, ay))
		{
			return false;
		}
		at = b2Vec2(ax, ay);
		return true;
	}

	PlayerInput InputMapper::read(const CameraRig& camera, float width, float height)
	{
		PlayerInput input;
		if (!m_input)
		{
			return input;
		}
		float mx = 0.0f, my = 0.0f;
		if (key_down(Key::A) || key_down(Key::Left)) mx -= 1.0f;
		if (key_down(Key::D) || key_down(Key::Right)) mx += 1.0f;
		if (key_down(Key::W) || key_down(Key::Up)) my += 1.0f;
		if (key_down(Key::S) || key_down(Key::Down)) my -= 1.0f;
		input.move = camera_relative(b2Vec2(mx, my));

		// Aim: the keys IJKL take over until the mouse moves again
		b2Vec2 aim(0.0f, 0.0f);
		if (m_settings.aimKeys)
		{
			if (key_down(Key::J)) aim.x -= 1.0f;
			if (key_down(Key::L)) aim.x += 1.0f;
			if (key_down(Key::I)) aim.y += 1.0f;
			if (key_down(Key::K)) aim.y -= 1.0f;
		}
		if (aim.Normalize() > 0.0f)
		{
			input.aimDirection = camera_relative(aim);
			m_keyboardAim = true;
		}
		const int x = m_mouse.x, y = m_mouse.y;
		if (x != m_lastMouseX || y != m_lastMouseY)
		{
			if (m_lastMouseX >= 0) m_keyboardAim = false;
			m_lastMouseX = x;
			m_lastMouseY = y;
		}
		b2Vec2 aimPoint;
		if (mouse_aim(camera, width, height, aimPoint))
		{
			input.hasAimPoint = true;
			input.aimPoint = aimPoint;
		}
		input.fire = key_down(Key::Space) || m_mouse.left;
		// the game's buttons, and the ones that went down since the last read
		for (const auto& [key, bit] : m_settings.buttons)
		{
			if (key_down(key)) input.held |= 1u << bit;
		}
		for (auto& [bit, reads] : m_taps)
		{
			input.held |= 1u << bit;
			--reads;
		}
		m_taps.erase(std::remove_if(m_taps.begin(), m_taps.end(), [](const auto& tapped) { return tapped.second <= 0; }), m_taps.end());
		input.pressed = input.held & ~m_lastHeld;
		m_lastHeld = input.held;

		// Sticks (the touch screen's, a gamepad's): axes 0-1 move, 2-3 aim (y up). Aiming past a third of the way fires.
		if (Pakal::IAxisDevice* sticks = m_input->get_axis_device(0))
		{
			const b2Vec2 move(sticks->get_axis_state(0), sticks->get_axis_state(1));
			if (move.LengthSquared() > 0.0f)
			{
				input.move = camera_relative(move);
			}
			b2Vec2 stick(sticks->get_axis_state(2), sticks->get_axis_state(3));
			const float push = stick.Length();
			if (push > m_settings.stickAimMinimum)
			{
				stick *= 1.0f / push;
				input.aimDirection = camera_relative(stick);
				input.hasAimPoint = false;
				input.fire = input.fire || push > m_settings.stickFireThreshold;
			}
		}
		return input;
	}
}
