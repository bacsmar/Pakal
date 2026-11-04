///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SpriteComponent_Bgfx.cpp
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: January 30, 2026
//
// Purpose: 2D Sprite rendering component using bgfx - Implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include "SpriteComponent_Bgfx.h"
#include "BgfxGraphicsSystem.h"
#include "LogMgr.h"
#include "ResourceManager.h"
#include <bgfx/bgfx.h>
#include <bx/math.h>
#include <bx/allocator.h>
#include <bimg/decode.h>
#include <cmath>
#include <fstream>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Pakal
{
	namespace
	{
		bgfx::UniformHandle s_texColorUniform = BGFX_INVALID_HANDLE;
		uint32_t s_spriteInstanceCount = 0;

		// Textures shared by the sprites: loading a file reads and decodes it and makes a GPU texture, so a file shown by many
		// sprites (every enemy of a kind, every particle) is loaded once. Keyed by the resolved path; destroyed with its last sprite.
		struct SharedTexture
		{
			bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
			int width = 0, height = 0;
			int users = 0;
		};
		std::mutex s_textureMutex;
		std::unordered_map<std::string, SharedTexture> s_textures;
		SharedTexture s_white;   // the 1x1 white texture of the solid colors and the placeholders
	}

	void SpriteComponent_Bgfx::release_texture()
	{
		std::lock_guard<std::mutex> lock(s_textureMutex);
		switch (m_textureOwner)
		{
		case TextureOwner::Cached:
		{
			auto found = s_textures.find(m_textureKey);
			if (found != s_textures.end() && --found->second.users <= 0)
			{
				if (bgfx::isValid(found->second.handle)) bgfx::destroy(found->second.handle);
				s_textures.erase(found);
			}
			break;
		}
		case TextureOwner::White:
			if (--s_white.users <= 0)
			{
				if (bgfx::isValid(s_white.handle)) bgfx::destroy(s_white.handle);
				s_white = SharedTexture();
			}
			break;
		case TextureOwner::None:
			break;
		}
		m_textureOwner = TextureOwner::None;
		m_textureKey.clear();
		m_texture = BGFX_INVALID_HANDLE;
	}

	void SpriteComponent_Bgfx::acquire_white_texture()
	{
		release_texture();
		std::lock_guard<std::mutex> lock(s_textureMutex);
		if (!bgfx::isValid(s_white.handle))
		{
			const bgfx::Memory* mem = bgfx::alloc(4);
			mem->data[0] = mem->data[1] = mem->data[2] = mem->data[3] = 0xFF;
			s_white.handle = bgfx::createTexture2D(1, 1, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT, mem);
		}
		++s_white.users;
		m_textureOwner = TextureOwner::White;
		m_texture = s_white.handle;
	}


	// Static vertex layout initialization
	bgfx::VertexLayout SpriteComponent_Bgfx::SpriteVertex::ms_layout;
	
	void SpriteComponent_Bgfx::SpriteVertex::init()
	{
		ms_layout
			.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
			.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
			.end();
	}
	
	SpriteComponent_Bgfx::SpriteComponent_Bgfx(BgfxGraphicsSystem* graphicsSystem) :
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
		m_vertexBufferDirty(true),
		m_graphics_system(graphicsSystem)
	{
		++s_spriteInstanceCount;
		if (s_spriteInstanceCount == 1)
		{
			if (!bgfx::isValid(s_texColorUniform))
			{
				s_texColorUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
			}
		}
		
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
		// Use bgfx::copy() to ensure bgfx owns the memory (makeRef only stores a pointer to
		// the local array, which would be destroyed when the constructor returns).
		static const uint16_t indices[] = { 0, 1, 2, 2, 3, 0 };
		m_indexBuffer = bgfx::createIndexBuffer(
			bgfx::copy(indices, sizeof(indices))
		);
		m_buffersCreated = true;

		// Register with graphics system
		if (m_graphics_system)
		{
			m_graphics_system->register_sprite(this);
			LOG_INFO("[SpriteComponent_Bgfx] Sprite created and registered");
		}
		else
		{
			LOG_WARNING("[SpriteComponent_Bgfx] Sprite created but no graphics system!");
		}
	}
	
	SpriteComponent_Bgfx::~SpriteComponent_Bgfx()
	{
		auto releaseResources = [this]()
		{
			if (m_graphics_system)
			{
				m_graphics_system->unregister_sprite(this);
			}

			if (s_spriteInstanceCount > 0)
			{
				--s_spriteInstanceCount;
				if (s_spriteInstanceCount == 0)
				{
					if (bgfx::isValid(s_texColorUniform))
					{
						bgfx::destroy(s_texColorUniform);
						s_texColorUniform = BGFX_INVALID_HANDLE;
					}
				}
			}

			release_texture();

			if (bgfx::isValid(m_vertexBuffer))
			{
				bgfx::destroy(m_vertexBuffer);
			}

			if (bgfx::isValid(m_indexBuffer))
			{
				bgfx::destroy(m_indexBuffer);
			}

			m_program = BGFX_INVALID_HANDLE;
			m_programLoaded = false;
		};

		if (m_graphics_system)
		{
			auto state = m_graphics_system->get_state();
			if (state == SystemState::Running || state == SystemState::Paused)
			{
				auto task = m_graphics_system->execute_block(releaseResources);
				if (task)
				{
					task->wait();
				}
				return;
			}
		}

		releaseResources();
	}
	
	void SpriteComponent_Bgfx::set_texture(const std::string& texturePath)
	{
		m_texturePath = texturePath;
		load_texture(texturePath);
	}
	
	void SpriteComponent_Bgfx::load_texture(const std::string& path)
	{
		const std::string resolved_path = ResourceMgr.resolve_file_path(path);

		auto set_placeholder_texture = [this]()
		{
			acquire_white_texture();
			m_textureLoaded = bgfx::isValid(m_texture);
			m_textureWidth = 1;
			m_textureHeight = 1;
			update_texture_coords();
		};

		// a file some sprite already shows: its texture is shared (nothing is read, decoded or uploaded again)
		if (m_textureOwner == TextureOwner::Cached && m_textureKey == resolved_path && bgfx::isValid(m_texture))
		{
			return;
		}
		SharedTexture hit;
		{
			std::lock_guard<std::mutex> lock(s_textureMutex);
			auto found = s_textures.find(resolved_path);
			if (found != s_textures.end() && bgfx::isValid(found->second.handle))
			{
				++found->second.users;
				hit = found->second;
			}
		}
		if (bgfx::isValid(hit.handle))
		{
			release_texture();
			m_textureOwner = TextureOwner::Cached;
			m_textureKey = resolved_path;
			m_texture = hit.handle;
			m_textureLoaded = true;
			m_textureWidth = hit.width;
			m_textureHeight = hit.height;
			update_texture_coords();
			return;
		}

		std::ifstream input(resolved_path, std::ios::binary | std::ios::ate);
		if (!input.is_open())
		{
			LOG_WARNING("[SpriteComponent_Bgfx] Unable to open texture '%s' (resolved '%s', root '%s'), using placeholder",
				path.c_str(), resolved_path.c_str(), ResourceMgr.get_root_path().c_str());
			set_placeholder_texture();
			return;
		}

		std::streamsize file_size = input.tellg();
		if (file_size <= 0)
		{
			LOG_WARNING("[SpriteComponent_Bgfx] Texture '%s' is empty/unreadable, using placeholder", path.c_str());
			set_placeholder_texture();
			return;
		}

		input.seekg(0, std::ios::beg);
		std::vector<uint8_t> file_data(static_cast<size_t>(file_size));
		if (!input.read(reinterpret_cast<char*>(file_data.data()), file_size))
		{
			LOG_WARNING("[SpriteComponent_Bgfx] Failed to read texture '%s', using placeholder", path.c_str());
			set_placeholder_texture();
			return;
		}

		bx::DefaultAllocator allocator;
		bimg::ImageContainer* image = bimg::imageParse(&allocator, file_data.data(), static_cast<uint32_t>(file_data.size()));
		if (!image)
		{
			LOG_WARNING("[SpriteComponent_Bgfx] Failed to decode texture '%s', using placeholder", path.c_str());
			set_placeholder_texture();
			return;
		}

		release_texture();

		const bgfx::Memory* mem = bgfx::copy(image->m_data, image->m_size);
		bgfx::TextureHandle created = bgfx::createTexture2D(
			static_cast<uint16_t>(image->m_width),
			static_cast<uint16_t>(image->m_height),
			image->m_numMips > 1,
			image->m_numLayers,
			static_cast<bgfx::TextureFormat::Enum>(image->m_format),
			BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT,
			mem);

		m_textureLoaded = bgfx::isValid(created);
		m_textureWidth = image->m_width;
		m_textureHeight = image->m_height;
		bimg::imageFree(image);
		if (m_textureLoaded)
		{
			// the first sprite to show the file puts its texture in the cache (another thread may have been faster: its one is used)
			std::lock_guard<std::mutex> lock(s_textureMutex);
			SharedTexture& shared = s_textures[resolved_path];
			if (bgfx::isValid(shared.handle))
			{
				bgfx::destroy(created);
				created = shared.handle;
			}
			else
			{
				shared.handle = created;
				shared.width = m_textureWidth;
				shared.height = m_textureHeight;
			}
			++shared.users;
			m_textureOwner = TextureOwner::Cached;
			m_textureKey = resolved_path;
			m_texture = created;
		}

		if (!m_textureLoaded)
		{
			LOG_WARNING("[SpriteComponent_Bgfx] bgfx texture creation failed for '%s', using placeholder", path.c_str());
			set_placeholder_texture();
			return;
		}

		LOG_INFO("[SpriteComponent_Bgfx] Texture loaded: %s (%dx%d)", path.c_str(), m_textureWidth, m_textureHeight);
		update_texture_coords();
	}
	
	void SpriteComponent_Bgfx::create_solid_color(uint32_t color, int width, int height)
	{
		// the shared 1x1 white texture, colored and scaled
		acquire_white_texture();
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
	
	void SpriteComponent_Bgfx::set_position(const tmath::vectorn<float, 2>& pos)
	{
		m_position = pos;
		m_vertexBufferDirty = true;
	}
	
	void SpriteComponent_Bgfx::set_position(float x, float y)
	{
		set_position(tmath::vectorn<float, 2>(x, y));
	}
	
	void SpriteComponent_Bgfx::set_rotation(float angle)
	{
		m_rotation = angle;
		m_vertexBufferDirty = true;
	}
	
	void SpriteComponent_Bgfx::set_scale(const tmath::vectorn<float, 2>& scale)
	{
		m_scale = scale;
		m_vertexBufferDirty = true;
	}
	
	void SpriteComponent_Bgfx::set_scale(float x, float y)
	{
		set_scale(tmath::vectorn<float, 2>(x, y));
	}
	
	void SpriteComponent_Bgfx::set_origin(float x, float y)
	{
		m_origin = tmath::vectorn<float, 2>(x, y);
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
			// From the texture now: a texture that did not load is a 1x1 placeholder narrower than one frame (0 per row)
			const int framesPerRow = std::max(1, m_textureWidth / m_frameWidth);
			int frameX = (m_currentFrame % framesPerRow) * m_frameWidth;
			int frameY = (m_currentFrame / framesPerRow) * m_frameHeight;
			
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
		vertices[0].v = v0;
		vertices[0].w = 0.0f;
		vertices[0].q = 0.0f;
		vertices[0].abgr = m_color;
		
		// Bottom-right
		vertices[1].x = right;
		vertices[1].y = bottom;
		vertices[1].z = 0.0f;
		vertices[1].u = u1;
		vertices[1].v = v0;
		vertices[1].w = 0.0f;
		vertices[1].q = 0.0f;
		vertices[1].abgr = m_color;
		
		// Top-right
		vertices[2].x = right;
		vertices[2].y = top;
		vertices[2].z = 0.0f;
		vertices[2].u = u1;
		vertices[2].v = v1;
		vertices[2].w = 0.0f;
		vertices[2].q = 0.0f;
		vertices[2].abgr = m_color;
		
		// Top-left
		vertices[3].x = left;
		vertices[3].y = top;
		vertices[3].z = 0.0f;
		vertices[3].u = u0;
		vertices[3].v = v1;
		vertices[3].w = 0.0f;
		vertices[3].q = 0.0f;
		vertices[3].abgr = m_color;
		
		// Destroy old vertex buffer if it exists
		if (bgfx::isValid(m_vertexBuffer))
		{
			bgfx::destroy(m_vertexBuffer);
		}
		
		// Create new vertex buffer (use bgfx::copy so bgfx owns the data,
		// since vertices[] is a local stack array that will be freed on return)
		m_vertexBuffer = bgfx::createVertexBuffer(
			bgfx::copy(vertices, sizeof(vertices)),
			SpriteVertex::ms_layout
		);
		
		m_vertexBufferDirty = false;
	}
	
	void SpriteComponent_Bgfx::render(bgfx::ViewId viewId, const float* viewProj)
	{
		if (!m_visible || !m_textureLoaded)
		{
			return;
		}
		
		// Update vertex buffer if needed
		if (m_vertexBufferDirty)
		{
			update_vertex_buffer();
		}
		
		// Bail early if any handle is invalid
		if (!bgfx::isValid(m_vertexBuffer) || !bgfx::isValid(m_indexBuffer) || !bgfx::isValid(m_program))
		{
			LOG_ERROR("[SpriteComponent_Bgfx] INVALID handle: vb=%d ib=%d prog=%d",
				bgfx::isValid(m_vertexBuffer) ? 1 : 0,
				bgfx::isValid(m_indexBuffer) ? 1 : 0,
				bgfx::isValid(m_program) ? 1 : 0);
			return;
		}

		// Apply transformations: scale, rotation, translation
		float transform[16];
		bx::mtxSRT(transform,
			m_scale.x, m_scale.y, 1.0f,
			0.0f, 0.0f, m_rotation,
			m_position.x, m_position.y, (float)m_layer * 0.01f
		);
		bgfx::setTransform(transform);

		bgfx::setVertexBuffer(0, m_vertexBuffer);
		bgfx::setIndexBuffer(m_indexBuffer);

		if (bgfx::isValid(s_texColorUniform) && bgfx::isValid(m_texture))
		{
			bgfx::setTexture(0, s_texColorUniform, m_texture);
		}
		else
		{
			LOG_ERROR("[SpriteComponent_Bgfx] texture/uniform invalid: uniform=%d tex=%d",
				bgfx::isValid(s_texColorUniform) ? 1 : 0, bgfx::isValid(m_texture) ? 1 : 0);
		}

		uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA;
		bgfx::setState(state);

		bgfx::submit(viewId, m_program);
	}
}
