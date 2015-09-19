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

#ifdef PAKAL_ANDROID_PLATFORM
	#include "android/OsWrapperAndroid.h"
	//using OsWrapperImpl = Pakal::OsWrapperAndroid;
	//typedef Pakal::OsWrapperAndroid OsWrapperImpl;
	#define OsWrapperImpl Pakal::OsWrapperAndroid
#else
	class OsDummyWrapper{};
	typedef OsDummyWrapper OsWrapperImpl;
#endif

namespace Pakal
{
	class _PAKALExport OSManager
	{		
		template <class OSManager> friend class SingletonHolder;
		friend class Engine;
		friend class OsWrapperImpl;
	public :
		
		struct WindowArgs
		{
			unsigned windowId;
			unsigned size_x;
			unsigned size_y;
		};

		struct WindowImpl
		{
			virtual unsigned create_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned int bitsPerPixel) = 0;
			virtual ~WindowImpl(){}
		};
			
		Event<WindowArgs>	event_window_created;
		Event<WindowArgs>	event_window_destroyed;
		Event<WindowArgs>	event_window_redraw_needed;
		Event<WindowArgs>	event_window_resized;
		Event<bool>			event_window_focused;		

		virtual BasicTaskPtr setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned int bitsPerPixel);
		virtual void  close_window();
		virtual void  process_os_events();

		void on_window_created(const WindowArgs& arg);
		
	protected:
		WindowImpl* m_windowImpl;
		TaskCompletionSource m_task;
		// access throug Engine...
		static OSManager& instance();
		virtual ~OSManager();
		OSManager();
	};
}