///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SoundManagerAAudio.h
// Purpose: Sound of the Android build: short effects (WAV, 16-bit PCM) mixed by hand into one AAudio output stream.
//          It is what SoundManagerSFML is on the desktop for the Zombies module (AudioCues).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "SoundManager.h"

#include <aaudio/AAudio.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace Pakal
{
	// A sound loaded into memory (mono: stereo files are mixed down)
	struct PcmSound
	{
		std::vector<int16_t> samples;
		int sampleRate = 44100;
	};

	class SoundManagerAAudio final : public SoundManager
	{
	public:
		SoundManagerAAudio() = default;
		~SoundManagerAAudio() override;

		void initialize() override;
		void terminate() override;
		void register_component_factories(std::vector<IComponentFactory*>&) override {}
		void set_volume(float volume) override { m_volume = volume; }
		float get_volume() override { return m_volume; }

		// Reads a WAV file (PCM, 8 or 16 bits). Returns null when it is not one.
		std::shared_ptr<PcmSound> load_wav(const std::string& path);
		// Starts the sound now, at 'gain' (0..1); several can play at once
		void play(const std::shared_ptr<PcmSound>& sound, float gain);

	private:
		struct Voice
		{
			std::shared_ptr<PcmSound> sound;
			double position = 0.0; // in samples of the sound
			float gain = 1.0f;
		};

		static aaudio_data_callback_result_t on_data(AAudioStream* stream, void* user, void* audio, int32_t frames);
		void mix(int16_t* out, int32_t frames);

		AAudioStream* m_stream = nullptr;
		int m_sampleRate = 48000;
		float m_volume = 1.0f;
		std::mutex m_mutex;
		std::vector<Voice> m_voices;
	};
}
