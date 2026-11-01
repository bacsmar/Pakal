///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Profiler.cpp
// Purpose: What a run costs - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "Profiler.h"
#include "LogMgr.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <string>

#if defined(_WIN32)
#	define WIN32_LEAN_AND_MEAN
#	define NOMINMAX
#	include <windows.h>
#	include <psapi.h>
#elif defined(__ANDROID__)
#	include <dlfcn.h>
#	include <sys/system_properties.h>
#	include <unistd.h>
#else
#	include <unistd.h>
#endif

namespace Pakal
{
	namespace
	{
		// The value at 'share' (0..1) of times sorted
		double percentile(std::vector<float>& times, double share)
		{
			if (times.empty()) return 0.0;
			std::sort(times.begin(), times.end());
			const size_t index = std::min(times.size() - 1, static_cast<size_t>(share * static_cast<double>(times.size() - 1) + 0.5));
			return times[index];
		}

		// The process's resident memory, in megabytes (0 where it cannot be read)
		double process_megabytes()
		{
#if defined(_WIN32)
			PROCESS_MEMORY_COUNTERS counters{};
			if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters))) return counters.WorkingSetSize / (1024.0 * 1024.0);
			return 0.0;
#else
			std::ifstream statm("/proc/self/statm");
			long pages = 0, resident = 0;
			if (statm >> pages >> resident) return resident * static_cast<double>(sysconf(_SC_PAGESIZE)) / (1024.0 * 1024.0);
			return 0.0;
#endif
		}

#if defined(__ANDROID__)
		// The device's thermal status (Android 10+: 0 none, 1 light, 2 moderate, 3 severe, 4 critical...), -1 unknown. The
		// functions are looked up at run time: the app also runs on older versions.
		int thermal_status()
		{
			using Acquire = void* (*)();
			using Status = int (*)(void*);
			static void* library = dlopen("libandroid.so", RTLD_NOW);
			static Acquire acquire = library ? reinterpret_cast<Acquire>(dlsym(library, "AThermal_acquireManager")) : nullptr;
			static Status status = library ? reinterpret_cast<Status>(dlsym(library, "AThermal_getCurrentThermalStatus")) : nullptr;
			static void* manager = acquire ? acquire() : nullptr;
			return manager && status ? status(manager) : -1;
		}

		std::string device_model()
		{
			char model[PROP_VALUE_MAX] = {};
			__system_property_get("ro.product.model", model);
			return model;
		}
