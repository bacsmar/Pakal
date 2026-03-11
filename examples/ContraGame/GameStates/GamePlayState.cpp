///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GamePlayState.cpp
// Purpose: Main gameplay state for Contra game - Scene-based implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "GamePlayState.h"
#include "GameTitleState.h"
#include "GenericEntity.h"
#include "ComponentManager.h"
#include "Engine.h"
#include "GameStateManager.h"
#include "EntityManager.h"
#include "LogMgr.h"
#include "resources/SceneLoader.h"
#include <algorithm>

// Game components
#include "../Components/Health.h"
#include "../Components/Weapon.h"
#include "../Components/PlayerController.h"
#include "../Components/EnemyAI.h"

// Engine components  
#include "components/SpriteComponent2D.h"
#include "components/CameraComponent2D.h"
#include "InputManager_Polling.h"
#include "components/SpritePhysicsComponent.h"

namespace Pakal
{

	GamePlayState::GamePlayState() : BaseGameState("GamePlay"),
		m_engine(nullptr),
		m_player(nullptr),
		m_goal(nullptr),
		m_camera(nullptr),
		m_currentLevel(1),
		m_maxLevels(2),
		m_levelTransitionTimer(0.0f),
		m_levelCompleted(false),
		m_returningToMenu(false),
		m_score(0),
		m_lives(3),
		m_enemiesKilled(0),
		m_totalEnemies(0),
		m_gameWon(false),
		m_gameLost(false)
	{
	}
	
	GamePlayState::~GamePlayState()
	{
	}
	
	void GamePlayState::on_initialize(Engine* engine)
	{
		LOG_INFO("[GamePlayState] Initializing gameplay state");
		m_engine = engine;

		m_currentLevel = 1;
		m_levelTransitionTimer = 0.0f;
		m_levelCompleted = false;
		m_returningToMenu = false;
		m_gameWon = false;
		m_gameLost = false;

		// Initialize SceneLoader
		m_sceneLoader = std::make_unique<SceneLoader>(engine);
		if (!m_sceneLoader) {
			LOG_ERROR("[GamePlayState] Failed to create SceneLoader");
			return;
		}

		load_level(m_currentLevel);
		
		LOG_INFO("[GamePlayState] Gameplay state initialized");
	}

	void GamePlayState::on_terminate(Engine* engine)
	{
		LOG_INFO("[GamePlayState] Terminating gameplay state");
		cleanup_level_entities();
		m_sceneLoader.reset();
	}

	void GamePlayState::on_activate(Engine* engine)
	{
		LOG_INFO("[GamePlayState] Activating game state");
	}

	void GamePlayState::on_deactivate(Engine* engine)
	{
		LOG_INFO("[GamePlayState] Deactivating game state");
	}
	
	void GamePlayState::on_update(unsigned long dtMilliseconds)
	{
		if (!m_engine) return;

		float deltaTime = dtMilliseconds / 1000.0f;
		update_game_logic(deltaTime);
		check_win_lose_conditions();

		// Handle level transitions
		if (m_levelCompleted && m_levelTransitionTimer >= 2.0f)
		{
			if (m_currentLevel < m_maxLevels)
			{
				++m_currentLevel;
				LOG_INFO("[GamePlayState] Loading level %d", m_currentLevel);
				load_level(m_currentLevel);
			}
			else
			{
				LOG_INFO("[GamePlayState] All levels completed! Returning to title.");
				auto* manager = get_manager();
				if (manager)
				{
					manager->transition_to_state(new GameTitleState(), true);
				}
			}
		}

		if (m_gameLost)
		{
			auto* manager = get_manager();
			if (manager)
			{
				LOG_INFO("[GamePlayState] Game over. Returning to title.");
				manager->transition_to_state(new GameTitleState(), true);
			}
		}

		// Update transition timer
		if (m_levelCompleted)
		{
			m_levelTransitionTimer += deltaTime;
		}
	}

	void GamePlayState::load_level(int levelNumber)
	{
		LOG_INFO("[GamePlayState] Loading level %d", levelNumber);
		cleanup_level_entities();

		m_enemies.clear();
		m_player = nullptr;
		m_goal = nullptr;
		m_camera = nullptr;
		m_levelCompleted = false;
		m_levelTransitionTimer = 0.0f;
		m_gameWon = false;
		m_gameLost = false;
		m_enemiesKilled = 0;

		// Load scene from JSON
		std::string sceneFile = "Assets/scenes/level" + std::to_string(levelNumber) + ".json";
		if (!m_sceneLoader->load_scene(sceneFile)) {
			LOG_ERROR("[GamePlayState] Failed to load scene: %s", sceneFile.c_str());
			return;
		}

		setup_loaded_scene();
	}

	void GamePlayState::setup_loaded_scene()
	{
		LOG_INFO("[GamePlayState] Setting up loaded scene");

		// Validate required entities exist
		std::vector<std::string> required = {"player", "goal", "camera"};
		if (!m_sceneLoader->validate_required_entities(required)) {
			LOG_ERROR("[GamePlayState] Scene validation failed - missing required entities");
			return;
		}

		// Get required entities
		m_player = m_sceneLoader->get_entity("player");
		m_goal = m_sceneLoader->get_entity("goal");
		m_camera = m_sceneLoader->get_entity("camera");

		if (!m_player || !m_goal || !m_camera) {
			LOG_ERROR("[GamePlayState] Failed to retrieve required entities from scene");
			return;
		}

		// Register all loaded entities as level entities
		for (auto entity : m_sceneLoader->get_loaded_entities()) {
			register_level_entity(entity);
		}

		// Collect enemies from loaded entities (entities starting with "enemy_")
		m_totalEnemies = 0;
		for (const auto& loadedEntity : m_sceneLoader->get_loaded_entities()) {
			std::string descriptor = loadedEntity->get_descriptor();
			if (descriptor.find("enemy") != std::string::npos) {
				m_enemies.push_back(loadedEntity);
				m_totalEnemies++;
			}
		}

		LOG_INFO("[GamePlayState] Scene setup complete: %d enemies configured", m_totalEnemies);

		// Setup entity-specific game components
		setup_entity_components();

		// Configure camera to follow player
		auto* cameraComp = m_camera->get_component<CameraComponent2D>();
		if (cameraComp && m_player) {
			cameraComp->follow_target(m_player, 0.1f);
		}
	}

