///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: DungeonMenuState.h
// Purpose: The dungeon's first screen: play alone, host a game for others on this network, or join one
//          of the games found on it (servers and hosting players answer LanBrowser: no address to type).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "BaseGameState.h"
#include "net/LanDiscovery.h"
#include "topdown/view/UiPanel.h"

#include <memory>
#include <string>
#include <vector>

namespace Pakal
{
	class Engine;
}

namespace Dungeon
{
	// This device's name, for the games it hosts (a phone's model, the computer's name)
	std::string device_name();

	class DungeonMenuState : public Pakal::BaseGameState
	{
	public:
		DungeonMenuState() : BaseGameState("DungeonMenu") {}

	protected:
		void on_initialize(Pakal::Engine* engine) override;
		void on_terminate(Pakal::Engine* engine) override;
		void on_update(unsigned long dtMilliseconds) override;
		void on_activate(Pakal::Engine*) override {}
		void on_deactivate(Pakal::Engine*) override {}

	private:
		void show_games();

		Pakal::Engine* m_engine = nullptr;
		std::unique_ptr<Pakal::TopDown::UiPanel> m_ui;
		Pakal::LanBrowser m_browser;
		double m_seconds = 0.0, m_lastSearch = -10.0;
		std::vector<Pakal::LanBrowser::Found> m_games;   // as listed (a button's number is its index)
		std::string m_shown = "-";
	};
}
