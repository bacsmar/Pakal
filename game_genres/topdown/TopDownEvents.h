///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: TopDownEvents.h
// Purpose: What a TopDown game tells the rest of the program (TopDownGame::events, delivered at the end
//          of each step): sounds, shots, impacts, explosions, camera shakes and the effects of props.
//          The level plays and draws them; tests count them. A game adds event types of its own the
//          same way (any copyable struct).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/Damage.h"

#include <box2d/b2_math.h>

#include <string>

namespace Pakal::TopDown
{
	// A sound of the game's table (TopDownRules::sound) at a point
	struct SoundEvent
	{
		std::string name;
		b2Vec2 at;
	};

	// A shot leaves the gun
	struct ShotEvent
	{
		b2Vec2 muzzle;
		b2Vec2 direction;
	};

	// What a bullet landed on
	enum class ImpactKind
	{
		Wall,
		Enemy,
		Prop,
	};

	struct ImpactEvent
	{
		b2Vec2 at;
		b2Vec2 direction;
		ImpactKind kind;
	};

	// An explosion: the level draws the blast and shakes the camera this hard
	struct ExplosionEvent
	{
		b2Vec2 at;
		float radius;
		float shake;
	};

	// A blow all around a point (an ability of the RPG kit): the view shows a ring
	struct AreaEvent
	{
		b2Vec2 at;
		float radius;
	};

	// Something else shakes the camera
	struct ShakeEvent
	{
		b2Vec2 at;
		float power;
	};

	// An actor took a hit (TopDownGame::damage): the hit as it came through the target's resistances and defence
	struct DamageEvent
	{
		uintptr_t target;
		DamageInfo damage;
		DamageResult result;
	};

	// A prop shows an effect of the game's data ("Hydrant  Water", "GazelleGlassBreak_swap_01", "FireBurning"...), or
	// "Break" when it breaks; 'size' is about how wide it is (meters)
	struct PropEffectEvent
	{
		b2Vec2 at;
		std::string effect;
		float size;
	};
}
