///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EnemyAI.cpp
// Purpose: Enemy AI behavior component - Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "EnemyAI.h"
#include "Health.h"
#include "Weapon.h"
#include "Entity.h"
#include "EntityManager.h"
#include "GenericEntity.h"
#include "LogMgr.h"
#include "components/SpriteComponent2D.h"
#include "components/SpritePhysicsComponent.h"
#include <cmath>

namespace Pakal
{
	EnemyAI::EnemyAI() :
		m_state(PATROL),
		m_playerHandle(),
		m_physics(nullptr),
		m_sprite(nullptr),
		m_weapon(nullptr),
		m_health(nullptr),
		m_patrolRange(5.0f),
		m_chaseRange(10.0f),
		m_attackRange(8.0f),
		m_fireRate(1.0f),
		m_fireTimer(0.0f),
		m_personality(Balanced),
		m_flankRouteOffset(2.5f),
		m_routeActive(false),
		m_routeTargetX(0.0f),
		m_strafeTimer(0.0f),
		m_strafeRight(true),
		m_idleStart(0),
		m_idleEnd(2),
		m_walkStart(3),
		m_walkEnd(5),
		m_runStart(6),
		m_runEnd(8),
		m_attackStart(15),
		m_attackEnd(17),
		m_currentAnimStart(-1),
		m_currentAnimEnd(-1),
		m_currentAnimLoop(true),
		m_facingRight(true),
		m_patrolStart(0.0f, 0.0f),
		m_patrolRight(true)
	{
	}

	void EnemyAI::set_idle_animation(int startFrame, int endFrame)
	{
		m_idleStart = startFrame;
		m_idleEnd = endFrame;
	}

	void EnemyAI::set_walk_animation(int startFrame, int endFrame)
	{
		m_walkStart = startFrame;
		m_walkEnd = endFrame;
	}

	void EnemyAI::set_run_animation(int startFrame, int endFrame)
	{
		m_runStart = startFrame;
		m_runEnd = endFrame;
	}

	void EnemyAI::set_attack_animation(int startFrame, int endFrame)
	{
		m_attackStart = startFrame;
		m_attackEnd = endFrame;
	}

	void EnemyAI::apply_animation(int startFrame, int endFrame, bool loop)
	{
		if (!m_sprite)
			return;

		if (startFrame == m_currentAnimStart && endFrame == m_currentAnimEnd && loop == m_currentAnimLoop)
			return;

		m_sprite->play_animation(startFrame, endFrame, loop);
		m_currentAnimStart = startFrame;
		m_currentAnimEnd = endFrame;
		m_currentAnimLoop = loop;
	}

	void EnemyAI::update_facing_from_direction(bool facingRight)
	{
		m_facingRight = facingRight;
		if (m_sprite)
		{
			// Atlas art is authored facing right by default.
			m_sprite->set_flip(!m_facingRight, false);
		}
	}

	void EnemyAI::update_facing_from_dx(float dx)
	{
		const float deadZone = 0.05f;
		if (dx > deadZone)
		{
			update_facing_from_direction(true);
		}
		else if (dx < -deadZone)
		{
			update_facing_from_direction(false);
		}
	}

	float EnemyAI::choose_chase_target_x(float enemyX, float playerX, float distanceToPlayer)
	{
		if (m_personality != Aggressive)
		{
			m_routeActive = false;
			return playerX;
		}

		const float routeArrivalThreshold = 0.7f;
		const float routeEngageDistance = m_attackRange + 4.0f;

		if (!m_routeActive && distanceToPlayer > routeEngageDistance)
		{
			const float side = (playerX - enemyX >= 0.0f) ? 1.0f : -1.0f;
			// Aggressive enemies try to cross to the opposite side before closing in.
			m_routeTargetX = playerX + side * m_flankRouteOffset;
			m_routeActive = true;
		}

		if (m_routeActive)
		{
			if (std::fabs(m_routeTargetX - enemyX) <= routeArrivalThreshold)
			{
				m_routeActive = false;
				return playerX;
			}
			return m_routeTargetX;
		}

		return playerX;
	}

	void EnemyAI::set_player_entity(Entity* player)
	{
		set_player_entity(player ? player->get_handle() : EntityHandle{});
	}

	void EnemyAI::set_player_entity(EntityHandle player)
	{
		m_playerHandle = player;
	}

	Entity* EnemyAI::resolve_player_entity() const
	{
		auto* parent = get_parent_entity();
		if (!parent)
		{
			return nullptr;
		}

		auto* entityManager = parent->entity_manager();
		if (!entityManager)
		{
			return nullptr;
		}

		return entityManager->resolve(m_playerHandle);
	}
	
	void EnemyAI::initialize()
	{
		Entity* parent = get_parent_entity();
		if (!parent)
			return;

		auto* genericParent = dynamic_cast<GenericEntity*>(parent);
		if (!genericParent)
			return;
		
		// Get component references
		m_physics = genericParent->get_component<SpritePhysicsComponent>();
		m_sprite = genericParent->get_component<SpriteComponent2D>();
		m_weapon = genericParent->get_component<Weapon>();
		m_health = genericParent->get_component<Health>();
		
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

		update_facing_from_direction(true);
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
		(void)deltaTime;
		if (!m_physics)
			return;

		apply_animation(m_walkStart, m_walkEnd, true);
		
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
		float xVel = m_patrolRight ? patrolSpeed : -patrolSpeed;
		update_facing_from_direction(m_patrolRight);
		m_physics->set_horizontal_velocity(xVel);
		
		// TODO: Advanced patrol behaviors (when ADVANCED_ENEMY_AI is defined)
		#ifdef ADVANCED_ENEMY_AI
		check_and_jump_obstacles();
		perform_diagonal_patrol();
		#endif
	}
	
