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
#include <string>

namespace Pakal
{
	namespace
	{
		CombatFaction faction_for_entity(const Entity& entity)
		{
			const auto& descriptor = entity.get_descriptor();
			if (descriptor.find("enemy") != std::string::npos)
			{
				return CombatFaction::Enemy;
			}
			if (descriptor.find("player") != std::string::npos)
			{
				return CombatFaction::Player;
			}
			return CombatFaction::Neutral;
		}

		bool can_damage(CombatFaction projectileFaction, const Entity& target)
		{
			const auto targetFaction = faction_for_entity(target);
			return (projectileFaction == CombatFaction::Player && targetFaction == CombatFaction::Enemy) ||
				(projectileFaction == CombatFaction::Enemy && targetFaction == CombatFaction::Player);
		}
	}

	Projectile::Projectile() :
		m_velocity(0.0f, 0.0f),
		m_damage(10.0f),
		m_faction(CombatFaction::Player),
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
		if (m_physics)
		{
			m_physics->event_collide.add_listener([this](const Entity* other)
			{
				if (other)
				{
					on_collision(const_cast<Entity*>(other));
				}
			});
		}
		
		if (!m_physics || !m_sprite)
		{
			LOG_WARNING("Projectile: Missing physics or sprite component");
		}
	}
	
	void Projectile::update(float deltaTime)
	{
		if (!is_alive())
		{
			if (auto* parent = get_parent_entity())
			{
				parent->request_dispose();
			}
			return;
		}
		
		update_lifetime(deltaTime);
		update_physics(deltaTime);

		if (!is_alive())
		{
			if (auto* parent = get_parent_entity())
			{
				parent->request_dispose();
			}
		}
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
		if (auto* parent = get_parent_entity())
		{
			parent->request_dispose();
		}
	}
	
	void Projectile::on_collision(Entity* other)
	{
		if (!other)
		{
			return;
		}

		if (other->get_component<Projectile>())
		{
			m_lifetime = 0.0f;
			if (auto* parent = get_parent_entity())
			{
				parent->request_dispose();
			}
			return;
		}

		auto* health = other->get_component<Health>();
		if (health && can_damage(m_faction, *other))
		{
			health->take_damage(m_damage);
			LOG_INFO("[Projectile] Hit %s, dealt %f damage, faction=%d", other->get_descriptor().c_str(), m_damage, static_cast<int>(m_faction));
		}
		
		m_lifetime = 0.0f;
		if (auto* parent = get_parent_entity())
		{
			parent->request_dispose();
		}
	}
}