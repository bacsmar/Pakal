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

#include <cassert>
#include <cstdio>
#include <cstdarg>
#include <cstring>

#include "B2DebugDrawIrr.h"
#include "irrlicht.h"
#include <vector>

using namespace std;
using namespace irr;


struct B2DDIrrImpl
{
    B2DDIrrImpl(int initArraySizes = 10) : vertices((u32)initArraySizes), indices((u32)initArraySizes), translation(0, 0), scale(1), screenCenter(400.f, 300.f) {}

    core::array<video::S3DVertex> & initVertices() { vertices.clear(); return vertices; }
    core::array<u16> & initIndices() { indices.clear(); return indices; }

    core::array<video::S3DVertex> vertices;
    core::array<u16> indices;

    core::vector2df translation;
    float scale;
    core::vector2df screenCenter;
};


B2DebugDrawIrr::B2DebugDrawIrr(irr::IrrlichtDevice *device, irr::video::IVideoDriver *driver) :
    mDevice(device),
    mDriver(driver)
{
    assert(mDevice);
    assert(mDriver);
    mImpl = new B2DDIrrImpl();	
	auto center = mDriver->getViewPort().getCenter();
	mImpl->screenCenter.X = static_cast<f32>(center.X);
	mImpl->screenCenter.Y = static_cast<f32>(center.Y);
    assert(mImpl);
}

B2DebugDrawIrr::~B2DebugDrawIrr()
{
    delete mImpl;
    mImpl = nullptr;
}

void B2DebugDrawIrr::DrawParticles(const b2Vec2* centers, float32 radius, const b2ParticleColor* colors, int32 count)
{
}

void B2DebugDrawIrr::setTranslation(float x, float y)
{
    mImpl->translation.set(x, y);
}

void B2DebugDrawIrr::setScale(float scale)
{
    mImpl->scale = scale;
}

void B2DebugDrawIrr::setScreenCenter(float x, float y)
{
    mImpl->screenCenter.set(x, y);
}

void B2DebugDrawIrr::update_camera()
{
	if( mDevice)
	{
		auto camera = mDevice->getSceneManager()->getActiveCamera();
		if( camera != nullptr)
		{
			camera->render();
		}		
	}	
}

void B2DebugDrawIrr::DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{

    const core::vector2df &translation = mImpl->translation, &screenCenter = mImpl->screenCenter;
    const float scale = mImpl->scale;
    core::array<video::S3DVertex> &vs = mImpl->initVertices();
    core::array<u16> &is = mImpl->initIndices();
    const video::SColor c((u32)255, (u32)(color.r * 255), (u32)(color.g * 255), (u32)(color.b * 255));
    for (int a = 0; a < vertexCount; ++a)
    {
        video::S3DVertex v((vertices[a].x + translation.X) * scale + screenCenter.X, (vertices[a].y + translation.Y) * scale + screenCenter.Y, 0, 0, 0, 0, c, 0, 0);
        vs.push_back(v);
        is.push_back(a);
    }
    mDriver->draw2DVertexPrimitiveList(vs.pointer(), vertexCount, is.pointer(), vertexCount, video::EVT_STANDARD, scene::EPT_LINE_LOOP, video::EIT_16BIT);
}


float GetScreenScaleFactor(const core::vector3df& worldpnt, scene::ICameraSceneNode* camera)
{
	if (camera->isOrthogonal())
		return 2.0f / camera->getProjectionMatrix()[0];
	core::vector3df viewpnt = worldpnt;
	camera->getViewMatrix().transformVect(viewpnt);
	return  2.0f * viewpnt.Z / camera->getProjectionMatrix()[0];
}

void B2DebugDrawIrr::Calculate2dFrom3D(core::vector3df& point, const core::matrix4& viewProjectionMatrix, const core::vector2df& screenCenter) const
{
	f32 transformedPos[4] = { point.X, point.Y, point.Z, 1.0f };
	viewProjectionMatrix.multiplyWith1x4Matrix(transformedPos);
	const f32 zDiv = transformedPos[3] == 0.0f ? 1.0f : core::reciprocal(transformedPos[3]);


	point.X = screenCenter.X + core::round32(screenCenter.X * (transformedPos[0] * zDiv));
	point.Y = screenCenter.Y - core::round32(screenCenter.Y * (transformedPos[1] * zDiv));
}

