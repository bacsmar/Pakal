#pragma once

#include "Config.h"
#include "Event.h"

#include <thread>
#include <atomic>

namespace Pakal
{
	
	class _PAKALExport Timer
	{
		friend class TimerManager;	// in timer.cpp
		uint32_t	m_interval;
		uint32_t	m_scheduled;
		bool		running;
	public:
		Event<void>	event_elapsed;
		
		explicit Timer(unsigned ms = 100);
		~Timer();

		Timer(const Timer& other) = delete;
		Timer& operator=(const Timer& other) = delete;

		void set_interval(unsigned ms);

		void start();
		void stop();
	};	
}