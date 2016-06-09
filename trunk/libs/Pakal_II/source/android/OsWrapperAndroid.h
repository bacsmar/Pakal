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
//		static void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize);
//		static int get_window_from_surface(int surfacePtr);
		std::string get_app_path() override;
		//void on_app_started();
		//void on_app_resumed();
		//void on_app_paused();
		//void on_app_stoped();
		//void on_app_finished();
		//void on_window_redraw_needed(const WindowArgs& windowArgs);
		//void on_window_resized(const WindowArgs& windowArgs);
		//void on_window_focused(bool b);				
		//AConfiguration*		configuration;
		//unsigned			orientation;
	};


}