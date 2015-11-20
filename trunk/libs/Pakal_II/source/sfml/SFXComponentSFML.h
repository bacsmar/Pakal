#pragma once
#include "components/SFXComponent.h"
#include "SFML/Audio.hpp"

#include <map>


namespace Pakal
{
	//class path;
	class SoundManagerSFML;

	class _PAKALExport SFXComponentSFML : public SFXComponent
	{
		DECLARE_RTTI_WITH_BASE(SFXComponentSFML, SFXComponent);
		SoundManagerSFML* m_manager;

		std::map<unsigned, SharedPtr<sf::SoundBuffer>> m_sounds;

	public:

		SFXComponentSFML(SoundManagerSFML* manager) : m_manager(manager) { }

		~SFXComponentSFML();

		void initialize() override {};
		void terminate() override;
		bool add(unsigned id, const Path& resourcePath) override;
		bool add(unsigned id, SharedPtr<std::istream> resourceStream,const Path& resourceName) override;
		void remove(unsigned id) override;
		void play(unsigned id, const SoundSettings& settings) override;

	};
}