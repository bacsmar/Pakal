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
#include "../Components/Projectile.h"

// Engine components  
#include "components/SpriteComponent2D.h"
#include "components/CameraComponent2D.h"
#include "InputManager_Polling.h"
#include "components/SpritePhysicsComponent.h"
#include <unordered_set>

namespace Pakal
{
	namespace
	{
		void sync_sprite_to_physics(GenericEntity& entity)
		{
			auto* physics = entity.get_component<SpritePhysicsComponent>();
			auto* sprite = entity.get_component<SpriteComponent2D>();
			if (!physics || !sprite)
			{
				return;
			}

			auto position = physics->get_position();
			sprite->set_position(position.x, position.y);
		}
	}

	GamePlayState::GamePlayState() : BaseGameState("GamePlay"),
		m_engine(nullptr),
		m_player(),
		m_goal(),
		m_camera(),
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
		update_level_components(deltaTime);
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
		m_player = {};
		m_goal = {};
		m_camera = {};
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
		m_player = m_sceneLoader->get_entity_handle("player");
		m_goal = m_sceneLoader->get_entity_handle("goal");
		m_camera = m_sceneLoader->get_entity_handle("camera");

		if (!m_player || !m_goal || !m_camera) {
			LOG_ERROR("[GamePlayState] Failed to retrieve required entities from scene");
			return;
		}

		// Register all loaded entities as level entities
		for (const auto& handle : m_sceneLoader->get_loaded_entity_handles()) {
			register_level_entity(handle);
		}

		// Collect enemies from loaded entities (entities starting with "enemy_")
		m_totalEnemies = 0;
		for (const auto& loadedEntityHandle : m_sceneLoader->get_loaded_entity_handles()) {
			auto* loadedEntity = resolve_entity(loadedEntityHandle);
			if (!loadedEntity) {
				continue;
			}

			std::string descriptor = loadedEntity->get_descriptor();
			if (descriptor.find("enemy") != std::string::npos) {
				m_enemies.push_back(loadedEntityHandle);
				m_totalEnemies++;
			}
		}

		LOG_INFO("[GamePlayState] Scene setup complete: %d enemies configured", m_totalEnemies);

		// Setup entity-specific game components
		setup_entity_components();

