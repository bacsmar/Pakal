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
#include "BgfxDebugUI.h"
#include "GpuParticles.h"

#include <bgfx/bgfx.h>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <mutex>

namespace Pakal
{
	class IDebugDrawerClient;
	class SpriteComponent_Bgfx;
	class CameraComponent_Bgfx;
	class CameraComponent3D_Bgfx;
	class MeshComponent3D_Bgfx;

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
		bgfx::ViewId m_transparent_view_id = 1; // blended 3D materials, drawn after the main view
		bgfx::ViewId m_post_view_id = 2;        // the post-process pass: the frame, composed onto the back buffer
		bgfx::ViewId m_overlay_view_id = 4;     // lines in pixels over everything (touch controls)
		bgfx::ViewId m_shadow_view_id = 3;      // the sun's view (depth only); runs before the others (see on_update_graphics)
		bgfx::ViewId m_ui_view_id = 5;          // the game's interface (the UI manager), over the picture
		bgfx::ViewId m_debug_ui_view_id = 6;    // the development panels (Dear ImGui), over everything
		bgfx::ViewId m_easu_view_id = 7;        // upscaling: the finished picture made the screen's size (FSR 1 EASU)
		bgfx::ViewId m_rcas_view_id = 8;        // ... and sharpened onto the back buffer (FSR 1 RCAS)
		BgfxDebugUI m_debug_ui;
		uint32_t m_width = 800;           // the size everything is drawn at (the camera, the interface, the input)
		uint32_t m_height = 600;
		uint32_t m_output_width = 0;      // the back buffer's, when it is bigger: the picture is scaled up to it (0: the same)
		uint32_t m_output_height = 0;
		float m_render_scale = 1.0f;      // PAKAL_RENDER_SCALE (desktop, to try the upscaler): drawn at the window's size times this
		// Desktop: the most lines the picture is drawn at (PAKAL_RENDER_HEIGHT, 1080 by default; 0: the window's own). A
		// taller window (a maximized one) gets the picture at this height, scaled up to it by FSR. 0 when the size is fixed
		// otherwise (a phone's settings, PAKAL_RENDER_SCALE).
		uint32_t m_auto_render_height = 0;
		float m_pending_render_scale = 0.0f; // set_render_scale, for the next frame (0: none)
		struct Governor
		{
			bool enabled = false;
			unsigned maxFps = 60, cap = 60;
			float baseScale = 1.0f;          // the render scale it may go back up to
			int slow = 0, spare = 0;         // seconds in a row too slow / with room to spare
			int thermal = -1;                // the system's thermal status (Android 11+), -1 unknown
			double secondsSinceDown = 1e9;   // since it last stepped down (going back up waits)
			double secondsSinceUp = 1e9;     // since it last stepped up
			double backoff = 1.0;            // the waits before stepping up, doubled each time a step up does not hold
			bool fsrDropped = false;         // its first step: FSR's upscale traded for a plain stretch (far cheaper on phones)
			int warmup = 0;                  // seconds still not counted after a load (the driver's first draws of it are slow)
			int frames = 0;
			double workMs = 0.0, lastWorkMs = 0.0, lastFps = 0.0, worstMs = 0.0;
			std::chrono::steady_clock::time_point windowStart = std::chrono::steady_clock::now();
		} m_governor;
		std::atomic<bool> m_governorHeld{ false };
		void governor_frame(double workMs);
		int read_thermal_status();
		void apply_render_scale();
		uint32_t m_reset_flags = BGFX_RESET_VSYNC;

		std::vector<IDebugDrawerClient*> m_debug_renderers;
		std::vector<SpriteComponent_Bgfx*> m_sprites;
		ICameraComponent_Bgfx* m_active_camera = nullptr;
		float m_debug_text_timer = 0.0f;
		bool m_debug_text_visible = true;
		bgfx::ProgramHandle m_sprite_program;

