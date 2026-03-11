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
#include "CameraComponent3D_Bgfx.h"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <debugdraw/debugdraw.h>
#include <bx/file.h>
#include <bx/readerwriter.h>
#include <string>
#include <cstdlib>
#include <cstring>

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

	bgfx::RendererType::Enum parse_renderer_from_env(const char* value)
	{
		if (!value)
		{
			return bgfx::RendererType::Count;
		}

		if (0 == std::strcmp(value, "opengl")) return bgfx::RendererType::OpenGL;
		if (0 == std::strcmp(value, "opengles")) return bgfx::RendererType::OpenGLES;
		if (0 == std::strcmp(value, "vulkan")) return bgfx::RendererType::Vulkan;
		if (0 == std::strcmp(value, "noop")) return bgfx::RendererType::Noop;
		if (0 == std::strcmp(value, "auto")) return bgfx::RendererType::Count;

		return bgfx::RendererType::Count;
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
		const char* renderer_env = std::getenv("PAKAL_BGFX_RENDERER");
		init.type = parse_renderer_from_env(renderer_env);
		init.vendorId = BGFX_PCI_ID_NONE;

		if (init.type == bgfx::RendererType::Count)
		{
			LOG_INFO("[BgfxGraphicsSystem] Renderer selection: auto");
		}
		else
		{
			LOG_INFO("[BgfxGraphicsSystem] Renderer selection from env '%s': %s",
				renderer_env ? renderer_env : "<null>",
				bgfx::getRendererName(init.type));
		}
		
		// Set platform data - for X11 we need both display and window
		#if BX_PLATFORM_LINUX
			init.platformData.ndt = args.native_display; // X11 Display* (may be null in non-X11 sessions)
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

		ddInit();
		LOG_INFO("[BgfxGraphicsSystem] debugdraw initialized");

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
		ddShutdown();
		LOG_INFO("[BgfxGraphicsSystem] debugdraw shutdown complete");

		bgfx::shutdown();
		LOG_INFO("[BgfxGraphicsSystem] bgfx shutdown complete");
	}

	void BgfxGraphicsSystem::on_update_graphics(long long dt)
	{
		// Clear every frame
		bgfx::setViewClear(
			m_main_view_id,
			BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
			0x303030ff,
			1.0f,
			0
		);
		bgfx::setViewRect(m_main_view_id, 0, 0, uint16_t(m_width), uint16_t(m_height));

		// Apply camera matrices if we have an active camera
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
		factories.emplace_back(CreateComponentFactory<CameraComponent3D_Bgfx, CameraComponent3D_Bgfx>(this));
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

	void BgfxGraphicsSystem::set_active_camera(ICameraComponent_Bgfx* camera)
	{
		m_active_camera = camera;
	}

	void BgfxGraphicsSystem::render_sprites()
	{
		if (m_sprites.empty())
		{
			return;
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
		const char* renderer_dir = get_shader_renderer_dir(bgfx::getRendererType());
		// Prefer custom RGBA sprite shaders; keep fallbacks for robustness.
		const char* vs_names[] = { "vs_sprite", "vs_particle", "vs_cubes" };
		const char* fs_names[] = { "fs_sprite", "fs_particle", "fs_cubes" };

		const char* roots[] = {
			"Assets/shaders",
			"shaders",
			"bin/Assets/shaders",
			"bin/shaders",
			"../bin/Assets/shaders",
			"../bin/shaders"
		};
		// Try candidate shader names in priority order
		constexpr int NUM_SHADER_VARIANTS = 3;
		for (int vi = 0; vi < NUM_SHADER_VARIANTS && !bgfx::isValid(m_sprite_program); ++vi)
		{
			const char* vs_name = vs_names[vi];
			const char* fs_name = fs_names[vi];

			for (const char* root : roots)
			{
				std::string vs_path = std::string(root) + "/" + renderer_dir + "/" + vs_name + ".bin";
				std::string fs_path = std::string(root) + "/" + renderer_dir + "/" + fs_name + ".bin";

				const bgfx::Memory* vs_mem = load_shader_memory(vs_path.c_str());
				const bgfx::Memory* fs_mem = load_shader_memory(fs_path.c_str());

				if (vs_mem && fs_mem)
				{
					bgfx::ShaderHandle vsh = bgfx::createShader(vs_mem);
					bgfx::ShaderHandle fsh = bgfx::createShader(fs_mem);

					if (bgfx::isValid(vsh) && bgfx::isValid(fsh))
					{
						bgfx::setName(vsh, vs_name);
						bgfx::setName(fsh, fs_name);
						m_sprite_program = bgfx::createProgram(vsh, fsh, true);
						LOG_INFO("[BgfxGraphicsSystem] Sprite shader selected: %s/%s (%s)", vs_name, fs_name, renderer_dir);
						break;
					}
					else
					{
						if (bgfx::isValid(vsh)) bgfx::destroy(vsh);
						if (bgfx::isValid(fsh)) bgfx::destroy(fsh);
					}
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
