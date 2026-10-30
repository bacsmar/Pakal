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
			float fovY = 0.0f;
			m_pan.sample(timeMs, eye, target, fovY);
			m_fovY = fovY > 0.0f ? fovY : m_settings.panFovY;
			m_far = m_pan.farMeters > 0.0f ? m_pan.farMeters : kPlayFar;
			m_eye = { eye[0], eye[1], eye[2] };
			m_target = { target[0], target[1], target[2] };
			return;
		}
		m_far = kPlayFar;
		const Vec3 shake = shake_offset(timeMs);
		rest_pose(follow(focus, timeMs), scale, m_eye, m_target);
		m_eye = { m_eye.x + shake.x, m_eye.y + shake.y, m_eye.z + shake.z };
		m_fovY = m_settings.fovY;
	}

	// Unity's SmoothDamp (Game Programming Gems 4, 'Critically Damped Ease-In/Ease-Out Smoothing'), per axis
	Vec3 CameraRig::follow(const Vec3& focus, double timeMs)
	{
		const bool first = m_followClockMs < 0.0;
		const float dt = first ? 0.0f : static_cast<float>(timeMs - m_followClockMs) / 1000.0f;
		m_followClockMs = timeMs;
		const float dx = focus.x - m_followed.x, dy = focus.y - m_followed.y, dz = focus.z - m_followed.z;
		if (m_settings.followSeconds <= 0.0f || first || dx * dx + dy * dy + dz * dz > m_settings.snapMeters * m_settings.snapMeters)
		{
			m_followed = focus;
			m_followSpeed = { 0.0f, 0.0f, 0.0f };
			return focus;
		}
		if (dt <= 0.0f) return m_followed;
		const float omega = 2.0f / m_settings.followSeconds;
		const float x = omega * dt;
		const float decay = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
		auto axis = [&](float& value, float& speed, float goal)
		{
			const float change = value - goal;
			const float temp = (speed + omega * change) * dt;
			speed = (speed - omega * temp) * decay;
			value = goal + (change + temp) * decay;
		};
		axis(m_followed.x, m_followSpeed.x, focus.x);
		axis(m_followed.y, m_followSpeed.y, focus.y);
		axis(m_followed.z, m_followSpeed.z, focus.z);
		return m_followed;
	}

	void CameraRig::apply(Pakal::CameraComponent3D_Bgfx& camera, int width, int height) const
	{
		camera.set_viewport(0, 0, width, height);
		camera.set_perspective(m_fovY, 0.5f, m_far);
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
		// from one random point to the next, shakeHz of them a second, eased: a tremor, not a jolt every frame
		m_shakePhase += elapsed / 1000.0f * m_settings.shakeHz;
		while (m_shakePhase >= 1.0f)
		{
			m_shakePhase -= 1.0f;
			m_shakeFrom = m_shakeTo;
			m_shakeTo = { m_jitter.uniform(-1.0f, 1.0f), m_jitter.uniform(-1.0f, 1.0f), m_jitter.uniform(-1.0f, 1.0f) };
		}
		const float t = m_shakePhase * m_shakePhase * (3.0f - 2.0f * m_shakePhase);
		const float meters = 0.01f * power;
		return { meters * (m_shakeFrom.x + (m_shakeTo.x - m_shakeFrom.x) * t), meters * (m_shakeFrom.y + (m_shakeTo.y - m_shakeFrom.y) * t),
			meters * (m_shakeFrom.z + (m_shakeTo.z - m_shakeFrom.z) * t) };
	}
}
