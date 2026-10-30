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
#include "Profiler.h"

#include "SpriteComponent_Bgfx.h"
#include "MeshComponent3D_Bgfx.h"
#include "BgfxCallbacks.h"
#include "ResourceManager.h"
#include "ZipSourceMiniz.h"
#include "IUIManager.h"
#include "CameraComponent_Bgfx.h"
#include "CameraComponent3D_Bgfx.h"
#include "components/SpriteComponent2D.h"
#include "components/CameraComponent2D.h"
#include "components/CameraComponent3D.h"

#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#if defined(__ANDROID__)
#include <sys/system_properties.h>
#include <dlfcn.h>
#endif
#include <debugdraw/debugdraw.h>
#include <bx/file.h>
#include <bx/math.h>
#include <bx/readerwriter.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <cstdio>
#include <sstream>
#include <string>
#include <cstdlib>
#include <cstring>

namespace
{
	const bgfx::Memory* load_shader_memory(const char* path)
	{
		// from wherever the resource manager finds it (a folder, a zip, the APK)
		const auto bytes = ResourceMgr.read_bytes(path);
		if (!bytes)
		{
			return nullptr;
		}
		const uint32_t size = static_cast<uint32_t>(bytes->size());
		const bgfx::Memory* mem = bgfx::alloc(size + 1);
		if (size) std::memcpy(mem->data, bytes->data(), size);
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
		if (m_auto_render_height > 0 && args.size_y > 0)
		{
			// no taller than the most lines wanted: a maximized window is drawn smaller and scaled up
			m_render_scale = std::min(1.0f, static_cast<float>(m_auto_render_height) / static_cast<float>(args.size_y));
			if (m_render_scale < 1.0f) LOG_INFO("[BgfxGraphicsSystem] Drawing at %.0f%% of the window (PAKAL_RENDER_HEIGHT %u)", m_render_scale * 100.0f, m_auto_render_height);
		}
		if (m_render_scale < 1.0f)
		{
			m_output_width = args.size_x;
			m_output_height = args.size_y;
			m_width = std::max(1u, uint32_t(args.size_x * m_render_scale));
			m_height = std::max(1u, uint32_t(args.size_y * m_render_scale));
		}
		else
		{
			m_width = args.size_x;
			m_height = args.size_y;
			if (m_output_width) { m_output_width = args.size_x; m_output_height = args.size_y; }   // drawn at the window's size
			if (m_auto_render_height > 0) m_render_scale = 1.0f;
		}
		reset_backbuffer();
		bgfx::setViewRect(m_main_view_id, 0, 0, uint16_t(m_width), uint16_t(m_height));
	}

	void BgfxGraphicsSystem::set_governor(bool enabled, unsigned maxFps)
	{
		m_governor = Governor();
		m_governor.enabled = enabled;
		m_governor.maxFps = m_governor.cap = maxFps;
		m_governor.baseScale = get_render_scale();
		set_target_fps(maxFps);
		LOG_INFO("[Governor] %s, starting at %u FPS, render scale %.0f%%", enabled ? "on" : "off", maxFps, m_governor.baseScale * 100.0f);
	}

	void BgfxGraphicsSystem::set_fsr(bool on)
	{
		// PAKAL_FSR (a launch's debug switch) wins over the game's saved option: a run told "no FSR" stays without it
		if (const char* forced = std::getenv("PAKAL_FSR"))
		{
			const bool want = std::string(forced) != "0";
			if (want != on) LOG_INFO("[BgfxGraphicsSystem] FSR %s asked, kept %s by PAKAL_FSR", on ? "on" : "off", want ? "on" : "off");
			on = want;
		}
		m_fsr = on;
		m_upscaler = on ? Upscaler::Fsr : Upscaler::Bilinear;
		const float scale = std::clamp(on ? m_settings.fsr_scale : m_settings.plain_scale, 0.25f, 1.0f);
		m_governor.baseScale = scale;
		set_render_scale(scale);
		LOG_INFO("[BgfxGraphicsSystem] FSR %s: drawing at %.0f%% of the screen", on ? "on" : "off", scale * 100.0f);
	}

	std::string BgfxGraphicsSystem::governor_status() const
	{
		char text[160];
		std::snprintf(text, sizeof(text), "%s: cap %u, scale %.0f%%, %.0f FPS, work %.1f ms, thermal %d, wait x%.0f",
			m_governor.enabled ? "Governor" : "Governor off", m_governor.cap, get_render_scale() * 100.0f, m_governor.lastFps,
			m_governor.lastWorkMs, m_governor.thermal, m_governor.backoff);
		return text;
	}

	// The system's thermal status (Android 11+: 0 none ... 6 shutdown), looked up at run time; -1 where there is none
	int BgfxGraphicsSystem::read_thermal_status()
	{
#if BX_PLATFORM_ANDROID
		using Acquire = void* (*)();
		using Status = int (*)(void*);
		static void* library = dlopen("libandroid.so", RTLD_NOW);
		static const auto acquire = library ? reinterpret_cast<Acquire>(dlsym(library, "AThermal_acquireManager")) : nullptr;
		static const auto status = library ? reinterpret_cast<Status>(dlsym(library, "AThermal_getCurrentThermalStatus")) : nullptr;
		static void* manager = acquire ? acquire() : nullptr;
		return manager && status ? status(manager) : -1;
#else
		return -1;
#endif
	}

	// Once a second: too slow for 3 seconds in a row -> a step down (60 -> 30 FPS, then 12% less render scale, down to half
	// the screen); room to spare for 20 seconds -> a step up (the scale 30 s after the last step down, the cap last and 2
	// minutes after). A thermal status of moderate or worse counts as too slow. The room to spare is the game thread's work
	// (the GPU's is not measured on phones): a step up that does not hold for a minute doubles the waits (up to 8 times),
	// so a phone that cannot keep 60 stops trying, and one that can stays there.
	void BgfxGraphicsSystem::governor_frame(double workMs)
	{
		Governor& g = m_governor;
		++g.frames;
		g.workMs += workMs;
		g.worstMs = std::max(g.worstMs, workMs);
		const auto now = std::chrono::steady_clock::now();
		const double seconds = std::chrono::duration<double>(now - g.windowStart).count();
		if (seconds < 1.0)
		{
			return;
		}
		g.lastFps = g.frames / seconds;
		g.lastWorkMs = g.workMs / std::max(1, g.frames);
		const bool loading = g.worstMs > 120.0;   // a level loading in steps: not the device being slow
		g.frames = 0;
		g.workMs = 0.0;
		g.worstMs = 0.0;
		g.windowStart = now;
		g.secondsSinceDown += seconds;
		g.secondsSinceUp += seconds;
		g.thermal = read_thermal_status();
		if (loading) g.warmup = 6;
		const bool warming = g.warmup > 0;
		if (warming) --g.warmup;
		if (!g.enabled || loading || warming || m_governorHeld)
		{
			g.slow = 0;
			return;
		}
		const bool hot = g.thermal >= 2;   // ATHERMAL_STATUS_MODERATE
		const bool slow = hot || g.lastFps < g.cap * 0.87;
		const bool spare = !hot && g.lastWorkMs < 14.0;
		g.slow = slow ? g.slow + 1 : 0;
		g.spare = spare ? g.spare + 1 : 0;
		const float scale = get_render_scale();
		if (g.slow >= 3)
		{
			g.slow = g.spare = 0;
			g.secondsSinceDown = 0.0;
			if (g.secondsSinceUp < 60.0) g.backoff = std::min(8.0, g.backoff * 2.0);   // the last step up did not hold
			if (g.cap > 30 && m_upscaler == Upscaler::Fsr && is_upscaling())
			{
				// the cheapest step first: a little sharpness instead of half the frames
				m_upscaler = Upscaler::Bilinear;
				g.fsrDropped = true;
				LOG_INFO("[Governor] %.0f FPS (thermal %d): the picture stretched plainly instead of FSR", g.lastFps, g.thermal);
			}
			else if (g.cap > 30)
			{
				g.cap = 30;
				set_target_fps(30);
				LOG_INFO("[Governor] %.0f FPS (thermal %d): the picture at 30", g.lastFps, g.thermal);
			}
			else if (can_upscale() && m_fsr && scale > 0.51f)
			{
				set_render_scale(std::max(0.5f, scale * 0.88f));
				LOG_INFO("[Governor] %.0f FPS at 30 (thermal %d): render scale %.0f%%", g.lastFps, g.thermal, std::max(0.5f, scale * 0.88f) * 100.0f);
			}
		}
		else if (g.spare >= 20)
		{
			g.spare = 0;
			if (scale < g.baseScale - 0.01f && g.secondsSinceDown > 30.0 * g.backoff)
			{
				g.secondsSinceUp = 0.0;
				set_render_scale(std::min(g.baseScale, scale / 0.88f));
				LOG_INFO("[Governor] room to spare (%.1f ms a frame): render scale %.0f%%", g.lastWorkMs, std::min(g.baseScale, scale / 0.88f) * 100.0f);
			}
			else if (scale >= g.baseScale - 0.01f && g.cap < g.maxFps && g.secondsSinceDown > 120.0 * g.backoff)
			{
				g.secondsSinceUp = 0.0;
				g.cap = g.maxFps;
				set_target_fps(g.maxFps);
				LOG_INFO("[Governor] room to spare (%.1f ms a frame): trying %u FPS again", g.lastWorkMs, g.maxFps);
			}
			else if (g.fsrDropped && g.cap >= g.maxFps && g.secondsSinceDown > 60.0 * g.backoff)
			{
				g.secondsSinceUp = 0.0;
				g.fsrDropped = false;
				m_upscaler = Upscaler::Fsr;
				LOG_INFO("[Governor] room to spare (%.1f ms a frame): FSR again", g.lastWorkMs);
			}
		}
	}

	// A new render scale: the picture's size changes, the back buffer's does not (the targets are made again for it)
	void BgfxGraphicsSystem::apply_render_scale()
	{
		if (m_pending_render_scale <= 0.0f)
		{
			return;
		}
		const float scale = std::clamp(m_pending_render_scale, 0.25f, 1.0f);
		m_pending_render_scale = 0.0f;
		const uint32_t outW = get_output_width(), outH = get_output_height();
		m_output_width = outW;
		m_output_height = outH;
		m_width = std::max(1u, uint32_t(std::lround(outW * scale)));
		m_height = std::max(1u, uint32_t(std::lround(outH * scale)));
		if (m_render_scale < 1.0f || scale < 1.0f) m_render_scale = scale; // a desktop window keeps it when resized
		LOG_INFO("[BgfxGraphicsSystem] Drawing at %ux%u of %ux%u (%.0f%%)", m_width, m_height, outW, outH, scale * 100.0f);
	}

	void BgfxGraphicsSystem::reset_backbuffer()
	{
		bgfx::reset(get_output_width(), get_output_height(), m_reset_flags);
	}

