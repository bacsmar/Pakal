///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EnemyAI2D.cpp
//
// Purpose: Top-down enemy that chases a visible target - implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/EnemyAI2D.h"

#include <algorithm>
#include <cmath>

namespace Pakal::TopDown
{
	namespace
	{
		const float kPathRefreshSeconds = 0.5f;
		const float kWaypointReached = 0.25f;
	}

	EnemyAI2D::EnemyAI2D(PhysicsWorld2D& world, const b2Vec2& position, const EnemyConfig& config, uintptr_t id) :
		m_config(config),
		m_id(id),
		m_state(EnemyState::Idle),
		m_body(new CharacterController2D(world.native(), position, config.radius, config.bodyCategory, 0xFFFF, id)),
		m_pathTimer(0.0f),
		m_attackTimer(0.0f),
		m_random(static_cast<uint32>(id) * 2654435761u + 1u)
	{
		health.maximum = config.maxHealth;
		health.current = config.maxHealth;
	}

	void EnemyAI2D::set_config(const EnemyConfig& config, bool keepHealthFraction)
	{
		const float fraction = health.maximum > 0.0f ? health.current / health.maximum : 1.0f;
		m_config = config;
		health.maximum = config.maxHealth;
		if (!health.is_dead())
		{
			health.current = keepHealthFraction ? config.maxHealth * fraction : config.maxHealth;
		}
	}

	void EnemyAI2D::apply_damage(float amount)
	{
		health.apply_damage(amount);
		if (health.is_dead())
		{
			m_state = EnemyState::Dead;
			m_scripted = false;
			m_path.clear();
			// The visual corpse may remain; its physics body must not stop movement
			// or consume subsequent weapon raycasts.
			m_body->set_enabled(false);
		}
	}

	void EnemyAI2D::hold_attacks(float seconds)
	{
		m_holdAttacks = std::max(m_holdAttacks, seconds);
	}

	void EnemyAI2D::knock_back(const b2Vec2& from, float speed, float seconds)
	{
		if (is_dead() || seconds <= 0.0f)
		{
			return;
		}
		b2Vec2 away = m_body->get_position() - from;
		if (away.Normalize() < 0.0001f)
		{
			away = -m_facing;
		}
		m_knockDirection = away;
		m_knockSpeed = speed * 100.0f;
		m_knockAccel = 0.0f;
		m_knockTimer = seconds;
		m_windup = 0.0f;
		m_attacking = 0.0f;
		m_scripted = false;
		m_path.clear();
	}

	void EnemyAI2D::end_attack()
	{
		m_scripted = false;
	}

	void EnemyAI2D::teleport(const b2Vec2& position)
	{
		m_body->teleport(position);
		m_path.clear();
		m_pathTimer = 0.0f;
	}

	bool EnemyAI2D::has_line_of_sight(PhysicsWorld2D& world, const b2Vec2& target) const
	{
		const b2Vec2 from = m_body->get_position();
		const RayCastHit hit = world.raycast(from, target, m_config.sightBlockers);
		if (!hit.hit)
		{
			return true;
		}
		// A wall closer than the target blocks the view
		const float distance = (target - from).Length();
		return hit.fraction * distance >= distance - 0.05f;
	}