		// 3D meshes, lit with one directional light, and a sky dome
		std::vector<MeshComponent3D_Bgfx*> m_meshes3d;
		struct Line3D { tmath::vector3df from, to; uint32_t color; bool depthTest; };
		std::mutex m_lineMutex;
		std::vector<Line3D> m_pendingLines;
		struct Triangle3D { tmath::vector3df a, b, c; uint32_t color; bool depthTest; };
		std::vector<Triangle3D> m_pendingTriangles;
		struct ScreenLine { float x0, y0, x1, y1; uint32_t color; };
		std::vector<ScreenLine> m_pendingScreenLines;
		struct Particle { tmath::vector3df at; float radius; uint32_t color; bool additive; bool flat = false; uint16_t sprite = 0; float angle = 0.0f; };
		std::vector<Particle> m_pendingParticles;
		// What the game committed last (commit_frame_drawing): drawn every frame until the next commit. The graphics thread
		// is not the game's on Android: taking the lists as they were cut a game frame in half (some of it drawn, the rest
		// not) or drew a frame with none (the game had not updated): stains, halos and bars blinked.
		struct Committed { std::vector<Line3D> lines; std::vector<Triangle3D> triangles; std::vector<ScreenLine> screenLines; std::vector<Particle> particles; };
		Committed m_committed;
		bool m_commits = false;   // the game commits its frames (else each frame takes what was queued, as before)
		std::chrono::steady_clock::time_point m_lastCommit;   // a quarter of a second without one: back to taking the queue
		Committed take_frame_drawing();
		Committed m_frameDrawing;   // this frame's (graphics thread)
		bgfx::ProgramHandle m_particle_program = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_u_particle_sheet = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_s_particle = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle m_particle_sheet = BGFX_INVALID_HANDLE;
		float m_particle_sheet_size[2] = { 0.0f, 0.0f };   // its columns and rows
		void render_particles();
		bgfx::ProgramHandle m_mesh_program = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle m_skin_program = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle m_pbr_program = BGFX_INVALID_HANDLE;
		// Many copies of one static model in one draw (GPU instancing): the same shading, the transform per instance
		bgfx::ProgramHandle m_pbr_inst_program = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle m_mesh_inst_program = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle m_shadow_inst_program = BGFX_INVALID_HANDLE;
		// The copies of each model, kept while the list of models stays the same (a version bumped on each register and
		// unregister; refreshed every so often too, for the models that finish loading)
		std::vector<std::vector<MeshComponent3D_Bgfx*>> m_instanceGroups;
		uint32_t m_meshesVersion = 1, m_groupsVersion = 0, m_groupsAge = 0, m_instanceFrame = 0;
		// The models that stay put in squares of kGridCell meters, each with the box of what is in it: a frame walks only the
		// squares the camera (or the sun's shadows into its view) can see, and the rest of the models (animated, moving,
		// new) one by one. Made again every second, and at once when a model in it goes or moves.
		struct GridCell { float center[3] = {}, extent[3] = {}; std::vector<MeshComponent3D_Bgfx*> meshes; };
		std::vector<GridCell> m_gridCells;
		std::vector<MeshComponent3D_Bgfx*> m_loose;        // not in the grid: walked every frame
		std::vector<MeshComponent3D_Bgfx*> m_frameMeshes;  // this frame's: the loose ones and those of the squares in view
		bool m_gridStale = true;
		uint32_t m_gridAge = 0;
		void update_grid();
		bgfx::ProgramHandle m_sky_program = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_u_light_dir = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_u_light_color = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_u_ambient = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_u_ambient_ground = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_u_sky_top = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_u_sky_horizon = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_u_sky_sun = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_u_sky_sun_color = BGFX_INVALID_HANDLE;
		bgfx::VertexBufferHandle m_sky_vb = BGFX_INVALID_HANDLE;
		bgfx::IndexBufferHandle m_sky_ib = BGFX_INVALID_HANDLE;
		float m_light_dir[4] = { 0.4f, -1.0f, -0.6f, 0.0f };
		float m_light_color[4] = { 1.0f, 0.95f, 0.85f, 1.0f };
		float m_ambient[4] = { 0.25f, 0.25f, 0.3f, 1.0f };
		float m_ambient_ground[4] = { 0.16f, 0.15f, 0.14f, 1.0f };
		float m_sky_top[4] = { 0.25f, 0.45f, 0.75f, 1.0f };
		float m_sky_horizon[4] = { 0.75f, 0.82f, 0.9f, 1.0f };
		float m_sky_clouds = 0.55f;   // how much of the sky the clouds cover (0..1)

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
		void create_sprite_shader();
		void create_mesh_resources();
		void destroy_mesh_resources();
		void render_sky();
		void render_meshes3d(float dt_seconds);
		void create_post_resources();
		void destroy_post_resources();
		bool prepare_post_target();       // (re)makes the frame target when the size changed; false when post-processing cannot run
		void render_post();
		void create_shadow_resources();
		void destroy_shadow_resources();
		void prepare_shadow();           // sets up the sun's view for this frame, or turns shadows off
		void fit_shadow_to_camera(const float lightView[16], float focusY, float& centerX, float& centerY, float& half) const;
		void log_frame_stats();          // PAKAL_FPS_LOG: draws, triangles and timings of the last frame

