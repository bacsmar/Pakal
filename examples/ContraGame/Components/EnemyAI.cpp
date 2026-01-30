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
		// TODO: Uncomment when components are linked
		// m_physics = parent->get_component<SpritebodyComponent_Box2D>();
		// m_sprite = parent->get_component<SpriteComponent_Bgfx>();
		m_weapon = parent->get_component<Weapon>();
		m_health = parent->get_component<Health>();
		
		// Set patrol start position
		// TODO: Get from physics component
		// m_patrolStart = m_physics->get_position();
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
		// Check if player is in range
		if (can_see_player())
		{
			m_state = CHASE;
			return;
		}
		
		// Simple patrol logic
		// TODO: Implement when physics component is available
		// Move back and forth within patrol range
	}
	
	void EnemyAI::update_chase(float deltaTime)
	{
		if (!m_player)
			return;
		
		// Check distance to player
		// TODO: Calculate actual distance using physics positions
		float distanceToPlayer = 10.0f; // Placeholder
		
		if (distanceToPlayer > m_chaseRange)
		{
			// Player is too far, go back to patrol
			m_state = PATROL;
			return;
		}
		
		if (distanceToPlayer <= m_attackRange)
		{
			// Player is in attack range
			m_state = ATTACK;
			return;
		}
		
		// Move towards player
		// TODO: Implement when physics component is available
	}
	
	void EnemyAI::update_attack(float deltaTime)
	{
		if (!m_player)
			return;
		
		// Check distance to player
		// TODO: Calculate actual distance using physics positions
		float distanceToPlayer = 8.0f; // Placeholder
		
		if (distanceToPlayer > m_attackRange)
		{
			// Player moved out of range
			m_state = CHASE;
			return;
		}
		
		// Shoot at player
		shoot_at_player();
	}
	
	bool EnemyAI::can_see_player()
	{
		if (!m_player || !m_physics)
			return false;
		
		// TODO: Implement line-of-sight check
		// For now, just check distance
		// Calculate distance between enemy and player
		// Return true if within chase range
		
		return false; // Placeholder
	}
	
	void EnemyAI::shoot_at_player()
	{
		if (!m_weapon || !m_player || m_fireTimer > 0.0f)
			return;
		
		// Calculate direction to player
		// TODO: Get actual positions from physics components
		tmath::vectorn<float, 2> direction(1.0f, 0.0f); // Placeholder
		
		// Fire weapon
		m_weapon->fire(direction);
		m_fireTimer = m_fireRate;
	}
}
