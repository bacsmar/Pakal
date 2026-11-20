#pragma once
#include "SoundManager.h"

#include "TaskFwd.h"
#include "SFML/Audio.hpp"
#include "ResourceManager.h"
#include "Timer.h"

#include <algorithm>
#include <list>
#include <map>
#include <vector>

namespace Pakal
{
	// The music: one stream, always open, that SFML's thread fills from the mixer (silence between tracks)
	class MusicStreamSFML final : public sf::SoundStream
	{
	public:
		static const unsigned kRate = 48000;
		static const int kFrames = 2048;

		explicit MusicStreamSFML(MusicMixer& mixer) : m_mixer(mixer)
		{
			initialize(2, kRate);
			setRelativeToListener(true);
			setAttenuation(0.0f);
		}
		~MusicStreamSFML() override { stop(); }

	private:
		bool onGetData(Chunk& data) override
		{
			m_mix.assign(2 * kFrames, 0.0f);
			m_mixer.render(m_mix.data(), kFrames, static_cast<int>(kRate));
			m_samples.resize(m_mix.size());
			for (size_t i = 0; i < m_mix.size(); ++i)
			{
				m_samples[i] = static_cast<sf::Int16>(std::clamp(m_mix[i], -1.0f, 1.0f) * 32767.0f);
			}
			data.samples = m_samples.data();
			data.sampleCount = m_samples.size();
			return true;   // it never ends: silence between tracks
		}
		void onSeek(sf::Time) override {}

		MusicMixer& m_mixer;
		std::vector<float> m_mix;
		std::vector<sf::Int16> m_samples;
	};

	class _PAKALExport SoundManagerSFML : public SoundManager
	{
		std::mutex m_buffer_mutex,m_players_mutex;

		std::map<Path, WeakPtr<sf::SoundBuffer>> m_buffers;
		std::list<UniquePtr<sf::Sound>> m_active_players;

		MusicMixer m_music;
		UniquePtr<MusicStreamSFML> m_musicStream;

		Timer m_timer;
		EventId m_clean_players_id = 0;
		
		void clean_players();
		void clean_expired_buffers();

	public:
		void initialize() override;
		void terminate() override;
		void register_component_factories(std::vector<IComponentFactory*>& factories) override;
		void set_volume(float volume) override;
		float get_volume() override;
		bool play_music(const std::string& path, const MusicSettings& settings) override;
		void stop_music(float fadeSeconds) override;
		void set_music_volume(float volume) override { m_music.set_volume(volume); }
		void set_paused(bool paused) override;
		~SoundManagerSFML() override;

		void hint_buffer_removed();

		SharedPtr<sf::SoundBuffer> load_sfx(const Path& resourcePath);
		SharedPtr<sf::SoundBuffer> load_sfx(SharedPtr<std::istream> resourceStream,const Path& resourceName);

		void play_sfx(SharedPtr<sf::SoundBuffer> buffer, const SoundSettings& settings);

		// Mono sine tone generated in memory (no asset needed). Spatialized when played with a position.
		SharedPtr<sf::SoundBuffer> create_tone(float frequencyHz, float seconds);

		void set_listener(const tmath::vector3df& position, const tmath::vector3df& forward, const tmath::vector3df& up) override;

		// Applies the volume, pitch and spatial settings to a sound before it plays
		static void apply_settings(sf::Sound& sound, const sf::SoundBuffer& buffer, const SoundSettings& settings);

	};


}