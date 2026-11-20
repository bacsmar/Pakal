///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AudioCues.h
// Purpose: The level's sounds by the game's names. audio.json (tools/zombies_convert/extract_audio.py)
//          lists the files of each name; an event with several files plays one of them at random.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Config.h"
#include "RandomStreams.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace Pakal
{
	class SoundManager;
	class SoundEffect;
}

namespace Pakal
{
	class _PAKALExport AudioCues
	{
	public:
		// Loads every sound listed in <levelDir>/audio.json. Without a sound manager it stays silent. 'seed': its random
		// stream ("audio", which variant of a sound plays).
		AudioCues(Pakal::SoundManager* manager, const std::string& levelDir, uint32_t seed);
		~AudioCues();

		// Plays one sound of 'name' at 'volume' (0..1). Unknown names are reported once.
		void play(const std::string& name, float volume = 1.0f);
		// One more sound under 'name', from a file of the game's (not the level's audio.json): false when it does not load
		bool add(const std::string& name, const std::string& path);
		size_t name_count() const { return m_sounds.size(); }
		// Over every sound it plays, 0..1 (the player's option)
		void set_volume(float volume) { m_volume = volume < 0.0f ? 0.0f : (volume > 1.0f ? 1.0f : volume); }

	private:
		Pakal::SoundManager* m_manager = nullptr;
		std::map<std::string, std::vector<std::shared_ptr<Pakal::SoundEffect>>> m_sounds;
		std::map<std::string, bool> m_reported;
		Pakal::RandomStream m_random;
		float m_volume = 1.0f;
	};
}
