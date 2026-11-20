///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: UiNav.h
// Purpose: The keys a game reads (and the gamepad's inputs it bound to them, set_pad_bindings) through a
//          document's buttons: the arrows move the focus (IUIManager::navigate) and Return presses the
//          focused button (activate). Held, an arrow repeats after a moment. What was held when the
//          document started taking the keys waits to be let go. Once the keys or a pad have been used, every
//          document keeps one button lit (IUIManager::focus_first): a pad's A always presses something.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "IInputManager.h"
#include "IUIManager.h"

namespace Pakal
{
	class UiNav
	{
	public:
		// Each frame the document takes the keys ('seconds': the frame's). A frame it does not, call reset() instead.
		void update(IInputManager* input, IUIManager* ui, unsigned document, float seconds)
		{
			if (!input || !ui) return;
			static constexpr Key keys[5] = { Key::Up, Key::Down, Key::Left, Key::Right, Key::Return };
			static constexpr IUIManager::NavDirection ways[4] = { IUIManager::NavDirection::Up, IUIManager::NavDirection::Down,
				IUIManager::NavDirection::Left, IUIManager::NavDirection::Right };
			bool now[5];
			bool any = false;
			for (int i = 0; i < 5; ++i) any |= now[i] = input->is_key_pressed(keys[i]);
			// the first time the keys or a pad move through a menu, a button lights up (that press only lights it); from then
			// on every document keeps one lit. A finger alone never lights one
			bool first = false;
			if (any && !s_used)
			{
				s_used = true;
				first = true;
			}
			if (s_used) ui->focus_first(document);
			if (!m_armed)
			{
				m_armed = !any;
				for (int i = 0; i < 5; ++i) m_was[i] = now[i];
				return;
			}
			if (first)
			{
				for (int i = 0; i < 5; ++i) m_was[i] = now[i];
				return;
			}
			for (int i = 0; i < 4; ++i)
			{
				if (now[i] && !m_was[i])
				{
					ui->navigate(document, ways[i]);
					m_repeat = kRepeatDelay;
				}
				else if (now[i])
				{
					m_repeat -= seconds;
					if (m_repeat <= 0.0f)
					{
						ui->navigate(document, ways[i]);
						m_repeat = kRepeatEvery;
					}
				}
			}
			if (now[4] && !m_was[4]) ui->activate(document);
			for (int i = 0; i < 5; ++i) m_was[i] = now[i];
		}
		void reset() { m_armed = false; }

	private:
		static constexpr float kRepeatDelay = 0.45f, kRepeatEvery = 0.15f;
		static inline bool s_used = false;   // the keys or a pad have moved through a menu (every document then keeps a button lit)
		bool m_was[5] = {};
		bool m_armed = false;
		float m_repeat = 0.0f;
	};
}
