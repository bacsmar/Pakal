///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SpriteComponent_Bgfx.cpp
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: January 30, 2026
//
// Purpose: 2D Sprite rendering component using bgfx - Implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "SpriteComponent_Bgfx.h"
#include "LogMgr.h"
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <cmath>

namespace Pakal
{
	// Static vertex layout initialization
	bgfx::VertexLayout SpriteComponent_Bgfx::SpriteVertex::ms_layout;
	
	void SpriteComponent_Bgfx::SpriteVertex::init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 2, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.end();
	}
	
	SpriteComponent_Bgfx::SpriteComponent_Bgfx() :
		m_texture(BGFX_INVALID_HANDLE),
		m_textureLoaded(false),
		m_textureWidth(0),
		m_textureHeight(0),
		m_vertexBuffer(BGFX_INVALID_HANDLE),
		m_indexBuffer(BGFX_INVALID_HANDLE),
		m_buffersCreated(false),
		m_program(BGFX_INVALID_HANDLE),
		m_programLoaded(false),
		m_position(0.0f, 0.0f),
		m_rotation(0.0f),
		m_scale(1.0f, 1.0f),
		m_origin(0.5f, 0.5f),
		m_layer(0),
		m_color(0xFFFFFFFF), // White in ABGR
		m_flipX(false),
		m_flipY(false),
		m_visible(true),
		m_currentFrame(0),
		m_frameWidth(0),
		m_frameHeight(0),
		m_framesPerRow(1),
		m_animStart(0),
		m_animEnd(0),
		m_animLoop(false),
		m_animPlaying(false),
		m_animFps(10.0f),
		m_animTime(0.0f),
		m_vertexBufferDirty(true)
	{
		// Initialize texture region to full texture
		m_texRegion[0] = 0;
		m_texRegion[1] = 0;
		m_texRegion[2] = 0;
		m_texRegion[3] = 0;
		
		// Initialize vertex layout (static, only once)
		static bool layoutInitialized = false;
		if (!layoutInitialized)
		{
			SpriteVertex::init();
			layoutInitialized = true;
		}
		
		// Create index buffer (6 indices for 2 triangles)
		// This is shared by all sprites and doesn't change
		uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };
		m_indexBuffer = bgfx::createIndexBuffer(
			bgfx::makeRef(indices, sizeof(indices)),
			BGFX_BUFFER_INDEX16
		);
		m_buffersCreated = true;
	}
	
	SpriteComponent_Bgfx::~SpriteComponent_Bgfx()
	{
		if (bgfx::isValid(m_texture))
		{
			bgfx::destroy(m_texture);
		}
		
		if (bgfx::isValid(m_vertexBuffer))
		{
			bgfx::destroy(m_vertexBuffer);
		}
		
		if (bgfx::isValid(m_indexBuffer))
		{
			bgfx::destroy(m_indexBuffer);
		}
		
		if (bgfx::isValid(m_program))
		{
			bgfx::destroy(m_program);
		}
	}
	
	void SpriteComponent_Bgfx::set_texture(const std::string& texturePath)
	{
		m_texturePath = texturePath;
		load_texture(texturePath);
	}
	
	void SpriteComponent_Bgfx::load_texture(const std::string& path)
	{
		// For now, we'll use a placeholder white texture
		// In a full implementation, this would load from file using bimg
		LOG_WARNING("[SpriteComponent_Bgfx] Texture loading not yet implemented, using white placeholder");
		
		// Create a 1x1 white texture as placeholder
		const bgfx::Memory* mem = bgfx::alloc(4); // RGBA
		mem->data[0] = mem->data[1] = mem->data[2] = mem->data[3] = 0xFF;
		
		if (bgfx::isValid(m_texture))
		{
			bgfx::destroy(m_texture);
		}
		
		m_texture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, 
			BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT, mem);
		m_textureLoaded = true;
		m_textureWidth = 1;
		m_textureHeight = 1;
		
		update_texture_coords();
	}
	
	void SpriteComponent_Bgfx::create_solid_color(uint32_t color, int width, int height)
	{
		// Create 1x1 white texture
		const bgfx::Memory* mem = bgfx::alloc(4); // RGBA
		mem->data[0] = mem->data[1] = mem->data[2] = mem->data[3] = 0xFF;
		
		if (bgfx::isValid(m_texture))
		{
			bgfx::destroy(m_texture);
		}
		
		m_texture = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8,
			BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT, mem);
		m_textureLoaded = true;
		m_textureWidth = 1;
		m_textureHeight = 1;
		
		// Use color and scale to create colored rectangle
		set_color(color);
		set_scale((float)width, (float)height);
		
		update_texture_coords();
	}
	
	void SpriteComponent_Bgfx::set_texture_region(int x, int y, int width, int height)
	{
		m_texRegion[0] = x;
		m_texRegion[1] = y;
		m_texRegion[2] = width;
		m_texRegion[3] = height;
		update_texture_coords();
	}
	
	void SpriteComponent_Bgfx::set_position(const tmath::vector2df& pos)
	{
		m_position = pos;
		m_vertexBufferDirty = true;
	}
	
	void SpriteComponent_Bgfx::set_position(float x, float y)
	{
		set_position(tmath::vector2df(x, y));
	}
	
	void SpriteComponent_Bgfx::set_rotation(float angle)
	{
		m_rotation = angle;
		m_vertexBufferDirty = true;
	}
	
	void SpriteComponent_Bgfx::set_scale(const tmath::vector2df& scale)
	{
		m_scale = scale;
		m_vertexBufferDirty = true;
	}
	
	void SpriteComponent_Bgfx::set_scale(float x, float y)
	{
		set_scale(tmath::vector2df(x, y));
	}
	
	void SpriteComponent_Bgfx::set_origin(float x, float y)
	{
		m_origin = tmath::vector2df(x, y);
		m_vertexBufferDirty = true;
	}
	
	void SpriteComponent_Bgfx::set_layer(int layer)
	{
		m_layer = layer;
	}
	
	void SpriteComponent_Bgfx::set_color(uint32_t rgba)
	{
		// Convert RGBA to ABGR for bgfx
		uint8_t r = (rgba >> 24) & 0xFF;
		uint8_t g = (rgba >> 16) & 0xFF;
		uint8_t b = (rgba >> 8) & 0xFF;
		uint8_t a = rgba & 0xFF;
		m_color = (a << 24) | (b << 16) | (g << 8) | r;
		m_vertexBufferDirty = true;
	}
	
	void SpriteComponent_Bgfx::set_color(float r, float g, float b, float a)
	{
		uint8_t ri = (uint8_t)(r * 255.0f);
		uint8_t gi = (uint8_t)(g * 255.0f);
		uint8_t bi = (uint8_t)(b * 255.0f);
		uint8_t ai = (uint8_t)(a * 255.0f);
		m_color = (ai << 24) | (bi << 16) | (gi << 8) | ri;
		m_vertexBufferDirty = true;
	}
	
	void SpriteComponent_Bgfx::set_flip(bool flipX, bool flipY)
	{
		m_flipX = flipX;
		m_flipY = flipY;
		update_texture_coords();
	}
	
	void SpriteComponent_Bgfx::set_frame(int frame)
	{
		m_currentFrame = frame;
		update_texture_coords();
	}
	
	void SpriteComponent_Bgfx::set_frame_size(int width, int height)
	{
		m_frameWidth = width;
		m_frameHeight = height;
		if (m_textureWidth > 0 && width > 0)
		{
			m_framesPerRow = m_textureWidth / width;
		}
		update_texture_coords();
	}
	
	void SpriteComponent_Bgfx::play_animation(int startFrame, int endFrame, bool loop)
	{
		m_animStart = startFrame;
		m_animEnd = endFrame;
		m_animLoop = loop;
		m_animPlaying = true;
		m_currentFrame = startFrame;
		m_animTime = 0.0f;
		update_texture_coords();
	}
	
	void SpriteComponent_Bgfx::stop_animation()
	{
		m_animPlaying = false;
	}
	
	void SpriteComponent_Bgfx::update(float deltaTime)
	{
		if (m_animPlaying)
		{
			update_animation(deltaTime);
		}
	}
	
	void SpriteComponent_Bgfx::update_animation(float deltaTime)
	{
		m_animTime += deltaTime;
		float frameTime = 1.0f / m_animFps;
		
		if (m_animTime >= frameTime)
		{
			m_animTime -= frameTime;
			m_currentFrame++;
			
			if (m_currentFrame > m_animEnd)
			{
				if (m_animLoop)
				{
					m_currentFrame = m_animStart;
				}
				else
				{
					m_currentFrame = m_animEnd;
					m_animPlaying = false;
				}
			}
			
			update_texture_coords();
		}
	}
	
	void SpriteComponent_Bgfx::update_texture_coords()
	{
		m_vertexBufferDirty = true;
	}
	
	void SpriteComponent_Bgfx::update_vertex_buffer()
	{
		if (!m_vertexBufferDirty)
			return;
		
		// Calculate texture coordinates
		float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
		
		if (m_frameWidth > 0 && m_frameHeight > 0 && m_textureWidth > 0 && m_textureHeight > 0)
		{
			// Sprite sheet animation
			int frameX = (m_currentFrame % m_framesPerRow) * m_frameWidth;
			int frameY = (m_currentFrame / m_framesPerRow) * m_frameHeight;
			
			u0 = (float)frameX / (float)m_textureWidth;
			v0 = (float)frameY / (float)m_textureHeight;
			u1 = (float)(frameX + m_frameWidth) / (float)m_textureWidth;
			v1 = (float)(frameY + m_frameHeight) / (float)m_textureHeight;
		}
		else if (m_texRegion[2] > 0 && m_texRegion[3] > 0 && m_textureWidth > 0 && m_textureHeight > 0)
		{
			// Texture region
			u0 = (float)m_texRegion[0] / (float)m_textureWidth;
			v0 = (float)m_texRegion[1] / (float)m_textureHeight;
			u1 = (float)(m_texRegion[0] + m_texRegion[2]) / (float)m_textureWidth;
			v1 = (float)(m_texRegion[1] + m_texRegion[3]) / (float)m_textureHeight;
		}
		
		// Apply flip
		if (m_flipX)
		{
			float temp = u0;
			u0 = u1;
			u1 = temp;
		}
		if (m_flipY)
		{
			float temp = v0;
			v0 = v1;
			v1 = temp;
		}
		
		// Create quad vertices
		// Vertices are in local space, will be transformed by model matrix
		float halfWidth = 0.5f;
		float halfHeight = 0.5f;
		
		// Origin-adjusted positions
		float left = -m_origin.x;
		float right = left + 1.0f;
		float top = -m_origin.y;
		float bottom = top + 1.0f;
		
		SpriteVertex vertices[4];
		
		// Bottom-left
		vertices[0].x = left;
		vertices[0].y = bottom;
		vertices[0].z = 0.0f;
		vertices[0].u = u0;
		vertices[0].v = v1;
		vertices[0].abgr = m_color;
		
		// Bottom-right
		vertices[1].x = right;
		vertices[1].y = bottom;
		vertices[1].z = 0.0f;
		vertices[1].u = u1;
		vertices[1].v = v1;
		vertices[1].abgr = m_color;
		
		// Top-right
		vertices[2].x = right;
		vertices[2].y = top;
		vertices[2].z = 0.0f;
		vertices[2].u = u1;
		vertices[2].v = v0;
		vertices[2].abgr = m_color;
		
		// Top-left
		vertices[3].x = left;
		vertices[3].y = top;
		vertices[3].z = 0.0f;
		vertices[3].u = u0;
		vertices[3].v = v0;
		vertices[3].abgr = m_color;
		
		// Destroy old vertex buffer if it exists
		if (bgfx::isValid(m_vertexBuffer))
		{
			bgfx::destroy(m_vertexBuffer);
		}
		
		// Create new vertex buffer
		m_vertexBuffer = bgfx::createVertexBuffer(
			bgfx::makeRef(vertices, sizeof(vertices)),
			SpriteVertex::ms_layout
		);
		
		m_vertexBufferDirty = false;
	}
	
	void SpriteComponent_Bgfx::render(bgfx::ViewId viewId, const float* viewProj)
	{
		if (!m_visible || !m_textureLoaded)
			return;
		
		// Update vertex buffer if needed
		if (m_vertexBufferDirty)
		{
			update_vertex_buffer();
		}
		
		// Create model matrix (transform)
		float model[16];
		bx::mtxIdentity(model);
		
		// Apply transformations: scale, rotation, translation
		float transform[16];
		bx::mtxSRT(transform,
			m_scale.x, m_scale.y, 1.0f,     // scale
			0.0f, 0.0f, m_rotation,           // rotation
			m_position.x, m_position.y, (float)m_layer * 0.01f  // translation
		);
		
		bx::mtxCopy(model, transform);
		
		// Set model matrix
		bgfx::setTransform(model);
		
		// Set vertex and index buffers
		bgfx::setVertexBuffer(0, m_vertexBuffer);
		bgfx::setIndexBuffer(m_indexBuffer);
		
		// Set texture
		bgfx::setTexture(0, bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler), m_texture);
		
		// Set render state (alpha blending)
		uint64_t state = BGFX_STATE_WRITE_RGB
			| BGFX_STATE_WRITE_A
			| BGFX_STATE_BLEND_ALPHA;
		bgfx::setState(state);
		
		// Submit draw call (without program for now - will need to be set up later)
		// bgfx::submit(viewId, m_program);
		
		// For now, we'll just log a warning that shaders need to be set up
		// This will be completed when we integrate with the graphics system
	}
}
