///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BgfxDebugDrawer.cpp
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: january 29 2026
//
// Purpose: bgfx debug drawer implementation for Box2D visualization
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "BgfxDebugDrawer.h"

#if PAKAL_USE_BOX2D == 1

#include "BgfxGraphicsSystem.h"
#include <bgfx/bgfx.h>
#include <cmath>

namespace Pakal
{
	BgfxDebugDrawer::BgfxDebugDrawer(BgfxGraphicsSystem* graphicsSystem)
		: m_graphics_system(graphicsSystem)
	{
	}

	BgfxDebugDrawer::~BgfxDebugDrawer()
	{
	}

	void BgfxDebugDrawer::DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
	{
		// Use bgfx debug draw API to draw polygon lines
		// For now, we'll use a simple line-based approach
		for (int32 i = 0; i < vertexCount; ++i)
		{
			const b2Vec2& v1 = vertices[i];
			const b2Vec2& v2 = vertices[(i + 1) % vertexCount];
			DrawSegment(v1, v2, color);
		}
	}

	void BgfxDebugDrawer::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
	{
		// Draw filled polygon (we'll draw outline for now as a simple implementation)
		DrawPolygon(vertices, vertexCount, color);
	}

	void BgfxDebugDrawer::DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color)
	{
		// Draw circle using line segments
		const int32 segmentCount = 16;
		const float angleStep = 2.0f * 3.14159265f / segmentCount;
		
		b2Vec2 prevPoint;
		prevPoint.x = center.x + radius;
		prevPoint.y = center.y;

		for (int32 i = 1; i <= segmentCount; ++i)
		{
			float angle = angleStep * i;
			b2Vec2 point;
			point.x = center.x + radius * cosf(angle);
			point.y = center.y + radius * sinf(angle);
			
			DrawSegment(prevPoint, point, color);
			prevPoint = point;
		}
	}

	void BgfxDebugDrawer::DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color)
	{
		// Draw circle outline
		DrawCircle(center, radius, color);
		
		// Draw radius line to show rotation
		b2Vec2 endPoint;
		endPoint.x = center.x + radius * axis.x;
		endPoint.y = center.y + radius * axis.y;
		DrawSegment(center, endPoint, color);
	}

	void BgfxDebugDrawer::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color)
	{
		// Apply translation and scale
		float x1 = (p1.x + m_translation_x) * m_scale;
		float y1 = (p1.y + m_translation_y) * m_scale;
		float x2 = (p2.x + m_translation_x) * m_scale;
		float y2 = (p2.y + m_translation_y) * m_scale;

		// Convert Box2D color to bgfx color (ABGR format)
		uint32_t bgfxColor = 
			(uint32_t)(color.a * 255.0f) << 24 |
			(uint32_t)(color.b * 255.0f) << 16 |
			(uint32_t)(color.g * 255.0f) << 8  |
			(uint32_t)(color.r * 255.0f);

		// Use bgfx debug draw to draw line
		bgfx::DebugVertex vertices[2];
		vertices[0].m_x = x1;
		vertices[0].m_y = y1;
		vertices[0].m_z = 0.0f;
		vertices[0].m_abgr = bgfxColor;

		vertices[1].m_x = x2;
		vertices[1].m_y = y2;
		vertices[1].m_z = 0.0f;
		vertices[1].m_abgr = bgfxColor;

		bgfx::dbgDrawLine(vertices[0], vertices[1], bgfxColor);
	}

	void BgfxDebugDrawer::DrawTransform(const b2Transform& xf)
	{
		const float axisScale = 0.4f;
		b2Vec2 p1 = xf.p;
		
		// Draw X axis (red)
		b2Vec2 p2 = p1 + axisScale * xf.q.GetXAxis();
		b2Color red(1.0f, 0.0f, 0.0f);
		DrawSegment(p1, p2, red);

		// Draw Y axis (green)
		p2 = p1 + axisScale * xf.q.GetYAxis();
		b2Color green(0.0f, 1.0f, 0.0f);
		DrawSegment(p1, p2, green);
	}

	void BgfxDebugDrawer::DrawPoint(const b2Vec2& p, float32 size, const b2Color& color)
	{
		// Draw a small cross to represent the point
		float halfSize = size * 0.5f;
		b2Vec2 p1(p.x - halfSize, p.y);
		b2Vec2 p2(p.x + halfSize, p.y);
		DrawSegment(p1, p2, color);

		p1 = b2Vec2(p.x, p.y - halfSize);
		p2 = b2Vec2(p.x, p.y + halfSize);
		DrawSegment(p1, p2, color);
	}

#if B2_VERSION_MAJOR >= 3
	void BgfxDebugDrawer::DrawParticles(const b2Vec2* centers, float32 radius, const b2ParticleColor* colors, int32 count)
	{
		// Draw each particle as a small circle
		for (int32 i = 0; i < count; ++i)
		{
			b2Color color;
			if (colors)
			{
				color.r = colors[i].r / 255.0f;
				color.g = colors[i].g / 255.0f;
				color.b = colors[i].b / 255.0f;
				color.a = colors[i].a / 255.0f;
			}
			else
			{
				color.Set(1.0f, 1.0f, 1.0f);
			}
			DrawSolidCircle(centers[i], radius, b2Vec2(0, 0), color);
		}
	}
#endif

	void BgfxDebugDrawer::set_translation(float x, float y)
	{
		m_translation_x = x;
		m_translation_y = y;
	}

	void BgfxDebugDrawer::set_scale(float scale)
	{
		m_scale = scale;
	}
}

#endif // PAKAL_USE_BOX2D
