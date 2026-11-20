#include "GameSettings.h"
#include "Engine.h"
#include "SoundManager.h"
#include "LogMgr.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>

namespace Pakal
{
	struct GameSettings::State
	{
		nlohmann::json values = nlohmann::json::object();
	};

	namespace
	{
		int step10(int percent) { return std::clamp((percent + 5) / 10 * 10, 0, 100); }
	}

	GameSettings::GameSettings(std::string path, const Defaults& defaults) : m_path(std::move(path)), m_defaults(defaults), m_state(new State())
	{
		std::ifstream in(m_path, std::ios::binary);
		if (!in) return;
		nlohmann::json json = nlohmann::json::parse(in, nullptr, false);
		if (json.is_object()) m_state->values = std::move(json);
		else LOG_WARNING("[Settings] %s is not a JSON object: the defaults", m_path.c_str());
	}

	GameSettings::~GameSettings() { delete m_state; }

	int GameSettings::music() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return step10(m_state->values.value(kMusic, m_defaults.music));
	}

	int GameSettings::sounds() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return step10(m_state->values.value(kSounds, m_defaults.sounds));
	}

	std::string GameSettings::language() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_state->values.value(kLanguage, std::string());
	}

	std::string GameSettings::language_to_use(const std::string& device, const std::vector<std::string>& available) const
	{
		const std::string chosen = language();
		auto has = [&available](const std::string& language) { return std::find(available.begin(), available.end(), language) != available.end(); };
		if (!chosen.empty() && (available.empty() || has(chosen))) return chosen;
		if (!device.empty() && (available.empty() || has(device))) return device;
		return available.empty() ? std::string() : available.front();
	}

	bool GameSettings::telemetry() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_state->values.value(kTelemetry, m_defaults.telemetry);
	}

	std::string GameSettings::quality() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_state->values.value(kQuality, std::string("auto"));
	}

	std::string GameSettings::fsr() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		const std::string mode = m_state->values.value(kFsr, std::string("auto"));
		return mode == "on" || mode == "off" ? mode : "auto";
	}

	bool GameSettings::vibration() const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		return m_state->values.value(kVibration, m_defaults.vibration);
	}

	void GameSettings::set_music(int percent)
	{
		{ std::lock_guard<std::mutex> lock(m_mutex); m_state->values[kMusic] = step10(percent); save_locked(); }
		changed(kMusic);
	}

	void GameSettings::set_sounds(int percent)
	{
		{ std::lock_guard<std::mutex> lock(m_mutex); m_state->values[kSounds] = step10(percent); save_locked(); }
		changed(kSounds);
	}

	void GameSettings::set_language(const std::string& language)
	{
		{ std::lock_guard<std::mutex> lock(m_mutex); m_state->values[kLanguage] = language; save_locked(); }
		changed(kLanguage);
	}

	void GameSettings::set_telemetry(bool on)
	{
		{ std::lock_guard<std::mutex> lock(m_mutex); m_state->values[kTelemetry] = on; save_locked(); }
		changed(kTelemetry);
	}

	void GameSettings::set_quality(const std::string& quality)
	{
		{ std::lock_guard<std::mutex> lock(m_mutex); m_state->values[kQuality] = quality.empty() ? std::string("auto") : quality; save_locked(); }
		changed(kQuality);
	}

	void GameSettings::set_fsr(const std::string& mode)
	{
		{ std::lock_guard<std::mutex> lock(m_mutex); m_state->values[kFsr] = mode == "on" || mode == "off" ? mode : std::string("auto"); save_locked(); }
		changed(kFsr);
	}

	void GameSettings::set_vibration(bool on)
	{
		{ std::lock_guard<std::mutex> lock(m_mutex); m_state->values[kVibration] = on; save_locked(); }
		changed(kVibration);
	}

	std::string GameSettings::extra(const std::string& name, const std::string& fallbackJson) const
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		const auto extras = m_state->values.find("extra");
		if (extras == m_state->values.end() || !extras->is_object()) return fallbackJson;
		const auto value = extras->find(name);
		return value == extras->end() ? fallbackJson : value->dump();
	}

	void GameSettings::set_extra(const std::string& name, const std::string& json)
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			nlohmann::json value = nlohmann::json::parse(json, nullptr, false);
			if (value.is_discarded())
			{
				LOG_WARNING("[Settings] %s: not JSON, not kept", name.c_str());
				return;
			}
			m_state->values["extra"][name] = std::move(value);
			save_locked();
		}
		changed(name.c_str());
	}

	void GameSettings::apply_sound(Engine& engine) const
	{
		if (SoundManager* sound = engine.sound_manager())
		{
			sound->set_music_volume(music() / 100.0f);
			sound->set_volume(sounds() / 100.0f);
		}
	}

	std::string GameSettings::to_json() const
	{
		nlohmann::json json = nlohmann::json::object();
		json[kMusic] = music();
		json[kSounds] = sounds();
		json[kLanguage] = language();
		json[kTelemetry] = telemetry();
		json[kQuality] = quality();
		json[kFsr] = fsr();
		json[kVibration] = vibration();
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			const auto extras = m_state->values.find("extra");
			if (extras != m_state->values.end() && extras->is_object())
				for (auto it = extras->begin(); it != extras->end(); ++it) json[it.key()] = it.value();
		}
		return json.dump();
	}

	void GameSettings::reset()
	{
		{ std::lock_guard<std::mutex> lock(m_mutex); m_state->values = nlohmann::json::object(); save_locked(); }
		for (const char* name : { kMusic, kSounds, kLanguage, kTelemetry, kQuality, kFsr, kVibration }) changed(name);
	}

	void GameSettings::changed(const char* name)
	{
		event_changed(std::string(name));
	}

	bool GameSettings::save_locked() const
	{
		// written beside it, then moved over it: a crash while writing never leaves half a file
		std::error_code error;
		const std::filesystem::path target(m_path);
		if (target.has_parent_path()) std::filesystem::create_directories(target.parent_path(), error);
		const std::string side = m_path + ".new";
		{
			std::ofstream out(side, std::ios::binary | std::ios::trunc);
			if (!out) { LOG_WARNING("[Settings] %s could not be written", side.c_str()); return false; }
			out << m_state->values.dump(1, '\t');
			if (!out) return false;
		}
		std::filesystem::rename(side, target, error);
		if (error) LOG_WARNING("[Settings] %s: %s", m_path.c_str(), error.message().c_str());
		return !error;
	}

	GameSettings::About GameSettings::about(const Engine& engine, const std::string& creditsFile, const std::string& privacyUrl)
	{
		const BuildIdentity& build = engine.build_identity();
		About about;
		about.version = build.version;
		about.build = build.build > 0 ? std::to_string(build.build) : build.commit;
		about.channel = build.channel;
		about.credits_file = creditsFile;
		about.privacy_url = privacyUrl;
		return about;
	}

	std::string GameSettings::about_json(const About& about)
	{
		nlohmann::json json = { { "version", about.version }, { "build", about.build }, { "channel", about.channel },
			{ "credits_file", about.credits_file }, { "privacy_url", about.privacy_url } };
		// C-OPT-9's line: "v1.0.0 (531)"; production shows it too (only the corner label is hidden there)
		json["line"] = (about.version.empty() ? std::string() : "v" + about.version) + (about.build.empty() ? std::string() : " (" + about.build + ")");
		return json.dump();
	}
}
