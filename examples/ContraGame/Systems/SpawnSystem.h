///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SpawnSystem.h
// Creation Date: January 2026
//
// Purpose: Enemy and collectible spawning system
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Config.h"
#include "PakalMath.h"
#include <vector>

namespace Pakal
{
	class Entity;
	class EntityManager;

	class _PAKALExport SpawnSystem
	{
	public:
		struct SpawnPoint
		{
			tmath::vector2df position;
			std::string enemyType;  // "soldier", "turret", etc.
			float triggerDistance;   // Distance from player to trigger spawn
			bool hasSpawned;
			int waveIndex;           // Which wave this belongs to
		};

		struct Wave
		{
			int waveNumber;
			std::vector<int> spawnPointIndices;
			float delay;  // Delay before wave starts
			bool isActive;
			bool isComplete;
		};

		SpawnSystem();
		~SpawnSystem();

		void initialize(EntityManager* entityManager);
		void terminate();
		void update(float deltaTime);

		// Configuration
		void add_spawn_point(const tmath::vector2df& position, const std::string& enemyType, 
			float triggerDistance = 500.0f, int waveIndex = 0);
		void add_wave(int waveNumber, const std::vector<int>& spawnPointIndices, float delay = 0.0f);
		void set_player_entity(Entity* player) { m_player = player; }

		// Control
		void start_wave(int waveNumber);
		void reset_all_spawns();
		int get_current_wave() const { return m_currentWave; }
		int get_total_enemies_spawned() const { return m_totalSpawned; }
		int get_active_enemies() const { return m_activeEnemies.size(); }

	private:
		void check_spawn_triggers();
		void spawn_enemy_at_point(const SpawnPoint& spawnPoint);
		Entity* create_enemy(const std::string& type, const tmath::vector2df& position);

		EntityManager* m_entityManager;
		Entity* m_player;

		std::vector<SpawnPoint> m_spawnPoints;
		std::vector<Wave> m_waves;
		std::vector<Entity*> m_activeEnemies;

		int m_currentWave;
		int m_totalSpawned;
		float m_waveTimer;
	};
}
