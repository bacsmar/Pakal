///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: QaToolsTests.cpp
// Purpose: The QA hooks (source/mcp/QaTools) on a headless engine (the core with its dummy systems), over the
//          MCP server's socket: synthetic keys, pad buttons and sticks, the mouse and touches reach the game the
//          way the platform's own input does (held keys, events, at the next window events); state answers the
//          stack top first with each state's status line and the game's; log.tail finds a line; time.scale
//          scales SimulationClock; quit ends the run as a window's close does. And they are allowed only behind
//          a token, never in a release.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "BaseGameState.h"
#include "Engine.h"
#include "GameStateManager.h"
#include "IInputManager.h"
#include "IPakalApplication.h"
#include "LogMgr.h"
#include "OSManager.h"
#include "SimulationClock.h"
#include "TimeScale.h"
#include "mcp/McpClient.h"
#include "mcp/McpServer.h"
#include "mcp/QaTools.h"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <thread>

using namespace Pakal;
using json = nlohmann::json;

namespace
{
	std::atomic<int> g_failures{ 0 };

	void check(bool condition, const char* what)
	{
		std::printf("%s %s\n", condition ? "ok  " : "FAIL", what);
		if (!condition) ++g_failures;
	}

	const unsigned short kPort = 18766;
	const char* const kToken = "qa-secret";

	// What the game saw, as a game reads it
	std::atomic<bool> g_returnHeld{ false }, g_spaceHeld{ false }, g_keyEvent{ false };
	std::atomic<float> g_stickX{ 0.0f }, g_stickY{ 0.0f };
	std::atomic<int> g_pressX{ -1 }, g_pressY{ -1 }, g_pressButton{ -1 }, g_releases{ 0 };

	class QaState : public BaseGameState
	{
	public:
		QaState() : BaseGameState("QaTestState") {}
		std::string describe() const override { return "screen=test"; }

	protected:
		void on_initialize(Engine* engine) override { m_engine = engine; }
		void on_terminate(Engine*) override {}
		void on_activate(Engine*) override {}
		void on_deactivate(Engine*) override {}
		void on_update(unsigned long) override
		{
			IInputManager* input = m_engine->input_manager();
			if (input->is_key_pressed(Key::Return)) g_returnHeld = true;
			if (input->is_key_pressed(Key::Space)) g_spaceHeld = true;   // the pad's A is bound to it
			if (IAxisDevice* sticks = input->get_axis_device(0))
			{
				g_stickX = sticks->get_axis_state(0);
				g_stickY = sticks->get_axis_state(1);
			}
		}

	private:
		Engine* m_engine = nullptr;
	};

	class QaApplication : public IPakalApplication
	{
	public:
		std::atomic<bool> ended{ false };
		const char* get_name() override { return "QaToolsTests"; }
		std::string describe() override { return "qa-game v1"; }

		void start(Engine& engine) override
		{
			IInputManager* input = engine.input_manager();
			input->set_pad_bindings({ { PadInput::A, Key::Space, -1 } });
			input->event_key_down.add_listener([](const KeyArgs& args) { if (args.key == Key::Return) g_keyEvent = true; });
			input->event_mouse_pressed.add_listener([](const MouseArgs& args)
			{
				g_pressX = static_cast<int>(args.x);
				g_pressY = static_cast<int>(args.y);
				g_pressButton = static_cast<int>(args.button_id);
			});
			input->event_mouse_released.add_listener([](const MouseArgs&) { ++g_releases; });
			engine.game_state_manager()->push_state(new QaState(), true);
		}

		void end(Engine&) override { ended = true; }
	};

	bool call(McpClient& client, const char* tool, const json& arguments, json& out)
	{
		std::string error;
		const bool ok = client.call(tool, arguments, out, error);
		if (!ok) std::printf("     %s: %s\n", tool, error.c_str());
		return ok;
	}

