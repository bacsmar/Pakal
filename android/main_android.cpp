///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: main_android.cpp
// Purpose: Entry point of the Android build (NativeActivity + android_native_app_glue). It plays the part
//          PakalPlayerMain, SFML's window and SFML's input play on the desktop:
//            - copies the assets of the APK to the app's storage the first time (the engine reads files),
//            - gives bgfx the ANativeWindow,
//            - turns touches into the input the Zombies module reads: the left half of the screen is a
//              virtual stick, the right half aims and fires. A touch that lands on the game's interface (its
//              buttons) or on the development panels goes to them instead; three fingers at once show or hide
//              the development panels.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "AndroidAssetSource.h"
#include "Engine.h"
#include "GameModuleLoader.h"
#include "IInputManager.h"
#include "LogMgr.h"
#include "OSManager.h"
#include "ResourceManager.h"
#include "SoundManagerAAudio.h"
#include "IUIManager.h"
#include "bgfx/BgfxGraphicsSystem.h"

#include <android/asset_manager.h>
#include <android/input.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>
#include <sys/stat.h>
#include <unistd.h>

#include <array>
#include <atomic>
#include <algorithm>
#include <dlfcn.h>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace Pakal;

namespace
{
	android_app* g_app = nullptr;
	std::atomic<bool> g_window_ready{ false };
	Engine* g_engine = nullptr;        // set while the game runs
	bool g_resumed = true;             // the activity is in the foreground
	bool g_window_ok = true;           // there is a surface to draw on
	bool g_engine_running = true;      // the engine is not paused
	int g_render_height = 0;           // the height the game renders at (0: the screen's)
	// The window's size on the screen, in pixels (touches come in these). Not ANativeWindow_getWidth: once the buffers
	// have a geometry of their own (the render resolution) that returns the buffers' size.
	int g_screen_width = 0, g_screen_height = 0;
	int g_buffer_width = 0, g_buffer_height = 0; // the size the picture is drawn at
	// The picture is drawn smaller and scaled up to the screen by FSR 1 (the window's buffers are the screen's size, the
	// interface is drawn at it); PAKAL_FSR=0 leaves the scaling to the system compositor (bilinear, interface included)
	bool g_fsr = true;
	int g_target_fps = 60;   // the picture's rate (see the start)

	// Asks the screen for the picture's rate (Android 11+: phones with an adaptive refresh rate pick a mode that suits it
	// instead of guessing). Looked up at run time: the game runs on Android 8 too.
	void set_window_frame_rate(ANativeWindow* window, int fps)
	{
		using SetFrameRate = int32_t (*)(ANativeWindow*, float, int8_t);
		static const auto setFrameRate = reinterpret_cast<SetFrameRate>(dlsym(RTLD_DEFAULT, "ANativeWindow_setFrameRate"));
		if (setFrameRate && window)
		{
			setFrameRate(window, static_cast<float>(fps), 0 /* ANATIVEWINDOW_FRAME_RATE_COMPATIBILITY_DEFAULT */);
		}
	}
	void apply_run_state();
	void read_screen_size(android_app* app);
	void apply_render_geometry(android_app* app);

	// ---------------------------------------------------------------------------------------------
	// Input
	// ---------------------------------------------------------------------------------------------
	// Two virtual sticks: the left half of the screen moves, the right half aims (and fires when pushed far enough). Each stick
	// appears where the thumb lands. They are axis device 0: axes 0-1 the move stick, 2-3 the aim stick (x right, y up, length
	// up to 1, zero inside the dead zone).
	class TouchInputManager final : public IInputManager, public IAxisDevice
	{
	public:
		void initialize() override {}
		void terminate() override {}
		IButtonDevice* get_button_device(unsigned) const override { return nullptr; }
		IAxisDevice* get_axis_device(unsigned id) const override { return id == 0 ? const_cast<TouchInputManager*>(this) : nullptr; }
		void set_device_id(unsigned) override {}
		unsigned get_device_id() const override { return 0; }
		float get_axis_state(unsigned axis) override { return axis < m_axes.size() ? m_axes[axis].load() : 0.0f; }
		void map_axis(unsigned, unsigned) override {}
		ITouchDevice* get_touch_device(unsigned) const override { return nullptr; }
		void register_component_factories(std::vector<IComponentFactory*>&) override {}
		tmath::vector3df get_sensor_value(Sensors) const override { return tmath::vector3df(0.0f, 0.0f, 0.0f); }
		bool is_sensor_available(Sensors) const override { return false; }
		void set_sensor_enabled(Sensors, bool) override {}
		tmath::vector2di get_mouse_position() const override { return { 0, 0 }; }

		bool is_key_pressed(Key key) override
		{
			const int index = static_cast<int>(key);
			return index >= 0 && index < static_cast<int>(m_keys.size()) && m_keys[index].load();
		}

		// Touches come in screen pixels; the game renders at a lower resolution and aims in those pixels
		void set_aim_scale(float scale) { m_aim_scale = scale; }
		void set_touch_controls(bool shown) override { m_controls.store(shown); }
		void set_touch_aim(bool aims) override { m_aim.store(aims); }
		void set_game_touches(bool on) override { m_gameTouches.store(on); }
		tmath::vector2di get_touch_area() const override { return { g_screen_width, g_screen_height }; }

