///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: OccluderFade.h
// Purpose: Models that stand between the camera and what it follows (tall buildings, trees) fade out
//          while they hide it, and come back after. Render space, meters.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/MouseAim.h"

#include <vector>

namespace Pakal
{
	class MeshComponent3D_Bgfx;
}

namespace Pakal::TopDown
{
	class OccluderFade
	{
	public:
		struct Occluder
		{
			Pakal::MeshComponent3D_Bgfx* mesh = nullptr;
			Vec3 low{ 0, 0, 0 }, high{ 0, 0, 0 };   // its box
			float opacity = 1.0f;
			float faded = 0.16f;                      // its opacity while it hides the target (a tree keeps more)
		};

		void add(const Occluder& occluder) { m_occluders.push_back(occluder); }
		bool empty() const { return m_occluders.empty(); }
		void clear() { m_occluders.clear(); }

		// 'watching': the ones between 'eye' and 'target' fade out; the others (all, when not watching) come back
		void update(const Vec3& eye, const Vec3& target, bool watching, float dt);

	private:
		std::vector<Occluder> m_occluders;
	};
}