void B2DebugDrawIrr::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{
	auto camera = mDevice->getSceneManager()->getActiveCamera();		
	auto viewProjectionMatrix = camera->getProjectionMatrix() * camera->getViewMatrix();    

    const video::SColor c1((u32)128, (u32)(color.r * 0.5f * 255), (u32)(color.g * 0.5f * 255), (u32)(color.b * 0.5f * 255));
    const video::SColor c2((u32)255, (u32)(color.r * 255), (u32)(color.g * 255), (u32)(color.b * 255));

    core::array<video::S3DVertex> &vs = mImpl->initVertices();
    core::array<u16> &is = mImpl->initIndices();

    for (int a = 0; a < vertexCount; ++a)
    {        
        video::S3DVertex v( (vertices[a].x ) , (vertices[a].y ) , 0, 0, 0, 0, c1, 0, 0);
		Calculate2dFrom3D(v.Pos, viewProjectionMatrix, mImpl->screenCenter);
		 
        vs.push_back(v);
        is.push_back(vertexCount-a-1);
    }

	//mDriver->setTransform(video::ETS_PROJECTION, camera->getProjectionMatrix());
	//mDriver->setTransform(video::ETS_VIEW, camera->getViewMatrix());
	//mDriver->setTransform(video::ETS_WORLD, viewProjectionMatrix);
	//mDriver->setTransform(video::ETS_PROJECTION, viewProjectionMatrix);

    video::SMaterial mat;
    mat.MaterialType = video::EMT_TRANSPARENT_VERTEX_ALPHA;
    mat.Lighting = false;
	//mat.BackfaceCulling = false;
    mDriver->setMaterial(mat);
    mDriver->draw2DVertexPrimitiveList(vs.pointer(), vertexCount, is.pointer(), vertexCount - 2, video::EVT_STANDARD, scene::EPT_TRIANGLE_FAN, video::EIT_16BIT);
    for (int a = 0; a < vertexCount; ++a)
        vs[a].Color = c2;
    mat.MaterialType = video::EMT_SOLID;
    mDriver->setMaterial(mat);
    mDriver->draw2DVertexPrimitiveList(vs.pointer(), vertexCount, is.pointer(), vertexCount, video::EVT_STANDARD, scene::EPT_LINE_LOOP, video::EIT_16BIT);
}

void B2DebugDrawIrr::DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color)
{
    const core::vector2df &translation = mImpl->translation, &screenCenter = mImpl->screenCenter;
    const float scale = mImpl->scale * 5;
    core::array<video::S3DVertex> &vs = mImpl->initVertices();
    core::array<u16> &is = mImpl->initIndices();
    const video::SColor c((u32)255, (u32)(color.r * 255), (u32)(color.g * 255), (u32)(color.b * 255));
    const int vertexCount = 16;
    const float32 k_increment = 2.0f * b2_pi / vertexCount;
    float32 theta = 0.0f;
    for (int a = 0; a < vertexCount; ++a)
    {
        const b2Vec2 vp = center + radius * b2Vec2(cosf(theta), sinf(theta)) + b2Vec2(translation.X, translation.Y);
        //video::S3DVertex v(vp.x * scale + screenCenter.X, vp.y * scale + screenCenter.Y, 0, 0, 0, 0, c, 0, 0);
        video::S3DVertex v(vp.x * scale + screenCenter.X, vp.y * scale + screenCenter.Y, 0, 0, 0, 0, c, 0, 0);
        vs.push_back(v);
        is.push_back(a);
        theta += k_increment;
    }
    mDriver->draw2DVertexPrimitiveList(vs.pointer(), vertexCount, is.pointer(), vertexCount, video::EVT_STANDARD, scene::EPT_LINE_LOOP, video::EIT_16BIT);
}