	void BgfxGraphicsSystem::on_init_graphics(const WindowArgs& args)
	{
		LOG_INFO("[BgfxGraphicsSystem] Initializing bgfx graphics system");
		// the resource manager's zip source (miniz comes with bimg, which this backend links); the resource manager is
		// initialized by now (the engine's run)
		if (!ResourceMgr.has_source_factory<ZipSource>())
		{
			ResourceMgr.register_source<ZipSource>([]() -> ZipSource* { return new ZipSourceMiniz(); });
		}
		// PAKAL_MOUNT_ZIP=<a.zip>[;<b.zip>...]: their files read as if they were on the disk (a pack, a mod, a test)
		if (const char* zips = std::getenv("PAKAL_MOUNT_ZIP"))
		{
			std::stringstream list(zips);
			for (std::string zip; std::getline(list, zip, ';');)
			{
				if (!zip.empty()) ResourceMgr.add_source<ZipSource>(zip, true, std::string());
			}
		}
		LOG_INFO("[BgfxGraphicsSystem] Window ID: %u, Resolution: %dx%d", args.windowId, args.size_x, args.size_y);

		m_width = args.size_x;
		m_height = args.size_y;
		// Drawn smaller than the screen and scaled up: the settings' output size (Android), or PAKAL_RENDER_SCALE on desktops
		// (the same size counts: nothing is scaled until the render scale goes down, but it can)
		if (m_settings.output_resolution.x >= int(m_width) && m_settings.output_resolution.y >= int(m_height) && m_settings.output_resolution.x > 0)
		{
			m_output_width = uint32_t(m_settings.output_resolution.x);
			m_output_height = uint32_t(m_settings.output_resolution.y);
		}
		else if (const char* scale = std::getenv("PAKAL_RENDER_SCALE"))
		{
			m_render_scale = std::clamp(static_cast<float>(std::atof(scale)), 0.25f, 1.0f);
			if (m_render_scale < 1.0f)
			{
				m_output_width = m_width;
				m_output_height = m_height;
				m_width = std::max(1u, uint32_t(m_width * m_render_scale));
				m_height = std::max(1u, uint32_t(m_height * m_render_scale));
			}
		}
#if !BX_PLATFORM_ANDROID
		else
		{
			// A desktop window: drawn at its size up to PAKAL_RENDER_HEIGHT lines (1080 by default), scaled up past that
			const char* lines = std::getenv("PAKAL_RENDER_HEIGHT");
			m_auto_render_height = lines ? static_cast<uint32_t>(std::max(0, std::atoi(lines))) : 1080u;
			if (m_auto_render_height > 0)
			{
				m_output_width = m_width;
				m_output_height = m_height;
				m_render_scale = std::min(1.0f, static_cast<float>(m_auto_render_height) / static_cast<float>(std::max(1u, m_height)));
				m_width = std::max(1u, uint32_t(m_width * m_render_scale));
				m_height = std::max(1u, uint32_t(m_height * m_render_scale));
			}
		}
#endif
		if (const char* mode = std::getenv("PAKAL_UPSCALER")) m_upscaler = std::string(mode) == "bilinear" ? Upscaler::Bilinear : Upscaler::Fsr;
		// stops below the most sharpening; "off": none, and no RCAS pass at all (EASU writes the screen)
		if (const char* sharp = std::getenv("PAKAL_SHARPNESS")) m_sharpness = std::string(sharp) == "off" ? -1.0f : static_cast<float>(std::atof(sharp));
		if (is_upscaling())
		{
			LOG_INFO("[BgfxGraphicsSystem] Drawing at %ux%u, scaled up to %ux%u", m_width, m_height, m_output_width, m_output_height);
		}

		// Setup bgfx initialization parameters
		bgfx::Init init;
		const char* renderer_env = std::getenv("PAKAL_BGFX_RENDERER");
		init.type = parse_renderer_from_env(renderer_env);
#if BX_PLATFORM_ANDROID
		// The Android build has OpenGL ES only (the shaders are compiled for it)
		if (init.type == bgfx::RendererType::Count) init.type = bgfx::RendererType::OpenGLES;
#endif
		// Which GPU on a computer with two (a laptop's integrated one and a dedicated one): PAKAL_GPU=integrated|intel,
		// discrete (the default: NVIDIA, else the system's choice), nvidia, amd. Direct3D picks the adapter by vendor.
		init.vendorId = BGFX_PCI_ID_NVIDIA;
		if (const char* gpu = std::getenv("PAKAL_GPU"))
		{
			const std::string want = gpu;
			if (want == "integrated" || want == "intel") init.vendorId = BGFX_PCI_ID_INTEL;
			else if (want == "amd") init.vendorId = BGFX_PCI_ID_AMD;
			else if (want == "nvidia" || want == "discrete") init.vendorId = BGFX_PCI_ID_NVIDIA;
			else if (want == "any") init.vendorId = BGFX_PCI_ID_NONE;
		}
#if BX_PLATFORM_ANDROID
		init.vendorId = BGFX_PCI_ID_NONE;
#endif
		init.callback = &BgfxCallbacks::instance();

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
		#if BX_PLATFORM_ANDROID
			init.platformData.nwh = args.native_window;
			init.platformData.ndt = nullptr;
		#elif BX_PLATFORM_LINUX
			init.platformData.ndt = args.native_display; // X11 Display* (may be null in non-X11 sessions)
			init.platformData.nwh = (void*)(uintptr_t)args.windowId; // X11 Window
			LOG_INFO("[BgfxGraphicsSystem] Using X11: Display=%p, Window=%u", args.native_display, args.windowId);
		#else
			init.platformData.nwh = (void*)(uintptr_t)args.windowId;
			init.platformData.ndt = nullptr;
		#endif
		
		// Set resolution
		init.resolution.width = get_output_width();
		init.resolution.height = get_output_height();
		init.resolution.reset = m_settings.vsync ? BGFX_RESET_VSYNC : BGFX_RESET_NONE;
		m_reset_flags = init.resolution.reset;

		LOG_INFO("[BgfxGraphicsSystem] Calling bgfx::init()...");
		
		// Initialize bgfx
		if (!bgfx::init(init))
		{
			LOG_ERROR("[BgfxGraphicsSystem] Failed to initialize bgfx!");
			return;
		}

		LOG_INFO("[BgfxGraphicsSystem] bgfx initialized successfully (quality %s)", quality_name(m_quality));
		// Text overlay for draw_text (HUD and debug messages)
		// Performance runs (PAKAL_FPS_LOG) also time every view on the GPU
		bgfx::setDebug(BGFX_DEBUG_TEXT | (std::getenv("PAKAL_FPS_LOG") ? BGFX_DEBUG_PROFILER : 0));
		LOG_INFO("[BgfxGraphicsSystem] Renderer: %s", bgfx::getRendererName(bgfx::getRendererType()));
		{
			const bgfx::Caps* caps = bgfx::getCaps();
			const uint16_t vendor = caps->vendorId;
			LOG_INFO("[BgfxGraphicsSystem] GPU: %s (vendor 0x%04x, device 0x%04x, %u adapter(s))",
				vendor == BGFX_PCI_ID_NVIDIA ? "NVIDIA" : vendor == BGFX_PCI_ID_AMD ? "AMD" : vendor == BGFX_PCI_ID_INTEL ? "Intel" : "other",
				vendor, caps->deviceId, unsigned(caps->numGPUs));
			// what a profiling session says the device draws with
			char gpu[96];
			std::snprintf(gpu, sizeof(gpu), "%s, vendor 0x%04x, device 0x%04x", bgfx::getRendererName(bgfx::getRendererType()), vendor, caps->deviceId);
			Profiler::get().info("gpu", gpu);
		}

		ddInit();
		LOG_INFO("[BgfxGraphicsSystem] debugdraw initialized");

		// Create sprite shader program
		create_sprite_shader();

		// Create 3D mesh shaders, light uniforms and the sky dome
		create_mesh_resources();
		create_post_resources();
		create_shadow_resources();
		m_easu_program = load_program("vs_post", "fs_fsr_easu");
		m_rcas_program = load_program("vs_post", "fs_fsr_rcas");
		const char* fsrNames[5] = { "u_fsrCon0", "u_fsrCon1", "u_fsrCon2", "u_fsrCon3", "u_fsrRcas" };
		for (int i = 0; i < 5; ++i) m_u_fsr[i] = bgfx::createUniform(fsrNames[i], bgfx::UniformType::Vec4);
		m_s_fsr_input = bgfx::createUniform("s_fsrInput", bgfx::UniformType::Sampler);

		// Setup main view
		bgfx::setViewClear(m_main_view_id, 
			BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 
			0x303030ff, // Dark gray background
			1.0f, 
			0);
		bgfx::setViewRect(m_main_view_id, 0, 0, uint16_t(m_width), uint16_t(m_height));

		// Setup window event callbacks
		setup_window_callbacks();

		// The development panels (the game's UI manager is made by GraphicsSystem and initialized after this)
		m_debug_ui.initialize(m_debug_ui_view_id, get_output_height());
		if (m_settings.governor) set_governor(true, m_settings.max_fps);
		m_debug_ui.attach_input(OSMgr.get_input_manager());
	}

	void BgfxGraphicsSystem::on_terminate_graphics()
	{
		LOG_INFO("[BgfxGraphicsSystem] Terminating bgfx graphics system");

		// Clear sprite registrations
		m_sprites.clear();
		m_meshes3d.clear();
		m_active_camera = nullptr;

		// Destroy sprite shader
		if (bgfx::isValid(m_sprite_program))
		{
			bgfx::destroy(m_sprite_program);
		}

		clear_gi_volume();
		clear_lightmap();
		destroy_shadow_resources();
		destroy_post_resources();
		destroy_upscale_resources();
		destroy_mesh_resources();

		// Remove event listeners
		OSMgr.event_window_created	 -= m_created_callback_id;
		OSMgr.event_window_resized	 -= m_resized_callback_id;
		OSMgr.event_window_destroyed -= m_destroyed_callback_id;

		m_debug_ui.terminate();

		// Shutdown bgfx
		ddShutdown();
		LOG_INFO("[BgfxGraphicsSystem] debugdraw shutdown complete");

		bgfx::shutdown();
		LOG_INFO("[BgfxGraphicsSystem] bgfx shutdown complete");
	}

