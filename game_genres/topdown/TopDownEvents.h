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

	// A consumable starts on an actor (ConsumableComponent::apply): a boost, a melee weapon or a gun lent, health, an effect
	// (an air strike). The view shows the moment (a power-up's flash, its pan, its aura while it lasts).
	struct ConsumableEvent
	{
		uintptr_t actor;
		b2Vec2 at;
		int id;
		float seconds;          // how long it lasts (0: instant)
		bool melee, firearm;
		float speedFactor, damageFactor, defenceFactor;
		float health;           // health it gives
		std::string effect;     // an effect that is not a stat ("airstrike"), or empty
	};

	// Something on the floor was taken (the view makes it fly to the one who took it)
	struct PickupEvent
	{
		uintptr_t taker;
		b2Vec2 at;
		int pickupId;
		std::string type;   // the pickup's type ("Money", "Token", ...)
	};

	// A prop shows an effect of the game's data (a name the game's EffectsSystem settings map to water, fire, glass or debris), or
	// "Break" when it breaks; 'size' is about how wide it is (meters)
	struct PropEffectEvent
	{
		b2Vec2 at;
		std::string effect;
		float size;
	};
}
