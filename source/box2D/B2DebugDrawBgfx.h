/*
* Copyright (c) 2006-2007 Erin Catto http://www.gphysics.com
*
* This software is provided 'as-is', without any express or implied
* warranty.  In no event will the authors be held liable for any damages
* arising from the use of this software.
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 2. Altered source versions must not be misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
* --
*
* bgfx version of Box2D Debug Drawing
* Adapted for bgfx graphics backend
*/

#ifndef B2DEBUGDRAWBGFX_H
#define B2DEBUGDRAWBGFX_H

#include <box2d/box2d.h>
#include <bgfx/bgfx.h>
#include <cstdint>
#include <vector>

namespace Pakal
{
	struct DebugVertex
	{
		float x, y, z;
		uint32_t color;

		static void init();
		static bgfx::VertexLayout ms_layout;
	};

	// This class implements debug drawing callbacks that are invoked
	// inside b2World::Step using bgfx for rendering
	class B2DebugDrawBgfx : public b2Draw 
	{	
	public:
		B2DebugDrawBgfx();
		~B2DebugDrawBgfx();

		void DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
		void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
		void DrawCircle(const b2Vec2& center, float radius, const b2Color& color) override;
		void DrawSolidCircle(const b2Vec2& center, float radius, const b2Vec2& axis, const b2Color& color) override;
		void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) override;
		void DrawTransform(const b2Transform& xf) override;
		void DrawPoint(const b2Vec2& p, float size, const b2Color& color);
		void DrawString(int x, int y, const char* string, ...); 
		void DrawAABB(b2AABB* aabb, const b2Color& color);

		// Rendering
		void flush();

		// Settings
		void setTranslation(float x, float y);
		void setScale(float scale);
		void setScreenCenter(float x, float y);

	protected:
		uint32_t b2ColorToRGBA(const b2Color& color, float alpha = 1.0f);
		void addVertex(const b2Vec2& v, uint32_t color);
		void addLine(const b2Vec2& p1, const b2Vec2& p2, uint32_t color);
		void addCircle(const b2Vec2& center, float radius, uint32_t color, bool filled = false);

		std::vector<DebugVertex> m_vertices;
		
		float m_translation_x = 0.0f;
		float m_translation_y = 0.0f;
		float m_scale = 1.0f;
		float m_screen_center_x = 0.0f;
		float m_screen_center_y = 0.0f;
		
		bgfx::ProgramHandle m_program;
		bgfx::VertexBufferHandle m_vbh;
		bgfx::IndexBufferHandle m_ibh;
		bool m_initialized = false;
	};
}

#endif // B2DEBUGDRAWBGFX_H
