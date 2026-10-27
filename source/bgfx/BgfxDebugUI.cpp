#include "BgfxDebugUI.h"

#include "IInputManager.h"
#include "LogMgr.h"

#include <dear-imgui/imgui.h>
#include <dear-imgui/imgui_internal.h>
#include <imgui/imgui.h>   // bgfx's ImGui renderer (external/bgfx/examples/common/imgui)

#include <algorithm>
#include <cstdlib>

namespace Pakal
{
	namespace
	{
		ImGuiKey to_imgui(Key key)
		{
			const int k = static_cast<int>(key);
			if (k >= static_cast<int>(Key::A) && k <= static_cast<int>(Key::Z)) return ImGuiKey(ImGuiKey_A + (k - static_cast<int>(Key::A)));
			if (k >= static_cast<int>(Key::Num0) && k <= static_cast<int>(Key::Num9)) return ImGuiKey(ImGuiKey_0 + (k - static_cast<int>(Key::Num0)));
			if (k >= static_cast<int>(Key::F1) && k <= static_cast<int>(Key::F12)) return ImGuiKey(ImGuiKey_F1 + (k - static_cast<int>(Key::F1)));
			switch (key)
			{
			case Key::Escape: return ImGuiKey_Escape;
			case Key::Return: return ImGuiKey_Enter;
			case Key::Tab: return ImGuiKey_Tab;
			case Key::BackSpace: return ImGuiKey_Backspace;
			case Key::Space: return ImGuiKey_Space;
			case Key::Delete: return ImGuiKey_Delete;
			case Key::Insert: return ImGuiKey_Insert;
			case Key::Home: return ImGuiKey_Home;
			case Key::End: return ImGuiKey_End;
			case Key::PageUp: return ImGuiKey_PageUp;
			case Key::PageDown: return ImGuiKey_PageDown;
			case Key::Left: return ImGuiKey_LeftArrow;
			case Key::Right: return ImGuiKey_RightArrow;
			case Key::Up: return ImGuiKey_UpArrow;
			case Key::Down: return ImGuiKey_DownArrow;
			case Key::LControl: return ImGuiKey_LeftCtrl;
			case Key::RControl: return ImGuiKey_RightCtrl;
			case Key::LShift: return ImGuiKey_LeftShift;
			case Key::RShift: return ImGuiKey_RightShift;
			case Key::LAlt: return ImGuiKey_LeftAlt;
			case Key::RAlt: return ImGuiKey_RightAlt;
			case Key::Comma: return ImGuiKey_Comma;
			case Key::Period: return ImGuiKey_Period;
			case Key::Slash: return ImGuiKey_Slash;
			case Key::Dash: return ImGuiKey_Minus;
			case Key::Equal: return ImGuiKey_Equal;
			default: return ImGuiKey_None;
			}
		}
	}

	void BgfxDebugUI::initialize(bgfx::ViewId view, unsigned height)
	{
		m_view = view;
		// The text is about 1/40 of the screen's height, never smaller than 16 px (a phone at 1080 lines: 27 px). On touch
		// screens the panels are for a finger: bigger (1/26 of the height) and every control takes touches a little around it
#if defined(__ANDROID__)
		const float fontSize = std::max(16.0f, static_cast<float>(height) / 26.0f);
#else
		const float fontSize = std::max(16.0f, static_cast<float>(height) / 40.0f);
#endif
		imguiCreate(fontSize);
		ImGuiStyle& style = ImGui::GetStyle();
		style.ScaleAllSizes(fontSize / 16.0f);
#if defined(__ANDROID__)
		style.TouchExtraPadding = ImVec2(fontSize * 0.35f, fontSize * 0.35f);
		style.ItemSpacing.y = fontSize * 0.5f;
		style.ScrollbarSize = fontSize * 1.2f;
#endif
		style.Alpha = 0.94f;
		const char* show = std::getenv("PAKAL_DEV_UI");
		m_visible = show && show[0] == '1';
		m_initialized = true;
		LOG_INFO("[DebugUI] ImGui %s ready (view %u, font %.0f px)", ImGui::GetVersion(), unsigned(view), fontSize);
	}

