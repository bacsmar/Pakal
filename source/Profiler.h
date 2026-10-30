///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: Profiler.h
// Purpose: What a run costs, measured while it plays: the frame times (their percentiles each second
//          and over the whole run), the time of named sections of code (PAKAL_PROFILE_SCOPE, from any
//          thread), the memory of the process and the counters the subsystems set (GPU memory, models,
//          actors), and the device's temperature where the platform tells it. On with PAKAL_PROFILE
//          (a file: one JSON object per line, NDJSON, each second and a summary at the end; "1": only the
//          log) or PAKAL_FPS_LOG. PAKAL_PROFILE_SKIP=<s> leaves the first seconds (the loading) out of
//          the summary; PAKAL_PROFILE_SECONDS=<s> ends the run then. tools/pakal_prof.py reads the file,
//          tools/pakal_bench.py compares a run with a device's budget. Off, a scope costs one test of a flag.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Config.h"

#include <chrono>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace Pakal
{
	class _PAKALExport Profiler
	{
	public:
		static Profiler& get();

		bool enabled() const { return m_enabled; }

		// Time spent in a section of code this frame (ProfileScope adds it)
		void add(const char* section, double milliseconds);
		// The value of a counter now (GPU memory, models, actors...): the last one set is written each second
		void counter(const std::string& name, double value);
		// A fact of the run written once (the device, the level, the quality)
		void info(const std::string& name, const std::string& value);

		// A drawn frame is over: its time. Each second this writes a record (and the log line).
		void frame(double milliseconds);
		// The run is over: the summary record (every frame's percentiles), and the file is closed
		void finish();
		// A recording started while the program runs (a development menu): from now on, into this file (one before it
		// is finished first). stop() writes its summary and closes it, and the profiler goes back to how it started
		bool start(const std::string& path);
		void stop();
		bool recording() const { return m_recording; }
		const std::string& recording_path() const { return m_recordingPath; }
		double recording_seconds() const;
		// PAKAL_PROFILE_SECONDS have passed: the program should end (the engine asks each frame)
		bool time_is_up() const;

	private:
		Profiler();
		void write_second(double seconds);

		struct Section
		{
			double total = 0.0, worst = 0.0;
			unsigned calls = 0;
		};

		bool m_enabled = false;
		bool m_recording = false;
		std::string m_recordingPath;
		std::mutex m_mutex;
		std::FILE* m_file = nullptr;
		std::map<std::string, Section> m_sections;       // this second's
		std::map<std::string, double> m_counters;
		std::map<std::string, std::string> m_info;
		bool m_infoWritten = false;
		std::vector<float> m_second;                     // this second's frame times
		std::vector<float> m_all;                        // every frame's (the summary)
		std::map<std::string, Section> m_allSections;
		size_t m_allSectionFrames = 0;   // the frames of the seconds those sum (the warm-up left out)
		std::chrono::steady_clock::time_point m_start, m_last;
		bool m_finished = false;
		double m_skipSeconds = 0.0;   // PAKAL_PROFILE_SKIP
		double m_runSeconds = 0.0;    // PAKAL_PROFILE_SECONDS (0: no end)
	};

	// Adds the time from here to the end of the block to a section (its name must live until the block ends)
	class ProfileScope
	{
	public:
		explicit ProfileScope(const char* section) : m_section(Profiler::get().enabled() ? section : nullptr)
		{
			if (m_section) m_start = std::chrono::steady_clock::now();
		}
		~ProfileScope()
		{
			if (m_section) Profiler::get().add(m_section, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - m_start).count());
		}
		ProfileScope(const ProfileScope&) = delete;
		ProfileScope& operator=(const ProfileScope&) = delete;

	private:
		const char* m_section;
		std::chrono::steady_clock::time_point m_start;
	};
}

#define PAKAL_PROFILE_JOIN2(a, b) a##b
#define PAKAL_PROFILE_JOIN(a, b) PAKAL_PROFILE_JOIN2(a, b)
// Times the rest of the block as 'section' ("sim.step", "gfx.frame"...)
#define PAKAL_PROFILE_SCOPE(section) ::Pakal::ProfileScope PAKAL_PROFILE_JOIN(pakalProfileScope, __LINE__)(section)
