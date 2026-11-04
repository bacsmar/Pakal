///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CoreTests.cpp
// Purpose: the core alone (PakalCore: no bgfx, no Box2D, no SFML) starts, runs its systems and ends with the dummy
// systems in place of the backends
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "IPakalApplication.h"
#include "OSManager.h"
#include "GraphicsSystem.h"
#include "PhysicsSystem.h"
#include "EntityManager.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

using namespace Pakal;

namespace
{
	int g_failures = 0;

	void check(bool condition, const char* what)
	{
		std::printf("%s %s\n", condition ? "ok  " : "FAIL", what);
		if (!condition) ++g_failures;
	}

	class CoreApplication : public IPakalApplication
	{
	public:
		std::atomic<bool> started{ false };
		std::atomic<bool> ended{ false };

		const char* get_name() override { return "CoreTests"; }

		void start(Engine& engine) override
		{
			started = true;
			check(engine.graphics_system() && std::strcmp(engine.graphics_system()->get_system_name(), "DummyGraphicsSystem") == 0,
				"the graphics system is the dummy one");
			check(engine.physics_system() && std::strcmp(engine.physics_system()->get_system_name(), "DummyPhysicsSystem") == 0,
				"the physics system is the dummy one");
			check(engine.input_manager() != nullptr, "there is an input manager (one without devices)");
			check(engine.entity_manager() != nullptr, "there is an entity manager");
			check(engine.get_ui_manager() != nullptr, "there is a UI manager (the dummy one)");

			// let the systems run a moment, then end the application as a window's close would
			std::thread([&engine]()
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(300));
				engine.os_manager()->event_app_finished();
			}).detach();
		}

		void end(Engine& engine) override { ended = true; }
	};
}

int main()
{
	{
		Engine::Settings settings;
		Engine engine(settings);
		CoreApplication app;
		engine.run(&app);
		check(app.started, "the application started");
		check(app.ended, "the application ended");
	}
	std::printf(g_failures == 0 ? "CoreTests: all passed\n" : "CoreTests: %d failed\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
