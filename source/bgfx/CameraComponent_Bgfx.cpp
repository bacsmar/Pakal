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
#include "BgfxGraphicsSystem.h"
#include "Entity.h"
#include "EntityManager.h"
#include "LogMgr.h"
#include "components/SpritePhysicsComponent.h"
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <cmath>

namespace Pakal
{
	CameraComponent_Bgfx::CameraComponent_Bgfx(BgfxGraphicsSystem* graphicsSystem) :
		m_position(0.0f, 0.0f),
		m_zoom(1.0f),
		m_orthoWidth(800.0f),
		m_orthoHeight(600.0f),
		m_nearPlane(0.1f),
		m_farPlane(100.0f),
		m_followTarget(),
		m_followSmooth(0.1f),
		m_targetPosition(0.0f, 0.0f),
		m_hasBounds(false),
		m_matricesDirty(true),
		m_graphics_system(graphicsSystem)
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
		
		// Register with graphics system
		if (m_graphics_system)
		{
			auto setActiveCamera = [graphicsSystem = m_graphics_system, this]()
			{
				graphicsSystem->set_active_camera(this);
			};

			auto state = m_graphics_system->get_state();
			if (state == SystemState::Running || state == SystemState::Paused)
			{
				auto task = m_graphics_system->execute_block(setActiveCamera);
				if (task)
				{
					task->wait();
				}
			}
			else
			{
				setActiveCamera();
			}
		}
	}
	
	CameraComponent_Bgfx::~CameraComponent_Bgfx()
	{
		// Unregister from graphics system
		if (m_graphics_system)
		{
			auto clearActiveCamera = [graphicsSystem = m_graphics_system, this]()
			{
				graphicsSystem->clear_active_camera(this);
			};

			auto state = m_graphics_system->get_state();
			if (state == SystemState::Running || state == SystemState::Paused)
			{
				auto task = m_graphics_system->execute_block(clearActiveCamera);
				if (task)
				{
					task->wait();
				}
			}
			else
			{
				clearActiveCamera();
			}
		}
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
	
	void CameraComponent_Bgfx::set_position(const tmath::vectorn<float, 2>& pos)
	{
		m_position = pos;
		m_matricesDirty = true;
	}
	
	void CameraComponent_Bgfx::set_zoom(float zoom)
	{
		m_zoom = zoom;
		if (m_zoom < 0.1f) m_zoom = 0.1f; // Clamp minimum zoom
		m_matricesDirty = true;
		LOG_INFO("[CameraComponent_Bgfx] Zoom set to: %f", m_zoom);
	}
	
	void CameraComponent_Bgfx::follow_target(Entity* target, float smoothness)
	{
		follow_target(target ? target->get_handle() : EntityHandle{}, smoothness);

		if (target)
		{
			if (auto* physics = target->get_component<SpritePhysicsComponent>())
			{
				auto position = physics->get_position();
				m_targetPosition = { position.x, position.y };
			}
		}
	}

	void CameraComponent_Bgfx::follow_target(EntityHandle target, float smoothness)
	{
		m_followTarget = target;
		m_followSmooth = smoothness;

		if (!target)
		{
			return;
		}

		Entity* resolvedTarget = nullptr;
		if (auto* parent = get_parent_entity())
		{
			if (auto* entityManager = parent->entity_manager())
			{
				resolvedTarget = entityManager->resolve(target);
			}
		}

		if (auto* physics = resolvedTarget ? resolvedTarget->get_component<SpritePhysicsComponent>() : nullptr)
		{
			auto position = physics->get_position();
			m_targetPosition = { position.x, position.y };
		}
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
		Entity* target = nullptr;
		if (auto* parent = get_parent_entity())
		{
			if (auto* entityManager = parent->entity_manager())
			{
				target = entityManager->resolve(m_followTarget);
			}
		}

		if (!target)
		{
			m_followTarget = {};
			return;
		}

		if (auto* physics = target->get_component<SpritePhysicsComponent>())
		{
			auto position = physics->get_position();
			m_targetPosition = { position.x, position.y };
		}
		
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
			-100.0f, 100.0f,
			0.0f,  // offset
			bgfx::getCaps()->homogeneousDepth
		);

		// 2D camera view matrix: translate world by inverse camera position.
		// This avoids handedness/depth ambiguity from 3D lookAt in a 2D pipeline.
		bx::mtxTranslate(m_view, -m_position.x, -m_position.y, 0.0f);
		
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
	
	tmath::vectorn<float, 2> CameraComponent_Bgfx::screen_to_world(const tmath::vectorn<float, 2>& screen) const
	{
		tmath::vectorn<float, 2> result;
		screen_to_world(screen.x, screen.y, result.x, result.y);
		return result;
	}
	
	tmath::vectorn<float, 2> CameraComponent_Bgfx::world_to_screen(const tmath::vectorn<float, 2>& world) const
	{
		tmath::vectorn<float, 2> result;
		world_to_screen(world.x, world.y, result.x, result.y);
		return result;
	}
}
