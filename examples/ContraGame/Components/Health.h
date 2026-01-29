///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Health.h
// Creation Date: January 2026
//
// Purpose: Health/damage component for game entities
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Component.h"
#include "Event.h"

namespace Pakal
{
	class _PAKALExport Health : public Component
	{
		DECLARE_RTTI_WITH_BASE(Health, Component);

	public:
		Health();
		~Health() override = default;

		// Health management
		void set_max_health(float health);
		void take_damage(float damage);
		void heal(float amount);
		void kill(); // Instant kill

		// Queries
		float get_current_health() const { return m_currentHealth; }
		float get_max_health() const { return m_maxHealth; }
		float get_health_percent() const;
		bool is_alive() const { return m_currentHealth > 0.0f; }

		// Invincibility
		void set_invincible(bool invincible) { m_invincible = invincible; }
		bool is_invincible() const { return m_invincible; }

		// Events
		Event<float> on_damage_taken;  // Passes damage amount
		Event<> on_death;
		Event<float> on_healed;  // Passes heal amount

	private:
		float m_maxHealth;
		float m_currentHealth;
		bool m_invincible;
		bool m_isDead;
	};
}
