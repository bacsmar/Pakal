///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PropMesh.cpp
// Purpose: The model of a prop - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/PropMesh.h"

#include "bgfx/MeshComponent3D_Bgfx.h"

namespace Pakal::TopDown
{
	void PropMesh::show_state(bool drawn, bool broken)
	{
		if (!m_mesh)
		{
			return;
		}
		// The color changes only when it breaks or is repaired (the level may tint it otherwise)
		if (broken != m_dark)
		{
			m_dark = broken;
			if (broken) m_mesh->set_color(0.3f, 0.3f, 0.3f);
			else m_mesh->set_color(1.0f, 1.0f, 1.0f);
		}
		if (!drawn)
		{
			m_mesh->set_matrix(std::array<float, 16>{}.data()); // all zero: nothing is drawn
			return;
		}
		std::array<float, 16> matrix = m_placement;
		if (broken)
		{
			// Flattened on the floor: the vertical (render Y) part of each axis shrinks, the position stays
			matrix[1] *= 0.35f;
			matrix[5] *= 0.35f;
			matrix[9] *= 0.35f;
		}
		m_mesh->set_matrix(matrix.data());
	}
}
