///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CameraRig.h
// Purpose: The camera of a top-down game: it sits at a fixed offset from what it follows (above and
//          behind it), flies a scripted pan (CameraPan) while one is active, and shakes after a blast
//          near it. Render space, meters. The simulation asks for its pose at rest (rest_pose: no
//          shake, the followed point at the step), never for the drawn one.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "RandomStreams.h"
#include "topdown/CameraPan.h"
#include "topdown/MouseAim.h"

namespace Pakal
{
	class CameraComponent3D_Bgfx;
}

namespace Pakal::TopDown
{
	class CameraRig
	{
	public:
		struct Settings
		{
			float side = -12.5f, height = 17.1f, back = 12.5f;  // the offset from the point it follows
			float fovY = 22.0f;                                  // degrees
			float panFovY = 60.0f;                               // ... during a scripted pan
			// A shake from a point is full within 'closeCm' of the camera and nothing past 'farCm'; it lasts 'closeMs' times
			// that share and jitters up to power x 'closePower' x share centimeters, fading out over its last 'fade' share
			float closeCm = 400.0f, farCm = 5000.0f, closeMs = 800.0f, closePower = 0.7f, fade = 0.25f;
			// How it follows: a camera operator easing after what it follows (a critically damped spring, about this many
			// seconds to catch up; 0: locked to it). A jump farther than 'snapMeters' (a teleport, a new level) is not eased.
			float followSeconds = 0.0f, snapMeters = 8.0f;
			// The shake moves smoothly between points picked this many times per second (not a new jolt every frame)
			float shakeHz = 20.0f;
		};

		// 'jitter' is the presentation's random stream for the shake
		CameraRig(const Settings& settings, Pakal::RandomStream& jitter) : m_settings(settings), m_jitter(jitter) {}

		// Where it sits following 'focus' at rest (no shake); 'scale' pulls it back
		void rest_pose(const Vec3& focus, float scale, Vec3& eye, Vec3& target) const;

		// This frame: the pan if one is active, else 'focus' from the offset, shaking. 'timeMs' is the game clock.
		void update(const Vec3& focus, double timeMs, float scale = 1.0f);
		// Places a camera where the rig is
		void apply(Pakal::CameraComponent3D_Bgfx& camera, int width, int height) const;

		// A blast at 'source' shakes it by how close it is
		void shake_from(const Vec3& source, float power);
		// A script's ShakeCamera: this long, jittering 'power' centimeters
		void shake(float power, float milliseconds);

		CameraPan& pan() { return m_pan; }
		bool panning() const { return m_pan.active; }
		const Vec3& eye() const { return m_eye; }
		const Vec3& target() const { return m_target; }
		float fov_y() const { return m_fovY; }
		const Settings& settings() const { return m_settings; }
		// The camera nearer (k < 1) or farther (k > 1) along the same direction, e.g. for a level whose world is at
		// another scale
		void scale_distance(float k) { m_settings.side *= k; m_settings.height *= k; m_settings.back *= k; }

	private:
		Vec3 shake_offset(double timeMs);
		Vec3 follow(const Vec3& focus, double timeMs);

		Settings m_settings;
		Pakal::RandomStream& m_jitter;
		CameraPan m_pan;
		Vec3 m_eye{ 0.0f, 0.0f, 0.0f };   // where it was last placed
		Vec3 m_target{ 0.0f, 0.0f, 0.0f };
		float m_fovY = 22.0f;
		static constexpr float kPlayFar = 5000.0f;   // meters: everything, in play
		float m_far = kPlayFar;                       // (a pan's own, while it plays)
		float m_shakeLeftMs = 0.0f, m_shakeTotalMs = 0.0f, m_shakePower = 0.0f;
		double m_shakeClockMs = -1.0;
		Vec3 m_shakeFrom{ 0.0f, 0.0f, 0.0f }, m_shakeTo{ 0.0f, 0.0f, 0.0f };
		float m_shakePhase = 1.0f;          // between m_shakeFrom and m_shakeTo (0..1)
		Vec3 m_followed{ 0.0f, 0.0f, 0.0f }, m_followSpeed{ 0.0f, 0.0f, 0.0f };
		double m_followClockMs = -1.0;
	};
}