		bgfx::TextureHandle m_gi_texture = BGFX_INVALID_HANDLE;
		float m_gi_origin[4] = {};
		float m_gi_size[4] = {};
		bgfx::TextureHandle m_lightmap_texture = BGFX_INVALID_HANDLE;
		float m_lightmap_shadow = 0.0f;
		bool m_lightmap_enabled = true;
		bool m_gi_enabled = true;
		bool m_shadow_moving_only = false; // the shadow map leaves out what the lightmap has (Low quality with a lightmap)
		bgfx::ProgramHandle m_shadow_program = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle m_shadow_skin_program = BGFX_INVALID_HANDLE;
		bgfx::FrameBufferHandle m_shadow_target = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle m_shadow_depth = BGFX_INVALID_HANDLE;
		uint32_t m_shadow_size = 0;
		bool m_shadow_active = false;     // this frame has a shadow map

		bgfx::ProgramHandle m_post_program = BGFX_INVALID_HANDLE;
		bgfx::FrameBufferHandle m_post_target = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle m_post_color = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle m_post_depth = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_u_post[11] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
		float m_sketch = 0.0f;
		bgfx::UniformHandle m_s_post_color = BGFX_INVALID_HANDLE;
		bgfx::UniformHandle m_s_post_depth = BGFX_INVALID_HANDLE;
		bgfx::VertexLayout m_post_layout;
		uint32_t m_post_width = 0, m_post_height = 0;
		bool m_post_active = false;       // this frame is drawn into the post-process target

		// Upscaling: what goes to the screen is drawn into the frame target, then scaled up to the back buffer
		bool prepare_upscale();           // (re)makes the targets; false when the picture is drawn at the screen's size
		void render_upscale();
		void destroy_upscale_resources();
		void reset_backbuffer();
		bgfx::ProgramHandle m_easu_program = BGFX_INVALID_HANDLE;
		bgfx::ProgramHandle m_rcas_program = BGFX_INVALID_HANDLE;
		bgfx::FrameBufferHandle m_frame_target = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle m_frame_color = BGFX_INVALID_HANDLE;
		bgfx::FrameBufferHandle m_easu_target = BGFX_INVALID_HANDLE;
		bgfx::TextureHandle m_easu_color = BGFX_INVALID_HANDLE;
		uint32_t m_upscale_width = 0, m_upscale_height = 0, m_upscale_out_width = 0, m_upscale_out_height = 0;
		bgfx::UniformHandle m_u_fsr[5] = { BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE, BGFX_INVALID_HANDLE };
		bgfx::UniformHandle m_s_fsr_input = BGFX_INVALID_HANDLE;
		bool m_upscale_active = false;    // this frame goes through the frame target

