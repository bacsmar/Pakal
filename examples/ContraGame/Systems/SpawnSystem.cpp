///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SpawnSystem.cpp
// Creation Date: January 2026
//
// Purpose: Enemy and collectible spawning implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "SpawnSystem.h"
#include "Entity.h"
#include "EntityManager.h"
#include "components/SpriteComponent.h"
#include "../Components/Health.h"
#include "../Components/Weapon.h"
#include "../Components/EnemyAI.h"
#include <cmath>
#include <algorithm>

using namespace Pakal;

SpawnSystem::SpawnSystem()
	: m_entityManager(nullptr)
	, m_player(nullptr)
	, m_currentWave(0)
	, m_totalSpawned(0)
	, m_waveTimer(0.0f)
{
}

SpawnSystem::~SpawnSystem()
{
	terminate();
}

void SpawnSystem::initialize(EntityManager* entityManager)
{
	m_entityManager = entityManager;
}

void SpawnSystem::terminate()
{
	m_spawnPoints.clear();
	m_waves.clear();
	m_activeEnemies.clear();
	m_entityManager = nullptr;
	m_player = nullptr;
}

void SpawnSystem::update(float deltaTime)
{
	// Update wave timer
	if (m_waveTimer > 0.0f)
	{
		m_waveTimer -= deltaTime;
		return;  // Wait for wave delay
	}

	// Check spawn triggers (distance-based spawning)
	check_spawn_triggers();

	// Clean up destroyed enemies
	m_activeEnemies.erase(
		std::remove_if(m_activeEnemies.begin(), m_activeEnemies.end(),
			[](Entity* enemy)
			{
				// TODO: Check if entity is marked for destruction
				// For now, check if health component exists and is dead
				auto health = enemy->get_component<Health>();
				return health && !health->is_alive();
			}),
		m_activeEnemies.end()
	);
}

void SpawnSystem::add_spawn_point(const tmath::vector2df& position, const std::string& enemyType,
	float triggerDistance, int waveIndex)
{
	SpawnPoint point;
	point.position = position;
	point.enemyType = enemyType;
	point.triggerDistance = triggerDistance;
	point.hasSpawned = false;
	point.waveIndex = waveIndex;

	m_spawnPoints.push_back(point);
}

void SpawnSystem::add_wave(int waveNumber, const std::vector<int>& spawnPointIndices, float delay)
{
	Wave wave;
	wave.waveNumber = waveNumber;
	wave.spawnPointIndices = spawnPointIndices;
	wave.delay = delay;
	wave.isActive = false;
	wave.isComplete = false;

	m_waves.push_back(wave);
}

void SpawnSystem::start_wave(int waveNumber)
{
	for (auto& wave : m_waves)
	{
		if (wave.waveNumber == waveNumber)
		{
			wave.isActive = true;
			m_waveTimer = wave.delay;
			m_currentWave = waveNumber;
			break;
		}
	}
}

void SpawnSystem::reset_all_spawns()
{
	for (auto& point : m_spawnPoints)
	{
		point.hasSpawned = false;
	}

	for (auto& wave : m_waves)
	{
		wave.isActive = false;
		wave.isComplete = false;
	}

	m_totalSpawned = 0;
	m_currentWave = 0;
	m_waveTimer = 0.0f;
}

void SpawnSystem::check_spawn_triggers()
{
	if (!m_player || !m_entityManager)
		return;

	// Get player position
	auto playerSprite = m_player->get_component<SpriteComponent>();
	if (!playerSprite)
		return;

	auto playerPos = playerSprite->get_position();

	// Check each spawn point
	for (size_t i = 0; i < m_spawnPoints.size(); ++i)
	{
		auto& spawnPoint = m_spawnPoints[i];

		// Skip if already spawned
		if (spawnPoint.hasSpawned)
			continue;

		// Check if spawn point is in current wave
		bool isInCurrentWave = false;
		for (const auto& wave : m_waves)
		{
			if (wave.waveNumber == m_currentWave && wave.isActive)
			{
				// Check if this spawn point is in the wave
				auto it = std::find(wave.spawnPointIndices.begin(), 
					wave.spawnPointIndices.end(), static_cast<int>(i));
				if (it != wave.spawnPointIndices.end())
				{
					isInCurrentWave = true;
					break;
				}
			}
		}

		// If spawn point belongs to a wave, only spawn if that wave is active
		if (spawnPoint.waveIndex > 0 && !isInCurrentWave)
			continue;

		// Calculate distance to player
		float dx = spawnPoint.position.x - playerPos.x;
		float dy = spawnPoint.position.y - playerPos.y;
		float distance = std::sqrt(dx * dx + dy * dy);

		// Spawn if player is close enough
		if (distance < spawnPoint.triggerDistance)
		{
			spawn_enemy_at_point(spawnPoint);
			spawnPoint.hasSpawned = true;
		}
	}
}

void SpawnSystem::spawn_enemy_at_point(const SpawnPoint& spawnPoint)
{
	Entity* enemy = create_enemy(spawnPoint.enemyType, spawnPoint.position);
	if (enemy)
	{
		m_activeEnemies.push_back(enemy);
		m_totalSpawned++;
	}
}

Entity* SpawnSystem::create_enemy(const std::string& type, const tmath::vector2df& position)
{
	if (!m_entityManager)
		return nullptr;

	// TODO: Create enemy entity with proper components
	// This is a stub - actual implementation would:
	// 1. Create entity through EntityManager
	// 2. Add SpriteComponent with enemy sprite
	// 3. Add PhysicsComponent with Box2D body
	// 4. Add Health component
	// 5. Add Weapon component
	// 6. Add EnemyAI component
	// 7. Configure based on enemy type (soldier, turret, etc.)

	/*
	Entity* enemy = m_entityManager->create_entity();

	// Add sprite
	auto sprite = enemy->add_component<SpriteComponent>();
	sprite->initialize(spriteSettings);
	sprite->set_position(tmath::vector3df(position.x, position.y, 0.0f));

	// Add health
	auto health = enemy->add_component<Health>();
	health->set_max_health(50.0f);

	// Add weapon
	auto weapon = enemy->add_component<Weapon>();
	weapon->set_fire_rate(1.0f);
	weapon->set_owner_tag("enemy");

	// Add AI
	auto ai = enemy->add_component<EnemyAI>();
	ai->set_player_entity(m_player);
	ai->initialize();

	if (type == "soldier")
	{
		// Configure soldier enemy
		ai->set_move_speed(100.0f);
		ai->set_chase_range(400.0f);
		ai->set_attack_range(300.0f);
	}
	else if (type == "turret")
	{
		// Configure turret enemy (stationary)
		ai->set_move_speed(0.0f);
		ai->set_chase_range(600.0f);
		ai->set_attack_range(600.0f);
	}

	return enemy;
	*/

	return nullptr;  // Stub
}
