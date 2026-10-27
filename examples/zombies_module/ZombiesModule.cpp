///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ZombiesModule.cpp
// Purpose: Game module that runs a converted Zombies level inside PakalPlayer.
//          Level path: ZOMBIES_LEVEL (environment) or Assets/zombies/hollywood_s01t01/level.json
//          relative to the module's folder.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "GameModuleAPI.h"
#include "GameStateManager.h"
#include "IUIManager.h"
#include "IPakalApplication.h"
#include "LogMgr.h"
#include "ResourceManager.h"
#include "ZombiesLevelState.h"
#include "MenuState.h"
#include "CharacterViewer.h"

#include <cstdlib>
#include <string>

namespace
{

	class ZombiesApplication : public Pakal::IPakalApplication
	{
	public:
		const char* get_name() override
		{
			return "Zombies Module";
		}

		void start(Pakal::Engine& engine) override
		{
			// ZOMBIES_LEVEL (a debug switch) plays that level on its own; otherwise the game opens on its menu
			const char* override = std::getenv("ZOMBIES_LEVEL");

			// The interface's fonts (Assets/ui/fonts): once for every document
			if (Pakal::IUIManager* ui = engine.get_ui_manager())
			{
				for (const char* font : { "Assets/ui/fonts/Roboto-Regular.ttf", "Assets/ui/fonts/Roboto-Bold.ttf", "Assets/ui/fonts/Bangers-Regular.ttf" })
				{
					ui->load_font(font);
				}
			}
			// ZOMBIES_VIEWER=heroes|enemies opens the character viewer or the zombie encyclopedia
			const char* viewer = std::getenv("ZOMBIES_VIEWER");
			if (viewer && *viewer)
			{
				const bool enemies = std::string(viewer) == "enemies";
				engine.game_state_manager()->push_state(new Zombies::CharacterViewerState(
					enemies ? Zombies::CharacterViewerState::Mode::Enemies : Zombies::CharacterViewerState::Mode::Heroes), true);
			}
			else if (override && *override)
			{
				LOG_INFO("[ZombiesModule] starting level %s", override);
				engine.game_state_manager()->push_state(new Zombies::ZombiesLevelState(override), true);
			}
			else
			{
				engine.game_state_manager()->push_state(new Zombies::MenuState(), true);
			}
		}

		void end(Pakal::Engine& engine) override
		{
			(void)engine;
			LOG_INFO("[ZombiesModule] level ended");
		}
	};

	const char* get_module_name()
	{
		return "ZombiesModule";
	}

	Pakal::IPakalApplication* create_application()
	{
		return new ZombiesApplication();
	}

	void destroy_application(Pakal::IPakalApplication* application)
	{
		delete application;
	}

	void register_game_factories(void* engine)
	{
		(void)engine;
	}

	const Pakal::GameModuleExports g_zombies_exports =
	{
		Pakal::PAKAL_GAME_MODULE_API_VERSION,
		&get_module_name,
		&create_application,
		&destroy_application,
		&register_game_factories
	};
}

PAKAL_GAME_MODULE_EXPORT const Pakal::GameModuleExports* pakal_get_game_module_exports()
{
	return &g_zombies_exports;
}
