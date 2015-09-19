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

#include <android/window.h>
#include <android/native_activity.h>

namespace Pakal
{
	class _PAKALExport OsWrapperAndroid
	{
		friend class OSManager;
	protected:
		virtual ~OsWrapperAndroid();
		OsWrapperAndroid();		
	public:
		static void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize);
		ANativeActivity* activity;
	};


}