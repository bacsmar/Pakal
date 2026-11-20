///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: CoreTests.cpp
// Purpose: the core alone (PakalCore: no bgfx, no Box2D, no SFML) starts, runs its systems and ends with the dummy
// systems in place of the backends
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Engine.h"
#include "BuildIdentity.h"
#include "GameSettings.h"
#include <filesystem>
#include <fstream>
#include "IPakalApplication.h"
#include "OSManager.h"
#include "GraphicsSystem.h"
#include "PhysicsSystem.h"
#include "EntityManager.h"
#include "EventScheduler.h"
#include "ThreadPool.h"
#include "DoubleBufferedList.h"
#include "Task.h"
#include "GenericEntity.h"
#include "Factory.h"
#include "ComponentManager.h"
#include "ComponentFactory.h"
#include <stdexcept>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>
#include <functional>
#include <cstdlib>

using namespace Pakal;

namespace
{
	int g_failures = 0;

	void check(bool condition, const char* what)
	{
		std::printf("%s %s\n", condition ? "ok  " : "FAIL", what);
		if (!condition) ++g_failures;
	}

	// the entities' handles: one resolves while its entity lives and never after; an unknown or empty factory
	// makes nothing (no exception)
	void check_entities(EntityManager& entities)
	{
		entities.add_entity_factory(CreateFactory<AsyncEntity>("TestEntity", [&entities]() -> AsyncEntity* { return new GenericEntity(&entities); }));
		entities.add_entity_factory(CreateFactory<AsyncEntity>("NoEntity", []() -> AsyncEntity* { return nullptr; }));
		AsyncEntity* first = entities.create_entity("TestEntity");
		AsyncEntity* second = entities.create_entity("TestEntity");
		const EntityHandle handle = first ? first->get_handle() : EntityHandle{};
		check(first && second && entities.resolve(handle) == first, "an entity's handle resolves to it");
		entities.request_dispose(first);
		check(entities.process_pending_disposals() == 1, "a disposed entity is deleted at the next processing");
		check(entities.resolve(handle) == nullptr, "a disposed entity's handle no longer resolves");
		check(second && entities.resolve(second->get_handle()) == second, "the other entity still resolves");
		check(entities.create_entity("NoEntity") == nullptr, "a factory that makes nothing gives no entity (no crash)");
		check(entities.create_entity("Unknown") == nullptr, "an unknown entity type gives no entity");
	}

	// A component factory as a game module registers it: it must be deleted when the engine's run ends (the module is
	// unloaded right after), not later in ~Engine
	std::atomic<bool> g_module_factory_alive{ false };
	struct ModuleFactory : IComponentFactory
	{
		std::string name = "ModuleComponent";
		ModuleFactory() : IComponentFactory([]() -> AsyncComponent* { return nullptr; }) { g_module_factory_alive = true; }
		~ModuleFactory() override { g_module_factory_alive = false; }
		const std::string& get_typename() override { return name; }
	};

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
			if (engine.entity_manager()) check_entities(*engine.entity_manager());
			engine.component_manager()->register_factory(new ModuleFactory());

