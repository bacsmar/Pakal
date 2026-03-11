///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: BgfxGraphicsSystem.cpp
// Original Author: Salvador Noel Romo Garcia.
// Creation Date: January 29, 2026
//
// Purpose: bgfx graphics system implementation
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "BgfxGraphicsSystem.h"
#include "IDebugDrawer.h"
#include "OSManager.h"
#include "ComponentFactory.h"
#include "LogMgr.h"

#include "SpriteComponent_Bgfx.h"
#include "CameraComponent_Bgfx.h"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <bx/file.h>
#include <bx/readerwriter.h>
#include <string>

namespace
{
	const bgfx::Memory* load_shader_memory(const char* path)
	{
		bx::FileReader reader;
		if (!bx::open(&reader, path))
		{
			return nullptr;
		}

		uint32_t size = static_cast<uint32_t>(bx::getSize(&reader));
		const bgfx::Memory* mem = bgfx::alloc(size + 1);
		bx::read(&reader, mem->data, size, bx::ErrorAssert{});
		bx::close(&reader);
		mem->data[mem->size - 1] = '\0';
		return mem;
	}

	const char* get_shader_renderer_dir(bgfx::RendererType::Enum type)
	{
		switch (type)
		{
		case bgfx::RendererType::Direct3D11: return "dx11";
		case bgfx::RendererType::Direct3D12: return "dxil";
		case bgfx::RendererType::Metal:      return "metal";
		case bgfx::RendererType::OpenGL:     return "glsl";
		case bgfx::RendererType::OpenGLES:   return "essl";
		case bgfx::RendererType::Vulkan:     return "spirv";
		case bgfx::RendererType::WebGPU:     return "wgsl";
		case bgfx::RendererType::Noop:
		case bgfx::RendererType::Count:
		default:                             return "glsl";
		}
	}
}

namespace Pakal
{
	BgfxGraphicsSystem::BgfxGraphicsSystem(const Settings& settings) : GraphicsSystem(settings),
		m_resized_callback_id(0), m_destroyed_callback_id(0), m_created_callback_id(0),
		m_sprite_program(BGFX_INVALID_HANDLE)
	{
		LOG_INFO("[BgfxGraphicsSystem] Constructing bgfx graphics system");
	}

	BgfxGraphicsSystem::~BgfxGraphicsSystem()
	{
		LOG_INFO("[BgfxGraphicsSystem] Destroying bgfx graphics system");
	}

	void BgfxGraphicsSystem::setup_window_callbacks()
	{
		m_created_callback_id = OSMgr.event_window_created.add_listener([this](const WindowArgs& args)
		{
			LOG_INFO("[BgfxGraphicsSystem] Window created callback");
		});

		m_resized_callback_id = OSMgr.event_window_resized.add_listener([this](const WindowArgs& args)
		{
			on_window_resized(args);
		});

		m_destroyed_callback_id = OSMgr.event_window_destroyed.add_listener([this](const WindowArgs& args)
		{
			LOG_INFO("[BgfxGraphicsSystem] Window destroyed callback");
		});
	}

	void BgfxGraphicsSystem::on_window_resized(const WindowArgs& args)
	{
		LOG_INFO("[BgfxGraphicsSystem] Window resized to %dx%d", args.size_x, args.size_y);
		m_width = args.size_x;
		m_height = args.size_y;
		bgfx::reset(m_width, m_height, m_reset_flags);
		bgfx::setViewRect(m_main_view_id, 0, 0, bgfx::BackbufferRatio::Equal);
	}

