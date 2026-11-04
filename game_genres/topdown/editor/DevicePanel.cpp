///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: DevicePanel.cpp
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/editor/DevicePanel.h"

#include "mcp/McpClient.h"

#include <dear-imgui/imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

#if defined(_WIN32)
#define popen _popen
#define pclose _pclose
#endif

namespace Pakal::TopDown
{
	using Pakal::Json;

	namespace
	{
		// adb: ADB, else ANDROID_HOME / ANDROID_SDK_ROOT's platform-tools, else on the PATH
		std::string adb()
		{
			if (const char* path = std::getenv("ADB")) return std::string("\"") + path + "\"";
			for (const char* variable : { "ANDROID_HOME", "ANDROID_SDK_ROOT" })
			{
				const char* root = std::getenv(variable);
				if (!root) continue;
				std::error_code code;
				for (const char* name : { "adb.exe", "adb" })
				{
					const std::filesystem::path candidate = std::filesystem::path(root) / "platform-tools" / name;
					if (std::filesystem::is_regular_file(candidate, code)) return "\"" + candidate.string() + "\"";
				}
			}
			return "adb";
		}

		std::string run(const std::string& command)
		{
			std::string out;
#if defined(_WIN32)
			// cmd /c takes the whole line in quotes when it starts with a quoted program
			FILE* pipe = popen(("\"" + command + " 2>&1\"").c_str(), "r");
#else
			FILE* pipe = popen((command + " 2>&1").c_str(), "r");
#endif
			if (!pipe) return out;
			char buffer[512];
			while (std::fgets(buffer, sizeof(buffer), pipe)) out += buffer;
			pclose(pipe);
			return out;
		}

		double number(const Json& value, const char* key, double otherwise = 0.0)
		{
			const auto it = value.find(key);
			return it != value.end() && it->is_number() ? it->get<double>() : otherwise;
		}

		// A settings object reduced to what a snapshot puts back: the quality, its preset, post-process, shadows, scale, FSR
		Json restorable(const Json& settings)
		{
			if (!settings.is_object()) return Json();
			Json out = Json::object();
			for (const char* key : { "quality", "post", "shadows", "renderScale", "fsr", "sharpness", "governor" })
			{
				if (settings.contains(key)) out[key] = settings[key];
			}
			const std::string quality = settings.value("quality", std::string());
			if (settings.contains("presets") && settings["presets"].contains(quality)) out["preset"] = settings["presets"][quality];
			return out;
		}
	}

	DevicePanel::DevicePanel()
	{
		set_transport({});
	}

	DevicePanel::~DevicePanel()
	{
		disconnect();
		stop_recording();
	}

	void DevicePanel::set_transport(Transport transport)
	{
		if (transport)
		{
			m_transport = std::move(transport);
			return;
		}
		m_transport = [this](const std::string& tool, const Json& arguments, Json& out, std::string& error)
		{
			McpClient client(static_cast<unsigned short>(m_port), m_token, 8.0);
			nlohmann::json answer;
			if (!client.call(tool, nlohmann::json::parse(arguments.dump()), answer, error)) return false;
			out = Json::parse(answer.dump());
			return true;
		};
	}