	void BgfxDebugUI::terminate()
	{
		detach_input();
		if (!m_initialized) return;
		m_panels.clear();
		imguiDestroy();
		m_initialized = false;
	}

	void BgfxDebugUI::attach_input(IInputManager* input)
	{
		detach_input();
		m_input = input;
		if (!input) return;
		m_ids[0] = input->event_mouse_moved.add_listener([this](const MouseArgs& a)
		{
			InputEvent e{ InputEvent::Type::Move }; e.x = int(a.x); e.y = int(a.y); push(e);
		});
		m_ids[1] = input->event_mouse_pressed.add_listener([this](const MouseArgs& a)
		{
			InputEvent e{ InputEvent::Type::Button }; e.x = int(a.x); e.y = int(a.y); e.button = int(a.button_id); e.down = true; push(e);
		});
		m_ids[2] = input->event_mouse_released.add_listener([this](const MouseArgs& a)
		{
			InputEvent e{ InputEvent::Type::Button }; e.x = int(a.x); e.y = int(a.y); e.button = int(a.button_id); e.down = false; push(e);
		});
		m_ids[3] = input->event_mouse_wheel.add_listener([this](const MouseArgs& a)
		{
			InputEvent e{ InputEvent::Type::Wheel }; e.wheel = a.delta; push(e);
		});
		m_ids[4] = input->event_key_down.add_listener([this](const KeyArgs& a)
		{
			if (a.key == Key::F1) { toggle(); return; }
			InputEvent e{ InputEvent::Type::Key }; e.key = a.key; e.down = true; push(e);
		});
		m_ids[5] = input->event_key_up.add_listener([this](const KeyArgs& a)
		{
			InputEvent e{ InputEvent::Type::Key }; e.key = a.key; e.down = false; push(e);
		});
		m_ids[6] = input->event_text.add_listener([this](const TextArgs& a)
		{
			InputEvent e{ InputEvent::Type::Text }; e.text = a.unicode; push(e);
		});
	}

	void BgfxDebugUI::detach_input()
	{
		if (!m_input) return;
		m_input->event_mouse_moved.remove_listener(m_ids[0]);
		m_input->event_mouse_pressed.remove_listener(m_ids[1]);
		m_input->event_mouse_released.remove_listener(m_ids[2]);
		m_input->event_mouse_wheel.remove_listener(m_ids[3]);
		m_input->event_key_down.remove_listener(m_ids[4]);
		m_input->event_key_up.remove_listener(m_ids[5]);
		m_input->event_text.remove_listener(m_ids[6]);
		m_input = nullptr;
	}

	void BgfxDebugUI::pointer(int x, int y, bool down)
	{
		InputEvent move{ InputEvent::Type::Move }; move.x = x; move.y = y; push(move);
		InputEvent button{ InputEvent::Type::Button }; button.x = x; button.y = y; button.down = down; push(button);
	}

	void BgfxDebugUI::pointer_move(int x, int y)
	{
		InputEvent move{ InputEvent::Type::Move }; move.x = x; move.y = y; push(move);
	}

