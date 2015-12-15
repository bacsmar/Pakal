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
#include "EventArgs.h"

namespace Pakal
{

	class _PAKALExport OSManagerAbstract :  public IManager
	{	
		template <class OSManager> friend class SingletonHolder;
		friend class Engine;		
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

		using WindowArgs = Pakal::WindowArgs;
		using MouseArgs = Pakal::MouseArgs;
		
		Event<WindowArgs>	event_window_created;
		Event<WindowArgs>	event_window_destroyed;
		Event<WindowArgs>	event_window_redraw_needed;
		Event<WindowArgs>	event_window_resized;
		Event<bool>		event_window_focused;
		Event<void>		event_app_finished;
		Event<void>		event_app_paused;
		Event<void>		event_app_resumed;
		Event<void>		event_app_started;
		Event<void>		event_app_stoped;

		Event<MouseArgs>event_mouse_click;
		Event<MouseArgs>event_mouse_released;
		Event<MouseArgs>event_mouse_moved;

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

		virtual std::string get_system_sources() = 0;

	protected:		
		WindowImpl* m_windowImpl;
		TaskCompletionSource<WindowArgs> m_windows_setup_task;
		
		virtual ~OSManagerAbstract();
		OSManagerAbstract();
	};	
}