///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ContraGame.cpp
// Purpose: Main application class for Contra game - Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ContraGame.h"
#include "Engine.h"
#include "ComponentManager.h"
#include "ComponentFactory.h"
#include "GameStateManager.h"
#include "LogMgr.h"

// Game states
#include "GameStates/GamePlayState.h"
#include "GameStates/GameTitleState.h"

// Game components
#include "Components/Health.h"
#include "Components/Weapon.h"
#include "Components/PlayerController.h"
#include "Components/EnemyAI.h"
#include "Components/Projectile.h"
#include "Components/ParallaxController.h"

namespace Pakal
{
	void ContraGame::start(Engine& engine)
	{
		LOG_INFO("[ContraGame] Starting Contra game example");
		
		// Register custom components
		register_components(engine);
		
		// Setup game states
		setup_game_states(engine);
		
		LOG_INFO("[ContraGame] Contra game started successfully");
	}
	
	void ContraGame::end(Engine& engine)
	{
		LOG_INFO("[ContraGame] Ending Contra game example");
	}
	
	void ContraGame::register_components(Engine& engine)
	{
		LOG_INFO("[ContraGame] Registering game components");

		auto* componentMgr = engine.component_manager();
		if (!componentMgr)
		{
			LOG_ERROR("[ContraGame] ComponentManager is null; cannot register game components");
			return;
		}

		// Mount this module's Assets directory so the ResourceManager
		// resolves Assets/... paths regardless of working directory.
		const std::string assetsDir = ResourceMgr.get_root_path() + "/Assets";
		ResourceMgr.add_source<Pakal::DirectorySource>(assetsDir.c_str(), true);

		componentMgr->register_factory(CreateComponentFactory<Health, Health>());
		componentMgr->register_factory(CreateComponentFactory<Weapon, Weapon>());
		componentMgr->register_factory(CreateComponentFactory<PlayerController, PlayerController>());
		componentMgr->register_factory(CreateComponentFactory<EnemyAI, EnemyAI>());
		componentMgr->register_factory(CreateComponentFactory<Projectile, Projectile>());
		componentMgr->register_factory(CreateComponentFactory<ParallaxController, ParallaxController>());
	}
	
	void ContraGame::setup_game_states(Engine& engine)
	{
		LOG_INFO("[ContraGame] Setting up game states");
		
		auto* stateMgr = engine.game_state_manager();
		
		// Create and push title state
		auto* titleState = new GameTitleState();
		stateMgr->push_state(titleState, true); // true = auto-delete on pop
	}
}