	void BgfxGraphicsSystem::on_update_graphics(long long dt)
	{
		const auto frameStart = std::chrono::steady_clock::now();
		// Clear every frame
		bgfx::setViewClear(
			m_main_view_id,
			BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH,
			0x303030ff,
			1.0f,
			0
		);
		bgfx::setViewRect(m_main_view_id, 0, 0, uint16_t(m_width), uint16_t(m_height));

		// With post-processing the 3D views draw into the post target; otherwise straight onto the screen (or the frame
		// target, when the picture is scaled up)
		apply_render_scale();
		bgfx::setViewRect(m_main_view_id, 0, 0, uint16_t(m_width), uint16_t(m_height));
		m_upscale_active = prepare_upscale();
		m_post_active = (m_post.enabled || m_fade[3] > 0.001f || m_sketch > 0.0f) && m_active_camera && prepare_post_target();
		bgfx::FrameBufferHandle target = get_screen_target();
		if (m_post_active) target = m_post_target;
		bgfx::setViewFrameBuffer(m_main_view_id, target);
		bgfx::setViewFrameBuffer(m_transparent_view_id, target);

		// Solid 3D materials nearest first (MeshComponent3D_Bgfx::set_sort_view): what is hidden fails the depth test before
		// it is shaded. PAKAL_DEPTH_SORT=0 sorts them by program instead (fewer state changes, every pixel shaded).
		static const bool depthSort = !std::getenv("PAKAL_DEPTH_SORT") || std::string(std::getenv("PAKAL_DEPTH_SORT")) != "0";
		bgfx::setViewMode(m_main_view_id, depthSort ? bgfx::ViewMode::DepthAscending : bgfx::ViewMode::Default);

		// Blended 3D materials: same target and camera, no clear, drawn in submission order
		bgfx::setViewClear(m_transparent_view_id, BGFX_CLEAR_NONE);
		bgfx::setViewMode(m_transparent_view_id, bgfx::ViewMode::Sequential);
		bgfx::setViewRect(m_transparent_view_id, 0, 0, uint16_t(m_width), uint16_t(m_height));

		// Apply camera matrices if we have an active camera
		if (m_active_camera)
		{
			m_active_camera->apply_camera(m_main_view_id);
			m_active_camera->apply_camera(m_transparent_view_id);
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

		const float dt_seconds = static_cast<float>(dt) * 0.001f;

		// The sun's view first (view 3 runs before the rest), so the lit meshes can read its depth
		// The interface goes after the upscale (drawn at the screen's size, not scaled up with the picture)
		const bgfx::ViewId order[] = { m_shadow_view_id, m_main_view_id, m_transparent_view_id, m_post_view_id, m_overlay_view_id,
			m_easu_view_id, m_rcas_view_id, m_ui_view_id, m_debug_ui_view_id };
		bgfx::setViewOrder(0, BX_COUNTOF(order), order);
		prepare_shadow();
		const float giOrigin[4] = { m_gi_origin[0], m_gi_origin[1], m_gi_origin[2], m_gi_enabled ? m_gi_origin[3] : 0.0f };
		MeshComponent3D_Bgfx::set_gi_state(m_gi_texture, giOrigin, m_gi_size);
		MeshComponent3D_Bgfx::set_lightmap_state(m_lightmap_enabled ? m_lightmap_texture : bgfx::TextureHandle(BGFX_INVALID_HANDLE), m_lightmap_shadow);
		MeshComponent3D_Bgfx::set_depth_program(m_shadow_program);

		// Sky first (depth test LEQUAL, drawn at the far plane), then the lit 3D meshes
		render_sky();
		m_frameDrawing = take_frame_drawing();
		// Particles go first in the blended view (it keeps the order): a faded building in front of them is drawn after
		render_particles();
		// the GPU's particles: moved in the sun's view (the first to run), drawn with the blended things
		m_gpuParticles.frame(dt_seconds, m_shadow_view_id, m_transparent_view_id);
		{
			PAKAL_PROFILE_SCOPE("gfx.meshes");   // the models' poses and draws (gfx.pose: the poses alone)
			render_meshes3d(dt_seconds);
		}

		// Update and render all registered sprites
		render_sprites(dt_seconds);

		// Touch view to ensure it's rendered
		bgfx::touch(m_main_view_id);
		bgfx::touch(m_transparent_view_id);

		// Execute debug drawing
		for (auto* debugDrawer : m_debug_renderers)
		{
			if (debugDrawer)
			{
				debugDrawer->do_debug_draw();
			}
		}

		const std::vector<Line3D>& lines = m_frameDrawing.lines;
		const std::vector<Triangle3D>& triangles = m_frameDrawing.triangles;
		if (!triangles.empty())
		{
			DebugDrawEncoder encoder;
			encoder.begin(m_transparent_view_id);
			encoder.setWireframe(false);
			for (const auto& t : triangles)
			{
				encoder.setState(t.depthTest, false, true);
				encoder.setColor(t.color);
				// both windings: the shape is seen whichever way it faces the camera
				const DdVertex both[6] = { { t.a.x, t.a.y, t.a.z }, { t.b.x, t.b.y, t.b.z }, { t.c.x, t.c.y, t.c.z },
					{ t.a.x, t.a.y, t.a.z }, { t.c.x, t.c.y, t.c.z }, { t.b.x, t.b.y, t.b.z } };
				encoder.drawTriList(6, both);
			}
			encoder.end();
		}
		if (!lines.empty())
		{
			DebugDrawEncoder encoder;
			encoder.begin(m_transparent_view_id);
			for (const auto& line : lines)
			{
				encoder.setState(line.depthTest, false, true);
				encoder.setColor(line.color);
				encoder.moveTo(line.from.x, line.from.y, line.from.z);
				encoder.lineTo(line.to.x, line.to.y, line.to.z);
			}
			encoder.end();
		}

		if (m_post_active)
		{
			render_post();
		}

		// Lines in pixels over the finished picture (an orthographic view of the whole screen)
		const std::vector<ScreenLine>& screenLines = m_frameDrawing.screenLines;
		if (!screenLines.empty())
		{
			float identity[16], ortho[16];
			bx::mtxIdentity(identity);
			bx::mtxOrtho(ortho, 0.0f, float(m_width), float(m_height), 0.0f, 0.0f, 10.0f, 0.0f, bgfx::getCaps()->homogeneousDepth);
			bgfx::setViewFrameBuffer(m_overlay_view_id, get_screen_target());
			bgfx::setViewRect(m_overlay_view_id, 0, 0, uint16_t(m_width), uint16_t(m_height));
			bgfx::setViewTransform(m_overlay_view_id, identity, ortho);
			DebugDrawEncoder encoder;
			encoder.begin(m_overlay_view_id);
			encoder.setState(false, false, true);
			for (const auto& line : screenLines)
			{
				encoder.setColor(line.color);
				encoder.moveTo(line.x0, line.y0, 1.0f);
				encoder.lineTo(line.x1, line.y1, 1.0f);
			}
			encoder.end();
		}

		// The game's interface, then the development panels
		if (m_ui_manager)
		{
			PAKAL_PROFILE_SCOPE("gfx.ui");
			m_ui_manager->draw_ui();
		}
		m_debug_ui.render(get_output_width(), get_output_height(), BGFX_INVALID_HANDLE);
		if (m_upscale_active) render_upscale();

		// Advance to next frame
		{
			PAKAL_PROFILE_SCOPE("gfx.submit");   // bgfx hands the frame to the driver (and waits for the one before)
			bgfx::frame();
		}
		const double frameMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - frameStart).count();
		governor_frame(frameMs);
		log_frame_stats();
		// telemetry (PAKAL_PROFILE): the frame, and what the GPU holds
		Profiler& profiler = Profiler::get();
		if (profiler.enabled())
		{
			static auto previous = std::chrono::steady_clock::now();
			const auto now = std::chrono::steady_clock::now();
			profiler.frame(std::chrono::duration<double, std::milli>(now - previous).count());   // from one frame to the next
			previous = now;
			profiler.add("gfx.frame", frameMs);   // the graphics thread's own work
			const bgfx::Stats* stats = bgfx::getStats();
			profiler.counter("gpu.textures_mb", stats->textureMemoryUsed / (1024.0 * 1024.0));
			profiler.counter("gpu.targets_mb", stats->rtMemoryUsed / (1024.0 * 1024.0));
			profiler.counter("gpu.draws", stats->numDraw);
			profiler.counter("render.width", m_width);
			profiler.counter("render.height", m_height);
		}
	}

