///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: GamePlayState.cpp
// Creation Date: January 2026
//
// Purpose: Main gameplay state implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "GamePlayState.h"
#include "Engine.h"
#include "Entity.h"
#include "EntityManager.h"
#include "PhysicsSystem.h"
#include "GraphicsSystem.h"
#include "components/SpriteComponent.h"
#include "components/CameraComponent.h"
#include "components/PhysicComponent.h"
#include "../Components/PlayerController.h"
#include "../Components/EnemyAI.h"
#include "../Components/Health.h"
#include "../Components/Weapon.h"
#include "../Systems/CollisionSystem.h"
#include "../Systems/SpawnSystem.h"
#include "../Systems/ScoreSystem.h"

using namespace Pakal;

GamePlayState::GamePlayState()
	: BaseGameState("GamePlay")
	, m_engine(nullptr)
	, m_player(nullptr)
	, m_camera(nullptr)
	, m_collisionSystem(nullptr)
	, m_spawnSystem(nullptr)
	, m_scoreSystem(nullptr)
	, m_isPaused(false)
	, m_gameTime(0.0f)
	, m_levelWidth(5000.0f)
	, m_levelHeight(1000.0f)
	, m_cameraMin(0.0f, 0.0f)
	, m_cameraMax(5000.0f, 1000.0f)
{
}

GamePlayState::~GamePlayState()
{
	// Cleanup is done in on_terminate
}

void GamePlayState::on_initialize(Engine* engine)
{
	m_engine = engine;

	// Create game systems
	m_collisionSystem = new CollisionSystem();
	m_spawnSystem = new SpawnSystem();
	m_scoreSystem = new ScoreSystem();

	// Initialize systems
	m_collisionSystem->initialize(engine->physics_system());
	m_spawnSystem->initialize(engine->entity_manager());
	m_scoreSystem->initialize();

	// Setup score event handlers
	m_scoreSystem->on_game_over += [this]()
	{
		// TODO: Transition to game over state
		// get_manager()->push_state("GameOver");
	};

	// Create level
	setup_physics();
	create_level();
	create_player();
	create_camera();

	// Setup spawn system with player reference
	m_spawnSystem->set_player_entity(m_player);
}

void GamePlayState::on_terminate(Engine* engine)
{
	// Cleanup systems
	if (m_collisionSystem)
	{
		m_collisionSystem->terminate();
		delete m_collisionSystem;
		m_collisionSystem = nullptr;
	}

	if (m_spawnSystem)
	{
		m_spawnSystem->terminate();
		delete m_spawnSystem;
		m_spawnSystem = nullptr;
	}

	delete m_scoreSystem;
	m_scoreSystem = nullptr;

	// Entities are managed by EntityManager, so no manual cleanup needed
	m_player = nullptr;
	m_camera = nullptr;
	m_enemies.clear();
	m_platforms.clear();

	m_engine = nullptr;
}

void GamePlayState::on_update(unsigned long dtMilliseconds)
{
	if (m_isPaused)
		return;

	float deltaTime = dtMilliseconds / 1000.0f;
	m_gameTime += deltaTime;

	// Update systems
	if (m_collisionSystem)
		m_collisionSystem->update(deltaTime);

	if (m_spawnSystem)
		m_spawnSystem->update(deltaTime);

	if (m_scoreSystem)
		m_scoreSystem->update(deltaTime);

	// Update player
	if (m_player)
	{
		auto playerController = m_player->get_component<PlayerController>();
		if (playerController)
		{
			playerController->update(deltaTime);
		}
	}

	// Update enemies
	for (auto enemy : m_enemies)
	{
		if (!enemy)
			continue;

		auto enemyAI = enemy->get_component<EnemyAI>();
		if (enemyAI)
		{
			enemyAI->update(deltaTime);
		}
	}

	// Update camera
	update_camera(deltaTime);

	// Check game conditions
	check_win_condition();
	check_lose_condition();

	// Render
	on_render();
}

void GamePlayState::on_activate(Engine* engine)
{
	m_isPaused = false;
}

void GamePlayState::on_deactivate(Engine* engine)
{
	m_isPaused = true;
}

void GamePlayState::on_render()
{
	// Rendering is handled by graphics system automatically
	// Components like SpriteComponent will render themselves

	// Update UI
	update_ui();
}

void GamePlayState::setup_physics()
{
	if (!m_engine)
		return;

	// Physics settings
	auto physicsSystem = m_engine->physics_system();
	if (physicsSystem)
	{
		// Gravity for platformer
		// physicsSystem->set_gravity({0.0f, -20.0f});
	}
}

void GamePlayState::create_level()
{
	// Create ground
	create_platform(0.0f, -10.0f, 5000.0f, 20.0f);

	// Create platforms
	create_platform(500.0f, 50.0f, 200.0f, 20.0f);
	create_platform(1000.0f, 100.0f, 200.0f, 20.0f);
	create_platform(1500.0f, 150.0f, 200.0f, 20.0f);
	create_platform(2000.0f, 100.0f, 200.0f, 20.0f);
	create_platform(2500.0f, 50.0f, 200.0f, 20.0f);

	// Setup enemy spawns
	m_spawnSystem->add_spawn_point({800.0f, 60.0f}, "soldier", 500.0f);
	m_spawnSystem->add_spawn_point({1200.0f, 110.0f}, "soldier", 500.0f);
	m_spawnSystem->add_spawn_point({1800.0f, 160.0f}, "turret", 600.0f);
	m_spawnSystem->add_spawn_point({2200.0f, 110.0f}, "soldier", 500.0f);
	m_spawnSystem->add_spawn_point({2800.0f, 60.0f}, "soldier", 500.0f);

	// Start first wave
	m_spawnSystem->start_wave(0);
}

