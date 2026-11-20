///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: PoseSharingTests.cpp
// Purpose: A frame's posing shared among threads (WorkShare): every item done exactly once and all of them
//          before run() returns, frame after frame, few items on the caller alone. And the marks a model
//          passes while posed on a worker (MarkReports): held, then reported on the caller's thread, each
//          once and in order, as they would have been without the workers.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "AnimationMarks.h"
#include "WorkShare.h"

#include <atomic>
#include <cstdio>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace
{
	int g_failures = 0;

	void check(bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++g_failures;
	}
}

int main()
{
	using Pakal::MarkReports;
	using Pakal::WorkShare;

	// --- WorkShare ---
	{
		WorkShare share(3);
		bool once = true, allDone = true;
		for (size_t frame = 0; frame < 2000; ++frame)
		{
			const size_t count = frame % 211;   // 0..210 items, below and above the minimum
			std::vector<std::atomic<int>> visits(count);
			share.run(count, [&](size_t i) { visits[i].fetch_add(1, std::memory_order_relaxed); });
			// every item exactly once, and all of them by the time run returns
			for (size_t i = 0; i < count; ++i)
			{
				const int seen = visits[i].load();
				if (seen == 0) allDone = false;
				if (seen > 1) once = false;
			}
		}
		check(allDone, "every item is done before run() returns, frame after frame");
		check(once, "no item is done twice");

		const std::thread::id caller = std::this_thread::get_id();
		bool onCaller = true;
		share.run(7, [&](size_t) { if (std::this_thread::get_id() != caller) onCaller = false; });
		check(onCaller, "below the minimum, the caller does every item");

		std::atomic<int> others{ 0 };
		share.run(400, [&](size_t) { if (std::this_thread::get_id() != caller) others.fetch_add(1); });
		check(others.load() > 0, "above it, the workers take their shares");

		WorkShare alone(0);
		size_t done = 0;
		alone.run(100, [&](size_t) { ++done; });
		check(done == 100, "with no workers, the caller does them all");
	}

	// --- MarkReports, as BgfxGraphicsSystem poses: deferred on the workers, flushed on its thread ---
	{
		struct Model
		{
			MarkReports marks;
			std::vector<std::string> passed;   // what one update passes
			std::vector<std::string> heard;    // what its handler heard
			std::vector<std::thread::id> heardOn;
		};
		const size_t count = 64;
		std::vector<std::unique_ptr<Model>> models;
		for (size_t i = 0; i < count; ++i)
		{
			auto model = std::make_unique<Model>();
			model->passed = { "step" + std::to_string(i), "impact" + std::to_string(i), "stepR" + std::to_string(i) };
			models.push_back(std::move(model));
		}
		WorkShare share(3);
		const std::thread::id caller = std::this_thread::get_id();
		for (int frame = 0; frame < 3; ++frame)
		{
			share.run(count, [&](size_t i)
			{
				Model& model = *models[i];
				model.marks.defer(true);
				model.marks.report(7, model.passed, [&](int, const std::string&) { model.heard.push_back("NOT HELD"); });
				model.marks.defer(false);
			});
			for (auto& model : models)
			{
				Model& m = *model;
				m.marks.flush([&](int clip, const std::string& mark)
				{
					m.heard.push_back(std::to_string(clip) + ":" + mark);
					m.heardOn.push_back(std::this_thread::get_id());
				});
			}
		}
		bool inOrder = true, onThread = true, empty = true;
		for (size_t i = 0; i < count; ++i)
		{
			const Model& m = *models[i];
			std::vector<std::string> expected;
			for (int frame = 0; frame < 3; ++frame)
				for (const auto& mark : m.passed) expected.push_back("7:" + mark);
			if (m.heard != expected) inOrder = false;
			for (const auto& id : m.heardOn) if (id != caller) onThread = false;
			if (m.marks.held() != 0) empty = false;
		}
		check(inOrder, "a deferred model's marks are heard once each, in order, frame after frame");
		check(onThread, "and on the thread that flushes them, not on a worker");
		check(empty, "nothing stays held after a flush");

		MarkReports direct;
		std::vector<std::string> heard;
		direct.report(2, { "a", "b" }, [&](int, const std::string& mark) { heard.push_back(mark); });
		check(heard == std::vector<std::string>{ "a", "b" } && direct.held() == 0, "not deferred: reported at once");

		MarkReports dropped;
		dropped.defer(true);
		dropped.report(1, { "x" }, nullptr);
		dropped.defer(false);
		dropped.flush(nullptr);
		check(dropped.held() == 0, "held without a handler: dropped at the flush, not kept for later");
	}

	std::printf("%s (%d failures)\n", g_failures == 0 ? "ALL PASSED" : "FAILED", g_failures);
	return g_failures == 0 ? 0 : 1;
}
