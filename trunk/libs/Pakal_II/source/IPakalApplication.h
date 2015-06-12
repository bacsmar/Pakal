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

	class ComponentSystem;
	class GameStateSystem;
	class Engine;

	class IPakalApplication
	{
	public:
		virtual bool setUpEnvironment( Engine * engine) = 0;

		virtual bool setUpComponents( ComponentSystem * componentSystem) = 0;

		virtual bool setUpGameStates( GameStateSystem * gameStateSystem) = 0;

		virtual ~IPakalApplication() {}
	};
}