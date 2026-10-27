///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BrawlerModule.cpp
// Purpose: Game module of the brawler prototype (PakalPlayer BrawlerModule.dll): one arena of
//          Assets/brawler/brawler.json. BRAWLER_AUTOPLAY: a bot fights; BRAWLER_SEED: the seed.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "BrawlerLevelState.h"

#include "Engine.h"
#include "GameModuleAPI.h"
#include "GameStateManager.h"
#include "IPakalApplication.h"
#include "LogMgr.h"

namespace
{
	class BrawlerApplication : public Pakal::IPakalApplication
	{
	public:
		const char* get_name() override { return "Brawler prototype"; }

		void start(Pakal::Engine& engine) override
		{
			engine.game_state_manager()->push_state(new Brawler::BrawlerLevelState(), true);
		}

		void end(Pakal::Engine& engine) override
		{
			(void)engine;
			LOG_INFO("[BrawlerModule] ended");
		}
	};

	const char* get_module_name() { return "BrawlerModule"; }
	Pakal::IPakalApplication* create_application() { return new BrawlerApplication(); }
	void destroy_application(Pakal::IPakalApplication* application) { delete application; }
	void register_game_factories(void* engine) { (void)engine; }

	const Pakal::GameModuleExports g_brawler_exports =
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
	return &g_brawler_exports;
}