	public:
		// Image post-processing: the frame goes to a target and is composed onto the screen with color grading, a tilt-shift
		// blur and screen-space ambient occlusion. Off by default; the sprite games are not affected.
		struct PostSettings
		{
			bool enabled = false;
			float exposure = 1.0f;
			float contrast = 1.0f;
			float saturation = 1.0f;
			float gain[3] = { 1.0f, 1.0f, 1.0f };  // color multiplier (a tint)
			float vignette = 0.0f;                  // 0..1, darkening toward the corners
			float tiltBlur = 0.0f;                  // pixels of blur at the far edge of the picture (0: off)
			float tiltFocus = 0.5f;                 // the sharp line, from the top of the picture (0..1)
			float tiltBand = 0.25f;                 // half height of the sharp band around that line (0..0.5)
			float aoStrength = 0.0f;                // 0: off
			float aoRadius = 0.8f;                  // meters
			float aoBias = 0.08f;
			float aoRange = 2.0f;                   // meters: what is farther does not occlude
			// How many samples the pass takes (1: all; less is cheaper and noisier). The quality level caps it.
			float quality = 1.0f;
			// Fog by distance from the camera (depth): toward 'fogColor' (the horizon's), from 'fogStart' meters on, with
			// 'fogDensity' per meter, never more than 'fogMax'; the sky (what is at the far plane) up to 'fogSky'. Off at 0.
			float fogColor[3] = { 0.8f, 0.85f, 0.9f };
			float fogDensity = 0.0f;
			float fogStart = 0.0f;
			float fogMax = 1.0f;
			float fogSky = 0.0f;
			// From 55% of 'fogHorizon' meters to it, what is drawn fades wholly into what the sky shows at the horizon (0: off)
			float fogHorizon = 0.0f;
		};
		// The sun's shadows: a depth map of the models seen from the light, around the point the camera looks at
		struct ShadowSettings
		{
			bool enabled = false;
			uint32_t mapSize = 2048;
			float extent = 40.0f;          // meters from the focus to each side of the map (the most it covers when fitted)
			// The map covers only what the camera sees, up to 'receiverHeight' meters over the focus (higher surfaces are
			// left unshadowed): fewer models cast into it and its texels are smaller
			bool fitToCamera = true;
			float receiverHeight = 10.0f;
			float strength = 0.85f;        // how dark the sun's light gets in shadow (1: all of it)
			float bias = 0.0015f;          // depth bias, in the map's depth range
			float normalOffset = 0.06f;    // meters, along the surface normal
		};
		// Baked ambient light: a 3D grid of RGB8 probes covering a box of the world (render space; a negative size flips that axis).
		// 'strength' is how much of the ambient light comes from it (1: all). Call on the graphics thread.
		void set_gi_volume(const uint8_t* rgba, uint16_t nx, uint16_t ny, uint16_t nz, const float origin[3], const float size[3], float strength);
		void clear_gi_volume();
		// The lightmap atlas of the level's fixed models (each says where it is in it: MeshComponent3D_Bgfx::set_lightmap_rect)
		// and how much its baked sun shadow darkens. Call on the graphics thread.
		bool set_lightmap(const std::string& path, float shadowStrength);
		void clear_lightmap();
		// Turn the loaded lightmap and light probes off and on (to compare); they stay loaded
		void set_lightmap_enabled(bool enabled) { m_lightmap_enabled = enabled; apply_quality(); }
		bool get_lightmap_enabled() const { return m_lightmap_enabled; }
		bool has_lightmap() const { return bgfx::isValid(m_lightmap_texture); }
		void set_gi_enabled(bool enabled) { m_gi_enabled = enabled; }
		bool get_gi_enabled() const { return m_gi_enabled; }
		bool has_gi() const { return bgfx::isValid(m_gi_texture); }
		// The settings as the game asks for them; what is drawn is that, capped by the quality level
		void set_shadows(const ShadowSettings& settings) { m_shadowsWanted = settings; apply_quality(); }
		const ShadowSettings& get_shadows() const { return m_shadows; }
		void set_post_process(const PostSettings& settings) { m_postWanted = settings; apply_quality(); }
		const PostSettings& get_post_process() const { return m_post; }

		// How much of the picture's cost the device can take. Each level caps the shadow and post-process settings:
		// Each level is the most a kind of device keeps at 30 FPS or more in play (Downtown). The lowest one is for old
		// phones; the others add samples and, on phones, resolution (see android/main_android.cpp):
		//   Low: 2048 shadow map, post-process (ambient occlusion, blur) with 30% of its samples.
		//   Medium: 50% of the samples. High: all of them. Ultra: High drawn 1080 px tall on Android.
		enum class Quality { Low, Medium, High, Ultra };
		void set_quality(Quality quality) { m_quality = quality; apply_quality(); }
		Quality get_quality() const { return m_quality; }
		// PAKAL_QUALITY=low|medium|high|ultra, else Ultra on desktops and, on Android, a level from the system's age
		static Quality default_quality();
		static const char* quality_name(Quality quality);
		// The picture fades into a color (amount 1 covers the whole screen, 0 shows the game). Works with or without the
		// post-process settings: a fade turns the post pass on (with neutral grading) while it is visible.
		// The sketch look (the player's option): the picture drawn as an etching on warm paper, this strong (0: off). It runs in
		// the post-process pass, which it turns on by itself.
		void set_sketch(float strength) { m_sketch = strength; }
		float get_sketch() const { return m_sketch; }
		void set_screen_fade(float r, float g, float b, float amount) { m_fade[0] = r; m_fade[1] = g; m_fade[2] = b; m_fade[3] = amount; }
		float get_screen_fade() const { return m_fade[3]; }

