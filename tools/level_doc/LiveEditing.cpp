///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: LiveEditing.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "LiveEditing.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

#ifdef _WIN32
#include <process.h>
#define PAKAL_GETPID _getpid
#else
#include <unistd.h>
#define PAKAL_GETPID getpid
#endif

namespace Pakal::LiveEditing
{
	namespace fs = std::filesystem;

	namespace
	{
		const double kStaleSeconds = 4.0;   // a heartbeat older than this: the editor is gone

		double now_seconds()
		{
			return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
		}

		bool read_json(const fs::path& path, Json& out)
		{
			std::ifstream in(path, std::ios::binary);
			if (!in) return false;
			out = Json::parse(in, nullptr, false);
			return !out.is_discarded();
		}

		// Whole or not at all: a reader never sees half of it
		bool write_json(const fs::path& path, const Json& json)
		{
			const fs::path temporary = path.string() + ".tmp";
			{
				std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
				if (!file) return false;
				file << json.dump(1) << "\n";
			}
			std::error_code code;
			fs::rename(temporary, path, code);
			if (code)
			{
				fs::remove(path, code);
				fs::rename(temporary, path, code);
			}
			return !code;
		}
	}

	std::string folder_of(const std::string& levelPath)
	{
		const fs::path file(levelPath);
		return (file.parent_path() / (file.stem().string() + ".live")).string();
	}

	// --- the editor's side ---

	Host::~Host()
	{
		close();
	}

	bool Host::open(const std::string& levelPath)
	{
		if (levelPath == m_level && !m_folder.empty()) return true;
		close();
		m_level = levelPath;
		m_folder = folder_of(levelPath);
		std::error_code code;
		fs::create_directories(fs::path(m_folder) / "inbox", code);
		fs::create_directories(fs::path(m_folder) / "outbox", code);
		return !code;
	}

	void Host::close()
	{
		if (m_folder.empty()) return;
		std::error_code code;
		fs::remove_all(m_folder, code);
		m_folder.clear();
		m_level.clear();
	}

	void Host::beat(const Json& state)
	{
		if (m_folder.empty()) return;
		write_json(fs::path(m_folder) / "editor.json", Json{ { "pid", PAKAL_GETPID() }, { "time", now_seconds() }, { "level", m_level }, { "state", state } });
	}

	std::vector<std::pair<std::string, Json>> Host::take_requests()
	{
		std::vector<std::pair<std::string, Json>> requests;
		if (m_folder.empty()) return requests;
		std::error_code code;
		std::vector<fs::path> files;
		for (fs::directory_iterator it(fs::path(m_folder) / "inbox", code), end; !code && it != end; it.increment(code))
		{
			if (it->path().extension() == ".json") files.push_back(it->path());
		}
		std::sort(files.begin(), files.end());   // the ids start with the time they were made
		for (const auto& file : files)
		{
			Json request;
			const bool read = read_json(file, request);
			fs::remove(file, code);
			requests.emplace_back(file.stem().string(), read ? request : Json());
		}
		return requests;
	}

	void Host::answer(const std::string& id, const Json& reply)
	{
		if (m_folder.empty()) return;
		write_json(fs::path(m_folder) / "outbox" / (id + ".json"), reply);
	}

	// --- the other side ---

	bool editor_open(const std::string& levelPath, Json* state)
	{
		Json beat;
		if (!read_json(fs::path(folder_of(levelPath)) / "editor.json", beat) || !beat.is_object()) return false;
		const double age = now_seconds() - beat.value("time", 0.0);
		if (age > kStaleSeconds || age < -kStaleSeconds) return false;
		if (state) *state = beat.value("state", Json::object());
		return true;
	}

	bool ask(const std::string& levelPath, const Json& request, Json& reply, std::string& error, double timeoutSeconds)
	{
		const fs::path folder(folder_of(levelPath));
		if (!editor_open(levelPath))
		{
			error = "no editor is open on '" + levelPath + "'";
			return false;
		}
		static std::atomic<int> counter{ 0 };
		char id[64];
		std::snprintf(id, sizeof(id), "%014.3f_%d_%d", now_seconds(), static_cast<int>(PAKAL_GETPID()), counter++);
		const fs::path answer = folder / "outbox" / (std::string(id) + ".json");
		if (!write_json(folder / "inbox" / (std::string(id) + ".json"), request))
		{
			error = "cannot write to '" + folder.string() + "'";
			return false;
		}
		const auto until = std::chrono::steady_clock::now() + std::chrono::duration<double>(timeoutSeconds);
		while (std::chrono::steady_clock::now() < until)
		{
			std::error_code code;
			if (fs::exists(answer, code) && read_json(answer, reply))
			{
				fs::remove(answer, code);
				return true;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
		}
		// not taken: it should not be done later, unseen
		std::error_code code;
		const bool taken = !fs::remove(folder / "inbox" / (std::string(id) + ".json"), code);
		error = taken ? "the editor took the request but did not answer in time (it may still do it)" : "the editor did not take the request (it was not done)";
		return false;
	}
}
