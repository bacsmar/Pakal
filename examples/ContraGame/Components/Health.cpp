///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Health.cpp
// Creation Date: January 2026
//
// Purpose: Health/damage component implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Health.h"
#include <algorithm>

using namespace Pakal;

IMPLEMENT_RTTI(Health, Component);

Health::Health() 
	: m_maxHealth(100.0f)
	, m_currentHealth(100.0f)
	, m_invincible(false)
	, m_isDead(false)
{
}

void Health::set_max_health(float health)
{
	m_maxHealth = health;
	m_currentHealth = health;
	m_isDead = false;
}

void Health::take_damage(float damage)
{
	if (m_invincible || m_isDead || damage <= 0.0f)
		return;

	m_currentHealth = std::max(0.0f, m_currentHealth - damage);

	on_damage_taken(damage);

	if (m_currentHealth <= 0.0f && !m_isDead)
	{
		m_isDead = true;
		on_death();
	}
}

void Health::heal(float amount)
{
	if (m_isDead || amount <= 0.0f)
		return;

	float oldHealth = m_currentHealth;
	m_currentHealth = std::min(m_maxHealth, m_currentHealth + amount);

	float actualHeal = m_currentHealth - oldHealth;
	if (actualHeal > 0.0f)
	{
		on_healed(actualHeal);
	}
}

void Health::kill()
{
	if (m_isDead)
		return;

	m_currentHealth = 0.0f;
	m_isDead = true;
	on_death();
}

float Health::get_health_percent() const
{
	if (m_maxHealth <= 0.0f)
		return 0.0f;
	return m_currentHealth / m_maxHealth;
}