	// One line per second with what the last frame cost (PAKAL_FPS_LOG=1): draws and triangles, CPU and GPU time,
	// and the GPU time of each view
	void BgfxGraphicsSystem::log_frame_stats()
	{
		static const bool enabled = std::getenv("PAKAL_FPS_LOG") != nullptr;
		static auto last = std::chrono::steady_clock::now();
		MeshComponent3D_Bgfx::PassStats passes[3];
		MeshComponent3D_Bgfx::take_pass_stats(passes); // per frame: what is logged is the last frame
		// Hitches: a frame much longer than the ones before it is logged at once, with what it drew
		static auto previous = std::chrono::steady_clock::now();
		static double average = 16.0;
		static int frames = 0;          // frames since the last line, and the longest of them
		static double worstMs = 0.0;
		if (enabled)
		{
			const auto now = std::chrono::steady_clock::now();
			const double frameMs = std::chrono::duration<double, std::milli>(now - previous).count();
			previous = now;
			++frames;
			worstMs = std::max(worstMs, frameMs);
			if (frameMs > 50.0 && frameMs > 2.5 * average)
			{
				LOG_INFO("[Hitch] %.0f ms (usual %.0f): shadow %u models, opaque %u, transparent %u", frameMs, average,
					passes[0].models, passes[1].models, passes[2].models);
			}
			average += (std::min(frameMs, 200.0) - average) * 0.05;
		}
		if (!enabled || std::chrono::steady_clock::now() - last < std::chrono::seconds(1))
		{
			return;
		}
		const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - last).count();
		LOG_INFO("[FPS] %.1f (worst frame %.0f ms) at %ux%u", frames / seconds, worstMs, m_width, m_height);
		frames = 0;
		worstMs = 0.0;
		last = std::chrono::steady_clock::now();
		const bgfx::Stats* stats = bgfx::getStats();
		const double cpuMs = stats->cpuTimerFreq ? 1000.0 * double(stats->cpuTimeFrame) / double(stats->cpuTimerFreq) : 0.0;
		const double gpuMs = stats->gpuTimerFreq ? 1000.0 * double(stats->gpuTimeEnd - stats->gpuTimeBegin) / double(stats->gpuTimerFreq) : 0.0;
		std::string views;
		for (uint16_t i = 0; i < stats->numViews; ++i)
		{
			const bgfx::ViewStats& view = stats->viewStats[i];
			const double ms = stats->gpuTimerFreq ? 1000.0 * double(view.gpuTimeEnd - view.gpuTimeBegin) / double(stats->gpuTimerFreq) : 0.0;
			char part[48];
			std::snprintf(part, sizeof(part), " v%u=%.2f", unsigned(view.view), ms);
			views += part;
		}
		LOG_INFO("[GPU] models/tris shadow=%u/%u opaque=%u/%u transparent=%u/%u", passes[0].models, passes[0].triangles,
			passes[1].models, passes[1].triangles, passes[2].models, passes[2].triangles);
		LOG_INFO("[GPU] draws=%u tris=%u cpu=%.2fms gpu=%.2fms submit=%.2fms wait=%.2fms views:%s", stats->numDraw,
			stats->numPrims[bgfx::Topology::TriList], cpuMs, gpuMs,
			stats->cpuTimerFreq ? 1000.0 * double(stats->cpuTimeEnd - stats->cpuTimeBegin) / double(stats->cpuTimerFreq) : 0.0,
			stats->cpuTimerFreq ? 1000.0 * double(stats->waitRender) / double(stats->cpuTimerFreq) : 0.0, views.c_str());
	}

	void BgfxGraphicsSystem::set_native_window(void* window, unsigned width, unsigned height, unsigned outputWidth, unsigned outputHeight)
	{
		execute_block([this, window, width, height, outputWidth, outputHeight]()
		{
			bgfx::PlatformData data;
			data.nwh = window;
			bgfx::setPlatformData(data); // marks the context to be made again on the next reset
			m_width = width;
			m_height = height;
			m_output_width = outputWidth >= width && outputHeight >= height ? outputWidth : 0;
			m_output_height = outputWidth >= width && outputHeight >= height ? outputHeight : 0;
			reset_backbuffer();
			bgfx::dbgTextClear(); // text written for the old size would stay where the new layout does not write
			bgfx::setViewRect(m_main_view_id, 0, 0, uint16_t(m_width), uint16_t(m_height));
			LOG_INFO("[BgfxGraphicsSystem] New window %p, %ux%u", window, width, height);
		});
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
		factories.emplace_back(CreateComponentFactory<SpriteComponent2D, SpriteComponent_Bgfx>(this));
		factories.emplace_back(CreateComponentFactory<CameraComponent2D, CameraComponent_Bgfx>(this));
		factories.emplace_back(CreateComponentFactory<CameraComponent3D, CameraComponent3D_Bgfx>(this));
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

	void BgfxGraphicsSystem::register_mesh3d(MeshComponent3D_Bgfx* mesh)
	{
		if (mesh)
		{
			m_meshes3d.push_back(mesh);   // drawn one by one until the next regrouping takes it
			m_loose.push_back(mesh);      // ... and walked every frame until the grid takes it
		}
	}

	void BgfxGraphicsSystem::unregister_mesh3d(MeshComponent3D_Bgfx* mesh)
	{
		if (mesh)
		{
			auto it = std::find(m_meshes3d.begin(), m_meshes3d.end(), mesh);
			if (it != m_meshes3d.end())
			{
				m_meshes3d.erase(it);
				// a copy in an instance group: the groups would keep a pointer to it (characters and pickups come and go
				// all the time: those are in none, and the groups stay)
				if (mesh->grouped()) ++m_meshesVersion;
				// in the grid: it is made again before the next frame walks it; loose: out of the list
				auto loose = std::find(m_loose.begin(), m_loose.end(), mesh);
				if (loose != m_loose.end()) m_loose.erase(loose);
				else m_gridStale = true;
			}
		}
	}

	void BgfxGraphicsSystem::set_directional_light(float dirX, float dirY, float dirZ, float r, float g, float b)
	{
		m_light_dir[0] = dirX;
		m_light_dir[1] = dirY;
		m_light_dir[2] = dirZ;
		m_light_color[0] = r;
		m_light_color[1] = g;
		m_light_color[2] = b;
	}

	void BgfxGraphicsSystem::set_ambient_light(float r, float g, float b)
	{
		m_ambient[0] = r;
		m_ambient[1] = g;
		m_ambient[2] = b;
	}

	void BgfxGraphicsSystem::set_ground_ambient_light(float r, float g, float b)
	{
		m_ambient_ground[0] = r;
		m_ambient_ground[1] = g;
		m_ambient_ground[2] = b;
	}

	void BgfxGraphicsSystem::set_sky_colors(float topR, float topG, float topB, float horizonR, float horizonG, float horizonB)
	{
		m_sky_top[0] = topR;
		m_sky_top[1] = topG;
		m_sky_top[2] = topB;
		m_sky_horizon[0] = horizonR;
		m_sky_horizon[1] = horizonG;
		m_sky_horizon[2] = horizonB;
	}

	void BgfxGraphicsSystem::draw_text(int column, int row, const char* text)
	{
		// Text grid cells (8x16 pixels on screen); drawn each frame, like any debug text
		bgfx::dbgTextPrintf(static_cast<uint16_t>(column), static_cast<uint16_t>(row), 0x0f, "%s", text);
	}

	void BgfxGraphicsSystem::draw_screen_line(float x0, float y0, float x1, float y1, uint32_t color)
	{
		std::lock_guard<std::mutex> lock(m_lineMutex);
		m_pendingScreenLines.push_back({ x0, y0, x1, y1, color });
	}

	void BgfxGraphicsSystem::draw_line3d(const tmath::vector3df& from, const tmath::vector3df& to, uint32_t color, bool depthTest)
	{
		std::lock_guard<std::mutex> lock(m_lineMutex);
		m_pendingLines.push_back({ from, to, color, depthTest });
	}

	void BgfxGraphicsSystem::draw_triangle3d(const tmath::vector3df& a, const tmath::vector3df& b, const tmath::vector3df& c, uint32_t color,
		bool depthTest)
	{
		std::lock_guard<std::mutex> lock(m_lineMutex);
		m_pendingTriangles.push_back({ a, b, c, color, depthTest });
	}

	void BgfxGraphicsSystem::draw_particle(const tmath::vector3df& at, float radius, uint32_t color, bool additive)
	{
		std::lock_guard<std::mutex> lock(m_lineMutex);
		m_pendingParticles.push_back({ at, radius, color, additive });
	}

	void BgfxGraphicsSystem::draw_ground_spot(const tmath::vector3df& at, float radius, uint32_t color, bool additive)
	{
		std::lock_guard<std::mutex> lock(m_lineMutex);
		m_pendingParticles.push_back({ at, radius, color, additive, true });
	}

	void BgfxGraphicsSystem::draw_sprite_particle(const tmath::vector3df& at, float radius, uint32_t color, bool additive, uint16_t sprite, float angle)
	{
		std::lock_guard<std::mutex> lock(m_lineMutex);
		m_pendingParticles.push_back({ at, radius, color, additive, false, sprite, angle });
	}

	void BgfxGraphicsSystem::draw_sprite_spot(const tmath::vector3df& at, float radius, uint32_t color, bool additive, uint16_t sprite, float angle)
	{
		std::lock_guard<std::mutex> lock(m_lineMutex);
		m_pendingParticles.push_back({ at, radius, color, additive, true, sprite, angle });
	}

	void BgfxGraphicsSystem::set_particle_sprites(const std::string& texture, int columns, int rows)
	{
		MeshComponent3D_Bgfx::release_level_texture(m_particle_sheet);   // (the textures' shared cache counts its users: never destroyed here)
		m_particle_sheet = BGFX_INVALID_HANDLE;
		if (texture.empty() || columns < 1 || rows < 1) return;
		m_particle_sheet = MeshComponent3D_Bgfx::load_level_texture(texture);
		m_particle_sheet_size[0] = float(columns);
		m_particle_sheet_size[1] = float(rows);
		LOG_INFO("[BgfxGraphicsSystem] particle sprites: %s (%dx%d)%s", texture.c_str(), columns, rows,
			bgfx::isValid(m_particle_sheet) ? "" : " could not be loaded");
	}

	void BgfxGraphicsSystem::commit_frame_drawing()
	{
		std::lock_guard<std::mutex> lock(m_lineMutex);
		m_commits = true;
		m_lastCommit = std::chrono::steady_clock::now();
		m_committed.lines.swap(m_pendingLines);
		m_committed.triangles.swap(m_pendingTriangles);
		m_committed.screenLines.swap(m_pendingScreenLines);
		m_committed.particles.swap(m_pendingParticles);
		m_pendingLines.clear();
		m_pendingTriangles.clear();
		m_pendingScreenLines.clear();
		m_pendingParticles.clear();
	}

	BgfxGraphicsSystem::Committed BgfxGraphicsSystem::take_frame_drawing()
	{
		std::lock_guard<std::mutex> lock(m_lineMutex);
		// a copy: drawn again if the game has not committed another; a game that stopped committing (it left the level)
		// gets the queue taken again
		if (m_commits && std::chrono::steady_clock::now() - m_lastCommit < std::chrono::milliseconds(250)) return m_committed;
		m_commits = false;
		Committed frame;
		frame.lines.swap(m_pendingLines);
		frame.triangles.swap(m_pendingTriangles);
		frame.screenLines.swap(m_pendingScreenLines);
		frame.particles.swap(m_pendingParticles);
		return frame;
	}

	// The frame's particles as camera-facing quads (vs_particle widens each one in view space): the blended ones sorted
	// farthest first (the flat spots on the floor before them, in their order), then the additive ones (light added in proportion to their alpha). They test depth but do not write it.
	void BgfxGraphicsSystem::render_particles()
	{
		std::vector<Particle>& particles = m_frameDrawing.particles;
		const float* view = nullptr;
		const float* proj = nullptr;
		if (particles.empty() || !bgfx::isValid(m_particle_program) || !m_active_camera || !m_active_camera->get_view_projection(view, proj))
		{
			return;
		}
		auto depth = [view](const Particle& p) { return view[2] * p.at.x + view[6] * p.at.y + view[10] * p.at.z + view[14]; };
		std::stable_sort(particles.begin(), particles.end(), [&depth](const Particle& a, const Particle& b)
		{
			if (a.additive != b.additive) return !a.additive; // blended first
			// the spots on the floor under the rest, in the order they came (the newest on top): by depth two that overlap
			// would swap as the camera moves, and flicker
			if (a.flat != b.flat) return a.flat;
			if (a.flat) return false;
			return depth(a) > depth(b);
		});

		struct Vertex { float x, y, z, u, v, size, pad, sprite, angle, unused0, unused1; uint32_t color; };
		static bgfx::VertexLayout layout;
		if (layout.getStride() == 0)
		{
			layout.begin()
				.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord0, 4, bgfx::AttribType::Float)
				.add(bgfx::Attrib::TexCoord1, 4, bgfx::AttribType::Float)
				.add(bgfx::Attrib::Color0, 4, bgfx::AttribType::Uint8, true)
				.end();
		}
		const bool sheet = bgfx::isValid(m_particle_sheet);
		const uint32_t blendedCount = static_cast<uint32_t>(std::count_if(particles.begin(), particles.end(), [](const Particle& p) { return !p.additive; }));
		auto submit = [&](uint32_t first, uint32_t count, uint64_t blend)
		{
			if (count == 0) return;
			count = std::min<uint32_t>(count, 65536 / 4);
			if (bgfx::getAvailTransientVertexBuffer(count * 4, layout) < count * 4 || bgfx::getAvailTransientIndexBuffer(count * 6) < count * 6)
			{
				return;
			}
			bgfx::TransientVertexBuffer tvb;
			bgfx::TransientIndexBuffer tib;
			bgfx::allocTransientVertexBuffer(&tvb, count * 4, layout);
			bgfx::allocTransientIndexBuffer(&tib, count * 6);
			Vertex* vertex = reinterpret_cast<Vertex*>(tvb.data);
			uint16_t* index = reinterpret_cast<uint16_t*>(tib.data);
			static const float corners[4][2] = { { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f } };
			for (uint32_t i = 0; i < count; ++i)
			{
				const Particle& p = particles[first + i];
				for (int c = 0; c < 4; ++c)
				{
					vertex[i * 4 + c] = { p.at.x, p.at.y, p.at.z, corners[c][0], corners[c][1], p.radius, p.flat ? 1.0f : 0.0f,
						sheet ? float(p.sprite) : 0.0f, p.angle, 0.0f, 0.0f, p.color };
				}
				const uint16_t base = static_cast<uint16_t>(i * 4);
				const uint16_t quad[6] = { base, uint16_t(base + 1), uint16_t(base + 2), base, uint16_t(base + 2), uint16_t(base + 3) };
				std::copy(quad, quad + 6, index + i * 6);
			}
			bgfx::setVertexBuffer(0, &tvb);
			bgfx::setIndexBuffer(&tib);
			const float sheetSize[4] = { m_particle_sheet_size[0], m_particle_sheet_size[1], 0.0f, 0.0f };
			bgfx::setUniform(m_u_particle_sheet, sheetSize);
			if (sheet) bgfx::setTexture(0, m_s_particle, m_particle_sheet);
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_DEPTH_TEST_LESS | BGFX_STATE_MSAA | blend);
			bgfx::submit(m_transparent_view_id, m_particle_program);
		};
		submit(0, blendedCount, BGFX_STATE_BLEND_ALPHA);
		submit(blendedCount, static_cast<uint32_t>(particles.size()) - blendedCount, BGFX_STATE_BLEND_FUNC(BGFX_STATE_BLEND_SRC_ALPHA, BGFX_STATE_BLEND_ONE));
	}

	void BgfxGraphicsSystem::request_screenshot(const char* filePath)
	{
		bgfx::requestScreenShot(BGFX_INVALID_HANDLE, filePath);
	}

	void BgfxGraphicsSystem::set_active_camera(ICameraComponent_Bgfx* camera)
	{
		m_active_camera = camera;
	}

	void BgfxGraphicsSystem::clear_active_camera(ICameraComponent_Bgfx* camera)
	{
		if (m_active_camera == camera)
		{
			m_active_camera = nullptr;
		}
	}

	void BgfxGraphicsSystem::render_sprites(float dt_seconds)
	{
		if (m_sprites.empty())
		{
			return;
		}

		for (auto* sprite : m_sprites)
		{
			if (sprite)
			{
				sprite->update(dt_seconds);
				sprite->render(m_main_view_id, nullptr);
			}
		}
	}

	bgfx::ProgramHandle BgfxGraphicsSystem::load_compute(const char* csName)
	{
		const char* renderer_dir = get_shader_renderer_dir(bgfx::getRendererType());
		for (const char* root : { "Assets/shaders", "shaders", "bin/Assets/shaders", "bin/shaders", "../bin/Assets/shaders", "../bin/shaders" })
		{
			const std::string path = std::string(root) + "/" + renderer_dir + "/" + csName + ".bin";
			const bgfx::Memory* memory = load_shader_memory(path.c_str());
			if (!memory) continue;
			bgfx::ShaderHandle shader = bgfx::createShader(memory);
			if (!bgfx::isValid(shader)) continue;
			bgfx::setName(shader, csName);
			LOG_INFO("[BgfxGraphicsSystem] Compute program %s loaded (%s)", csName, renderer_dir);
			return bgfx::createProgram(shader, true);
		}
		LOG_WARNING("[BgfxGraphicsSystem] Compute program %s not found for %s", csName, renderer_dir);
		return BGFX_INVALID_HANDLE;
	}

	bgfx::ProgramHandle BgfxGraphicsSystem::load_program(const char* vsName, const char* fsName)
	{
		const char* renderer_dir = get_shader_renderer_dir(bgfx::getRendererType());
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
			std::string vs_path = std::string(root) + "/" + renderer_dir + "/" + vsName + ".bin";
			std::string fs_path = std::string(root) + "/" + renderer_dir + "/" + fsName + ".bin";

			const bgfx::Memory* vs_mem = load_shader_memory(vs_path.c_str());
			const bgfx::Memory* fs_mem = load_shader_memory(fs_path.c_str());
			if (!vs_mem || !fs_mem)
			{
				continue;
			}

			bgfx::ShaderHandle vsh = bgfx::createShader(vs_mem);
			bgfx::ShaderHandle fsh = bgfx::createShader(fs_mem);
			if (bgfx::isValid(vsh) && bgfx::isValid(fsh))
			{
				bgfx::setName(vsh, vsName);
				bgfx::setName(fsh, fsName);
				LOG_INFO("[BgfxGraphicsSystem] Shader program %s/%s loaded (%s)", vsName, fsName, renderer_dir);
				// destroyShaders = true: the shader handles go away with the program
				return bgfx::createProgram(vsh, fsh, true);
			}

			if (bgfx::isValid(vsh)) bgfx::destroy(vsh);
			if (bgfx::isValid(fsh)) bgfx::destroy(fsh);
		}

		LOG_WARNING("[BgfxGraphicsSystem] Shader program %s/%s not found for %s", vsName, fsName, renderer_dir);
		return BGFX_INVALID_HANDLE;
	}

	void BgfxGraphicsSystem::create_mesh_resources()
	{
		m_mesh_program = load_program("vs_mesh", "fs_mesh");
		m_skin_program = load_program("vs_skin", "fs_mesh");
		m_pbr_program = load_program("vs_pbr", "fs_pbr");
		m_pbr_inst_program = load_program("vs_pbr_inst", "fs_pbr");
		m_mesh_inst_program = load_program("vs_mesh_inst", "fs_mesh");
		static const bool gpuParticlesWanted = !(std::getenv("PAKAL_GPU_PARTICLES") && std::string(std::getenv("PAKAL_GPU_PARTICLES")) == "0");
		if (gpuParticlesWanted && (bgfx::getCaps()->supported & BGFX_CAPS_COMPUTE))
		{
			m_gpuParticles.create([this]() { return load_compute("cs_particles"); }, [this]() { return load_program("vs_gpu_particle", "fs_particle"); });
			LOG_INFO("[BgfxGraphicsSystem] GPU particles %s", m_gpuParticles.available() ? "on" : "off (the CPU's)");
		}
		m_sky_program = load_program("vs_sky", "fs_sky");
		// the uniforms before the program (GL finds a program's uniforms when it is linked)
		m_u_particle_sheet = bgfx::createUniform("u_particleSheet", bgfx::UniformType::Vec4);
		m_s_particle = bgfx::createUniform("s_texParticle", bgfx::UniformType::Sampler);
		m_particle_program = load_program("vs_particle", "fs_particle_sprite");

		m_u_light_dir = bgfx::createUniform("u_lightDir", bgfx::UniformType::Vec4);
		m_u_light_color = bgfx::createUniform("u_lightColor", bgfx::UniformType::Vec4);
		m_u_ambient = bgfx::createUniform("u_ambient", bgfx::UniformType::Vec4);
		m_u_ambient_ground = bgfx::createUniform("u_ambientGround", bgfx::UniformType::Vec4);
		m_u_sky_top = bgfx::createUniform("u_skyTop", bgfx::UniformType::Vec4);
		m_u_sky_horizon = bgfx::createUniform("u_skyHorizon", bgfx::UniformType::Vec4);
		m_u_sky_sun = bgfx::createUniform("u_skySun", bgfx::UniformType::Vec4);
		m_u_sky_sun_color = bgfx::createUniform("u_skySunColor", bgfx::UniformType::Vec4);

		// Unit cube used as sky dome; the sky shader ignores the camera position.
		static const float skyVertices[] = {
			-1.0f, -1.0f, -1.0f,   1.0f, -1.0f, -1.0f,   1.0f,  1.0f, -1.0f,  -1.0f,  1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,   1.0f, -1.0f,  1.0f,   1.0f,  1.0f,  1.0f,  -1.0f,  1.0f,  1.0f,
		};
		static const uint16_t skyIndices[] = {
			0, 2, 1,  0, 3, 2,
			4, 5, 6,  4, 6, 7,
			0, 1, 5,  0, 5, 4,
			2, 3, 7,  2, 7, 6,
			1, 2, 6,  1, 6, 5,
			0, 4, 7,  0, 7, 3,
		};

		bgfx::VertexLayout skyLayout;
		skyLayout.begin()
			.add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
			.end();
		m_sky_vb = bgfx::createVertexBuffer(bgfx::copy(skyVertices, sizeof(skyVertices)), skyLayout);
		m_sky_ib = bgfx::createIndexBuffer(bgfx::copy(skyIndices, sizeof(skyIndices)));
	}

	void BgfxGraphicsSystem::destroy_mesh_resources()
	{
		auto destroyProgram = [](bgfx::ProgramHandle& h) { if (bgfx::isValid(h)) bgfx::destroy(h); h = BGFX_INVALID_HANDLE; };
		auto destroyUniform = [](bgfx::UniformHandle& h) { if (bgfx::isValid(h)) bgfx::destroy(h); h = BGFX_INVALID_HANDLE; };

		m_gpuParticles.destroy();
		destroyProgram(m_mesh_program);
		destroyProgram(m_skin_program);
		destroyProgram(m_pbr_program);
		destroyProgram(m_pbr_inst_program);
		destroyProgram(m_mesh_inst_program);
		destroyProgram(m_sky_program);
		destroyProgram(m_particle_program);
		destroyUniform(m_u_particle_sheet);
		destroyUniform(m_s_particle);
		MeshComponent3D_Bgfx::release_level_texture(m_particle_sheet);   // (the textures' shared cache counts its users: never destroyed here)
		m_particle_sheet = BGFX_INVALID_HANDLE;
		destroyUniform(m_u_light_dir);
		destroyUniform(m_u_light_color);
		destroyUniform(m_u_ambient);
		destroyUniform(m_u_ambient_ground);
		destroyUniform(m_u_sky_top);
		destroyUniform(m_u_sky_horizon);
		destroyUniform(m_u_sky_sun);
		destroyUniform(m_u_sky_sun_color);

		if (bgfx::isValid(m_sky_vb)) bgfx::destroy(m_sky_vb);
		if (bgfx::isValid(m_sky_ib)) bgfx::destroy(m_sky_ib);
		m_sky_vb = BGFX_INVALID_HANDLE;
		m_sky_ib = BGFX_INVALID_HANDLE;
	}

	void BgfxGraphicsSystem::create_post_resources()
	{
		m_post_program = load_program("vs_post", "fs_post");
		for (int i = 0; i < 11; ++i)
		{
			const std::string name = "u_post" + std::to_string(i);
			m_u_post[i] = bgfx::createUniform(name.c_str(), bgfx::UniformType::Vec4);
		}
		m_s_post_color = bgfx::createUniform("s_postColor", bgfx::UniformType::Sampler);
		m_s_post_depth = bgfx::createUniform("s_postDepth", bgfx::UniformType::Sampler);
		m_post_layout.begin().add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float).end();
	}

	void BgfxGraphicsSystem::destroy_post_resources()
	{
		if (bgfx::isValid(m_post_target)) bgfx::destroy(m_post_target); // its textures go with it
		m_post_target = BGFX_INVALID_HANDLE;
		m_post_color = m_post_depth = BGFX_INVALID_HANDLE;
		if (bgfx::isValid(m_post_program)) bgfx::destroy(m_post_program);
		m_post_program = BGFX_INVALID_HANDLE;
		for (auto& uniform : m_u_post)
		{
			if (bgfx::isValid(uniform)) bgfx::destroy(uniform);
			uniform = BGFX_INVALID_HANDLE;
		}
		if (bgfx::isValid(m_s_post_color)) bgfx::destroy(m_s_post_color);
		if (bgfx::isValid(m_s_post_depth)) bgfx::destroy(m_s_post_depth);
		m_s_post_color = m_s_post_depth = BGFX_INVALID_HANDLE;
		m_post_width = m_post_height = 0;
	}

	bool BgfxGraphicsSystem::prepare_post_target()
	{
		if (!bgfx::isValid(m_post_program))
		{
			return false;
		}
		if (bgfx::isValid(m_post_target) && m_post_width == m_width && m_post_height == m_height)
		{
			return true;
		}
		if (bgfx::isValid(m_post_target))
		{
			bgfx::destroy(m_post_target);
			m_post_target = BGFX_INVALID_HANDLE;
		}

		// A depth format the pass can read (the sampler is a point sampler: depth is not filtered)
		const uint64_t sampling = BGFX_TEXTURE_RT | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT | BGFX_SAMPLER_MIP_POINT
			| BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
		const bgfx::TextureFormat::Enum depthFormats[] = { bgfx::TextureFormat::D32F, bgfx::TextureFormat::D24S8, bgfx::TextureFormat::D24 };
		bgfx::TextureFormat::Enum depthFormat = bgfx::TextureFormat::Count;
		for (auto format : depthFormats)
		{
			if (bgfx::getCaps()->formats[format] & (BGFX_CAPS_FORMAT_TEXTURE_2D | BGFX_CAPS_FORMAT_TEXTURE_FRAMEBUFFER))
			{
				depthFormat = format;
				break;
			}
		}
		if (depthFormat == bgfx::TextureFormat::Count)
		{
			static bool warned = false;
			if (!warned) { warned = true; LOG_WARNING("[BgfxGraphicsSystem] No readable depth format: post-processing is off"); }
			m_post.enabled = false;
			return false;
		}
		m_post_color = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::BGRA8,
			BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP);
		m_post_depth = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, depthFormat, sampling);
		const bgfx::TextureHandle attachments[] = { m_post_color, m_post_depth };
		m_post_target = bgfx::createFrameBuffer(2, attachments, true);
		m_post_width = m_width;
		m_post_height = m_height;
		LOG_INFO("[BgfxGraphicsSystem] Post-process target %ux%u created", m_width, m_height);
		return bgfx::isValid(m_post_target);
	}

	bool BgfxGraphicsSystem::prepare_upscale()
	{
		if (!is_upscaling() || !bgfx::isValid(m_easu_program) || !bgfx::isValid(m_rcas_program))
		{
			return false;
		}
		const uint32_t outW = get_output_width(), outH = get_output_height();
		if (bgfx::isValid(m_frame_target) && m_upscale_width == m_width && m_upscale_height == m_height
			&& m_upscale_out_width == outW && m_upscale_out_height == outH)
		{
			return true;
		}
		if (bgfx::isValid(m_frame_target)) bgfx::destroy(m_frame_target);
		if (bgfx::isValid(m_easu_target)) bgfx::destroy(m_easu_target);
		const uint64_t sampled = BGFX_TEXTURE_RT | BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
		// The frame: color, and a depth buffer for the 3D views when there is no post-process
		m_frame_color = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1, bgfx::TextureFormat::BGRA8, sampled);
		const bgfx::TextureHandle depth = bgfx::createTexture2D(uint16_t(m_width), uint16_t(m_height), false, 1,
			bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT_WRITE_ONLY);
		const bgfx::TextureHandle frame[] = { m_frame_color, depth };
		m_frame_target = bgfx::createFrameBuffer(2, frame, true);
		// EASU's result, read by RCAS a pixel at a time
		m_easu_color = bgfx::createTexture2D(uint16_t(outW), uint16_t(outH), false, 1, bgfx::TextureFormat::BGRA8,
			sampled | BGFX_SAMPLER_MIN_POINT | BGFX_SAMPLER_MAG_POINT);
		m_easu_target = bgfx::createFrameBuffer(1, &m_easu_color, true);
		m_upscale_width = m_width;
		m_upscale_height = m_height;
		m_upscale_out_width = outW;
		m_upscale_out_height = outH;
		LOG_INFO("[BgfxGraphicsSystem] Upscale targets: %ux%u -> %ux%u", m_width, m_height, outW, outH);
		return bgfx::isValid(m_frame_target) && bgfx::isValid(m_easu_target);
	}

	void BgfxGraphicsSystem::destroy_upscale_resources()
	{
		if (bgfx::isValid(m_frame_target)) bgfx::destroy(m_frame_target);
		if (bgfx::isValid(m_easu_target)) bgfx::destroy(m_easu_target);
		m_frame_target = m_easu_target = BGFX_INVALID_HANDLE;
		m_frame_color = m_easu_color = BGFX_INVALID_HANDLE;
		m_upscale_width = m_upscale_height = m_upscale_out_width = m_upscale_out_height = 0;
		if (bgfx::isValid(m_easu_program)) bgfx::destroy(m_easu_program);
		if (bgfx::isValid(m_rcas_program)) bgfx::destroy(m_rcas_program);
		m_easu_program = m_rcas_program = BGFX_INVALID_HANDLE;
		for (auto& uniform : m_u_fsr)
		{
			if (bgfx::isValid(uniform)) bgfx::destroy(uniform);
			uniform = BGFX_INVALID_HANDLE;
		}
		if (bgfx::isValid(m_s_fsr_input)) bgfx::destroy(m_s_fsr_input);
		m_s_fsr_input = BGFX_INVALID_HANDLE;
	}

	// The finished frame (picture and interface) onto the back buffer at the screen's size: FSR 1 rebuilds the edges (EASU)
	// and sharpens (RCAS); the bilinear mode stretches it and leaves it as it is
	void BgfxGraphicsSystem::render_upscale()
	{
		const float inW = float(m_width), inH = float(m_height);
		const float outW = float(get_output_width()), outH = float(get_output_height());
		const bool fsr = m_upscaler == Upscaler::Fsr;
		const float con0[4] = { inW / outW, inH / outH, 0.5f * inW / outW - 0.5f, 0.5f * inH / outH - 0.5f };
		const float con1[4] = { 1.0f / inW, 1.0f / inH, 1.0f / inW, -1.0f / inH };
		const float con2[4] = { -1.0f / inW, 2.0f / inH, 1.0f / inW, 2.0f / inH };
		const float con3[4] = { 0.0f, 4.0f / inH, 0.0f, fsr ? 0.0f : 1.0f };
		const float rcas[4] = { 1.0f / outW, 1.0f / outH, fsr ? std::exp2(-m_sharpness) : 0.0f, 0.0f };
		auto pass = [&](bgfx::ViewId view, bgfx::FrameBufferHandle target, bgfx::ProgramHandle program, bgfx::TextureHandle input)
		{
			bgfx::setViewName(view, view == m_easu_view_id ? "EASU" : "RCAS");
			bgfx::setViewFrameBuffer(view, target);
			bgfx::setViewClear(view, BGFX_CLEAR_NONE);
			bgfx::setViewRect(view, 0, 0, uint16_t(outW), uint16_t(outH));
			bgfx::TransientVertexBuffer triangle;
			if (bgfx::getAvailTransientVertexBuffer(3, m_post_layout) < 3) return;
			bgfx::allocTransientVertexBuffer(&triangle, 3, m_post_layout);
			const float corners[9] = { -1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f, -1.0f, 3.0f, 0.0f };
			std::copy(corners, corners + 9, reinterpret_cast<float*>(triangle.data));
			bgfx::setUniform(m_u_fsr[0], con0);
			bgfx::setUniform(m_u_fsr[1], con1);
			bgfx::setUniform(m_u_fsr[2], con2);
			bgfx::setUniform(m_u_fsr[3], con3);
			bgfx::setUniform(m_u_fsr[4], rcas);
			bgfx::setVertexBuffer(0, &triangle);
			bgfx::setTexture(0, m_s_fsr_input, input);
			bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
			bgfx::submit(view, program);
		};
		// without sharpening (a plain stretch, or sharpness off) the first pass writes the screen: one full-screen pass less
		if (!fsr || m_sharpness < 0.0f)
		{
			pass(m_easu_view_id, BGFX_INVALID_HANDLE, m_easu_program, m_frame_color);
			return;
		}
		pass(m_easu_view_id, m_easu_target, m_easu_program, m_frame_color);
		pass(m_rcas_view_id, BGFX_INVALID_HANDLE, m_rcas_program, m_easu_color);
	}

	void BgfxGraphicsSystem::render_post()
	{
		float scaleX = 1.0f, scaleY = 1.0f, nearPlane = 0.1f, farPlane = 1000.0f;
		if (m_active_camera)
		{
			m_active_camera->get_projection(scaleX, scaleY, nearPlane, farPlane);
		}
		// Without post settings (a fade alone) the grading does nothing
		PostSettings neutral;
		neutral.exposure = neutral.contrast = neutral.saturation = 1.0f;
		neutral.gain[0] = neutral.gain[1] = neutral.gain[2] = 1.0f;
		neutral.vignette = neutral.tiltBlur = neutral.aoStrength = 0.0f;
		const PostSettings& s = m_post.enabled ? m_post : neutral;
		const float p0[4] = { s.exposure, s.contrast, s.saturation, s.vignette };
		const float p1[4] = { s.gain[0], s.gain[1], s.gain[2], s.tiltBlur };
		const float p2[4] = { s.tiltFocus, s.tiltBand, s.aoStrength, s.aoRadius };
		const float p3[4] = { 1.0f / float(m_width), 1.0f / float(m_height), nearPlane, farPlane };
		const float p4[4] = { scaleX, scaleY, s.aoBias, s.aoRange };
		bgfx::setViewFrameBuffer(m_post_view_id, get_screen_target());
		bgfx::setViewClear(m_post_view_id, BGFX_CLEAR_NONE);
		bgfx::setViewRect(m_post_view_id, 0, 0, uint16_t(m_width), uint16_t(m_height));
		// (a touch would discard the uniforms set before it, so the draw state is set after the view is set up)
		bgfx::setUniform(m_u_post[0], p0);
		bgfx::setUniform(m_u_post[1], p1);
		bgfx::setUniform(m_u_post[2], p2);
		bgfx::setUniform(m_u_post[3], p3);
		bgfx::setUniform(m_u_post[4], p4);
		bgfx::setUniform(m_u_post[5], m_fade);
		const float p6[4] = { s.quality, m_sketch, 0.0f, 0.0f };
		bgfx::setUniform(m_u_post[6], p6);
		// the view's translation: where the world's origin is in the view (the sketch's strokes are fixed to the scene)
		float p7[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		const float* view = nullptr;
		const float* proj = nullptr;
		if (m_active_camera && m_active_camera->get_view_projection(view, proj) && view)
		{
			p7[0] = view[12];
			p7[1] = view[13];
			p7[2] = view[14];
		}
		bgfx::setUniform(m_u_post[7], p7);
		const float p8[4] = { s.fogColor[0], s.fogColor[1], s.fogColor[2], m_post.enabled ? s.fogDensity : 0.0f };
		const float p9[4] = { s.fogStart, s.fogMax, s.fogSky, s.fogHorizon };
		bgfx::setUniform(m_u_post[8], p8);
		bgfx::setUniform(m_u_post[9], p9);
		const float p10[4] = { m_sky_horizon[0], m_sky_horizon[1], m_sky_horizon[2], 0.0f };
		bgfx::setUniform(m_u_post[10], p10);

		bgfx::TransientVertexBuffer triangle;
		if (bgfx::getAvailTransientVertexBuffer(3, m_post_layout) < 3)
		{
			return;
		}
		bgfx::allocTransientVertexBuffer(&triangle, 3, m_post_layout);
		float* v = reinterpret_cast<float*>(triangle.data);
		const float corners[9] = { -1.0f, -1.0f, 0.0f, 3.0f, -1.0f, 0.0f, -1.0f, 3.0f, 0.0f };
		std::copy(corners, corners + 9, v);
		bgfx::setVertexBuffer(0, &triangle);
		bgfx::setTexture(0, m_s_post_color, m_post_color);
		bgfx::setTexture(1, m_s_post_depth, m_post_depth);
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A);
		bgfx::submit(m_post_view_id, m_post_program);
	}

	void BgfxGraphicsSystem::set_gi_volume(const uint8_t* rgba, uint16_t nx, uint16_t ny, uint16_t nz, const float origin[3], const float size[3], float strength)
	{
		clear_gi_volume();
		const uint64_t flags = BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP | BGFX_SAMPLER_W_CLAMP;
		m_gi_texture = bgfx::createTexture3D(nx, ny, nz, false, bgfx::TextureFormat::RGBA8, flags,
			bgfx::copy(rgba, uint32_t(nx) * ny * nz * 4));
		if (!bgfx::isValid(m_gi_texture))
		{
			LOG_WARNING("[BgfxGraphicsSystem] The baked ambient light could not be loaded");
			return;
		}
		std::copy(origin, origin + 3, m_gi_origin);
		std::copy(size, size + 3, m_gi_size);
		m_gi_origin[3] = strength;
		m_gi_size[3] = 0.0f;
		LOG_INFO("[BgfxGraphicsSystem] Baked ambient light %ux%ux%u loaded", nx, ny, nz);
	}

	void BgfxGraphicsSystem::clear_gi_volume()
	{
		if (bgfx::isValid(m_gi_texture)) bgfx::destroy(m_gi_texture);
		m_gi_texture = BGFX_INVALID_HANDLE;
		m_gi_origin[3] = 0.0f;
	}

	bool BgfxGraphicsSystem::set_lightmap(const std::string& path, float shadowStrength)
	{
		clear_lightmap();
		m_lightmap_texture = MeshComponent3D_Bgfx::load_level_texture(path);
		if (!bgfx::isValid(m_lightmap_texture))
		{
			LOG_WARNING("[BgfxGraphicsSystem] The lightmap '%s' could not be loaded", path.c_str());
			return false;
		}
		m_lightmap_shadow = shadowStrength;
		apply_quality();
		LOG_INFO("[BgfxGraphicsSystem] Lightmap '%s' loaded", path.c_str());
		return true;
	}

	void BgfxGraphicsSystem::clear_lightmap()
	{
		MeshComponent3D_Bgfx::release_level_texture(m_lightmap_texture);
		m_lightmap_texture = BGFX_INVALID_HANDLE;
		apply_quality();
	}

	void BgfxGraphicsSystem::create_shadow_resources()
	{
		m_shadow_program = load_program("vs_shadow", "fs_shadow");
		m_shadow_skin_program = load_program("vs_shadow_skin", "fs_shadow");
		m_shadow_inst_program = load_program("vs_shadow_inst", "fs_shadow");
	}

	void BgfxGraphicsSystem::destroy_shadow_resources()
	{
		if (bgfx::isValid(m_shadow_target)) bgfx::destroy(m_shadow_target); // its depth texture goes with it
		m_shadow_target = BGFX_INVALID_HANDLE;
		m_shadow_depth = BGFX_INVALID_HANDLE;
		if (bgfx::isValid(m_shadow_program)) bgfx::destroy(m_shadow_program);
		if (bgfx::isValid(m_shadow_skin_program)) bgfx::destroy(m_shadow_skin_program);
		if (bgfx::isValid(m_shadow_inst_program)) bgfx::destroy(m_shadow_inst_program);
		m_shadow_program = m_shadow_skin_program = m_shadow_inst_program = BGFX_INVALID_HANDLE;
		m_shadow_size = 0;
	}

	void BgfxGraphicsSystem::prepare_shadow()
	{
		m_shadow_active = false;
		const float off[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		float identity[16];
		bx::mtxIdentity(identity);
		if (!m_shadows.enabled || !m_active_camera || !bgfx::isValid(m_shadow_program))
		{
			MeshComponent3D_Bgfx::set_shadow_state(BGFX_INVALID_HANDLE, identity, off);
			return;
		}
		float fx = 0.0f, fy = 0.0f, fz = 0.0f;
		if (!m_active_camera->get_focus(fx, fy, fz))
		{
			MeshComponent3D_Bgfx::set_shadow_state(BGFX_INVALID_HANDLE, identity, off);
			return;
		}

		// A depth-only target the size of the map (made again when the size changes)
		if (!bgfx::isValid(m_shadow_target) || m_shadow_size != m_shadows.mapSize)
		{
			if (bgfx::isValid(m_shadow_target)) bgfx::destroy(m_shadow_target);
			// sampled with comparison and filtering (the shaders' shadow2D: the hardware's PCF)
			const uint64_t flags = BGFX_TEXTURE_RT | BGFX_SAMPLER_COMPARE_LEQUAL | BGFX_SAMPLER_MIP_POINT
				| BGFX_SAMPLER_U_CLAMP | BGFX_SAMPLER_V_CLAMP;
			m_shadow_depth = bgfx::createTexture2D(uint16_t(m_shadows.mapSize), uint16_t(m_shadows.mapSize), false, 1, bgfx::TextureFormat::D32F, flags);
			m_shadow_target = bgfx::createFrameBuffer(1, &m_shadow_depth, true);
			m_shadow_size = m_shadows.mapSize;
			if (!bgfx::isValid(m_shadow_target))
			{
				LOG_WARNING("[BgfxGraphicsSystem] The shadow map could not be created: shadows are off");
				m_shadows.enabled = false;
				return;
			}
			LOG_INFO("[BgfxGraphicsSystem] Shadow map %ux%u created", m_shadow_size, m_shadow_size);
		}

		// The sun's view looks along the light, centered on what the camera looks at; the map is shifted in whole texels
		// so shadows do not shimmer when the focus moves
		const float len = std::sqrt(m_light_dir[0] * m_light_dir[0] + m_light_dir[1] * m_light_dir[1] + m_light_dir[2] * m_light_dir[2]);
		const bx::Vec3 dir = len > 0.0001f ? bx::Vec3(m_light_dir[0] / len, m_light_dir[1] / len, m_light_dir[2] / len) : bx::Vec3(0.0f, -1.0f, 0.0f);
		const float reach = 120.0f;
		const bx::Vec3 at(fx, fy, fz);
		const bx::Vec3 eye(fx - dir.x * reach, fy - dir.y * reach, fz - dir.z * reach);
		const bx::Vec3 up = std::fabs(dir.y) > 0.95f ? bx::Vec3(0.0f, 0.0f, 1.0f) : bx::Vec3(0.0f, 1.0f, 0.0f);
		float view[16], proj[16];
		bx::mtxLookAt(view, eye, at, up);
		const bgfx::Caps* caps = bgfx::getCaps();
		float centerX = 0.0f, centerY = 0.0f, half = m_shadows.extent;
		if (m_shadows.fitToCamera)
		{
			fit_shadow_to_camera(view, fy, centerX, centerY, half);
		}
		bx::mtxOrtho(proj, centerX - half, centerX + half, centerY - half, centerY + half, 0.1f, 2.0f * reach, 0.0f, caps->homogeneousDepth);
		{
			// where the focus lands in the map, rounded to a texel
			float viewProj[16];
			bx::mtxMul(viewProj, view, proj);
			const float cx = viewProj[12], cy = viewProj[13];
			const float texel = 2.0f / float(m_shadow_size);
			proj[12] += std::round(cx / texel) * texel - cx;
			proj[13] += std::round(cy / texel) * texel - cy;
		}

		// a shadow goes this far sideways per meter of height: the casters whose shadow cannot reach the picture are left out
		const float sideways = std::sqrt(dir.x * dir.x + dir.z * dir.z);
		MeshComponent3D_Bgfx::set_shadow_reach(std::fabs(dir.y) > 0.05f ? std::min(20.0f, sideways / std::fabs(dir.y)) : 20.0f);

		bgfx::setViewFrameBuffer(m_shadow_view_id, m_shadow_target);
		bgfx::setViewClear(m_shadow_view_id, BGFX_CLEAR_DEPTH, 0, 1.0f, 0);
		bgfx::setViewRect(m_shadow_view_id, 0, 0, uint16_t(m_shadow_size), uint16_t(m_shadow_size));
		bgfx::setViewTransform(m_shadow_view_id, view, proj);
		bgfx::touch(m_shadow_view_id);

		// World to map texture coordinates (and depth): the light's view and projection, then the offset and scale to 0..1
		const float sy = caps->originBottomLeft ? 0.5f : -0.5f;
		const float sz = caps->homogeneousDepth ? 0.5f : 1.0f;
		const float tz = caps->homogeneousDepth ? 0.5f : 0.0f;
		const float crop[16] = { 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, sy, 0.0f, 0.0f, 0.0f, 0.0f, sz, 0.0f, 0.5f, 0.5f, tz, 1.0f };
		float lightViewProj[16], shadowMatrix[16];
		bx::mtxMul(lightViewProj, view, proj);
		bx::mtxMul(shadowMatrix, lightViewProj, crop);
		const float params[4] = { m_shadows.strength, m_shadows.bias, 1.0f / float(m_shadow_size), m_shadows.normalOffset };
		MeshComponent3D_Bgfx::set_shadow_state(m_shadow_depth, shadowMatrix, params);
		MeshComponent3D_Bgfx::set_cull_frustum(1, lightViewProj, caps->homogeneousDepth);
		m_shadow_active = true;
	}

	// The part of the camera's view that can receive shadows (between the focus's height, minus a little, and
	// 'receiverHeight' over it), seen from the light: a square around it, in whole meters so it keeps its size while the
	// camera only moves. Leaves the defaults (the whole extent around the focus) when the camera has no matrices.
	void BgfxGraphicsSystem::fit_shadow_to_camera(const float lightView[16], float focusY, float& centerX, float& centerY, float& half) const
	{
		const float* camView = nullptr;
		const float* camProj = nullptr;
		if (!m_active_camera || !m_active_camera->get_view_projection(camView, camProj))
		{
			return;
		}
		float viewProj[16], inverse[16];
		bx::mtxMul(viewProj, camView, camProj);
		bx::mtxInverse(inverse, viewProj);
		const float nearZ = bgfx::getCaps()->homogeneousDepth ? -1.0f : 0.0f;
		const float low = focusY - 1.0f, high = focusY + m_shadows.receiverHeight;
		float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
		int points = 0;
		for (int corner = 0; corner < 4; ++corner)
		{
			const float sx = (corner & 1) ? 1.0f : -1.0f, sy = (corner & 2) ? 1.0f : -1.0f;
			const bx::Vec3 a = bx::mulH(bx::Vec3(sx, sy, nearZ), inverse);
			const bx::Vec3 b = bx::mulH(bx::Vec3(sx, sy, 1.0f), inverse);
			// The corner's ray clipped to the slab of heights that receive shadows
			float t0 = 0.0f, t1 = 1.0f;
			const float dy = b.y - a.y;
			if (std::fabs(dy) < 1e-6f)
			{
				if (a.y < low || a.y > high) continue;
			}
			else
			{
				float ta = (low - a.y) / dy, tb = (high - a.y) / dy;
				if (ta > tb) std::swap(ta, tb);
				t0 = std::max(t0, ta);
				t1 = std::min(t1, tb);
				if (t0 > t1) continue;
			}
			for (const float t : { t0, t1 })
			{
				const bx::Vec3 world(a.x + (b.x - a.x) * t, a.y + dy * t, a.z + (b.z - a.z) * t);
				const bx::Vec3 light = bx::mul(world, lightView);
				minX = std::min(minX, light.x); maxX = std::max(maxX, light.x);
				minY = std::min(minY, light.y); maxY = std::max(maxY, light.y);
				++points;
			}
		}
		if (points == 0)
		{
			return;
		}
		// Clamped to the extent around the focus (the light's view is centered on it)
		const float limit = m_shadows.extent;
		minX = std::max(minX, -limit); maxX = std::min(maxX, limit);
		minY = std::max(minY, -limit); maxY = std::min(maxY, limit);
		if (minX >= maxX || minY >= maxY)
		{
			return;
		}
		half = std::min(limit, std::ceil(0.5f * std::max(maxX - minX, maxY - minY) + 1.0f));
		centerX = 0.5f * (minX + maxX);
		centerY = 0.5f * (minY + maxY);
		static float logged = 0.0f;
		if (half != logged)
		{
			logged = half;
			LOG_INFO("[BgfxGraphicsSystem] Shadow map fitted to the camera: %.0f m across", 2.0f * half);
		}
	}

	BgfxGraphicsSystem::Quality BgfxGraphicsSystem::default_quality()
	{
		if (const char* text = std::getenv("PAKAL_QUALITY"))
		{
			const std::string name = text;
			if (name == "low") return Quality::Low;
			if (name == "medium") return Quality::Medium;
			if (name == "high") return Quality::High;
			if (name == "ultra") return Quality::Ultra;
		}
#if defined(__ANDROID__)
		// The system's version stands for the phone's age: Android 8-9 low, 10-13 high, 14 and later ultra
		char sdk[PROP_VALUE_MAX] = {};
		const int level = __system_property_get("ro.build.version.sdk", sdk) > 0 ? std::atoi(sdk) : 0;
		if (level < 29) return Quality::Low;
		if (level < 34) return Quality::High;
		return Quality::Ultra;
#else
		return Quality::Ultra;
#endif
	}

	const char* BgfxGraphicsSystem::quality_name(Quality quality)
	{
		switch (quality)
		{
		case Quality::Low: return "low";
		case Quality::Medium: return "medium";
		case Quality::High: return "high";
		default: return "ultra";
		}
	}

	void BgfxGraphicsSystem::apply_quality()
	{
		Profiler::get().info("quality", quality_name(m_quality));
		m_shadows = m_shadowsWanted;
		m_post = m_postWanted;
		m_shadow_moving_only = false;
		switch (m_quality)
		{
		case Quality::Low:
			m_shadows.mapSize = std::min<uint32_t>(m_shadows.mapSize, 2048);
			// With a lightmap the fixed scene's shadow on the floor is baked: the map only holds what moves (characters,
			// props that break), at half the size
			if (bgfx::isValid(m_lightmap_texture) && m_lightmap_enabled)
			{
				m_shadow_moving_only = true;
				m_shadows.mapSize = std::min<uint32_t>(m_shadows.mapSize, 1024);
			}
			m_post.quality = std::min(m_post.quality, 0.3f);
			break;
		case Quality::Medium:
			m_shadows.mapSize = std::min<uint32_t>(m_shadows.mapSize, 2048);
			m_post.quality = std::min(m_post.quality, 0.5f);
			break;
		case Quality::High:
		case Quality::Ultra:
			break;
		}
	}

	void BgfxGraphicsSystem::render_sky()
	{
		if (!bgfx::isValid(m_sky_program) || !bgfx::isValid(m_sky_vb))
		{
			return;
		}

		bgfx::setUniform(m_u_sky_top, m_sky_top);
		bgfx::setUniform(m_u_sky_horizon, m_sky_horizon);
		{
			// the sun where the light comes from, and the time the clouds drift with
			static const auto clockStart = std::chrono::steady_clock::now();
			const float seconds = std::fmod(std::chrono::duration<float>(std::chrono::steady_clock::now() - clockStart).count(), 100000.0f);
			const float lx = -m_light_dir[0], ly = -m_light_dir[1], lz = -m_light_dir[2];
			const float length = std::max(1e-4f, std::sqrt(lx * lx + ly * ly + lz * lz));
			const float sun[4] = { lx / length, ly / length, lz / length, seconds };
			// the light's color brought down to a color (its intensity may be over 1), and the clouds' cover
			const float peak = std::max(1.0f, std::max(m_light_color[0], std::max(m_light_color[1], m_light_color[2])));
			const float sunColor[4] = { m_light_color[0] / peak, m_light_color[1] / peak, m_light_color[2] / peak, m_sky_clouds };
			bgfx::setUniform(m_u_sky_sun, sun);
			bgfx::setUniform(m_u_sky_sun_color, sunColor);
		}
		bgfx::setVertexBuffer(0, m_sky_vb);
		bgfx::setIndexBuffer(m_sky_ib);
		bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_DEPTH_TEST_LEQUAL);
		bgfx::submit(m_main_view_id, m_sky_program);
	}

	void BgfxGraphicsSystem::update_grid()
	{
		// a phone walked the thousands of models of a town every frame (a box test and a pointer each, twice with the sun's
		// view) to draw the few hundred in view
		PAKAL_PROFILE_SCOPE("gfx.grid");
		const float kGridCell = 32.0f;
		static const bool enabled = [] { const char* text = std::getenv("PAKAL_GRID"); return !text || std::string(text) != "0"; }();
		if (!enabled)
		{
			// PAKAL_GRID=0: every model walked every frame (to measure what the grid saves)
			m_loose.assign(m_meshes3d.begin(), m_meshes3d.end());
			m_frameMeshes.assign(m_meshes3d.begin(), m_meshes3d.end());
			return;
		}
		if (MeshComponent3D_Bgfx::take_grid_moved()) m_gridStale = true;
		if (m_gridStale || ++m_gridAge > 30)
		{
			m_gridStale = false;
			m_gridAge = 0;
			m_gridCells.clear();
			m_loose.clear();
			std::unordered_map<int64_t, size_t> cellOf;
			std::vector<std::array<float, 6>> bounds;   // each cell's lowest and highest corner
			for (auto* mesh : m_meshes3d)
			{
				if (!mesh) continue;
				// in it: what did not move since the grid was made last (a second ago), so what moves stays loose
				const uint32_t moves = mesh->moves();
				const bool still = mesh->grid_candidate() && moves == mesh->grid_moves();
				mesh->set_gridded(still, moves);
				if (!still)
				{
					m_loose.push_back(mesh);
					continue;
				}
				float center[3], extent[3];
				mesh->get_world_box(center, extent);
				const int64_t cx = static_cast<int64_t>(std::floor(center[0] / kGridCell));
				const int64_t cz = static_cast<int64_t>(std::floor(center[2] / kGridCell));
				const int64_t key = (cx << 32) ^ (cz & 0xffffffff);
				auto found = cellOf.find(key);
				if (found == cellOf.end())
				{
					found = cellOf.emplace(key, m_gridCells.size()).first;
					m_gridCells.emplace_back();
					bounds.push_back({ center[0] - extent[0], center[1] - extent[1], center[2] - extent[2],
						center[0] + extent[0], center[1] + extent[1], center[2] + extent[2] });
				}
				std::array<float, 6>& box = bounds[found->second];
				for (int axis = 0; axis < 3; ++axis)
				{
					box[axis] = std::min(box[axis], center[axis] - extent[axis]);
					box[3 + axis] = std::max(box[3 + axis], center[axis] + extent[axis]);
				}
				m_gridCells[found->second].meshes.push_back(mesh);
			}
			for (size_t i = 0; i < m_gridCells.size(); ++i)
			{
				for (int axis = 0; axis < 3; ++axis)
				{
					m_gridCells[i].center[axis] = 0.5f * (bounds[i][axis] + bounds[i][3 + axis]);
					m_gridCells[i].extent[axis] = 0.5f * (bounds[i][3 + axis] - bounds[i][axis]);
				}
			}
		}

		// this frame's: the loose ones, and the squares the camera sees or whose shadows can fall in its view (the test each
		// model makes, on the square's box)
		m_frameMeshes.assign(m_loose.begin(), m_loose.end());
		const float reach = MeshComponent3D_Bgfx::shadow_reach();
		for (const GridCell& cell : m_gridCells)
		{
			bool seen = !MeshComponent3D_Bgfx::box_outside_frustum(0, cell.center, cell.extent);
			if (!seen && m_shadow_active && !MeshComponent3D_Bgfx::box_outside_frustum(1, cell.center, cell.extent))
			{
				if (reach < 0.0f) seen = true;
				else
				{
					const float grow = std::max(0.0f, cell.center[1] + cell.extent[1]) * reach + 0.5f;
					const float grown[3] = { cell.extent[0] + grow, cell.extent[1] + grow, cell.extent[2] + grow };
					seen = !MeshComponent3D_Bgfx::box_outside_frustum(0, cell.center, grown);
				}
			}
			if (seen) m_frameMeshes.insert(m_frameMeshes.end(), cell.meshes.begin(), cell.meshes.end());
		}
	}

	void BgfxGraphicsSystem::render_meshes3d(float dt_seconds)
	{
		update_grid();
		// A crowd on screen (more than kFullRatePoses skinned models): the ones that carry nothing are posed every other
		// frame, half of them each frame (their clips' clocks go on every frame). On a phone at 30 FPS posing was a third of
		// the meshes' time with a horde in view
		const int kFullRatePoses = 16;
		int skinned = 0;
		for (auto* mesh : m_loose) if (mesh && mesh->is_skinned() && mesh->drawn_in_main_view()) ++skinned;
		const bool halve = skinned > kFullRatePoses;
		static unsigned frameParity = 0;
		++frameParity;
		unsigned crowdIndex = 0;

		// GPU instancing: the static copies of one model (a town's palms, lamps, benches, plants: the same model hundreds of
		// times) in one draw per material instead of one per copy. Copies that are faded, tinted, lightmapped or few (under
		// kMinInstances) are drawn one by one as before. Off on Android unless PAKAL_INSTANCING=1: on a Kirin 980 sending draws
		// was not what cost (walking and testing the models was), and grouping them cost more than it saved (p95 41 -> 46 ms);
		// PAKAL_INSTANCING=0 turns it off on desktop
		const int kMinInstances = 4;
		static const bool instancingWanted = []
		{
			const char* text = std::getenv("PAKAL_INSTANCING");
			if (text) return std::string(text) != "0";
#if defined(__ANDROID__)
			return false;
#else
			return true;
#endif
		}();
		const bool instancing = instancingWanted && (bgfx::getCaps()->supported & BGFX_CAPS_INSTANCING) != 0
			&& bgfx::isValid(m_pbr_inst_program) && bgfx::isValid(m_mesh_inst_program);
		++m_instanceFrame;
		if (instancing)
		{
			PAKAL_PROFILE_SCOPE("gfx.instances");
			// the groups: rebuilt when a grouped copy went away, and every 4 s for new models and the ones that finished
			// loading (meanwhile they are drawn one by one)
			if (m_groupsVersion != m_meshesVersion || ++m_groupsAge > 240)
			{
				m_groupsVersion = m_meshesVersion;
				m_groupsAge = 0;
				std::unordered_map<uint16_t, std::vector<MeshComponent3D_Bgfx*>> byModel;
				for (auto* mesh : m_meshes3d)
				{
					if (!mesh) continue;
					mesh->set_grouped(false);
					if (mesh->instanceable()) byModel[mesh->instance_key()].push_back(mesh);
				}
				m_instanceGroups.clear();
				for (auto& entry : byModel)
				{
					if (entry.second.size() < static_cast<size_t>(kMinInstances)) continue;
					for (auto* mesh : entry.second) mesh->set_grouped(true);
					m_instanceGroups.push_back(std::move(entry.second));
				}
			}
			std::vector<MeshComponent3D_Bgfx*> taken, seen;
			for (const auto& copies : m_instanceGroups)
			{
				// this frame's: the copies still drawn as their model is (one faded or hidden now goes one by one)
				taken.clear();
				for (auto* copy : copies)
				{
					if (copy->instanceable()) taken.push_back(copy);
				}
				if (taken.size() < static_cast<size_t>(kMinInstances)) continue;
				bool drawn = true;
				if (m_shadow_active && bgfx::isValid(m_shadow_inst_program))
				{
					seen.clear();
					for (auto* copy : taken)
					{
						if (!(m_shadow_moving_only && copy->shadow_baked()) && copy->drawn_in_depth_view()) seen.push_back(copy);
					}
					drawn = MeshComponent3D_Bgfx::render_instances(m_shadow_view_id, m_shadow_inst_program, seen, true);
				}
				seen.clear();
				for (auto* copy : taken)
				{
					if (copy->drawn_in_main_view()) seen.push_back(copy);
				}
				if (!seen.empty() && drawn)
				{
					bgfx::setUniform(m_u_light_dir, m_light_dir);
					bgfx::setUniform(m_u_light_color, m_light_color);
					bgfx::setUniform(m_u_ambient, m_ambient);
					bgfx::setUniform(m_u_ambient_ground, m_ambient_ground);
					drawn = MeshComponent3D_Bgfx::render_instances(m_main_view_id,
						taken.front()->uses_pbr() ? m_pbr_inst_program : m_mesh_inst_program, seen, false);
				}
				// bgfx out of room for instances this frame: these copies are drawn one by one below
				if (drawn)
				{
					for (auto* copy : taken) copy->mark_instanced(m_instanceFrame);
				}
			}
		}

		for (auto* mesh : m_frameMeshes)
		{
			if (!mesh || mesh->instanced_in(m_instanceFrame))
			{
				continue;
			}

			// Animation advances once per frame, whether or not the mesh is drawn (a model without bones has none: thousands
			// of them in a town, not timed one by one)
			if (mesh->is_skinned())
			{
				PAKAL_PROFILE_SCOPE("gfx.pose");
				const bool pose = !halve || mesh->carries() || ((crowdIndex++ + frameParity) % 2 == 0);
				mesh->update(dt_seconds, pose);
			}
			if (m_shadow_active && !(m_shadow_moving_only && mesh->shadow_baked()))
			{
				mesh->render_depth(m_shadow_view_id, mesh->is_skinned() ? m_shadow_skin_program : m_shadow_program);
			}

			bgfx::ProgramHandle program = mesh->is_skinned() ? m_skin_program : (mesh->uses_pbr() && bgfx::isValid(m_pbr_program) ? m_pbr_program : m_mesh_program);
			if (!bgfx::isValid(program))
			{
				continue;
			}

			// Light uniforms are set before each draw, so every mesh gets the same values (only for the ones drawn: most of a
			// town is out of the frame)
			if (!mesh->drawn_in_main_view()) continue;
			bgfx::setUniform(m_u_light_dir, m_light_dir);
			bgfx::setUniform(m_u_light_color, m_light_color);
			bgfx::setUniform(m_u_ambient, m_ambient);
			bgfx::setUniform(m_u_ambient_ground, m_ambient_ground);
			mesh->render(m_main_view_id, program);
		}

		// Blended materials after every opaque one, so they see the full depth buffer; a faded model (a building or tree
		// hiding the player) draws all its materials here. Farthest first, by the nearest point of each box (a building can
		// wrap the camera): a faded building writes its depth before its color, which would hide a faded tree behind it
		// drawn later.
		std::vector<std::pair<float, MeshComponent3D_Bgfx*>> blended;
		const float* view = nullptr;
		const float* proj = nullptr;
		const bool sorted = m_active_camera && m_active_camera->get_view_projection(view, proj);
		for (auto* mesh : m_frameMeshes)
		{
			if (!mesh || !mesh->needs_transparent_pass())
			{
				continue;
			}
			float center[3] = { 0.0f, 0.0f, 0.0f }, extent[3] = { 0.0f, 0.0f, 0.0f };
			if (sorted) mesh->get_world_box(center, extent);
			const float depth = sorted ? view[2] * center[0] + view[6] * center[1] + view[10] * center[2] + view[14]
				- (std::fabs(view[2]) * extent[0] + std::fabs(view[6]) * extent[1] + std::fabs(view[10]) * extent[2]) : 0.0f;
			blended.push_back({ depth, mesh });
		}
		std::stable_sort(blended.begin(), blended.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
		for (const auto& entry : blended)
		{
			MeshComponent3D_Bgfx* mesh = entry.second;
			bgfx::ProgramHandle program = mesh->is_skinned() ? m_skin_program : (mesh->uses_pbr() && bgfx::isValid(m_pbr_program) ? m_pbr_program : m_mesh_program);
			if (!bgfx::isValid(program))
			{
				continue;
			}
			bgfx::setUniform(m_u_light_dir, m_light_dir);
			bgfx::setUniform(m_u_light_color, m_light_color);
			bgfx::setUniform(m_u_ambient, m_ambient);
			bgfx::setUniform(m_u_ambient_ground, m_ambient_ground);
			mesh->render(m_transparent_view_id, program, true);
		}
	}

	void BgfxGraphicsSystem::create_sprite_shader()
	{
		const char* renderer_dir = get_shader_renderer_dir(bgfx::getRendererType());
		// Prefer custom RGBA sprite shaders; keep fallbacks for robustness.
		const char* vs_names[] = { "vs_sprite_simple", "vs_sprite", "vs_cubes" };
		const char* fs_names[] = { "fs_sprite_simple", "fs_sprite", "fs_cubes" };

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
