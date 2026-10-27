///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: DungeonMenuState.cpp
// Purpose: The dungeon's first screen - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "DungeonMenuState.h"
#include "DungeonLevelState.h"
#include "DungeonNet.h"

#include "Engine.h"
#include "GameStateManager.h"
#include "IInputManager.h"
#include "IUIManager.h"
#include "LogMgr.h"
#include "OSManager.h"

#include <cstdlib>

#if defined(__ANDROID__)
	#include <sys/system_properties.h>
#endif

using namespace Pakal;
using namespace Pakal::TopDown;

namespace Dungeon
{
	namespace
	{
		const char* kMenuFile = "Assets/dungeon/ui/menu.rml";
		const unsigned kMenuDocument = 12;

		std::string escape(const std::string& text)
		{
			std::string out;
			for (char c : text)
			{
				if (c == '<') out += "&lt;";
				else if (c == '>') out += "&gt;";
				else if (c == '&') out += "&amp;";
				else out += c;
			}
			return out;
		}
	}

	std::string device_name()
	{
		if (const char* name = std::getenv("DUNGEON_NAME")) return name;
#if defined(__ANDROID__)
		char model[PROP_VALUE_MAX] = {};
		if (__system_property_get("ro.product.model", model) > 0) return model;
		return "phone";
#else
		const char* computer = std::getenv("COMPUTERNAME");
		return computer ? computer : "computer";
#endif
	}

	void DungeonMenuState::on_initialize(Engine* engine)
	{
		m_engine = engine;
		if (IInputManager* input = engine->input_manager()) input->set_touch_controls(false);
		if (IUIManager* ui = engine->get_ui_manager())
		{
			for (const char* font : { "Assets/ui/fonts/Roboto-Regular.ttf", "Assets/ui/fonts/Roboto-Bold.ttf", "Assets/ui/fonts/Bangers-Regular.ttf" }) ui->load_font(font);
			m_ui = std::make_unique<UiPanel>(ui, kMenuDocument);
			m_ui->load(kMenuFile, "dungeon_menu");
			m_ui->text("m_device", "this device: " + escape(device_name()));
		}
		if (!m_browser.open(kLanGame)) LOG_WARNING("[DungeonMenu] cannot look for games on the network");
		show_games();
	}

	void DungeonMenuState::on_terminate(Engine* engine)
	{
		(void)engine;
		m_browser.close();
		m_ui.reset();
	}

	void DungeonMenuState::on_update(unsigned long dtMilliseconds)
	{
		m_seconds += static_cast<double>(dtMilliseconds) / 1000.0;
		// the network is asked every second; the list shows who answered in the last few
		if (m_seconds - m_lastSearch >= 1.0)
		{
			m_lastSearch = m_seconds;
			m_browser.search(m_seconds);
		}
		m_browser.poll(m_seconds);
		show_games();
		if (!m_ui) return;
		for (const auto& clicked : m_ui->clicks())
		{
			DungeonLevelState::Session session;
			if (clicked == "m_alone") session.mode = DungeonLevelState::Session::Mode::Alone;
			else if (clicked == "m_host") session.mode = DungeonLevelState::Session::Mode::Host;
			else if (clicked.rfind("game_", 0) == 0)
			{
				const size_t index = static_cast<size_t>(std::atoi(clicked.c_str() + 5));
				if (index >= m_games.size()) continue;
				session.mode = DungeonLevelState::Session::Mode::Join;
				session.host = m_games[index].host;
				session.port = m_games[index].port;
			}
			else continue;
			get_manager()->transition_to_state(new DungeonLevelState(0, session), true);
			return;   // this state is gone
		}
	}

	// A button for each game found; rebuilt only when the list changes
	void DungeonMenuState::show_games()
	{
		if (!m_ui) return;
		m_games = m_browser.found();
		std::string markup;
		for (size_t i = 0; i < m_games.size(); ++i)
		{
			markup += "<div id=\"game_" + std::to_string(i) + "\" class=\"button m_game\">JOIN " + escape(m_games[i].info)
				+ "<span class=\"m_address\">" + m_games[i].host + ":" + std::to_string(m_games[i].port) + "</span></div>";
		}
		if (markup.empty()) markup = "<div class=\"m_none\">looking for games on this network...</div>";
		if (markup == m_shown) return;
		m_shown = markup;
		m_ui->markup("m_games", markup);
	}
}
