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

	bool SFXComponentSFML::add(unsigned id, const path& resourcePath)
	{
		ASSERT(!utils::contains(m_sounds,id));
			
		auto buffer = m_manager->load_sfx(resourcePath);

		if (buffer != nullptr)
		{
			m_sounds[id] = buffer;
			return true;
		}

		return false;
	}

	bool SFXComponentSFML::add(unsigned id, SharedPtr<IStream> resourceStream)
	{
		ASSERT(!utils::contains(m_sounds, id));

		auto buffer = m_manager->load_sfx(resourceStream);

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
		m_manager->play_sfx(m_sounds[id], settings);
	}
}