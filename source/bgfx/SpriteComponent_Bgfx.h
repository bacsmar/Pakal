///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SpriteComponent_Bgfx.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: January 30, 2026
//
// Purpose: 2D Sprite rendering component using bgfx
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Config.h"
#include "Component.h"
#include <bgfx/bgfx.h>
#include "math/vector2.h"
#include "math/vector3.h"
#include <string>

namespace Pakal
{
	class _PAKALExport SpriteComponent_Bgfx : public Component
	{
		DECLARE_RTTI_WITH_BASE(SpriteComponent_Bgfx, Component);
		
	public:
		explicit SpriteComponent_Bgfx();
		virtual ~SpriteComponent_Bgfx();
		
		// Texture management
		void set_texture(const std::string& texturePath);
		void set_texture_region(int x, int y, int width, int height); // For sprite sheets
		
		// Transform
		void set_position(const tmath::vectorn<float, 2>& pos);
		void set_position(float x, float y);
		tmath::vectorn<float, 2> get_position() const { return m_position; }
		
		void set_rotation(float angle); // In radians
		float get_rotation() const { return m_rotation; }
		
		void set_scale(const tmath::vectorn<float, 2>& scale);
		void set_scale(float x, float y);
		void set_scale(float uniform) { set_scale(uniform, uniform); }
		tmath::vectorn<float, 2> get_scale() const { return m_scale; }
		
		void set_origin(float x, float y); // Pivot point (0,0 = top-left, 0.5,0.5 = center)
		tmath::vectorn<float, 2> get_origin() const { return m_origin; }
		
		// Rendering properties
		void set_layer(int layer); // Z-order for sorting
		int get_layer() const { return m_layer; }
		
		void set_color(uint32_t rgba);
		void set_color(float r, float g, float b, float a = 1.0f);
		uint32_t get_color() const { return m_color; }
		
		void set_flip(bool flipX, bool flipY);
		bool get_flip_x() const { return m_flipX; }
		bool get_flip_y() const { return m_flipY; }
		
		void set_visible(bool visible) { m_visible = visible; }
		bool is_visible() const { return m_visible; }
		
		// Animation support (simple frame-based)
		void set_frame(int frame);
		int get_frame() const { return m_currentFrame; }
		
		void set_frame_size(int width, int height);
		void set_animation_fps(float fps) { m_animFps = fps; }
		void play_animation(int startFrame, int endFrame, bool loop = true);
		void stop_animation();
		
		// Lifecycle
		void update(float deltaTime);
		
		// Rendering (called by graphics system)
		void render(bgfx::ViewId viewId, const float* viewProj);
		
		// Create solid color sprite
		void create_solid_color(uint32_t color, int width, int height);
		
	private:
		void update_vertex_buffer();
		void update_texture_coords();
		void update_animation(float deltaTime);
		void load_texture(const std::string& path);
		
		// Vertex structure for sprite quad
		struct SpriteVertex
		{
			float x, y, z;
			float u, v;
			uint32_t abgr;
			
			static void init();
			static bgfx::VertexLayout ms_layout;
		};
		
		// Texture
		bgfx::TextureHandle m_texture;
		bool m_textureLoaded;
		std::string m_texturePath;
		int m_textureWidth;
		int m_textureHeight;
		
		// Buffers
		bgfx::VertexBufferHandle m_vertexBuffer;
		bgfx::IndexBufferHandle m_indexBuffer;
		bool m_buffersCreated;
		
		// Shader program
		bgfx::ProgramHandle m_program;
		bool m_programLoaded;
		
		// Transform
		tmath::vectorn<float, 2> m_position;
		float m_rotation; // radians
		tmath::vectorn<float, 2> m_scale;
		tmath::vectorn<float, 2> m_origin; // 0-1 normalized
		
		// Texture region (for sprite sheets)
		int m_texRegion[4]; // x, y, width, height in pixels
		
		// Rendering state
		int m_layer;
		uint32_t m_color; // ABGR format for bgfx
		bool m_flipX;
		bool m_flipY;
		bool m_visible;
		
		// Animation
		int m_currentFrame;
		int m_frameWidth;
		int m_frameHeight;
		int m_framesPerRow;
		int m_animStart;
		int m_animEnd;
		bool m_animLoop;
		bool m_animPlaying;
		float m_animFps;
		float m_animTime;
		
		// Flags
		bool m_vertexBufferDirty;
	};
}
