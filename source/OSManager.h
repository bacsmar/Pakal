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
#include "IManager.h"

#ifdef PAKAL_ANDROID_PLATFORM
	#include "android/OsWrapperAndroid.h"
	#define OsWrapperImpl Pakal::OsWrapperAndroid
#else
class OsDummyWrapper{};
	typedef OsDummyWrapper OsWrapperImpl;
#endif

namespace Pakal
{

	class _PAKALExport OSManager : public OsWrapperImpl, public IManager
	{	
		template <class OSManager> friend class SingletonHolder;
		friend class Engine;
		friend class OsWrapperImpl;
	public :			
		void initialize() override;
		void terminate() override;
		void wait_for_os_events();

		struct WindowImpl
		{
			virtual unsigned setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned int bitsPerPixel) = 0;
			virtual void	process_os_events() = 0;
			virtual void wait_for_os_events() = 0;
			virtual ~WindowImpl(){}
		};

		struct WindowArgs
		{
			WindowArgs() : windowId(0), size_x(0), size_y(0){}
			unsigned windowId;
			unsigned size_x;
			unsigned size_y;
		};

		
		Event<OSManager,WindowArgs>	event_window_created;
		Event<OSManager,WindowArgs>	event_window_destroyed;
		Event<OSManager,WindowArgs>	event_window_redraw_needed;
		Event<OSManager,WindowArgs>	event_window_resized;
		Event<OSManager,bool>		event_window_focused;
		Event<OSManager,void>		event_app_finished;
		Event<OSManager,void>		event_app_paused;
		Event<OSManager,void>		event_app_resumed;
		Event<OSManager,void>		event_app_started;
		Event<OSManager,void>		event_app_stoped;

		virtual TaskPtr<WindowArgs> setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned int bitsPerPixel);
		virtual void  process_window_events();

		void on_window_created(const WindowArgs& arg);
		void on_window_destroyed(const WindowArgs& arg);
		void on_window_redraw_needed(const WindowArgs& arg);
		void on_window_resized(const WindowArgs& arg);
		void on_window_focused(bool);
		void on_app_finished();
		void on_app_paused();
		void on_app_resumed();
		void on_app_started();
		void on_app_stoped();

	protected:		
		WindowImpl* m_windowImpl;
		TaskCompletionSource<WindowArgs> m_windows_setup_task;

		static OSManager& instance();
		virtual ~OSManager();
		OSManager();
	};
}