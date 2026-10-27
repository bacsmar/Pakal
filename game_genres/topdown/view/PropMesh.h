///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PropMesh.h
// Purpose: The model of a prop (PropComponent's view): hidden while the prop is not drawn, flattened
//          on the floor and darkened while broken.
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
		// 'mesh' is owned by the level; 'placement' is where it stands (a render matrix)
		PropMesh(Pakal::MeshComponent3D_Bgfx* mesh, const std::array<float, 16>& placement) : m_mesh(mesh), m_placement(placement) {}

		void show_state(bool drawn, bool broken) override;

	private:
		Pakal::MeshComponent3D_Bgfx* m_mesh;
		std::array<float, 16> m_placement;
		bool m_dark = false;   // the color of a broken prop is on
	};
}
