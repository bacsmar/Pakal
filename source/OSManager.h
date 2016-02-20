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
#include "PakalMath.h"
#include "Event.h"
#include "EventArgs.h"
#include "TaskCompletionSource.h"

#include <functional>


namespace Pakal
{	
	class IInputManager;

	class OSManager
	{
		template <class OSManager> friend class SingletonHolder;

	public:
		struct IWindowManager
		{
			virtual unsigned setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned int bitsPerPixel) = 0;
			virtual void close_window() = 0;
			virtual void process_window_events() = 0;
			virtual ~IWindowManager() {}
		};
		struct Settings
		{
			std::function<IInputManager*(OSManager*)> input_manager_allocator;
			std::function<IWindowManager*(OSManager*)> window_manager_allocator;
		};
	private:

		IWindowManager*	m_window_impl;
		IInputManager*  m_input_manager;
		TaskCompletionSource<WindowArgs> m_windows_setup_task;

	protected:
		virtual ~OSManager() {}
		OSManager() = default;
	public:
		static OSManager& instance();

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

		inline IInputManager* get_input_manager() const { return m_input_manager; };
		inline IWindowManager* get_window_manager() const { return m_window_impl;  };
		inline virtual std::string get_app_path() { return "./";  };
		
		void initialize(const Settings& settings);
		void terminate();

		TaskPtr<WindowArgs> setup_window(unsigned windowId, const tmath::vector2di& dimensions, bool fullscreen, unsigned int bitsPerPixel);
		void close_window();

		void  process_window_events();

		virtual void on_window_created(const WindowArgs& arg);
		virtual void on_window_destroyed(const WindowArgs& arg);
		
	};
}

#define OSMgr Pakal::OSManager::instance()