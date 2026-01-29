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

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>

namespace Pakal
{
	BgfxGraphicsSystem::BgfxGraphicsSystem(const Settings& settings) : GraphicsSystem(settings),
		m_resized_callback_id(0), m_destroyed_callback_id(0), m_created_callback_id(0)
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
		init.type = bgfx::RendererType::Count; // Auto-select best renderer
		init.vendorId = BGFX_PCI_ID_NONE;
		
		// Set platform data - native window handle
		init.platformData.nwh = (void*)(uintptr_t)args.windowId;
		init.platformData.ndt = nullptr;
		
		// Set resolution
		init.resolution.width = m_width;
		init.resolution.height = m_height;
		init.resolution.reset = m_settings.vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;
		m_reset_flags = init.resolution.reset;

		// Initialize bgfx
		if (!bgfx::init(init))
		{
			LOG_ERROR("[BgfxGraphicsSystem] Failed to initialize bgfx!");
			return;
		}

		LOG_INFO("[BgfxGraphicsSystem] bgfx initialized successfully");
		LOG_INFO("[BgfxGraphicsSystem] Renderer: %s", bgfx::getRendererName(bgfx::getRendererType()));

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
		// Set view and projection matrices for main view (2D orthographic for now)
		bgfx::setViewRect(m_main_view_id, 0, 0, uint16_t(m_width), uint16_t(m_height));

		// Touch view to ensure it's rendered
		bgfx::touch(m_main_view_id);

		// Update all updatable components
		{
			std::lock_guard<std::mutex> lock(m_updatablesMutex);
			for (auto* updatable : m_updatables)
			{
				updatable->update(static_cast<unsigned>(dt));
			}
		}

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
		// TODO: Register bgfx-specific component factories
		// For now, we don't have any components implemented yet
		LOG_INFO("[BgfxGraphicsSystem] Registering component factories (none yet)");
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
}
