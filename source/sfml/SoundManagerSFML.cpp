#include "SoundManagerSFML.h"
#include <memory>
#include "math/tm.h"
#include "Utils.h"
#include "SFXComponentSFML.h"
#include "MusicComponentSFML.h"
#include "StreamSFML.h"
#include "Utils.h"


namespace Pakal
{
	void SoundManagerSFML::initialize()
	{
		ASSERT(m_clean_players_id == 0);

		m_timer.set_interval(500);
		m_clean_players_id =  m_timer.event_elapsed.add_listener(std::bind(&SoundManagerSFML::clean_players,this));
	}

	void SoundManagerSFML::terminate()
	{
		m_timer.stop();
		m_timer.event_elapsed.remove_listener(m_clean_players_id);

		std::lock_guard<std::mutex> lock2(m_buffer_mutex);
		std::lock_guard<std::mutex> lock(m_players_mutex);

		m_buffers.clear();
		m_active_players.clear();
	}

	void SoundManagerSFML::register_component_factories(std::vector<IComponentFactory*>& factories)
	{
		factories.emplace_back(CreateComponentFactory<SFXComponent, SFXComponentSFML>(this));
		factories.emplace_back(CreateComponentFactory<MusicComponent, MusicComponentSFML>());
	}

	void SoundManagerSFML::set_volume(float volume)
	{
		sf::Listener::setGlobalVolume(volume);
	}

	float SoundManagerSFML::get_volume()
	{
		return sf::Listener::getGlobalVolume();
	}

	void SoundManagerSFML::hint_buffer_removed()
	{
		clean_expired_buffers();
	}

	SharedPtr<sf::SoundBuffer> SoundManagerSFML::load_sfx(const path& resourcePath)
	{
		//try retireve from cache
		{
			std::lock_guard<std::mutex> lock(m_buffer_mutex);

			auto bufferPos = m_buffers.find(resourcePath);

			if (bufferPos != m_buffers.end())
			{
				if (auto buffer = bufferPos->second.lock())
					return buffer;

				m_buffers.erase(bufferPos);
			}
		}

		//if not found in cache then create it and load it.
		SharedPtr<StreamSFML> stream = rmgr.open_resource<StreamSFML>(resourcePath, false);
		SharedPtr<sf::SoundBuffer> buffer = std::make_shared<sf::SoundBuffer>();

		if (stream != nullptr && buffer->loadFromStream(*stream))
		{
			std::lock_guard<std::mutex> lock(m_buffer_mutex);

			auto bufferPos = m_buffers.find(resourcePath);

			if (bufferPos != m_buffers.end())
			{
				if (auto b = bufferPos->second.lock())
					return b;

				m_buffers.erase(bufferPos);
			}

			m_buffers[resourcePath] = buffer;
			return buffer;
		}

		return nullptr;
	}

	SharedPtr<sf::SoundBuffer> SoundManagerSFML::load_sfx(SharedPtr<IStream> resourceStream)
	{
		//try retireve from cache
		{
			std::lock_guard<std::mutex> lock(m_buffer_mutex);

			auto bufferPos = m_buffers.find(resourceStream->resource_name());

			if (bufferPos != m_buffers.end())
			{
				if (auto buffer = bufferPos->second.lock())
					return buffer;
				
				m_buffers.erase(bufferPos);
			}
		}

		//if not found in cache then create it and load it.
		SharedPtr<StreamSFML> stream = std::make_shared<StreamSFML>(resourceStream);
		SharedPtr<sf::SoundBuffer> buffer = std::make_shared<sf::SoundBuffer>();

		if (buffer->loadFromStream(*stream))
		{
			std::lock_guard<std::mutex> lock(m_buffer_mutex);

			auto bufferPos = m_buffers.find(resourceStream->resource_name());

			if (bufferPos != m_buffers.end())
			{
				if (auto b = bufferPos->second.lock())
					return b;
				
				m_buffers.erase(bufferPos);
			}

			m_buffers[resourceStream->resource_name()] = buffer;
			return buffer;
		}

		return nullptr;
	}

	void SoundManagerSFML::play_sfx(SharedPtr<sf::SoundBuffer> buffer, const SoundSettings& settings)
	{
		UniquePtr<sf::Sound> sound = std::make_unique<sf::Sound>();

		sound->setBuffer(*buffer);
		sound->setVolume(settings.volume);
		sound->setPitch(settings.pitch);
		sound->setRelativeToListener(settings.relative_to_source);
		sound->setPosition(settings.position.x, settings.position.y, settings.position.z);
		sound->play();

		std::lock_guard<std::mutex> lock(m_players_mutex);

		if (m_active_players.empty())
		{
			m_timer.start();
		}

		m_active_players.push_back(std::move(sound));
	}

	void SoundManagerSFML::clean_expired_buffers()
	{
		std::lock_guard<std::mutex> l(m_buffer_mutex);

		LOG_INFO("Cleaning expired buffers...");
		utils::erase_if(m_buffers, [](const std::pair<path, WeakPtr<sf::SoundBuffer>>& buffer) { return buffer.second.expired();  });
	}

	void SoundManagerSFML::clean_players() 
	{
		std::lock_guard<std::mutex> l(m_players_mutex);

		utils::erase_if(m_active_players, [](const UniquePtr<sf::Sound>& player) { return player->getStatus() == sf::SoundSource::Stopped;  });

		if (m_active_players.empty())
		{
			m_timer.stop();
		}
	}
}