///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: TouchSticksPanel.h
// Purpose: The touch screen's two sticks (InputMapper::touch_sticks) drawn by the game's interface:
//          an RmlUi document of the game's own (its look is the game's), whose elements are placed
//          each frame where the thumbs are. It takes no input: the fingers go to the InputMapper.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/view/InputMapper.h"
#include "topdown/view/UiPanel.h"

#include <string>

namespace Pakal::TopDown
{
	// The document has four elements, positioned absolutely by the panel (left, top, width, height):
	//   move_base, move_thumb, aim_base, aim_thumb
	// and its body gets the classes "moving" / "aiming" while a thumb is on a stick and "firing" while the aim stick fires.
	// Its body should have pointer-events: none (the fingers are the game's).
	class TouchSticksPanel
	{
	public:
		TouchSticksPanel(Pakal::IUIManager* ui, unsigned id) : m_panel(ui, id) {}

		// Loads the document ('body': its body's id) when the sticks are on, and unloads it when they are off
		void update(const InputMapper::TouchSticks& sticks, const std::string& document, const std::string& body);
		void unload() { m_panel.unload(); }

	private:
		void place(const std::string& element, float centerX, float centerY, float radius, float aspect);

		UiPanel m_panel;
	};
}
