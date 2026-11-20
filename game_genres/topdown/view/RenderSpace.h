///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: RenderSpace.h
// Purpose: From the simulation's plane (meters, level X east and Y north) to render space (meters,
//          Y up): the floor is the XZ plane, and level north is render -Z.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "PakalMath.h"

#include "math/Vec2.h"

namespace Pakal::TopDown
{
	inline tmath::vector3df floor_point(const Pakal::Vec2& p, float height = 0.0f)
	{
		return tmath::vector3df(p.x, height, -p.y);
	}
}
