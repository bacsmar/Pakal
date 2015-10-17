#pragma once
#include "SoundManager.h"

#include "TaskFwd.h"
#include "SFML/Audio.hpp"
#include "IStream.h"
#include "ResourceManager.h"
#include "Timer.h"

#include <map>
#include <list>

namespace Pakal
{

	class _PAKALExport SoundManagerSFML : public SoundManager
	{
		std::mutex m_buffer_mutex,m_players_mutex;

		std::map<path, WeakPtr<sf::SoundBuffer>> m_buffers;
		std::list<UniquePtr<sf::Sound>> m_active_players;

		Timer m_timer;
		ulonglong m_clean_players_id = 0;

		ResourceManager& rmgr = ResourceManager::instance();
		
		void clean_players();
		void clean_expired_buffers();

	public:
		void initialize() override;
		void terminate() override;
		void register_component_factories(std::vector<IComponentFactory*>& factories) override;
		void set_volume(float volume) override;
		float get_volume() override;

		void hint_buffer_removed();

		SharedPtr<sf::SoundBuffer> load_sfx(const path& resourcePath);
		SharedPtr<sf::SoundBuffer> load_sfx(SharedPtr<IStream> resourceStream);

		void play_sfx(SharedPtr<sf::SoundBuffer> buffer, const SoundSettings& settings);

	};


}