		// The touch controls drawn over the picture (pixels of the render): each stick is a ring with the thumb's disc in it, and
		// the strip that restarts the level is a square with an R. Hints stay while nothing touches them.
		void draw_overlay(BgfxGraphicsSystem& graphics, int screenWidth, int screenHeight)
		{
			if (!m_controls.load() || m_gameTouches.load()) return;   // a menu or a viewer, or the game draws its own
			const float s = static_cast<float>(graphics.get_height()) / static_cast<float>(screenHeight);
			const float w = static_cast<float>(screenWidth), h = static_cast<float>(screenHeight);
			const uint32_t dim = 0x55ffffff, bright = 0xe0ffffff;
			auto line = [&](float x0, float y0, float x1, float y1, uint32_t color) { graphics.draw_screen_line(x0 * s, y0 * s, x1 * s, y1 * s, color); };
			auto ring = [&](float cx, float cy, float radius, uint32_t color)
			{
				const int segments = 28;
				for (int i = 0; i < segments; ++i)
				{
					const float a0 = 6.2831853f * i / segments, a1 = 6.2831853f * (i + 1) / segments;
					line(cx + radius * std::cos(a0), cy + radius * std::sin(a0), cx + radius * std::cos(a1), cy + radius * std::sin(a1), color);
				}
			};

			const Touch* stick = nullptr;
			const Touch* aim = nullptr;
			for (const Touch& touch : m_touches)
			{
				if (touch.role == Role::Stick) stick = &touch;
				else if (touch.role == Role::Aim) aim = &touch;
			}

			// The thumb's disc moves at most 60% of the ring's radius
			auto draw_stick = [&](const Touch* touch, float hintX, float hintY, uint32_t active)
			{
				const float radius = stick_radius(h);
				if (!touch)
				{
					ring(hintX, hintY, radius, dim);
					ring(hintX, hintY, radius * 0.4f, dim);
					return;
				}
				ring(touch->origin_x, touch->origin_y, radius, bright);
				float dx = touch->x - touch->origin_x, dy = touch->y - touch->origin_y;
				const float length = std::sqrt(dx * dx + dy * dy), limit = radius * 0.6f;
				if (length > limit) { dx *= limit / length; dy *= limit / length; }
				ring(touch->origin_x + dx, touch->origin_y + dy, radius * 0.4f, active);
			};
			draw_stick(stick, w * 0.16f, h * 0.72f, bright);
			// The aim stick's disc turns red while it fires
			const bool firing = aim && std::hypot(m_axes[2].load(), m_axes[3].load()) > kFireThreshold;
			if (m_aim.load()) draw_stick(aim, w * 0.84f, h * 0.72f, firing ? 0xe04040ff : bright);
		}

		// Called from the looper thread with every motion event of the screen
		void on_motion(AInputEvent* event, int width, int height)
		{
			const int32_t action = AMotionEvent_getAction(event);
			const int32_t kind = action & AMOTION_EVENT_ACTION_MASK;
			const size_t changed = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

			if (kind == AMOTION_EVENT_ACTION_DOWN || kind == AMOTION_EVENT_ACTION_POINTER_DOWN)
			{
				begin_touch(AMotionEvent_getPointerId(event, changed), AMotionEvent_getX(event, changed), AMotionEvent_getY(event, changed), width, height);
			}
			else if (kind == AMOTION_EVENT_ACTION_UP || kind == AMOTION_EVENT_ACTION_POINTER_UP || kind == AMOTION_EVENT_ACTION_CANCEL)
			{
				if (kind == AMOTION_EVENT_ACTION_CANCEL)
				{
					release_all();
				}
				else
				{
					end_touch(AMotionEvent_getPointerId(event, changed));
				}
			}
			else if (kind == AMOTION_EVENT_ACTION_MOVE)
			{
				for (size_t i = 0; i < AMotionEvent_getPointerCount(event); ++i)
				{
					move_touch(AMotionEvent_getPointerId(event, i), AMotionEvent_getX(event, i), AMotionEvent_getY(event, i));
				}
			}
		}

		// A key the system gives (its Back, as Escape)
		void set_key(Key key, bool value) { m_keys[static_cast<int>(key)].store(value); }

	private:
		// Free: a finger off the buttons of a menu, for the state to read (event_touch_*: a viewer turns its model with it)
		enum class Role { None, Stick, Aim, Ui, Debug, Free };
		struct Touch
		{
			int32_t id = -1;
			Role role = Role::None;
			float origin_x = 0.0f, origin_y = 0.0f;
			float x = 0.0f, y = 0.0f; // where the finger is now
			float ui_dx = 0.0f, ui_dy = 0.0f; // a menu touch that landed just off a button: the shift onto it (render pixels)
			bool dragged = false;     // moved past the tap slop: a drag, not a tap
		};

