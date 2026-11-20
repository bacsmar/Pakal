///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: QaTools.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "mcp/QaTools.h"

#include "Engine.h"
#include "GameStateManager.h"
#include "IInputManager.h"
#include "IUIManager.h"
#include "LogMgr.h"
#include "OSManager.h"
#include "TimeScale.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <thread>

namespace Pakal
{
	namespace
	{
		using json = nlohmann::json;
		using Synthetic = IInputManager::SyntheticEvent;
		using Kind = Synthetic::Kind;

		// --- the log's last lines ---
		struct LogLine { unsigned long long number; int level; std::string text; };
		constexpr size_t kLogLines = 2000;
		std::mutex s_logMutex;
		std::deque<LogLine> s_log;
		unsigned long long s_logNumber = 0;
		bool s_logKept = false;

		std::string lower(std::string text)
		{
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			return text;
		}

		json object_schema(json properties = json::object())
		{
			return { { "type", "object" }, { "properties", std::move(properties) } };
		}

		json property(const char* type, const char* description)
		{
			return { { "type", type }, { "description", description } };
		}

		// --- names ---
		Key key_named(const std::string& name)
		{
			const std::string wanted = lower(name);
			static const std::pair<const char*, Key> aliases[] = { { "enter", Key::Return }, { "esc", Key::Escape }, { "backspace", Key::BackSpace },
				{ "ctrl", Key::LControl }, { "shift", Key::LShift }, { "alt", Key::LAlt }, { "space", Key::Space }, { "pageup", Key::PageUp },
				{ "pagedown", Key::PageDown }, { "0", Key::Num0 }, { "1", Key::Num1 }, { "2", Key::Num2 }, { "3", Key::Num3 }, { "4", Key::Num4 },
				{ "5", Key::Num5 }, { "6", Key::Num6 }, { "7", Key::Num7 }, { "8", Key::Num8 }, { "9", Key::Num9 } };
			for (const auto& [alias, key] : aliases)
			{
				if (wanted == alias) return key;
			}
			for (int k = 0; k < static_cast<int>(Key::KeyCount); ++k)
			{
				if (wanted == lower(KeyName(static_cast<Key>(k)))) return static_cast<Key>(k);
			}
			throw std::runtime_error("no key '" + name + "' (names as in EventArgs.h: A..Z, Num0..Num9, Escape, Return, Space, Left, Right, Up, Down, F1..F15, Tab, LShift...)");
		}

		PadInput pad_named(const std::string& name)
		{
			static const std::pair<const char*, PadInput> names[] = { { "a", PadInput::A }, { "b", PadInput::B }, { "x", PadInput::X },
				{ "y", PadInput::Y }, { "lb", PadInput::LeftBumper }, { "leftbumper", PadInput::LeftBumper }, { "rb", PadInput::RightBumper },
				{ "rightbumper", PadInput::RightBumper }, { "back", PadInput::Back }, { "select", PadInput::Back }, { "start", PadInput::Start },
				{ "ls", PadInput::LeftStick }, { "leftstick", PadInput::LeftStick }, { "rs", PadInput::RightStick }, { "rightstick", PadInput::RightStick },
				{ "left", PadInput::Left }, { "right", PadInput::Right }, { "up", PadInput::Up }, { "down", PadInput::Down },
				{ "lt", PadInput::LeftTrigger }, { "lefttrigger", PadInput::LeftTrigger }, { "rt", PadInput::RightTrigger }, { "righttrigger", PadInput::RightTrigger } };
			const std::string wanted = lower(name);
			for (const auto& [n, input] : names)
			{
				if (wanted == n) return input;
			}
			throw std::runtime_error("no pad button '" + name + "' (A, B, X, Y, LB, RB, Back, Start, LS, RS, Left, Right, Up, Down, LT, RT)");
		}

		MouseButton button_named(const std::string& name)
		{
			const std::string wanted = lower(name);
			if (wanted == "left") return MouseButton::Left;
			if (wanted == "right") return MouseButton::Right;
			if (wanted == "middle") return MouseButton::Middle;
			throw std::runtime_error("no mouse button '" + name + "' (left, right, middle)");
		}

