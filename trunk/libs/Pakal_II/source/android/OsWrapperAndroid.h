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
#include <android/configuration.h>

#include "OsManager.h"

namespace Pakal
{
	class _PAKALExport OsWrapperAndroid  : public OSManager
	{
		template <class OSManager> friend class SingletonHolder;		
		friend class OSManager_t;
	protected:
		virtual ~OsWrapperAndroid();
		OsWrapperAndroid();
	public:
		ANativeActivity*	activity;
		std::string get_app_path() override;
	};


}