		// A finger is not a mouse: it lands a little off target and slides while pressing. A menu touch that moves less than
		// this (a share of the screen's height) is a tap where it started, and one that lands this close to a button takes it.
		static constexpr float kTapSlop = 0.035f;

		// The point of the menus (render pixels) a touch at (rx, ry) means: itself, or the nearest one around it, within the
		// slop, that is over a button. False when none is.
		static bool ui_target(IUIManager* gui, int rx, int ry, float slop, int& outX, int& outY)
		{
			if (gui->is_over_ui(rx, ry)) { outX = rx; outY = ry; return true; }
			// Rings outward in small steps: the nearest button wins (a touch between two takes the closer one)
			for (int ringIndex = 1; ringIndex <= 6; ++ringIndex)
			{
				const float r = slop * ringIndex / 6.0f;
				for (int k = 0; k < 8; ++k)
				{
					const float a = 0.785398f * k;
					const int px = rx + static_cast<int>(r * std::cos(a)), py = ry + static_cast<int>(r * std::sin(a));
					if (gui->is_over_ui(px, py)) { outX = px; outY = py; return true; }
				}
			}
			return false;
		}

		// The sticks' size, as on the Unity version: a 120 px radius on a 1080 px tall screen
		static float stick_radius(float screenHeight) { return screenHeight * (120.0f / 1080.0f); }
		static constexpr float kDeadZone = 0.15f;      // of the radius
		static constexpr float kFireThreshold = 0.41f; // only for the drawing: the game decides when the aim stick fires

		// The stick's value from where the thumb is: the offset over the radius, at most 1, zero inside the dead zone
		void update_stick(const Touch& touch)
		{
			const size_t axis = touch.role == Role::Stick ? 0 : 2;
			float x = (touch.x - touch.origin_x) / stick_radius(m_screen_height);
			float y = (touch.origin_y - touch.y) / stick_radius(m_screen_height); // up on the screen is positive
			const float length = std::sqrt(x * x + y * y);
			if (length < kDeadZone) { x = 0.0f; y = 0.0f; }
			else if (length > 1.0f) { x /= length; y /= length; }
			m_axes[axis].store(x);
			m_axes[axis + 1].store(y);
		}

		// The interface's pixels: the back buffer's (the screen's when the picture is scaled up with FSR)
		int ui_x(float x) const { return static_cast<int>(x * m_ui_scale); }
		int ui_y(float y) const { return static_cast<int>(y * m_ui_scale); }
	public:
		void set_ui_scale(float scale) { m_ui_scale = scale; }
	private:
		static BgfxGraphicsSystem* graphics() { return g_engine ? static_cast<BgfxGraphicsSystem*>(g_engine->graphics_system()) : nullptr; }
		static IUIManager* ui() { return g_engine ? g_engine->get_ui_manager() : nullptr; }

		void begin_touch(int32_t id, float x, float y, int width, int height)
		{
			m_screen_height = static_cast<float>(height);
			Touch touch;
			touch.id = id;
			touch.x = x;
			touch.y = y;
			touch.origin_x = x;
			touch.origin_y = y;
			BgfxGraphicsSystem* g = graphics();
			IUIManager* gui = ui();
			const int rx = ui_x(x), ry = ui_y(y);
			const float slop = ui_y(kTapSlop * height);
			int ux = 0, uy = 0;
			if (m_touches.size() == 2 && g)
			{
				// A third finger: show or hide the development panels (the touch does nothing else)
				g->get_debug_ui().toggle();
				touch.role = Role::None;
			}
			else if (g && g->get_debug_ui().is_over(rx, ry))
			{
				touch.role = Role::Debug;
				g->get_debug_ui().pointer(rx, ry, true);
			}
			else if (gui && ui_target(gui, rx, ry, slop, ux, uy))
			{
				touch.role = Role::Ui;
				touch.ui_dx = static_cast<float>(ux - rx);
				touch.ui_dy = static_cast<float>(uy - ry);
				gui->touch(id, ux, uy, 0);
			}
			else if (!m_controls.load() || m_gameTouches.load())
			{
				touch.role = Role::Free;   // off the buttons: the state's (a drag, the game's own sticks)
				event_touch_began.notify(TouchArgs{ static_cast<unsigned>(id), int(x), int(y) });
			}
			else
			{
				touch.role = x < width * 0.5f ? Role::Stick : (m_aim.load() ? Role::Aim : Role::None);
				for (const Touch& other : m_touches)
				{
					if (other.role == touch.role) touch.role = Role::None; // one thumb per stick
				}
				// a finger landed while it is played (a skippable cutscene ends with it): told to the state as well
				event_touch_began.notify(TouchArgs{ static_cast<unsigned>(id), int(x), int(y) });
			}
			m_touches.push_back(touch);
		}

