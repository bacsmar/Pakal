///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Projectile.cpp
// Purpose: Projectile component for game entities - Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Projectile.h"
#include "Entity.h"
#include "LogMgr.h"
#include "components/SpriteComponent2D.h"
#include "components/SpritePhysicsComponent.h"
#include "Components/Health.h"

namespace Pakal
{
	Projectile::Projectile() :
		m_velocity(0.0f, 0.0f),
		m_damage(10.0f),
		m_faction(0),  // Default to player faction
		m_lifetime(5.0f),
		m_physics(nullptr),
		m_sprite(nullptr)
	{
	}
	
	void Projectile::initialize()
	{
		Entity* parent = get_parent_entity();
		if (!parent)
			return;
		
		// Get component references
		m_physics = parent->get_component<SpritePhysicsComponent>();
		m_sprite = parent->get_component<SpriteComponent2D>();
		
		if (!m_physics || !m_sprite)
		{
			LOG_WARNING("Projectile: Missing physics or sprite component");
		}
	}
	
	void Projectile::update(float deltaTime)
	{
		if (!is_alive())
			return;
		
		update_lifetime(deltaTime);
		update_physics(deltaTime);
	}
	
	void Projectile::update_physics(float deltaTime)
	{
		if (!m_physics)
			return;
		
		// Update position based on velocity
		auto currentPos = m_physics->get_position();
		auto newPos = currentPos + tmath::vector3df(m_velocity.x, m_velocity.y, 0.0f) * deltaTime;
		m_physics->set_position(newPos);
	}
	
	void Projectile::update_lifetime(float deltaTime)
	{
		m_lifetime -= deltaTime;
	}
	
	void Projectile::on_collision(Projectile* other)
	{
		// Projectiles destroy each other on collision
		m_lifetime = 0.0f;
	}
	
	void Projectile::on_collision(Entity* other)
	{
		// Check if the other entity has a Health component
		auto* health = other->get_component<Health>();
		if (health)
		{
			// Apply damage to the entity
			health->take_damage(m_damage);
			
			// If this is a player projectile hitting an enemy, or enemy projectile hitting player
			// we should destroy the projectile
			LOG_INFO("[Projectile] Hit entity with faction %d, dealt %f damage", m_faction, m_damage);
		}
		
		// Destroy projectile on any collision
		m_lifetime = 0.0f;
	}
}