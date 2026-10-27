///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Box2DQueries.h
//
// Purpose: World queries for Box2D worlds (ray casts). Work on any b2World, so they can be used
//          with Box2DPhysicsSystem::get_world().
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <box2d/box2d.h>

namespace Pakal
{
	struct RayCastHit
	{
		bool hit = false;
		b2Vec2 point = { 0.0f, 0.0f };
		b2Vec2 normal = { 0.0f, 0.0f };
		float fraction = 1.0f; // 0..1 along the ray
		b2Body* body = nullptr;
		bool staticBody = false; // true when the hit body is static (walls, level geometry)
		uint16 categoryBits = 0; // category of the hit fixture
		uintptr_t userData = 0;  // id stored on the body by its owner (0 if none)
	};

	// Closest fixture hit by the segment from 'from' to 'to'. Only fixtures whose category bits
	// overlap 'categoryMask' are reported, so a ray can ignore the caster's own body.
	RayCastHit raycast_closest(b2World* world, const b2Vec2& from, const b2Vec2& to, uint16 categoryMask = 0xFFFF);
}
