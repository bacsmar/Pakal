///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: OccluderFade.h
// Purpose: Buildings, trees and other tall things fade while they stand between the camera and the
//          player, and come back after. Each keeps the box of its model and its placement: the camera's
//          line to the player is tested in the model's own frame, so a turned building fades only when
//          the building itself is in the way, not the room around it.
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "topdown/MouseAim.h"

#include <string>
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
			Vec3 low{ 0, 0, 0 }, high{ 0, 0, 0 };   // its model's box (model units)
			float toModel[12] = { 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0 };   // world -> model: 3x3 (columns) and the offset
			float opacity = 1.0f;
			float faded = 0.16f;                      // its opacity while it hides the target (a tree keeps more)
		};

		// 'placement': the model's world matrix (column-major 4x4, rotation, scale and position); 'low', 'high': its model box
		static Occluder make(Pakal::MeshComponent3D_Bgfx* mesh, const float placement[16], const float low[3], const float high[3]);

		// Whether a placed level object may hide the player: its name says Building or Tree, or it is tall and wide (a kiosk,
		// an umbrella, an awning, a big sign) and not a piece of the floor ("ground", "street", "grass"...), the town's far
		// ring ("background"), the letters of a sign or a cloud
		static bool may_hide(const std::string& name, const float placement[16], const float low[3], const float high[3]);
		// The points of a character standing at 'at' (level meters) whose view counts: its chest and its head. Its feet behind
		// a low wall do not make the wall fade.
		static std::vector<Vec3> body_points(float x, float y) { return { Vec3{ x, 1.0f, -y }, Vec3{ x, 1.6f, -y } }; }

		void add(const Occluder& occluder) { m_occluders.push_back(occluder); }
		bool empty() const { return m_occluders.empty(); }
		void clear() { m_occluders.clear(); }

		// 'watching': the ones hiding any of the 'targets' (points of the player's body) from 'eye' fade out; the others (all,
		// when not watching) come back
		void update(const Vec3& eye, const std::vector<Vec3>& targets, bool watching, float dt);
		void update(const Vec3& eye, const Vec3& target, bool watching, float dt) { update(eye, std::vector<Vec3>{ target }, watching, dt); }

	private:
		std::vector<Occluder> m_occluders;
	};
}
