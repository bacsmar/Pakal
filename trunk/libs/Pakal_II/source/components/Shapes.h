#pragma once
#include "Config.h"
#include "math/tm.h"

#include <vector>

namespace Pakal
{
	enum class ShapeType
	{
		CircleShape,
		PolygonShape,
		BoxShape,
		EdgeShape,
		ChainShape,
	};

	struct _PAKALExport BodyShape
	{
		ShapeType m_shape_type;

		explicit BodyShape(ShapeType s) : m_shape_type(s) {}
		virtual ~BodyShape() {}
	};

	struct _PAKALExport CircleShape : BodyShape
	{
		float radius;

		CircleShape() : BodyShape(ShapeType::CircleShape) { }
	};

	struct _PAKALExport PolygonShape : BodyShape
	{
		PolygonShape() : BodyShape(ShapeType::PolygonShape) { }

		void set_polygon(const tmath::vector3df vertices[],int count)
		{
			for (int i = 0; i < count; i++)
			{
				this->vertices.push_back(vertices[i]);
			}
		}
		
		std::vector<tmath::vector3df> vertices;
	};

	struct _PAKALExport BoxShape : BodyShape
	{
		BoxShape() : BodyShape(ShapeType::BoxShape) { }

		void set_as_box(const tmath::vector3df& dimensions, const tmath::vector3df& center = tmath::vector3df(0,0,0), const tmath::vector3df& angle = tmath::vector3df(0,0,0))
		{
			this->dimensions = dimensions;
			this->center = center;
			this->angle = angle;
		}

		tmath::vector3df dimensions;
		tmath::vector3df center;
		tmath::vector3df angle;
	};

}