		IInputManager& input_of(Engine& engine)
		{
			IInputManager* input = engine.input_manager();
			if (!input) throw std::runtime_error("the game has no input manager");
			return *input;
		}

		// down, up or tap (down, then up 'hold_ms' later)
		std::string action_of(const json& arguments, const char* fallback)
		{
			const std::string action = lower(arguments.value("action", std::string(fallback)));
			return action;
		}

		int hold_of(const json& arguments)
		{
			return std::clamp(arguments.value("hold_ms", 80), 0, 10000);
		}

		// Waits until the platform has delivered the event numbered 'number' (its next frame; a tap's release 'hold_ms' later)
		json delivered(const json& arguments, IInputManager& input, unsigned long long number, int extraMs)
		{
			if (!arguments.value("wait", true)) return json{ { "queued", true } };
			const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000 + extraMs);
			while (input.synthetic_delivered() < number)
			{
				if (std::chrono::steady_clock::now() > until) return json{ { "queued", true }, { "delivered", false } };
				std::this_thread::sleep_for(std::chrono::milliseconds(2));
			}
			return json{ { "delivered", true } };
		}

		// A press: down, up, or tap (down now, up hold_ms later)
		json press(const json& arguments, IInputManager& input, Synthetic down, Kind upKind)
		{
			const std::string action = action_of(arguments, "tap");
			const int hold = hold_of(arguments);
			if (action == "down") return delivered(arguments, input, input.queue_synthetic(down), 0);
			Synthetic up = down;
			up.kind = upKind;
			if (action == "up") return delivered(arguments, input, input.queue_synthetic(up), 0);
			if (action != "tap") throw std::runtime_error("action is down, up or tap");
			input.queue_synthetic(down);
			up.due_ms = IInputManager::synthetic_now_ms() + hold;
			return delivered(arguments, input, input.queue_synthetic(up), hold);
		}

		// A point: x, y in the window's pixels (the touch screen's on a phone), or 0..1 with normalized: true
		void point_of(const json& arguments, Synthetic& event, const char* xName = "x", const char* yName = "y")
		{
			if (!arguments.contains(xName) || !arguments.contains(yName)) throw std::runtime_error(std::string("give ") + xName + " and " + yName);
			event.x = arguments.at(xName).get<float>();
			event.y = arguments.at(yName).get<float>();
			event.normalized = arguments.value("normalized", false);
		}

