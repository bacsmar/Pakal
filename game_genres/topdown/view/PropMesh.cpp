///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PropMesh.cpp
// Purpose: The model of a prop - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/PropMesh.h"

#include "bgfx/MeshComponent3D_Bgfx.h"

#include <cmath>

namespace Pakal::TopDown
{
	void PropMesh::show_state(bool drawn, bool broken)
	{
		if (!m_mesh)
		{
			return;
		}
		m_drawn = drawn;
		if (m_shows != Shows::Always)
		{
			// a whole model and a broken one: each as it is, one at a time
			const bool shown = drawn && (broken == (m_shows == Shows::Broken));
			m_mesh->set_matrix(shown ? m_placement.data() : std::array<float, 16>{}.data());
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

	void PropMesh::show_hit(float left)
	{
		if (!m_mesh || m_shows == Shows::Broken || !m_drawn || m_dark) return;
		if (!m_lit) m_mesh->get_color(m_color);
		m_lit = left > 0.0f;
		const float glow = 1.0f + (m_flash - 1.0f) * left;
		m_mesh->set_color(m_color[0] * (m_lit ? glow : 1.0f), m_color[1] * (m_lit ? glow : 1.0f), m_color[2] * (m_lit ? glow : 1.0f));
		// side to side along its own x, three times over the hit, smaller as it fades (left 0: where it stands)
		std::array<float, 16> matrix = m_placement;
		const float push = m_shake * left * std::sin(left * 3.0f * 6.2832f);
		const float length = std::sqrt(matrix[0] * matrix[0] + matrix[1] * matrix[1] + matrix[2] * matrix[2]);
		if (length > 1e-6f)
		{
			matrix[12] += matrix[0] / length * push;
			matrix[14] += matrix[2] / length * push;
		}
		m_mesh->set_matrix(matrix.data());
	}
}
