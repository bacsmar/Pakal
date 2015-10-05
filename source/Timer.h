#pragma once

#include "Config.h"
#include "Event.h"

#include <thread>
#include <atomic>


/*
Author: Luis Gudiño Salinas
Behaviour:
	default is 100 ms although you can set a different interval using the method set_interval

	then you call start and the event_elapsed is going to be notified every (N ms + the time the event takes to notify the listeners)
	all methods are thread safe

	it uses a std::thread all the time the object is alive and does not recreate a new thread every time you call start

*/
namespace Pakal
{
	
	class _PAKALExport Timer
	{
		UniquePtr<std::thread> m_thread;

		std::condition_variable m_condition;

		std::atomic_bool m_running, m_active;
		std::atomic_uint m_interval;

		void thread_loop();

	public:
		Event<Timer, void> event_elapsed;

		Timer();
		Timer(unsigned ms);
		~Timer();

		Timer(const Timer& other) = delete;
		Timer& operator=(const Timer& other) = delete;

		void set_interval(unsigned ms);

		void start();
		void stop();
	};

}