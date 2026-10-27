#pragma once

#include "Config.h"
#include "EventArgs.h"

#include <bgfx/bgfx.h>

#include <functional>
#include <mutex>
#include <vector>

namespace Pakal
{
	class IInputManager;

	// Dear ImGui over bgfx, for development tools: panels that games and the engine register (stats, switches, cheats,
	// the future level editor). It draws in its own view on top of everything. Hidden until toggled (F1, or a tap with
	// three fingers on touch screens); PAKAL_DEV_UI=1 shows it from the start.
	class _PAKALExport BgfxDebugUI
	{
	public:
		using Panel = std::function<void()>;

		void initialize(bgfx::ViewId view, unsigned height);
		void terminate();

		// Listens to the mouse, keys and text of the input manager (the events may come from another thread)
		void attach_input(IInputManager* input);
		void detach_input();

		// Builds and draws one frame of the panels (call between the scene and bgfx::frame)
		void render(unsigned width, unsigned height, bgfx::FrameBufferHandle target);

		// A panel is a function that makes ImGui calls (windows, buttons, ...); it runs once per frame while visible, on the
		// graphics thread (on Android not the game's: a panel that reads the game's state must lock it)
		unsigned add_panel(Panel panel);
		void remove_panel(unsigned id);

		void set_visible(bool visible) { m_visible = visible; }
		bool is_visible() const { return m_visible; }
		void toggle() { m_visible = !m_visible; }

		// While true, the game should ignore the mouse/keys (the pointer is over a window, a text field has focus)
		bool wants_mouse() const { return m_visible && m_wants_mouse; }
		bool wants_keyboard() const { return m_visible && m_wants_keyboard; }

		// Touch screens (the platform calls these): a press, a move and a release of the pointer, in pixels of the render
		void pointer(int x, int y, bool down);
		void pointer_move(int x, int y);
		// Whether a point (pixels of the render) is on one of the panels' windows, as they were drawn last
		bool is_over(int x, int y) const;

	private:
		struct InputEvent
		{
			enum class Type { Move, Button, Wheel, Key, Text } type;
			int x = 0, y = 0, button = 0;
			bool down = false;
			float wheel = 0.0f;
			Key key = Key::Unknown;
			unsigned text = 0;
		};
		void push(const InputEvent& event);
		void feed_events();

		bool m_initialized = false;
		bool m_visible = false;
		bool m_wants_mouse = false;
		bool m_wants_keyboard = false;
		bgfx::ViewId m_view = 0;
		float m_wheel = 0.0f;
		int m_mouse_x = 0, m_mouse_y = 0;
		uint8_t m_buttons = 0;

		mutable std::mutex m_mutex;
		std::vector<InputEvent> m_events;
		struct Rect { float x0, y0, x1, y1; };
		std::vector<Rect> m_windows;

		struct Entry { unsigned id; Panel panel; };
		std::vector<Entry> m_panels;
		// Held while the panels run: once remove_panel returns (from any thread), that panel is not running and will not run
		std::recursive_mutex m_panels_mutex;
		unsigned m_next_id = 1;

		IInputManager* m_input = nullptr;
		unsigned m_ids[7] = {};
	};
}
