#pragma once
#include "Config.h"
#include "PakalMath.h"
#include <vector>

namespace Pakal
{
	class _PAKALExport MeshBuilder
	{
		std::vector<int> m_indices;
		std::vector<tmath::vector2df> m_uvs;
		std::vector<tmath::vector3df> m_positions;

	public:

		std::vector<int>& get_indices()
		{
			return m_indices;
		}
		std::vector<tmath::vector2df>& get_uvs()
		{
			return m_uvs;
		}
		std::vector<tmath::vector3df>& get_positions()
		{
			return m_positions;
		}

		void add_quad(const tmath::vector3df& p0, const tmath::vector3df& p1, const tmath::vector3df& p2, const tmath::vector3df& p3, const tmath::vector2df& uv0, const tmath::vector2df& uv1, const tmath::vector2df& uv2, const tmath::vector2df& uv3);

		void add_triangle(const tmath::vector3df& p0, const tmath::vector3df& p1, const tmath::vector3df& p2, const tmath::vector2df& uv0, const tmath::vector2df& uv1, const tmath::vector2df& uv2);

		void clear();
	};
}
