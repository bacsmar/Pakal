///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: UiPanel.h
// Purpose: A game's interface made of one RmlUi document (a HUD, an inventory, a menu): loaded and
//          shown, its elements filled by id (text, markup, a bar's share, a class on or off), and the
//          buttons pressed on it collected for the game to read each frame. Unloaded when it goes.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>

namespace Pakal
{
	class IUIManager;
}

namespace Pakal::TopDown
{
	class UiPanel
	{
	public:
		// 'id': the document's number for the interface (unique among the documents shown at once)
		UiPanel(Pakal::IUIManager* ui, unsigned id) : m_ui(ui), m_id(id) {}
		~UiPanel() { unload(); }
		UiPanel(const UiPanel&) = delete;
		UiPanel& operator=(const UiPanel&) = delete;

		// Loads and shows the document (a .rml file). On a touch screen its element 'body' (the body's id) gets the class
		// "touch" (to hide the keys' hints, make buttons bigger).
		bool load(const std::string& document, const std::string& body = std::string());
		void unload();
		bool loaded() const { return m_loaded; }

		void text(const std::string& element, const std::string& value);
		void markup(const std::string& element, const std::string& value);
		// A bar's fill: the element's width as this share (0..1) of its parent
		void bar(const std::string& element, float share);
		// One property of an element's style ("height", "40%")
		void property(const std::string& element, const std::string& name, const std::string& value);
		void set_class(const std::string& element, const std::string& name, bool on);
		void show(const std::string& element, bool shown) { set_class(element, "hidden", !shown); }
		void attribute(const std::string& element, const std::string& name, const std::string& value);

		// The ids of the buttons pressed since the last call (in order)
		std::vector<std::string> clicks();

	private:
		Pakal::IUIManager* m_ui;
		unsigned m_id;
		bool m_loaded = false;
		unsigned m_listener = 0;
		std::mutex m_mutex;
		std::vector<std::string> m_clicks;
		// What each element was last given (text, a property, a class...): the same again changes nothing, and RmlUi is
		// not made to lay the document out again for it (a HUD sets its values every frame)
		std::unordered_map<std::string, std::string> m_shown;
		bool changed(const std::string& key, const std::string& value);
	};
}
