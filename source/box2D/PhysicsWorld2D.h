///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PhysicsWorld2D.h
//
// Purpose: Top-down 2D physics world owned by the engine. Box2D lives inside the engine library and
//          no header of the 2D physics names it (docs/adr ADR-0017): callers use this class,
//          PhysicsBody2D and CharacterController2D, in the engine's Vec2.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Platform.h"
#include "math/Vec2.h"
#include "Box2DQueries.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace Pakal
{
	// A shape of the world as a debug view draws it, in world coordinates (meters)
	struct PhysicsShape2D
	{
		enum class Kind { Polygon, Circle, Chain, Edge };
		Kind kind = Kind::Polygon;
		bool isStatic = false;          // level geometry (else a moving body)
		bool enabled = true;            // a disabled body takes no part in the physics
		uint16 categoryBits = 0;
		uintptr_t userData = 0;         // the body's owner id (0 if none)
		// Polygon: its corners (a closed outline); Chain: the points of its links, in order (open); Edge: its two ends
		const Vec2* points = nullptr;
		int count = 0;
		Vec2 center = { 0.0f, 0.0f };   // Circle
		float radius = 0.0f;            // Circle
		Vec2 lower = { 0.0f, 0.0f };    // the box around the shape
		Vec2 upper = { 0.0f, 0.0f };
	};

	class _PAKALExport PhysicsWorld2D
	{
	public:
		// Gravity is usually zero for top-down games.
		explicit PhysicsWorld2D(const Vec2& gravity);
		~PhysicsWorld2D();

		PhysicsWorld2D(const PhysicsWorld2D&) = delete;
		PhysicsWorld2D& operator=(const PhysicsWorld2D&) = delete;

		// Static geometry, in meters. 'halfSize' is half the width and height of the box. 'userData' is
		// reported by ray casts that hit the shape (RayCastHit::userData), 0 for none.
		void add_static_box(const Vec2& center, const Vec2& halfSize, uint16 categoryBits = 0x0001, uintptr_t userData = 0, float angle = 0.0f);
		void add_static_circle(const Vec2& center, float radius, uint16 categoryBits = 0x0001, uintptr_t userData = 0);
		// Static chain of edges through 'points' (meters). A closed chain joins the last point to the first.
		void add_static_chain(const std::vector<Vec2>& points, bool closed, uint16 categoryBits = 0x0001);
		// Solid polygon, with independent category/contact filters: any simple polygon, either winding, convex or not and
		// of any number of vertices. Box2D takes convex pieces of 8 at most: a bigger convex one is split in fans, a concave
		// one in triangles, all on one body. Pieces too thin for Box2D are left out with a warning.
		void add_static_polygon(const std::vector<Vec2>& points, uint16 categoryBits = 0x0001, uint16 maskBits = 0xFFFF);

		// Turns on or off the static shapes made with this userData (a barricade that breaks and is repaired)
		void set_static_enabled(uintptr_t userData, bool enabled);

		void step(float dtSeconds);

		// True if the point is inside any static fixture
		bool point_in_static(const Vec2& point) const;

		// Distance from the point to the closest static surface (0 or less when inside)
		float distance_to_static(const Vec2& point, uint16 categoryMask = 0xFFFF) const;
		// Whether static collision of those categories is closer than 'radius' to the point (only the shapes near it are
		// measured, so it is cheap for big levels)
		bool static_within(const Vec2& point, float radius, uint16 categoryMask = 0xFFFF) const;

		// Closest hit along the segment, reported as static or not
		RayCastHit raycast(const Vec2& from, const Vec2& to, uint16 categoryMask = 0xFFFF);

		// Every shape of the world, bodies and level geometry, for the views that draw the colliders. 'points' is valid
		// during the call only. Not for the simulation: it is slow on purpose (no queries, every fixture).
		void for_each_shape(const std::function<void(const PhysicsShape2D&)>& visit) const;

		// The backend's world, for the 2D physics' own .cpp files (box2D/Box2DInternal.h); opaque to everyone else
		struct Impl;
		Impl& impl() const { return *m_impl; }

	private:
		std::unique_ptr<Impl> m_impl;
	};
}
