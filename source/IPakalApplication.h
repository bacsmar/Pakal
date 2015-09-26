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

	class Engine;

	class IPakalApplication
	{
	public:

		virtual char* get_name() = 0;

		virtual void start(Engine* gameStateSystem) = 0;

		virtual void end(Engine* gameStateSystem) = 0;

		virtual ~IPakalApplication() {}
	};
}