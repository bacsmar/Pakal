///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EnemyAI.h
// Creation Date: January 2026
//
// Purpose: Enemy AI behavior controller
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Component.h"
#include "PakalMath.h"
#include <vector>

namespace Pakal
{
	class SpriteComponent;
	class Health;
	class Weapon;
	class PhysicComponent;
	class Entity;

	class _PAKALExport EnemyAI : public Component
	{
		DECLARE_RTTI_WITH_BASE(EnemyAI, Component);

	public:
		enum class AIState
		{
			PATROL,
			CHASE,
			ATTACK,
			RETREAT,
			DEAD
		};

		EnemyAI();
		~EnemyAI() override = default;

		void initialize();
		void update(float deltaTime);

		// Configuration
		void set_patrol_points(const std::vector<tmath::vector2df>& points);
		void set_chase_range(float range) { m_chaseRange = range; }
		void set_attack_range(float range) { m_attackRange = range; }
		void set_fire_rate(float rate) { m_fireRate = rate; }
		void set_move_speed(float speed) { m_moveSpeed = speed; }
		void set_player_entity(Entity* player) { m_player = player; }

		// State queries
		AIState get_state() const { return m_state; }
		bool is_active() const { return m_state != AIState::DEAD; }

	private:
		void update_patrol(float deltaTime);
		void update_chase(float deltaTime);
		void update_attack(float deltaTime);
		void update_retreat(float deltaTime);

		bool can_see_player();
		float get_distance_to_player();
		void shoot_at_player();
		void move_towards(const tmath::vector2df& target, float deltaTime);
		void change_state(AIState newState);

		AIState m_state;

		// References
		Entity* m_player;
		PhysicComponent* m_physics;
		SpriteComponent* m_sprite;
		Weapon* m_weapon;
		Health* m_health;

		// Patrol behavior
		std::vector<tmath::vector2df> m_patrolPoints;
		int m_currentPatrolPoint;
		float m_patrolWaitTime;
		float m_patrolWaitTimer;

		// Detection ranges
		float m_chaseRange;
		float m_attackRange;
		float m_losePlayerRange;

		// Movement
		float m_moveSpeed;

		// Combat
		float m_fireRate;
		float m_fireTimer;

		// Timing
		float m_stateTime;  // Time in current state
	};
}
