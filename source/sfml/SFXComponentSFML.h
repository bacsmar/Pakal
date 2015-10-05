#pragma once
#include "SFXComponent.h"
#include "SFML/Audio.hpp"

#include <map>


namespace Pakal
{
	class SoundManagerSFML;

	class _PAKALExport SFXComponentSFML : public SFXComponent
	{
		SoundManagerSFML* m_manager;

		std::map<unsigned, SharedPtr<sf::SoundBuffer>> m_sounds;

	public:
		DECLARE_RTTI_WITH_BASE(SFXComponentSFML, SFXComponent);

		SFXComponentSFML(SoundManagerSFML* manager) : m_manager(manager) { }

		~SFXComponentSFML();

		void initialize() override {};
		void terminate() override;
		bool add(unsigned id, const path& resourcePath) override;
		bool add(unsigned id, SharedPtr<IStream> resourceStream) override;
		void remove(unsigned id) override;
		void play(unsigned id, const SoundSettings& settings) override;

	};
}