///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EnemyAI.h
// Purpose: Enemy AI behavior component
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Component.h"
#include "EntityHandle.h"
#include "math/vector2.h"

namespace Pakal
{
	class Entity;
		class SpritePhysicsComponent;
		class SpriteComponent2D;
	class Weapon;
	class Health;
	
	class _PAKALExport EnemyAI : public Component
	{
		DECLARE_RTTI_WITH_BASE(EnemyAI, Component);
		
	public:
		enum AIState { PATROL, CHASE, ATTACK, DEAD };
		enum Personality { Balanced, Aggressive, Cautious, Sentinel };
		
		EnemyAI();
		virtual ~EnemyAI() = default;
		
		void initialize();
		void update(float deltaTime);
		
		void set_patrol_range(float range) { m_patrolRange = range; }
		void set_chase_range(float range) { m_chaseRange = range; }
		void set_attack_range(float range) { m_attackRange = range; }
		void set_fire_rate(float rate) { m_fireRate = rate; }
		void set_personality(Personality personality) { m_personality = personality; }
		void set_flank_route_offset(float offset) { m_flankRouteOffset = offset; }
		void set_player_entity(EntityHandle player);
		void set_player_entity(Entity* player);
		void set_idle_animation(int startFrame, int endFrame);
		void set_walk_animation(int startFrame, int endFrame);
		void set_run_animation(int startFrame, int endFrame);
		void set_attack_animation(int startFrame, int endFrame);
		
	private:
		Entity* resolve_player_entity() const;
		void apply_animation(int startFrame, int endFrame, bool loop);
		void update_facing_from_dx(float dx);
		void update_facing_from_direction(bool facingRight);
		float choose_chase_target_x(float enemyX, float playerX, float distanceToPlayer);
		void update_patrol(float deltaTime);
		void update_chase(float deltaTime);
		void update_attack(float deltaTime);
		
		bool can_see_player();
		void shoot_at_player();
		
		// Advanced behaviors (for future implementation)
		#ifdef ADVANCED_ENEMY_AI
		void check_and_jump_obstacles();
		void perform_diagonal_patrol();
		void manage_ammo();
		void coordinate_with_group();
		#endif
		
		AIState m_state;
		
		// Components
		EntityHandle m_playerHandle;
		SpritePhysicsComponent* m_physics;
		SpriteComponent2D* m_sprite;
		Weapon* m_weapon;
		Health* m_health;
		
		// AI parameters
		float m_patrolRange;
		float m_chaseRange;
		float m_attackRange;
		float m_fireRate;
		float m_fireTimer;
		Personality m_personality;
		float m_flankRouteOffset;
		bool m_routeActive;
		float m_routeTargetX;
		float m_strafeTimer;
		bool m_strafeRight;

		// Animation ranges (inclusive)
		int m_idleStart;
		int m_idleEnd;
		int m_walkStart;
		int m_walkEnd;
		int m_runStart;
		int m_runEnd;
		int m_attackStart;
		int m_attackEnd;
		int m_currentAnimStart;
		int m_currentAnimEnd;
		bool m_currentAnimLoop;
		bool m_facingRight;
		
		// Patrol
		tmath::vectorn<float, 2> m_patrolStart;
		bool m_patrolRight;
	};
}