	void BgfxDebugUI::push(const InputEvent& event)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_events.push_back(event);
	}

	void BgfxDebugUI::feed_events()
	{
		std::vector<InputEvent> events;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			events.swap(m_events);
		}
		ImGuiIO& io = ImGui::GetIO();
		// ImGui sees one pointer state per frame: a press and its release in the same frame (a quick tap) would cancel out.
		// After a button changes, the rest waits for the next frame.
		size_t used = 0;
		for (; used < events.size(); ++used)
		{
			const InputEvent& e = events[used];
			if (e.type == InputEvent::Type::Button && used > 0 && std::any_of(events.begin(), events.begin() + used,
				[](const InputEvent& before) { return before.type == InputEvent::Type::Button; }))
			{
				break;
			}
			switch (e.type)
			{
			case InputEvent::Type::Move:
				m_mouse_x = e.x; m_mouse_y = e.y;
				break;
			case InputEvent::Type::Button:
				m_mouse_x = e.x; m_mouse_y = e.y;
				{
					const uint8_t bit = e.button == 0 ? IMGUI_MBUT_LEFT : e.button == 1 ? IMGUI_MBUT_RIGHT : IMGUI_MBUT_MIDDLE;
					m_buttons = e.down ? uint8_t(m_buttons | bit) : uint8_t(m_buttons & ~bit);
				}
				break;
			case InputEvent::Type::Wheel:
				m_wheel += e.wheel;
				break;
			case InputEvent::Type::Key:
				if (const ImGuiKey key = to_imgui(e.key); key != ImGuiKey_None) io.AddKeyEvent(key, e.down);
				if (e.key == Key::LControl || e.key == Key::RControl) io.AddKeyEvent(ImGuiMod_Ctrl, e.down);
				if (e.key == Key::LShift || e.key == Key::RShift) io.AddKeyEvent(ImGuiMod_Shift, e.down);
				break;
			case InputEvent::Type::Text:
				if (e.text >= 32 && e.text != 127) io.AddInputCharacter(e.text);
				break;
			}
		}
		if (used < events.size())
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_events.insert(m_events.begin(), events.begin() + used, events.end());
		}
	}

	unsigned BgfxDebugUI::add_panel(Panel panel)
	{
		std::lock_guard<std::recursive_mutex> lock(m_panels_mutex);
		m_panels.push_back({ m_next_id, std::move(panel) });
		return m_next_id++;
	}

	void BgfxDebugUI::remove_panel(unsigned id)
	{
		std::lock_guard<std::recursive_mutex> lock(m_panels_mutex);
		m_panels.erase(std::remove_if(m_panels.begin(), m_panels.end(), [id](const Entry& e) { return e.id == id; }), m_panels.end());
	}

	void BgfxDebugUI::render(unsigned width, unsigned height, bgfx::FrameBufferHandle target)
	{
		if (!m_initialized) return;
		if (!m_visible)
		{
			// Keep the queue short and drop what was meant for the hidden windows
			std::lock_guard<std::mutex> lock(m_mutex);
			m_events.clear();
			m_windows.clear();
			m_wants_mouse = m_wants_keyboard = false;
			return;
		}
		feed_events();
		bgfx::setViewFrameBuffer(m_view, target);
		imguiBeginFrame(m_mouse_x, m_mouse_y, m_buttons, static_cast<int32_t>(m_wheel), uint16_t(width), uint16_t(height), -1, m_view);
		{
			// A panel may remove itself (or others) while it runs: iterate over a copy, skipping the ones removed meanwhile
			std::lock_guard<std::recursive_mutex> lock(m_panels_mutex);
			const std::vector<Entry> panels = m_panels;
			for (const Entry& entry : panels)
			{
				const bool alive = std::any_of(m_panels.begin(), m_panels.end(), [&](const Entry& e) { return e.id == entry.id; });
				if (alive) entry.panel();
			}
		}
		imguiEndFrame();
		const ImGuiIO& io = ImGui::GetIO();
		m_wants_mouse = io.WantCaptureMouse;
		m_wants_keyboard = io.WantCaptureKeyboard;
		std::vector<Rect> windows;
		for (const ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
		{
			if (window->Active && !window->Hidden && !(window->Flags & ImGuiWindowFlags_ChildWindow))
			{
				windows.push_back({ window->Pos.x, window->Pos.y, window->Pos.x + window->Size.x, window->Pos.y + window->Size.y });
			}
		}
		std::lock_guard<std::mutex> lock(m_mutex);
		m_windows.swap(windows);
	}

	bool BgfxDebugUI::is_over(int x, int y) const
	{
		if (!m_visible) return false;
		std::lock_guard<std::mutex> lock(m_mutex);
		for (const Rect& r : m_windows)
		{
			if (x >= r.x0 && x < r.x1 && y >= r.y0 && y < r.y1) return true;
		}
		return false;
	}
}
