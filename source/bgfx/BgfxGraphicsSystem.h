///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BgfxGraphicsSystem.h
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: January 29, 2026
//
// Purpose: bgfx graphics system implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "GraphicsSystem.h"
#include "ICameraComponent_Bgfx.h"

#include <bgfx/bgfx.h>
#include <vector>

namespace Pakal
{
	class IDebugDrawerClient;
	class SpriteComponent_Bgfx;
	class CameraComponent_Bgfx;
	class CameraComponent3D_Bgfx;
	class SkeletalAnimationComponent_Bgfx;

	class _PAKALExport BgfxGraphicsSystem final : public GraphicsSystem
	{		
	public:
		inline const char* get_system_name() override { return nameof(BgfxGraphicsSystem);  }

		explicit BgfxGraphicsSystem(const Settings& settings);
		virtual ~BgfxGraphicsSystem();

	protected:	
		
		EventId m_resized_callback_id;
		EventId m_destroyed_callback_id;
		EventId m_created_callback_id;

		bgfx::ViewId m_main_view_id = 0;
		uint32_t m_width = 800;
		uint32_t m_height = 600;
		uint32_t m_reset_flags = BGFX_RESET_VSYNC;

		std::vector<IDebugDrawerClient*> m_debug_renderers;
		std::vector<SpriteComponent_Bgfx*> m_sprites;
		std::vector<SkeletalAnimationComponent_Bgfx*> m_skeletalComponents;
		ICameraComponent_Bgfx* m_active_camera = nullptr;
		float m_debug_text_timer = 0.0f;
		bool m_debug_text_visible = true;
		bgfx::ProgramHandle m_sprite_program;
		int m_frame_count = 0;
		int m_screenshot_frame = -1;
		std::string m_screenshot_path;

		void on_init_graphics(const WindowArgs& args) override;
		void on_terminate_graphics() override;
		void on_update_graphics(long long dt) override;
		void on_pause_graphics() override;
		void on_resume_graphics() override;

		virtual tmath::vector2du get_screen_resolution() override;
		virtual void register_component_factories( std::vector<IComponentFactory*> &factories ) override;
		virtual void add_debug_drawer(IDebugDrawerClient * debugDrawer) override;

	private:
		void setup_window_callbacks();
		void on_window_resized(const WindowArgs& args);
		void render_sprites(float dt_seconds);
		void render_skeletal(float dt_seconds);
		void create_sprite_shader();
	
	public:
		void register_sprite(SpriteComponent_Bgfx* sprite);
		void unregister_sprite(SpriteComponent_Bgfx* sprite);
		void register_skeletal(SkeletalAnimationComponent_Bgfx* comp);
		void unregister_skeletal(SkeletalAnimationComponent_Bgfx* comp);
		void set_active_camera(ICameraComponent_Bgfx* camera);
		void clear_active_camera(ICameraComponent_Bgfx* camera);
		bgfx::ProgramHandle get_sprite_program() const { return m_sprite_program; }
		bgfx::ViewId get_main_view_id() const { return m_main_view_id; }
	};	
}
