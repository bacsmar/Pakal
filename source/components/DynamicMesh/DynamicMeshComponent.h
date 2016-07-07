#pragma once
#include "Config.h"
#include "Component.h"
#include "TaskFwd.h"
#include "DynamicMesh.h"
#include "persist/Archive.h"
#include <algorithm>


namespace Pakal
{

	class _PAKALExport DynamicMeshComponent : public Component
	{
		DECLARE_RTTI_WITH_BASE(DynamicMeshComponent,Component)
	public:

		struct Settings
		{
			std::string fill_texture;
			std::string edges_texture;

			float strech_threshold = 0.5f;
			DynamicMesh::FillMode fill_mode = DynamicMesh::FillMode::None;
			tmath::vector3di ambient_color = { 255,255,255};

			int smooth_factor = 5;
			int pixels_per_unit = 64;
			int split_corners_threshold = 90;

			bool is_closed = false;
			bool split_when_different = false;

			UVMappingPtr mapping;
			std::vector<VertexInfo> vertices;
			bool is_static_geometry = false;

			void persist(Archive* a)
			{
				a->value("uv_mapping", *mapping);
				a->value("vertices_info", "vertex", vertices);
				a->value("fill_mode", fill_mode);
				a->value("strech_threshold", strech_threshold);
				a->value("ambient_color", ambient_color);
				a->value("smooth_factor", smooth_factor);
				a->value("pixels_per_unit", pixels_per_unit);
				a->value("angle_corner_split_treshold", split_corners_threshold);
				a->value("is_closed", is_closed);
				a->value("split_when_different", split_when_different);
			}

		};

		virtual BasicTaskPtr intialize(const Settings& settings) = 0;
		virtual BasicTaskPtr terminate() = 0;

		virtual std::vector<VertexInfo>& get_vertices() = 0;
		virtual const std::vector<tmath::vector3df>& get_generated_vertices() const = 0;
		virtual void set_vertices(const std::vector<VertexInfo>&) = 0;
		virtual BasicTaskPtr tesellate() = 0;
	};

}