		void move_touch(int32_t id, float x, float y)
		{
			for (Touch& touch : m_touches)
			{
				if (touch.id != id) continue;
				touch.x = x;
				touch.y = y;
				if (touch.role == Role::Stick || touch.role == Role::Aim)
				{
					update_stick(touch);
					return;
				}
				if (touch.role == Role::Free)
				{
					event_touch_moved.notify(TouchArgs{ static_cast<unsigned>(id), int(x), int(y) });
					return;
				}
				// Menus and panels: the pointer stays where the finger landed until it moves past the slop (a drag)
				touch.dragged = touch.dragged || std::hypot(x - touch.origin_x, y - touch.origin_y) > kTapSlop * m_screen_height;
				if (!touch.dragged) return;
				if (touch.role == Role::Debug)
				{
					if (BgfxGraphicsSystem* g = graphics()) g->get_debug_ui().pointer_move(ui_x(x), ui_y(y));
				}
				else if (touch.role == Role::Ui)
				{
					if (IUIManager* gui = ui()) gui->touch(id, ui_x(x) + int(touch.ui_dx), ui_y(y) + int(touch.ui_dy), 1);
				}
				return;
			}
		}

		void end_touch(int32_t id)
		{
			for (size_t i = 0; i < m_touches.size(); ++i)
			{
				if (m_touches[i].id != id) continue;
				finish(m_touches[i]);
				m_touches.erase(m_touches.begin() + i);
				return;
			}
		}

		void release_all()
		{
			for (Touch& touch : m_touches) finish(touch);
			m_touches.clear();
		}

		void finish(const Touch& touch)
		{
			switch (touch.role)
			{
			case Role::Stick:
				m_axes[0].store(0.0f);
				m_axes[1].store(0.0f);
				break;
			case Role::Aim:
				m_axes[2].store(0.0f);
				m_axes[3].store(0.0f);
				break;
			case Role::Debug:
			case Role::Ui:
			{
				// A tap lets go where it pressed: on the same button, so it counts as a click
				const float fx = touch.dragged ? touch.x : touch.origin_x, fy = touch.dragged ? touch.y : touch.origin_y;
				if (touch.role == Role::Debug)
				{
					if (BgfxGraphicsSystem* g = graphics()) g->get_debug_ui().pointer(ui_x(fx), ui_y(fy), false);
				}
				else if (IUIManager* gui = ui())
				{
					gui->touch(touch.id, ui_x(fx) + int(touch.ui_dx), ui_y(fy) + int(touch.ui_dy), 2);
				}
				break;
			}
			case Role::Free:
				event_touch_ended.notify(TouchArgs{ static_cast<unsigned>(touch.id), int(touch.x), int(touch.y) });
				break;
			default:
				break;
			}
		}

		std::array<std::atomic<bool>, 256> m_keys{};
		std::atomic<bool> m_controls{ false };   // the sticks, while a level is played (set_touch_controls)
		std::atomic<bool> m_aim{ true };         // the right half aims (set_touch_aim)
		std::atomic<bool> m_gameTouches{ false }; // the game reads the fingers itself (set_game_touches)
		std::array<std::atomic<float>, 4> m_axes{};
		float m_screen_height = 1080.0f;
		std::vector<Touch> m_touches;
		float m_aim_scale = 1.0f;
		float m_ui_scale = 1.0f;
	};

	TouchInputManager* g_input = nullptr;
	float g_pending_scale = 1.0f;

	// ---------------------------------------------------------------------------------------------
	// Window: the activity's surface
	// ---------------------------------------------------------------------------------------------
	class NativeWindowManager final : public OSManager::IWindowManager
	{
	public:
		explicit NativeWindowManager(OSManager* os) : m_os(os) {}

