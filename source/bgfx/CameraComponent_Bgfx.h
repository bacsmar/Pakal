///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CameraComponent_Bgfx.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: January 30, 2026
//
// Purpose: 2D Orthographic camera component using bgfx
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Config.h"
#include "EntityHandle.h"
#include "ICameraComponent_Bgfx.h"
#include <ostream>
#include <type_traits>
#include <cmath>
#include <bgfx/bgfx.h>
#include "math/vector2.h"
#include "math/vector3.h"
#include "components/CameraComponent2D.h"

namespace Pakal
{
	class Entity;
	class BgfxGraphicsSystem;
	
	class _PAKALExport CameraComponent_Bgfx : public CameraComponent2D, public ICameraComponent_Bgfx
	{
		DECLARE_RTTI_WITH_BASE(CameraComponent_Bgfx, CameraComponent2D);
		
	public:
		explicit CameraComponent_Bgfx(BgfxGraphicsSystem* graphicsSystem = nullptr);
		virtual ~CameraComponent_Bgfx();
		
		// Camera setup
		void set_orthographic(float width, float height, float near = 0.1f, float far = 100.0f);
		void set_viewport(int x, int y, int width, int height);
		
		// Camera transform
		void set_position(float x, float y);
		void set_position(const tmath::vectorn<float, 2>& pos);
		tmath::vectorn<float, 2> get_position() const { return m_position; }
		
		void set_zoom(float zoom);
		float get_zoom() const { return m_zoom; }
		
		// Camera follow (for following player)
		void follow_target(EntityHandle target, float smoothness = 0.1f);
		void follow_target(Entity* target, float smoothness = 0.1f);
		void clear_follow_target() { m_followTarget = {}; }
		
		void set_bounds(float minX, float minY, float maxX, float maxY);
		void clear_bounds();
		bool has_bounds() const { return m_hasBounds; }
		
		// Update
		void update(float deltaTime);
		
		// Apply camera transforms to bgfx view
		void apply_camera(bgfx::ViewId viewId) override;
		
		// Utility functions
		void screen_to_world(float screenX, float screenY, float& worldX, float& worldY) const;
		void world_to_screen(float worldX, float worldY, float& screenX, float& screenY) const;
		
		tmath::vectorn<float, 2> screen_to_world(const tmath::vectorn<float, 2>& screen) const;
		tmath::vectorn<float, 2> world_to_screen(const tmath::vectorn<float, 2>& world) const;
		
		// Get matrices
		const float* get_view_matrix() const { return m_view; }
		const float* get_projection_matrix() const { return m_proj; }
		
	private:
		void update_matrices();
		void apply_bounds();
		void follow_update(float deltaTime);
		
		// Matrices
		float m_view[16];
		float m_proj[16];
		
		// Transform
		tmath::vectorn<float, 2> m_position;
		float m_zoom;
		
		// Orthographic settings
		float m_orthoWidth;
		float m_orthoHeight;
		float m_nearPlane;
		float m_farPlane;
		
		// Follow target
		EntityHandle m_followTarget;
		float m_followSmooth;
		tmath::vectorn<float, 2> m_targetPosition;
		
		// Bounds
		bool m_hasBounds;
		float m_bounds[4]; // minX, minY, maxX, maxY
		
		// Viewport
		int m_viewport[4]; // x, y, width, height
		
		// Flags
		bool m_matricesDirty;
		
		// Graphics system reference
		BgfxGraphicsSystem* m_graphics_system;
	};
}
