///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PhysicsBody2D.h
//
// Purpose: A box-shaped body of a PhysicsWorld2D with gravity, for the games that are not top-down (a fighter, a platformer's
//          hero, a platform that comes and goes). Box2D lives inside Pakal.dll only: a module cannot call it, so it asks
//          for the few things it needs here. Static, kinematic (it moves by its velocity and nothing pushes it) or dynamic.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Platform.h"
#include "math/Vec2.h"
#include "box2D/PhysicsWorld2D.h"

namespace Pakal
{
	class _PAKALExport PhysicsBody2D
	{
	public:
		enum class Type { Static, Kinematic, Dynamic };

		struct Def
		{
			Type type = Type::Dynamic;
			Vec2 position = Vec2(0.0f, 0.0f);   // the body's origin (meters)
			Vec2 halfSize = Vec2(0.5f, 0.5f);   // of the box
			Vec2 offset = Vec2(0.0f, 0.0f);     // the box's centre from the origin
			float density = 1.0f;
			float friction = 0.2f;
			float restitution = 0.0f;
			float gravityScale = 1.0f;
			bool fixedRotation = true;
			bool sensor = false;                    // it reports touching but nothing is pushed
			uint16 categoryBits = 0x0001;
			uint16 maskBits = 0xFFFF;
		};

		PhysicsBody2D(PhysicsWorld2D& world, const Def& def);
		~PhysicsBody2D();
		PhysicsBody2D(const PhysicsBody2D&) = delete;
		PhysicsBody2D& operator=(const PhysicsBody2D&) = delete;

		Vec2 position() const;
		float angle() const;
		Vec2 velocity() const;
		float mass() const;
		void set_position(const Vec2& position);
		void set_velocity(const Vec2& velocity);
		void apply_impulse(const Vec2& impulse);
		void set_gravity_scale(float scale);
		// A disabled body takes no part in the physics
		void set_enabled(bool enabled);
		// The box in the world (lower left, upper right)
		void bounds(Vec2& lower, Vec2& upper) const;
		// Something touches it from below (it stands on it): the floor, another body
		bool supported() const;
		// It touches that body now
		bool touching(const PhysicsBody2D& other) const;
		// What the contact of the box with the box of 'other' is (no physics: a hit box of a game)
		bool overlaps(const PhysicsBody2D& other) const;

	private:
		PhysicsWorld2D& m_world;
		void* m_body;   // the backend's body (box2D/Box2DInternal.h)
	};
}