void B2DebugDrawIrr::DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color)
{

	auto camera = mDevice->getSceneManager()->getActiveCamera();
	auto viewProjectionMatrix = camera->getProjectionMatrix() * camera->getViewMatrix();

    const core::vector2df &translation = mImpl->translation;
    const video::SColor c1((u32)128, (u32)(color.r * 0.5f * 255), (u32)(color.g * 0.5f * 255), (u32)(color.b * 0.5f * 255));
    const video::SColor c2((u32)255, (u32)(color.r * 255), (u32)(color.g * 255), (u32)(color.b * 255));
    core::array<video::S3DVertex> &vs = mImpl->initVertices();
    core::array<u16> &is = mImpl->initIndices();
    const int vertexCount = 16;
    const float32 k_increment = 2.0f * b2_pi / vertexCount;
    float32 theta = 0.0f;
    for (int a = 0; a < vertexCount; ++a)
    {
        b2Vec2 vp = center + radius * b2Vec2(cosf(theta), sinf(theta)) + b2Vec2(translation.X, translation.Y);
        //video::S3DVertex v(vp.x * scale + screenCenter.X, -vp.y * scale + screenCenter.Y, 0, 0, 0, 0, c1, 0, 0);
        video::S3DVertex v(vp.x , vp.y , 0, 0, 0, 0, c1, 0, 0);
		Calculate2dFrom3D(v.Pos, viewProjectionMatrix, mImpl->screenCenter);
        vs.push_back(v);
        is.push_back(vertexCount-a-1);
        theta += k_increment;
    }

    video::SMaterial mat;
    mat.MaterialType = video::EMT_TRANSPARENT_VERTEX_ALPHA;
    mat.Lighting = false;
	//mat.BackfaceCulling = false;
    mDriver->setMaterial(mat);
    mDriver->draw2DVertexPrimitiveList(vs.pointer(), vertexCount, is.pointer(), vertexCount - 2, video::EVT_STANDARD, scene::EPT_TRIANGLE_FAN, video::EIT_16BIT);
    for (int a = 0; a < vertexCount; ++a)
        vs[a].Color = c2;
    mat.MaterialType = video::EMT_SOLID;
    mDriver->setMaterial(mat);
    mDriver->draw2DVertexPrimitiveList(vs.pointer(), vertexCount, is.pointer(), vertexCount, video::EVT_STANDARD, scene::EPT_LINE_LOOP, video::EIT_16BIT);
}

void B2DebugDrawIrr::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color)
{
    const core::vector2df &translation = mImpl->translation, &screenCenter = mImpl->screenCenter;
    const float scale = mImpl->scale;
    const video::SColor c((u32)255, (u32)(color.r * 255), (u32)(color.g * 255), (u32)(color.b * 255));
    video::S3DVertex vs[2];
    u16 is[2] = { 0, 1 };
    vs[0].Pos.set((p1.x + translation.X) * scale + screenCenter.X, (p1.y + translation.Y) * scale + screenCenter.Y, 0);
    vs[1].Pos.set((p2.x + translation.X) * scale + screenCenter.X, (p2.y + translation.Y) * scale + screenCenter.Y, 0);
    vs[0].Color = c;
    vs[1].Color = c;
    mDriver->draw2DVertexPrimitiveList(vs, 2, is, 1, video::EVT_STANDARD, scene::EPT_LINES, video::EIT_16BIT);
}

void B2DebugDrawIrr::DrawTransform(const b2Transform& xf)
{
    const core::vector2df &translation = mImpl->translation, &screenCenter = mImpl->screenCenter;
    const float scale = mImpl->scale;
    b2Vec2 p1 = xf.p, p2;
    const float32 k_axisScale = 0.4f;
    const video::SColor c1(0xffff0000);
    const video::SColor c2(0xff00ff00);
    video::S3DVertex vs[4];
    u16 is[4] = { 0, 1, 2, 3 };
    vs[0].Pos.set((p1.x + translation.X) * scale + screenCenter.X, (p1.y + translation.Y) * scale + screenCenter.Y, 0);
    //p2 = p1 + k_axisScale * xf.R.col1;
	p2 = p1 + k_axisScale * xf.q.GetXAxis();
    vs[1].Pos.set((p2.x + translation.X) * scale + screenCenter.X, (p2.y + translation.Y) * scale + screenCenter.Y, 0);
    vs[2].Pos.set((p1.x + translation.X) * scale + screenCenter.X, (p1.y + translation.Y) * scale + screenCenter.Y, 0);
    //p2 = p1 + k_axisScale * xf.R.col2;
	p2 = p1 + k_axisScale * xf.q.GetYAxis();
    vs[3].Pos.set((p2.x + translation.X) * scale + screenCenter.X, (p2.y + translation.Y) * scale + screenCenter.Y, 0);
    vs[0].Color = c1;
    vs[1].Color = c1;
    vs[2].Color = c2;
    vs[3].Color = c2;
    mDriver->draw2DVertexPrimitiveList(vs, 4, is, 2, video::EVT_STANDARD, scene::EPT_LINES, video::EIT_16BIT);
}

