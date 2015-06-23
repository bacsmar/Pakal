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
    assert(mImpl);
}

B2DebugDrawIrr::~B2DebugDrawIrr()
{
    delete mImpl;
    mImpl = 0;
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


void B2DebugDrawIrr::DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{
    /*
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_LINE_LOOP);
    for (int32 i = 0; i < vertexCount; ++i)
    {
        glVertex2f(vertices[i].x, vertices[i].y);
    }
    glEnd();
    */
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

void B2DebugDrawIrr::DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color)
{
    /*
    glEnable(GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.5f * color.r, 0.5f * color.g, 0.5f * color.b, 0.5f);
    glBegin(GL_TRIANGLE_FAN);
    for (int32 i = 0; i < vertexCount; ++i)
    {
        glVertex2f(vertices[i].x, vertices[i].y);
    }
    glEnd();
    glDisable(GL_BLEND);

    glColor4f(color.r, color.g, color.b, 1.0f);
    glBegin(GL_LINE_LOOP);
    for (int32 i = 0; i < vertexCount; ++i)
    {
        glVertex2f(vertices[i].x, vertices[i].y);
    }
    glEnd();
    */
    const core::vector2df &translation = mImpl->translation, &screenCenter = mImpl->screenCenter;
    const float scale = mImpl->scale;
    const video::SColor c1((u32)128, (u32)(color.r * 0.5f * 255), (u32)(color.g * 0.5f * 255), (u32)(color.b * 0.5f * 255));
    const video::SColor c2((u32)255, (u32)(color.r * 255), (u32)(color.g * 255), (u32)(color.b * 255));
    core::array<video::S3DVertex> &vs = mImpl->initVertices();
    core::array<u16> &is = mImpl->initIndices();
    for (int a = 0; a < vertexCount; ++a)
    {
        video::S3DVertex v((vertices[a].x + translation.X) * scale + screenCenter.X, (vertices[a].y + translation.Y) * scale + screenCenter.Y, 0, 0, 0, 0, c1, 0, 0);
        vs.push_back(v);
        is.push_back(a);
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

void B2DebugDrawIrr::DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color)
{
    /*
    const float32 k_segments = 16.0f;
    const float32 k_increment = 2.0f * b2_pi / k_segments;
    float32 theta = 0.0f;
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_LINE_LOOP);
    for (int32 i = 0; i < k_segments; ++i)
    {
        b2Vec2 v = center + radius * b2Vec2(cosf(theta), sinf(theta));
        glVertex2f(v.x, v.y);
        theta += k_increment;
    }
    glEnd();
    */
    const core::vector2df &translation = mImpl->translation, &screenCenter = mImpl->screenCenter;
    const float scale = mImpl->scale;
    core::array<video::S3DVertex> &vs = mImpl->initVertices();
    core::array<u16> &is = mImpl->initIndices();
    const video::SColor c((u32)255, (u32)(color.r * 255), (u32)(color.g * 255), (u32)(color.b * 255));
    const int vertexCount = 16;
    const float32 k_increment = 2.0f * b2_pi / vertexCount;
    float32 theta = 0.0f;
    for (int a = 0; a < vertexCount; ++a)
    {
        const b2Vec2 vp = center + radius * b2Vec2(cosf(theta), sinf(theta)) + b2Vec2(translation.X, translation.Y);
        video::S3DVertex v(vp.x * scale + screenCenter.X, vp.y * scale + screenCenter.Y, 0, 0, 0, 0, c, 0, 0);
        vs.push_back(v);
        is.push_back(a);
        theta += k_increment;
    }
    mDriver->draw2DVertexPrimitiveList(vs.pointer(), vertexCount, is.pointer(), vertexCount, video::EVT_STANDARD, scene::EPT_LINE_LOOP, video::EIT_16BIT);
}

void B2DebugDrawIrr::DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color)
{
    /*
    const float32 k_segments = 16.0f;
    const float32 k_increment = 2.0f * b2_pi / k_segments;
    float32 theta = 0.0f;
    glEnable(GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glColor4f(0.5f * color.r, 0.5f * color.g, 0.5f * color.b, 0.5f);
    glBegin(GL_TRIANGLE_FAN);
    for (int32 i = 0; i < k_segments; ++i)
    {
        b2Vec2 v = center + radius * b2Vec2(cosf(theta), sinf(theta));
        glVertex2f(v.x, v.y);
        theta += k_increment;
    }
    glEnd();
    glDisable(GL_BLEND);

    theta = 0.0f;
    glColor4f(color.r, color.g, color.b, 1.0f);
    glBegin(GL_LINE_LOOP);
    for (int32 i = 0; i < k_segments; ++i)
    {
        b2Vec2 v = center + radius * b2Vec2(cosf(theta), sinf(theta));
        glVertex2f(v.x, v.y);
        theta += k_increment;
    }
    glEnd();

    b2Vec2 p = center + radius * axis;
    glBegin(GL_LINES);
    glVertex2f(center.x, center.y);
    glVertex2f(p.x, p.y);
    glEnd();
    */
    const core::vector2df &translation = mImpl->translation, &screenCenter = mImpl->screenCenter;
    const float scale = mImpl->scale;
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
        video::S3DVertex v(vp.x * scale + screenCenter.X, vp.y * scale + screenCenter.Y, 0, 0, 0, 0, c1, 0, 0);
        vs.push_back(v);
        is.push_back(a);
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
    /*
    glColor3f(color.r, color.g, color.b);
    glBegin(GL_LINES);
    glVertex2f(p1.x, p1.y);
    glVertex2f(p2.x, p2.y);
    glEnd();
    */
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
    /*
    b2Vec2 p1 = xf.position, p2;
    const float32 k_axisScale = 0.4f;
    glBegin(GL_LINES);
    
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex2f(p1.x, p1.y);
    p2 = p1 + k_axisScale * xf.R.col1;
    glVertex2f(p2.x, p2.y);

    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex2f(p1.x, p1.y);
    p2 = p1 + k_axisScale * xf.R.col2;
    glVertex2f(p2.x, p2.y);

    glEnd();
    */
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
    /*
    glPointSize(size);
    glBegin(GL_POINTS);
    glColor3f(color.r, color.g, color.b);
    glVertex2f(p.x, p.y);
    glEnd();
    glPointSize(1.0f);
    */
    const core::vector2df &translation = mImpl->translation, &screenCenter = mImpl->screenCenter;
    const float scale = mImpl->scale;
    const video::SColor c((u32)255, (u32)(color.r * 255), (u32)(color.g * 255), (u32)(color.b * 255));
    video::S3DVertex v((p.x + translation.X) * scale + screenCenter.X, (p.y + translation.Y) * scale + screenCenter.Y, 0, 0, 0, 0, c, 0, 0);
    mDriver->draw2DVertexPrimitiveList(&v, 1, 0, 1, video::EVT_STANDARD, scene::EPT_POINTS, video::EIT_16BIT);
}

void B2DebugDrawIrr::DrawString(int x, int y, const char *string, ...)
{
    /*
    char buffer[128];

    va_list arg;
    va_start(arg, string);
    vsprintf(buffer, string, arg);
    va_end(arg);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    int w = glutGet(GLUT_WINDOW_WIDTH);
    int h = glutGet(GLUT_WINDOW_HEIGHT);
    gluOrtho2D(0, w, h, 0);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    glColor3f(0.9f, 0.6f, 0.6f);
    glRasterPos2i(x, y);
    int32 length = (int32)strlen(buffer);
    for (int32 i = 0; i < length; ++i)
    {
        glutBitmapCharacter(GLUT_BITMAP_8_BY_13, buffer[i]);
    }

    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    */
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
    _vsnprintf(buffer, 128, string, arg);
    va_end(arg);
    core::stringw str(buffer);
    font->draw(str, core::recti((int)((x + translation.X) * scale + screenCenter.X), (int)((y + translation.Y) * scale + screenCenter.Y), 1000, 1000), video::SColor(255, 230, 154, 154), false, false, 0);
}

void B2DebugDrawIrr::DrawAABB(b2AABB* aabb, const b2Color& color)
{
    /*
    glColor3f(c.r, c.g, c.b);
    glBegin(GL_LINE_LOOP);
    glVertex2f(aabb->lowerBound.x, aabb->lowerBound.y);
    glVertex2f(aabb->upperBound.x, aabb->lowerBound.y);
    glVertex2f(aabb->upperBound.x, aabb->upperBound.y);
    glVertex2f(aabb->lowerBound.x, aabb->upperBound.y);
    glEnd();
    */
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
