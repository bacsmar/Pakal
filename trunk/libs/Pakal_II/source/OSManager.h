///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2015 The PakalEngine Open Source Project
// File: ComponentSystem.h
// Original Author: Salvador Noel Romo Garcia.
// last modification: 13 jun 2015
// Purpose: Class to control creation IComponents using factories.
//
///////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma once
#include "Config.h"
#include "Event.h"
#include "math/tm.h"

#include "TaskCompletionSource.h"
#include "OSManagerAbstract.h"

#ifdef PAKAL_ANDROID_PLATFORM
	#include "android/OsWrapperAndroid.h"
	#define OsWrapperImpl Pakal::OsWrapperAndroid
#else
	class OsDummyWrapper : public Pakal::OSManagerAbstract
	{
	public:
		std::string get_system_sources() override
		{
			return "./";
		};
	};
	typedef OsDummyWrapper OsWrapperImpl;
#endif

namespace Pakal
{

	
	class OSManager : public OsWrapperImpl
	{
		friend class OsWrapperImpl;
		friend class Engine;
		static OSManager& instance();
	};
}