void B2DebugDrawIrr::DrawPoint(const b2Vec2& p, float32 size, const b2Color& color)
{
    const core::vector2df &translation = mImpl->translation, &screenCenter = mImpl->screenCenter;
    const float scale = mImpl->scale;
    const video::SColor c((u32)255, (u32)(color.r * 255), (u32)(color.g * 255), (u32)(color.b * 255));
    video::S3DVertex v((p.x + translation.X) * scale + screenCenter.X, (p.y + translation.Y) * scale + screenCenter.Y, 0, 0, 0, 0, c, 0, 0);
    mDriver->draw2DVertexPrimitiveList(&v, 1, 0, 1, video::EVT_STANDARD, scene::EPT_POINTS, video::EIT_16BIT);
}

void B2DebugDrawIrr::DrawString(int x, int y, const char *string, ...)
{
    const core::vector2df &translation = mImpl->translation, &screenCenter = mImpl->screenCenter;
    const float scale = mImpl->scale;
    gui::IGUIEnvironment *guiEnv = mDevice->getGUIEnvironment();
    if (!guiEnv)
        return;
    gui::IGUIFont *font = guiEnv->getBuiltInFont();
    if (!font)
        return;
    char buffer[128];
    va_list arg;
    va_start(arg, string);
#ifdef _MSC_VER
	_vsnprintf_s(buffer,128,string, arg);
#else
	vsnprintf(buffer, 128, string, arg);
#endif    
    va_end(arg);
    core::stringw str(buffer);
    font->draw(str, core::recti((int)((x + translation.X) * scale + screenCenter.X), (int)((y + translation.Y) * scale + screenCenter.Y), 1000, 1000), video::SColor(255, 230, 154, 154), false, false, 0);
}

void B2DebugDrawIrr::DrawAABB(b2AABB* aabb, const b2Color& color)
{
    const core::vector2df &translation = mImpl->translation, &screenCenter = mImpl->screenCenter;
    const float scale = mImpl->scale;
    const video::SColor c((u32)255, (u32)(color.r * 255), (u32)(color.g * 255), (u32)(color.b * 255));
    video::S3DVertex vs[4];
    u16 is[4] = { 0, 1, 2, 3 };
    vs[0].Pos.set((aabb->lowerBound.x + translation.X) * scale + screenCenter.X, (aabb->lowerBound.y + translation.Y) * scale + screenCenter.Y, 0);
    vs[1].Pos.set((aabb->upperBound.x + translation.X) * scale + screenCenter.X, (aabb->lowerBound.y + translation.Y) * scale + screenCenter.Y, 0);
    vs[2].Pos.set((aabb->upperBound.x + translation.X) * scale + screenCenter.X, (aabb->upperBound.y + translation.Y) * scale + screenCenter.Y, 0);
    vs[3].Pos.set((aabb->lowerBound.x + translation.X) * scale + screenCenter.X, (aabb->upperBound.y + translation.Y) * scale + screenCenter.Y, 0);
    vs[0].Color = c;
    vs[1].Color = c;
    vs[2].Color = c;
    vs[3].Color = c;
    mDriver->draw2DVertexPrimitiveList(vs, 4, is, 4, video::EVT_STANDARD, scene::EPT_LINE_LOOP, video::EIT_16BIT);
}
