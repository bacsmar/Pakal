///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CharacterController2D.h
//
// Purpose: Top-down character body for a Box2D world. A dynamic circle with fixed rotation and no
//          gravity. The caller sets a desired direction and speed; Box2D keeps the body out of
//          static geometry, so the character slides along walls and stops against them.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Platform.h"
#include <box2d/box2d.h>

namespace Pakal
{
	class _PAKALExport CharacterController2D
	{
	public:
		// Positions and sizes are in meters (Box2D units).
		// userData identifies the owner in ray cast hits (see RayCastHit::userData)
		CharacterController2D(b2World* world, const b2Vec2& position, float radius,
			uint16 categoryBits = 0x0001, uint16 maskBits = 0xFFFF, uintptr_t userData = 0);
		~CharacterController2D();

		CharacterController2D(const CharacterController2D&) = delete;
		CharacterController2D& operator=(const CharacterController2D&) = delete;

		// Desired movement. The direction is normalized; a zero direction stops the character.
		void set_move(const b2Vec2& direction, float speedMetersPerSecond);

		// Applies the desired velocity. Call once per step, before b2World::Step.
		void update();

		b2Vec2 get_position() const;
		// Moves the character to a position (meters) and stops it
		void teleport(const b2Vec2& position);
		// A disabled body takes no part in the physics (a dead character no longer blocks the others)
		void set_enabled(bool enabled);
		b2Body* get_body() const { return m_body; }

	private:
		b2World* m_world;
		b2Body* m_body;
		b2Vec2 m_velocity;
	};
}
