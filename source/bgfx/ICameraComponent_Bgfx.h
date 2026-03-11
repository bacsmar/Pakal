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
	};
}