	void BgfxGraphicsSystem::on_init_graphics(const WindowArgs& args)
	{
		LOG_INFO("[BgfxGraphicsSystem] Initializing bgfx graphics system");
		LOG_INFO("[BgfxGraphicsSystem] Window ID: %u, Resolution: %dx%d", args.windowId, args.size_x, args.size_y);

		m_width = args.size_x;
		m_height = args.size_y;

		// Setup bgfx initialization parameters
		bgfx::Init init;
		init.type = bgfx::RendererType::OpenGL; // Force OpenGL for Linux
		init.vendorId = BGFX_PCI_ID_NONE;
		
		// Set platform data - for X11 we need both display and window
		#if BX_PLATFORM_LINUX
			init.platformData.ndt = args.native_display; // X11 Display*
			init.platformData.nwh = (void*)(uintptr_t)args.windowId; // X11 Window
			LOG_INFO("[BgfxGraphicsSystem] Using X11: Display=%p, Window=%u", args.native_display, args.windowId);
		#else
			init.platformData.nwh = (void*)(uintptr_t)args.windowId;
			init.platformData.ndt = nullptr;
		#endif
		
		// Set resolution
		init.resolution.width = m_width;
		init.resolution.height = m_height;
		init.resolution.reset = m_settings.vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;
		m_reset_flags = init.resolution.reset;

		LOG_INFO("[BgfxGraphicsSystem] Calling bgfx::init()...");
		
		// Initialize bgfx
		if (!bgfx::init(init))
		{
			LOG_ERROR("[BgfxGraphicsSystem] Failed to initialize bgfx!");
			return;
		}

		LOG_INFO("[BgfxGraphicsSystem] bgfx initialized successfully");
		LOG_INFO("[BgfxGraphicsSystem] Renderer: %s", bgfx::getRendererName(bgfx::getRendererType()));

		// Create sprite shader program
		create_sprite_shader();

		// Setup main view
		bgfx::setViewClear(m_main_view_id, 
			BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 
			0x303030ff, // Dark gray background
			1.0f, 
			0);
		bgfx::setViewRect(m_main_view_id, 0, 0, uint16_t(m_width), uint16_t(m_height));

		// Setup window event callbacks
		setup_window_callbacks();

		// Initialize UI manager if configured
		if (m_settings.ui_manager_allocator)
		{
			m_ui_manager = m_settings.ui_manager_allocator(this);
			if (m_ui_manager)
			{
				LOG_INFO("[BgfxGraphicsSystem] UI manager initialized");
			}
		}
	}

	void BgfxGraphicsSystem::on_terminate_graphics()
	{
		LOG_INFO("[BgfxGraphicsSystem] Terminating bgfx graphics system");

		// Clear sprite registrations
		m_sprites.clear();
		m_active_camera = nullptr;

		// Destroy sprite shader
		if (bgfx::isValid(m_sprite_program))
		{
			bgfx::destroy(m_sprite_program);
		}

		// Remove event listeners
		OSMgr.event_window_created	 -= m_created_callback_id;
		OSMgr.event_window_resized	 -= m_resized_callback_id;
		OSMgr.event_window_destroyed -= m_destroyed_callback_id;

		// Cleanup UI manager
		if (m_ui_manager)
		{
			delete m_ui_manager;
			m_ui_manager = nullptr;
		}

		// Shutdown bgfx
		bgfx::shutdown();
		LOG_INFO("[BgfxGraphicsSystem] bgfx shutdown complete");
	}

	void BgfxGraphicsSystem::on_update_graphics(long long dt)
	{
			static int update_count = 0;
			if (update_count++ == 0)
			{
				LOG_INFO("[BgfxGraphicsSystem] on_update_graphics() called for first time");
			}
		
			// Debug text overlay (visible even without sprite rendering)
			m_debug_text_timer += static_cast<float>(dt) / 1000.0f;
			if (m_debug_text_timer >= 0.5f)
			{
				m_debug_text_timer = 0.0f;
				m_debug_text_visible = !m_debug_text_visible;
			}

			bgfx::dbgTextClear();
			bgfx::dbgTextPrintf(1, 1, 0x0f, "CONTRA GAME (BGFX)");
			bgfx::dbgTextPrintf(1, 3, 0x0f, "Renderer: %s", bgfx::getRendererName(bgfx::getRendererType()));
			if (m_debug_text_visible)
			{
				bgfx::dbgTextPrintf(1, 5, 0x0f, "PRESS ANY KEY TO START");
			}

		// Set view and projection matrices for main view (2D orthographic for now)
		bgfx::setViewRect(m_main_view_id, 0, 0, uint16_t(m_width), uint16_t(m_height));

		// Apply camera matrices if we have an active camera
		if (m_active_camera)
		{
			m_active_camera->apply_camera(m_main_view_id);
		}

		// Render all registered sprites
		render_sprites();

		// Touch view to ensure it's rendered
		bgfx::touch(m_main_view_id);

		// Execute debug drawing
		for (auto* debugDrawer : m_debug_renderers)
		{
			if (debugDrawer)
			{
				debugDrawer->do_debug_draw();
			}
		}

		// Log every 60 frames to verify rendering continues
		static int total_frames = 0;
		total_frames++;
		if (total_frames % 60 == 0)
		{
			LOG_INFO("[BgfxGraphicsSystem] Frame %d - calling bgfx::frame()", total_frames);
		}

		// Advance to next frame
		bgfx::frame();
	}