	void drive(McpServer& server)
	{
		McpClient client(kPort, kToken);
		json out, tools;
		std::string error;
		check(client.list_tools(tools, error), "tools/list answers");
		bool all = true;
		for (const char* name : { "input.key", "input.pad", "input.touch", "input.pointer", "state", "log.tail", "time.scale", "quit" })
		{
			bool found = false;
			for (const auto& tool : tools) found = found || tool.value("name", std::string()) == name;
			all = all && found;
		}
		check(all, "every QA tool is listed");

		// state: the stack, top first, with the status lines (once the game has started)
		for (int i = 0; i < 100; ++i)
		{
			if (client.call("state", json::object(), out, error) && out.value("top", std::string()) == "QaTestState") break;
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		check(call(client, "state", json::object(), out) && out.value("top", std::string()) == "QaTestState"
			&& out.value("status", std::string()) == "screen=test" && out.value("game", std::string()) == "qa-game v1" && out.value("fresh", false),
			"state answers the top state, its status line and the game's");

		// a key: held for the game's update, and its event
		check(call(client, "input.key", { { "key", "Return" }, { "hold_ms", 100 } }, out) && out.value("delivered", false), "input.key tap is delivered");
		check(g_returnHeld.load(), "the game read the key held (is_key_pressed)");
		check(g_keyEvent.load(), "the game got the key's event");
		json bad;
		std::string badError;
		check(client.call("input.key", { { "key", "NoSuchKey" } }, bad, badError) == false, "an unknown key is an error");

		// a pad's button, through the game's bindings; its stick, as axis device 0
		check(call(client, "input.pad", { { "button", "A" }, { "hold_ms", 100 } }, out) && g_spaceHeld.load(), "input.pad A reads as the key bound to it");
		check(call(client, "input.pad", { { "stick", "left" }, { "x", 0.5 }, { "y", -1.0 } }, out), "input.pad pushes a stick");
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
		check(std::fabs(g_stickX.load() - 0.5f) < 1e-4f && std::fabs(g_stickY.load() + 1.0f) < 1e-4f, "the game reads the stick on axis device 0");
		call(client, "input.pad", { { "stick", "left" }, { "x", 0.0 }, { "y", 0.0 } }, out);

		// the mouse: a click where it is told; a touch at 0..1 of the window (the dummy window is 640x480)
		check(call(client, "input.pointer", { { "x", 100 }, { "y", 50 } }, out) && g_pressX == 100 && g_pressY == 50
			&& g_pressButton == static_cast<int>(MouseButton::Left) && g_releases == 1, "input.pointer click: pressed and released where told");
		check(call(client, "input.touch", { { "x", 0.5 }, { "y", 0.25 }, { "normalized", true } }, out) && g_pressX == 320 && g_pressY == 120
			&& g_releases == 2, "input.touch tap at 0..1 lands on the window's pixels");
		check(call(client, "input.touch", { { "action", "swipe" }, { "x", 10 }, { "y", 10 }, { "to_x", 200 }, { "to_y", 10 }, { "ms", 100 } }, out)
			&& g_releases == 3, "input.touch swipe ends with the finger up");

		// the log
		LOG_INFO("[QaToolsTests] marker-%d", 4217);
		check(call(client, "log.tail", { { "filter", "MARKER-4217" } }, out) && out["lines"].size() == 1, "log.tail finds a line by a filter");
		const unsigned long long last = out.value("last", 0ull);
		check(call(client, "log.tail", { { "after", last }, { "filter", "marker-4217" } }, out) && out["lines"].empty(), "log.tail after a number skips what was read");

		// the time scale: SimulationClock follows it
		check(call(client, "time.scale", { { "scale", 0 } }, out) && out.value("scale", -1.0f) == 0.0f, "time.scale 0");
		SimulationClock clock;
		clock.frame_seconds();
		std::this_thread::sleep_for(std::chrono::milliseconds(20));
		check(clock.frame_seconds() == 0.0f, "stopped: the simulation's frames bring no time");
		check(call(client, "time.scale", { { "scale", 9 } }, out) && out.value("scale", -1.0f) == kMaxTimeScale, "time.scale is clamped");
		call(client, "time.scale", { { "scale", 1 } }, out);
		check(sim_time_scale() == 1.0f, "time.scale back to 1");

		// without the token nothing answers
		McpClient stranger(kPort, "");
		check(!stranger.call("state", json::object(), out, error), "without the token the QA tools are refused");

		// quit: the game ends as a window's close ends it
		check(call(client, "quit", json::object(), out) && out.value("closing", false), "quit answers");
		(void)server;
	}
}

int main()
{
	std::string why;
	check(!qa_tools_allowed(nullptr, nullptr, why) && !why.empty(), "not allowed without a token");
	check(!qa_tools_allowed("", nullptr, why), "not allowed with an empty token");
	check(!qa_tools_allowed(kToken, "1", why), "not allowed in a release (PAKAL_RELEASE=1)");
	check(qa_tools_allowed(kToken, nullptr, why) && qa_tools_allowed(kToken, "0", why), "allowed behind a token, outside a release");

	Engine::Settings settings;
	settings.graphic_system_settings.resolution = { 640, 480 };
	settings.max_fps = 240;
	Engine engine(settings);
	QaApplication application;

	McpServer server;
	add_qa_tools(server, engine);
	std::string error;
	check(server.start(kPort, kToken, error), "the server starts");

	std::thread driver([&server]() { drive(server); });
	// a stuck run fails instead of hanging the tests
	std::thread watchdog([&application]()
	{
		for (int i = 0; i < 300 && !application.ended; ++i) std::this_thread::sleep_for(std::chrono::milliseconds(100));
		if (!application.ended)
		{
			std::printf("FAIL the run did not end (quit)\n");
			std::fflush(stdout);
			std::_Exit(1);
		}
	});
	engine.run(&application);
	driver.join();
	watchdog.join();
	check(application.ended.load(), "quit ended the run (the application's end ran)");
	server.stop();

	if (g_failures == 0) std::printf("QaToolsTests: all passed\n");
	return g_failures == 0 ? 0 : 1;
}
