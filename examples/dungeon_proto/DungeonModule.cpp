///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: DungeonModule.cpp
// Purpose: Game module of the dungeon prototype (PakalPlayer DungeonModule.dll): a dungeon of
//          Assets/dungeon/dungeon.json. It opens on the menu (alone, host, join); DUNGEON_PLAY, DUNGEON_AUTOPLAY
//          (a bot plays), DUNGEON_CONNECT or DUNGEON_SCREENSHOT go straight to the dungeon. DUNGEON_SEED: the
//          dungeon; DUNGEON_TIMESCALE: game seconds per real second.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "DungeonLevelState.h"
#include "DungeonMenuState.h"

#include "Engine.h"
#include "GameModuleAPI.h"
#include "GameStateManager.h"
#include "IPakalApplication.h"
#include "LogMgr.h"

#include <cstdlib>

namespace
{
	class DungeonApplication : public Pakal::IPakalApplication
	{
	public:
		const char* get_name() override { return "Dungeon prototype"; }

		void start(Pakal::Engine& engine) override
		{
			// the menu first, unless the run is already decided (a bot plays, a server is named, a capture is taken)
			const bool direct = std::getenv("DUNGEON_AUTOPLAY") || std::getenv("DUNGEON_CONNECT") || std::getenv("DUNGEON_SCREENSHOT") || std::getenv("DUNGEON_PLAY");
			if (direct) engine.game_state_manager()->push_state(new Dungeon::DungeonLevelState(), true);
			else engine.game_state_manager()->push_state(new Dungeon::DungeonMenuState(), true);
		}

		void end(Pakal::Engine& engine) override
		{
			(void)engine;
			LOG_INFO("[DungeonModule] ended");
		}
	};

	const char* get_module_name() { return "DungeonModule"; }
	Pakal::IPakalApplication* create_application() { return new DungeonApplication(); }
	void destroy_application(Pakal::IPakalApplication* application) { delete application; }
	void register_game_factories(void* engine) { (void)engine; }

	const Pakal::GameModuleExports g_dungeon_exports =
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
	return &g_dungeon_exports;
}