	void BgfxGraphicsSystem::on_pause_graphics()
	{
		LOG_INFO("[BgfxGraphicsSystem] Pausing graphics");
	}

	void BgfxGraphicsSystem::on_resume_graphics()
	{
		LOG_INFO("[BgfxGraphicsSystem] Resuming graphics");
	}

	tmath::vector2du BgfxGraphicsSystem::get_screen_resolution()
	{
		return tmath::vector2du(m_width, m_height);
	}

	void BgfxGraphicsSystem::register_component_factories(std::vector<IComponentFactory*>& factories)
	{
		LOG_INFO("[BgfxGraphicsSystem] Registering bgfx component factories");
		factories.emplace_back(CreateComponentFactory<SpriteComponent_Bgfx, SpriteComponent_Bgfx>(this));
		factories.emplace_back(CreateComponentFactory<CameraComponent_Bgfx, CameraComponent_Bgfx>(this));
	}

	void BgfxGraphicsSystem::add_debug_drawer(IDebugDrawerClient* debugDrawer)
	{
		if (debugDrawer)
		{
			LOG_INFO("[BgfxGraphicsSystem] Adding debug drawer");
			m_debug_renderers.push_back(debugDrawer);
			
			// Set the renderer info for the debug drawer
			// TODO: Create BgfxRendererInfo when needed
			// For now, passing nullptr is acceptable as debug drawing works without it
			debugDrawer->set_drawer(nullptr);
		}
	}

	void BgfxGraphicsSystem::register_sprite(SpriteComponent_Bgfx* sprite)
	{
		if (sprite)
		{
			m_sprites.push_back(sprite);
			// Assign shader program to sprite
			if (bgfx::isValid(m_sprite_program))
			{
				sprite->set_program(m_sprite_program);
			}
			LOG_INFO("[BgfxGraphicsSystem] Sprite registered, total sprites: %zu", m_sprites.size());
		}
	}

	void BgfxGraphicsSystem::unregister_sprite(SpriteComponent_Bgfx* sprite)
	{
		if (sprite)
		{
			auto it = std::find(m_sprites.begin(), m_sprites.end(), sprite);
			if (it != m_sprites.end())
			{
				m_sprites.erase(it);
			}
		}
	}

	void BgfxGraphicsSystem::set_active_camera(CameraComponent_Bgfx* camera)
	{
		m_active_camera = camera;
	}

	void BgfxGraphicsSystem::render_sprites()
	{
		static int call_count = 0;
		static bool first_call_logged = false;
		
		if (!first_call_logged)
		{
			LOG_INFO("[BgfxGraphicsSystem] render_sprites() called for first time, sprites: %zu", m_sprites.size());
			first_call_logged = true;
		}
		
		if (m_sprites.empty())
		{
			static bool logged = false;
			if (!logged)
			{
				LOG_WARNING("[BgfxGraphicsSystem] No sprites registered for rendering");
				logged = true;
			}
			return;
		}

		static int frame_count = 0;
		if (frame_count++ % 60 == 0)
		{
			LOG_INFO("[BgfxGraphicsSystem] Rendering %zu sprites (frame %d)", m_sprites.size(), frame_count);
		}

		// Apply camera if available
		if (m_active_camera)
		{
			m_active_camera->apply_camera(m_main_view_id);
		}
		else
		{
			static bool camera_warned = false;
			if (!camera_warned)
			{
				LOG_WARNING("[BgfxGraphicsSystem] No active camera set");
				camera_warned = true;
			}
		}

		// Render all sprites
		for (auto* sprite : m_sprites)
		{
			if (sprite)
			{
				sprite->render(m_main_view_id, nullptr);
			}
		}
	}

