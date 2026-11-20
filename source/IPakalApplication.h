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

#include <string>

namespace Pakal
{

	class Engine;

	class IPakalApplication
	{
	public:

		virtual const char* get_name() = 0;

		virtual void start(Engine&) = 0;

		virtual void end(Engine&) = 0;

		//virtual void setup_resources(Engine& engine) = 0;

		// A short status line of the whole game for the QA tools' "state" (its version, its mode): optional, empty by default.
		// Called on the game's thread, between two updates.
		virtual std::string describe() { return std::string(); }

		virtual ~IPakalApplication() {}
	};
}