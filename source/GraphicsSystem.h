#pragma once
#include <cstdint>
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

		// Ground cover: one model drawn at many places in one instanced draw (a patch of grass, its plants and stones).
		// 'matrices' are the copies' world transforms, 16 floats each (column-major), copied in. A copy farther than 'reach'
		// meters from the focus is not drawn, and grows out of the ground over the last 'grow' meters of it; it casts no
		// shadow. Any thread: the work is posted to the graphics thread, and the model is read there, by its first patch (a
		// level makes its cover behind its loading screen). The handle is a plain id: 0 where the platform draws no cover. A
		// model that cannot be read is logged, and its handle stays valid and draws nothing. destroy_scatter frees the patch,
		// and its model with the last patch of it (a level's unload must destroy each one it made).
		using ScatterHandle = uint32_t;
		virtual ScatterHandle create_scatter(const std::string& modelPath, std::vector<float> matrices, float reach, float grow)
		{ (void)modelPath; (void)matrices; (void)reach; (void)grow; return 0; }
		virtual void destroy_scatter(ScatterHandle handle) { (void)handle; }
		// Where the reach is measured from: what the camera looks at (a top-down camera's eye is tens of meters up and back).
		// Until it is set, the eye. Any thread.
		virtual void set_scatter_focus(float x, float y, float z) { (void)x; (void)y; (void)z; }
		// Every patch's reach times 'scale' (a quality level's: a cheap phone draws its cover nearer). Any thread.
		virtual void set_scatter_scale(float scale) { (void)scale; }
		// The share of every patch's copies drawn, 0..1 (always the same ones, so none blinks). Any thread.
		virtual void set_scatter_density(float density) { (void)density; }
		// Trees' leaf cards thinned with distance from the focus: all of them out to 'nearMeters', down to 'farShare' of them
		// from 'farMeters' on (a crown of thousands of overlapping alpha-tested cards still reads full with a share, and the
		// pixels shaded fall with it). 'nearShare': the share kept out to 'nearMeters' (from a top-down camera every crown in view
		// is near the focus). Both 1: off, the default. The sun's shadow map takes the same share at High. Any thread.
		virtual void set_foliage_lod(float nearMeters, float farMeters, float farShare, float nearShare = 1.0f)
		{ (void)nearMeters; (void)farMeters; (void)farShare; (void)nearShare; }

		// Grass made on the GPU (docs/GRASS.md): blades of a few vertices each, solid (no alpha test, so the GPU's hidden
		// surface removal works), drawn by tiles in a few instanced draws. Each blade's height, lean, color and wind are worked
		// out in the vertex shader from its tile's place and the field's maps; the blades thin out with distance from the
		// focus (set_scatter_focus) and end at 'reach', where the ground's own texture carries the grass. Render space
		// (x east, y up, z south), meters.
		struct GrassField
		{
			float minX = 0.0f, minZ = 0.0f, maxX = 0.0f, maxZ = 0.0f;   // the field's rectangle on the ground
			float groundY = 0.0f;              // the ground's height where 'heights' gives none
			// Where it grows, 0..255 per cell over the rectangle (row 0 at minZ, column 0 at minX); empty: everywhere. A cell's
			// value is the share of the blades kept there.
			std::vector<uint8_t> density;
			int densityWidth = 0, densityHeight = 0;
			// The ground's height over the rectangle (meters, row 0 at minZ); empty: groundY everywhere
			std::vector<float> heights;
			int heightsWidth = 0, heightsHeight = 0;
			float bladesPerSquareMeter = 60.0f; // where the density is full, near the focus
			float heightMin = 0.2f, heightMax = 0.5f;   // a blade's height (m), clumps of taller and shorter ones
			float width = 0.08f;               // a blade's width at its root (m)
			float colorRoot[3] = { 0.16f, 0.27f, 0.08f };   // at the ground (the dark between the blades)
			float colorTip[3] = { 0.50f, 0.66f, 0.24f };    // at the tip, in most clumps
			// Clumps of another color: each takes the share of the clumps given (0: none), its tips this color (dry ochre,
			// olive...)
			struct ClumpColor { float color[3]; float share; };
			ClumpColor clumpColors[3] = { { { 0.62f, 0.56f, 0.30f }, 0.25f }, { { 0.0f, 0.0f, 0.0f }, 0.0f }, { { 0.0f, 0.0f, 0.0f }, 0.0f } };
			// Dead blades among the living ones, anywhere: their share and color
			float deadShare = 0.0f;
			float colorDead[3] = { 0.45f, 0.30f, 0.18f };
			// The clumps: their size (m) and how far a clump's height and color depart from the field's (0: an even lawn)
			float clumpSize = 2.2f;
			float clumpStrength = 0.6f;
			// Tufts: this many blades fan out from a shared root (0: blades stand alone, a lawn), 'tuftSpread' how far they
			// lean out (0..1)
			int tuftBlades = 0;
			float tuftSpread = 0.5f;
			float reach = 28.0f;               // meters from the focus where the last blades end
			// How the blades thin out with distance: all of them out to 'thinFrom' of the reach, then fewer, down to 'farDensity'
			// of them at the reach (the ones left are wider). Fewer far blades are cheaper: from a top-down camera they are a
			// pixel or two wide.
			float thinFrom = 0.15f;
			float farDensity = 0.12f;
		};
		using GrassHandle = uint32_t;
		// The field's data is copied in; any thread (the work goes to the graphics thread). The handle is a plain id, 0 for
		// none (a platform that draws no grass); a field that cannot be made is logged and draws nothing. destroy_grass frees
		// it (a level's unload destroys each one it made).
		virtual GrassHandle create_grass(GrassField field) { (void)field; return 0; }
		virtual void destroy_grass(GrassHandle handle) { (void)handle; }
		// Every field's reach and density times these (a quality level's: a cheap phone draws less grass, nearer; the
		// governor can shed it). Any thread.
		virtual void set_grass_scale(float reach, float density) { (void)reach; (void)density; }
		// Whether the blades read the sun's shadow (one read a blade; on by default, PAKAL_GRASS_SHADOWS=0 starts it off): a
		// quality level can turn it off. Any thread.
		virtual void set_grass_shadows(bool on) { (void)on; }

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