#include "SoundManagerSFML.h"
#include <memory>
#include "Utils.h"
#include "SFXComponentSFML.h"
#include "MusicComponentSFML.h"
#include "StreamSFML.h"
#include "Utils.h"

#include <algorithm>
#include <cmath>
#include <vector>


namespace Pakal
{
	void SoundManagerSFML::initialize()
	{
		ASSERT(m_clean_players_id == 0);

		m_timer.set_interval(500);
		m_timer.event_elapsed += {m_clean_players_id, std::bind(&SoundManagerSFML::clean_players, this)};
	}

	void SoundManagerSFML::terminate()
	{
		m_timer.stop();
		m_timer.event_elapsed -= m_clean_players_id;

		mutex_guard lock2(m_buffer_mutex);
		mutex_guard lock(m_players_mutex);

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

	SharedPtr<sf::SoundBuffer> SoundManagerSFML::load_sfx(const Path& resourcePath)
	{
		//try retireve from cache
		{
			mutex_guard lock(m_buffer_mutex);

			auto bufferPos = m_buffers.find(resourcePath);

			if (bufferPos != m_buffers.end())
			{
				if (auto buffer = bufferPos->second.lock())
					return buffer;

				m_buffers.erase(bufferPos);
			}
		}

		//if not found in cache then create it and load it.
		SharedPtr<std::istream> stream = ResourceMgr.open_read_resource(resourcePath, false);
		
		return stream ? load_sfx(stream, resourcePath): nullptr;
	}

	SharedPtr<sf::SoundBuffer> SoundManagerSFML::load_sfx(SharedPtr<std::istream> resourceStream,const Path& resourceName)
	{
		//try retireve from cache
		{
			mutex_guard lock(m_buffer_mutex);

			auto bufferPos = m_buffers.find(resourceName);

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
			mutex_guard lock(m_buffer_mutex);

			m_buffers[resourceName] = buffer;

			return buffer;
		}

		return nullptr;
	}

	SharedPtr<sf::SoundBuffer> SoundManagerSFML::create_tone(float frequencyHz, float seconds)
	{
		const unsigned int sampleRate = 44100;
		const size_t count = static_cast<size_t>(seconds * sampleRate);
		std::vector<sf::Int16> samples(count);
		const double twoPi = 6.283185307179586;
		for (size_t i = 0; i < count; ++i)
		{
			// Short fade in and out, so the tone does not click
			const double t = static_cast<double>(i) / sampleRate;
			const double fade = std::min(1.0, std::min(t, seconds - t) / 0.01);
			samples[i] = static_cast<sf::Int16>(std::sin(twoPi * frequencyHz * t) * 0.3 * fade * 32767.0);
		}

		SharedPtr<sf::SoundBuffer> buffer = std::make_shared<sf::SoundBuffer>();
		if (!buffer->loadFromSamples(samples.data(), samples.size(), 1, sampleRate))
		{
			LOG_ERROR("[SoundManagerSFML] could not create a %.1f Hz tone", frequencyHz);
			return nullptr;
		}
		return buffer;
	}

	void SoundManagerSFML::set_listener(const tmath::vector3df& position, const tmath::vector3df& forward, const tmath::vector3df& up)
	{
		sf::Listener::setPosition(position.x, position.y, position.z);
		sf::Listener::setDirection(forward.x, forward.y, forward.z);
		sf::Listener::setUpVector(up.x, up.y, up.z);
	}

	void SoundManagerSFML::apply_settings(sf::Sound& sound, const sf::SoundBuffer& buffer, const SoundSettings& settings)
	{
		sound.setBuffer(buffer);
		sound.setVolume(settings.volume);
		sound.setPitch(settings.pitch);
		sound.setRelativeToListener(settings.relative_to_source);
		sound.setPosition(settings.position.x, settings.position.y, settings.position.z);
		sound.setMinDistance(settings.min_distance);
		sound.setAttenuation(settings.attenuation);

		// SFML only spatializes mono sources; a stereo sound plays centered whatever its position
		if (buffer.getChannelCount() != 1)
		{
			LOG_WARNING("[SoundManagerSFML] stereo sound: position is ignored (only mono sounds are spatialized)");
		}
	}

	void SoundManagerSFML::play_sfx(SharedPtr<sf::SoundBuffer> buffer, const SoundSettings& settings)
	{
		//UniquePtr<sf::Sound> sound = std::make_unique<sf::Sound>();
		UniquePtr<sf::Sound> sound = UniquePtr<sf::Sound>(new sf::Sound());

		apply_settings(*sound, *buffer, settings);
		sound->play();

		mutex_guard lock(m_players_mutex);

		if (m_active_players.empty())
		{
			m_timer.start();
		}

		m_active_players.push_back(std::move(sound));
	}

	void SoundManagerSFML::clean_expired_buffers()
	{
		mutex_guard l(m_buffer_mutex);

		LOG_INFO("Cleaning expired buffers...");
		map_utils::erase_if(m_buffers, [](const std::pair<Path, WeakPtr<sf::SoundBuffer>>& buffer) { return buffer.second.expired();  });
		LOG_INFO("Cleaning expired buffers...done");
	}

	void SoundManagerSFML::clean_players() 
	{
		mutex_guard l(m_players_mutex);

		map_utils::erase_if(m_active_players, [](const UniquePtr<sf::Sound>& player) { return player->getStatus() == sf::SoundSource::Stopped;  });

		if (m_active_players.empty())
		{
			m_timer.stop();
		}
	}
}