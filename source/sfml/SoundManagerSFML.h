#pragma once
#include "SoundManager.h"

#include "TaskFwd.h"
#include "SFML/Audio.hpp"
#include "ResourceManager.h"
#include "Timer.h"

#include <map>
#include <list>

namespace Pakal
{

	class _PAKALExport SoundManagerSFML : public SoundManager
	{
		std::mutex m_buffer_mutex,m_players_mutex;

		std::map<Path, WeakPtr<sf::SoundBuffer>> m_buffers;
		std::list<UniquePtr<sf::Sound>> m_active_players;

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