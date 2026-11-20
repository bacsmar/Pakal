///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Box2DInternal.h
//
// Purpose: what the 2D physics' .cpp files share and nothing else includes (docs/adr ADR-0017): Box2D's world inside a
//          PhysicsWorld2D, a body behind its opaque pointer, and the conversions between the engine's Vec2/Aabb2 and
//          Box2D's types (the same members, so a conversion copies two floats).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "box2D/PhysicsWorld2D.h"

#include <box2d/box2d.h>

namespace Pakal
{
	struct PhysicsWorld2D::Impl
	{
		explicit Impl(const b2Vec2& gravity) : world(gravity) {}
		b2World world;
	};

	inline b2Vec2 to_b2(const Vec2& v) { return b2Vec2(v.x, v.y); }
	inline Vec2 to_vec2(const b2Vec2& v) { return Vec2(v.x, v.y); }
	inline b2AABB to_b2(const Aabb2& box)
	{
		b2AABB out;
		out.lowerBound = to_b2(box.lowerBound);
		out.upperBound = to_b2(box.upperBound);
		return out;
	}
	inline Aabb2 to_aabb2(const b2AABB& box) { return Aabb2{ to_vec2(box.lowerBound), to_vec2(box.upperBound) }; }

	// Box2D's world of a PhysicsWorld2D
	inline b2World& box2d_world(PhysicsWorld2D& world) { return world.impl().world; }
	inline const b2World& box2d_world(const PhysicsWorld2D& world) { return world.impl().world; }
	// A body kept behind an opaque pointer (PhysicsBody2D, CharacterController2D)
	inline b2Body* box2d_body(void* body) { return static_cast<b2Body*>(body); }

	// The closest fixture the segment hits whose category overlaps 'categoryMask' (Box2DQueries.cpp); 'body' is the body
	// it belongs to (nullptr when nothing was hit)
	RayCastHit raycast_closest(b2World& world, const b2Vec2& from, const b2Vec2& to, uint16 categoryMask, const b2Body** body);
}
