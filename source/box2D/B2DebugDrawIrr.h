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
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 3. This notice may not be removed or altered from any source distribution.
* --
*
* Irrlicht version of Box2D Debug Drawing
* Copyright 2010 Jetro Lauha http://jet.ro
* See above for license.
*/

#ifndef B2DEBUGDRAWIRR_H
#define B2DEBUGDRAWIRR_H

#include <Box2D.h>
#include "irrlicht.h"

struct b2AABB;

namespace irr
{
	class IrrlichtDevice; 
	namespace video { class IVideoDriver; }	
}


// This class implements debug drawing callbacks that are invoked
// inside b2World::Step.
class B2DebugDrawIrr : public b2Draw 
{	
public:
    B2DebugDrawIrr(irr::IrrlichtDevice *device, irr::video::IVideoDriver *driver);
    ~B2DebugDrawIrr();

	void DrawParticles(const b2Vec2* centers, float32 radius, const b2ParticleColor* colors, int32 count) override;
    void DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
	void Calculate2dFrom3D(irr::core::vector3df& point, const irr::core::matrix4& viewProjectionMatrix, const irr::core::vector2df& screenCenter);
	void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
    void DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color) override;
    void DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color) override;
    void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) override;
    void DrawTransform(const b2Transform& xf) override;
    void DrawPoint(const b2Vec2& p, float32 size, const b2Color& color);
    void DrawString(int x, int y, const char* string, ...); 
    void DrawAABB(b2AABB* aabb, const b2Color& color);

    void setTranslation(float x, float y);
    void setScale(float scale);
    void setScreenCenter(float x, float y);
	void update_camera();

protected:
    irr::IrrlichtDevice *mDevice;
    irr::video::IVideoDriver *mDriver;
    struct B2DDIrrImpl *mImpl;
};


#endif // !B2DEBUGDRAWIRR_H
