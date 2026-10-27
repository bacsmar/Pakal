///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CameraRig.cpp
// Purpose: The camera of a top-down game - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/CameraRig.h"

#include "bgfx/CameraComponent3D_Bgfx.h"

#include <cmath>

namespace Pakal::TopDown
{
	void CameraRig::rest_pose(const Vec3& focus, float scale, Vec3& eye, Vec3& target) const
	{
		eye = { focus.x + m_settings.side * scale, focus.y + m_settings.height * scale, focus.z + m_settings.back * scale };
		target = focus;
	}

	void CameraRig::update(const Vec3& focus, double timeMs, float scale)
	{
		if (m_pan.active)
		{
			CameraPan::Point eye, target;
			m_pan.sample(timeMs, eye, target);
			m_fovY = m_settings.panFovY;
			m_eye = { eye[0], eye[1], eye[2] };
			m_target = { target[0], target[1], target[2] };
			return;
		}
		const Vec3 shake = shake_offset(timeMs);
		rest_pose(focus, scale, m_eye, m_target);
		m_eye = { m_eye.x + shake.x, m_eye.y + shake.y, m_eye.z + shake.z };
		m_fovY = m_settings.fovY;
	}

	void CameraRig::apply(Pakal::CameraComponent3D_Bgfx& camera, int width, int height) const
	{
		camera.set_viewport(0, 0, width, height);
		camera.set_perspective(m_fovY, 0.5f, 5000.0f);
		camera.set_position(m_eye.x, m_eye.y, m_eye.z);
		camera.look_at(m_target.x, m_target.y, m_target.z);
	}

	void CameraRig::shake_from(const Vec3& source, float power)
	{
		const float dx = source.x - m_eye.x, dy = source.y - m_eye.y, dz = source.z - m_eye.z;
		float distance = std::sqrt(dx * dx + dy * dy + dz * dz) * 100.0f;
		if (distance > m_settings.farCm)
		{
			return;
		}
		distance = distance < m_settings.closeCm ? 0.0f : distance - m_settings.closeCm;
		const float share = 1.0f - distance / (m_settings.farCm - m_settings.closeCm);
		m_shakeTotalMs = m_shakeLeftMs = m_settings.closeMs * share;
		m_shakePower = power * m_settings.closePower * share;
	}

	void CameraRig::shake(float power, float milliseconds)
	{
		m_shakeTotalMs = m_shakeLeftMs = milliseconds;
		m_shakePower = power;
	}

	Vec3 CameraRig::shake_offset(double timeMs)
	{
		const float elapsed = m_shakeClockMs < 0.0 ? 0.0f : static_cast<float>(timeMs - m_shakeClockMs);
		m_shakeClockMs = timeMs;
		if (m_shakeLeftMs <= 0.0f)
		{
			return { 0.0f, 0.0f, 0.0f };
		}
		m_shakeLeftMs -= elapsed;
		if (m_shakeLeftMs <= 0.0f)
		{
			return { 0.0f, 0.0f, 0.0f };
		}
		float power = m_shakePower;
		if (m_shakeLeftMs < m_shakeTotalMs * m_settings.fade)
		{
			power *= m_shakeLeftMs / (m_shakeTotalMs * m_settings.fade);
		}
		const float meters = 0.01f * power;
		return { meters * m_jitter.uniform(-1.0f, 1.0f), meters * m_jitter.uniform(-1.0f, 1.0f), meters * m_jitter.uniform(-1.0f, 1.0f) };
	}
}
