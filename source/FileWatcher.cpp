///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: FileWatcher.cpp
// Purpose: Files on the disk watched for changes - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "FileWatcher.h"

#include <filesystem>
#include <vector>

namespace Pakal
{
	FileWatcher::FileWatcher(std::chrono::milliseconds interval) : m_interval(interval)
	{
	}

	FileWatcher::~FileWatcher()
	{
		stop();
	}

	FileWatcher::Stamp FileWatcher::stamp_of(const std::string& file)
	{
		namespace fs = std::filesystem;
		Stamp stamp;
		std::error_code error;
		const fs::path path(file);
		if (!fs::is_regular_file(path, error)) return stamp;
		const auto time = fs::last_write_time(path, error);
		if (error) return stamp;
		const auto size = fs::file_size(path, error);
		if (error) return stamp;
		stamp.exists = true;
		stamp.size = size;
		stamp.time = static_cast<int64_t>(time.time_since_epoch().count());
		return stamp;
	}

	unsigned FileWatcher::watch(const std::string& file, Callback callback, const std::string& name)
	{
		Entry entry;
		entry.file = file;
		entry.name = name.empty() ? file : name;
		entry.callback = std::move(callback);
		entry.reported = entry.seen = stamp_of(file);
		std::lock_guard<std::mutex> lock(m_mutex);
		const unsigned id = m_next++;
		m_entries.emplace(id, std::move(entry));
		if (!m_worker.joinable())
		{
			m_stop = false;
			m_worker = std::thread([this]() { run(); });
		}
		return id;
	}

	void FileWatcher::unwatch(unsigned id)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_entries.erase(id);
	}

	bool FileWatcher::watching(unsigned id) const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_entries.count(id) != 0;
	}

	size_t FileWatcher::size() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_entries.size();
	}

	size_t FileWatcher::dispatch()
	{
		// taken out under the lock, run without it: a callback may watch and unwatch
		std::vector<std::pair<unsigned, std::string>> changed;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			for (auto& [id, entry] : m_entries)
			{
				if (!entry.pending) continue;
				entry.pending = false;
				changed.emplace_back(id, entry.name);
			}
		}
		size_t ran = 0;
		for (const auto& [id, name] : changed)
		{
			Callback callback;
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				auto found = m_entries.find(id);
				if (found == m_entries.end()) continue;   // unwatched by an earlier callback
				callback = found->second.callback;
			}
			if (callback)
			{
				callback(name);
				++ran;
			}
		}
		return ran;
	}

	void FileWatcher::stop()
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_stop = true;
		}
		m_wake.notify_all();
		if (m_worker.joinable()) m_worker.join();
	}

	void FileWatcher::run()
	{
		std::unique_lock<std::mutex> lock(m_mutex);
		while (!m_stop)
		{
			m_wake.wait_for(lock, m_interval, [this]() { return m_stop; });
			if (m_stop) break;
			// the files looked at without the lock (a slow disk does not hold watch() or dispatch())
			std::vector<std::pair<unsigned, std::string>> files;
			files.reserve(m_entries.size());
			for (const auto& [id, entry] : m_entries) files.emplace_back(id, entry.file);
			lock.unlock();
			std::vector<Stamp> stamps;
			stamps.reserve(files.size());
			for (const auto& file : files) stamps.push_back(stamp_of(file.second));
			lock.lock();
			for (size_t i = 0; i < files.size(); ++i)
			{
				auto found = m_entries.find(files[i].first);
				if (found == m_entries.end()) continue;
				Entry& entry = found->second;
				if (stamps[i] != entry.seen)
				{
					entry.seen = stamps[i];   // still being written, perhaps: the next look decides
				}
				else if (entry.seen != entry.reported)
				{
					entry.reported = entry.seen;
					entry.pending = true;
				}
			}
		}
	}
}
