///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CharacterController2D.h
//
// Purpose: Top-down character body for a PhysicsWorld2D. A dynamic circle with fixed rotation and no
//          gravity. The caller sets a desired direction and speed; the physics keeps the body out of
//          static geometry, so the character slides along walls and stops against them.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Platform.h"
#include "math/Vec2.h"

#include <cstdint>

namespace Pakal
{
	class PhysicsWorld2D;

	class _PAKALExport CharacterController2D
	{
	public:
		// Positions and sizes are in meters.
		// userData identifies the owner in ray cast hits (see RayCastHit::userData)
		CharacterController2D(PhysicsWorld2D& world, const Vec2& position, float radius,
			uint16 categoryBits = 0x0001, uint16 maskBits = 0xFFFF, uintptr_t userData = 0);
		~CharacterController2D();

		CharacterController2D(const CharacterController2D&) = delete;
		CharacterController2D& operator=(const CharacterController2D&) = delete;

		// Desired movement. The direction is normalized; a zero direction stops the character.
		void set_move(const Vec2& direction, float speedMetersPerSecond);

		// Applies the desired velocity. Call once per step, before PhysicsWorld2D::step.
		void update();

		Vec2 get_position() const;
		// Moves the character to a position (meters) and stops it
		void teleport(const Vec2& position);
		// A disabled body takes no part in the physics (a dead character no longer blocks the others)
		void set_enabled(bool enabled);
		// The layers it collides with (its category stays): e.g. one up on a car's roof passes over the car's layer
		void set_mask(uint16 maskBits);

	private:
		PhysicsWorld2D& m_world;
		void* m_body;   // the backend's body (box2D/Box2DInternal.h)
		Vec2 m_velocity;
	};
}
