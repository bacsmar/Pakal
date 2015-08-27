#include "SoundManager_SFML.h"


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
	m_music_player.openFromFile(filename);
	m_music_player.play();
}