	void GamePlayState::setup_entity_components()
	{
		LOG_INFO("[GamePlayState] Setting up entity components");

		auto* entityMgr = m_engine->entity_manager();

		// Setup player components
		if (m_player) {
			if (!m_player->get_component<Health>()) {
				auto* health = m_player->create_component<Health>();
				health->set_max_health(100.0f);
			}

			if (!m_player->get_component<Weapon>()) {
				auto* weapon = m_player->create_component<Weapon>();
				weapon->set_fire_rate(0.2f);
				weapon->set_projectile_speed(15.0f);
				weapon->set_entity_manager(entityMgr);
			}

			if (!m_player->get_component<PlayerController>()) {
				auto* controller = m_player->create_component<PlayerController>();
				controller->set_move_speed(5.0f);
				controller->set_jump_force(10.0f);
				controller->initialize();
			}
		}

		// Setup enemy components
		for (auto enemy : m_enemies) {
			if (!enemy) continue;

			if (!enemy->get_component<Health>()) {
				auto* health = enemy->create_component<Health>();
				health->set_max_health(50.0f);
			}

			if (!enemy->get_component<Weapon>()) {
				auto* weapon = enemy->create_component<Weapon>();
				weapon->set_fire_rate(1.0f);
				weapon->set_entity_manager(entityMgr);
			}

			if (!enemy->get_component<EnemyAI>()) {
				auto* ai = enemy->create_component<EnemyAI>();
				ai->set_patrol_range(5.0f);
				ai->set_chase_range(10.0f);
				ai->set_attack_range(8.0f);
				ai->set_player_entity(m_player);
				ai->initialize();
			}
		}

		LOG_INFO("[GamePlayState] Entity components setup complete");
	}

	void GamePlayState::cleanup_level_entities()
	{
		LOG_INFO("[GamePlayState] Cleaning up level entities");
		
		// Clear entity lists
		m_levelEntities.clear();
		m_enemies.clear();
		m_player = nullptr;
		m_goal = nullptr;
		m_camera = nullptr;

		// Unload scene from SceneLoader
		if (m_sceneLoader) {
			m_sceneLoader->unload_scene();
		}
	}

	void GamePlayState::register_level_entity(GenericEntity* entity)
	{
		if (entity && std::find(m_levelEntities.begin(), m_levelEntities.end(), entity) == m_levelEntities.end()) {
			m_levelEntities.push_back(entity);
		}
	}

	bool GamePlayState::is_back_to_menu_pressed() const
	{
		auto* inputMgr = m_engine->input_manager();
		if (!inputMgr) return false;
		return inputMgr->is_key_pressed(Key::Escape);
	}
	
	void GamePlayState::update_game_logic(float deltaTime)
	{
		if (!m_player || m_levelCompleted) return;

		// Check if player reached goal
		auto* playerPhysics = m_player->get_component<SpritePhysicsComponent>();
		auto* goalPhysics = m_goal ? m_goal->get_component<SpritePhysicsComponent>() : nullptr;
		if (!playerPhysics || !goalPhysics) {
			return;
		}

		auto playerPos = playerPhysics->get_position();
		auto goalPos = goalPhysics->get_position();

		if (std::abs(playerPos.x - goalPos.x) < 2.0f && std::abs(playerPos.y - goalPos.y) < 2.0f) {
			m_levelCompleted = true;
			LOG_INFO("[GamePlayState] Goal reached! Level completed.");
		}
	}

	void GamePlayState::check_win_lose_conditions()
	{
		if (!m_player) return;

		// Check if player fell off the level
		auto* playerPhysics = m_player->get_component<SpritePhysicsComponent>();
		if (!playerPhysics) {
			return;
		}

		auto playerPos = playerPhysics->get_position();
		if (playerPos.y < -5.0f) {
			m_gameLost = true;
			LOG_INFO("[GamePlayState] Player fell! Game over.");
			return;
		}

		// Update enemy kill count
		int aliveEnemies = 0;
		for (auto enemy : m_enemies) {
			if (enemy && enemy->get_component<Health>()) {
				auto* health = enemy->get_component<Health>();
				if (health && health->is_alive()) {
					aliveEnemies++;
				}
			}
		}

		if (aliveEnemies != (m_totalEnemies - m_enemiesKilled)) {
			m_enemiesKilled = m_totalEnemies - aliveEnemies;
			LOG_INFO("[GamePlayState] Enemies killed: %d/%d", m_enemiesKilled, m_totalEnemies);
		}

		// Check back to menu
		if (is_back_to_menu_pressed()) {
			auto* manager = get_manager();
			if (manager) {
				LOG_INFO("[GamePlayState] Returning to title menu");
				manager->transition_to_state(new GameTitleState(), true);
			}
		}
	}
}
