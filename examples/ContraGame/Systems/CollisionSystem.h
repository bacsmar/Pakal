///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CollisionSystem.h
// Creation Date: January 2026
//
// Purpose: Collision detection and response system
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Config.h"
#include <vector>
#include <unordered_map>

namespace Pakal
{
	class Entity;
	class PhysicsSystem;

	class _PAKALExport CollisionSystem
	{
	public:
		CollisionSystem();
		~CollisionSystem();

		void initialize(PhysicsSystem* physicsSystem);
		void terminate();
		void update(float deltaTime);

		// Collision callbacks
		void on_collision_begin(Entity* entityA, Entity* entityB);
		void on_collision_end(Entity* entityA, Entity* entityB);

		// Layer configuration
		enum CollisionLayer
		{
			LAYER_PLAYER = 0x0001,
			LAYER_ENEMY = 0x0002,
			LAYER_PLAYER_PROJECTILE = 0x0004,
			LAYER_ENEMY_PROJECTILE = 0x0008,
			LAYER_PLATFORM = 0x0010,
			LAYER_COLLECTIBLE = 0x0020,
			LAYER_ALL = 0xFFFF
		};

		// Setup collision filtering
		void set_collision_filter(Entity* entity, uint16_t category, uint16_t mask);

	private:
		void handle_projectile_collision(Entity* projectile, Entity* target);
		void handle_collectible_collision(Entity* collector, Entity* collectible);
		void handle_damage_collision(Entity* damager, Entity* target);

		PhysicsSystem* m_physicsSystem;

		// Track active collisions
		struct CollisionPair
		{
			Entity* entityA;
			Entity* entityB;

			bool operator==(const CollisionPair& other) const
			{
				return (entityA == other.entityA && entityB == other.entityB) ||
					   (entityA == other.entityB && entityB == other.entityA);
			}
		};
		std::vector<CollisionPair> m_activeCollisions;
	};
}
