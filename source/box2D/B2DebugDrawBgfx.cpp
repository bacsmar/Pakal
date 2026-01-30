/*
* bgfx version of Box2D Debug Drawing Implementation
*/

#include "B2DebugDrawBgfx.h"
#include <bgfx/bgfx.h>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Pakal
{
	bgfx::VertexLayout DebugVertex::ms_layout;

	void DebugVertex::init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.end();
	}

	B2DebugDrawBgfx::B2DebugDrawBgfx()
		: m_program(BGFX_INVALID_HANDLE)
		, m_vbh(BGFX_INVALID_HANDLE)
		, m_ibh(BGFX_INVALID_HANDLE)
	{
		DebugVertex::init();
	}

	B2DebugDrawBgfx::~B2DebugDrawBgfx()
	{
		if (bgfx::isValid(m_vbh))
		{
			bgfx::destroy(m_vbh);
		}
		if (bgfx::isValid(m_ibh))
		{
			bgfx::destroy(m_ibh);
		}
		if (bgfx::isValid(m_program))
		{
			bgfx::destroy(m_program);
		}
	}

	uint32_t B2DebugDrawBgfx::b2ColorToRGBA(const b2Color& color, float alpha)
	{
		uint32_t r = (uint32_t)(color.r * 255.0f) & 0xFF;
		uint32_t g = (uint32_t)(color.g * 255.0f) & 0xFF;
		uint32_t b = (uint32_t)(color.b * 255.0f) & 0xFF;
		uint32_t a = (uint32_t)(alpha * 255.0f) & 0xFF;
		return (a << 24) | (b << 16) | (g << 8) | r;
	}

	void B2DebugDrawBgfx::addVertex(const b2Vec2& v, uint32_t color)
	{
		DebugVertex vertex;
		vertex.x = v.x * m_scale + m_translation_x;
		vertex.y = v.y * m_scale + m_translation_y;
		vertex.z = 0.0f;
		vertex.color = color;
		m_vertices.push_back(vertex);
	}

	void B2DebugDrawBgfx::addLine(const b2Vec2& p1, const b2Vec2& p2, uint32_t color)
	{
		addVertex(p1, color);
		addVertex(p2, color);
	}

	void B2DebugDrawBgfx::DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
	{
		uint32_t rgba = b2ColorToRGBA(color);
		
		for (int32 i = 0; i < vertexCount; ++i)
		{
			addLine(vertices[i], vertices[(i + 1) % vertexCount], rgba);
		}
	}

	void B2DebugDrawBgfx::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
	{
		uint32_t rgba = b2ColorToRGBA(color, 0.5f);
		
		// Draw filled polygon (as fan of triangles from first vertex)
		for (int32 i = 1; i < vertexCount - 1; ++i)
		{
			addVertex(vertices[0], rgba);
			addVertex(vertices[i], rgba);
			addVertex(vertices[i + 1], rgba);
		}

		// Draw outline
		uint32_t outline = b2ColorToRGBA(color);
		for (int32 i = 0; i < vertexCount; ++i)
		{
			addLine(vertices[i], vertices[(i + 1) % vertexCount], outline);
		}
	}

	void B2DebugDrawBgfx::DrawCircle(const b2Vec2& center, float radius, const b2Color& color)
	{
		uint32_t rgba = b2ColorToRGBA(color);
		const int segments = 32;
		const float angleStep = 2.0f * 3.14159265f / segments;

		for (int i = 0; i < segments; ++i)
		{
			float angle1 = angleStep * i;
			float angle2 = angleStep * (i + 1);
			
			b2Vec2 p1(center.x + radius * std::cos(angle1), center.y + radius * std::sin(angle1));
			b2Vec2 p2(center.x + radius * std::cos(angle2), center.y + radius * std::sin(angle2));
			
			addLine(p1, p2, rgba);
		}
	}

	void B2DebugDrawBgfx::DrawSolidCircle(const b2Vec2& center, float radius, const b2Vec2& axis, const b2Color& color)
	{
		uint32_t rgba = b2ColorToRGBA(color, 0.5f);
		const int segments = 32;
		const float angleStep = 2.0f * 3.14159265f / segments;

		// Draw filled circle as fan of triangles
		for (int i = 0; i < segments; ++i)
		{
			float angle1 = angleStep * i;
			float angle2 = angleStep * (i + 1);
			
			b2Vec2 p1(center.x + radius * std::cos(angle1), center.y + radius * std::sin(angle1));
			b2Vec2 p2(center.x + radius * std::cos(angle2), center.y + radius * std::sin(angle2));
			
			addVertex(center, rgba);
			addVertex(p1, rgba);
			addVertex(p2, rgba);
		}

		// Draw outline
		uint32_t outline = b2ColorToRGBA(color);
		for (int i = 0; i < segments; ++i)
		{
			float angle1 = angleStep * i;
			float angle2 = angleStep * (i + 1);
			
			b2Vec2 p1(center.x + radius * std::cos(angle1), center.y + radius * std::sin(angle1));
			b2Vec2 p2(center.x + radius * std::cos(angle2), center.y + radius * std::sin(angle2));
			
			addLine(p1, p2, outline);
		}

		// Draw axis
		b2Vec2 p(center.x + radius * axis.x, center.y + radius * axis.y);
		addLine(center, p, outline);
	}

	void B2DebugDrawBgfx::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color)
	{
		uint32_t rgba = b2ColorToRGBA(color);
		addLine(p1, p2, rgba);
	}

	void B2DebugDrawBgfx::DrawTransform(const b2Transform& xf)
	{
		const float axisScale = 0.4f;
		
		// X-axis (red)
		b2Vec2 xEnd = xf.p + axisScale * b2Mul(xf.q, b2Vec2(1, 0));
		DrawSegment(xf.p, xEnd, b2Color(1, 0, 0));
		
		// Y-axis (green)
		b2Vec2 yEnd = xf.p + axisScale * b2Mul(xf.q, b2Vec2(0, 1));
		DrawSegment(xf.p, yEnd, b2Color(0, 1, 0));
	}

	void B2DebugDrawBgfx::DrawPoint(const b2Vec2& p, float size, const b2Color& color)
	{
		uint32_t rgba = b2ColorToRGBA(color);
		
		float halfSize = size * 0.5f;
		b2Vec2 p1(p.x - halfSize, p.y - halfSize);
		b2Vec2 p2(p.x + halfSize, p.y - halfSize);
		b2Vec2 p3(p.x + halfSize, p.y + halfSize);
		b2Vec2 p4(p.x - halfSize, p.y + halfSize);
		
		addLine(p1, p2, rgba);
		addLine(p2, p3, rgba);
		addLine(p3, p4, rgba);
		addLine(p4, p1, rgba);
	}

	void B2DebugDrawBgfx::DrawString(int x, int y, const char* string, ...)
	{
		// TODO: Implement text rendering with bgfx if needed
		// For now, just a stub
	}

	void B2DebugDrawBgfx::DrawAABB(b2AABB* aabb, const b2Color& color)
	{
		uint32_t rgba = b2ColorToRGBA(color);
		
		b2Vec2 p1(aabb->lowerBound.x, aabb->lowerBound.y);
		b2Vec2 p2(aabb->upperBound.x, aabb->lowerBound.y);
		b2Vec2 p3(aabb->upperBound.x, aabb->upperBound.y);
		b2Vec2 p4(aabb->lowerBound.x, aabb->upperBound.y);
		
		addLine(p1, p2, rgba);
		addLine(p2, p3, rgba);
		addLine(p3, p4, rgba);
		addLine(p4, p1, rgba);
	}

	void B2DebugDrawBgfx::flush()
	{
		if (m_vertices.empty())
		{
			return;
		}

		// Create/update vertex buffer
		const bgfx::Memory* mem = bgfx::alloc(m_vertices.size() * sizeof(DebugVertex));
		std::memcpy(mem->data, m_vertices.data(), mem->size);

		if (bgfx::isValid(m_vbh))
		{
			bgfx::destroy(m_vbh);
		}
		m_vbh = bgfx::createVertexBuffer(mem, DebugVertex::ms_layout);

		m_vertices.clear();
	}

	void B2DebugDrawBgfx::setTranslation(float x, float y)
	{
		m_translation_x = x;
		m_translation_y = y;
	}

	void B2DebugDrawBgfx::setScale(float scale)
	{
		m_scale = scale;
	}

	void B2DebugDrawBgfx::setScreenCenter(float x, float y)
	{
		m_screen_center_x = x;
		m_screen_center_y = y;
	}

}
