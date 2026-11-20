///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Box2DQueries.h
//
// Purpose: What a ray cast of the 2D physics reports (PhysicsWorld2D::raycast). No Box2D type: the query itself is
//          inside the physics (Box2DInternal.h).
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "math/Vec2.h"

#include <cstdint>

namespace Pakal
{
	struct RayCastHit
	{
		bool hit = false;
		Vec2 point = { 0.0f, 0.0f };
		Vec2 normal = { 0.0f, 0.0f };
		float fraction = 1.0f; // 0..1 along the ray
		bool staticBody = false; // true when the hit body is static (walls, level geometry)
		uint16 categoryBits = 0; // category of the hit fixture
		uintptr_t userData = 0;  // id stored on the body by its owner (0 if none)
	};
}
