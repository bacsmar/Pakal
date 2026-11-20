///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: MusicMixer.h
// Purpose: Music: OGG Vorbis tracks decoded as they play (stb_vorbis), a crossfade from one track to the next, and a loop
//          whose end crossfades into its start. The platform's sound manager renders it from its audio thread.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Config.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace Pakal
{
	struct MusicSettings
	{
		float gain = 1.0f;            // 0..1
		bool loop = true;
		float fadeSeconds = 1.0f;     // the fade in, and the fade out of the track it replaces
		float loopFadeSeconds = 3.0f; // a looping track's last seconds crossfade into its start (0: a plain jump)
	};

	class _PAKALExport MusicMixer
	{
	public:
		MusicMixer();
		~MusicMixer();

		// Starts 'ogg' (the file's bytes) while the tracks playing fade out. False when it is not OGG Vorbis.
		bool play(std::shared_ptr<const std::vector<uint8_t>> ogg, const MusicSettings& settings);
		// Every track fades out over 'fadeSeconds' (0: at once)
		void stop(float fadeSeconds);
		void set_volume(float volume);   // over every track, 0..1

		// Adds 'frames' stereo frames (interleaved L R) at 'rate' Hz into 'out'. From the audio thread.
		void render(float* out, int frames, int rate);

	private:
		struct Deck;
		std::unique_ptr<Deck> open(const std::shared_ptr<const std::vector<uint8_t>>& ogg, const MusicSettings& settings, float gain);

		std::mutex m_mutex;
		std::vector<std::unique_ptr<Deck>> m_decks;
		float m_volume = 1.0f;
	};
}