	void DevicePanel::connect()
	{
		if (m_running) return;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_history.clear();
			m_clock = -1.0;
			m_settingsWanted = true;
			m_error.clear();
		}
		m_running = true;
		m_worker = std::thread([this]() { work(); });
	}

	void DevicePanel::disconnect()
	{
		if (!m_running.exchange(false)) return;
		m_wake.notify_all();
		if (m_worker.joinable()) m_worker.join();
	}

	void DevicePanel::work()
	{
		auto last = std::chrono::steady_clock::now() - std::chrono::seconds(2);
		while (m_running)
		{
			std::vector<Json> pending;
			bool wantSettings = false;
			double clock = -1.0;
			{
				std::unique_lock<std::mutex> lock(m_mutex);
				m_wake.wait_for(lock, std::chrono::milliseconds(100), [this]() { return !m_running || !m_pending.empty() || m_settingsWanted; });
				pending.swap(m_pending);
				wantSettings = m_settingsWanted;
				m_settingsWanted = false;
				clock = m_clock;
			}
			if (!m_running) break;
			std::string error;
			for (const auto& arguments : pending)
			{
				Json answer;
				if (m_transport("set_settings", arguments, answer, error))
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					m_settings = answer;
				}
			}
			if (wantSettings)
			{
				Json answer;
				if (m_transport("get_settings", Json::object(), answer, error))
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					m_settings = answer;
				}
			}
			if (std::chrono::steady_clock::now() - last >= std::chrono::milliseconds(900))
			{
				last = std::chrono::steady_clock::now();
				Json answer;
				if (m_transport("get_profile", { { "after", clock } }, answer, error))
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					for (const auto& record : answer.value("records", Json::array()))
					{
						m_clock = std::max(m_clock, number(record, "clock", m_clock));
						m_history.push_back(record);
						if (m_record)
						{
							const std::string line = record.dump() + "\n";
							std::fputs(line.c_str(), m_record);
							std::fflush(m_record);
						}
					}
					while (m_history.size() > kHistory) m_history.pop_front();
				}
			}
			std::lock_guard<std::mutex> lock(m_mutex);
			m_error = error;
		}
	}

	size_t DevicePanel::records() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_history.size();
	}

	Json DevicePanel::settings() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_settings;
	}

	std::string DevicePanel::error() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_error;
	}

	void DevicePanel::set(const Json& arguments)
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_pending.push_back(arguments);
		}
		m_wake.notify_all();
	}

	void DevicePanel::keep(int which)
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_snapshots[which] = restorable(m_settings);
		m_snapshotMs[which] = 0.0;
	}

	void DevicePanel::apply(int which)
	{
		if (!m_snapshots[which].is_object()) return;
		set(m_snapshots[which]);
		std::lock_guard<std::mutex> lock(m_mutex);
		// measured over the seconds after it settles (2 s)
		m_measuring = which;
		m_measureFrom = m_clock + 2.0;
		m_snapshotMs[which] = 0.0;
	}

	bool DevicePanel::start_recording(const std::string& path)
	{
		stop_recording();
		std::error_code code;
		std::filesystem::create_directories(std::filesystem::path(path).parent_path(), code);
		std::lock_guard<std::mutex> lock(m_mutex);
		m_record = std::fopen(path.c_str(), "w");
		m_recordPath = path;
		return m_record != nullptr;
	}

	void DevicePanel::stop_recording()
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		if (m_record) std::fclose(m_record);
		m_record = nullptr;
	}

	void DevicePanel::refresh_devices()
	{
		m_devices.clear();
		const std::string out = run(adb() + " devices");
		size_t at = out.find('\n');
		while (at != std::string::npos && at + 1 < out.size())
		{
			const size_t end = out.find('\n', at + 1);
			std::string line = out.substr(at + 1, end == std::string::npos ? std::string::npos : end - at - 1);
			while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
			if (!line.empty() && line.find('\t') != std::string::npos) m_devices.push_back(line);
			at = end;
		}
		m_status = m_devices.empty() ? "adb sees no device" : std::to_string(m_devices.size()) + " device(s)";
	}

	bool DevicePanel::forward(const std::string& serial, std::string& error)
	{
		const std::string port = std::to_string(m_port);
		const std::string out = run(adb() + " -s " + serial + " forward tcp:" + port + " tcp:" + port);
		if (out.find("error") != std::string::npos)
		{
			error = out;
			return false;
		}
		return true;
	}

	void DevicePanel::draw(bool* open)
	{
		const ImGuiIO& io = ImGui::GetIO();
		ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f - 300.0f, 60.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(600.0f, 640.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Device", open))
		{
			ImGui::End();
			return;
		}
		// the target: the game on this computer, or a device (its port forwarded)
		ImGui::BeginDisabled(m_running);
		ImGui::SetNextItemWidth(200.0f);
		if (ImGui::BeginCombo("Target", m_target.c_str()))
		{
			if (ImGui::Selectable("local", m_target == "local")) m_target = "local";
			for (const auto& device : m_devices)
			{
				const std::string serial = device.substr(0, device.find('\t'));
				if (ImGui::Selectable(device.c_str(), m_target == serial)) m_target = serial;
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		if (ImGui::Button("adb devices")) refresh_devices();
		ImGui::SetNextItemWidth(90.0f);
		ImGui::InputInt("Port", &m_port, 0);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(160.0f);
		ImGui::InputTextWithHint("##token", "token (PAKAL_MCP_TOKEN)", m_token, sizeof(m_token), ImGuiInputTextFlags_Password);
		ImGui::EndDisabled();
		if (!m_running)
		{
			if (ImGui::Button("Connect"))
			{
				std::string error;
				if (m_target == "local" || forward(m_target, error)) connect();
				else m_status = error;
			}
		}
		else if (ImGui::Button("Disconnect"))
		{
			disconnect();
		}
		ImGui::SameLine();
		const std::string error = this->error();
		if (!error.empty()) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", error.c_str());
		else ImGui::TextDisabled("%s", m_running ? (std::to_string(records()) + " seconds received").c_str() : m_status.c_str());
		ImGui::TextDisabled("The game listens with PAKAL_MCP_PORT (a device: build_apk.py --tools, pakal_env.txt). docs/PROFILING.md");

		if (ImGui::BeginTabBar("##device"))
		{
			if (ImGui::BeginTabItem("Graphs"))
			{
				draw_graphs();
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Settings"))
			{
				draw_settings();
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::End();
	}

	void DevicePanel::draw_graphs()
	{
		std::deque<Json> history;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			history = m_history;
		}
		// recording what comes in
		if (!recording())
		{
			if (ImGui::Button("Record"))
			{
				char name[64];
				std::snprintf(name, sizeof(name), "editor_cache/device_%lld.ndjson", static_cast<long long>(std::time(nullptr)));
				start_recording(name);
			}
		}
		else
		{
			if (ImGui::Button("Stop recording")) stop_recording();
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.4f, 1.0f), "recording to %s (tools/pakal_prof.py reads it)", m_recordPath.c_str());
		}
		if (history.empty())
		{
			ImGui::TextDisabled("nothing yet");
			return;
		}
		// one line per series, over the seconds kept
		auto series = [&](const char* label, const std::function<float(const Json&)>& value, const char* unit)
		{
			std::vector<float> values;
			for (const auto& record : history) values.push_back(value(record));
			float high = 0.0f;
			for (float v : values) high = std::max(high, v);
			char overlay[96];
			std::snprintf(overlay, sizeof(overlay), "%s: %.2f %s (top %.2f)", label, values.back(), unit, high);
			ImGui::PlotLines("##series", values.data(), static_cast<int>(values.size()), 0, overlay, 0.0f, std::max(high * 1.15f, 0.001f), ImVec2(-1.0f, 46.0f));
		};
		auto counter = [](const char* name) { return [name](const Json& r) { return float(number(r.value("counters", Json::object()), name)); }; };
		ImGui::PushID("graphs");
		ImGui::PushID(0); series("fps", [](const Json& r) { return float(number(r, "fps")); }, ""); ImGui::PopID();
		ImGui::PushID(1); series("frame p50", [](const Json& r) { return float(number(r.value("frame_ms", Json::object()), "p50")); }, "ms"); ImGui::PopID();
		ImGui::PushID(2); series("frame p95", [](const Json& r) { return float(number(r.value("frame_ms", Json::object()), "p95")); }, "ms"); ImGui::PopID();
		ImGui::PushID(3); series("GPU", counter("gpu.frame_ms"), "ms"); ImGui::PopID();
		ImGui::PushID(4); series("draws", counter("gpu.draws"), ""); ImGui::PopID();
		ImGui::PushID(5); series("triangles", counter("gpu.primitives"), ""); ImGui::PopID();
		ImGui::PushID(6); series("memory", counter("mem.process_mb"), "MB"); ImGui::PopID();
		const Json& last = history.back();
		const Json counters = last.value("counters", Json::object());
		if (counters.contains("thermal.status")) { ImGui::PushID(7); series("thermal status", counter("thermal.status"), ""); ImGui::PopID(); }
		// the GPU per pass
		std::set<std::string> passes;
		for (auto it = counters.begin(); it != counters.end(); ++it)
		{
			if (it.key().rfind("gpu.view.", 0) == 0) passes.insert(it.key());
		}
		if (!passes.empty()) ImGui::SeparatorText("GPU per pass");
		int id = 100;
		for (const auto& pass : passes)
		{
			ImGui::PushID(id++);
			const std::string name = pass.substr(9);
			series(name.c_str(), counter(pass.c_str()), "");
			ImGui::PopID();
		}
		// the costliest sections of code, last second
		ImGui::SeparatorText("Sections (last second, ms per frame)");
		std::vector<std::pair<double, std::string>> sections;
		const Json all = last.value("sections", Json::object());
		for (auto it = all.begin(); it != all.end(); ++it) sections.push_back({ number(it.value(), "ms_per_frame"), it.key() });
		std::sort(sections.rbegin(), sections.rend());
		for (size_t i = 0; i < sections.size() && i < 10; ++i) ImGui::Text("%-24s %7.2f", sections[i].second.c_str(), sections[i].first);
		ImGui::PopID();
	}

	void DevicePanel::draw_settings()
	{
		const Json settings = this->settings();
		if (!settings.is_object())
		{
			ImGui::TextDisabled(m_running ? "asking..." : "connect first");
			return;
		}
		if (ImGui::Button("Read again"))
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_settingsWanted = true;
		}
		// A/B: two snapshots, each put back and measured
		ImGui::SameLine();
		for (int which = 0; which < 2; ++which)
		{
			ImGui::PushID(which);
			const char* label = which == 0 ? "A" : "B";
			if (ImGui::Button((std::string("Keep as ") + label).c_str())) keep(which);
			ImGui::SameLine();
			ImGui::BeginDisabled(!m_snapshots[which].is_object());
			if (ImGui::Button((std::string("Apply ") + label).c_str())) apply(which);
			ImGui::EndDisabled();
			ImGui::SameLine();
			ImGui::PopID();
		}
		{
			// the frame time after a snapshot was applied: the seconds received after it settled
			std::lock_guard<std::mutex> lock(m_mutex);
			if (m_measuring >= 0)
			{
				std::vector<double> times;
				for (const auto& record : m_history)
				{
					if (number(record, "clock") > m_measureFrom) times.push_back(number(record.value("frame_ms", Json::object()), "p50"));
				}
				if (!times.empty())
				{
					std::sort(times.begin(), times.end());
					m_snapshotMs[m_measuring] = times[times.size() / 2];
				}
			}
		}
		ImGui::Text("A %.2f ms  B %.2f ms", m_snapshotMs[0], m_snapshotMs[1]);
		if (ImGui::Button("Write back"))
		{
			// the game's own tool, which knows its data files
			Json answer;
			std::string error;
			const Json arguments = { { "settings", restorable(settings) } };
			if (m_transport("write_back", arguments, answer, error)) m_status = "written back: " + answer.dump();
			else m_status = "the game has no write_back tool (" + error + "): tools/pakal_device.py writeback --map";
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%s", m_status.c_str());

		// the settings, each applied as it changes
		auto send = [this](const char* group, const std::string& field, const Json& value)
		{
			Json arguments = Json::object();
			if (group) arguments[group] = { { field, value } };
			else arguments[field] = value;
			set(arguments);
		};
		ImGui::SeparatorText("Picture");
		static const char* kQualities[] = { "low", "medium", "high", "ultra", "extreme" };
		const std::string quality = settings.value("quality", std::string());
		if (ImGui::BeginCombo("Quality", quality.c_str()))
		{
			for (const char* q : kQualities)
			{
				if (ImGui::Selectable(q, quality == q)) send(nullptr, "quality", q);
			}
			ImGui::EndCombo();
		}
		float scale = static_cast<float>(number(settings, "renderScale", 1.0));
		ImGui::SliderFloat("Render scale", &scale, 0.3f, 1.0f, "%.2f");
		if (ImGui::IsItemDeactivatedAfterEdit()) send(nullptr, "renderScale", scale);
		bool fsr = settings.value("fsr", false), governor = settings.value("governor", false);
		if (ImGui::Checkbox("FSR", &fsr)) send(nullptr, "fsr", fsr);
		ImGui::SameLine();
		if (ImGui::Checkbox("Governor", &governor)) send(nullptr, "governor", governor);
		ImGui::TextDisabled("%s", settings.value("governorStatus", std::string()).c_str());
		// a group's fields: numbers dragged (sent when let go), switches at once
		auto group = [&](const char* title, const char* key, const Json& values)
		{
			if (!values.is_object() || !ImGui::CollapsingHeader(title)) return;
			ImGui::PushID(key);
			for (auto it = values.begin(); it != values.end(); ++it)
			{
				const std::string field = it.key();
				if (it->is_boolean())
				{
					bool value = it->get<bool>();
					if (ImGui::Checkbox(field.c_str(), &value)) send(key, field, value);
				}
				else if (it->is_number())
				{
					float value = it->get<float>();
					const float speed = std::max(0.001f, std::fabs(value) * 0.01f);
					ImGui::DragFloat(field.c_str(), &value, speed, 0.0f, 0.0f, it->is_number_integer() ? "%.0f" : "%.3f");
					if (ImGui::IsItemDeactivatedAfterEdit()) send(key, field, it->is_number_integer() ? Json(static_cast<long long>(value)) : Json(value));
				}
				else if (it->is_array() && it->size() == 3)
				{
					float value[3] = { (*it)[0].get<float>(), (*it)[1].get<float>(), (*it)[2].get<float>() };
					ImGui::DragFloat3(field.c_str(), value, 0.01f);
					if (ImGui::IsItemDeactivatedAfterEdit()) send(key, field, Json::array({ value[0], value[1], value[2] }));
				}
			}
			ImGui::PopID();
		};
		const Json presets = settings.value("presets", Json::object());
		group(("Preset (" + quality + ")").c_str(), "preset", presets.value(quality, Json::object()));
		group("Post-process", "post", settings.value("post", Json::object()));
		group("Shadows", "shadows", settings.value("shadows", Json::object()));
	}
}
