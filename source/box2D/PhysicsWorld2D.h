///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PhysicsWorld2D.h
//
// Purpose: Top-down 2D physics world owned by the engine. Box2D lives inside the engine library;
//          callers use this class (and CharacterController2D / raycast_closest) instead of linking
//          Box2D themselves. Two copies of Box2D in one process are not safe.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Platform.h"
#include "Box2DQueries.h"

#include <box2d/box2d.h>

#include <vector>

namespace Pakal
{
	class _PAKALExport PhysicsWorld2D
	{
	public:
		// Gravity is usually zero for top-down games.
		explicit PhysicsWorld2D(const b2Vec2& gravity);
		~PhysicsWorld2D();

		PhysicsWorld2D(const PhysicsWorld2D&) = delete;
		PhysicsWorld2D& operator=(const PhysicsWorld2D&) = delete;

		// Static geometry, in meters. 'halfSize' is half the width and height of the box. 'userData' is
		// reported by ray casts that hit the shape (RayCastHit::userData), 0 for none.
		void add_static_box(const b2Vec2& center, const b2Vec2& halfSize, uint16 categoryBits = 0x0001, uintptr_t userData = 0);
		void add_static_circle(const b2Vec2& center, float radius, uint16 categoryBits = 0x0001, uintptr_t userData = 0);
		// Static chain of edges through 'points' (meters). A closed chain joins the last point to the first.
		void add_static_chain(const std::vector<b2Vec2>& points, bool closed, uint16 categoryBits = 0x0001);
		// Convex solid polygon (3-8 vertices), with independent category/contact filters.
		void add_static_polygon(const std::vector<b2Vec2>& points, uint16 categoryBits = 0x0001, uint16 maskBits = 0xFFFF);

		// Turns on or off the static shapes made with this userData (a barricade that breaks and is repaired)
		void set_static_enabled(uintptr_t userData, bool enabled);

		void step(float dtSeconds);

		// True if the point is inside any static fixture
		bool point_in_static(const b2Vec2& point) const;

		// Distance from the point to the closest static surface (0 or less when inside)
		float distance_to_static(const b2Vec2& point, uint16 categoryMask = 0xFFFF) const;
		// Whether static collision of those categories is closer than 'radius' to the point (only the shapes near it are
		// measured, so it is cheap for big levels)
		bool static_within(const b2Vec2& point, float radius, uint16 categoryMask = 0xFFFF) const;

		// Closest hit along the segment, reported as static or not
		RayCastHit raycast(const b2Vec2& from, const b2Vec2& to, uint16 categoryMask = 0xFFFF);

		// Needed to create bodies such as CharacterController2D. Do not keep it beyond this object.
		b2World* native() { return &m_world; }

	private:
		b2World m_world;
	};
}