			// let the systems run a moment, then end the application as a window's close would
			std::thread([&engine]()
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(300));
				engine.os_manager()->event_app_finished();
			}).detach();
		}

		void end(Engine& engine) override { ended = true; }
	};

	// Pauses and resumes the engine a few times, as Android does when the app goes to the background: the threaded
	// systems (the engine's own, the physics) wait for their resume in their inbox; a lost notice left them paused
	int g_pause_cycles = 0;
	class PauseApplication : public IPakalApplication
	{
	public:
		std::atomic<bool> all_resumed{ false };
		const char* get_name() override { return "CoreTests pause"; }
		void start(Engine& engine) override
		{
			std::thread([&engine, this]()
			{
				bool ok = true;
				for (int i = 0; i < 8 && ok; ++i)
				{
					engine.pause()->wait();
					ok = engine.get_state() == SystemState::Paused && engine.physics_system()->get_state() == SystemState::Paused;
					engine.resume()->wait();
					ok = ok && engine.get_state() == SystemState::Running && engine.physics_system()->get_state() == SystemState::Running;
					if (ok) ++g_pause_cycles;
				}
				all_resumed = ok;
				engine.os_manager()->event_app_finished();
			}).detach();
		}
		void end(Engine&) override {}
	};

	// Runs 'test' on its own thread and fails (ending the program: the stuck thread cannot be joined) when it takes
	// more than 'seconds'
	void within(int seconds, const char* what, const std::function<bool()>& test)
	{
		std::atomic<int> result{ -1 };
		std::thread([&result, test]() { result = test() ? 1 : 0; }).detach();
		const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
		while (result < 0 && std::chrono::steady_clock::now() < until)
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		if (result < 0)
		{
			check(false, what);
			std::printf("CoreTests: stuck, ending\n");
			std::fflush(stdout);
			std::_Exit(1);
		}
		check(result == 1, what);
	}
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
		check(!g_module_factory_alive, "a module's component factory is deleted when the run ends (before ~Engine)");
	}

	// the build's identity and its label (C-ID-7, C-BLD-1)
	{
		const BuildIdentity store = BuildIdentity::parse(R"({"version": "1.0.0-beta.10", "channel": "store", "build": 530,
			"commit": "7f0e1aa", "dirty": false, "code": 10010, "label_flag_url": "https://example.com/flag.json"})");
		check(store.known && store.label() == "v1.0.0-beta.10-store  530.7f0e1aa", "a store build's label");
		check(store.code == 10010 && store.label_flag_url == "https://example.com/flag.json", "a store build's code and flag");
		const BuildIdentity demo = BuildIdentity::parse(R"({"edition": "demo mvp", "version": "0.1.0-beta.1", "channel": "dev",
			"build": 412, "commit": "a1b2c3d", "dirty": true})");
		check(demo.label() == "demo mvp v0.1.0-beta.1-dev  412.a1b2c3d.dirty", "an edition's dirty dev label");
		check(BuildIdentity::parse(R"({"version": "1.0.0", "channel": ""})").label().empty(), "production has no label");
		check(BuildIdentity::parse(R"({"build": 9, "commit": "abc1234"})").channel == "dev", "an old build.json is a dev build");
		const BuildIdentity unknown = BuildIdentity::parse("not json");
		check(!unknown.known && unknown.label() == "dev", "no build.json: an unknown dev build");
		check(BuildIdentity::parse(R"({"channel": "dev", "commit": "abc1234"})").label() == "dev  0.abc1234", "a desktop build's label");
		check(label_hidden_by_flag(R"({"hide_label": [529, 530]})", 530), "a promoted bundle's label is hidden");
		check(!label_hidden_by_flag(R"({"hide_label": [529]})", 530), "another bundle's label stays");
		check(!label_hidden_by_flag("<html>error</html>", 530) && !label_hidden_by_flag(R"({"hide_label": [0]})", 0),
			"a broken answer, or no version code, keeps the label");
	}

	// the options every game has (C-OPT): kept per device, volumes in steps of 10, the device's language first, a game's own
	{
		const std::filesystem::path file = std::filesystem::temp_directory_path() / "pakal_core_tests_settings.json";
		std::filesystem::remove(file);
		{
			GameSettings settings(file.string());
			check(settings.music() == 100 && settings.sounds() == 80 && !settings.telemetry() && settings.quality() == "auto",
				"the options' defaults");
			std::string told;
			settings.event_changed.add_listener([&told](const std::string& name) { told += name + ";"; });
			settings.set_music(47);
			settings.set_sounds(130);
			settings.set_telemetry(true);
			settings.set_fsr("sometimes");
			settings.set_extra("blood", "false");
			check(settings.music() == 50 && settings.sounds() == 100, "volumes go to steps of 10 within 0..100");
			check(settings.fsr() == "auto", "an unknown FSR mode is auto");
			check(told == "music;sounds;telemetry;fsr;blood;", "each change is told by name");
			check(settings.language_to_use("es", { "en", "es" }) == "es", "the first start follows the device");
			check(settings.language_to_use("fr", { "en", "es" }) == "en", "a device's language the game lacks: its first");
			settings.set_language("en");
			check(settings.language_to_use("es", { "en", "es" }) == "en", "the chosen language wins");
		}
		{
			GameSettings again(file.string());
			check(again.music() == 50 && again.telemetry() && again.language() == "en" && again.extra("blood", "true") == "false",
				"the options are kept for the next run");
			const std::string json = again.to_json();
			check(json.find("\"blood\":false") != std::string::npos && json.find("\"music\":50") != std::string::npos,
				"to_json has the options and the game's own");
			again.reset();
			check(again.music() == 100 && again.language().empty(), "reset gives the defaults");
		}
		{
			std::ofstream(file) << "{ broken";
			GameSettings broken(file.string());
			check(broken.music() == 100, "a broken file gives the defaults");
		}
		std::filesystem::remove(file);
		GameSettings::About about;
		about.version = "1.0.0"; about.build = "531";
		check(GameSettings::about_json(about).find("\"line\":\"v1.0.0 (531)\"") != std::string::npos, "the version line");
	}

	// Settings without an allocator: an error said, not a null system
	{
		Engine::Settings settings;
		settings.physics_system_allocator = nullptr;
		bool refused = false;
		try { Engine engine(settings); }
		catch (const std::invalid_argument&) { refused = true; }
		check(refused, "an engine whose settings lack an allocator refuses to start (no null system)");
	}

	// the workers: every job runs, the ones queued at once on many threads too
	within(20, "the worker pool runs every job sent to it", []()
	{
		std::atomic<int> ran{ 0 };
		std::vector<BasicTaskPtr> tasks;
		for (int i = 0; i < 300; ++i)
			tasks.push_back(EventScheduler::instance().execute_in_worker([&ran]() { ++ran; }));
		TaskUtils::wait_all(tasks);
		return ran == 300;
	});

	// a pool that ends runs what was still queued (a task someone waits on completes)
	within(20, "a pool that ends runs the jobs still queued", []()
	{
		std::atomic<int> ran{ 0 };
		{
			ThreadPool pool;
			for (int i = 0; i < 40; ++i)
				pool.execute([&ran]() { std::this_thread::sleep_for(std::chrono::milliseconds(1)); ++ran; });
		}
		return ran == 40;
	});

	// when_all completes once even when its tasks end at the same moment on many threads
	within(20, "when_all completes once when its tasks end together", []()
	{
		for (int round = 0; round < 50; ++round)
		{
			std::atomic<bool> go{ false };
			std::vector<BasicTaskPtr> tasks;
			for (int i = 0; i < 8; ++i)
				tasks.push_back(EventScheduler::instance().execute_in_worker([&go]() { while (!go) std::this_thread::yield(); }));
			std::atomic<int> completions{ 0 };
			auto all = TaskUtils::when_all(tasks);
			auto after = all->continue_with([&completions]() { ++completions; });
			go = true;
			after->wait();
			TaskUtils::wait_all(tasks);
			std::this_thread::sleep_for(std::chrono::milliseconds(2));
			if (completions != 1) return false;
		}
		return true;
	});

	// a thread waiting on an empty inbox wakes for every task pushed to it (one by one: the moment a notice was lost)
	within(20, "a waiting inbox never misses a task pushed to it", []()
	{
		DoubleBufferedQueue<int> queue;
		std::atomic<int> received{ 0 };
		const int rounds = 20000;
		std::thread consumer([&]()
		{
			for (int i = 0; i < rounds; ++i)
			{
				queue.pop(true);
				++received;
			}
		});
		for (int i = 0; i < rounds; ++i)
		{
			queue.push(i);
			while (received <= i) std::this_thread::yield();
		}
		consumer.join();
		return received == rounds;
	});

	// pause and resume, and the engine ends cleanly after them (its threads joined)
	within(30, "the engine pauses and resumes its threaded systems, again and again", []()
	{
		Engine::Settings settings;
		Engine engine(settings);
		PauseApplication app;
		engine.run(&app);
		return app.all_resumed.load() && g_pause_cycles == 8;
	});
	std::printf(g_failures == 0 ? "CoreTests: all passed\n" : "CoreTests: %d failed\n", g_failures);
	return g_failures == 0 ? 0 : 1;
}