	void EnemyAI::update_chase(float deltaTime)
	{
		(void)deltaTime;
		auto* player = resolve_player_entity();
		if (!player || !m_physics)
		{
			m_state = PATROL;
			return;
		}

		apply_animation(m_runStart, m_runEnd, true);
		
		// Get player's physics component to calculate distance
		auto* playerPhysics = player->get_component<SpritePhysicsComponent>();
		if (!playerPhysics)
		{
			m_state = PATROL;
			return;
		}
		
		// Calculate distance to player
		auto enemyPos = m_physics->get_position();
		auto playerPos = playerPhysics->get_position();
		
		float dx = playerPos.x - enemyPos.x;
		float dy = playerPos.y - enemyPos.y;
		float distanceToPlayer = std::sqrt(dx * dx + dy * dy);
		const float effectiveAttackRange = (m_personality == Cautious) ? (m_attackRange * 0.75f) : m_attackRange;
		
		if (distanceToPlayer > m_chaseRange)
		{
			// Player is too far, go back to patrol
			m_state = PATROL;
			return;
		}
		
		if (distanceToPlayer <= effectiveAttackRange && can_see_player())
		{
			// Player is in attack range and visible
			m_state = ATTACK;
			return;
		}
		
		float chaseTargetX = choose_chase_target_x(enemyPos.x, playerPos.x, distanceToPlayer);
		float routeDx = chaseTargetX - enemyPos.x;
		update_facing_from_dx(routeDx);

		// Move towards target according to personality.
		float chaseSpeed = 4.0f;
		if (m_personality == Aggressive)
		{
			chaseSpeed = 5.0f;
		}
		else if (m_personality == Sentinel)
		{
			chaseSpeed = 2.8f;
		}

		float xVel = (routeDx >= 0.0f) ? chaseSpeed : -chaseSpeed;
		m_physics->set_horizontal_velocity(xVel);

		// Keep pressure while chasing so enemies visibly shoot before point-blank range.
		shoot_at_player();
		
		// TODO: Advanced chase behaviors (when ADVANCED_ENEMY_AI is defined)
		#ifdef ADVANCED_ENEMY_AI
		check_and_jump_obstacles();
		coordinate_with_group();
		#endif
	}
	
	void EnemyAI::update_attack(float deltaTime)
	{
		auto* player = resolve_player_entity();
		if (!player || !m_physics)
		{
			m_state = PATROL;
			return;
		}

		apply_animation(m_attackStart, m_attackEnd, true);
		
		// Get player's physics component
		auto* playerPhysics = player->get_component<SpritePhysicsComponent>();
		if (!playerPhysics)
		{
			m_state = PATROL;
			return;
		}
		
		// Calculate distance to player
		auto enemyPos = m_physics->get_position();
		auto playerPos = playerPhysics->get_position();
		
		float dx = playerPos.x - enemyPos.x;
		float dy = playerPos.y - enemyPos.y;
		float distanceToPlayer = std::sqrt(dx * dx + dy * dy);
		update_facing_from_dx(dx);
		const float holdDistance = (m_personality == Cautious) ? (m_attackRange * 0.6f) : 0.0f;
		
		if (distanceToPlayer > m_attackRange || !can_see_player())
		{
			// Player moved out of range or out of sight
			m_state = CHASE;
			return;
		}
		
		// Keep tactical spacing based on personality.
		float xVel = 0.0f;
		if (m_personality == Aggressive)
		{
			const float pushDistance = m_attackRange * 0.4f;
			if (distanceToPlayer > pushDistance)
			{
				xVel = (dx >= 0.0f) ? 2.3f : -2.3f;
			}
		}
		else if (m_personality == Cautious)
		{
			if (distanceToPlayer < holdDistance)
			{
				xVel = (dx >= 0.0f) ? -2.0f : 2.0f;
			}
			else
			{
				m_strafeTimer -= deltaTime;
				if (m_strafeTimer <= 0.0f)
				{
					m_strafeTimer = 0.8f;
					m_strafeRight = !m_strafeRight;
				}
				xVel = m_strafeRight ? 1.6f : -1.6f;
			}
		}
		else if (m_personality == Sentinel)
		{
			const float homeDx = m_patrolStart.x - enemyPos.x;
			if (std::fabs(homeDx) > 1.5f)
			{
				xVel = (homeDx >= 0.0f) ? 1.8f : -1.8f;
			}
		}

		m_physics->set_horizontal_velocity(xVel);
		
		// Shoot at player
		shoot_at_player();
		
		// TODO: Advanced attack behaviors (when ADVANCED_ENEMY_AI is defined)
		#ifdef ADVANCED_ENEMY_AI
		manage_ammo();
		#endif
	}
	
	bool EnemyAI::can_see_player()
	{
		auto* player = resolve_player_entity();
		if (!player || !m_physics)
			return false;
		
		// Get player's physics component
		auto* playerPhysics = player->get_component<SpritePhysicsComponent>();
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
		float sightRange = m_chaseRange;
		if (m_personality == Aggressive)
		{
			sightRange *= 1.15f;
		}
		else if (m_personality == Sentinel)
		{
			sightRange *= 0.85f;
		}

		if (distance <= sightRange)
		{
			return true;
		}
		
		return false;
	}
	
	void EnemyAI::shoot_at_player()
	{
		auto* player = resolve_player_entity();
		if (!m_weapon || !player || m_fireTimer > 0.0f || !m_physics)
			return;
		
		// Get player's physics component
		auto* playerPhysics = player->get_component<SpritePhysicsComponent>();
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
