#include "DynamicMesh.h"
#include "MeshFns.h"
#include "UVMapping.h"
#include "poly2tri/poly2tri.h"
#include "persist/Archive.h"
#include <algorithm>
#include <memory>

using namespace Pakal;

DynamicMesh::Segment::Segment() : 
	direction(VertexDirection::Auto) , prev_direction(VertexDirection::None) , next_direction(VertexDirection::None), 
	begin(nullptr) , end(nullptr), prev_prev(nullptr), prev(nullptr) , next (nullptr), next_next(nullptr)
{
}

float DynamicMesh::Segment::angle_with_prev() const
{
	if (prev == nullptr) 
		return 180;

	float angle = math::angle_between(*end - *begin, *prev - *begin);
	return angle < 0 ? angle + 360 : angle;
}

float DynamicMesh::Segment::angle_with_next() const
{
	if (next == nullptr)
		return 180;

	float angle = math::angle_between(*end - *begin, *next - *end);
	return angle < 0 ? angle + 360 : angle;
}

void DynamicMesh::Segment::invert()
{
	math::swap(begin, end);
	math::swap(prev, next);
	math::swap(prev_prev, next_next);
	math::swap(prev_direction, next_direction);

}

VertexDirection DynamicMesh::segment_dir(VertexInfo* fst, VertexInfo* snd)
{
	if (fst->direction != VertexDirection::Auto)
		return fst->direction;

	auto normal = math::normal(fst->position,snd->position);

	if (std::abs(normal.x) > std::abs(normal.y))
	{
		return normal.x < 0
			? (is_inverted() ? VertexDirection::Left : VertexDirection::Right)
			: (is_inverted() ? VertexDirection::Right : VertexDirection::Left);
	}
	
	return normal.y < 0
		       ? (is_inverted() ? VertexDirection::Down : VertexDirection::Top)
		       : (is_inverted() ? VertexDirection::Top : VertexDirection::Down);
}

std::vector<DynamicMesh::Segment> DynamicMesh::generate_segments()
{
	auto isClosed = m_closed && m_vertices.size() > 2;

	std::vector<Segment> segments;
	segments.reserve(m_vertices.size());

	std::vector<VertexInfo*> vertices(m_vertices);
	if (isClosed) vertices.push_back(m_vertices[0]);

	int size = vertices.size();
	auto prev = vertices[0];
	for (int i = 1; i < size; i++)
	{
		auto cur = vertices[i];

		Segment segment;

		segment.prev_prev = &utils::at(m_vertices, i - 3, isClosed)->position;
		segment.prev = &utils::at(m_vertices, i - 2, isClosed)->position;
		segment.begin = &prev->position;
		segment.end = &cur->position;
		segment.next = &utils::at(m_vertices, i + 1, isClosed)->position;
		segment.next_next = &utils::at(m_vertices, i + 2, isClosed)->position;
		segment.direction = segment_dir(prev,cur);

		if (is_inverted())
			segment.invert();

		segments.push_back(segment);
		prev = cur;
	}

	size = segments.size();
	std::vector<Segment*> segmentsptrs(size);
	std::transform(segments.begin(), segments.end(), segmentsptrs.begin(), [](Segment& seg) { return &seg; });

	for (int i = 0; i < size; i++)
	{
		auto prevSeg = utils::at(segmentsptrs, i - 1, isClosed);
		auto nextSeg = utils::at(segmentsptrs, i + 1, isClosed);

		segmentsptrs[i]->prev_direction = prevSeg ? prevSeg->direction : VertexDirection::None;
		segmentsptrs[i]->next_direction = nextSeg ? nextSeg->direction : VertexDirection::None;
	}

	return segments;
}

DynamicMesh::DynamicMesh(const std::vector<VertexInfo*>& vertices, UVMappingPtr mapping) : m_uvMapping(mapping), m_vertices(vertices)
{}

DynamicMesh::DynamicMesh()
{}

DynamicMesh::~DynamicMesh()
{}

UVMapping::Segment* DynamicMesh::uv_mapping_for(VertexDirection direction)
{

	UVMapping::Segment* segment;

	switch (direction)
	{
	case Pakal::VertexDirection::Top:
		segment = &m_uvMapping->top;
		break;
	case Pakal::VertexDirection::Down:
		segment = m_uvMapping->bottom.is_valid() ? &m_uvMapping->bottom : &m_uvMapping->top;
		break;
	case Pakal::VertexDirection::Left:
		segment = m_uvMapping->left.is_valid() ? &m_uvMapping->left : m_uvMapping->right.is_valid() ? &m_uvMapping->right : &m_uvMapping->top;
		break;
	case Pakal::VertexDirection::Right:
		segment = m_uvMapping->right.is_valid() ? &m_uvMapping->right : m_uvMapping->left.is_valid() ? &m_uvMapping->left : &m_uvMapping->top;
		break;
	default:
		segment = nullptr;
		break;
	}

	return segment->is_valid() ? segment : nullptr;
}

