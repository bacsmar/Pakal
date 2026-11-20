///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AudioCues.cpp
// Purpose: The level's sounds by the game's names - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "audio/AudioCues.h"

#include "LogMgr.h"
#include "ResourceManager.h"
#include "SoundManager.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>
#include <type_traits>

namespace Pakal
{
	AudioCues::AudioCues(Pakal::SoundManager* manager, const std::string& levelDir, uint32_t seed) :
		m_random(seed)
	{
		m_manager = manager;
		if (!m_manager)
		{
			LOG_WARNING("[Audio] no sound manager of the platform: the level plays without sound");
			return;
		}
		bool found = false;
		const std::string text = ResourceMgr.read_text(levelDir + "audio.json", &found);   // a folder, a zip, the APK
		if (!found)
		{
			LOG_WARNING("[Audio] %saudio.json not found: run extract_audio.py", levelDir.c_str());
			return;
		}
		const nlohmann::json bank = nlohmann::json::parse(text, nullptr, false);
		if (!bank.is_object())
		{
			LOG_ERROR("[Audio] audio.json is not valid");
			return;
		}
		size_t files = 0;
		// a file that several names list (the fighters of a game share most of theirs) is read once
		std::map<std::string, std::decay_t<decltype(m_sounds.begin()->second.front())>> loaded;
		for (auto it = bank.begin(); it != bank.end(); ++it)
		{
			for (const auto& entry : it.value())
			{
				const std::string path = levelDir + entry.get<std::string>();
				if (const auto shared = loaded.find(path); shared != loaded.end())
				{
					m_sounds[it.key()].push_back(shared->second);
					continue;
				}
				if (auto buffer = m_manager->load_effect(path))
				{
					m_sounds[it.key()].push_back(buffer);
					loaded[path] = buffer;
					++files;
				}
			}
		}
		LOG_INFO("[Audio] audio: %zu names, %zu files", m_sounds.size(), files);
	}

	AudioCues::~AudioCues() = default;

	bool AudioCues::add(const std::string& name, const std::string& path)
	{
		if (!m_manager || name.empty() || path.empty()) return false;
		auto buffer = m_manager->load_effect(path);
		if (!buffer) return false;
		m_sounds[name].push_back(buffer);
		return true;
	}

	void AudioCues::play(const std::string& name, float volume)
	{
		volume *= m_volume;
		if (!m_manager || name.empty() || volume <= 0.0f)
		{
			return;
		}
		auto found = m_sounds.find(name);
		if (found == m_sounds.end() || found->second.empty())
		{
			if (!m_reported[name])
			{
				m_reported[name] = true;
				LOG_INFO("[Audio] no sound for '%s'", name.c_str());
			}
			return;
		}
		const auto& choices = found->second;
		const size_t pick = static_cast<size_t>(m_random.between(0, static_cast<int>(choices.size()) - 1));
		Pakal::SoundSettings settings;
		settings.volume = 100.0f * std::min(volume, 1.0f);
		settings.relative_to_source = true; // heard as is: the level sets the volume by distance
		settings.attenuation = 0.0f;
		m_manager->play_effect(choices[pick], settings);
	}
}
