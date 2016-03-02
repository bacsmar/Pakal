////////////////////////////////////////////////////////////
//
//
////////////////////////////////////////////////////////////

#pragma once
#include <vector>
#include "PakalMath.h"
#include "persist/Archive.h"
#include "persist/TextReader.h"

namespace Pakal
{
	class TextReader;

	struct SpritePhysics
	{
		class vector2df : public tmath::vector2df
		{
		public:
			void persist(Archive* archive)
			{
				archive->value("x", x);
				archive->value("y", y);
			}
		};

		struct Polygon
		{
			std::vector< vector2df> m_vertices;

			void persist(Archive* archive)
			{
				archive->value("","vertex", m_vertices);
			}
			// get memory consumption
			inline size_t get_memory_consumption() const
			{
				return m_vertices.size() * sizeof(vector2df);
			}
		};

		struct Circle
		{
			float r = 0;
			float x = 0;
			float y = 0;

			void persist(Archive* archive)
			{
				archive->value("r", r);
				archive->value("x", x);
				archive->value("y", y);
			}
		};

		struct Fixture
		{
			float density;
			float friction;
			float restitution;
			bool is_sensor = false;
			std::string type;
			std::vector<Polygon> m_polygons;
			Circle m_circle;

			inline size_t get_size() const
			{
				return m_polygons.size();
			}

			void persist(Archive* archive)
			{
				archive->value("density", density);
				archive->value("friction", friction);
				archive->value("restitution", restitution);
				archive->value("type", type);
				archive->value("polygons","polygon", m_polygons);
				archive->value("circle", m_circle);
				archive->value("is_sensor", is_sensor);
			}
			// get memory consumption
			inline size_t get_memory_consumption() const
			{
				size_t childMemory = 0;
				for (auto& child : m_polygons)
				{
					childMemory += child.get_memory_consumption();
				}
				return m_polygons.size() * sizeof(Polygon) + childMemory +  sizeof(Circle);
			}
		};

		std::vector<Fixture> m_fixtures;

		std::string name;
		bool		dynamic = false;
		bool		fixed_rotation = false;
		//vector2df	position;
		bool		awake = true;
		float		gravity_scale = 1.f;

		inline size_t get_size() const
		{
			return m_fixtures.size();
		}

		void persist(Archive* archive)
		{
			archive->value("name", name);
			archive->value("dynamic", dynamic);
			archive->value("fixed_rotation", fixed_rotation);
			//archive->value("position", position);
			archive->value("awake", awake);
			archive->value("gravity_scale", gravity_scale);
			archive->value("fixtures","fixture",m_fixtures);
		}
		// get memory consumption
		inline size_t get_memory_consumption() const
		{
			size_t childMemory = 0;
			for (auto& child : m_fixtures)
			{
				childMemory += child.get_memory_consumption();
			}
			return m_fixtures.size() * sizeof(Fixture) + childMemory;
		}

		tmath::rectf get_bounding_box()
		{
			tmath::rectf bounding_box;
			for (auto& fixture : m_fixtures) 
			{
				for (auto& polygon : fixture.m_polygons)
				{
					for (auto& vertex : polygon.m_vertices)
					{
						if (vertex.x > bounding_box.size.x) bounding_box.size.x = vertex.x;
						if (vertex.y > bounding_box.size.y) bounding_box.size.y = vertex.y;

						if (vertex.x < bounding_box.left_corner.x) bounding_box.left_corner.x = vertex.x;
						if (vertex.y < bounding_box.left_corner.y) bounding_box.left_corner.y = vertex.y;
					}
				}
								
				auto& circle = fixture.m_circle;
					{
						if (circle.x + circle.r > bounding_box.size.x) bounding_box.size.x = circle.x + circle.r;
						if (circle.y + circle.r > bounding_box.size.y) bounding_box.size.y = circle.y + circle.r;

						if (circle.x - circle.r < bounding_box.left_corner.x) bounding_box.left_corner.x = circle.x - circle.r;
						if (circle.y - circle.r < bounding_box.left_corner.y) bounding_box.left_corner.y = circle.y - circle.r;
					}
				
			}
			return bounding_box;
		}

	};

	struct SpriteSheetPhysics
	{
		std::vector<SpritePhysics*> bodies;

		void persist(Archive* archive)
		{
			archive->value("bodies","body", bodies);
		}
		
		inline size_t get_memory_consumption() const
		{
			size_t childMemory = 0;
			for(auto& child : bodies)
			{
				childMemory += child->get_memory_consumption();
			}
			return sizeof(SpritePhysics)* bodies.size() + sizeof(bodies) + childMemory;
		}

		bool load(TextReader& reader, std::istream& stream)
		{
			return reader.read(stream, "bodydef", *this);			
		}
	};

}
