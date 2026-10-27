///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: MouseAim.h
// Purpose: The point of the level under the mouse: a ray from the camera through the pixel, met with a
//          horizontal plane. Render space is Y up (level x, z, -y in meters), as in the level state.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <cmath>

namespace Pakal::TopDown
{
	struct Vec3
	{
		float x, y, z;
	};

	inline Vec3 operator-(const Vec3& a, const Vec3& b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
	inline Vec3 operator+(const Vec3& a, const Vec3& b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
	inline Vec3 operator*(float s, const Vec3& v) { return { s * v.x, s * v.y, s * v.z }; }
	inline Vec3 cross(const Vec3& a, const Vec3& b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
	inline Vec3 normalized(const Vec3& v)
	{
		const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
		return length > 0.0f ? Vec3{ v.x / length, v.y / length, v.z / length } : v;
	}

	// True when the segment a-b crosses the box lo-hi (slab method). Used to see a building between the camera and the player.
	inline bool segment_hits_box(const Vec3& a, const Vec3& b, const Vec3& lo, const Vec3& hi)
	{
		float t0 = 0.0f, t1 = 1.0f;
		const float start[3] = { a.x, a.y, a.z };
		const float delta[3] = { b.x - a.x, b.y - a.y, b.z - a.z };
		const float low[3] = { lo.x, lo.y, lo.z };
		const float high[3] = { hi.x, hi.y, hi.z };
		for (int i = 0; i < 3; ++i)
		{
			if (std::fabs(delta[i]) < 1e-8f)
			{
				if (start[i] < low[i] || start[i] > high[i]) return false;
				continue;
			}
			float entry = (low[i] - start[i]) / delta[i];
			float exit = (high[i] - start[i]) / delta[i];
			if (entry > exit) { const float t = entry; entry = exit; exit = t; }
			t0 = entry > t0 ? entry : t0;
			t1 = exit < t1 ? exit : t1;
			if (t0 > t1) return false;
		}
		return true;
	}

	// Pixel (px, py) of a width x height view (origin top left) seen by a camera at 'eye' looking at
	// 'target' (Y up) with a vertical field of view of 'fovYDegrees'. Gives the point where the ray
	// meets the plane y = planeY, in level meters (x east, y north). False when the ray misses it.
	inline bool mouse_on_plane(const Vec3& eye, const Vec3& target, float fovYDegrees, float px, float py,
		float width, float height, float planeY, float& levelX, float& levelY)
	{
		if (width <= 0.0f || height <= 0.0f)
		{
			return false;
		}
		const Vec3 forward = normalized(target - eye);
		const Vec3 right = normalized(cross(forward, Vec3{ 0.0f, 1.0f, 0.0f }));
		const Vec3 up = cross(right, forward);
		const float tanHalf = std::tan(fovYDegrees * 0.5f * 3.14159265f / 180.0f);
		const float ndcX = 2.0f * (px + 0.5f) / width - 1.0f;
		const float ndcY = 1.0f - 2.0f * (py + 0.5f) / height;
		const Vec3 direction = forward + (ndcX * tanHalf * width / height) * right + (ndcY * tanHalf) * up;
		if (std::fabs(direction.y) < 1e-6f)
		{
			return false;
		}
		const float t = (planeY - eye.y) / direction.y;
		if (t <= 0.0f)
		{
			return false;
		}
		const Vec3 hit = eye + t * direction;
		levelX = hit.x;
		levelY = -hit.z;
		return true;
	}
}
