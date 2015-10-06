#pragma once
#include "MusicComponent.h"
#include "sfml/StreamSFML.h"
#include "SFML/Audio.hpp"
#include "ResourceManager.h"

namespace Pakal
{
	class _PAKALExport MusicComponentSFML : public MusicComponent
	{
		DECLARE_RTTI_WITH_BASE(MusicComponentSFML, MusicComponent);
		SharedPtr<StreamSFML> m_buffer;
		sf::Music m_player;

		ResourceManager& rmgr = ResourceManager::instance();

	public:
		bool initialize(const path& resourcePath, const SoundSettings& settings) override;
		bool initialize(SharedPtr<IStream> resourceStream, const SoundSettings& settings) override;
		void terminate() override;

		~MusicComponentSFML();

		void play() override;
		void pause() override;
		void stop() override;
		void set_volume(float volume) override;
		float get_volume() override;
		void set_pitch(float pitch) override;
		float get_pitch() override;
		void set_position(const tmath::vector3df& position) override;
		tmath::vector3df get_position() override;
		void set_loop(bool loop) override;
		bool get_loop() override;
		void set_relative_to_listener(bool relative) override;
		bool get_relative_to_listener() override;
		Time get_duration() override;
		unsigned get_channel_count() override;
		unsigned get_sample_rate() override;
		MusicState get_state() override;
	};
}
