///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: SoundManagerAAudio.h
// Purpose: Sound of the Android build: short effects (WAV, 16-bit PCM) and the music (MusicMixer, OGG) mixed by hand
//          into one stereo AAudio output stream. It is what SoundManagerSFML is on the desktop.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "SoundManager.h"

#include <aaudio/AAudio.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
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

		bool play_music(const std::string& path, const MusicSettings& settings) override;
		void stop_music(float fadeSeconds) override { m_music.stop(fadeSeconds); }
		void set_music_volume(float volume) override { m_music.set_volume(volume); }
		void set_paused(bool paused) override;   // the output stream pauses (the music's feeder waits: its ring is full)

	private:
		struct Voice
		{
			std::shared_ptr<PcmSound> sound;
			double position = 0.0; // in samples of the sound
			float gain = 1.0f;
		};

		static aaudio_data_callback_result_t on_data(AAudioStream* stream, void* user, void* audio, int32_t frames);
		static void on_error(AAudioStream* stream, void* user, aaudio_result_t error);
		void mix(int16_t* out, int32_t frames);

		// The output stream. A disconnected one (its device changed) is closed and opened again, by the feeder or on resume
		bool open_stream();
		void close_stream();
		void reopen_stream();
		std::atomic<AAudioStream*> m_stream{ nullptr };
		std::mutex m_streamMutex;                  // opening, closing, pausing
		std::atomic<bool> m_lost{ false }, m_paused{ false };
		std::atomic<int> m_sampleRate{ 48000 };
		float m_volume = 1.0f;
		std::mutex m_mutex;
		std::vector<Voice> m_voices;

		// The music is decoded on a thread of its own into a ring of stereo frames that the output callback reads: no
		// decoding in the callback
		void feed_music();
		MusicMixer m_music;
		std::vector<float> m_ring;                 // stereo frames
		std::atomic<size_t> m_ringRead{ 0 }, m_ringWrite{ 0 };   // in frames, ever growing
		std::atomic<bool> m_feeding{ false }, m_musicOn{ false };
		std::thread m_feeder;
	};
}
