///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Weapon.h
// Purpose: Weapon/shooting component for game entities
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "CombatFaction.h"
#include "Component.h"
#include "EntityHandle.h"
#include "math/vector2.h"
#include <functional>

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
		void fire(const tmath::vectorn<float, 2>& direction);
		bool can_fire() const { return m_fireTimer <= 0.0f; }
		
		void set_fire_rate(float rate) { m_fireRate = rate; }
		void set_projectile_speed(float speed) { m_projectileSpeed = speed; }
		void set_damage(float damage) { m_damage = damage; }
		void set_entity_manager(EntityManager* manager) { m_entityManager = manager; }
		void set_faction(CombatFaction faction) { m_faction = faction; }
		void set_projectile_created_callback(const std::function<void(EntityHandle)>& callback) { m_projectileCreated = callback; }
		
		float get_fire_rate() const { return m_fireRate; }
		float get_projectile_speed() const { return m_projectileSpeed; }
		float get_damage() const { return m_damage; }
		CombatFaction get_faction() const { return m_faction; }
		
	private:
		float m_fireRate;   // Time between shots in seconds
		float m_projectileSpeed;
		float m_damage;
		float m_fireTimer;
		
		EntityManager* m_entityManager;
		CombatFaction m_faction;
		std::function<void(EntityHandle)> m_projectileCreated;
	};
}
