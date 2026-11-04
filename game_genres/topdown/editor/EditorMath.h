///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EditorMath.h
// Purpose: The editor's geometry: a 4x4 inverse, the ray under a point of the screen (from the view and
//          projection), a ray against a model's box through its placement, and a placement matrix taken
//          apart into the level document's terms (position in level units, rotation in degrees, scale),
//          so what a gizmo did becomes an operation on the document.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/view/LevelPlacement.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Pakal::TopDown::EditorMath
{
	struct Ray
	{
		std::array<float, 3> origin{}, direction{};
	};

	// Inverse of a column-major 4x4 matrix; false when it has none
	inline bool inverse(const Mat4& m, Mat4& out)
	{
		Mat4 inv{};
		inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
		inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
		inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
		inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
		inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
		inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
		inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
		inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
		inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
		inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
		inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
		inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
		inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
		inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
		inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
		inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
		const float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
		if (std::fabs(det) < 1e-12f) return false;
		for (int i = 0; i < 16; ++i) out[i] = inv[i] / det;
		return true;
	}

	inline std::array<float, 4> transform(const Mat4& m, float x, float y, float z, float w)
	{
		return { m[0] * x + m[4] * y + m[8] * z + m[12] * w, m[1] * x + m[5] * y + m[9] * z + m[13] * w,
			m[2] * x + m[6] * y + m[10] * z + m[14] * w, m[3] * x + m[7] * y + m[11] * z + m[15] * w };
	}

	// Where a point of render space is drawn (pixels, x right, y down); false when it is behind the camera
	inline bool to_screen(const float* view, const float* projection, float x, float y, float z, float width, float height, float& sx, float& sy)
	{
		Mat4 v{}, p{};
		std::copy(view, view + 16, v.begin());
		std::copy(projection, projection + 16, p.begin());
		const std::array<float, 4> seen = transform(v, x, y, z, 1.0f);
		const std::array<float, 4> clip = transform(p, seen[0], seen[1], seen[2], seen[3]);
		if (clip[3] <= 1e-5f) return false;
		sx = (clip[0] / clip[3] * 0.5f + 0.5f) * width;
		sy = (0.5f - clip[1] / clip[3] * 0.5f) * height;
		return true;
	}

	// The ray under a pixel (x right, y down) of a picture 'width' x 'height', in render space. Works with either depth
	// range of clip space: the two points it takes (depth 0 and 1) are both on the ray.
	inline bool screen_ray(const float* view, const float* projection, float x, float y, float width, float height, Ray& ray)
	{
		Mat4 v, p, inv;
		std::copy(view, view + 16, v.begin());
		std::copy(projection, projection + 16, p.begin());
		if (!inverse(mat4_mul(p, v), inv)) return false;
		const float nx = 2.0f * x / width - 1.0f, ny = 1.0f - 2.0f * y / height;
		auto near_ = transform(inv, nx, ny, 0.0f, 1.0f), far_ = transform(inv, nx, ny, 1.0f, 1.0f);
		if (std::fabs(near_[3]) < 1e-12f || std::fabs(far_[3]) < 1e-12f) return false;
		float length = 0.0f;
		for (int i = 0; i < 3; ++i)
		{
			ray.origin[i] = near_[i] / near_[3];
			ray.direction[i] = far_[i] / far_[3] - ray.origin[i];
			length += ray.direction[i] * ray.direction[i];
		}
		length = std::sqrt(length);
		if (length < 1e-12f) return false;
		for (auto& d : ray.direction) d /= length;
		return true;
	}

	// Where the ray meets a model's box (model space 'low'..'high') placed by 'placement': the distance along the ray (in
	// render meters), or a negative number when it misses
	inline float hit_box(const Ray& ray, const Mat4& placement, const float low[3], const float high[3])
	{
		Mat4 inv;
		if (!inverse(placement, inv)) return -1.0f;
		const auto o = transform(inv, ray.origin[0], ray.origin[1], ray.origin[2], 1.0f);
		const auto d = transform(inv, ray.direction[0], ray.direction[1], ray.direction[2], 0.0f);
		float enter = -1e30f, leave = 1e30f;
		for (int i = 0; i < 3; ++i)
		{
			if (std::fabs(d[i]) < 1e-12f)
			{
				if (o[i] < low[i] || o[i] > high[i]) return -1.0f;
				continue;
			}
			float t0 = (low[i] - o[i]) / d[i], t1 = (high[i] - o[i]) / d[i];
			if (t0 > t1) std::swap(t0, t1);
			enter = std::max(enter, t0);
			leave = std::min(leave, t1);
			if (enter > leave) return -1.0f;
		}
		if (leave < 0.0f) return -1.0f;
		// the distance is measured in render space: a scaled model's parameter along the ray is not meters
		const float t = std::max(enter, 0.0f);
		const std::array<float, 4> at{ o[0] + d[0] * t, o[1] + d[1] * t, o[2] + d[2] * t, 1.0f };
		const auto world = transform(placement, at[0], at[1], at[2], 1.0f);
		float distance = 0.0f;
		for (int i = 0; i < 3; ++i) distance += (world[i] - ray.origin[i]) * (world[i] - ray.origin[i]);
		return std::sqrt(distance);
	}

	// Where the ray meets the floor (render y = height): false when it goes away from it
	inline bool hit_floor(const Ray& ray, float height, float& x, float& z)
	{
		if (std::fabs(ray.direction[1]) < 1e-6f) return false;
		const float t = (height - ray.origin[1]) / ray.direction[1];
		if (t < 0.0f) return false;
		x = ray.origin[0] + ray.direction[0] * t;
		z = ray.origin[2] + ray.direction[2] * t;
		return true;
	}

	// A placement matrix (level_placement's) taken apart: position in level units, rotation in degrees (applied
	// Rz*Ry*Rx about the level's Z-up axes) and a uniform scale (the first axis' length)
	inline void decompose(const Mat4& m, double unitsPerMeter, double position[3], double rotation[3], double& scale)
	{
		position[0] = m[12] * unitsPerMeter;
		position[1] = -m[14] * unitsPerMeter;
		position[2] = m[13] * unitsPerMeter;
		scale = std::sqrt(double(m[0]) * m[0] + double(m[1]) * m[1] + double(m[2]) * m[2]);
		if (scale < 1e-9) scale = 1e-9;
		// R = toRender^-1 * (M / scale) * toLevel^-1, toRender = Rx(-90), toLevel = Rx(+90)
		const float halfPi = 1.5707963f;
		auto rotation_x = [](float t) { const float c = std::cos(t), s = std::sin(t); return Mat4{ 1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1 }; };
		Mat4 a = m;
		a[12] = a[13] = a[14] = 0.0f;
		for (int i = 0; i < 12; ++i) a[i] = static_cast<float>(a[i] / scale);
		const Mat4 r = mat4_mul(mat4_mul(rotation_x(halfPi), a), rotation_x(-halfPi));
		auto at = [&](int row, int column) { return double(r[column * 4 + row]); };   // column-major
		const double toDegrees = 180.0 / 3.14159265358979;
		const double sinY = std::clamp(-at(2, 0), -1.0, 1.0);
		rotation[1] = std::asin(sinY) * toDegrees;
		if (std::fabs(sinY) < 0.9999)
		{
			rotation[0] = std::atan2(at(2, 1), at(2, 2)) * toDegrees;
			rotation[2] = std::atan2(at(1, 0), at(0, 0)) * toDegrees;
		}
		else
		{
			// gimbal lock: all of the turn about Z
			rotation[0] = 0.0;
			rotation[2] = std::atan2(-at(0, 1), at(1, 1)) * toDegrees;
		}
	}
}
