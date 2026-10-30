///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LevelPlacement.h
// Purpose: Where a level document's object stands in render space: its position (level units, Z up),
//          its rotation (degrees about the level's X, Y, Z, applied Rz*Ry*Rx) and its uniform scale, as
//          a column-major matrix (bgfx's layout). Models are Y up and already in meters. An object with a basis has
//          its own rotation and scale matrix instead.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "LevelDocument.h"

#include <array>
#include <cmath>

namespace Pakal::TopDown
{
	using Mat4 = std::array<float, 16>;

	inline Mat4 mat4_mul(const Mat4& a, const Mat4& b)
	{
		Mat4 out{};
		for (int c = 0; c < 4; ++c)
		{
			for (int r = 0; r < 4; ++r)
			{
				float sum = 0.0f;
				for (int k = 0; k < 4; ++k) sum += a[k * 4 + r] * b[c * 4 + k];
				out[c * 4 + r] = sum;
			}
		}
		return out;
	}

	inline Mat4 level_placement(const Pakal::LevelObject& object, double unitsPerMeter)
	{
		const float degToRad = 3.14159265f / 180.0f, halfPi = 1.5707963f;
		auto rotation_x = [](float t) { const float c = std::cos(t), s = std::sin(t); return Mat4{ 1, 0, 0, 0, 0, c, s, 0, 0, -s, c, 0, 0, 0, 0, 1 }; };
		auto rotation_y = [](float t) { const float c = std::cos(t), s = std::sin(t); return Mat4{ c, 0, -s, 0, 0, 1, 0, 0, s, 0, c, 0, 0, 0, 0, 1 }; };
		auto rotation_z = [](float t) { const float c = std::cos(t), s = std::sin(t); return Mat4{ c, s, 0, 0, -s, c, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 }; };
		if (object.has_basis)
		{
			// its own rotation and scale, already in render space
			const auto& b = object.basis;
			return Mat4{ float(b[0]), float(b[1]), float(b[2]), 0, float(b[3]), float(b[4]), float(b[5]), 0, float(b[6]), float(b[7]), float(b[8]), 0,
				static_cast<float>(object.x / unitsPerMeter), static_cast<float>(object.z / unitsPerMeter), static_cast<float>(-object.y / unitsPerMeter), 1 };
		}
		const float s = static_cast<float>(object.scale);
		const Mat4 translate{ 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0,
			static_cast<float>(object.x / unitsPerMeter), static_cast<float>(object.z / unitsPerMeter), static_cast<float>(-object.y / unitsPerMeter), 1 };
		const Mat4 scaling{ s, 0, 0, 0, 0, s, 0, 0, 0, 0, s, 0, 0, 0, 0, 1 };
		// translate * toRender (level Z up -> render Y up) * Rz * Ry * Rx * toLevel (model Y up -> level Z up) * scale
		return mat4_mul(mat4_mul(mat4_mul(mat4_mul(translate, rotation_x(-halfPi)), rotation_z(static_cast<float>(object.rot[2]) * degToRad)),
			rotation_y(static_cast<float>(object.rot[1]) * degToRad)),
			mat4_mul(rotation_x(static_cast<float>(object.rot[0]) * degToRad), mat4_mul(rotation_x(halfPi), scaling)));
	}
}
