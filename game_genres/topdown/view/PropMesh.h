///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PropMesh.h
// Purpose: The model of a prop (PropComponent's view): hidden while the prop is not drawn, flattened
//          on the floor and darkened while broken. A prop with a model of its own for when it is broken
//          (a burnt gas pump) shows that one then, and hides the whole one.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "topdown/TopDownComponents.h"

#include <array>

namespace Pakal
{
	class MeshComponent3D_Bgfx;
}

namespace Pakal::TopDown
{
	class PropMesh : public PropView
	{
	public:
		// What a model shows: the prop always (flattened and dark once broken), only while it is whole (the prop has a
		// broken model), or only once it is broken (that model)
		enum class Shows { Always, Whole, Broken };

		// 'mesh' is owned by the level; 'placement' is where it stands (a render matrix)
		PropMesh(Pakal::MeshComponent3D_Bgfx* mesh, const std::array<float, 16>& placement, Shows shows = Shows::Always)
			: m_mesh(mesh), m_placement(placement), m_shows(shows) {}
		void set_shows(Shows shows) { m_shows = shows; }
		Shows shows() const { return m_shows; }

		void show_state(bool drawn, bool broken) override;
		void set_placement(const std::array<float, 16>& placement) override { m_placement = placement; }
		// A hit: it lights up ('flash' times its color) and shakes ('shake' meters side to side), both fading out
		void show_hit(float left) override;
		void set_hit_look(float flash, float shake) { m_flash = flash; m_shake = shake; }

	private:
		Pakal::MeshComponent3D_Bgfx* m_mesh;
		std::array<float, 16> m_placement;
		Shows m_shows = Shows::Always;
		bool m_dark = false;   // the color of a broken prop is on
		bool m_drawn = true;
		float m_flash = 1.0f, m_shake = 0.0f;
		bool m_lit = false;    // a hit's flash is on (over m_color)
		float m_color[3] = { 1.0f, 1.0f, 1.0f };   // its color before the flash
	};
}
