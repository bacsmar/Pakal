///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: TouchSticksPanel.cpp
// Purpose: The touch screen's two sticks drawn by the game's interface - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/TouchSticksPanel.h"

#include <cstdio>

namespace Pakal::TopDown
{
	void TouchSticksPanel::update(const InputMapper::TouchSticks& sticks, const std::string& document, const std::string& body)
	{
		if (!sticks.on)
		{
			m_panel.unload();
			return;
		}
		if (!m_panel.loaded() && !m_panel.load(document, body)) return;
		const float thumb = sticks.radius * 0.4f;
		place("move_base", sticks.move.baseX, sticks.move.baseY, sticks.radius, sticks.aspect);
		place("move_thumb", sticks.move.thumbX, sticks.move.thumbY, thumb, sticks.aspect);
		m_panel.show("aim_base", sticks.aims);
		m_panel.show("aim_thumb", sticks.aims);
		if (sticks.aims)
		{
			place("aim_base", sticks.aim.baseX, sticks.aim.baseY, sticks.radius, sticks.aspect);
			place("aim_thumb", sticks.aim.thumbX, sticks.aim.thumbY, thumb, sticks.aspect);
		}
		m_panel.set_class(body, "moving", sticks.move.held);
		m_panel.set_class(body, "aiming", sticks.aim.held);
		m_panel.set_class(body, "firing", sticks.firing);
	}

	// A circle centered on a point of the screen (shares, y down), its radius a share of the screen's height: in percent
	// of the body (the whole screen), so it holds at any resolution
	void TouchSticksPanel::place(const std::string& element, float centerX, float centerY, float radius, float aspect)
	{
		char value[24];
		std::snprintf(value, sizeof(value), "%.2f%%", 100.0f * (centerX - radius / aspect));
		m_panel.property(element, "left", value);
		std::snprintf(value, sizeof(value), "%.2f%%", 100.0f * (centerY - radius));
		m_panel.property(element, "top", value);
		std::snprintf(value, sizeof(value), "%.2f%%", 200.0f * radius / aspect);
		m_panel.property(element, "width", value);
		std::snprintf(value, sizeof(value), "%.2f%%", 200.0f * radius);
		m_panel.property(element, "height", value);
	}
}