#endif
	}

	Profiler& Profiler::get()
	{
		static Profiler profiler;
		return profiler;
	}

	Profiler::Profiler()
	{
		const char* path = std::getenv("PAKAL_PROFILE");
		m_enabled = (path && *path) || std::getenv("PAKAL_FPS_LOG") != nullptr;
		if (path && *path && std::string(path) != "1")
		{
			m_file = std::fopen(path, "w");
			if (!m_file) LOG_WARNING("[Profiler] cannot write %s", path);
		}
		m_start = m_last = std::chrono::steady_clock::now();
		if (const char* skip = std::getenv("PAKAL_PROFILE_SKIP")) m_skipSeconds = std::atof(skip);
		if (const char* run = std::getenv("PAKAL_PROFILE_SECONDS")) m_runSeconds = std::atof(run);
#if defined(_WIN32)
		m_info["platform"] = "windows";
		m_info["device"] = "windows";
#elif defined(__ANDROID__)
		m_info["platform"] = "android";
		m_info["device"] = device_model();
#else
		m_info["platform"] = "unix";
		m_info["device"] = "unix";
#endif
	}

	void Profiler::add(const char* section, double milliseconds)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		Section& s = m_sections[section];
		s.total += milliseconds;
		s.worst = std::max(s.worst, milliseconds);
		++s.calls;
	}

	void Profiler::counter(const std::string& name, double value)
	{
		if (!m_enabled) return;
		std::lock_guard<std::mutex> lock(m_mutex);
		m_counters[name] = value;
	}

	void Profiler::info(const std::string& name, const std::string& value)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_info[name] = value;
		m_infoWritten = false;   // written again with the next record
	}

	void Profiler::frame(double milliseconds)
	{
		if (!m_enabled) return;
		const auto now = std::chrono::steady_clock::now();
		std::lock_guard<std::mutex> lock(m_mutex);
		m_second.push_back(static_cast<float>(milliseconds));
		// past the warm-up: a frame that began after it (one that spans it keeps the loading's time out)
		if (std::chrono::duration<double>(now - m_start).count() - milliseconds / 1000.0 >= m_skipSeconds) m_all.push_back(static_cast<float>(milliseconds));
		const double seconds = std::chrono::duration<double>(now - m_last).count();
		if (seconds >= 1.0)
		{
			write_second(seconds);
			m_last = now;
		}
	}

	// One record: this second's frames, the time of each section per frame, the counters (called locked)
	void Profiler::write_second(double seconds)
	{
		m_counters["mem.process_mb"] = process_megabytes();
#if defined(__ANDROID__)
		m_counters["thermal.status"] = thermal_status();
#endif
		const size_t frames = m_second.size();
		nlohmann::json record;
		record["t"] = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count();
		record["fps"] = frames / seconds;
		std::vector<float> times = m_second;
		record["frame_ms"] = { { "p50", percentile(times, 0.5) }, { "p95", percentile(times, 0.95) }, { "p99", percentile(times, 0.99) },
			{ "max", times.empty() ? 0.0 : times.back() } };
		nlohmann::json sections = nlohmann::json::object();
		std::string line;
		// the summary's sections count only the seconds begun after the warm-up, over their own frames (ZM-QA-001)
		const bool counted = std::chrono::duration<double>(m_last - m_start).count() >= m_skipSeconds;
		if (counted) m_allSectionFrames += frames;
		for (const auto& [name, s] : m_sections)
		{
			// per frame: the share of this second's time per drawn frame
			const double perFrame = frames ? s.total / frames : 0.0;
			sections[name] = { { "ms_per_frame", perFrame }, { "worst_ms", s.worst }, { "calls", s.calls } };
			if (counted)
			{
				Section& all = m_allSections[name];
				all.total += s.total;
				all.worst = std::max(all.worst, s.worst);
				all.calls += s.calls;
			}
			char part[96];
			std::snprintf(part, sizeof(part), " %s=%.2f", name.c_str(), perFrame);
			line += part;
		}
		record["sections"] = sections;
		record["counters"] = m_counters;
		if (!m_infoWritten)
		{
			record["info"] = m_info;
			m_infoWritten = true;
		}
		LOG_INFO("[Profile] %.1f fps, frame p50 %.1f p95 %.1f p99 %.1f ms, %.0f MB;%s", record["fps"].get<double>(),
			record["frame_ms"]["p50"].get<double>(), record["frame_ms"]["p95"].get<double>(), record["frame_ms"]["p99"].get<double>(),
			m_counters["mem.process_mb"], line.c_str());
		if (m_file)
		{
			const std::string text = record.dump() + "\n";
			std::fputs(text.c_str(), m_file);
			std::fflush(m_file);
		}
		m_second.clear();
		m_sections.clear();
	}

	bool Profiler::start(const std::string& path)
	{
		if (m_recording || (m_enabled && !m_finished)) finish();
		std::lock_guard<std::mutex> lock(m_mutex);
		std::FILE* file = std::fopen(path.c_str(), "w");
		if (!file)
		{
			LOG_WARNING("[Profiler] cannot write %s", path.c_str());
			return false;
		}
		m_file = file;
		m_second.clear();
		m_all.clear();
		m_sections.clear();
		m_allSections.clear();
		m_allSectionFrames = 0;
		m_finished = false;
		m_infoWritten = false;
		m_skipSeconds = 0.0;
		m_runSeconds = 0.0;
		m_start = m_last = std::chrono::steady_clock::now();
		m_recording = true;
		m_recordingPath = path;
		m_enabled = true;
		LOG_INFO("[Profiler] recording to %s", path.c_str());
		return true;
	}

	void Profiler::stop()
	{
		if (!m_recording) return;
		finish();
		std::lock_guard<std::mutex> lock(m_mutex);
		m_recording = false;
		m_enabled = false;   // what the environment asked for at the start is over by now (its file is closed)
	}

	double Profiler::recording_seconds() const
	{
		return m_recording ? std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count() : 0.0;
	}

	bool Profiler::time_is_up() const
	{
		return m_enabled && m_runSeconds > 0.0 && std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count() >= m_runSeconds;
	}

	void Profiler::finish()
	{
		if (!m_enabled) return;
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_finished) return;
		m_finished = true;
		const double seconds = std::max(0.0, std::chrono::duration<double>(std::chrono::steady_clock::now() - m_start).count() - m_skipSeconds);
		nlohmann::json summary;
		summary["summary"] = true;
		summary["seconds"] = seconds;
		summary["frames"] = m_all.size();
		std::vector<float> times = m_all;
		summary["fps"] = seconds > 0.0 ? m_all.size() / seconds : 0.0;
		summary["frame_ms"] = { { "p50", percentile(times, 0.5) }, { "p95", percentile(times, 0.95) }, { "p99", percentile(times, 0.99) },
			{ "max", times.empty() ? 0.0 : times.back() } };
		nlohmann::json sections = nlohmann::json::object();
		for (const auto& [name, s] : m_allSections)
		{
			sections[name] = { { "ms_per_frame", m_allSectionFrames ? s.total / m_allSectionFrames : 0.0 }, { "worst_ms", s.worst }, { "calls", s.calls } };
		}
		summary["sections"] = sections;
		summary["counters"] = m_counters;
		summary["info"] = m_info;
		LOG_INFO("[Profile] run: %.1f fps over %.0f s, frame p50 %.1f p95 %.1f p99 %.1f ms", summary["fps"].get<double>(), seconds,
			summary["frame_ms"]["p50"].get<double>(), summary["frame_ms"]["p95"].get<double>(), summary["frame_ms"]["p99"].get<double>());
		if (m_file)
		{
			const std::string text = summary.dump() + "\n";
			std::fputs(text.c_str(), m_file);
			std::fclose(m_file);
			m_file = nullptr;
		}
	}
}
