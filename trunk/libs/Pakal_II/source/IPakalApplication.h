///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2012 The Pakal Engine Project
// File: IPakalApplication.h
// Original Author: wsnrg32@gmail.com
// Creation Date: may 8 2015
//
// Purpose: Application Interface
//
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once


namespace Pakal
{

	class ComponentManager;
	class GameStateManager;
	class Engine;

	class IPakalApplication
	{
	public:
		virtual bool setUpEnvironment( const Pakal::Engine * engine) = 0;

		virtual bool setUpComponents( const Pakal::ComponentManager * componentManager) = 0;

		virtual bool setUpGameStates( const Pakal::GameStateManager * gameStateManager) = 0;

		virtual ~IPakalApplication() {}
	};
}