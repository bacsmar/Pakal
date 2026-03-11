///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GamePlayState.cpp
// Purpose: Main gameplay state for Contra game - Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "GamePlayState.h"
#include "GameTitleState.h"
#include "GenericEntity.h"
#include "ComponentManager.h"
#include "components/SpritePhysicsComponent.h"
#include "Engine.h"
#include "GameStateManager.h"
#include "EntityManager.h"
#include "Entity.h"
#include "LogMgr.h"
#include <algorithm> // Add this for std::find

// Game components
#include "../Components/Health.h"
#include "../Components/Weapon.h"
#include "../Components/PlayerController.h"
#include "../Components/EnemyAI.h"

// Engine components  
#include "components/SpriteComponent2D.h"
#include "components/CameraComponent2D.h"
#include "InputManager_Polling.h"

namespace Pakal
{
	namespace
	{
		SpriteSheetPhysicsPtr create_box_body_physics(float halfWidth, float halfHeight, bool dynamicBody, bool fixedRotation)
		{
			auto sheet = std::make_shared<SpriteSheetPhysics>();
			auto* body = new SpritePhysics();
			body->name = "default";
			body->dynamic = dynamicBody;
			body->fixed_rotation = fixedRotation;

			SpritePhysics::Fixture fixture;
			fixture.type = "POLYGON";
			fixture.scale = 1.0f;
			fixture.density = 1.0f;
			fixture.friction = 0.3f;
			fixture.restitution = 0.0f;

			SpritePhysics::Polygon polygon;
			polygon.m_vertices.emplace_back(-halfWidth, -halfHeight);
			polygon.m_vertices.emplace_back(halfWidth, -halfHeight);
			polygon.m_vertices.emplace_back(halfWidth, halfHeight);
			polygon.m_vertices.emplace_back(-halfWidth, halfHeight);
			fixture.m_polygons.emplace_back(std::move(polygon));

			body->m_fixtures.emplace_back(std::move(fixture));
			sheet->bodies.emplace_back(body);
			return sheet;
		}
	}

	GamePlayState::GamePlayState() : BaseGameState("GamePlay"),
		m_engine(nullptr),
		m_player(nullptr),
		m_camera(nullptr),
		m_currentLevel(1),
		m_maxLevels(2),
		m_levelTransitionTimer(0.0f),
		m_levelCompleted(false),
		m_returningToMenu(false),
		m_playerGoalX(40.0f),
		m_score(0),
		m_lives(3),
		m_enemiesKilled(0),
		m_totalEnemies(5),
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

		load_level(m_currentLevel);
		
