///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Weapon.h
// Purpose: Weapon/shooting component for game entities
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Component.h"
#include "math/vector2.h"

namespace Pakal
{
	class EntityManager;
	
	class _PAKALExport Weapon : public Component
	{
		DECLARE_RTTI_WITH_BASE(Weapon, Component);
		
	public:
		Weapon();
		virtual ~Weapon() = default;
		
		void update(float deltaTime);
		void fire(const tmath::vector2df& direction);
		bool can_fire() const { return m_fireTimer <= 0.0f; }
		
		void set_fire_rate(float rate) { m_fireRate = rate; }
		void set_projectile_speed(float speed) { m_projectileSpeed = speed; }
		void set_damage(float damage) { m_damage = damage; }
		void set_entity_manager(EntityManager* manager) { m_entityManager = manager; }
		
		float get_fire_rate() const { return m_fireRate; }
		float get_projectile_speed() const { return m_projectileSpeed; }
		float get_damage() const { return m_damage; }
		
	private:
		void spawn_projectile(const tmath::vector2df& position, const tmath::vector2df& direction);
		
		float m_fireRate;   // Time between shots in seconds
		float m_projectileSpeed;
		float m_damage;
		float m_fireTimer;
		
		EntityManager* m_entityManager;
	};
}
