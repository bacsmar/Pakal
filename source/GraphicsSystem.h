#pragma once
#include <string>
#include <vector>
#include <memory>
#include "Config.h"
#include "IParticleRenderer.h"

#include "IComponentProvider.h"
#include "System.h"
#include "OSManager.h"

namespace Pakal
{
	class IDebugDrawerClient;
	class GraphicComponent;
	class IUIManager;
	class IInputManager;
	class Archive;

	// Textures being loaded ahead (GraphicsSystem::preload_textures): while it lives they stay loaded
	class TexturePreload
	{
	public:
		virtual ~TexturePreload() = default;
		// Every texture loaded, or failed (a sprite that names a failed one loads it itself, as without a preload)
		virtual bool ready() const = 0;
		// 0..1: the share of its textures done, for a bar on a loading screen
		virtual float progress() const = 0;
	};

	class _PAKALExport GraphicsSystem : public System, public IComponentProvider
	{
		friend class Engine;
		friend class GraphicComponent;

		void on_initialize() override final;
		void on_terminate() override final;
		void on_update(unsigned long dtMilliSeconds) override final;
		void on_pause() override final;
		void on_resume() override final;		
		
	public:

		virtual void set_window_caption(const char*);
		virtual void add_debug_drawer(IDebugDrawerClient * debugDrawer) = 0;

		virtual tmath::vector2du get_screen_resolution() = 0;

		// How sprites are ordered. Layer (the default): by their layer (0..99) only, as always. LayerThenOrder: by layer, then
		// by their order within it (SpriteComponent2D::set_order: an attacker over the one it hits, a y-sort); for a 2D
		// game, since its sprites then sort behind the meshes of the same view.
		enum class SpriteOrder { Layer, LayerThenOrder };
		// The picture of a coming frame, written to 'path' (a TGA, relative to the working folder) once the graphics have read
		// it back, a frame or two later. Any thread may ask: it is queued on the graphics thread. False when the platform
		// cannot take one.
		virtual bool take_screenshot(const std::string& path) { (void)path; return false; }

		// The textures a coming scene will show (the paths its sprites will give set_texture), read, decoded and handed to the
		// GPU on a worker thread, so that those sprites find them ready; a sprite that asks for one still being loaded waits
		// for it instead of reading it again. They stay loaded while the returned ticket lives, and after it as the last set:
		// until the next preload_textures with other textures, or release_preloaded() (back at a main menu). nullptr where
		// the platform does not preload: the sprites load their textures as always.
		virtual std::shared_ptr<TexturePreload> preload_textures(const std::vector<std::string>& paths) { (void)paths; return nullptr; }
		virtual void release_preloaded() {}
		virtual void set_sprite_order(SpriteOrder order) { (void)order; }
		// The picture's quality (shadows, post-process...) as a level a game's options can offer: 0 the lowest, up to
		// quality_levels() - 1; -1 the device's own (chosen at start). Any thread: it takes effect on the next frame.
		virtual int quality_levels() const { return 0; }
		virtual int get_quality_level() const { return -1; }
		virtual void set_quality_level(int level) { (void)level; }
		// From the game thread, once a frame after it queued its drawing (lines, particles, floor spots): that is its whole
		// frame, drawn until the next call. A game that never calls it has its queue taken whenever the graphics thread
		// draws, half filled at times: on Android its blood and halos blink.
		virtual void commit_frame_drawing() {}
		// What particle systems draw with (IParticleRenderer); nullptr where the platform draws none
		virtual IParticleRenderer* particle_renderer() { return nullptr; }

		IUIManager*		get_ui_interface() const { return m_ui_manager; };

		struct Settings
		{
			tmath::vector2di resolution = {800,600};
			// The screen's size when the picture is drawn smaller (resolution) and scaled up to it (FSR 1); 0: the same
			tmath::vector2di output_resolution = {0,0};
			// Steps the picture down (frame cap, render scale) when the device cannot hold it, and back up (BgfxGraphicsSystem)
			bool governor = false;
			// A 2D game: the 3D programs (meshes, skinning, PBR, the sky, particles, shadows) are not made. A phone's driver compiles
			// every program in the first frame (six seconds on a 2016 phone); a game that draws no mesh need not wait for them
			bool no_3d = false;
			// The player's FSR option (BgfxGraphicsSystem::set_fsr): the share of the screen's height the picture is drawn at
			// with FSR, and without it (a phone's big screen keeps a cap; the stretch is then plain bilinear)
			float fsr_scale = 0.67f;
			float plain_scale = 1.0f;
			int bits = 32;
			bool full_screen = false;
			bool vsync = false;
			unsigned max_fps = 1000;
			unsigned window_id = 0;
			std::function<IUIManager*(GraphicsSystem* gs)>	ui_manager_allocator;
			std::string resources_dir = "./";

			void persist(Archive* archive);

		};
		struct _PAKALExport IUpdatable
		{
			virtual ~IUpdatable() {}
			virtual void update(unsigned dt) = 0;
		};		

		virtual void add_to_update_list(IUpdatable *updatable);
		virtual void remove_from_update_list(IUpdatable *updatable);

	protected:
		Settings			m_settings;
		IUIManager*			m_ui_manager;

		explicit GraphicsSystem(const Settings& settings);
		virtual ~GraphicsSystem();

		virtual void on_init_graphics(const WindowArgs& args) = 0;
		virtual void on_update_graphics(long long dt) = 0;
		virtual void on_terminate_graphics() = 0;
		virtual void on_pause_graphics() = 0;
		virtual void on_resume_graphics() = 0;
	private:
		std::mutex					m_updatablesMutex;
		std::vector<IUpdatable*>	m_updatables;
	};
}