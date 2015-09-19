///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 PakalEngine
// File: SoundManager.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Base Class for sound managers
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "SoundManager.h"
#include "SFML/Audio.hpp"

namespace Pakal
{
	class SoundManagerSFML :
		public Pakal::SoundManager
	{
		//sf::Music m_music_player;
	public:
		void initialize() override;
		void terminate() override;	
		SoundManagerSFML();
		virtual ~SoundManagerSFML();

		void play_music(const std::string& filename) override;
	};
}