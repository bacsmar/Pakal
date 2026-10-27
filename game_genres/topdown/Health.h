///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Health.h
//
// Purpose: Health, the one gameplay value the enemy movement (EnemyAI2D) keeps. Weapons, pickups and
//          objectives are rules of a game, not of the engine: they live with the game (or the TopDown
//          module), not here.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <algorithm>

namespace Pakal::TopDown
{
	struct Health
	{
		float maximum = 100.0f;
		float current = 100.0f;

		bool is_dead() const { return current <= 0.0f; }

		// Returns the damage actually taken
		float apply_damage(float amount)
		{
			const float taken = std::min(std::max(amount, 0.0f), current);
			current -= taken;
			return taken;
		}
	};
}
