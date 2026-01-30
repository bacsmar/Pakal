///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Weapon.cpp
// Purpose: Weapon/shooting component for game entities - Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Weapon.h"
#include "EntityManager.h"
#include "Entity.h"
#include "LogMgr.h"

namespace Pakal
{
	Weapon::Weapon() :
		m_fireRate(0.2f),
		m_projectileSpeed(15.0f),
		m_damage(10.0f),
		m_fireTimer(0.0f),
		m_entityManager(nullptr)
	{
	}
	
	void Weapon::update(float deltaTime)
	{
		if (m_fireTimer > 0.0f)
		{
			m_fireTimer -= deltaTime;
		}
	}
	
	void Weapon::fire(const tmath::vector2df& direction)
	{
		if (!can_fire())
			return;
		
		// Get weapon position from parent entity
		Entity* parent = get_parent_entity();
		if (!parent)
			return;
		
		// For now, we'll just log that we're firing
		// In a full implementation, this would spawn a projectile entity
		LOG_INFO("[Weapon] Firing projectile in direction (%.2f, %.2f)", direction.x, direction.y);
		
		// Reset fire timer
		m_fireTimer = m_fireRate;
		
		// TODO: Implement projectile spawning when EntityManager is set
		if (m_entityManager)
		{
			// spawn_projectile will be implemented later
		}
	}
	
	void Weapon::spawn_projectile(const tmath::vector2df& position, const tmath::vector2df& direction)
	{
		if (!m_entityManager)
			return;
		
		// TODO: Create projectile entity
		// This will be implemented when we create the Projectile component
		LOG_INFO("[Weapon] Spawning projectile at (%.2f, %.2f)", position.x, position.y);
	}
}
