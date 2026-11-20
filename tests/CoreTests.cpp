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
#include "EventScheduler.h"
#include "ThreadPool.h"
#include "DoubleBufferedList.h"
#include "Task.h"

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
