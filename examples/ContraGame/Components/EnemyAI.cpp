///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EnemyAI.cpp
// Purpose: Enemy AI behavior component - Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "EnemyAI.h"
#include "Health.h"
#include "Weapon.h"
#include "Entity.h"
#include "LogMgr.h"
#include "components/SpriteComponent2D.h"
#include "components/SpritePhysicsComponent.h"
#include <cmath>

namespace Pakal
{
	EnemyAI::EnemyAI() :
		m_state(PATROL),
		m_player(nullptr),
		m_physics(nullptr),
		m_sprite(nullptr),
		m_weapon(nullptr),
		m_health(nullptr),
		m_patrolRange(5.0f),
		m_chaseRange(10.0f),
		m_attackRange(8.0f),
		m_fireRate(1.0f),
		m_fireTimer(0.0f),
		m_patrolStart(0.0f, 0.0f),
		m_patrolRight(true)
	{
	}
	
	void EnemyAI::initialize()
	{
		Entity* parent = get_parent_entity();
		if (!parent)
			return;
		
		// Get component references
		m_physics = parent->get_component<SpritePhysicsComponent>();
		m_sprite = parent->get_component<SpriteComponent2D>();
		m_weapon = parent->get_component<Weapon>();
		m_health = parent->get_component<Health>();
		
		if (!m_physics || !m_sprite)
		{
			LOG_WARNING("EnemyAI: Missing physics or sprite component");
		}
		
		// Set patrol start position
		if (m_physics)
		{
			auto pos = m_physics->get_position();
			m_patrolStart = tmath::vectorn<float, 2>(pos.x, pos.y);
		}
	}
	
	void EnemyAI::update(float deltaTime)
	{
		if (!m_health || !m_health->is_alive())
		{
			m_state = DEAD;
			return;
		}
		
		// Update fire timer
		if (m_fireTimer > 0.0f)
		{
			m_fireTimer -= deltaTime;
		}
		
		// Update AI based on current state
		switch (m_state)
		{
		case PATROL:
			update_patrol(deltaTime);
			break;
		case CHASE:
			update_chase(deltaTime);
			break;
		case ATTACK:
			update_attack(deltaTime);
			break;
		case DEAD:
			// Do nothing
			break;
		}
	}
	
	void EnemyAI::update_patrol(float deltaTime)
	{
		if (!m_physics)
			return;
		
		// Check if player is in range
		if (can_see_player())
		{
			m_state = CHASE;
			return;
		}
		
		// Simple patrol logic: move back and forth within patrol range
		auto currentPos = m_physics->get_position();
		float distanceFromStart = currentPos.x - m_patrolStart.x;
		
		// Change direction if we've reached patrol range limits
		if (distanceFromStart > m_patrolRange)
		{
			m_patrolRight = false;
		}
		else if (distanceFromStart < -m_patrolRange)
		{
			m_patrolRight = true;
		}
		
		// Move in current patrol direction
		float patrolSpeed = 2.0f; // Slower than chase speed
		auto currentVel = m_physics->get_lineal_velocity();
		float xVel = m_patrolRight ? patrolSpeed : -patrolSpeed;
		m_physics->set_lineal_velocity(tmath::vector2df(xVel, currentVel.y));
		
		// TODO: Advanced patrol behaviors (when ADVANCED_ENEMY_AI is defined)
		#ifdef ADVANCED_ENEMY_AI
		check_and_jump_obstacles();
		perform_diagonal_patrol();
		#endif
	}
	
	void EnemyAI::update_chase(float deltaTime)
	{
		if (!m_player || !m_physics)
			return;
		
		// Get player's physics component to calculate distance
		auto* playerPhysics = m_player->get_component<SpritePhysicsComponent>();
		if (!playerPhysics)
			return;
		
		// Calculate distance to player
		auto enemyPos = m_physics->get_position();
		auto playerPos = playerPhysics->get_position();
		
		float dx = playerPos.x - enemyPos.x;
		float dy = playerPos.y - enemyPos.y;
		float distanceToPlayer = std::sqrt(dx * dx + dy * dy);
		
		if (distanceToPlayer > m_chaseRange)
		{
			// Player is too far, go back to patrol
			m_state = PATROL;
			return;
		}
		
		if (distanceToPlayer <= m_attackRange && can_see_player())
		{
			// Player is in attack range and visible
			m_state = ATTACK;
			return;
		}
		
		// Move towards player
		float chaseSpeed = 4.0f; // Faster than patrol
		auto currentVel = m_physics->get_lineal_velocity();
		float xVel = (dx > 0) ? chaseSpeed : -chaseSpeed;
		m_physics->set_lineal_velocity(tmath::vector2df(xVel, currentVel.y));
		
		// TODO: Advanced chase behaviors (when ADVANCED_ENEMY_AI is defined)
		#ifdef ADVANCED_ENEMY_AI
		check_and_jump_obstacles();
		coordinate_with_group();
		#endif
	}
	
