#include "MeshBuilder.h"

using namespace std;

void Pakal::MeshBuilder::add_quad(const tmath::vector3df& p0, const tmath::vector3df& p1, const tmath::vector3df& p2, const tmath::vector3df& p3, const tmath::vector2df& uv0, const tmath::vector2df& uv1, const tmath::vector2df& uv2, const tmath::vector2df& uv3)
{
	int count = m_positions.size();

	m_positions.push_back(p0);
	m_positions.push_back(p1);
	m_positions.push_back(p2);
	m_positions.push_back(p3);

	m_uvs.push_back(uv0);
	m_uvs.push_back(uv1);
	m_uvs.push_back(uv2);
	m_uvs.push_back(uv3);

	m_indices.push_back(count + 0);
	m_indices.push_back(count + 1);
	m_indices.push_back(count + 2);
	m_indices.push_back(count + 2);
	m_indices.push_back(count + 3);
	m_indices.push_back(count + 0);
}

void Pakal::MeshBuilder::add_triangle(const tmath::vector3df& p0, const tmath::vector3df& p1, const tmath::vector3df& p2, const tmath::vector2df& uv0, const tmath::vector2df& uv1, const tmath::vector2df& uv2)
{
	int count = m_positions.size();

	m_positions.push_back(p0);
	m_positions.push_back(p1);
	m_positions.push_back(p2);

	m_uvs.push_back(uv0);
	m_uvs.push_back(uv1);
	m_uvs.push_back(uv2);

	m_indices.push_back(count + 0);
	m_indices.push_back(count + 1);
	m_indices.push_back(count + 2);
}

void Pakal::MeshBuilder::clear()
{
	m_indices.clear();
	m_uvs.clear();
	m_positions.clear();
}