	private:
		void apply_quality();
		Quality m_quality = default_quality();
		PostSettings m_postWanted;
		ShadowSettings m_shadowsWanted;
		PostSettings m_post;
		float m_fade[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		ShadowSettings m_shadows;

	public:
		void register_sprite(SpriteComponent_Bgfx* sprite);
		void unregister_sprite(SpriteComponent_Bgfx* sprite);
		void register_mesh3d(MeshComponent3D_Bgfx* mesh);
		void unregister_mesh3d(MeshComponent3D_Bgfx* mesh);
		// Direction is where the light travels (from the light into the scene).
		void set_directional_light(float dirX, float dirY, float dirZ, float r, float g, float b);
		void set_ambient_light(float r, float g, float b);
		// The ambient light that reaches surfaces facing down (what the ground bounces up)
		void set_ground_ambient_light(float r, float g, float b);
		void set_sky_colors(float topR, float topG, float topB, float horizonR, float horizonG, float horizonB);
		// How much of the sky the clouds cover (0: a clear sky, 1: overcast); the sun is drawn where the light comes from
		void set_sky_clouds(float cover) { m_sky_clouds = cover < 0.0f ? 0.0f : (cover > 1.0f ? 1.0f : cover); }
		bgfx::ProgramHandle get_mesh_program() const { return m_mesh_program; }
		// Saves the next presented frame as a TGA file. Call from the graphics thread (e.g. execute_block).
		void request_screenshot(const char* filePath);
		// The window of the app changed (Android gives a new surface when the app comes back from the background): bgfx draws on it
		// width x height: what the picture is drawn at; outputWidth x outputHeight: the window's buffers when bigger (scaled up)
		void set_native_window(void* window, unsigned width, unsigned height, unsigned outputWidth = 0, unsigned outputHeight = 0);
		// How the picture is scaled up to the screen when it is drawn smaller: FSR 1 (edges rebuilt, then sharpened) or a
		// plain bilinear stretch (what the system compositor does), to compare
		enum class Upscaler { Bilinear, Fsr };
		void set_upscaler(Upscaler upscaler) { m_upscaler = upscaler; }
		Upscaler get_upscaler() const { return m_upscaler; }
		// The player's option: FSR on draws the picture at the settings' fsr_scale of the screen and scales it up with FSR 1 (the
		// governor may draw it smaller still); off draws it at plain_scale (the screen's size on a desktop) and the governor only
		// lowers the frame cap. Applied at the next frame.
		void set_fsr(bool on);
		bool get_fsr() const { return m_fsr; }
		bool is_upscaling() const { return m_output_width > m_width || m_output_height > m_height; }
		// Drawn at the screen's size or smaller (the render scale can change): the upscale settings apply
		bool can_upscale() const { return m_output_width > 0; }
		uint32_t get_output_width() const { return m_output_width ? m_output_width : m_width; }
		uint32_t get_output_height() const { return m_output_height ? m_output_height : m_height; }
		// The share of the screen's size the picture is drawn at (1: the screen's), the rest made up by the upscaler: FSR's
		// modes are 0.77 (ultra quality), 0.67 (quality), 0.59 (balanced), 0.5 (performance). Applied at the next frame.
		void set_render_scale(float scale) { m_pending_render_scale = scale; }
		float get_render_scale() const { return float(m_height) / float(get_output_height()); }
		// Performance governor (phones): watches how long each frame takes and the system's thermal status, and steps the
		// picture down when the device cannot hold it (the frame cap from 60 to 30, then the render scale, which FSR makes up)
		// and back up when there is room for a while. Some phones cut their GPU's clock to less than half when it works flat
		// out, without reporting any thermal status: the frame times are what tells. maxFps: the cap it starts at.
		void set_governor(bool enabled, unsigned maxFps);
		bool get_governor() const { return m_governor.enabled; }
		// While held (a game's camera pan: a wide shot of the whole town for a few seconds), its seconds count for nothing:
		// a slow pan does not take the play down to 30. The seconds just after a load do not count either
		void hold_governor(bool held) { m_governorHeld = held; }
		// One line for the dev panel: cap, scale, thermal status, last frame work
		std::string governor_status() const;
		// RCAS sharpening in stops: 0 the most, each one halves it
		void set_sharpness(float stops) { m_sharpness = stops; }
		float get_sharpness() const { return m_sharpness; }
		// Where the picture's views draw this frame: the frame target when upscaling, else the back buffer. The interface
		// (RmlUi, the development panels) is drawn after the upscale, at the output size, in its pixels
		bgfx::FrameBufferHandle get_screen_target() const { return m_upscale_active ? m_frame_target : bgfx::FrameBufferHandle(BGFX_INVALID_HANDLE); }
		// Text on the screen at a grid cell (column, row). Call every frame; the text is not kept.
		void draw_text(int column, int row, const char* text);
		// One-frame world-space line; queued safely from the game thread. Color is ABGR.
		// A line in pixels (0,0 is the top left of the picture), drawn over everything in the next frame (touch controls, markers)
		void draw_screen_line(float x0, float y0, float x1, float y1, uint32_t color = 0xc0ffffff);
		void draw_line3d(const tmath::vector3df& from, const tmath::vector3df& to, uint32_t color = 0xff80e8ff, bool depthTest = true);
		// One-frame solid world-space triangle, seen from both sides (markers, pickups: shapes that read from far away). ABGR.
		void draw_triangle3d(const tmath::vector3df& a, const tmath::vector3df& b, const tmath::vector3df& c, uint32_t color,
			bool depthTest = true);
		// One-frame particle: a soft round dot facing the camera, 'radius' world units, ABGR color (alpha: how much it covers).
		// Additive ones add light (fire, sparks); the others blend over the scene (smoke, water, dust), farthest first.
		void draw_particle(const tmath::vector3df& at, float radius, uint32_t color, bool additive = false);
		// The same lying flat on the floor (a world-space disc on x-z at 'at'): a pool of light, a stain
		void draw_ground_spot(const tmath::vector3df& at, float radius, uint32_t color, bool additive = false);
		// The sprites particles may show instead of the dot: one texture (straight alpha) cut into columns x rows cells,
		// counted from 1 left to right, top to bottom. Call on the graphics thread; an empty path drops it.
		void set_particle_sprites(const std::string& texture, int columns, int rows);
		bool has_particle_sprites() const { return bgfx::isValid(m_particle_sheet); }
		// A particle or a floor spot showing a sprite of that sheet (0: the dot), turned 'angle' radians, tinted by 'color'
		void draw_sprite_particle(const tmath::vector3df& at, float radius, uint32_t color, bool additive, uint16_t sprite, float angle);
		void draw_sprite_spot(const tmath::vector3df& at, float radius, uint32_t color, bool additive, uint16_t sprite, float angle);
		// Particles the GPU moves (compute) and draws, when the device can (gpu_particles): the game hands each one over
		// once and forgets it. PAKAL_GPU_PARTICLES=0 keeps them on the CPU
		bool gpu_particles() const { return m_gpuParticles.available(); }
		void emit_gpu_particle(const GpuParticles::Particle& particle) { m_gpuParticles.emit(particle); }
		// The game's seconds (none while it is paused) move the GPU's particles; clear: none left (a level that ends)
		void advance_gpu_particles(float seconds) { m_gpuParticles.advance(seconds); }
		void clear_gpu_particles() { m_gpuParticles.clear(); }
		// From the game thread, at the end of its frame: what it queued since the last call (lines, triangles, particles,
		// screen lines) is its whole frame, drawn until the next call. A game that never calls it has its queue taken each frame.
		void commit_frame_drawing();
		void set_active_camera(ICameraComponent_Bgfx* camera);
		void clear_active_camera(ICameraComponent_Bgfx* camera);
		bgfx::ProgramHandle get_sprite_program() const { return m_sprite_program; }
		bgfx::ViewId get_main_view_id() const { return m_main_view_id; }
		// Size of the back buffer in pixels (follows the window)
		uint32_t get_width() const { return m_width; }
		uint32_t get_height() const { return m_height; }
		// A program from the compiled shaders (Assets/shaders/<renderer>/<name>.bin); invalid if they are missing
		bgfx::ProgramHandle load_program(const char* vsName, const char* fsName);
		bgfx::ProgramHandle load_compute(const char* csName);
		GpuParticles m_gpuParticles;
		// Development panels (Dear ImGui): games add their own with get_debug_ui().add_panel(...)
		BgfxDebugUI& get_debug_ui() { return m_debug_ui; }
	private:
		Upscaler m_upscaler = Upscaler::Fsr;
		bool m_fsr = true;
		float m_sharpness = 0.25f;   // RCAS: stops below the most sharpening; negative: no RCAS pass
	public:
		// The view the game's interface draws in (after the 3D picture and the overlay lines)
		bgfx::ViewId get_ui_view_id() const { return m_ui_view_id; }
	};	
}
