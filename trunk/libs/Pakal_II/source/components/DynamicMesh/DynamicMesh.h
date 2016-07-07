#pragma once
#include "Config.h"
#include "PakalMath.h"
#include "VertexInfo.h"
#include "UVMapping.h"
#include "MeshBuilder.h"


#include <vector>

namespace Pakal
{


	class _PAKALExport DynamicMesh
	{
	public:
		enum class FillMode
		{
			None = 0,
			Fill,
			Inverted
		};
	private:

		struct Segment
		{
			VertexDirection direction, prev_direction, next_direction;
			tmath::vector2df *begin, *end;
			tmath::vector2df *prev_prev, *prev, *next, *next_next;

			Segment();
			float angle_with_prev() const;
			float angle_with_next() const;
			void invert();
		};
		enum class SegmentSide
		{
			Left, Right
		};

		MeshBuilder m_edgeBuilder;
		MeshBuilder m_fillBuilder;

		UVMappingPtr m_uvMapping = nullptr;
		std::vector<VertexInfo*> m_vertices;

		FillMode m_fillMode = FillMode::None;
		float m_strechThreshold = 0.5f;
		tmath::vector3di m_ambientColor = {255,255,255};

		int m_smoothFactor  = 1;
		int m_pixelsPerUnit = 64;
		int m_splitCornersThreshold = 180;

		bool m_closed = false;
		bool m_splitWhenDifferent = true;

		bool is_inverted() const
		{
			return m_fillMode == FillMode::Inverted && m_vertices.size() > 2;
		}

		std::vector<Segment> generate_segments();
		std::vector<tmath::vector2df> draw_segment(Segment& segment);
		void draw_cap(tmath::rectf rect, SegmentSide side, tmath::vector2df top, tmath::vector2df bottom, float zOffset);
		void draw_fill(std::vector<tmath::vector2df>& fillVertices);


		VertexDirection segment_dir(VertexInfo* fst, VertexInfo* snd);
		UVMapping::Segment* uv_mapping_for(VertexDirection direction);
		bool should_close_segment(const Segment& segment, SegmentSide side);
		tmath::vector2df get_units_per_edge_uv()
		{
			return{ m_uvMapping->edge_size.x / float(m_pixelsPerUnit) , m_uvMapping->edge_size.y / float(m_pixelsPerUnit) };
		}
		tmath::vector2df get_units_per_fill_uv()
		{
			return{ m_uvMapping->fill_size.x / float(m_pixelsPerUnit) , m_uvMapping->fill_size.y / float(m_pixelsPerUnit) };
		}

	public:
		DynamicMesh(const std::vector<VertexInfo*>& vertices, UVMappingPtr mapping);
		DynamicMesh();
		virtual ~DynamicMesh();

		const MeshBuilder& get_edge_mesh() const
		{
			return m_edgeBuilder;
		}
		MeshBuilder& get_edge_mesh()
		{
			return m_edgeBuilder;
		}
		const MeshBuilder& get_fill_mesh() const
		{
			return m_fillBuilder;
		}
		MeshBuilder& get_fill_mesh()
		{
			return m_fillBuilder;
		}

		std::vector<VertexInfo*>& get_vertices()
		{
			return m_vertices;
		}
		UVMappingPtr get_uv_mapping() const
		{
			return m_uvMapping;
		}
		FillMode get_fill_mode() const
		{
			return m_fillMode;
		}
		float get_strech_threshold() const
		{
			return m_strechThreshold;
		}
		const tmath::vector3di& get_ambient_color() const
		{
			return m_ambientColor;
		}
		int get_smooth_factor() const
		{
			return m_smoothFactor;
		}
		int get_pixels_per_unit() const
		{
			return m_pixelsPerUnit;
		}
		int get_split_corners_threshold() const
		{
			return m_splitCornersThreshold;
		}
		bool get_is_closed() const
		{
			return m_closed;
		}
		bool get_split_when_different() const
		{
			return m_splitWhenDifferent;
		}

		void set_uv_mapping(UVMappingPtr uvMapping)
		{
			m_uvMapping = uvMapping;
		}
		void set_vertices(const std::vector<VertexInfo*>& vertexInfos)
		{
			m_vertices = vertexInfos;
		}
		void set_fill_mode(FillMode fillMode)
		{
			m_fillMode = fillMode;
		}
		void set_strech_threshold(float strechThreshold)
		{
			m_strechThreshold = strechThreshold;
		}
		void set_ambient_color(const tmath::vector3di& ambientColor)
		{
			m_ambientColor = ambientColor;
		}
		void set_smooth_factor(int smoothFactor)
		{
			m_smoothFactor = smoothFactor;
		}
		void set_pixels_per_unit(int pixelsPerUnit)
		{
			m_pixelsPerUnit = pixelsPerUnit;
		}
		void set_split_corners_threshold(int splitCornersThreshold)
		{
			m_splitCornersThreshold = splitCornersThreshold;
		}
		void set_is_closed(bool closed)
		{
			m_closed = closed;
		}
		void set_split_when_different(bool splitWhenDifferent)
		{
			m_splitWhenDifferent = splitWhenDifferent;
		}

		void tesellate();
	};
}