	void BgfxGraphicsSystem::create_sprite_shader()
	{
		LOG_INFO("[BgfxGraphicsSystem] Creating sprite shader from binaries");

		const char* renderer_dir = get_shader_renderer_dir(bgfx::getRendererType());
		// Use particle shaders - simple and perfect for 2D sprites (Position + TexCoord0 + Color0)
		//const char* vs_name = "vs_particle";
		//const char* fs_name = "fs_particle";
		const char* vs_name = "vs_cubes";
		const char* fs_name = "fs_cubes";

		LOG_INFO("[BgfxGraphicsSystem] Renderer directory: %s", renderer_dir);

		const char* roots[] = {
			"Assets/shaders",
			"shaders",
			"bin/Assets/shaders",
			"bin/shaders",
			"../bin/Assets/shaders",
			"../bin/shaders"
		};
		for (const char* root : roots)
		{
			std::string vs_path = std::string(root) + "/" + renderer_dir + "/" + vs_name + ".bin";
			std::string fs_path = std::string(root) + "/" + renderer_dir + "/" + fs_name + ".bin";

			LOG_INFO("[BgfxGraphicsSystem] Trying VS: %s", vs_path.c_str());
			LOG_INFO("[BgfxGraphicsSystem] Trying FS: %s", fs_path.c_str());

			const bgfx::Memory* vs_mem = load_shader_memory(vs_path.c_str());
			const bgfx::Memory* fs_mem = load_shader_memory(fs_path.c_str());

			if (vs_mem)
			{
				LOG_INFO("[BgfxGraphicsSystem] VS loaded, size: %d bytes", vs_mem->size);
			}
			else
			{
				LOG_WARNING("[BgfxGraphicsSystem] VS failed to load from %s", vs_path.c_str());
			}

			if (fs_mem)
			{
				LOG_INFO("[BgfxGraphicsSystem] FS loaded, size: %d bytes", fs_mem->size);
			}
			else
			{
				LOG_WARNING("[BgfxGraphicsSystem] FS failed to load from %s", fs_path.c_str());
			}

			if (vs_mem && fs_mem)
			{
				bgfx::ShaderHandle vsh = bgfx::createShader(vs_mem);
				bgfx::ShaderHandle fsh = bgfx::createShader(fs_mem);
				
				if (bgfx::isValid(vsh))
				{
					LOG_INFO("[BgfxGraphicsSystem] Vertex shader created successfully");
					bgfx::setName(vsh, vs_name);
				}
				else
				{
					LOG_ERROR("[BgfxGraphicsSystem] Failed to create vertex shader from memory");
				}

				if (bgfx::isValid(fsh))
				{
					LOG_INFO("[BgfxGraphicsSystem] Fragment shader created successfully");
					bgfx::setName(fsh, fs_name);
				}
				else
				{
					LOG_ERROR("[BgfxGraphicsSystem] Failed to create fragment shader from memory");
				}

				if (bgfx::isValid(vsh) && bgfx::isValid(fsh))
				{
					m_sprite_program = bgfx::createProgram(vsh, fsh, true);
					LOG_INFO("[BgfxGraphicsSystem] Program handle created: %d", m_sprite_program.idx);
					break;
				}
			}
		}

		if (bgfx::isValid(m_sprite_program))
		{
			LOG_INFO("[BgfxGraphicsSystem] Sprite shader program created successfully");
		}
		else
		{
			LOG_ERROR("[BgfxGraphicsSystem] Failed to create sprite shader program (missing binaries?)");
		}
	}
}