bool DynamicMesh::should_close_segment(const Segment& segment, SegmentSide side)
{
	if (is_inverted())
	{
		side = side == SegmentSide::Left ? SegmentSide::Right : SegmentSide::Left;
	}

	//Check if the segments are different
	if (m_splitWhenDifferent && ((side == SegmentSide::Left && segment.direction != segment.prev_direction) || (side == SegmentSide::Right && segment.direction != segment.next_direction)))
	{
		return true;
	}

	double angle = side == SegmentSide::Left ? segment.angle_with_prev() : segment.angle_with_next();

	//Check if the angle between the segments breaks the threshold
	if (angle < m_splitCornersThreshold || angle >(360 - m_splitCornersThreshold))
	{
		return true;
	}

	//Checks if is beginning or end of the path
	return angle == 180 && !(side == SegmentSide::Left ? segment.prev != nullptr : segment.next != nullptr);

}

void DynamicMesh::draw_cap(tmath::rectf rect, SegmentSide side, tmath::vector2df top, tmath::vector2df bottom, float zOffset)
{
	auto capUv = m_uvMapping->to_uv(rect);
	auto capOffset = math::normal(bottom, top) * capUv.size.x * get_units_per_edge_uv().x;

	auto otherTop = side == SegmentSide::Left ? top - capOffset : top + capOffset;
	auto otherBottom = side == SegmentSide::Left ? bottom - capOffset : bottom + capOffset;


	if (side == SegmentSide::Left)
	{
		math::swap(top, otherTop);
		math::swap(bottom, otherBottom);
	}

	m_edgeBuilder.add_quad({ bottom.x, bottom.y, zOffset }, 
					   { top.x, top.y, zOffset }, 
					   { otherTop.x, otherTop.y, zOffset }, 
					   { otherBottom.x, otherBottom.y, zOffset }, 
						capUv.bottom_left(), capUv.top_left(), capUv.top_right(), capUv.bottom_right());

}

std::vector<tmath::vector2df> DynamicMesh::draw_segment(Segment& segment)
{
	auto segmentUvMapping = uv_mapping_for(segment.direction);

	if (!segmentUvMapping)
	{
		return {segment.begin, segment.end };
	}

	auto bodyUvSize = m_uvMapping->to_uv(segmentUvMapping->bodies[0]).size;
	auto unitsPerEdgeUv = get_units_per_edge_uv();
	auto bodyWidthInUnits = bodyUvSize.x * unitsPerEdgeUv.x;
	auto halfBodyHeightInUnits = bodyUvSize.y * unitsPerEdgeUv.y / 2;

	tmath::rectf bodyUV;
	auto start = *segment.begin;
	auto smoothFactor = std::max(1, m_smoothFactor);

	auto doLeftCap = should_close_segment(segment,SegmentSide::Left);
	auto doRightCap = should_close_segment(segment,SegmentSide::Right);

	if (doLeftCap)
		segment.prev_prev = segment.prev = nullptr;

	if (doRightCap)
		segment.next_next = segment.next = nullptr;

	Segment testSegment;
	testSegment.prev = segment.prev_prev;
	testSegment.begin = segment.prev;
	testSegment.end = segment.begin;
	if (segment.prev_prev != nullptr && segment.prev != nullptr && should_close_segment(testSegment, SegmentSide::Left))
		segment.prev_prev = nullptr;

	auto prevNumOfCuts = static_cast<float>(std::max(int(std::floor(math::distance(segment.prev != nullptr ? *segment.prev : *segment.begin, *segment.begin) / (bodyWidthInUnits + m_strechThreshold))), 1))*smoothFactor;
	auto endPrevious = math::hermite_lerp(*utils::first_valid({ segment.prev_prev, segment.prev,segment.begin }), *utils::first_valid({ segment.prev,segment.begin }), *segment.begin, *segment.end, prevNumOfCuts == 1 ? 0.001f : (prevNumOfCuts - 1) / prevNumOfCuts);
	auto startOffset = math::normal(start, endPrevious) * halfBodyHeightInUnits;

	if (doLeftCap)
		draw_cap(segmentUvMapping->left_cap, SegmentSide::Left, *segment.begin + startOffset, *segment.begin - startOffset, segmentUvMapping->z_offset);


	if (doLeftCap && doRightCap)
		smoothFactor = 1;

	std::vector<tmath::vector2df> fillVertices;
	auto numberOfCuts = std::max(int(std::floor(math::distance(*segment.begin, *segment.end) / (bodyWidthInUnits + m_strechThreshold))), 1) * smoothFactor;

	for (int i = 0; i < numberOfCuts; i++)
	{
		float percentEnd = (i + 1) / float(numberOfCuts);
		
		auto end = math::hermite_lerp(*utils::first_valid({ segment.prev,segment.begin }), *segment.begin, *segment.end, *utils::first_valid({ segment.next,segment.end }), percentEnd);
		auto endOffset = math::normal(end, start) * halfBodyHeightInUnits;

		auto localTopLeft = start + startOffset;
		auto localBottomLeft = start - startOffset;

		auto localTopRight = end + endOffset;
		auto localBottomRight = end - endOffset;

		fillVertices.push_back(start);
		start = end;
		startOffset = endOffset;

		if (i % smoothFactor == 0)
		{
			bodyUV = m_uvMapping->to_uv(segmentUvMapping->bodies[std::abs(reinterpret_cast<int&>(percentEnd) % int(segmentUvMapping->bodies.size()))]);
			bodyUV.size.x /= smoothFactor;
		}
		else
			bodyUV.left_corner.x += bodyUV.size.x;

		m_edgeBuilder.add_quad({ localBottomLeft.x,localBottomLeft.y,segmentUvMapping->z_offset }, 
						   { localTopLeft.x,localTopLeft.y,segmentUvMapping->z_offset }, 
						   { localTopRight.x,localTopRight.y,segmentUvMapping->z_offset }, 
						   { localBottomRight.x,localBottomRight.y,segmentUvMapping->z_offset }, 
							 bodyUV.bottom_left(), bodyUV.top_left(), bodyUV.top_right(), bodyUV.bottom_right());
	}

	if (doRightCap)
		draw_cap(segmentUvMapping->right_cap, SegmentSide::Right, *segment.end + startOffset, *segment.end - startOffset, segmentUvMapping->z_offset);

	return fillVertices;
}

