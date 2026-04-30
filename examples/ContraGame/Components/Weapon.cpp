///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Weapon.cpp
// Purpose: Weapon/shooting component for game entities - Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Weapon.h"
#include "EntityManager.h"
#include "Entity.h"
#include "LogMgr.h"
#include "Components/Projectile.h"
#include "components/SpritePhysicsComponent.h"
#include "components/SpriteComponent2D.h"
#include "GenericEntity.h"

namespace Pakal
{
	namespace
	{
		SpriteSheetPhysicsPtr create_projectile_body_physics()
		{
			auto sheet = std::make_shared<SpriteSheetPhysics>();
			auto* body = new SpritePhysics();
			body->name = "projectile";
			body->dynamic = true;
			body->fixed_rotation = true;

			SpritePhysics::Fixture fixture;
			fixture.type = "CIRCLE";
			fixture.scale = 1.0f;
			fixture.density = 1.0f;
			fixture.friction = 0.0f;
			fixture.restitution = 0.0f;
			fixture.m_circle.r = 0.25f;
			fixture.m_circle.x = 0.0f;
			fixture.m_circle.y = 0.0f;

			body->m_fixtures.emplace_back(std::move(fixture));
			sheet->bodies.emplace_back(body);
			return sheet;
		}
	}

	Weapon::Weapon() :
		m_fireRate(0.2f),
		m_projectileSpeed(15.0f),
		m_damage(10.0f),
		m_fireTimer(0.0f),
		m_entityManager(nullptr),
		m_faction(CombatFaction::Player)
	{
	}
	
	void Weapon::update(float deltaTime)
	{
		if (m_fireTimer > 0.0f)
		{
			m_fireTimer -= deltaTime;
		}
	}
	
	void Weapon::fire(const tmath::vectorn<float, 2>& direction)
	{
		if (!can_fire())
			return;
		
		// Get weapon position from parent entity
		Entity* parent = get_parent_entity();
		if (!parent)
			return;
		
		if (m_entityManager)
		{
			// Get parent entity position for projectile spawn
			auto* physics = parent->get_component<SpritePhysicsComponent>();
			if (physics)
			{
				auto position = physics->get_position();
				
				// Create projectile entity
				auto* projectileEntity = static_cast<GenericEntity*>(m_entityManager->create_entity("Pakal::GenericEntity", "projectile"));
				if (projectileEntity)
				{
					// Set up projectile physics
					auto* projectilePhysics = projectileEntity->create_component<SpritePhysicsComponent>();
					if (projectilePhysics)
					{
						SpritePhysicsComponent::Settings physics_settings(create_projectile_body_physics());
						physics_settings.position = position;
						physics_settings.scale = 0.5f;  // Small projectile
						physics_settings.body_type = SpritePhysicsComponent::DynamicBody;
						physics_settings.fixed_rotation = true;
						auto initializeTask = projectilePhysics->initialize(physics_settings);
						if (initializeTask)
						{
							initializeTask->wait();
						}
					}
					
					// Set up projectile sprite
					auto* projectileSprite = projectileEntity->create_component<SpriteComponent2D>();
					if (projectileSprite)
					{
						projectileSprite->set_position(position.x, position.y);
						projectileSprite->set_scale(0.5f, 0.5f);
						projectileSprite->set_color(1.0f, 1.0f, 0.0f, 1.0f); // Yellow projectile
						// Use solid color instead of texture file
						projectileSprite->create_solid_color(0xFFFFFF00, 1, 1); // Yellow color
						projectileSprite->set_layer(5);
					}
					
					// Set up projectile component
					auto* projectileComponent = projectileEntity->create_component<Projectile>();
					if (projectileComponent)
					{
						projectileComponent->set_velocity(direction * m_projectileSpeed);
						projectileComponent->set_damage(m_damage);
						projectileComponent->set_faction(m_faction);
						projectileComponent->set_lifetime(5.0f);
						projectileComponent->initialize();
					}

					if (m_projectileCreated)
					{
						m_projectileCreated(projectileEntity->get_handle());
					}
					
					LOG_INFO("[Weapon] Firing projectile in direction (%.2f, %.2f), faction=%d", direction.x, direction.y, static_cast<int>(m_faction));
				}
			}
		}
		
		// Reset fire timer
		m_fireTimer = m_fireRate;
	}
}