		WindowArgs setup_window(unsigned, const tmath::vector2di&, bool, unsigned) override
		{
			// The window can go between the start and here (the phone locks as the game starts): bgfx must not get one that
			// is gone, so wait until there is one again
			bool waited = false;
			while (!g_window_ready && !g_app->destroyRequested)
			{
				waited = true;
				int events = 0;
				android_poll_source* source = nullptr;
				if (ALooper_pollOnce(-1, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0 && source) source->process(g_app, source);
			}
			if (waited && g_app->window)
			{
				LOG_INFO("[Android] the window is back");
				read_screen_size(g_app);
				apply_render_geometry(g_app);   // its shape may have changed (locked in portrait, back in landscape)
			}
			WindowArgs args;
			args.windowId = 1; // nonzero: there is a window
			args.native_window = g_app->window;
			args.size_x = static_cast<unsigned>(ANativeWindow_getWidth(g_app->window));
			args.size_y = static_cast<unsigned>(ANativeWindow_getHeight(g_app->window));
			return args;
		}

		void close_window() override {}

		void process_window_events() override
		{
			int events = 0;
			android_poll_source* source = nullptr;
			while (ALooper_pollOnce(0, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0)
			{
				if (source) source->process(g_app, source);
				if (g_app->destroyRequested)
				{
					m_os->event_app_finished();
					return;
				}
			}
			apply_run_state(); // a pause or resume that arrived while the engine was not ready
			// The touch controls over the picture (not while the engine is paused: there is no surface)
			if (g_engine && g_engine_running && g_input && g_app->window)
			{
				if (auto* graphics = static_cast<BgfxGraphicsSystem*>(g_engine->graphics_system()))
				{
					g_input->draw_overlay(*graphics, g_screen_width, g_screen_height);
				}
			}
		}

		void set_title(const std::string&) override {}
		tmath::vector2di get_position() override { return { 0, 0 }; }
		tmath::vector2di get_size() override
		{
			return { ANativeWindow_getWidth(g_app->window), ANativeWindow_getHeight(g_app->window) };
		}

	private:
		OSManager* m_os;
	};

	// The game runs while the app is in front and has a surface; otherwise the engine is paused (a lost surface cannot be drawn on)
	void apply_run_state()
	{
		if (!g_engine) return;
		const bool shouldRun = g_resumed && g_window_ok;
		if (shouldRun == g_engine_running) return;
		// While the engine is still starting (or already pausing/resuming) the change waits: it is applied on a later
		// frame (process_window_events calls this every time). Pausing during the start crashed (the systems are not
		// running yet), e.g. when the screen goes off while the level loads.
		auto* graphics = g_engine->graphics_system();
		const SystemState wanted = shouldRun ? SystemState::Paused : SystemState::Running;
		if (g_engine->get_state() != wanted || !graphics || graphics->get_state() != wanted) return;
		g_engine_running = shouldRun;
		if (shouldRun)
		{
			LOG_INFO("[Android] resuming");
			g_engine->resume();
		}
		else
		{
			LOG_INFO("[Android] pausing");
			g_engine->pause();
		}
	}

	// The window's size on the screen
	void read_screen_size(android_app* app)
	{
		// The window's own size, not the content rect: the window covers the screen under the navigation bar, and the
		// compositor stretches the buffers over all of it. Buffers the content rect's height (a few percent shorter on a screen with a navigation bar)
		// came out stretched 7% and touches landed off what was drawn, more the lower on the screen.
		// Geometry 0 x 0 gives the window's size back (with a geometry set, the window reports the buffers').
		if (app->window)
		{
			ANativeWindow_setBuffersGeometry(app->window, 0, 0, 0);
			const int width = ANativeWindow_getWidth(app->window), height = ANativeWindow_getHeight(app->window);
			if (width > 0 && height > 0)
			{
				g_screen_width = width;
				g_screen_height = height;
			}
		}
	}

	// The buffers: the screen's size when FSR scales the picture up to it, else the render size (the compositor stretches it)
	void set_buffers_geometry(android_app* app)
	{
		const int height = g_render_height > 0 && g_render_height < g_screen_height ? g_render_height : g_screen_height;
		const int width = static_cast<int>(static_cast<float>(g_screen_width) * height / g_screen_height);
		if (g_fsr) ANativeWindow_setBuffersGeometry(app->window, g_screen_width, g_screen_height, 0);
		else ANativeWindow_setBuffersGeometry(app->window, width, height, 0);
	}

	// Gives the buffers the render resolution (the screen's shape, PAKAL_RENDER_HEIGHT tall) and, when the game runs, hands
	// the surface to the graphics again: a new window (back from the background) or the same one with another shape (the
	// game started in portrait, with the phone locked, and turned to landscape)
	void apply_render_geometry(android_app* app)
	{
		if (!app->window || g_screen_width <= 0 || g_screen_height <= 0) return;
		const int height = g_render_height > 0 && g_render_height < g_screen_height ? g_render_height : g_screen_height;
		const int width = static_cast<int>(static_cast<float>(g_screen_width) * height / g_screen_height);
		// with FSR the buffers are the screen's even when the picture is drawn at its size: the render scale can go down later
		const bool upscale = g_fsr;
		set_window_frame_rate(app->window, g_target_fps);
		set_buffers_geometry(app);
		g_buffer_width = width;
		g_buffer_height = height;
		if (g_input)
		{
			g_input->set_aim_scale(static_cast<float>(height) / g_screen_height);
			g_input->set_ui_scale(upscale ? 1.0f : static_cast<float>(height) / g_screen_height);
		}
		LOG_INFO("[Android] screen %dx%d, rendering %dx%d%s", g_screen_width, g_screen_height, width, height, upscale ? " (FSR 1 up to the screen)" : "");
		if (!g_engine) return;
		if (auto* graphics = static_cast<BgfxGraphicsSystem*>(g_engine->graphics_system()))
		{
			graphics->set_native_window(app->window, static_cast<unsigned>(width), static_cast<unsigned>(height),
				upscale ? static_cast<unsigned>(g_screen_width) : 0u, upscale ? static_cast<unsigned>(g_screen_height) : 0u);
		}
	}

	// The window changed shape: new geometry when the screen's aspect is not the buffers' any more
	void on_window_shape(android_app* app)
	{
		const int oldWidth = g_screen_width, oldHeight = g_screen_height;
		read_screen_size(app);
		if (g_screen_width != oldWidth || g_screen_height != oldHeight)
		{
			apply_render_geometry(app);
		}
		else if (app->window)
		{
			set_buffers_geometry(app); // reading the size took the geometry away
		}
	}

	void on_command(android_app* app, int32_t command)
	{
		switch (command)
		{
		case APP_CMD_INIT_WINDOW:
			g_window_ready = app->window != nullptr;
			if (g_engine)
			{
				read_screen_size(app);
				apply_render_geometry(app); // coming back from the background
				g_window_ok = app->window != nullptr;
				apply_run_state();
			}
			break;
		case APP_CMD_TERM_WINDOW:
			g_window_ready = false;
			LOG_INFO("[Android] window lost");
			g_window_ok = false;
			apply_run_state();
			break;
		case APP_CMD_PAUSE:
			g_resumed = false;
			apply_run_state();
			break;
		case APP_CMD_RESUME:
			g_resumed = true;
			apply_run_state();
			break;
		case APP_CMD_CONTENT_RECT_CHANGED:
		case APP_CMD_WINDOW_RESIZED:
		case APP_CMD_CONFIG_CHANGED:
			on_window_shape(app);
			break;
		case APP_CMD_GAINED_FOCUS:
			OSMgr.event_window_focused(true);
			break;
		case APP_CMD_LOST_FOCUS:
			OSMgr.event_window_focused(false);
			break;
		default:
			break;
		}
	}

	int32_t on_input(android_app* app, AInputEvent* event)
	{
		// The system's Back is the game's Escape (each screen says what it does: back one screen, the pause); left to the
		// system it sent the app to the background from any screen
		if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY && AKeyEvent_getKeyCode(event) == AKEYCODE_BACK && g_input)
		{
			const int32_t action = AKeyEvent_getAction(event);
			if (action == AKEY_EVENT_ACTION_DOWN) g_input->set_key(Key::Escape, true);
			else if (action == AKEY_EVENT_ACTION_UP) g_input->set_key(Key::Escape, false);
			return 1;
		}
		if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION || !g_input || !app->window)
		{
			return 0;
		}
		g_input->on_motion(event, g_screen_width, g_screen_height);
		return 1;
	}

