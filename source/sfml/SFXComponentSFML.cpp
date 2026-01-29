#include "SFXComponentSFML.h"
#include "SoundManagerSFML.h"
#include "Utils.h"

namespace Pakal
{
	SFXComponentSFML::~SFXComponentSFML()
	{
		ASSERT(m_sounds.empty());
	}

	void SFXComponentSFML::terminate()
	{
		m_sounds.clear();
		m_manager->hint_buffer_removed();
	}

	bool SFXComponentSFML::add(unsigned id, const Path& resourcePath)
	{
		ASSERT(!map_utils::contains(m_sounds,id));
			
		auto buffer = m_manager->load_sfx(resourcePath);

		if (buffer != nullptr)
		{
			m_sounds[id] = buffer;
			return true;
		}

		return false;
	}

	bool SFXComponentSFML::add(unsigned id, SharedPtr<std::istream> resourceStream, const Path& resourceName)
	{
		ASSERT(!map_utils::contains(m_sounds, id));

		auto buffer = m_manager->load_sfx(resourceStream,resourceName);

		if (buffer != nullptr)
		{
			m_sounds[id] = buffer;
			return true;
		}

		return false;

	}

	void SFXComponentSFML::remove(unsigned id)
	{
		m_sounds.erase(id);
		m_manager->hint_buffer_removed();
	}

	void SFXComponentSFML::play(unsigned id, const SoundSettings& settings)
	{
		SharedPtr<sf::SoundBuffer> buffer;

		if( map_utils::try_get(m_sounds,id,buffer))
		{
			m_manager->play_sfx(buffer, settings);
		}
		else
		{
			LOG_ERROR("[SFXComponent] can't play sound id %d", id);
		}
	}
}