		const char* const kTouchHelp = "A finger on the screen, through the platform's own touch path (on a phone: the interface, the development "
			"panels, the sticks, the game's touches; on the desktop as SFML gives a touch: the mouse, button = finger). action: tap (default; "
			"down, then up hold_ms later), down, move, up, or swipe (from x, y to to_x, to_y in ms, default 300). x, y: pixels of the window "
			"(the touch screen's on a phone) or 0..1 with normalized: true. finger: the touch's id (0). wait (true): answers once delivered.";
	}

	bool qa_tools_allowed(const char* token, const char* release, std::string& why)
	{
		if (release && std::string(release) == "1")
		{
			why = "a release (PAKAL_RELEASE=1)";
			return false;
		}
		if (!token || !*token)
		{
			why = "the server has no token (PAKAL_MCP_TOKEN)";
			return false;
		}
		why.clear();
		return true;
	}

	void qa_log_keep()
	{
		{
			std::lock_guard<std::mutex> lock(s_logMutex);
			if (s_logKept) return;
			s_logKept = true;
		}
		LogMgr::instance().set_sink([](int level, const char* text)
		{
			std::lock_guard<std::mutex> lock(s_logMutex);
			s_log.push_back({ ++s_logNumber, level, text });
			if (s_log.size() > kLogLines) s_log.pop_front();
		});
	}

	std::vector<std::string> qa_log_tail(size_t count, const std::string& filter, int minLevel, unsigned long long after, unsigned long long& last)
	{
		const std::string wanted = lower(filter);
		std::vector<std::string> out;
		std::lock_guard<std::mutex> lock(s_logMutex);
		last = s_logNumber;
		for (auto it = s_log.rbegin(); it != s_log.rend() && out.size() < count; ++it)
		{
			if (it->number <= after) break;
			if (it->level < minLevel) continue;
			if (!wanted.empty() && lower(it->text).find(wanted) == std::string::npos) continue;
			out.push_back(it->text);
		}
		std::reverse(out.begin(), out.end());
		return out;
	}

	void add_qa_tools(McpServer& server, Engine& engine)
	{
		qa_log_keep();
		Engine* e = &engine;

		server.add_tool("input.key", { "A key of the keyboard, as if pressed: the game reads it held (is_key_pressed, whatever window has the focus) "
			"and gets its events (on the desktop). key: its name (A..Z, Num0..Num9, Escape, Return/Enter, Space, Left, Right, Up, Down, Tab, F1..F15, "
			"LShift...). action: tap (default: down, then up hold_ms later, 80), down, up. wait (true): answers once delivered.",
			object_schema({ { "key", property("string", "The key's name") }, { "action", property("string", "tap, down or up") },
				{ "hold_ms", property("integer", "How long a tap holds it (80)") }, { "wait", property("boolean", "Answer once delivered (true)") } }),
			[e](const json& arguments)
			{
				IInputManager& input = input_of(*e);
				Synthetic down;
				down.kind = Kind::KeyDown;
				down.key = key_named(arguments.at("key").get<std::string>());
				return press(arguments, input, down, Kind::KeyUp);
			} });

		server.add_tool("input.pad", { "A gamepad of a seat (0..3), as if held: its buttons read as the keys the game bound them to (set_pad_bindings), "
			"its sticks as axis device 0 (axes 0-1 the left stick, 2-3 the right; y up), the left stick past half its way as Left/Right/Up/Down. "
			"Either button (A, B, X, Y, LB, RB, Back, Start, LS, RS, Left, Right, Up, Down, LT, RT) with action tap (default), down or up and "
			"hold_ms; or stick (left/right) with x, y (-1..1, y up) and optionally ms (then it goes back to the middle).",
			object_schema({ { "seat", property("integer", "The pad's seat, 0..3 (0)") }, { "button", property("string", "A button's name") },
				{ "action", property("string", "tap, down or up") }, { "hold_ms", property("integer", "How long a tap holds it (80)") },
				{ "stick", property("string", "left or right") }, { "x", property("number", "-1..1") }, { "y", property("number", "-1..1, up positive") },
				{ "ms", property("integer", "How long the stick stays pushed (0: until moved again)") }, { "wait", property("boolean", "Answer once delivered (true)") } }),
			[e](const json& arguments)
			{
				IInputManager& input = input_of(*e);
				const int seat = arguments.value("seat", 0);
				if (seat < 0 || seat >= kPadSlots) throw std::runtime_error("seat is 0.." + std::to_string(kPadSlots - 1));
				if (arguments.contains("stick"))
				{
					const std::string stick = lower(arguments.at("stick").get<std::string>());
					if (stick != "left" && stick != "right") throw std::runtime_error("stick is left or right");
					Synthetic push;
					push.kind = Kind::PadStick;
					push.slot = seat;
					push.stick = stick == "left" ? 0u : 1u;
					push.x = arguments.value("x", 0.0f);
					push.y = arguments.value("y", 0.0f);
					unsigned long long number = input.queue_synthetic(push);
					const int ms = std::clamp(arguments.value("ms", 0), 0, 60000);
					if (ms > 0)
					{
						Synthetic back = push;
						back.x = back.y = 0.0f;
						back.due_ms = IInputManager::synthetic_now_ms() + ms;
						number = input.queue_synthetic(back);
					}
					return delivered(arguments, input, number, ms);
				}
				Synthetic down;
				down.kind = Kind::PadDown;
				down.slot = seat;
				down.pad = pad_named(arguments.at("button").get<std::string>());
				return press(arguments, input, down, Kind::PadUp);
			} });

		server.add_tool("input.touch", { kTouchHelp,
			object_schema({ { "action", property("string", "tap, down, move, up or swipe") }, { "x", property("number", "Pixels, or 0..1 when normalized") },
				{ "y", property("number", "Pixels, or 0..1 when normalized") }, { "to_x", property("number", "swipe: where it ends") },
				{ "to_y", property("number", "swipe: where it ends") }, { "ms", property("integer", "swipe: how long (300)") },
				{ "normalized", property("boolean", "x, y are 0..1 of the screen") }, { "finger", property("integer", "The touch's id (0)") },
				{ "hold_ms", property("integer", "How long a tap holds (80)") }, { "wait", property("boolean", "Answer once delivered (true)") } }),
			[e](const json& arguments)
			{
				IInputManager& input = input_of(*e);
				const std::string action = action_of(arguments, "tap");
				Synthetic event;
				event.finger = arguments.value("finger", 0);
				point_of(arguments, event);
				if (action == "down" || action == "move" || action == "up")
				{
					event.kind = action == "down" ? Kind::TouchDown : action == "move" ? Kind::TouchMove : Kind::TouchUp;
					return delivered(arguments, input, input.queue_synthetic(event), 0);
				}
				if (action == "tap")
				{
					event.kind = Kind::TouchDown;
					return press(json{ { "action", "tap" }, { "hold_ms", hold_of(arguments) }, { "wait", arguments.value("wait", true) } }, input, event, Kind::TouchUp);
				}
				if (action != "swipe") throw std::runtime_error("action is tap, down, move, up or swipe");
				Synthetic end = event;
				point_of(arguments, end, "to_x", "to_y");
				const int ms = std::clamp(arguments.value("ms", 300), 16, 10000);
				const long long start = IInputManager::synthetic_now_ms();
				event.kind = Kind::TouchDown;
				input.queue_synthetic(event);
				const int steps = std::max(2, ms / 16);
				unsigned long long number = 0;
				for (int i = 1; i <= steps; ++i)
				{
					Synthetic move = event;
					const float t = static_cast<float>(i) / static_cast<float>(steps);
					move.kind = Kind::TouchMove;
					move.x = event.x + (end.x - event.x) * t;
					move.y = event.y + (end.y - event.y) * t;
					move.due_ms = start + static_cast<long long>(ms * t);
					number = input.queue_synthetic(move);
				}
				end.kind = Kind::TouchUp;
				end.due_ms = start + ms;
				number = input.queue_synthetic(end);
				return delivered(arguments, input, number, ms);
			} });

		server.add_tool("input.pointer", { "The mouse (on a phone: finger 0's touch). action: click (default: down, then up hold_ms later), down, up, move, "
			"or wheel (delta). x, y: pixels of the window, or 0..1 with normalized: true. button: left (default), right, middle. wait (true).",
			object_schema({ { "action", property("string", "click, down, up, move or wheel") }, { "x", property("number", "Pixels, or 0..1 when normalized") },
				{ "y", property("number", "Pixels, or 0..1 when normalized") }, { "normalized", property("boolean", "x, y are 0..1 of the window") },
				{ "button", property("string", "left, right or middle") }, { "delta", property("number", "wheel: the steps (up positive)") },
				{ "hold_ms", property("integer", "How long a click holds (80)") }, { "wait", property("boolean", "Answer once delivered (true)") } }),
			[e](const json& arguments)
			{
				IInputManager& input = input_of(*e);
				const std::string action = action_of(arguments, "click");
				Synthetic event;
				point_of(arguments, event);
				event.button = button_named(arguments.value("button", std::string("left")));
				event.delta = arguments.value("delta", 0.0f);
				if (action == "move" || action == "down" || action == "up" || action == "wheel")
				{
					event.kind = action == "move" ? Kind::MouseMove : action == "down" ? Kind::MouseDown : action == "up" ? Kind::MouseUp : Kind::MouseWheel;
					return delivered(arguments, input, input.queue_synthetic(event), 0);
				}
				if (action != "click") throw std::runtime_error("action is click, down, up, move or wheel");
				event.kind = Kind::MouseDown;
				return press(json{ { "action", "tap" }, { "hold_ms", hold_of(arguments) }, { "wait", arguments.value("wait", true) } }, input, event, Kind::MouseUp);
			} });

		server.add_tool("state", { "Where the game is: its state stack (top first: name and status line, BaseGameState::describe), the game's own "
			"line (IPakalApplication::describe), the UI documents loaded (id, source file, title, visible), the time scale. fresh: false when the "
			"game did not update to answer (paused, loading) and the stack is the last one seen.",
			object_schema(),
			[e](const json&)
			{
				json out;
				if (GameStateManager* states = e->game_state_manager())
				{
					const GameStateManager::Snapshot before = states->snapshot(0);
					const GameStateManager::Snapshot taken = states->snapshot(500);
					json stack = json::array();
					for (const auto& entry : taken.states) stack.push_back({ { "name", entry.name }, { "status", entry.status } });
					out["states"] = stack;
					out["top"] = taken.states.empty() ? std::string() : taken.states.front().name;
					out["status"] = taken.states.empty() ? std::string() : taken.states.front().status;
					out["game"] = taken.game;
					out["fresh"] = taken.generation > before.generation;
				}
				json documents = json::array();
				if (e->graphics_system() && e->get_ui_manager())
				{
					for (const auto& document : e->get_ui_manager()->documents())
					{
						documents.push_back({ { "id", document.id }, { "source", document.source }, { "title", document.title }, { "visible", document.visible } });
					}
				}
				out["documents"] = documents;
				out["time_scale"] = sim_time_scale();
				return out;
			} });

		server.add_tool("log.tail", { "The last lines of the game's log (kept in memory since the server started, the last 2000). lines (50): how many "
			"at most; filter: only those containing it (any case); level: the lowest kept, debug (default: all), info, warning, error; after: only lines after this "
			"number (the 'last' an earlier call answered), to follow the log.",
			object_schema({ { "lines", property("integer", "At most this many (50)") }, { "filter", property("string", "Only lines containing it") },
				{ "level", property("string", "debug, info, warning or error") }, { "after", property("integer", "Only lines after this number") } }),
			[](const json& arguments)
			{
				const std::string level = lower(arguments.value("level", std::string("debug")));
				const int minLevel = level == "error" ? LogMgr::LOG_ERROR : level == "warning" ? LogMgr::LOG_WARNING : level == "info" ? LogMgr::LOG_INFO : LogMgr::LOG_DEBUG;
				unsigned long long last = 0;
				const auto lines = qa_log_tail(static_cast<size_t>(std::clamp(arguments.value("lines", 50), 1, 2000)), arguments.value("filter", std::string()),
					minLevel, arguments.value("after", 0ull), last);
				return json{ { "lines", lines }, { "last", last } };
			} });

		server.add_tool("time.scale", { "How fast the simulation runs: scale 0 stops it, 1 is normal, up to 4 faster. Without scale, answers it. It "
			"scales SimulationClock (the fixed steps of the TopDown genre's levels and their animations and effects); a game that counts the "
			"engine's milliseconds itself (menus, the old genres) is not slowed.",
			object_schema({ { "scale", property("number", "0..4") } }),
			[](const json& arguments)
			{
				if (arguments.contains("scale")) set_sim_time_scale(arguments.at("scale").get<float>());
				return json{ { "scale", sim_time_scale() } };
			} });

		server.add_tool("quit", { "Ends the game as when the person closes its window (the platform's close: event_app_finished, the game ends its "
			"states and saves as it does then). Answers at once; the process exits shortly after.",
			object_schema(),
			[e](const json&)
			{
				e->os_manager()->request_close();
				return json{ { "closing", true } };
			} });
	}
}
