///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EnemyAI.cpp
// Creation Date: January 2026
//
// Purpose: Enemy AI behavior implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "EnemyAI.h"
#include "Health.h"
#include "Weapon.h"
#include "Entity.h"
#include "components/SpriteComponent.h"
#include "components/PhysicComponent.h"
#include <cmath>
#include <algorithm>

using namespace Pakal;

IMPLEMENT_RTTI(EnemyAI, Component);

EnemyAI::EnemyAI()
	: m_state(AIState::PATROL)
	, m_player(nullptr)
	, m_physics(nullptr)
	, m_sprite(nullptr)
	, m_weapon(nullptr)
	, m_health(nullptr)
	, m_currentPatrolPoint(0)
	, m_patrolWaitTime(1.0f)
	, m_patrolWaitTimer(0.0f)
	, m_chaseRange(500.0f)
	, m_attackRange(300.0f)
	, m_losePlayerRange(700.0f)
	, m_moveSpeed(100.0f)
	, m_fireRate(1.0f)
	, m_fireTimer(0.0f)
	, m_stateTime(0.0f)
{
}

void EnemyAI::initialize()
{
	Entity* entity = get_parent_entity();
	if (!entity)
		return;

	// Get component references
	m_sprite = entity->get_component<SpriteComponent>();
	m_weapon = entity->get_component<Weapon>();
	m_health = entity->get_component<Health>();
	m_physics = entity->get_component<PhysicComponent>();

	// Setup death callback
	if (m_health)
	{
		m_health->on_death += [this]()
		{
			change_state(AIState::DEAD);
		};
	}

	// Setup default patrol if none specified
	if (m_patrolPoints.empty() && m_sprite)
	{
		auto pos = m_sprite->get_position();
		m_patrolPoints.push_back(tmath::vector2df(pos.x - 100.0f, pos.y));
		m_patrolPoints.push_back(tmath::vector2df(pos.x + 100.0f, pos.y));
	}
}

void EnemyAI::update(float deltaTime)
{
	if (m_state == AIState::DEAD)
	{
		// TODO: Play death animation, then destroy entity
		return;
	}

	// Check if still alive
	if (m_health && !m_health->is_alive())
	{
		change_state(AIState::DEAD);
		return;
	}

	// Update weapon cooldown
	if (m_weapon)
	{
		m_weapon->update(deltaTime);
	}

	// Update fire timer
	if (m_fireTimer > 0.0f)
	{
		m_fireTimer -= deltaTime;
	}

	m_stateTime += deltaTime;

	// State machine
	switch (m_state)
	{
	case AIState::PATROL:
		update_patrol(deltaTime);
		break;
	case AIState::CHASE:
		update_chase(deltaTime);
		break;
	case AIState::ATTACK:
		update_attack(deltaTime);
		break;
	case AIState::RETREAT:
		update_retreat(deltaTime);
		break;
	default:
		break;
	}
}

void EnemyAI::set_patrol_points(const std::vector<tmath::vector2df>& points)
{
	m_patrolPoints = points;
	m_currentPatrolPoint = 0;
}

void EnemyAI::update_patrol(float deltaTime)
{
	// Check if player is in range
	if (can_see_player())
	{
		float distance = get_distance_to_player();
		if (distance < m_chaseRange)
		{
			change_state(AIState::CHASE);
			return;
		}
	}

	// Patrol between points
	if (m_patrolPoints.empty())
		return;

	// Wait at patrol point
	if (m_patrolWaitTimer > 0.0f)
	{
		m_patrolWaitTimer -= deltaTime;
		return;
	}

	// Get current patrol target
	tmath::vector2df target = m_patrolPoints[m_currentPatrolPoint];

	// Move towards patrol point
	move_towards(target, deltaTime);

	// Check if reached patrol point
	if (!m_sprite)
		return;

	auto pos = m_sprite->get_position();
	float distToTarget = std::sqrt(
		(target.x - pos.x) * (target.x - pos.x) +
		(target.y - pos.y) * (target.y - pos.y)
	);

	if (distToTarget < 10.0f)
	{
		// Reached patrol point, move to next
		m_currentPatrolPoint = (m_currentPatrolPoint + 1) % m_patrolPoints.size();
		m_patrolWaitTimer = m_patrolWaitTime;
	}
}

void EnemyAI::update_chase(float deltaTime)
{
	if (!m_player)
	{
		change_state(AIState::PATROL);
		return;
	}

	float distance = get_distance_to_player();

	// Lost player
	if (!can_see_player() || distance > m_losePlayerRange)
	{
		change_state(AIState::PATROL);
		return;
	}

	// Close enough to attack
	if (distance < m_attackRange)
	{
		change_state(AIState::ATTACK);
		return;
	}

	// Chase player
	auto playerSprite = m_player->get_component<SpriteComponent>();
	if (playerSprite)
	{
		auto playerPos = playerSprite->get_position();
		move_towards(tmath::vector2df(playerPos.x, playerPos.y), deltaTime);
	}
}

