///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: MusicPlayer.h
// Purpose: Background music by file: one track at a time, looping, over SoundManager::play_music (OGG Vorbis, crossfaded). The same
//          track asked again goes on playing. The files are read as the resource manager finds them (a folder, a zip, the APK).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Config.h"

#include <string>

namespace Pakal
{
	class SoundManager;
}

namespace Pakal
{
	class _PAKALExport MusicPlayer
	{
	public:
		explicit MusicPlayer(Pakal::SoundManager* manager);
		~MusicPlayer();

		// Plays 'file' at 'volume' (0..1) from its start, over and over. The track that is playing already does not start again.
		// False when there is nothing to play it on, or the file cannot be read
		bool play(const std::string& file, float volume = 0.6f, bool loop = true);
		void stop();
		// The volume (0..1) of the track that is playing
		void set_volume(float volume);
		const std::string& playing() const { return m_file; }

	private:
		Pakal::SoundManager* m_manager = nullptr;
		std::string m_file;
	};
}