void GamePlayState::create_player()
{
	if (!m_engine)
		return;

	auto entityManager = m_engine->entity_manager();
	if (!entityManager)
		return;

	// TODO: Create player entity with components
	// m_player = entityManager->create_entity();
	
	// Add sprite component
	// auto sprite = m_player->add_component<SpriteComponent>();
	// sprite->set_position({100.0f, 0.0f, 0.0f});

	// Add physics component
	// auto physics = m_player->add_component<PhysicsComponent>();
	// physics->create_box_body(1.0f, 2.0f, true);

	// Add health
	// auto health = m_player->add_component<Health>();
	// health->set_max_health(100.0f);

	// Add weapon
	// auto weapon = m_player->add_component<Weapon>();
	// weapon->set_fire_rate(5.0f);
	// weapon->set_owner_tag("player");

	// Add player controller
	// auto controller = m_player->add_component<PlayerController>();
	// controller->initialize();
}

void GamePlayState::create_camera()
{
	if (!m_engine)
		return;

	auto entityManager = m_engine->entity_manager();
	if (!entityManager)
		return;

	// TODO: Create camera entity
	// m_camera = entityManager->create_entity();

	// Add camera component
	// auto cameraComp = m_camera->add_component<CameraComponent>();
	// CameraComponent::Settings settings;
	// settings.position = {100.0f, 0.0f, 300.0f};
	// settings.look_at = {100.0f, 0.0f, 0.0f};
	// cameraComp->initialize(settings);

	// Setup orthographic projection for 2D
	// auto graphicsSystem = m_engine->graphics_system();
	// if (graphicsSystem)
	// {
	//     auto resolution = graphicsSystem->get_screen_resolution();
	//     // Setup 2D camera projection
	// }
}

void GamePlayState::update_camera(float deltaTime)
{
	if (!m_camera || !m_player)
		return;

	// TODO: Implement camera following
	// auto cameraComp = m_camera->get_component<CameraComponent>();
	// auto playerSprite = m_player->get_component<SpriteComponent>();
	
	// if (cameraComp && playerSprite)
	// {
	//     auto playerPos = playerSprite->get_position();
	//     auto cameraPos = cameraComp->get_position();
	//     
	//     // Smooth camera follow
	//     float smoothness = 0.1f;
	//     float targetX = playerPos.x;
	//     float targetY = playerPos.y;
	//     
	//     // Clamp to level bounds
	//     targetX = std::max(m_cameraMin.x, std::min(m_cameraMax.x, targetX));
	//     targetY = std::max(m_cameraMin.y, std::min(m_cameraMax.y, targetY));
	//     
	//     // Lerp camera position
	//     float newX = cameraPos.x + (targetX - cameraPos.x) * smoothness;
	//     float newY = cameraPos.y + (targetY - cameraPos.y) * smoothness;
	//     
	//     cameraComp->set_position({newX, newY, cameraPos.z});
	//     cameraComp->set_target_and_position({newX, newY, 0.0f}, {newX, newY, cameraPos.z});
	// }
}

void GamePlayState::update_ui()
{
	if (!m_scoreSystem)
		return;

	// TODO: Update UI elements
	// - Health bar
	// - Lives counter
	// - Score display
	// - Combo counter

	// auto uiManager = m_engine->get_ui_manager();
	// if (uiManager)
	// {
	//     uiManager->set_text("score", std::to_string(m_scoreSystem->get_score()));
	//     uiManager->set_text("lives", std::to_string(m_scoreSystem->get_lives()));
	//     uiManager->set_text("combo", std::to_string(m_scoreSystem->get_combo()));
	// }
}

void GamePlayState::check_win_condition()
{
	// Win when all enemies are defeated
	if (m_spawnSystem && m_spawnSystem->get_active_enemies() == 0)
	{
		// TODO: Check if all spawn points have been triggered
		// If so, transition to win state
	}
}

void GamePlayState::check_lose_condition()
{
	// Lose when lives reach 0 (handled by ScoreSystem event)
	// Or when player health reaches 0
	
	if (m_player)
	{
		auto health = m_player->get_component<Health>();
		if (health && !health->is_alive())
		{
			m_scoreSystem->lose_life();
			
			if (!m_scoreSystem->is_game_over())
			{
				// Respawn player
				health->set_max_health(100.0f);
				// Reset player position
			}
		}
	}
}

Entity* GamePlayState::create_platform(float x, float y, float width, float height)
{
	if (!m_engine)
		return nullptr;

	auto entityManager = m_engine->entity_manager();
	if (!entityManager)
		return nullptr;

	// TODO: Create platform entity
	// Entity* platform = entityManager->create_entity();
	
	// Add sprite (optional, for visual)
	// auto sprite = platform->add_component<SpriteComponent>();
	// sprite->set_position({x, y, 0.0f});
	// sprite->set_scale({width / 64.0f, height / 64.0f, 1.0f});

	// Add physics (static body)
	// auto physics = platform->add_component<PhysicsComponent>();
	// physics->create_box_body(width, height, false);  // false = static

	// m_platforms.push_back(platform);
	// return platform;

	return nullptr;  // Stub
}

Entity* GamePlayState::spawn_enemy_soldier(float x, float y)
{
	// This would be called by SpawnSystem
	// See SpawnSystem::create_enemy() for implementation
	return nullptr;
}

Entity* GamePlayState::spawn_enemy_turret(float x, float y)
{
	// This would be called by SpawnSystem
	// See SpawnSystem::create_enemy() for implementation
	return nullptr;
}
