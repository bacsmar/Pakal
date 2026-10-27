///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AudioCues.cpp
// Purpose: The level's sounds by the game's names - implementation
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "topdown/view/AudioCues.h"

#include "LogMgr.h"
#if PAKAL_USE_SFML_AUDIO == 1
#include "sfml/SoundManagerSFML.h"
#elif defined(__ANDROID__)
#include "SoundManagerAAudio.h"
#endif

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

namespace Pakal::TopDown
{
	AudioCues::AudioCues(Pakal::SoundManager* manager, const std::string& levelDir, uint32_t seed) :
		m_random(seed)
	{
#if PAKAL_USE_SFML_AUDIO == 1
		m_manager = dynamic_cast<Pakal::SoundManagerSFML*>(manager);
#elif defined(__ANDROID__)
		m_manager = dynamic_cast<Pakal::SoundManagerAAudio*>(manager);
#else
		(void)manager;
#endif
		if (!m_manager)
		{
			LOG_WARNING("[TopDown] no sound manager of the platform: the level plays without sound");
			return;
		}
		std::ifstream file(levelDir + "audio.json");
		if (!file)
		{
			LOG_WARNING("[TopDown] %saudio.json not found: run extract_audio.py", levelDir.c_str());
			return;
		}
		const nlohmann::json bank = nlohmann::json::parse(file, nullptr, false);
		if (!bank.is_object())
		{
			LOG_ERROR("[TopDown] audio.json is not valid");
			return;
		}
		size_t files = 0;
		for (auto it = bank.begin(); it != bank.end(); ++it)
		{
			for (const auto& entry : it.value())
			{
				const std::string path = levelDir + entry.get<std::string>();
#if PAKAL_USE_SFML_AUDIO == 1
				auto stream = std::make_shared<std::ifstream>(path, std::ios::binary);
				if (!*stream)
				{
					continue;
				}
				if (auto buffer = m_manager->load_sfx(stream, path))
#else
				if (auto buffer = m_manager->load_wav(path))
#endif
				{
					m_sounds[it.key()].push_back(buffer);
					++files;
				}
			}
		}
		LOG_INFO("[TopDown] audio: %zu names, %zu files", m_sounds.size(), files);
	}

	AudioCues::~AudioCues() = default;

	void AudioCues::play(const std::string& name, float volume)
	{
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
				LOG_INFO("[TopDown] no sound for '%s'", name.c_str());
			}
			return;
		}
		const auto& choices = found->second;
		const size_t pick = static_cast<size_t>(m_random.between(0, static_cast<int>(choices.size()) - 1));
#if PAKAL_USE_SFML_AUDIO == 1
		Pakal::SoundSettings settings;
		settings.volume = 100.0f * std::min(volume, 1.0f);
		settings.relative_to_source = true; // heard as is: the level sets the volume by distance
		settings.attenuation = 0.0f;
		m_manager->play_sfx(choices[pick], settings);
#else
		m_manager->play(choices[pick], std::min(volume, 1.0f));
#endif
	}
}
