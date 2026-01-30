///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EnemyAI.h
// Purpose: Enemy AI behavior component
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Component.h"
#include "math/vector2.h"

namespace Pakal
{
	class Entity;
	class SpritebodyComponent_Box2D;
	class SpriteComponent_Bgfx;
	class Weapon;
	class Health;
	
	class _PAKALExport EnemyAI : public Component
	{
		DECLARE_RTTI_WITH_BASE(EnemyAI, Component);
		
	public:
		enum AIState { PATROL, CHASE, ATTACK, DEAD };
		
		EnemyAI();
		virtual ~EnemyAI() = default;
		
		void initialize();
		void update(float deltaTime);
		
		void set_patrol_range(float range) { m_patrolRange = range; }
		void set_chase_range(float range) { m_chaseRange = range; }
		void set_attack_range(float range) { m_attackRange = range; }
		void set_fire_rate(float rate) { m_fireRate = rate; }
		void set_player_entity(Entity* player) { m_player = player; }
		
	private:
		void update_patrol(float deltaTime);
		void update_chase(float deltaTime);
		void update_attack(float deltaTime);
		
		bool can_see_player();
		void shoot_at_player();
		
		AIState m_state;
		
		// Components
		Entity* m_player;
		SpritebodyComponent_Box2D* m_physics;
		SpriteComponent_Bgfx* m_sprite;
		Weapon* m_weapon;
		Health* m_health;
		
		// AI parameters
		float m_patrolRange;
		float m_chaseRange;
		float m_attackRange;
		float m_fireRate;
		float m_fireTimer;
		
		// Patrol
		tmath::vectorn<float, 2> m_patrolStart;
		bool m_patrolRight;
	};
}
