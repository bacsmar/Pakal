///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: MenuState.h
// Purpose: The game outside the missions: the main menu, the mission list (in order, with their stars) and the shop
//          (guns, upgrades, health, boosts), on the RmlUi document Assets/ui/menu.rml. The campaign and the profile live in
//          the Session, shared with the missions.
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "BaseGameState.h"
#include "Meta.h"

#include <mutex>
#include <string>
#include <vector>

namespace Pakal { class IUIManager; class Engine; }

namespace Zombies
{
	// The campaign and the player's profile while the game runs. The profile is saved after every change.
	struct Session
	{
		Campaign campaign;
		Profile profile;
		bool campaignLoaded = false;

		static Session& get();      // loads the campaign and the profile the first time
		void save() const;
		// The options on the graphics (quality, FSR): at the start and after each change
		void apply_options(Pakal::Engine* engine) const;
		void apply_options_once(Pakal::Engine* engine) { if (!optionsApplied) { apply_options(engine); optionsApplied = true; } }
		bool optionsApplied = false;   // the graphics keep the options once given (the first menu or level gives them)
		// The options' rows (the menu's OPTIONS screen and the pause panel), and a click on one of them: applied and saved.
		// Returns false when the element is not an option.
		std::string options_markup() const;
		bool handle_option(const std::string& element, Pakal::Engine* engine);
		// A path from the working folder to a file of the game (the resource root on Android)
		static std::string resolve(const std::string& path);
	};

	class MenuState : public Pakal::BaseGameState
	{
	public:
		enum class Screen { Main, Missions, Shop, Heroes, Options };
		explicit MenuState(Screen screen = Screen::Main);

	protected:
		void on_initialize(Pakal::Engine* engine) override;
		void on_terminate(Pakal::Engine* engine) override;
		void on_update(unsigned long dtMilliseconds) override;
		void on_activate(Pakal::Engine* engine) override {}
		void on_deactivate(Pakal::Engine* engine) override {}

	private:
		void show(Screen screen);
		void fill_missions();
		void fill_shop();
		void fill_heroes();
		void handle(const std::string& element);

		Pakal::Engine* m_engine = nullptr;
		Pakal::IUIManager* m_ui = nullptr;
		// Debug: ZOMBIES_MENU_CLICKS=<id>,<id>,... clicks those elements one per half second, then ZOMBIES_MENU_SHOT=<file.tga>
		// captures the screen
		std::vector<std::string> m_scripted;
		float m_scriptClock = 0.0f;
		bool m_shotTaken = false;
		unsigned m_clicks = 0;
		Screen m_screen;
		std::string m_message;
		// Clicks come from the drawing thread; the updates act on them
		std::mutex m_mutex;
		std::vector<std::string> m_pending;
	};
}
