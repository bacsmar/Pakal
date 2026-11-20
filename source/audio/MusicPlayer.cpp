///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: MusicPlayer.cpp
// Purpose: Background music by file - implementation, over SoundManager::play_music
///////////////////////////////////////////////////////////////////////////////////////////////////////

#include "audio/MusicPlayer.h"

#include "LogMgr.h"
#include "SoundManager.h"

namespace Pakal
{
	MusicPlayer::MusicPlayer(Pakal::SoundManager* manager) : m_manager(manager) {}

	MusicPlayer::~MusicPlayer() = default;

	bool MusicPlayer::play(const std::string& file, float volume, bool loop)
	{
		if (!m_manager || file.empty()) return false;
		if (file == m_file) return true;
		MusicSettings settings;
		settings.gain = volume;
		settings.loop = loop;
		if (!m_manager->play_music(file, settings))
		{
			LOG_WARNING("[MusicPlayer] cannot play %s", file.c_str());
			return false;
		}
		m_file = file;
		LOG_INFO("[MusicPlayer] playing %s", file.c_str());
		return true;
	}

	void MusicPlayer::set_volume(float volume)
	{
		if (m_manager) m_manager->set_music_volume(volume);
	}

	void MusicPlayer::stop()
	{
		if (m_manager) m_manager->stop_music(0.5f);
		m_file.clear();
	}
}
