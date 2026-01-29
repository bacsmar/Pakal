///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Weapon.cpp
// Creation Date: January 2026
//
// Purpose: Weapon system implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Weapon.h"
#include "Entity.h"
#include "EntityManager.h"
#include "components/SpriteComponent.h"
#include <cmath>

using namespace Pakal;

IMPLEMENT_RTTI(Weapon, Component);
IMPLEMENT_RTTI(Projectile, Component);

//////////////////////////////////////////////////////////////////////////
// Weapon Implementation
//////////////////////////////////////////////////////////////////////////

Weapon::Weapon()
	: m_fireRate(2.0f)  // 2 shots per second
	, m_projectileSpeed(500.0f)
	, m_damage(10.0f)
	, m_firePointOffset(0.0f, 0.0f)
	, m_ownerTag("player")
	, m_fireTimer(0.0f)
	, m_lastDirection(1.0f, 0.0f)
	, m_entityManager(nullptr)
{
}

void Weapon::update(float deltaTime)
{
	if (m_fireTimer > 0.0f)
	{
		m_fireTimer -= deltaTime;
	}
}

void Weapon::fire()
{
	if (!can_fire())
		return;

	spawn_projectile(m_lastDirection);
	m_fireTimer = 1.0f / m_fireRate;
}

void Weapon::fire_at_direction(const tmath::vector2df& direction)
{
	if (!can_fire())
		return;

	// Normalize direction
	float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
	if (length > 0.0f)
	{
		m_lastDirection = tmath::vector2df(direction.x / length, direction.y / length);
	}

	spawn_projectile(m_lastDirection);
	m_fireTimer = 1.0f / m_fireRate;
}

void Weapon::fire_at_target(const tmath::vector2df& target)
{
	if (!can_fire())
		return;

	Entity* owner = get_parent_entity();
	if (!owner)
		return;

	// Get owner position (simplified - assumes we can get position somehow)
	// In a real implementation, this would get position from transform or sprite component
	tmath::vector2df ownerPos(0.0f, 0.0f);

	tmath::vector2df direction = tmath::vector2df(
		target.x - ownerPos.x,
		target.y - ownerPos.y
	);

	fire_at_direction(direction);
}

void Weapon::spawn_projectile(const tmath::vector2df& direction)
{
	// This is a simplified version
	// In a real implementation, this would create a projectile entity
	// with proper components (sprite, physics, projectile behavior)

	// TODO: Implement when EntityManager is available
	// For now, this is a stub
}

//////////////////////////////////////////////////////////////////////////
// Projectile Implementation
//////////////////////////////////////////////////////////////////////////

Projectile::Projectile()
	: m_direction(1.0f, 0.0f)
	, m_speed(500.0f)
	, m_damage(10.0f)
	, m_lifetime(3.0f)
	, m_timeAlive(0.0f)
	, m_ownerTag("player")
{
}

void Projectile::update(float deltaTime)
{
	m_timeAlive += deltaTime;

	// Check lifetime
	if (m_timeAlive >= m_lifetime)
	{
		// Destroy this entity
		Entity* owner = get_parent_entity();
		if (owner)
		{
			// TODO: Mark for destruction
			// owner->destroy();
		}
		return;
	}

	// Move projectile
	Entity* owner = get_parent_entity();
	if (!owner)
		return;

	// Get sprite component to update position
	auto sprite = owner->get_component<SpriteComponent>();
	if (sprite)
	{
		auto pos = sprite->get_position();
		pos.x += m_direction.x * m_speed * deltaTime;
		pos.y += m_direction.y * m_speed * deltaTime;
		sprite->set_position(pos);
	}

	// TODO: Check for collisions
	// This would be handled by the collision system
}

void Projectile::set_direction(const tmath::vector2df& direction)
{
	// Normalize
	float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
	if (length > 0.0f)
	{
		m_direction = tmath::vector2df(direction.x / length, direction.y / length);
	}
	else
	{
		m_direction = tmath::vector2df(1.0f, 0.0f);
	}
}

void Projectile::on_collision(Entity* other)
{
	// Handle collision
	// This would be called by the collision system

	// TODO: Deal damage if it's a valid target
	// TODO: Destroy projectile
}
