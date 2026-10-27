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

// The sound backend: SFML on the desktop, the AAudio mixer of android/ on Android
#if PAKAL_USE_SFML_AUDIO == 1
namespace sf
{
	class SoundBuffer;
}
namespace Pakal
{
	class SoundManagerSFML;
}
#else
namespace Pakal
{
	struct PcmSound;
	class SoundManagerAAudio;
}
#endif

namespace Pakal
{
	class SoundManager;
}

namespace Pakal::TopDown
{
	class AudioCues
	{
	public:
		// Loads every sound listed in <levelDir>/audio.json. Without a sound manager it stays silent. 'seed': its random
		// stream ("audio", which variant of a sound plays).
		AudioCues(Pakal::SoundManager* manager, const std::string& levelDir, uint32_t seed);
		~AudioCues();

		// Plays one sound of 'name' at 'volume' (0..1). Unknown names are reported once.
		void play(const std::string& name, float volume = 1.0f);
		size_t name_count() const { return m_sounds.size(); }

	private:
#if PAKAL_USE_SFML_AUDIO == 1
		using Manager = Pakal::SoundManagerSFML;
		using Buffer = sf::SoundBuffer;
#else
		using Manager = Pakal::SoundManagerAAudio;
		using Buffer = Pakal::PcmSound;
#endif
		Manager* m_manager = nullptr;
		std::map<std::string, std::vector<std::shared_ptr<Buffer>>> m_sounds;
		std::map<std::string, bool> m_reported;
		Pakal::RandomStream m_random;
	};
}
