///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: OccluderFade.cpp
// Purpose: Buildings that fade while they hide the player - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////
#include "topdown/view/OccluderFade.h"

#include "bgfx/MeshComponent3D_Bgfx.h"

#include <algorithm>
#include <cmath>

namespace Pakal::TopDown
{
	namespace
	{
		Vec3 to_model(const float m[12], const Vec3& p)
		{
			const float x = p.x + m[9], y = p.y + m[10], z = p.z + m[11];
			return { m[0] * x + m[3] * y + m[6] * z, m[1] * x + m[4] * y + m[7] * z, m[2] * x + m[5] * y + m[8] * z };
		}
	}

	OccluderFade::Occluder OccluderFade::make(Pakal::MeshComponent3D_Bgfx* mesh, const float placement[16], const float low[3], const float high[3])
	{
		Occluder occluder;
		occluder.mesh = mesh;
		occluder.low = { low[0], low[1], low[2] };
		occluder.high = { high[0], high[1], high[2] };
		// the inverse of the placement's 3x3 (rotation and scale), and minus its position: model = R^-1 (world - position)
		const float a = placement[0], b = placement[4], c = placement[8];
		const float d = placement[1], e = placement[5], f = placement[9];
		const float g = placement[2], h = placement[6], i = placement[10];
		const float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
		if (std::fabs(det) < 1e-9f) return occluder;
		const float k = 1.0f / det;
		// columns of the inverse (rows of the inverse, read as row-major: inv[r][c] at index c * 3 + r)
		const float inv[3][3] = { { (e * i - f * h) * k, (c * h - b * i) * k, (b * f - c * e) * k },
		                          { (f * g - d * i) * k, (a * i - c * g) * k, (c * d - a * f) * k },
		                          { (d * h - e * g) * k, (b * g - a * h) * k, (a * e - b * d) * k } };
		for (int r = 0; r < 3; ++r)
		{
			for (int col = 0; col < 3; ++col) occluder.toModel[col * 3 + r] = inv[r][col];
		}
		occluder.toModel[9] = -placement[12];
		occluder.toModel[10] = -placement[13];
		occluder.toModel[11] = -placement[14];
		return occluder;
	}

	bool OccluderFade::may_hide(const std::string& name, const float placement[16], const float low[3], const float high[3])
	{
		auto starts = [&name](const char* prefix) { return name.rfind(prefix, 0) == 0; };
		if (starts("Building") || starts("Tree")) return true;
		if (starts("ground") || starts("street") || starts("background") || starts("grass") || starts("letter") || starts("cloud")) return false;
		auto axis = [placement](int c) { return std::sqrt(placement[c * 4] * placement[c * 4] + placement[c * 4 + 1] * placement[c * 4 + 1] + placement[c * 4 + 2] * placement[c * 4 + 2]); };
		const float tall = (high[1] - low[1]) * axis(1);
		const float wide = (high[0] - low[0]) * axis(0) * (high[2] - low[2]) * axis(2);
		return tall >= 2.2f && tall < 40.0f && wide >= 3.0f;
	}

	void OccluderFade::update(const Vec3& eye, const std::vector<Vec3>& targets, bool watching, float dt)
	{
		const float speed = 5.0f;   // opacity per second
		for (auto& occluder : m_occluders)
		{
			bool blocks = false;
			if (watching)
			{
				const Vec3 from = to_model(occluder.toModel, eye);
				for (const Vec3& target : targets)
				{
					if (segment_hits_box(from, to_model(occluder.toModel, target), occluder.low, occluder.high))
					{
						blocks = true;
						break;
					}
				}
			}
			const float goal = blocks ? occluder.faded : 1.0f;
			const float step = speed * dt;
			occluder.opacity += std::clamp(goal - occluder.opacity, -step, step);
			occluder.mesh->set_opacity(occluder.opacity);
		}
	}
}