	float EnemyAI2D::update(float dt, const b2Vec2& target, const NavGrid2D& nav, PhysicsWorld2D& world)
	{
		if (is_dead())
		{
			m_body->set_move(b2Vec2(0.0f, 0.0f), 0.0f);
			m_body->update();
			return 0.0f;
		}

		if (!m_scripted)
		{
			m_attackTimer -= dt;
		}
		m_pathTimer -= dt;
		m_holdAttacks = std::max(0.0f, m_holdAttacks - dt);
		m_attackStarted = false;
		const float speedScale = m_speedScale;
		m_speedScale = 1.0f;

		// Knocked back: it slides away, slowing down (speed in cm/s, time in ms), then stands
		// stunned until the timer ends
		if (m_knockTimer > 0.0f)
		{
			const float ms = dt * 1000.0f;
			if (m_knockSpeed > 0.0f)
			{
				m_knockSpeed = std::max(0.0f, m_knockSpeed + m_knockAccel * ms);
				m_knockAccel += -0.05f * ms;
			}
			m_knockTimer -= dt;
			m_body->set_move(m_knockDirection, m_knockSpeed / 100.0f);
			m_body->update();
			return 0.0f;
		}

		const b2Vec2 position = m_body->get_position();
		const float distance = (target - position).Length();
		const bool sees = distance <= m_config.sightRange && has_line_of_sight(world, target);
		float dealt = 0.0f;

		// The whole attack (its clip) stands still after the hit too
		if (m_attacking > 0.0f && m_windup <= 0.0f)
		{
			m_attacking = std::max(0.0f, m_attacking - dt);
			m_body->set_move(b2Vec2(0.0f, 0.0f), 0.0f);
			m_body->update();
			return 0.0f;
		}

		// A wound-up attack stands still until it lands; it hits only if the target is still in reach
		if (m_windup > 0.0f)
		{
			m_windup -= dt;
			if (m_windup <= 0.0f)
			{
				m_windup = 0.0f;
				bool inArc = true;
				if (m_config.attackArcDegrees > 0.0f && distance > 0.0001f)
				{
					const b2Vec2 toTarget = (1.0f / distance) * (target - position);
					inArc = b2Dot(toTarget, m_attackDirection) >= std::cos(0.5f * m_config.attackArcDegrees * 0.0174533f);
				}
				if (distance <= m_config.attackRange && inArc)
				{
					dealt = m_config.attackDamage;
				}
			}
			m_body->set_move(b2Vec2(0.0f, 0.0f), 0.0f);
			m_body->update();
			return dealt;
		}

		if (!sees && !m_config.pursueTarget)
		{
			m_state = EnemyState::Idle;
			m_path.clear();
			m_body->set_move(b2Vec2(0.0f, 0.0f), 0.0f);
			m_body->update();
			return 0.0f;
		}

		m_state = EnemyState::Chase;
		if (m_pathTimer <= 0.0f || m_path.empty())
		{
			if (!nav.find_path(position, target, m_path))
			{
				m_path.clear(); // no route: go straight for the target
			}
			m_pathTimer = kPathRefreshSeconds;
		}
		while (!m_path.empty() && (m_path.front() - position).Length() < kWaypointReached)
		{
			m_path.erase(m_path.begin());
		}

		// It turns toward the target at its turn speed (at once without one); a scripted attack turns only when it says so
		if (distance > 0.0001f && (!m_scripted || m_attackTurns))
		{
			const b2Vec2 toTarget = (1.0f / distance) * (target - position);
			if (m_config.turnDegreesPerSecond <= 0.0f)
			{
				m_facing = toTarget;
			}
			else
			{
				const float current = std::atan2(m_facing.y, m_facing.x);
				float diff = std::atan2(toTarget.y, toTarget.x) - current;
				while (diff > 3.14159265f) diff -= 6.2831853f;
				while (diff < -3.14159265f) diff += 6.2831853f;
				const float most = m_config.turnDegreesPerSecond * 0.0174533f * dt;
				const float angle = current + std::max(-most, std::min(most, diff));
				m_facing = b2Vec2(std::cos(angle), std::sin(angle));
			}
		}
		const bool facesTarget = distance <= 0.0001f || b2Dot(m_facing, (1.0f / distance) * (target - position)) > 0.94f;
		const float stopDistance = m_config.stopDistance >= 0.0f ? m_config.stopDistance : m_config.attackRange;

		// A scripted attack moves as the game says: along the path toward the target, or straight back from it
		if (m_scripted)
		{
			b2Vec2 way(0.0f, 0.0f);
			float speed = std::fabs(m_attackSpeed); // its own speed (the scale is for chasing)
			if (m_attackSpeed > 0.0f)
			{
				way = (m_path.empty() ? target : m_path.front()) - position;
			}
			else if (m_attackSpeed < 0.0f && distance > 0.0001f)
			{
				way = (-1.0f / distance) * (target - position);
				if (!nav.is_walkable(position + 0.5f * way))
				{
					speed = 0.0f; // backed up against a wall
				}
			}
			m_body->set_move(way, speed);
			m_body->update();
			return 0.0f;
		}

		const float startRange = m_config.attackStartRange >= 0.0f ? m_config.attackStartRange : m_config.attackRange;
		const bool canAttack = distance <= startRange && m_attackTimer <= 0.0f && m_holdAttacks <= 0.0f && sees && facesTarget;
		if (canAttack || (distance <= m_config.attackRange && distance <= stopDistance))
		{
			m_body->set_move(b2Vec2(0.0f, 0.0f), 0.0f);
			float wait = m_config.attackCooldown;
			if (canAttack && m_config.attackCooldownMax > m_config.attackCooldown)
			{
				m_random = m_random * 1664525u + 1013904223u;
				wait += (m_config.attackCooldownMax - m_config.attackCooldown) * static_cast<float>(m_random >> 8) / 16777216.0f;
			}
			if (canAttack && m_config.scriptedAttack)
			{
				// the wait counts from its end (the timer stands still while it lasts)
				m_attackTimer = wait;
				m_attackStarted = true;
				m_scripted = true;
				m_attackSpeed = 0.0f;
				m_attackTurns = false;
				if (distance > 0.0001f)
				{
					m_attackDirection = (1.0f / distance) * (target - position);
				}
			}
			else if (canAttack)
			{
				const float duration = std::max(m_config.attackDuration, m_config.attackWindup);
				m_attackTimer = wait + duration;
				m_attackStarted = true;
				if (distance > 0.0001f)
				{
					m_attackDirection = (1.0f / distance) * (target - position);
				}
				m_attacking = duration - m_config.attackWindup;
				if (m_config.attackWindup > 0.0f)
				{
					m_windup = m_config.attackWindup;
				}
				else
				{
					dealt = m_config.attackDamage;
				}
			}
		}
		else
		{
			const b2Vec2 goal = m_path.empty() ? target : m_path.front();
			m_body->set_move(goal - position, m_config.speed * speedScale);
		}

		m_body->update();
		return dealt;
	}
}
