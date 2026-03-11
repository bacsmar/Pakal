///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GamePlayState.cpp
// Purpose: Main gameplay state for Contra game - Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "GamePlayState.h"
#include "GenericEntity.h"
#include "ComponentManager.h"
#include "components/SpritePhysicsComponent.h"
#include "Engine.h"
#include "EntityManager.h"
#include "Entity.h"
#include "LogMgr.h"

// Game components
#include "../Components/Health.h"
#include "../Components/Weapon.h"
#include "../Components/PlayerController.h"
#include "../Components/EnemyAI.h"

// Engine components  
#include "bgfx/SpriteComponent_Bgfx.h"
#include "bgfx/CameraComponent_Bgfx.h"
#include "box2D/SpritePhysicsComponent_Box2D.h"

namespace Pakal
{
	GamePlayState::GamePlayState() : BaseGameState("GamePlay"),
		m_engine(nullptr),
		m_player(nullptr),
		m_camera(nullptr),
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
		
		// Create game world
		create_level();
		create_player();
		create_enemies();
		setup_camera();
		
		LOG_INFO("[GamePlayState] Gameplay state initialized");
	}
	
	void GamePlayState::on_terminate(Engine* engine)
	{
		LOG_INFO("[GamePlayState] Terminating gameplay state");
		
		// Note: Entity cleanup is handled by EntityManager during engine shutdown
		// No need to manually destroy entities - they will be cleaned up automatically
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
		
		// Update game logic
		update_game_logic();
		
		// Check win/lose conditions
		check_win_lose_conditions();
	}
	
	void GamePlayState::create_level()
	{
		LOG_INFO("[GamePlayState] Creating level");
		
		// Create ground platform
		create_platform(0.0f, -5.0f, 50.0f, 1.0f);
		
		// Create elevated platforms
		create_platform(5.0f, 0.0f, 5.0f, 0.5f);
		create_platform(15.0f, 2.0f, 5.0f, 0.5f);
		create_platform(25.0f, 4.0f, 5.0f, 0.5f);
	}
	
	void GamePlayState::create_platform(float x, float y, float width, float height)
	{
		auto* entityMgr = m_engine->entity_manager();
		auto* platform = dynamic_cast<GenericEntity*>(entityMgr->create_entity("Pakal::GenericEntity", "platform"));
		if (!platform) return;
		
		// Add and configure sprite component
		auto* sprite = platform->create_component<SpriteComponent_Bgfx>();
		sprite->set_position(x, y);
		sprite->set_scale(width, height);
		sprite->set_color(0.5f, 0.3f, 0.2f, 1.0f); // Brown color
		sprite->create_solid_color(0xFFFFFFFF, 1, 1); // White texture
		
		// Add and configure physics component for static platform
		auto* physics = platform->create_component<SpritebodyComponent_Box2D>();
		SpritePhysicsComponent::Settings physics_settings;
		physics_settings.position = tmath::vector3df(x, y, 0.0f);
		physics_settings.scale = width;
		physics->initialize(physics_settings);
		physics->set_type(SpritePhysicsComponent::StaticBody);
	}
	
	void GamePlayState::create_player()
	{
		LOG_INFO("[GamePlayState] Creating player");
		
		auto* entityMgr = m_engine->entity_manager();
		m_player = dynamic_cast<GenericEntity*>(entityMgr->create_entity("Pakal::GenericEntity", "player"));
		if (!m_player) return;
		
		// Add sprite component
		auto* sprite = m_player->create_component<SpriteComponent_Bgfx>();
		sprite->set_position(0.0f, 0.0f);
		sprite->set_scale(1.0f, 2.0f);
		sprite->set_color(0.0f, 0.5f, 1.0f, 1.0f); // Blue color for player
		sprite->create_solid_color(0xFFFFFFFF, 1, 1);
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
		auto* physics = m_player->create_component<SpritebodyComponent_Box2D>();
		SpritePhysicsComponent::Settings physics_settings;
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
			
			// Position enemies at different locations
			float xPos = 10.0f + i * 5.0f;
			float yPos = 0.0f;
			
			// Add sprite component
			auto* sprite = enemy->create_component<SpriteComponent_Bgfx>();
			sprite->set_position(xPos, yPos);
			sprite->set_scale(1.0f, 2.0f);
			sprite->set_color(1.0f, 0.0f, 0.0f, 1.0f); // Red color for enemies
			sprite->create_solid_color(0xFFFFFFFF, 1, 1);
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
			auto* physics = enemy->create_component<SpritebodyComponent_Box2D>();
			SpritePhysicsComponent::Settings physics_settings;
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
		
		// Add camera component
		auto* camera = m_camera->create_component<CameraComponent_Bgfx>();
		camera->set_orthographic(800.0f, 600.0f);
		camera->set_viewport(0, 0, 800, 600);
		camera->set_position(0.0f, 0.0f);
		camera->set_zoom(1.0f);
		
		// Set camera to follow player
		if (m_player)
		{
			camera->follow_target(m_player, 0.1f);
		}
		
		// Set level bounds
		camera->set_bounds(-25.0f, -10.0f, 35.0f, 10.0f);
	}
	
	void GamePlayState::update_game_logic()
	{
		// Update player controller
		if (m_player)
		{
			auto* controller = m_player->get_component<PlayerController>();
			if (controller)
			{
				controller->update(0.016f); // Assuming 60 FPS
			}
			
			auto* weapon = m_player->get_component<Weapon>();
			if (weapon)
			{
				weapon->update(0.016f);
			}
		}
		
		// Update enemies
		for (GenericEntity* enemy : m_enemies)
		{
			if (!enemy)
				continue;
			
			auto* ai = enemy->get_component<EnemyAI>();
			if (ai)
			{
				ai->update(0.016f);
			}
			
			auto* weapon = enemy->get_component<Weapon>();
			if (weapon)
			{
				weapon->update(0.016f);
			}
		}
		
		// Update camera
		if (m_camera)
		{
			auto* camera = m_camera->get_component<CameraComponent_Bgfx>();
			if (camera)
			{
				camera->update(0.016f);
			}
		}
	}
	
	void GamePlayState::check_win_lose_conditions()
	{
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
		
		// Count alive enemies
		int aliveEnemies = 0;
		for (GenericEntity* enemy : m_enemies)
		{
			if (enemy)
			{
				auto* health = enemy->get_component<Health>();
				if (health && health->is_alive())
				{
					aliveEnemies++;
				}
			}
		}
		
		// Check if all enemies are dead
		if (aliveEnemies == 0 && !m_gameWon)
		{
			m_gameWon = true;
			LOG_INFO("[GamePlayState] Victory - All enemies defeated!");
		}
	}
}
