///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CameraComponent_Bgfx.cpp
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: January 30, 2026
//
// Purpose: 2D Orthographic camera component using bgfx - Implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "CameraComponent_Bgfx.h"
#include "Entity.h"
#include "LogMgr.h"
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <cmath>

namespace Pakal
{
	CameraComponent_Bgfx::CameraComponent_Bgfx() :
		m_position(0.0f, 0.0f),
		m_zoom(1.0f),
		m_orthoWidth(800.0f),
		m_orthoHeight(600.0f),
		m_nearPlane(0.1f),
		m_farPlane(100.0f),
		m_followTarget(nullptr),
		m_followSmooth(0.1f),
		m_targetPosition(0.0f, 0.0f),
		m_hasBounds(false),
		m_matricesDirty(true)
	{
		// Initialize viewport
		m_viewport[0] = 0;
		m_viewport[1] = 0;
		m_viewport[2] = 800;
		m_viewport[3] = 600;
		
		// Initialize bounds
		m_bounds[0] = m_bounds[1] = m_bounds[2] = m_bounds[3] = 0.0f;
		
		// Initialize matrices
		bx::mtxIdentity(m_view);
		bx::mtxIdentity(m_proj);
	}
	
	CameraComponent_Bgfx::~CameraComponent_Bgfx()
	{
	}
	
	void CameraComponent_Bgfx::set_orthographic(float width, float height, float near, float far)
	{
		m_orthoWidth = width;
		m_orthoHeight = height;
		m_nearPlane = near;
		m_farPlane = far;
		m_matricesDirty = true;
	}
	
	void CameraComponent_Bgfx::set_viewport(int x, int y, int width, int height)
	{
		m_viewport[0] = x;
		m_viewport[1] = y;
		m_viewport[2] = width;
		m_viewport[3] = height;
	}
	
	void CameraComponent_Bgfx::set_position(float x, float y)
	{
		m_position.x = x;
		m_position.y = y;
		m_matricesDirty = true;
	}
	
	void CameraComponent_Bgfx::set_position(const tmath::vector2df& pos)
	{
		m_position = pos;
		m_matricesDirty = true;
	}
	
	void CameraComponent_Bgfx::set_zoom(float zoom)
	{
		m_zoom = zoom;
		if (m_zoom < 0.1f) m_zoom = 0.1f; // Clamp minimum zoom
		m_matricesDirty = true;
	}
	
	void CameraComponent_Bgfx::follow_target(Entity* target, float smoothness)
	{
		m_followTarget = target;
		m_followSmooth = smoothness;
	}
	
	void CameraComponent_Bgfx::set_bounds(float minX, float minY, float maxX, float maxY)
	{
		m_bounds[0] = minX;
		m_bounds[1] = minY;
		m_bounds[2] = maxX;
		m_bounds[3] = maxY;
		m_hasBounds = true;
	}
	
	void CameraComponent_Bgfx::clear_bounds()
	{
		m_hasBounds = false;
	}
	
	void CameraComponent_Bgfx::update(float deltaTime)
	{
		if (m_followTarget)
		{
			follow_update(deltaTime);
		}
		
		if (m_hasBounds)
		{
			apply_bounds();
		}
		
		if (m_matricesDirty)
		{
			update_matrices();
		}
	}
	
	void CameraComponent_Bgfx::follow_update(float deltaTime)
	{
		// Get target position from a physics component or transform
		// For now, we'll just use a simple lerp approach
		// In a full implementation, we'd query the target entity's position
		
		// Smooth follow using lerp
		float lerpFactor = m_followSmooth * deltaTime * 10.0f; // Scale by 10 for reasonable speed
		if (lerpFactor > 1.0f) lerpFactor = 1.0f;
		
		m_position.x += (m_targetPosition.x - m_position.x) * lerpFactor;
		m_position.y += (m_targetPosition.y - m_position.y) * lerpFactor;
		
		m_matricesDirty = true;
	}
	
