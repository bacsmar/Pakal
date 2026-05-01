///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Projectile.h
// Purpose: Projectile component for game entities
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "CombatFaction.h"
#include "Component.h"
#include "math/vector2.h"
#include <atomic>

namespace Pakal
{
	class EntityManager;
	class SpritePhysicsComponent;
	class SpriteComponent2D;
	
	class _PAKALExport Projectile : public Component
	{
		DECLARE_RTTI_WITH_BASE(Projectile, Component);
		
	public:
		Projectile();
		virtual ~Projectile() = default;
		
		void initialize();
		void update(float deltaTime);
		
		void set_velocity(const tmath::vectorn<float, 2>& velocity) { m_velocity = velocity; }
		void set_damage(float damage) { m_damage = damage; }
		void set_faction(CombatFaction faction) { m_faction = faction; }
		void set_lifetime(float lifetime) { m_lifetime = lifetime; }
		
		float get_damage() const { return m_damage; }
		CombatFaction get_faction() const { return m_faction; }
		bool is_alive() const { return !m_expired && m_lifetime > 0.0f; }
		
		// Collision handling
		void on_collision(Projectile* other);
		void on_collision(Entity* other);
		
	private:
		void update_physics(float deltaTime);
		void update_lifetime(float deltaTime);
		
		tmath::vectorn<float, 2> m_velocity;
		float m_damage;
		CombatFaction m_faction;
		float m_lifetime;
		std::atomic_bool m_expired;
		
		// Components (obtained in initialize())
		SpritePhysicsComponent* m_physics;
		SpriteComponent2D* m_sprite;
	};
}