	// ---------------------------------------------------------------------------------------------
	// Assets: the engine reads files, so the APK's assets are copied once to the app's storage
	// ---------------------------------------------------------------------------------------------
	bool make_directories(const std::string& file_path)
	{
		for (size_t at = file_path.find('/', 1); at != std::string::npos; at = file_path.find('/', at + 1))
		{
			mkdir(file_path.substr(0, at).c_str(), 0771);
		}
		return true;
	}

	bool read_asset_text(AAssetManager* assets, const char* name, std::string& text)
	{
		AAsset* asset = AAssetManager_open(assets, name, AASSET_MODE_BUFFER);
		if (!asset) return false;
		text.assign(static_cast<const char*>(AAsset_getBuffer(asset)), static_cast<size_t>(AAsset_getLength(asset)));
		AAsset_close(asset);
		return true;
	}

	bool copy_asset(AAssetManager* assets, const std::string& name, const std::string& target)
	{
		AAsset* asset = AAssetManager_open(assets, name.c_str(), AASSET_MODE_STREAMING);
		if (!asset) return false;
		make_directories(target);
		std::ofstream out(target, std::ios::binary | std::ios::trunc);
		std::vector<char> chunk(1 << 16);
		int read = 0;
		while ((read = AAsset_read(asset, chunk.data(), chunk.size())) > 0)
		{
			out.write(chunk.data(), read);
		}
		AAsset_close(asset);
		return static_cast<bool>(out);
	}

	// assets.list (made when the APK is packaged) names every file; its text is the version of the content
	void extract_assets(android_app* app, const std::string& root)
	{
		std::string list;
		if (!read_asset_text(app->activity->assetManager, "assets.list", list))
		{
			LOG_ERROR("[Android] assets.list is not in the APK");
			return;
		}
		const std::string stamp_path = root + "/assets.stamp";
		std::string stamp;
		{
			std::ifstream in(stamp_path, std::ios::binary);
			std::ostringstream text;
			text << in.rdbuf();
			stamp = text.str();
		}
		if (stamp == list)
		{
			LOG_INFO("[Android] assets already extracted");
			return;
		}
		// The lines of the last extraction ("path<TAB>md5"): a file whose line is unchanged is already there
		std::set<std::string> known;
		{
			std::istringstream before(stamp);
			for (std::string line; std::getline(before, line);) known.insert(line);
		}
		size_t count = 0;
		std::set<std::string> listed;
		std::istringstream lines(list);
		for (std::string line; std::getline(lines, line);)
		{
			while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
			if (line.empty()) continue;
			const std::string path = line.substr(0, line.find('\t'));
			listed.insert(path);
			struct stat info;
			if (known.count(line) && stat((root + "/" + path).c_str(), &info) == 0) continue;
			if (!copy_asset(app->activity->assetManager, path, root + "/" + path))
			{
				LOG_ERROR("[Android] could not extract %s", path.c_str());
			}
			++count;
		}
		// Files of the last extraction that the package no longer has (textures replaced by their packed copies) go away
		size_t removed = 0;
		for (const std::string& line : known)
		{
			const std::string path = line.substr(0, line.find('\t'));
			if (!path.empty() && !listed.count(path) && std::remove((root + "/" + path).c_str()) == 0) ++removed;
		}
		std::ofstream(stamp_path, std::ios::binary | std::ios::trunc) << list;
		LOG_INFO("[Android] %zu assets extracted to %s, %zu old ones removed", count, root.c_str(), removed);
	}

