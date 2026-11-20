///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: FileWatcherTests.cpp
// Purpose: The file watcher behind reloading in dev builds (docs/UI_RELOAD.md): a file changed on the disk fires its
//          callback once, on the thread that dispatches (never the worker); an unchanged file fires nothing; a file
//          unwatched fires nothing; a file that appears counts as changed; ResourceManager::watch delivers through
//          dispatch_changes() and forgets what it kept of the file, so reading it again reads the new one.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "FileWatcher.h"
#include "ResourceManager.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <thread>

namespace
{
	namespace fs = std::filesystem;
	using namespace std::chrono_literals;

	void write(const fs::path& file, const std::string& text)
	{
		std::ofstream out(file, std::ios::binary | std::ios::trunc);
		out << text;
	}

	// dispatch() called from this thread for a while; true when 'done' says so before the time is up
	bool pump(const std::function<void()>& dispatch, const std::function<bool()>& done, std::chrono::milliseconds limit)
	{
		const auto until = std::chrono::steady_clock::now() + limit;
		while (std::chrono::steady_clock::now() < until)
		{
			dispatch();
			if (done()) return true;
			std::this_thread::sleep_for(10ms);
		}
		dispatch();
		return done();
	}
}

int main()
{
	int failures = 0;
	auto check = [&](bool ok, const char* message)
	{
		std::printf("[%s] %s\n", ok ? "PASS" : "FAIL", message);
		if (!ok) ++failures;
	};

	const fs::path folder = fs::temp_directory_path() / ("pakal_file_watcher_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	fs::create_directories(folder);
	const fs::path file = folder / "strings.csv";
	write(file, "key,en\nHELLO,Hello\n");
	const std::thread::id self = std::this_thread::get_id();

	{
		Pakal::FileWatcher watcher(20ms);
		int fired = 0;
		bool sameThread = true;
		std::string name;
		const unsigned id = watcher.watch(file.string(), [&](const std::string& changed)
		{
			++fired;
			name = changed;
			sameThread = sameThread && std::this_thread::get_id() == self;
		}, "Assets/strings.csv");
		check(id != 0 && watcher.watching(id), "a file is watched");

		pump([&]() { watcher.dispatch(); }, [&]() { return fired > 0; }, 200ms);
		check(fired == 0, "an unchanged file fires nothing");

		write(file, "key,en\nHELLO,Hello there\n");
		check(pump([&]() { watcher.dispatch(); }, [&]() { return fired > 0; }, 3000ms), "a changed file fires its callback");
		pump([&]() { watcher.dispatch(); }, [&]() { return false; }, 300ms);
		check(fired == 1, "it fires once for one change");
		check(sameThread, "on the thread that dispatches, not the worker");
		check(name == "Assets/strings.csv", "the callback gets the name it was watched by");

		watcher.unwatch(id);
		write(file, "key,en\nHELLO,Hi\n");
		pump([&]() { watcher.dispatch(); }, [&]() { return false; }, 300ms);
		check(fired == 1 && !watcher.watching(id), "an unwatched file fires nothing");

		// a file that is not there yet: it appearing is a change (a copy pushed later over the one in a package)
		const fs::path later = folder / "later.json";
		int appeared = 0;
		watcher.watch(later.string(), [&](const std::string&) { ++appeared; });
		write(later, "{}");
		check(pump([&]() { watcher.dispatch(); }, [&]() { return appeared > 0; }, 3000ms) && appeared == 1, "a file that appears counts as changed");

		// a callback that unwatches itself and watches again (what a reloaded document does)
		unsigned self_id = 0;
		int rewatched = 0;
		std::function<void(const std::string&)> again = [&](const std::string&)
		{
			++rewatched;
			watcher.unwatch(self_id);
			self_id = watcher.watch(file.string(), again);
		};
		self_id = watcher.watch(file.string(), again);
		write(file, "key,en\nHELLO,Hey you\n");
		check(pump([&]() { watcher.dispatch(); }, [&]() { return rewatched > 0; }, 3000ms) && rewatched == 1 && watcher.watching(self_id),
			"a callback may unwatch itself and watch again");
		watcher.stop();
	}

#if PAKAL_FILE_RELOAD
	{
		// Through the resource manager, by the path the game reads with (under the root folder)
		Pakal::ResourceManager& resources = ResourceMgr;
		resources.set_root_path(folder.string());
		check(resources.reload_enabled(), "files reload in this build (desktop)");
		check(resources.read_text("strings.csv") == "key,en\nHELLO,Hey you\n", "the file is read under the root folder");
		const auto held = resources.read_bytes("strings.csv");   // (held: without forgetting, a read would get these bytes)
		int fired = 0;
		bool sameThread = true;
		std::string changed;
		const unsigned id = resources.watch("strings.csv", [&](const std::string& path)
		{
			++fired;
			changed = resources.read_text(path);
			sameThread = sameThread && std::this_thread::get_id() == self;
		});
		check(id != 0, "ResourceManager::watch watches it");
		std::this_thread::sleep_for(50ms);
		write(file, "key,en\nHELLO,Hola\n");
		check(pump([&]() { resources.dispatch_changes(); }, [&]() { return fired > 0; }, 5000ms), "dispatch_changes() runs the callback");
		check(fired == 1 && sameThread, "once, on the thread that dispatches");
		check(changed == "key,en\nHELLO,Hola\n", "reading it in the callback reads the new file");
		resources.unwatch(id);
		resources.terminate();   // stops the worker
		(void)held;
	}
#else
	check(!ResourceMgr.reload_enabled() && ResourceMgr.watch("x", [](const std::string&) {}) == 0, "a build without reloading watches nothing");
#endif

	std::error_code error;
	fs::remove_all(folder, error);
	std::printf("%s\n", failures == 0 ? "All file watcher tests passed" : "File watcher tests FAILED");
	return failures == 0 ? 0 : 1;
}
