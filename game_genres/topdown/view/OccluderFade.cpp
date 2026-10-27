///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: OccluderFade.cpp
// Purpose: Models between the camera and what it follows fade out - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/OccluderFade.h"

#include "bgfx/MeshComponent3D_Bgfx.h"

#include <algorithm>

namespace Pakal::TopDown
{
	void OccluderFade::update(const Vec3& eye, const Vec3& target, bool watching, float dt)
	{
		const float margin = 0.6f;
		const float speed = 5.0f;   // opacity per second
		for (auto& occluder : m_occluders)
		{
			bool blocks = false;
			if (watching)
			{
				const Vec3 low{ occluder.low.x - margin, occluder.low.y, occluder.low.z - margin };
				const Vec3 high{ occluder.high.x + margin, occluder.high.y, occluder.high.z + margin };
				blocks = segment_hits_box(eye, target, low, high);
			}
			const float goal = blocks ? occluder.faded : 1.0f;
			const float step = speed * dt;
			occluder.opacity += std::clamp(goal - occluder.opacity, -step, step);
			occluder.mesh->set_opacity(occluder.opacity);
		}
	}
}
