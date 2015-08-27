///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 PakalEngine
// File: SoundManager.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Base Class for sound managers
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "IManager.h"
namespace Pakal
{
	class SoundManager :
		public Pakal::IManager
	{
	public:
		//void initialize() override;
		//void terminate() override;
	public:
		SoundManager(void);
		virtual ~SoundManager(void);
	};
}