#include "SoundManager_SFML.h"
#include <Pakal_II/source/ResourceManager.h>
#include "StreamFileSFML.h"


void Pakal::SoundManagerSFML::initialize()
{	
}

void Pakal::SoundManagerSFML::terminate()
{
	m_music_player.stop();	
}

Pakal::SoundManagerSFML::SoundManagerSFML()
{
}

Pakal::SoundManagerSFML::~SoundManagerSFML()
{
}

void Pakal::SoundManagerSFML::play_music(const std::string & filename)
{
	//auto resource = ResourceManager::instance().open_resource(filename);
	//SFMLReadPakalFile *file = new SFMLReadPakalFile (resource);
	//m_music_player.openFromStream(*file);	
	//m_music_player.play();
}