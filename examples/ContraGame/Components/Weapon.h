///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Weapon.h
// Creation Date: January 2026
//
// Purpose: Weapon system for shooting projectiles
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Component.h"
#include "PakalMath.h"
#include <string>

namespace Pakal
{
	class EntityManager;

	class _PAKALExport Weapon : public Component
	{
		DECLARE_RTTI_WITH_BASE(Weapon, Component);

	public:
		Weapon();
		~Weapon() override = default;

		void update(float deltaTime);

		// Configuration
		void set_fire_rate(float rate) { m_fireRate = rate; }
		void set_projectile_speed(float speed) { m_projectileSpeed = speed; }
		void set_damage(float damage) { m_damage = damage; }
		void set_fire_point_offset(const tmath::vector2df& offset) { m_firePointOffset = offset; }
		void set_owner_tag(const std::string& tag) { m_ownerTag = tag; }

		// Getters
		float get_fire_rate() const { return m_fireRate; }
		float get_projectile_speed() const { return m_projectileSpeed; }
		float get_damage() const { return m_damage; }
		const std::string& get_owner_tag() const { return m_ownerTag; }

		// Actions
		void fire();
		void fire_at_direction(const tmath::vector2df& direction);
		void fire_at_target(const tmath::vector2df& target);

		bool can_fire() const { return m_fireTimer <= 0.0f; }

	private:
		void spawn_projectile(const tmath::vector2df& direction);

		float m_fireRate;          // Shots per second
		float m_projectileSpeed;
		float m_damage;
		tmath::vector2df m_firePointOffset;
		std::string m_ownerTag;    // "player" or "enemy"

		float m_fireTimer;
		tmath::vector2df m_lastDirection;

		EntityManager* m_entityManager;
	};

	class _PAKALExport Projectile : public Component
	{
		DECLARE_RTTI_WITH_BASE(Projectile, Component);

	public:
		Projectile();
		~Projectile() override = default;

		void update(float deltaTime);

		void set_direction(const tmath::vector2df& direction);
		void set_speed(float speed) { m_speed = speed; }
		void set_damage(float damage) { m_damage = damage; }
		void set_lifetime(float lifetime) { m_lifetime = lifetime; }
		void set_owner_tag(const std::string& tag) { m_ownerTag = tag; }

		const std::string& get_owner_tag() const { return m_ownerTag; }
		float get_damage() const { return m_damage; }

	private:
		void on_collision(Entity* other);

		tmath::vector2df m_direction;
		float m_speed;
		float m_damage;
		float m_lifetime;
		float m_timeAlive;
		std::string m_ownerTag;  // "player" or "enemy"
	};
}
