///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Health.h
// Purpose: Health/damage component for game entities
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
		virtual ~Health() = default;
		
		void set_max_health(float health);
		void take_damage(float damage);
		void heal(float amount);
		
		float get_current_health() const { return m_currentHealth; }
		float get_max_health() const { return m_maxHealth; }
		float get_health_percent() const { return m_currentHealth / m_maxHealth; }
		bool is_alive() const { return m_currentHealth > 0.0f; }
		
		// Events
		Event<float> on_damage_taken;
		Event<> on_death;
		
	private:
		float m_maxHealth;
		float m_currentHealth;
	};
}