	// The copies an earlier version extracted (assets.list's files under the app's storage) go away once: read before the
	// APK's, an old copy would hide the new one
	void forget_extracted_assets(const std::string& root)
	{
		const std::string stamp_path = root + "/assets.stamp";
		std::ifstream in(stamp_path, std::ios::binary);
		if (!in) return;
		size_t removed = 0;
		for (std::string line; std::getline(in, line);)
		{
			const std::string path = line.substr(0, line.find('\t'));
			if (!path.empty() && std::remove((root + "/" + path).c_str()) == 0) ++removed;
		}
		in.close();
		std::remove(stamp_path.c_str());
		LOG_INFO("[Android] %zu extracted assets removed: the game reads them from the APK", removed);
	}

	// The activity's share sheet for a file (PakalActivity.shareFile), from any thread
	bool share_with_activity(const std::string& path, const std::string& title)
	{
		if (!g_app || !g_app->activity) return false;
		JavaVM* vm = g_app->activity->vm;
		JNIEnv* env = nullptr;
		bool attached = false;
		if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK)
		{
			if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return false;
			attached = true;
		}
		bool shared = false;
		jobject activity = g_app->activity->clazz;
		jclass type = env->GetObjectClass(activity);
		jmethodID method = env->GetMethodID(type, "shareFile", "(Ljava/lang/String;Ljava/lang/String;)V");
		if (method)
		{
			jstring jpath = env->NewStringUTF(path.c_str());
			jstring jtitle = env->NewStringUTF(title.c_str());
			env->CallVoidMethod(activity, method, jpath, jtitle);
			shared = !env->ExceptionCheck();
			env->DeleteLocalRef(jpath);
			env->DeleteLocalRef(jtitle);
		}
		if (env->ExceptionCheck()) env->ExceptionClear();
		env->DeleteLocalRef(type);
		if (attached) vm->DetachCurrentThread();
		LOG_INFO("[Android] share %s: %s", path.c_str(), shared ? "the share sheet" : "failed");
		return shared;
	}

	// Debug switches without a shell: KEY=VALUE lines of <external files dir>/pakal_env.txt become environment variables
	// (ZOMBIES_POST=off, ZOMBIES_SHADOWS=off, ZOMBIES_GI=off, PAKAL_FPS_LOG=1, ...)
	void load_env_file(android_app* app)
	{
		const char* external = app->activity->externalDataPath;
		if (!external) return;
		std::ifstream in(std::string(external) + "/pakal_env.txt");
		for (std::string line; std::getline(in, line);)
		{
			while (!line.empty() && (line.back() == 0x0D || line.back() == ' ')) line.pop_back();
			const size_t equals = line.find('=');
			if (equals == std::string::npos || line[0] == '#') continue;
			setenv(line.substr(0, equals).c_str(), line.substr(equals + 1).c_str(), 1);
			LOG_INFO("[Android] env %s", line.c_str());
		}
	}

	void run_game(android_app* app)
	{
		load_env_file(app);
		const std::string root = app->activity->internalDataPath;
		// The game reads its files straight from the APK (AndroidAssetSource, through the resource manager); the app's
		// storage keeps only what it writes (saves). PAKAL_EXTRACT=1 copies them out first, as before.
		const char* extractText = std::getenv("PAKAL_EXTRACT");
		if (extractText && std::string(extractText) == "1")
		{
			extract_assets(app, root);
		}
		else
		{
			forget_extracted_assets(root);
		}
		chdir(root.c_str());
		ResourceMgr.set_root_path(root);
		if (!ResourceMgr.has_source_factory<AndroidAssetSource>())
		{
			ResourceMgr.register_source<AndroidAssetSource>([]() { return new AndroidAssetSource(); });
		}
		ResourceMgr.add_source<AndroidAssetSource>(app->activity->assetManager, root);
		// The game opens on its menu; ZOMBIES_LEVEL in pakal_env.txt (a full path) plays one level on its own instead

		if (!app->window)
		{
			LOG_ERROR("[Android] no window to start");
			return;
		}
		// Render at a lower resolution than the screen and scale it up with FSR 1: two thirds of the screen's height (FSR's
		// quality mode), at most 720 px (1080 at Ultra: big screens with a strong GPU). A phone left with its screen at HD+
		// (720 lines) draws 480. PAKAL_RENDER_HEIGHT (pixels) sets the height.
		const char* heightText = std::getenv("PAKAL_RENDER_HEIGHT");
		const bool ultra = BgfxGraphicsSystem::default_quality() == BgfxGraphicsSystem::Quality::Ultra;
		if (const char* fsr = std::getenv("PAKAL_FSR")) g_fsr = std::string(fsr) != "0";
		read_screen_size(app);
		const int cap = ultra ? 1080 : 720;
		g_render_height = heightText ? std::atoi(heightText) : (g_fsr ? std::min(cap, (g_screen_height * 2 + 2) / 3) : cap);
		// The picture starts at 60 and the governor (BgfxGraphicsSystem) steps it down to 30, then the render scale, when the
		// phone cannot hold it: some phones cut their GPU's clock to less than half when it works flat out (at a steady 30
		// they keep it), others hold 60. 30 divides the screen's 60 Hz, so every
		// frame stays on screen as long. PAKAL_MAX_FPS fixes the cap (and turns the governor off); PAKAL_GOVERNOR=0 too.
		g_target_fps = 60;
		bool governor = true;
		if (const char* fps = std::getenv("PAKAL_MAX_FPS")) { g_target_fps = std::max(15, std::atoi(fps)); governor = false; }
		if (const char* text = std::getenv("PAKAL_GOVERNOR")) governor = governor && std::string(text) != "0";
		apply_render_geometry(app);
		const int width = g_buffer_width, height = g_buffer_height;
		g_pending_scale = static_cast<float>(height) / g_screen_height;

		Engine::Settings settings;
		// The game's loop runs at 120 Hz: the picture (60 Hz, vsync) then finds positions at most ~8 ms old
		settings.max_fps = 120;
		settings.graphic_system_settings.max_fps = static_cast<unsigned>(g_target_fps);
		settings.graphic_system_settings.governor = governor;
		settings.graphic_system_settings.resolution = { width, height };
		if (g_fsr) settings.graphic_system_settings.output_resolution = { g_screen_width, g_screen_height };
		// the player's FSR option (the game's options; on at first): with it the height above, without it the screen's up to the
		// cap. Both are of the output: the screen when this launch upscales; the buffers themselves when it does not (they are
		// already that height: the screen's scale again drew 720 lines at 480), and FSR turned on later draws two thirds of them
		settings.graphic_system_settings.fsr_scale = g_fsr ? static_cast<float>(height) / static_cast<float>(g_screen_height) : 2.0f / 3.0f;
		settings.graphic_system_settings.plain_scale = g_fsr ? static_cast<float>(std::min(cap, g_screen_height)) / static_cast<float>(g_screen_height) : 1.0f;
		settings.graphic_system_settings.vsync = true;
		settings.graphic_system_settings.full_screen = false;
		settings.physic_system_settings.gravity = tmath::vector3df(0.0f, -20.0f, 0.0f);
		settings.physic_system_settings.max_fps = 120;   // one 8 ms step per update (Box2DPhysicsSystem)
		settings.sound_manager_allocator = [](Engine*) { return static_cast<SoundManager*>(new SoundManagerAAudio()); };
		settings.os_manager_settings.input_manager_allocator = [](OSManager*)
		{
			g_input = new TouchInputManager();
			g_input->set_aim_scale(g_pending_scale);
			g_input->set_ui_scale(g_fsr ? 1.0f : g_pending_scale);
			return static_cast<IInputManager*>(g_input);
		};
		settings.os_manager_settings.window_manager_allocator = [](OSManager* os)
		{
			return static_cast<OSManager::IWindowManager*>(new NativeWindowManager(os));
		};

		Engine engine(settings);
		// files for the person to take away (a profiling session) go to the app's external folder, and are handed over
		// with the share sheet (PakalActivity.shareFile: a copy in Downloads, then e-mail, chat...)
		if (g_app->activity->externalDataPath) OSMgr.set_data_folder(std::string(g_app->activity->externalDataPath) + "/");
		OSMgr.set_share_handler([](const std::string& path, const std::string& title) { return share_with_activity(path, title); });
		GameModuleLoader module;
		if (!module.load("static"))
		{
			LOG_ERROR("[Android] the game module did not load");
			return;
		}
		module.register_game_factories(&engine);
		g_engine = &engine;
		engine.run(module.application());
		g_engine = nullptr;
	}
}

void android_main(android_app* app)
{
	g_app = app;
	app->onAppCmd = on_command;
	app->onInputEvent = on_input;

	// Wait for the surface: the engine creates its graphics from it
	while (!g_window_ready && !app->destroyRequested)
	{
		int events = 0;
		android_poll_source* source = nullptr;
		if (ALooper_pollOnce(-1, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0 && source)
		{
			source->process(app, source);
		}
	}
	if (!app->destroyRequested)
	{
		try
		{
			run_game(app);
		}
		catch (const std::exception& e)
		{
			LOG_ERROR("[Android] fatal: %s", e.what());
		}
	}
	ANativeActivity_finish(app->activity);
	// native_app_glue needs the thread to keep polling until the activity is destroyed
	while (!app->destroyRequested)
	{
		int events = 0;
		android_poll_source* source = nullptr;
		if (ALooper_pollOnce(-1, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0 && source)
		{
			source->process(app, source);
		}
	}
	// The game is over with its activity: the process ends here. Left to linger, Android 8 crashed it seconds
	// later in a binder thread letting go of an object already gone ("the app stopped" after quitting)
	LOG_INFO("[Android] the activity is gone: the process ends");
	_exit(0);
}
