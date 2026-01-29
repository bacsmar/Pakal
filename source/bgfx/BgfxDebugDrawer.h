///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BgfxDebugDrawer.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: january 29 2026
//
// Purpose: bgfx debug drawer implementation for Box2D visualization
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "config.h"

#if PAKAL_USE_BOX2D == 1

#include <Box2D/Box2D.h>
#include "IDebugDrawer.h"

namespace Pakal
{
	class BgfxGraphicsSystem;

	class BgfxDebugDrawer : public b2Draw
	{
	public:
		BgfxDebugDrawer(BgfxGraphicsSystem* graphicsSystem);
		virtual ~BgfxDebugDrawer();

		void DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
		void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
		void DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color) override;
		void DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color) override;
		void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) override;
		void DrawTransform(const b2Transform& xf) override;
		void DrawPoint(const b2Vec2& p, float32 size, const b2Color& color) override;

#if B2_VERSION_MAJOR >= 3
		void DrawParticles(const b2Vec2* centers, float32 radius, const b2ParticleColor* colors, int32 count) override;
#endif

		void set_translation(float x, float y);
		void set_scale(float scale);

	private:
		BgfxGraphicsSystem* m_graphics_system;
		float m_translation_x = 0.0f;
		float m_translation_y = 0.0f;
		float m_scale = 1.0f;
	};
}

#endif // PAKAL_USE_BOX2D