	void CameraComponent_Bgfx::apply_bounds()
	{
		// Calculate half-width and half-height of visible area
		float halfWidth = (m_orthoWidth / m_zoom) * 0.5f;
		float halfHeight = (m_orthoHeight / m_zoom) * 0.5f;
		
		// Clamp camera position to bounds
		if (m_position.x - halfWidth < m_bounds[0])
			m_position.x = m_bounds[0] + halfWidth;
		if (m_position.x + halfWidth > m_bounds[2])
			m_position.x = m_bounds[2] - halfWidth;
		if (m_position.y - halfHeight < m_bounds[1])
			m_position.y = m_bounds[1] + halfHeight;
		if (m_position.y + halfHeight > m_bounds[3])
			m_position.y = m_bounds[3] - halfHeight;
		
		m_matricesDirty = true;
	}
	
	void CameraComponent_Bgfx::update_matrices()
	{
		// Create orthographic projection matrix
		float halfWidth = (m_orthoWidth / m_zoom) * 0.5f;
		float halfHeight = (m_orthoHeight / m_zoom) * 0.5f;
		
		bx::mtxOrtho(m_proj,
			-halfWidth, halfWidth,
			-halfHeight, halfHeight,
			m_nearPlane, m_farPlane,
			0.0f,  // offset
			bgfx::getCaps()->homogeneousDepth
		);
		
		// Create view matrix
		// Camera looks at (x, y, 0) from position (x, y, 10)
		float eye[3] = { m_position.x, m_position.y, 10.0f };
		float at[3] = { m_position.x, m_position.y, 0.0f };
		float up[3] = { 0.0f, 1.0f, 0.0f };
		
		bx::mtxLookAt(m_view, eye, at, up, bx::Handness::Right);
		
		m_matricesDirty = false;
	}
	
	void CameraComponent_Bgfx::apply_camera(bgfx::ViewId viewId)
	{
		if (m_matricesDirty)
		{
			update_matrices();
		}
		
		// Set view and projection matrices
		bgfx::setViewTransform(viewId, m_view, m_proj);
		
		// Set viewport
		bgfx::setViewRect(viewId, m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
	}
	
	void CameraComponent_Bgfx::screen_to_world(float screenX, float screenY, float& worldX, float& worldY) const
	{
		// Convert screen coordinates to normalized device coordinates (-1 to 1)
		float ndcX = (screenX / (float)m_viewport[2]) * 2.0f - 1.0f;
		float ndcY = 1.0f - (screenY / (float)m_viewport[3]) * 2.0f; // Flip Y
		
		// Apply inverse projection and view
		float halfWidth = (m_orthoWidth / m_zoom) * 0.5f;
		float halfHeight = (m_orthoHeight / m_zoom) * 0.5f;
		
		worldX = m_position.x + ndcX * halfWidth;
		worldY = m_position.y + ndcY * halfHeight;
	}
	
	void CameraComponent_Bgfx::world_to_screen(float worldX, float worldY, float& screenX, float& screenY) const
	{
		// Apply view and projection
		float halfWidth = (m_orthoWidth / m_zoom) * 0.5f;
		float halfHeight = (m_orthoHeight / m_zoom) * 0.5f;
		
		float ndcX = (worldX - m_position.x) / halfWidth;
		float ndcY = (worldY - m_position.y) / halfHeight;
		
		// Convert from NDC to screen coordinates
		screenX = (ndcX + 1.0f) * 0.5f * (float)m_viewport[2];
		screenY = (1.0f - ndcY) * 0.5f * (float)m_viewport[3];
	}
	
	tmath::vector2df CameraComponent_Bgfx::screen_to_world(const tmath::vector2df& screen) const
	{
		tmath::vector2df result;
		screen_to_world(screen.x, screen.y, result.x, result.y);
		return result;
	}
	
	tmath::vector2df CameraComponent_Bgfx::world_to_screen(const tmath::vector2df& world) const
	{
		tmath::vector2df result;
		world_to_screen(world.x, world.y, result.x, result.y);
		return result;
	}
}
