#pragma once
#include "PakalMath.h"
#include <vector>
#include <memory>

namespace Pakal
{
	class Archive;

	struct _PAKALExport UVMapping
	{
		struct Segment
		{
			float z_offset;
			tmath::rectf left_cap;
			tmath::rectf right_cap;
			std::vector<tmath::rectf> bodies;

			bool is_valid();
			void persist(Archive* a);
		};

		tmath::vector2di edge_size;
		tmath::vector2di fill_size;
		Segment top, left, right, bottom;

		void persist(Archive* a);
		tmath::rectf to_uv(tmath::rectf rect);
	};

	using UVMappingPtr = std::shared_ptr<UVMapping>;

}


