///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Damage.h
// Purpose: One hit, described whole (DamageInfo): how much, of what type, who dealt it, from where, and
//          what it does besides hurting (a push, a stun, how the target dies). TopDownGame::damage takes
//          it through the target's resistances and defence, lets the game apply it, and reports it
//          (DamageEvent).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/TopDownRules.h"

#include "math/Vec2.h"

#include <cstdint>

namespace Pakal::TopDown
{
	// What kind of hit it is: resistances are by type (StatsComponent::resistances). A game adds its own from Game on.
	struct DamageType
	{
		enum : int
		{
			Generic,   // a script, a fall, anything else
			Bullet,
			Melee,
			Blast,     // an explosion
			Attack,    // an enemy's own attack (a bite, a punch, a breath)
			Game = 100,
		};
	};

	struct DamageInfo
	{
		float amount = 0.0f;
		int type = DamageType::Generic;
		uintptr_t source = 0;                   // the actor that dealt it (0: none, or not known)
		Pakal::Vec2 from = Pakal::Vec2(0.0f, 0.0f);       // where it came from (the push goes away from it)
		bool knockback = false;                 // pushes the target away instead of its hurt stun
		float hitstun = 0.0f;                   // seconds the target cannot act (0: its own default)
		bool critical = false;                  // the game decides what a critical hit does more
		bool guarded = false;                   // the target guarded it (a Brawler kit's guard: 'amount' is already its share)
		DeathKind death = DeathKind::Normal;    // how the target dies if this kills it
	};

	// What a hit did (TopDownGame::apply_damage)
	struct DamageResult
	{
		bool hit = false;       // it was alive and took the hit
		float taken = 0.0f;     // the damage it took, after its own factors
		bool killed = false;
	};
}
