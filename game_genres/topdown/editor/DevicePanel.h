///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: DevicePanel.h
// Purpose: The editor's Device window: a client of a running game's engine tools (source/mcp/EngineTools,
//          docs/PROFILING.md), the game on this computer or on an Android device through `adb forward`.
//          It draws the last minutes as graphs (frame time, the GPU's time per pass, draws, triangles,
//          memory, temperature), shows the settings the tools change (quality, its preset, post-process,
//          render scale, FSR, the governor) as fields that apply at once, keeps two snapshots of them to
//          switch between (A/B, each measured), records what it receives as the profiler's NDJSON
//          (tools/pakal_prof.py), and hands the tuned values to the game ("write_back", or a map file).
//          The requests run on a thread of its own: a tool waits for the game's graphics thread, which may be
//          the one that draws this window. tools/pakal_device.py does the same from a shell.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "LevelDocument.h"

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Pakal::TopDown
{
	class DevicePanel
	{
	public:
		// How a tool is called: true with its answer, false with what went wrong. By default the game's MCP server on
		// 127.0.0.1:<port> (McpClient); a test gives its own.
		using Transport = std::function<bool(const std::string& tool, const Pakal::Json& arguments, Pakal::Json& out, std::string& error)>;

		DevicePanel();
		~DevicePanel();
		DevicePanel(const DevicePanel&) = delete;
		DevicePanel& operator=(const DevicePanel&) = delete;

		void set_transport(Transport transport);
		// Starts asking (each second: the records after the last one; the settings when they change), or stops
		void connect();
		void disconnect();
		bool connected() const { return m_running; }

		// The window (graphics thread)
		void draw(bool* open);

		// What it holds (for a test): the records received, the last settings, the last error
		size_t records() const;
		Pakal::Json settings() const;
		std::string error() const;
		// Changes settings (as set_settings takes them) on the worker; the answer replaces the settings shown
		void set(const Pakal::Json& arguments);
		// Two snapshots of the settings, and putting one back
		void keep(int which);
		void apply(int which);
		const Pakal::Json& snapshot(int which) const { return m_snapshots[which]; }
		// Recording what comes in, into a file (NDJSON, the profiler's records)
		bool start_recording(const std::string& path);
		void stop_recording();
		bool recording() const { return m_record != nullptr; }

	private:
		void work();
		void refresh_devices();
		bool forward(const std::string& serial, std::string& error);
		void draw_graphs();
		void draw_settings();

		Transport m_transport;
		int m_port = 8765;
		char m_token[64] = {};
		std::vector<std::string> m_devices;   // "serial  state" from adb devices
		std::string m_target = "local";
		std::string m_status;

		std::thread m_worker;
		std::atomic<bool> m_running{ false };
		mutable std::mutex m_mutex;
		std::condition_variable m_wake;
		std::deque<Pakal::Json> m_history;    // the records, oldest first (the last kHistory)
		Pakal::Json m_settings;
		std::vector<Pakal::Json> m_pending;   // set_settings to send
		bool m_settingsWanted = true;
		double m_clock = -1.0;
		std::string m_error;
		std::FILE* m_record = nullptr;
		std::string m_recordPath;
		Pakal::Json m_snapshots[2];
		double m_snapshotMs[2] = { 0.0, 0.0 };   // frame p50 measured after each was applied (0: not yet)
		int m_measuring = -1;
		double m_measureFrom = 0.0;
		static constexpr size_t kHistory = 300;
	};
}
