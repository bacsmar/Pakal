///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CameraComponent3D_Bgfx.h
//
// Purpose: 3D perspective camera component for bgfx
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Config.h"
#include "ICameraComponent_Bgfx.h"
#include <ostream>
#include <type_traits>
#include <cmath>
#include "math/vector3.h"
#include <bgfx/bgfx.h>
#include "components/CameraComponent3D.h"

namespace Pakal
{
	class BgfxGraphicsSystem;

	class _PAKALExport CameraComponent3D_Bgfx : public CameraComponent3D, public ICameraComponent_Bgfx
	{
		DECLARE_RTTI_WITH_BASE(CameraComponent3D_Bgfx, CameraComponent3D);

	public:
		explicit CameraComponent3D_Bgfx(BgfxGraphicsSystem* graphicsSystem = nullptr);
		virtual ~CameraComponent3D_Bgfx();

		void set_perspective(float fovYDegrees, float nearPlane = 0.1f, float farPlane = 2000.0f);
		void set_viewport(int x, int y, int width, int height);

		void set_position(float x, float y, float z);
		void set_position(const tmath::vectorn<float, 3>& pos);
		tmath::vectorn<float, 3> get_position() const { return m_position; }

		void set_rotation(float yawRadians, float pitchRadians);
		float get_yaw() const { return m_yaw; }
		float get_pitch() const { return m_pitch; }

		void look_at(float x, float y, float z);
		void clear_look_at();

		void update(float deltaTime);
		void apply_camera(bgfx::ViewId viewId) override;

		const float* get_view_matrix() const { return m_view; }
		const float* get_projection_matrix() const { return m_proj; }

	private:
		void update_matrices();

		float m_view[16];
		float m_proj[16];

		tmath::vectorn<float, 3> m_position;
		float m_yaw;
		float m_pitch;

		bool m_hasLookAt;
		tmath::vectorn<float, 3> m_lookAtTarget;

		float m_fovYDegrees;
		float m_nearPlane;
		float m_farPlane;
		int m_viewport[4];

		bool m_matricesDirty;
		BgfxGraphicsSystem* m_graphics_system;
	};
}
