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
#include "Component.h"
#include <bgfx/bgfx.h>
#include "math/vector2.h"
#include "math/vector3.h"

namespace Pakal
{
	class Entity;
	
	class _PAKALExport CameraComponent_Bgfx : public Component
	{
		DECLARE_RTTI_WITH_BASE(CameraComponent_Bgfx, Component);
		
	public:
		CameraComponent_Bgfx();
		virtual ~CameraComponent_Bgfx();
		
		// Camera setup
		void set_orthographic(float width, float height, float near = 0.1f, float far = 100.0f);
		void set_viewport(int x, int y, int width, int height);
		
		// Camera transform
		void set_position(float x, float y);
		void set_position(const tmath::vector2df& pos);
		tmath::vector2df get_position() const { return m_position; }
		
		void set_zoom(float zoom);
		float get_zoom() const { return m_zoom; }
		
		// Camera follow (for following player)
		void follow_target(Entity* target, float smoothness = 0.1f);
		void clear_follow_target() { m_followTarget = nullptr; }
		
		void set_bounds(float minX, float minY, float maxX, float maxY);
		void clear_bounds();
		bool has_bounds() const { return m_hasBounds; }
		
		// Update
		void update(float deltaTime);
		
		// Apply camera transforms to bgfx view
		void apply_camera(bgfx::ViewId viewId);
		
		// Utility functions
		void screen_to_world(float screenX, float screenY, float& worldX, float& worldY) const;
		void world_to_screen(float worldX, float worldY, float& screenX, float& screenY) const;
		
		tmath::vector2df screen_to_world(const tmath::vector2df& screen) const;
		tmath::vector2df world_to_screen(const tmath::vector2df& world) const;
		
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
		tmath::vector2df m_position;
		float m_zoom;
		
		// Orthographic settings
		float m_orthoWidth;
		float m_orthoHeight;
		float m_nearPlane;
		float m_farPlane;
		
		// Follow target
		Entity* m_followTarget;
		float m_followSmooth;
		tmath::vector2df m_targetPosition;
		
		// Bounds
		bool m_hasBounds;
		float m_bounds[4]; // minX, minY, maxX, maxY
		
		// Viewport
		int m_viewport[4]; // x, y, width, height
		
		// Flags
		bool m_matricesDirty;
	};
}
