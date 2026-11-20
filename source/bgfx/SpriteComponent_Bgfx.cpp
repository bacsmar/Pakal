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
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
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
		// the sprite's own looks (fs_sprite_simple): set for every sprite drawn, since a uniform keeps the last value given
		bgfx::UniformHandle s_flashUniform = BGFX_INVALID_HANDLE, s_rimUniform = BGFX_INVALID_HANDLE, s_rimDirUniform = BGFX_INVALID_HANDLE,
			s_hueUniform = BGFX_INVALID_HANDLE, s_lifeUniform = BGFX_INVALID_HANDLE, s_texLifeUniform = BGFX_INVALID_HANDLE;

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
		// The textures being loaded (by a preload's worker or a sprite): a thread that asks for one meanwhile waits, it does not read it again
		struct TextureLoad
		{
			bool done = false;
			bool failed = false;
		};
		std::unordered_map<std::string, std::shared_ptr<TextureLoad>> s_loading;
		std::condition_variable s_textureLoaded;
		SharedTexture s_white;   // the 1x1 white texture of the solid colors and the placeholders
	}

	// The sprites' uniforms, made once on the graphics thread when it starts (made from the game's thread with the first sprite, bgfx
	// kept that thread waiting for seconds on a phone: the first fight of a session loaded seven seconds late)
	void SpriteComponent_Bgfx::create_uniforms()
	{
		if (!bgfx::isValid(s_texColorUniform)) s_texColorUniform = bgfx::createUniform("s_texColor", bgfx::UniformType::Sampler);
		if (!bgfx::isValid(s_flashUniform)) s_flashUniform = bgfx::createUniform("u_spriteFlash", bgfx::UniformType::Vec4);
		if (!bgfx::isValid(s_rimUniform)) s_rimUniform = bgfx::createUniform("u_spriteRim", bgfx::UniformType::Vec4);
		if (!bgfx::isValid(s_rimDirUniform)) s_rimDirUniform = bgfx::createUniform("u_spriteRimDir", bgfx::UniformType::Vec4);
		if (!bgfx::isValid(s_hueUniform)) s_hueUniform = bgfx::createUniform("u_spriteHue", bgfx::UniformType::Vec4);
		if (!bgfx::isValid(s_lifeUniform)) s_lifeUniform = bgfx::createUniform("u_spriteLife", bgfx::UniformType::Vec4);
		if (!bgfx::isValid(s_texLifeUniform)) s_texLifeUniform = bgfx::createUniform("s_texLife", bgfx::UniformType::Sampler);
	}

	void SpriteComponent_Bgfx::destroy_uniforms()
	{
		for (bgfx::UniformHandle* u : { &s_texColorUniform, &s_flashUniform, &s_rimUniform, &s_rimDirUniform, &s_hueUniform, &s_lifeUniform, &s_texLifeUniform })
		{
			if (bgfx::isValid(*u)) bgfx::destroy(*u);
			*u = BGFX_INVALID_HANDLE;
		}
	}

	void SpriteComponent_Bgfx::release_texture()
	{
		release_texture_of(m_textureOwner, m_textureKey);
		m_textureOwner = TextureOwner::None;
		m_textureKey.clear();
		m_texture = BGFX_INVALID_HANDLE;
	}

	void SpriteComponent_Bgfx::release_texture_of(TextureOwner owner, const std::string& key)
	{
		std::lock_guard<std::mutex> lock(s_textureMutex);
		switch (owner)
		{
		case TextureOwner::Cached:
		{
			auto found = s_textures.find(key);
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
		// (the uniforms are the graphics system's, made when it starts: create_uniforms; a sprite made here touches none)
		
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
		}
		else
		{
			LOG_WARNING("[SpriteComponent_Bgfx] Sprite created but no graphics system!");
		}
	}
	
	SpriteComponent_Bgfx::~SpriteComponent_Bgfx()
	{
		// out of the list the graphics thread draws, at once (under its lock: it is not being drawn now, and never will be again)
		if (m_graphics_system) m_graphics_system->unregister_sprite(this);
		// what the GPU holds is let go of on the graphics thread, without waiting for it: a scene of hundreds of sprites leaves in an
		// instant, instead of a frame for each one (the sprite itself is gone: the block holds the handles, not the sprite)
		const bgfx::VertexBufferHandle vertices = m_vertexBuffer;
		const bgfx::IndexBufferHandle indices = m_indexBuffer, meshIndices = m_meshIndexBuffer;
		const TextureOwner owner = m_textureOwner;
		const std::string key = m_textureKey;
		auto releaseResources = [vertices, indices, meshIndices, owner, key]()
		{
			release_texture_of(owner, key);
			if (bgfx::isValid(vertices)) bgfx::destroy(vertices);
			if (bgfx::isValid(indices)) bgfx::destroy(indices);
			if (bgfx::isValid(meshIndices)) bgfx::destroy(meshIndices);
		};
		if (m_graphics_system)
		{
			auto state = m_graphics_system->get_state();
			if (state == SystemState::Running || state == SystemState::Paused)
			{
				m_graphics_system->execute_block(releaseResources);
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
	
	bool SpriteComponent_Bgfx::acquire_texture(const std::string& path, std::string& key, bgfx::TextureHandle& handle, int& width, int& height)
	{
		key = ResourceMgr.resolve_file_path(path);
		// cached: one more user. Being loaded by another thread (a preload, another sprite): wait for it, then take it from the
		// cache. Else this thread loads it, and the others that ask meanwhile wait for it.
		std::shared_ptr<TextureLoad> mine;
		{
			std::unique_lock<std::mutex> lock(s_textureMutex);
			for (;;)
			{
				auto found = s_textures.find(key);
				if (found != s_textures.end() && bgfx::isValid(found->second.handle))
				{
					++found->second.users;
					handle = found->second.handle;
					width = found->second.width;
					height = found->second.height;
					return true;
				}
				auto loading = s_loading.find(key);
				if (loading == s_loading.end()) break;
				const std::shared_ptr<TextureLoad> other = loading->second;
				s_textureLoaded.wait(lock, [&other]() { return other->done; });
				if (other->failed) return false;
			}
			mine = std::make_shared<TextureLoad>();
			s_loading[key] = mine;
		}
		auto fail = [&]()
		{
			std::lock_guard<std::mutex> lock(s_textureMutex);
			mine->done = true;
			mine->failed = true;
			s_loading.erase(key);
			s_textureLoaded.notify_all();
			return false;
		};

		// Through the resource manager (a folder, a zip, the APK's assets on Android). A packed copy, the same name with .ktx
		// added (compressed: android/pack_textures.py), wins over the original, which the package may leave out.
		const std::string packed = path + ".ktx";
		// (a texture slow to come is told in the log, with where its time went: reading, decoding, giving it to the GPU)
		const auto startedAt = std::chrono::steady_clock::now();
		const auto file_data = ResourceMgr.read_bytes(ResourceMgr.exists(packed) ? packed : path);
		const auto readAt = std::chrono::steady_clock::now();
		if (!file_data || file_data->empty())
		{
			LOG_WARNING("[SpriteComponent_Bgfx] Unable to read texture '%s' (resolved '%s', root '%s'), using placeholder",
				path.c_str(), key.c_str(), ResourceMgr.get_root_path().c_str());
			return fail();
		}
		bx::DefaultAllocator allocator;
		bimg::ImageContainer* image = bimg::imageParse(&allocator, file_data->data(), static_cast<uint32_t>(file_data->size()));
		const auto decodedAt = std::chrono::steady_clock::now();
		if (!image)
		{
			LOG_WARNING("[SpriteComponent_Bgfx] Failed to decode texture '%s', using placeholder", path.c_str());
			return fail();
		}
		const bgfx::Memory* mem = bgfx::copy(image->m_data, image->m_size);
		const bgfx::TextureHandle created = bgfx::createTexture2D(static_cast<uint16_t>(image->m_width), static_cast<uint16_t>(image->m_height),
			image->m_numMips > 1, image->m_numLayers, static_cast<bgfx::TextureFormat::Enum>(image->m_format), BGFX_TEXTURE_NONE | BGFX_SAMPLER_POINT, mem);
		width = image->m_width;
		height = image->m_height;
		{
			const auto ms = [](auto a, auto b) { return std::chrono::duration<double, std::milli>(b - a).count(); };
			const auto doneAt = std::chrono::steady_clock::now();
			if (ms(startedAt, doneAt) > 100.0)
				LOG_INFO("[SpriteComponent_Bgfx] '%s' %dx%d took %.0f ms (read %.0f, decode %.0f, to the GPU %.0f)", path.c_str(), width, height,
					ms(startedAt, doneAt), ms(startedAt, readAt), ms(readAt, decodedAt), ms(decodedAt, doneAt));
		}
		bimg::imageFree(image);
		if (!bgfx::isValid(created))
		{
			LOG_WARNING("[SpriteComponent_Bgfx] bgfx texture creation failed for '%s', using placeholder", path.c_str());
			return fail();
		}
		{
			std::lock_guard<std::mutex> lock(s_textureMutex);
			SharedTexture& shared = s_textures[key];
			shared.handle = created;
			shared.width = width;
			shared.height = height;
			shared.users = 1;
			handle = created;
			mine->done = true;
			s_loading.erase(key);
			s_textureLoaded.notify_all();
		}
		LOG_INFO("[SpriteComponent_Bgfx] Texture loaded: %s (%dx%d)", path.c_str(), width, height);
		return true;
	}

	void SpriteComponent_Bgfx::release_cached_texture(const std::string& key)
	{
		release_texture_of(TextureOwner::Cached, key);
	}

	void SpriteComponent_Bgfx::load_texture(const std::string& path)
	{
		// a file this sprite already shows: nothing to do
		if (m_textureOwner == TextureOwner::Cached && m_textureKey == ResourceMgr.resolve_file_path(path) && bgfx::isValid(m_texture))
		{
			return;
		}
		// shared with the sprites that show it, or preloaded (BgfxGraphicsSystem::preload_textures): nothing is read, decoded or
		// uploaded again; being loaded by another thread: waited for, not read twice
		std::string key;
		bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
		int width = 0, height = 0;
		if (!acquire_texture(path, key, handle, width, height))
		{
			acquire_white_texture();
			m_textureLoaded = bgfx::isValid(m_texture);
			m_textureWidth = 1;
			m_textureHeight = 1;
			update_texture_coords();
			return;
		}
		release_texture();
		m_textureOwner = TextureOwner::Cached;
		m_textureKey = key;
		m_texture = handle;
		m_textureLoaded = true;
		m_textureWidth = width;
		m_textureHeight = height;
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
	
	void SpriteComponent_Bgfx::set_mesh(const SpriteMesh* mesh)
	{
		if (mesh && mesh->positions.size() >= 6 && mesh->uvs.size() == mesh->positions.size() && mesh->indices.size() >= 3 && mesh->indices.size() % 3 == 0)
		{
			m_mesh = *mesh;
			m_hasMesh = true;
			m_meshIndicesDirty = true;
		}
		else
		{
			m_hasMesh = false;
			m_mesh = SpriteMesh();
		}
		m_vertexBufferDirty = true;
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
	
	static_assert(SpriteComponent_Bgfx::sort_depth(5, 123.0f, false) == 5 && SpriteComponent_Bgfx::sort_depth(150, 0.0f, false) == 99);
	static_assert(SpriteComponent_Bgfx::sort_depth(1, -32767.0f, true) > SpriteComponent_Bgfx::sort_depth(0, 32767.0f, true));
	static_assert(SpriteComponent_Bgfx::sort_depth(3, 2.0f, true) > SpriteComponent_Bgfx::sort_depth(3, 1.0f, true));

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

		if (m_hasMesh && m_textureWidth > 0 && m_textureHeight > 0)
		{
			// A mesh: its own vertices (flipped by mirroring them, the origin moved like the quad's) and indices
			const size_t count = m_mesh.positions.size() / 2;
			std::vector<SpriteVertex> vertices(count);
			for (size_t i = 0; i < count; ++i)
			{
				float x = m_mesh.positions[2 * i], y = m_mesh.positions[2 * i + 1];
				if (m_flipX) x = -x;
				if (m_flipY) y = -y;
				vertices[i].x = x + (0.5f - m_origin.x);
				vertices[i].y = y + (0.5f - m_origin.y);
				vertices[i].z = 0.0f;
				vertices[i].u = m_mesh.uvs[2 * i] / static_cast<float>(m_textureWidth);
				vertices[i].v = m_mesh.uvs[2 * i + 1] / static_cast<float>(m_textureHeight);
				vertices[i].w = 0.0f;
				vertices[i].q = 0.0f;
				vertices[i].abgr = m_color;
			}
			if (bgfx::isValid(m_vertexBuffer))
			{
				bgfx::destroy(m_vertexBuffer);
			}
			m_vertexBuffer = bgfx::createVertexBuffer(bgfx::copy(vertices.data(), static_cast<uint32_t>(vertices.size() * sizeof(SpriteVertex))), SpriteVertex::ms_layout);
			if (m_meshIndicesDirty)
			{
				if (bgfx::isValid(m_meshIndexBuffer))
				{
					bgfx::destroy(m_meshIndexBuffer);
				}
				m_meshIndexBuffer = bgfx::createIndexBuffer(bgfx::copy(m_mesh.indices.data(), static_cast<uint32_t>(m_mesh.indices.size() * sizeof(uint16_t))));
				m_meshIndicesDirty = false;
			}
			m_vertexBufferDirty = false;
			return;
		}

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
		
		const bgfx::IndexBufferHandle indexBuffer = m_hasMesh && bgfx::isValid(m_meshIndexBuffer) ? m_meshIndexBuffer : m_indexBuffer;

		// Bail early if any handle is invalid
		if (!bgfx::isValid(m_vertexBuffer) || !bgfx::isValid(indexBuffer) || !bgfx::isValid(m_program))
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
		bgfx::setIndexBuffer(indexBuffer);

		if (bgfx::isValid(s_texColorUniform) && bgfx::isValid(m_texture))
		{
			bgfx::setTexture(0, s_texColorUniform, m_texture);
		}
		else
		{
			LOG_ERROR("[SpriteComponent_Bgfx] texture/uniform invalid: uniform=%d tex=%d",
				bgfx::isValid(s_texColorUniform) ? 1 : 0, bgfx::isValid(m_texture) ? 1 : 0);
		}

		if (bgfx::isValid(s_flashUniform))
		{
			// (the rim's light in the texture: x mirrored with the sprite, y down as the texture's rows)
			const float rimDir[4] = { m_flipX ? -m_rimDir[0] : m_rimDir[0], m_flipY ? m_rimDir[1] : -m_rimDir[1],
				m_textureWidth > 0 ? 1.0f / static_cast<float>(m_textureWidth) : 0.0f, m_textureHeight > 0 ? 1.0f / static_cast<float>(m_textureHeight) : 0.0f };
			bgfx::setUniform(s_flashUniform, m_flash);
			bgfx::setUniform(s_rimUniform, m_rim);
			bgfx::setUniform(s_rimDirUniform, rimDir);
			bgfx::setUniform(s_hueUniform, m_hue);
			// life (set_life): its mask in the second sampler; a sprite without one draws with strength 0 (the shader skips it)
			const bool life = m_life[1] > 0.0f && m_lifeMask && bgfx::isValid(m_lifeMask->m_texture) && bgfx::isValid(s_texLifeUniform);
			const float lifeNow[4] = { m_life[0], life ? m_life[1] : 0.0f, 0.0f, 0.0f };
			if (bgfx::isValid(s_lifeUniform)) bgfx::setUniform(s_lifeUniform, lifeNow);
			// (the mask smoothly: linear between its texels, so its edges and its half size do not show as blocks)
			if (life) bgfx::setTexture(1, s_texLifeUniform, m_lifeMask->m_texture, BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
		}

		uint64_t state = BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_BLEND_ALPHA;
		bgfx::setState(state);

		// its layer is its depth: the main view sorts by depth (BgfxGraphicsSystem: DepthAscending), and with all at 0 bgfx ordered the
		// sprites by their program, so a sprite of a lower layer with another program (a fighter's, its rim and flash) was drawn over
		// a higher one (a black cover). 0 to 99: still before any mesh past a meter (a mesh's depth is its distance in centimetres)
		// (LayerThenOrder: a 2D game's choice, its layer then its order within it)
		const bool byOrder = m_graphics_system && m_graphics_system->sprite_order() == GraphicsSystem::SpriteOrder::LayerThenOrder;
		bgfx::submit(viewId, m_program, sort_depth(m_layer, m_order, byOrder));
	}
}