	void EnemyAI::update_attack(float deltaTime)
	{
		if (!m_player || !m_physics)
			return;
		
		// Get player's physics component
		auto* playerPhysics = m_player->get_component<SpritePhysicsComponent>();
		if (!playerPhysics)
			return;
		
		// Calculate distance to player
		auto enemyPos = m_physics->get_position();
		auto playerPos = playerPhysics->get_position();
		
		float dx = playerPos.x - enemyPos.x;
		float dy = playerPos.y - enemyPos.y;
		float distanceToPlayer = std::sqrt(dx * dx + dy * dy);
		
		if (distanceToPlayer > m_attackRange || !can_see_player())
		{
			// Player moved out of range or out of sight
			m_state = CHASE;
			return;
		}
		
		// Stop moving when attacking
		auto currentVel = m_physics->get_lineal_velocity();
		m_physics->set_lineal_velocity(tmath::vector2df(0.0f, currentVel.y));
		
		// Shoot at player
		shoot_at_player();
		
		// TODO: Advanced attack behaviors (when ADVANCED_ENEMY_AI is defined)
		#ifdef ADVANCED_ENEMY_AI
		manage_ammo();
		#endif
	}
	
	bool EnemyAI::can_see_player()
	{
		if (!m_player || !m_physics)
			return false;
		
		// Get player's physics component
		auto* playerPhysics = m_player->get_component<SpritePhysicsComponent>();
		if (!playerPhysics)
			return false;
		
		// Calculate distance between enemy and player
		auto enemyPos = m_physics->get_position();
		auto playerPos = playerPhysics->get_position();
		
		float dx = playerPos.x - enemyPos.x;
		float dy = playerPos.y - enemyPos.y;
		float distance = std::sqrt(dx * dx + dy * dy);
		
		// Simple line-of-sight: if within chase range, can see player
		// TODO: Use Box2D raycasting for proper line-of-sight checking (detect obstacles)
		if (distance <= m_chaseRange)
		{
			return true;
		}
		
		return false;
	}
	
	void EnemyAI::shoot_at_player()
	{
		if (!m_weapon || !m_player || m_fireTimer > 0.0f || !m_physics)
			return;
		
		// Get player's physics component
		auto* playerPhysics = m_player->get_component<SpritePhysicsComponent>();
		if (!playerPhysics)
			return;
		
		// Calculate direction to player
		auto enemyPos = m_physics->get_position();
		auto playerPos = playerPhysics->get_position();
		
		float dx = playerPos.x - enemyPos.x;
		float dy = playerPos.y - enemyPos.y;
		float length = std::sqrt(dx * dx + dy * dy);
		
		// Normalize direction vector
		if (length > 0.001f)
		{
			dx /= length;
			dy /= length;
		}
		
		tmath::vectorn<float, 2> direction(dx, dy);
		
		// Fire weapon
		m_weapon->fire(direction);
		m_fireTimer = m_fireRate;
	}
	
	// Advanced behaviors (placeholders for future implementation)
	#ifdef ADVANCED_ENEMY_AI
	void EnemyAI::check_and_jump_obstacles()
	{
		// TODO: Use Box2D raycasting to detect obstacles ahead
		// If obstacle detected, apply upward impulse to jump over it
		// Example: m_physics->apply_impulse(0.0f, 8.0f);
	}
	
	void EnemyAI::perform_diagonal_patrol()
	{
		// TODO: Add vertical movement to patrol (diagonal patterns)
		// Useful for flying enemies or climbing enemies
		// Example: alternate between moving up/down while patrolling
	}
	
	void EnemyAI::manage_ammo()
	{
		// TODO: Track ammunition count
		// If low on ammo, switch to retreat behavior or melee attack
		// Could add reload mechanic with vulnerable state
	}
	
	void EnemyAI::coordinate_with_group()
	{
		// TODO: Communication with nearby enemies
		// Share player position, coordinate flanking maneuvers
		// Form defensive positions or attack patterns
		// Requires enemy group management system
	}
	#endif
}