		// Configure camera with proper viewport and zoom
		auto* cameraEntityResolved = resolve_entity(m_camera);
		auto* cameraComp = cameraEntityResolved ? cameraEntityResolved->get_component<CameraComponent2D>() : nullptr;
		if (cameraComp && m_player) {
			// // Set orthographic projection to match current window resolution
			// // This ensures sprites render at proper scale
			// cameraComp->set_orthographic(1280.0f, 720.0f, 0.1f, 100.0f);
			
			// // Set a reasonable zoom level to see the level properly
			// // Lower zoom = objects appear larger
			// cameraComp->set_zoom(0.8f);
			
			// Configure camera to follow the player
			cameraComp->follow_target(m_player, 0.1f);
		}
	}

	void GamePlayState::setup_entity_components()
	{
		LOG_INFO("[GamePlayState] Setting up entity components");

		auto* entityMgr = m_engine->entity_manager();

		// Setup player components
		auto* playerEntity = resolve_entity(m_player);
		if (playerEntity) {
			if (!playerEntity->get_component<Health>()) {
				auto* health = playerEntity->create_component<Health>();
				health->set_max_health(100.0f);
			}

			if (!playerEntity->get_component<Weapon>()) {
				auto* weapon = playerEntity->create_component<Weapon>();
				weapon->set_fire_rate(0.2f);
				weapon->set_projectile_speed(15.0f);
				weapon->set_entity_manager(entityMgr);
				weapon->set_faction(CombatFaction::Player);
				weapon->set_projectile_created_callback([this](EntityHandle projectile) {
					register_level_entity(projectile);
				});
			}

			if (!playerEntity->get_component<PlayerController>()) {
				auto* controller = playerEntity->create_component<PlayerController>();
				controller->set_move_speed(5.0f);
				controller->set_jump_force(10.0f);
				controller->initialize();
			}
		}

		// Setup enemy components
		for (auto enemyHandle : m_enemies) {
			auto* enemy = resolve_entity(enemyHandle);
			if (!enemy) continue;

			if (!enemy->get_component<Health>()) {
				auto* health = enemy->create_component<Health>();
				health->set_max_health(50.0f);
			}

			if (!enemy->get_component<Weapon>()) {
				auto* weapon = enemy->create_component<Weapon>();
				weapon->set_fire_rate(1.0f);
				weapon->set_entity_manager(entityMgr);
				weapon->set_faction(CombatFaction::Enemy);
				weapon->set_projectile_created_callback([this](EntityHandle projectile) {
					register_level_entity(projectile);
				});
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

	void GamePlayState::update_level_components(float deltaTime)
	{
		const auto levelEntities = m_levelEntities;
		for (const auto& handle : levelEntities) {
			auto* entity = resolve_entity(handle);
			if (!entity || entity->is_pending_dispose()) {
				continue;
			}

			if (auto* weapon = entity->get_component<Weapon>()) {
				weapon->update(deltaTime);
			}
			if (auto* controller = entity->get_component<PlayerController>()) {
				controller->update(deltaTime);
			}
			if (auto* enemyAI = entity->get_component<EnemyAI>()) {
				enemyAI->update(deltaTime);
			}
			if (auto* projectile = entity->get_component<Projectile>()) {
				projectile->update(deltaTime);
			}

			sync_sprite_to_physics(*entity);

			if (auto* camera = entity->get_component<CameraComponent2D>()) {
				camera->update(deltaTime);
			}

			const auto& descriptor = entity->get_descriptor();
			if (descriptor.find("enemy") != std::string::npos) {
				if (auto* health = entity->get_component<Health>()) {
					if (!health->is_alive()) {
						entity->request_dispose();
					}
				}
			}
		}
	}

	void GamePlayState::cleanup_level_entities()
	{
		LOG_INFO("[GamePlayState] Cleaning up level entities");

		std::unordered_set<uint64_t> uniqueEntities;
		for (const auto& handle : m_levelEntities) {
			if (handle.is_valid()) {
				uniqueEntities.insert(handle.id);
			}
		}

		size_t destroyedCount = 0;
		for (const auto& handle : m_levelEntities) {
			if (!handle.is_valid() || uniqueEntities.find(handle.id) == uniqueEntities.end()) {
				continue;
			}
			if (m_engine && m_engine->entity_manager()) {
				m_engine->entity_manager()->request_dispose(handle);
			}
			uniqueEntities.erase(handle.id);
		}

		if (m_engine && m_engine->entity_manager()) {
			destroyedCount = m_engine->entity_manager()->process_pending_disposals();
		}

		LOG_INFO("[GamePlayState] Destroyed %zu entities", destroyedCount);

		m_levelEntities.clear();
		m_enemies.clear();
		m_player = {};
		m_goal = {};
		m_camera = {};

		// Unload scene from SceneLoader
		if (m_sceneLoader) {
			m_sceneLoader->unload_scene();
		}
	}

	void GamePlayState::register_level_entity(EntityHandle handle)
	{
		if (!handle.is_valid()) {
			return;
		}

		auto exists = std::find_if(m_levelEntities.begin(), m_levelEntities.end(), [handle](const EntityHandle& current) {
			return current.id == handle.id;
		});
		if (exists == m_levelEntities.end()) {
			m_levelEntities.push_back(handle);
		}
	}

	GenericEntity* GamePlayState::resolve_entity(EntityHandle handle) const
	{
		if (!m_engine || !m_engine->entity_manager()) {
			return nullptr;
		}

		return m_engine->entity_manager()->resolve<GenericEntity>(handle);
	}

	bool GamePlayState::is_back_to_menu_pressed() const
	{
		auto* inputMgr = m_engine->input_manager();
		if (!inputMgr) return false;
		return inputMgr->is_key_pressed(Key::Escape);
	}
	
	void GamePlayState::update_game_logic(float deltaTime)
	{
		auto* playerEntity = resolve_entity(m_player);
		auto* goalEntity = resolve_entity(m_goal);
		if (!playerEntity || m_levelCompleted) return;

		// Check if player reached goal
		auto* playerPhysics = playerEntity->get_component<SpritePhysicsComponent>();
		auto* goalPhysics = goalEntity ? goalEntity->get_component<SpritePhysicsComponent>() : nullptr;
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
		auto* playerEntity = resolve_entity(m_player);
		if (!playerEntity) return;

		// Check if player fell off the level
		auto* playerPhysics = playerEntity->get_component<SpritePhysicsComponent>();
		if (!playerPhysics) {
			return;
		}

		auto playerPos = playerPhysics->get_position();
		if (playerPos.y < -5.0f) {
			m_gameLost = true;
			LOG_INFO("[GamePlayState] Player fell! Game over.");
			return;
		}

		auto* playerHealth = playerEntity->get_component<Health>();
		if (playerHealth && !playerHealth->is_alive()) {
			m_gameLost = true;
			LOG_INFO("[GamePlayState] Player died! Game over.");
			return;
		}

		// Update enemy kill count
		int aliveEnemies = 0;
		for (auto enemyHandle : m_enemies) {
			auto* enemy = resolve_entity(enemyHandle);
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
