////////////////////////////////////////////////////////////
//
//
////////////////////////////////////////////////////////////

#pragma once
#include <vector>
#include "math/tm.h"
#include "persist/Archive.h"

namespace Pakal
{

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

	};

	struct SpritePhysicsLoader
	{
		std::vector<SpritePhysics*> animations;

		void persist(Archive* archive)
		{
			archive->value("bodies","body",animations);
		}
		// get memory consumption
		inline size_t get_memory_consumption() const
		{
			size_t childMemory = 0;
			for(auto& child : animations)
			{
				childMemory += child->get_memory_consumption();
			}
			return sizeof(SpritePhysics)* animations.size() + sizeof(animations) + childMemory;
		}
	};

}