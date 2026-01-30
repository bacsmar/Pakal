///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Health.cpp
// Purpose: Health/damage component for game entities - Implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Health.h"
#include "LogMgr.h"

namespace Pakal
{
	Health::Health() :
		m_maxHealth(100.0f),
		m_currentHealth(100.0f)
	{
	}
	
	void Health::set_max_health(float health)
	{
		m_maxHealth = health;
		m_currentHealth = health;
	}
	
	void Health::take_damage(float damage)
	{
		if (!is_alive())
			return;
		
		m_currentHealth -= damage;
		
		if (m_currentHealth < 0.0f)
			m_currentHealth = 0.0f;
		
		on_damage_taken.execute(damage);
		
		if (m_currentHealth <= 0.0f)
		{
			on_death.execute();
		}
	}
	
	void Health::heal(float amount)
	{
		m_currentHealth += amount;
		
		if (m_currentHealth > m_maxHealth)
			m_currentHealth = m_maxHealth;
	}
}