void EnemyAI::update_attack(float deltaTime)
{
	if (!m_player)
	{
		change_state(AIState::PATROL);
		return;
	}

	float distance = get_distance_to_player();

	// Player too far
	if (distance > m_attackRange * 1.2f)
	{
		change_state(AIState::CHASE);
		return;
	}

	// Player very close - could add retreat behavior
	// if (distance < m_attackRange * 0.3f)
	// {
	//     change_state(AIState::RETREAT);
	//     return;
	// }

	// Shoot at player
	shoot_at_player();

	// Face player
	if (m_sprite && m_player)
	{
		auto playerSprite = m_player->get_component<SpriteComponent>();
		if (playerSprite)
		{
			auto myPos = m_sprite->get_position();
			auto playerPos = playerSprite->get_position();
			bool playerOnRight = playerPos.x > myPos.x;
			m_sprite->set_flipped(!playerOnRight);
		}
	}
}

void EnemyAI::update_retreat(float deltaTime)
{
	// Simple retreat behavior - move away from player
	if (!m_player || !m_sprite)
	{
		change_state(AIState::PATROL);
		return;
	}

	auto playerSprite = m_player->get_component<SpriteComponent>();
	if (!playerSprite)
	{
		change_state(AIState::PATROL);
		return;
	}

	// Move away from player
	auto myPos = m_sprite->get_position();
	auto playerPos = playerSprite->get_position();

	tmath::vector2df awayDirection(
		myPos.x - playerPos.x,
		myPos.y - playerPos.y
	);

	float length = std::sqrt(awayDirection.x * awayDirection.x + awayDirection.y * awayDirection.y);
	if (length > 0.0f)
	{
		awayDirection.x /= length;
		awayDirection.y /= length;
	}

	tmath::vector2df retreatTarget(
		myPos.x + awayDirection.x * 100.0f,
		myPos.y + awayDirection.y * 100.0f
	);

	move_towards(retreatTarget, deltaTime);

	// Return to attack when far enough
	float distance = get_distance_to_player();
	if (distance > m_attackRange)
	{
		change_state(AIState::ATTACK);
	}
}

bool EnemyAI::can_see_player()
{
	if (!m_player || !m_sprite)
		return false;

	// Simplified line of sight check
	// TODO: Implement proper ray-cast check for obstacles
	
	auto playerSprite = m_player->get_component<SpriteComponent>();
	if (!playerSprite)
		return false;

	// For now, just check if in range
	return get_distance_to_player() < m_chaseRange;
}

float EnemyAI::get_distance_to_player()
{
	if (!m_player || !m_sprite)
		return 99999.0f;

	auto playerSprite = m_player->get_component<SpriteComponent>();
	if (!playerSprite)
		return 99999.0f;

	auto myPos = m_sprite->get_position();
	auto playerPos = playerSprite->get_position();

	float dx = playerPos.x - myPos.x;
	float dy = playerPos.y - myPos.y;

	return std::sqrt(dx * dx + dy * dy);
}

void EnemyAI::shoot_at_player()
{
	if (!m_weapon || !m_player || !m_sprite)
		return;

	if (!m_weapon->can_fire())
		return;

	auto playerSprite = m_player->get_component<SpriteComponent>();
	if (!playerSprite)
		return;

	// Calculate direction to player
	auto myPos = m_sprite->get_position();
	auto playerPos = playerSprite->get_position();

	tmath::vector2df direction(
		playerPos.x - myPos.x,
		playerPos.y - myPos.y
	);

	// Fire weapon
	m_weapon->fire_at_direction(direction);
}

void EnemyAI::move_towards(const tmath::vector2df& target, float deltaTime)
{
	if (!m_sprite)
		return;

	auto pos = m_sprite->get_position();

	// Calculate direction
	float dx = target.x - pos.x;
	float dy = target.y - pos.y;
	float distance = std::sqrt(dx * dx + dy * dy);

	if (distance < 1.0f)
		return;

	// Normalize and scale by speed
	dx = (dx / distance) * m_moveSpeed * deltaTime;
	dy = (dy / distance) * m_moveSpeed * deltaTime;

	// Apply movement
	if (m_physics)
	{
		// TODO: Apply velocity to physics body
		// m_physics->set_linear_velocity({dx / deltaTime, dy / deltaTime});
	}
	else
	{
		// Direct position update
		pos.x += dx;
		pos.y += dy;
		m_sprite->set_position(pos);
	}

	// Update facing direction
	if (dx != 0.0f)
	{
		m_sprite->set_flipped(dx < 0.0f);
	}
}

void EnemyAI::change_state(AIState newState)
{
	if (m_state == newState)
		return;

	m_state = newState;
	m_stateTime = 0.0f;

	// State-specific initialization
	switch (newState)
	{
	case AIState::PATROL:
		// Reset patrol
		break;
	case AIState::CHASE:
		// Start chasing
		break;
	case AIState::ATTACK:
		// Begin attack
		break;
	case AIState::RETREAT:
		// Start retreating
		break;
	case AIState::DEAD:
		// Death logic
		break;
	}
}
