///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ContraGame.cpp
// Purpose: Main application class for Contra game - Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ContraGame.h"
#include "Engine.h"
#include "ComponentManager.h"
#include "GameStateManager.h"
#include "LogMgr.h"

// Game states
#include "GameStates/GamePlayState.h"

// Game components
#include "Components/Health.h"
#include "Components/Weapon.h"
#include "Components/PlayerController.h"
#include "Components/EnemyAI.h"

// Engine components
#include "bgfx/SpriteComponent_Bgfx.h"
#include "bgfx/CameraComponent_Bgfx.h"

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
		
		// Register rendering components
		componentMgr->register_component<SpriteComponent_Bgfx>("SpriteComponent_Bgfx");
		componentMgr->register_component<CameraComponent_Bgfx>("CameraComponent_Bgfx");
		
		// Register game components
		componentMgr->register_component<Health>("Health");
		componentMgr->register_component<Weapon>("Weapon");
		componentMgr->register_component<PlayerController>("PlayerController");
		componentMgr->register_component<EnemyAI>("EnemyAI");
	}
	
	void ContraGame::setup_game_states(Engine& engine)
	{
		LOG_INFO("[ContraGame] Setting up game states");
		
		auto* stateMgr = engine.game_state_manager();
		
		// Create and push gameplay state
		auto* gameplayState = new GamePlayState();
		stateMgr->push_state(gameplayState, true); // true = auto-delete on pop
	}
}
