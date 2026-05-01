///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CameraComponent3D_Bgfx.cpp
//
// Purpose: 3D perspective camera component for bgfx - Implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "CameraComponent3D_Bgfx.h"
#include "BgfxGraphicsSystem.h"
#include <bx/math.h>
#include <algorithm>

namespace Pakal
{
	CameraComponent3D_Bgfx::CameraComponent3D_Bgfx(BgfxGraphicsSystem* graphicsSystem) :
		m_position(0.0f, 0.0f, 10.0f),
		m_yaw(0.0f),
		m_pitch(0.0f),
		m_hasLookAt(false),
		m_lookAtTarget(0.0f, 0.0f, 0.0f),
		m_fovYDegrees(60.0f),
		m_nearPlane(0.1f),
		m_farPlane(2000.0f),
		m_matricesDirty(true),
		m_graphics_system(graphicsSystem)
	{
		m_viewport[0] = 0;
		m_viewport[1] = 0;
		m_viewport[2] = 1280;
		m_viewport[3] = 720;

		bx::mtxIdentity(m_view);
		bx::mtxIdentity(m_proj);

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

	CameraComponent3D_Bgfx::~CameraComponent3D_Bgfx()
	{
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

	void CameraComponent3D_Bgfx::set_perspective(float fovYDegrees, float nearPlane, float farPlane)
	{
		m_fovYDegrees = std::max(1.0f, fovYDegrees);
		m_nearPlane = nearPlane;
		m_farPlane = farPlane;
		m_matricesDirty = true;
	}

	void CameraComponent3D_Bgfx::set_viewport(int x, int y, int width, int height)
	{
		m_viewport[0] = x;
		m_viewport[1] = y;
		m_viewport[2] = width;
		m_viewport[3] = height;
		m_matricesDirty = true;
	}

	void CameraComponent3D_Bgfx::set_position(float x, float y, float z)
	{
		m_position.x = x;
		m_position.y = y;
		m_position.z = z;
		m_matricesDirty = true;
	}

	void CameraComponent3D_Bgfx::set_position(const tmath::vectorn<float, 3>& pos)
	{
		m_position = pos;
		m_matricesDirty = true;
	}

	void CameraComponent3D_Bgfx::set_rotation(float yawRadians, float pitchRadians)
	{
		m_yaw = yawRadians;
		m_pitch = std::clamp(pitchRadians, -1.55f, 1.55f);
		m_hasLookAt = false;
		m_matricesDirty = true;
	}

	void CameraComponent3D_Bgfx::look_at(float x, float y, float z)
	{
		m_lookAtTarget.x = x;
		m_lookAtTarget.y = y;
		m_lookAtTarget.z = z;
		m_hasLookAt = true;
		m_matricesDirty = true;
	}

	void CameraComponent3D_Bgfx::clear_look_at()
	{
		m_hasLookAt = false;
		m_matricesDirty = true;
	}

	void CameraComponent3D_Bgfx::update(float deltaTime)
	{
		(void)deltaTime;
		if (m_matricesDirty)
		{
			update_matrices();
		}
	}

	void CameraComponent3D_Bgfx::update_matrices()
	{
		const float aspect = m_viewport[3] > 0 ? (float)m_viewport[2] / (float)m_viewport[3] : (16.0f / 9.0f);
		bx::mtxProj(m_proj, m_fovYDegrees, aspect, m_nearPlane, m_farPlane, bgfx::getCaps()->homogeneousDepth);

		bx::Vec3 eye = { m_position.x, m_position.y, m_position.z };
		bx::Vec3 up = { 0.0f, 1.0f, 0.0f };
		bx::Vec3 at = { 0.0f, 0.0f, 0.0f };

		if (m_hasLookAt)
		{
			at.x = m_lookAtTarget.x;
			at.y = m_lookAtTarget.y;
			at.z = m_lookAtTarget.z;
		}
		else
		{
			const float cosPitch = std::cos(m_pitch);
			at.x = m_position.x + std::sin(m_yaw) * cosPitch;
			at.y = m_position.y + std::sin(m_pitch);
			at.z = m_position.z + std::cos(m_yaw) * cosPitch;
		}

		bx::mtxLookAt(m_view, eye, at, up);
		m_matricesDirty = false;
	}

	void CameraComponent3D_Bgfx::apply_camera(bgfx::ViewId viewId)
	{
		if (m_matricesDirty)
		{
			update_matrices();
		}

		bgfx::setViewTransform(viewId, m_view, m_proj);
		bgfx::setViewRect(viewId, m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
	}
}