void DynamicMesh::draw_fill(std::vector<tmath::vector2df>& fillVertices)
{
	if (m_vertices.size() <= 2 || m_fillMode == FillMode::None)
	{
		fillVertices.clear();
		return;
	}

	if (!m_closed)
		fillVertices.push_back(m_vertices.back()->position);

	std::vector<p2t::Point*> hole;
	std::vector<p2t::Point*> points(fillVertices.size());

	std::transform(fillVertices.begin(), fillVertices.end(),
				   points.begin(), [](const tmath::vector2df& v){ return new p2t::Point(v.x, v.y); });

	p2t::CDT* triangulator;

	if (is_inverted())
	{
		p2t::Polygon polygon(points);

		auto center = polygon.get_centroid();
		auto size = polygon.get_dimentions();

		auto topLeft = new p2t::Point(center.x - size.x, center.y + size.y);
		auto topRight = new p2t::Point(center.x + size.x, center.y + size.y);
		auto bottomLeft = new p2t::Point(center.x - size.x, center.y - size.y);
		auto bottomRight = new p2t::Point(center.x + size.x, center.y - size.y);

		hole = { bottomLeft,topLeft,topRight,bottomRight };
		
		triangulator = new p2t::CDT(hole);
		triangulator->AddHole(points);
	}
	else
	{
		triangulator = new p2t::CDT(points);
	}

	triangulator->Triangulate();
	auto&& triangles = triangulator->GetTriangles();

	auto upf = get_units_per_fill_uv();
	for(auto t : triangles)
	{
		auto& p0 = *t->GetPoint(0);
		auto& p1 = *t->GetPoint(1);
		auto& p2 = *t->GetPoint(2);

		m_fillBuilder.add_triangle({ float(p0.x),float(p0.y),0.f },
								   { float(p1.x),float(p1.y),0.f },
								   { float(p2.x),float(p2.y),0.f },
								   { float(p0.x / upf.x), float(p0.y / upf.y) },
								   { float(p1.x / upf.x), float(p1.y / upf.y) }, 
								   { float(p2.x / upf.x), float(p2.y / upf.y) });
	}

	delete triangulator;
	std::for_each(points.begin(), points.end(), std::default_delete<p2t::Point>());
	std::for_each(hole.begin(), hole.end(), std::default_delete<p2t::Point>());
}

void DynamicMesh::tesellate()
{
	m_edgeBuilder.clear();
	m_fillBuilder.clear();

	std::vector<tmath::vector2df> fillVertices;
	
	auto segments = generate_segments();
	for (auto& segment : segments)
	{
		auto&& fillPoints = draw_segment(segment);

		if (is_inverted())
			std::reverse(fillPoints.begin(), fillPoints.end());

		fillVertices.reserve(fillVertices.size() + fillPoints.size());
		fillVertices.insert(fillVertices.end(), fillPoints.begin(), fillPoints.end());
	}

	draw_fill(fillVertices);
}