		LOG_INFO("[GamePlayState] Gameplay state initialized");
	}
	
	void GamePlayState::on_terminate(Engine* engine)
	{
		LOG_INFO("[GamePlayState] Terminating gameplay state");

		cleanup_level_entities();
		m_player = nullptr;
		m_camera = nullptr;
		m_enemies.clear();
	}
	
	void GamePlayState::on_activate(Engine* engine)
	{
		LOG_INFO("[GamePlayState] Activating gameplay state");
	}
	
	void GamePlayState::on_deactivate(Engine* engine)
	{
		LOG_INFO("[GamePlayState] Deactivating gameplay state");
	}
	
	void GamePlayState::on_update(unsigned long dtMilliseconds)
	{
		float deltaTime = dtMilliseconds / 1000.0f;

		if (is_back_to_menu_pressed())
		{
			auto* manager = get_manager();
			if (manager)
			{
				LOG_INFO("[GamePlayState] Returning to title menu");
				manager->transition_to_state(new GameTitleState(), true);
				return;
			}
		}
		
		// Update game logic
		update_game_logic(deltaTime);
		
		// Check win/lose conditions
		check_win_lose_conditions();

		if (m_levelCompleted)
		{
			m_levelTransitionTimer += deltaTime;
			if (m_levelTransitionTimer >= 1.0f)
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
	}

	void GamePlayState::load_level(int levelNumber)
	{
		cleanup_level_entities();

		m_enemies.clear();
		m_player = nullptr;
		m_camera = nullptr;
		m_levelCompleted = false;
		m_levelTransitionTimer = 0.0f;
		m_gameWon = false;
		m_gameLost = false;

		const LevelConfig config = get_level_config(levelNumber);
		m_totalEnemies = config.enemyCount;
		m_enemiesKilled = 0;
		m_playerGoalX = config.goalX;

		create_level();
		create_player();
		create_enemies();
		setup_camera();

		if (m_player)
		{
			auto* physics = m_player->get_component<SpritePhysicsComponent>();
			if (physics)
			{
				physics->set_position(tmath::vector3df(config.startX, 0.0f, 0.0f));
			}
			auto* sprite = m_player->get_component<SpriteComponent2D>();
			if (sprite)
			{
				sprite->set_position(config.startX, 0.0f);
			}
		}

		LOG_INFO("[GamePlayState] Level %d ready (goalX=%.2f, enemies=%d)", config.levelNumber, config.goalX, config.enemyCount);
	}

	void GamePlayState::cleanup_level_entities()
	{
		for (GenericEntity* entity : m_levelEntities)
		{
			delete entity;
		}
		m_levelEntities.clear();
	}

	void GamePlayState::register_level_entity(GenericEntity* entity)
	{
		if (entity)
		{
			m_levelEntities.push_back(entity);
		}
	}

	GamePlayState::LevelConfig GamePlayState::get_level_config(int levelNumber) const
	{
		if (levelNumber <= 1)
		{
			return { 1, 0.0f, 34.0f, 4 };
		}

		return { 2, 2.0f, 56.0f, 7 };
	}

	bool GamePlayState::is_back_to_menu_pressed() const
	{
		auto& input = InputManager_Polling::instance();
		return input.poll_key_down(Key::Escape);
	}
	
	void GamePlayState::create_level()
	{
		LOG_INFO("[GamePlayState] Creating level geometry for level %d", m_currentLevel);

		if (m_currentLevel == 1)
		{
			create_platform(16.0f, -5.0f, 42.0f, 1.0f);
			create_platform(8.0f, -1.0f, 6.0f, 0.6f);
			create_platform(17.0f, 1.5f, 5.5f, 0.6f);
			create_platform(26.0f, 3.0f, 4.5f, 0.6f);
			create_platform(33.0f, 0.5f, 3.5f, 0.6f);
		}
		else
		{
			create_platform(30.0f, -5.0f, 65.0f, 1.0f);
			create_platform(12.0f, -1.0f, 5.0f, 0.6f);
			create_platform(20.0f, 1.5f, 5.0f, 0.6f);
			create_platform(29.0f, 3.2f, 5.0f, 0.6f);
			create_platform(38.0f, 1.0f, 5.0f, 0.6f);
			create_platform(47.0f, 3.0f, 5.0f, 0.6f);
			create_platform(55.0f, 0.0f, 4.0f, 0.6f);
		}
	}
	
	void GamePlayState::create_platform(float x, float y, float width, float height)
	{
		auto* entityMgr = m_engine->entity_manager();
		auto* platform = dynamic_cast<GenericEntity*>(entityMgr->create_entity("Pakal::GenericEntity", "platform"));
		if (!platform) return;
		register_level_entity(platform);
		
		// Add and configure sprite component
		auto* sprite = platform->create_component<SpriteComponent2D>();
		sprite->set_position(x, y);
		sprite->set_scale(width, height);
		sprite->set_color(1.0f, 1.0f, 1.0f, 1.0f);
		sprite->set_texture("Assets/sprites/platform.png");
		
		// Add and configure physics component for static platform
		auto* physics = platform->create_component<SpritePhysicsComponent>();
		SpritePhysicsComponent::Settings physics_settings(create_box_body_physics(width * 0.5f, height * 0.5f, false, true));
		physics_settings.position = tmath::vector3df(x, y, 0.0f);
		physics_settings.scale = 1.0f;
		physics->initialize(physics_settings);
		physics->set_type(SpritePhysicsComponent::StaticBody);
	}
	
	void GamePlayState::create_player()
	{
		LOG_INFO("[GamePlayState] Creating player");
		
		auto* entityMgr = m_engine->entity_manager();
		m_player = dynamic_cast<GenericEntity*>(entityMgr->create_entity("Pakal::GenericEntity", "player"));
		if (!m_player) return;
		register_level_entity(m_player);
		
		// Add sprite component
		auto* sprite = m_player->create_component<SpriteComponent2D>();
		sprite->set_position(0.0f, 0.0f);
		sprite->set_scale(1.0f, 2.0f);
		sprite->set_color(1.0f, 1.0f, 1.0f, 1.0f);
		sprite->set_texture("Assets/sprites/player_idle.png");
		sprite->set_layer(10); // Higher layer for player
		
		// Add health component
		auto* health = m_player->create_component<Health>();
		health->set_max_health(100.0f);
		
		// Add weapon component
		auto* weapon = m_player->create_component<Weapon>();
		weapon->set_fire_rate(0.2f);
		weapon->set_projectile_speed(15.0f);
		weapon->set_entity_manager(entityMgr);
		
		// Add player controller
		auto* controller = m_player->create_component<PlayerController>();
		controller->set_move_speed(5.0f);
		controller->set_jump_force(10.0f);
		
		// Add physics component for dynamic player body
		auto* physics = m_player->create_component<SpritePhysicsComponent>();
		SpritePhysicsComponent::Settings physics_settings(create_box_body_physics(0.5f, 1.0f, true, true));
		physics_settings.position = tmath::vector3df(0.0f, 0.0f, 0.0f);
		physics_settings.scale = 1.0f;
		physics->initialize(physics_settings);
		physics->set_type(SpritePhysicsComponent::DynamicBody);
		physics->set_fixed_rotation(true); // Prevent player from rotating
		
		// Initialize controller after all components are added
		controller->initialize();
	}
	
	void GamePlayState::create_enemies()
	{
		LOG_INFO("[GamePlayState] Creating enemies");
		
		auto* entityMgr = m_engine->entity_manager();
		
		// Create several enemies
		for (int i = 0; i < m_totalEnemies; i++)
		{
			auto* enemy = dynamic_cast<GenericEntity*>(entityMgr->create_entity("Pakal::GenericEntity", "enemy"));
			if (!enemy) continue;
			register_level_entity(enemy);
			
			// Position enemies at different locations
			float spacing = (m_currentLevel == 1) ? 5.0f : 6.5f;
			float xPos = 10.0f + i * spacing;
			float yPos = (i % 2 == 0) ? 0.0f : 1.0f;
			
			// Add sprite component
			auto* sprite = enemy->create_component<SpriteComponent2D>();
			sprite->set_position(xPos, yPos);
			sprite->set_scale(1.0f, 2.0f);
			sprite->set_color(1.0f, 1.0f, 1.0f, 1.0f);
			sprite->set_texture("Assets/sprites/enemy.jpg");
			sprite->set_layer(10);
			
			// Add health component
			auto* health = enemy->create_component<Health>();
			health->set_max_health(50.0f);
			
			// Add weapon component
			auto* weapon = enemy->create_component<Weapon>();
			weapon->set_fire_rate(1.0f);
			weapon->set_entity_manager(entityMgr);
			
			// Add AI component
			auto* ai = enemy->create_component<EnemyAI>();
			ai->set_patrol_range(5.0f);
			ai->set_chase_range(10.0f);
			ai->set_attack_range(8.0f);
			ai->set_player_entity(m_player);
			
			// Add physics component for dynamic enemy body
			auto* physics = enemy->create_component<SpritePhysicsComponent>();
			SpritePhysicsComponent::Settings physics_settings(create_box_body_physics(0.5f, 1.0f, true, true));
			physics_settings.position = tmath::vector3df(xPos, yPos, 0.0f);
			physics_settings.scale = 1.0f;
			physics->initialize(physics_settings);
			physics->set_type(SpritePhysicsComponent::DynamicBody);
			physics->set_fixed_rotation(true); // Prevent enemy from rotating
			
			// Initialize AI after all components are added
			ai->initialize();
			
			m_enemies.push_back(enemy);
		}
	}
	
	void GamePlayState::setup_camera()
	{
		LOG_INFO("[GamePlayState] Setting up camera");
		
		auto* entityMgr = m_engine->entity_manager();
		m_camera = dynamic_cast<GenericEntity*>(entityMgr->create_entity("Pakal::GenericEntity", "camera"));
		if (!m_camera) return;
		register_level_entity(m_camera);
		
		// Add camera component
		auto* camera = m_camera->create_component<CameraComponent2D>();
		camera->set_orthographic(32.0f, 18.0f);
		camera->set_viewport(0, 0, 1280, 720);
		camera->set_position(0.0f, 0.0f);
		camera->set_zoom(1.0f);
		
		// Set camera to follow player
		if (m_player)
		{
			camera->follow_target(m_player, 0.1f);
		}
		
		// Set level bounds
		const float maxX = (m_currentLevel == 1) ? 40.0f : 62.0f;
		camera->set_bounds(-2.0f, -10.0f, maxX, 10.0f);
	}
	
//////////. ..
	void GamePlayState::update_game_logic(float deltaTime)
	{
		// Update player controller
		if (m_player)
		{
			auto* controller = m_player->get_component<PlayerController>();
			if (controller)
			{
				controller->update(deltaTime);
			}
			
			auto* weapon = m_player->get_component<Weapon>();
			if (weapon)
			{
				weapon->update(deltaTime);
			}
		}
		
		// Update enemies and handle dead enemies
		std::vector<GenericEntity*>::iterator it = m_enemies.begin();
		while (it != m_enemies.end())
		{
			GenericEntity* enemy = *it;
			if (!enemy)
			{
				it = m_enemies.erase(it);
				continue;
			}
			
			// Check if enemy is dead
			auto* health = enemy->get_component<Health>();
			if (health && !health->is_alive())
			{
				// Remove dead enemy from level entities
				auto levelIt = std::find(m_levelEntities.begin(), m_levelEntities.end(), enemy);
				if (levelIt != m_levelEntities.end())
				{
					m_levelEntities.erase(levelIt);
				}
				
				// Remove from enemies list
				it = m_enemies.erase(it);
				
				// Increment kill count
				m_enemiesKilled++;
				
				// Delete the enemy entity
				delete enemy;
				continue;
			}
			
			// Update enemy components if alive
			auto* ai = enemy->get_component<EnemyAI>();
			if (ai)
			{
				ai->update(deltaTime);
			}
			
			auto* weapon = enemy->get_component<Weapon>();
			if (weapon)
			{
				weapon->update(deltaTime);
			}
			
			++it;
		}
		
		// Update camera
		if (m_camera)
		{
			auto* camera = m_camera->get_component<CameraComponent2D>();
			if (camera)
			{
				camera->update(deltaTime);
			}
		}
	}
	
	void GamePlayState::check_win_lose_conditions()
	{
		if (!m_player || m_levelCompleted)
		{
			return;
		}

		// Check if player is dead
		if (m_player)
		{
			auto* health = m_player->get_component<Health>();
			if (health && !health->is_alive())
			{
				m_lives--;
				if (m_lives <= 0)
				{
					m_gameLost = true;
					LOG_INFO("[GamePlayState] Game Over - Player defeated");
				}
			}
		}

		// Fail condition: player falls out of map
		auto* physics = m_player->get_component<SpritePhysicsComponent>();
		if (physics)
		{
			auto playerPos = physics->get_position();
			if (playerPos.y < -15.0f)
			{
				m_gameLost = true;
				LOG_INFO("[GamePlayState] Game Over - Player fell out of the level");
				return;
			}

			// Level completion: reach right-side objective AND kill all enemies
			if (playerPos.x >= m_playerGoalX)
			{
				// Check if all enemies are defeated
				if (m_enemiesKilled >= m_totalEnemies)
				{
					m_levelCompleted = true;
					m_gameWon = (m_currentLevel >= m_maxLevels);
					LOG_INFO("[GamePlayState] Level %d complete! All enemies defeated.", m_currentLevel);
				}
				else
				{
					// Player reached goal but hasn't defeated all enemies yet
					LOG_INFO("[GamePlayState] Reached goal but need to defeat all enemies first.");
				}
			}
		}
	}
}
