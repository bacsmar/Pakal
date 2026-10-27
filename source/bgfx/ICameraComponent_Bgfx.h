///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: ICameraComponent_Bgfx.h
//
// Purpose: Common camera interface for bgfx camera components (2D/3D)
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <bgfx/bgfx.h>

namespace Pakal
{
	class ICameraComponent_Bgfx
	{
	public:
		virtual ~ICameraComponent_Bgfx() = default;
		virtual void apply_camera(bgfx::ViewId viewId) = 0;
		// Scale of the perspective projection on x and y (the matrix's [0] and [5]) and its planes: what the post-process needs
		// to rebuild view positions from depth. False for cameras that are not perspective.
		// The point the camera looks at, in world space (where the sun's shadow map is centered). False when it has none.
		virtual bool get_focus(float& x, float& y, float& z) const
		{
			(void)x; (void)y; (void)z;
			return false;
		}
		virtual bool get_projection(float& scaleX, float& scaleY, float& nearPlane, float& farPlane) const
		{
			(void)scaleX; (void)scaleY; (void)nearPlane; (void)farPlane;
			return false;
		}
		// The view and projection matrices as last applied (bgfx layout): what the sun's shadow map fits to. False when unknown.
		virtual bool get_view_projection(const float*& view, const float*& proj) const
		{
			(void)view; (void)proj;
			return false;
		}
	};
}
