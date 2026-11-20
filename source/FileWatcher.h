///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: FileWatcher.h
// Purpose: Files on the disk watched for changes, for reloading what they feed while the game runs (docs/UI_RELOAD.md).
//          A worker thread looks at each file's time and size every so often (0.5 s by default: plain std::filesystem,
//          the same on every platform); a change counts once the file has stayed the same for one more look (an editor
//          that writes a file in steps is seen once). The callbacks run on the thread that calls dispatch(), never on the
//          worker: the engine calls it from its own thread, the one the game's states run on.
//          Games reach it through ResourceManager::watch (the paths they read with); this class is the mechanism.
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once

#include "Config.h"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <thread>

// Whether this build reloads files: the desktop's builds, and Android's only with the engine's tools (build_apk.py --tools).
// PAKAL_NO_FILE_RELOAD (cmake -DPAKAL_FILE_RELOAD=OFF) takes it out of any build: a build for players.
#if !defined(PAKAL_NO_FILE_RELOAD) && (!defined(__ANDROID__) || defined(PAKAL_ENGINE_TOOLS))
#define PAKAL_FILE_RELOAD 1
#else
#define PAKAL_FILE_RELOAD 0
#endif

namespace Pakal
{
	class _PAKALExport FileWatcher final
	{
	public:
		// The name the file was watched by (watch's 'name')
		using Callback = std::function<void(const std::string& name)>;

		explicit FileWatcher(std::chrono::milliseconds interval = std::chrono::milliseconds(500));
		~FileWatcher();
		FileWatcher(const FileWatcher&) = delete;
		FileWatcher& operator=(const FileWatcher&) = delete;

		// A file on the disk ('file': a path the process can open) watched; the callback gets 'name' (the file's path when it
		// is empty). A file that is not there yet is watched too: it appearing is a change. The first watch starts the worker.
		// Returns an id for unwatch (never 0). Any thread.
		unsigned watch(const std::string& file, Callback callback, const std::string& name = std::string());
		// No more callbacks for it (one already running finishes). Any thread, a callback too.
		void unwatch(unsigned id);
		bool watching(unsigned id) const;
		// The callbacks of the files that changed since the last call, run here, on the caller's thread; how many ran. A
		// callback may watch and unwatch (its own id too).
		size_t dispatch();
		// The worker stopped (watch starts it again)
		void stop();
		size_t size() const;

	private:
		struct Stamp
		{
			bool exists = false;
			uintmax_t size = 0;
			int64_t time = 0;
			bool operator==(const Stamp& other) const { return exists == other.exists && size == other.size && time == other.time; }
			bool operator!=(const Stamp& other) const { return !(*this == other); }
		};
		struct Entry
		{
			std::string file, name;
			Callback callback;
			Stamp reported;   // what the last change reported (or what it was when watched)
			Stamp seen;       // what the last look saw: a change counts when two looks agree
			bool pending = false;
		};
		static Stamp stamp_of(const std::string& file);
		void run();

		const std::chrono::milliseconds m_interval;
		mutable std::mutex m_mutex;
		std::condition_variable m_wake;
		std::map<unsigned, Entry> m_entries;
		unsigned m_next = 1;
		std::thread m_worker;
		bool m_stop = false;
	};
}
