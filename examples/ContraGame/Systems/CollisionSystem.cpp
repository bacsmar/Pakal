///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CollisionSystem.cpp
// Creation Date: January 2026
//
// Purpose: Collision detection and response implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "CollisionSystem.h"
#include "Entity.h"
#include "PhysicsSystem.h"
#include "../Components/Health.h"
#include "../Components/Weapon.h"

using namespace Pakal;

CollisionSystem::CollisionSystem()
	: m_physicsSystem(nullptr)
{
}

CollisionSystem::~CollisionSystem()
{
	terminate();
}

void CollisionSystem::initialize(PhysicsSystem* physicsSystem)
{
	m_physicsSystem = physicsSystem;

	// TODO: Register collision callbacks with Box2D physics system
	// physicsSystem->register_collision_listener(this);
}

void CollisionSystem::terminate()
{
	m_activeCollisions.clear();
	m_physicsSystem = nullptr;
}

void CollisionSystem::update(float deltaTime)
{
	// Collision handling is primarily done through callbacks
	// This update can be used for additional per-frame collision logic
}

void CollisionSystem::on_collision_begin(Entity* entityA, Entity* entityB)
{
	if (!entityA || !entityB)
		return;

	// Add to active collisions
	CollisionPair pair = { entityA, entityB };
	m_activeCollisions.push_back(pair);

	// Check what type of collision this is
	auto projectileA = entityA->get_component<Projectile>();
	auto projectileB = entityB->get_component<Projectile>();

	// Projectile collisions
	if (projectileA && !projectileB)
	{
		handle_projectile_collision(entityA, entityB);
	}
	else if (projectileB && !projectileA)
	{
		handle_projectile_collision(entityB, entityA);
	}

	// TODO: Collectible collisions
	// Check for collector component and collectible component

	// TODO: Damage zone collisions
	// Check for damage-dealing areas
}

void CollisionSystem::on_collision_end(Entity* entityA, Entity* entityB)
{
	if (!entityA || !entityB)
		return;

	// Remove from active collisions
	CollisionPair pair = { entityA, entityB };
	m_activeCollisions.erase(
		std::remove_if(m_activeCollisions.begin(), m_activeCollisions.end(),
			[&](const CollisionPair& p) { return p == pair; }),
		m_activeCollisions.end()
	);
}

void CollisionSystem::set_collision_filter(Entity* entity, uint16_t category, uint16_t mask)
{
	if (!entity)
		return;

	// TODO: Set Box2D collision filter
	// auto physicsComponent = entity->get_component<PhysicsComponent>();
	// if (physicsComponent)
	// {
	//     physicsComponent->set_collision_filter(category, mask);
	// }
}

void CollisionSystem::handle_projectile_collision(Entity* projectile, Entity* target)
{
	auto projectileComp = projectile->get_component<Projectile>();
	if (!projectileComp)
		return;

	// Check if target has health
	auto targetHealth = target->get_component<Health>();
	if (!targetHealth)
		return;

	// Check owner tag to prevent self-damage
	const std::string& ownerTag = projectileComp->get_owner_tag();
	
	// Simplified tag checking
	// TODO: Implement proper entity tagging system
	// For now, assume:
	// - "player" projectiles can hit enemies
	// - "enemy" projectiles can hit player

	// Apply damage
	float damage = projectileComp->get_damage();
	targetHealth->take_damage(damage);

	// Destroy projectile
	// TODO: Mark projectile entity for destruction
	// projectile->destroy();
}

void CollisionSystem::handle_collectible_collision(Entity* collector, Entity* collectible)
{
	// TODO: Implement collectible pickup logic
	// - Check collector has Collector component
	// - Get collectible type and apply effect (health, ammo, score)
	// - Destroy collectible entity
	// - Play pickup sound/effect
}

void CollisionSystem::handle_damage_collision(Entity* damager, Entity* target)
{
	// TODO: Handle damage zones (spikes, fire, etc.)
	// These deal continuous damage while in contact
}
