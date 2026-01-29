///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ContraGame.cpp
// Creation Date: January 2026
//
// Purpose: Contra-style run & gun game application implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "ContraGame.h"
#include "Engine.h"
#include "GameStateManager.h"
#include "ComponentManager.h"
#include "ResourceManager.h"
#include "LogMgr.h"

#include "GameState/MenuState.h"
#include "GameState/GamePlayState.h"
#include "GameState/GameOverState.h"

#include "Components/Health.h"
#include "Components/Weapon.h"
#include "Components/PlayerController.h"
#include "Components/EnemyAI.h"

using namespace Pakal;

ContraGame::ContraGame()
{
}

void ContraGame::start(Engine& engine)
{
	LOG_INFO("[ContraGame] Starting Contra Game Example");

	// Register custom components
	register_components(engine);

	// Register game states
	register_game_states(engine);

	// Load game resources
	load_resources(engine);

	// Start with menu state
	auto gameStateManager = engine.game_state_manager();
	if (gameStateManager)
	{
		// TODO: Push menu state
		// gameStateManager->push_state("Menu");
		
		// For now, start directly with gameplay for testing
		// gameStateManager->push_state("GamePlay");
	}

	LOG_INFO("[ContraGame] Game started successfully");
}

void ContraGame::end(Engine& engine)
{
	LOG_INFO("[ContraGame] Shutting down Contra Game Example");

	// Cleanup is handled by engine
}

void ContraGame::register_components(Engine& engine)
{
	LOG_INFO("[ContraGame] Registering game components");

	auto componentManager = engine.component_manager();
	if (!componentManager)
		return;

	// TODO: Register custom component factories
	// This allows components to be created dynamically
	
	// Example:
	// componentManager->register_component<Health>();
	// componentManager->register_component<Weapon>();
	// componentManager->register_component<Projectile>();
	// componentManager->register_component<PlayerController>();
	// componentManager->register_component<EnemyAI>();
}

void ContraGame::register_game_states(Engine& engine)
{
	LOG_INFO("[ContraGame] Registering game states");

	auto gameStateManager = engine.game_state_manager();
	if (!gameStateManager)
		return;

	// TODO: Register game states with the state manager
	// This allows states to be referenced by name
	
	// Example:
	// gameStateManager->register_state("Menu", new MenuState());
	// gameStateManager->register_state("GamePlay", new GamePlayState());
	// gameStateManager->register_state("GameOver", new GameOverState());
}

void ContraGame::load_resources(Engine& engine)
{
	LOG_INFO("[ContraGame] Loading game resources");

	auto resourceManager = engine.resource_manager();
	if (!resourceManager)
		return;

	// TODO: Load game resources
	// - Sprite sheets
	// - Textures
	// - Sounds
	// - Level data

	// Example:
	// resourceManager->add_source("Assets");
	// resourceManager->load_texture("player.png");
	// resourceManager->load_texture("enemy_soldier.png");
	// resourceManager->load_texture("enemy_turret.png");
	// resourceManager->load_sound("shoot.wav");
	// resourceManager->load_sound("jump.wav");
	// resourceManager->load_sound("hit.wav");

	LOG_INFO("[ContraGame] Resources loaded");
}
