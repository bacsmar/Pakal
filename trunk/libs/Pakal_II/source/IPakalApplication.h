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
	class GameStateManager;
	class Engine;

	class IPakalApplication
	{
	public:
		virtual void setup_environment( Engine * engine) = 0;

		virtual void start( GameStateManager * gameStateSystem) = 0;

		virtual void end(GameStateManager * gameStateSystem) = 0;

		virtual ~IPakalApplication() {}
	};
}