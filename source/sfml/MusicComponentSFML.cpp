#include "MusicComponentSFML.h"

namespace Pakal
{
	bool MusicComponentSFML::initialize(const path& resourcePath, const SoundSettings& settings)
	{
		return initialize(ResourceMgr.open_read_resource(resourcePath, false),settings);
	}

	bool MusicComponentSFML::initialize(SharedPtr<std::istream> resourceStream, const SoundSettings& settings)
	{
		m_buffer = std::make_shared<StreamSFML>(resourceStream);

		set_volume(settings.volume);
		set_position(settings.position);
		set_pitch(settings.pitch);
		set_relative_to_listener(settings.relative_to_source);
		set_loop(settings.loop);

		if (m_player.openFromStream(*m_buffer))
		{
			return true;
		}

		m_buffer = nullptr;
		return false;
	}

	void MusicComponentSFML::terminate()
	{
		stop();
		m_buffer = nullptr;

	}

	MusicComponentSFML::~MusicComponentSFML()
	{
		ASSERT(m_buffer == nullptr);
	}

	void MusicComponentSFML::play()
	{
		m_player.play();
	}

	void MusicComponentSFML::pause()
	{
		m_player.pause();
	}

	void MusicComponentSFML::stop()
	{
		m_player.stop();
	}

	void MusicComponentSFML::set_volume(float volume)
	{
		m_player.setVolume(volume);
	}

	float MusicComponentSFML::get_volume()
	{
		return m_player.getVolume();
	}

	void MusicComponentSFML::set_pitch(float pitch)
	{
		m_player.setPitch(pitch);
	}

	float MusicComponentSFML::get_pitch()
	{
		return m_player.getPitch();
	}

	void MusicComponentSFML::set_position(const tmath::vector3df& position)
	{
		m_player.setPosition(position.x,position.y,position.z);
	}

	tmath::vector3df MusicComponentSFML::get_position()
	{
		auto p = m_player.getPosition();
		return tmath::vector3df(p.x,p.y,p.z);
	}

	void MusicComponentSFML::set_loop(bool loop)
	{
		m_player.setLoop(loop);
	}

	bool MusicComponentSFML::get_loop()
	{
		return m_player.getLoop();
	}

	void MusicComponentSFML::set_relative_to_listener(bool relative)
	{
		m_player.setRelativeToListener(relative);
	}

	bool MusicComponentSFML::get_relative_to_listener()
	{
		return m_player.isRelativeToListener();
	}

	Time MusicComponentSFML::get_duration()
	{
		return Time(m_player.getDuration().asMicroseconds());
	}

	unsigned MusicComponentSFML::get_channel_count()
	{
		return m_player.getChannelCount();
	}

	unsigned MusicComponentSFML::get_sample_rate()
	{
		return m_player.getSampleRate();
	}

	MusicState MusicComponentSFML::get_state()
	{
		switch (m_player.getStatus())
		{
			case sf::SoundSource::Stopped: return MusicState::Stopped;
			case sf::SoundSource::Paused: return MusicState::Paused;
			case sf::SoundSource::Playing: return MusicState::Playing;
			default: break;
		}

		return MusicState::Invalid;
	}
}