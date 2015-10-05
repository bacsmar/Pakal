#include "Timer.h"

namespace Pakal
{
	void Timer::thread_loop()
	{
		std::mutex mutex;
		std::unique_lock<std::mutex> lk(mutex);

		do
		{
			m_condition.wait(lk, [this]() { return m_running || !m_active; });

			while (m_running)
			{
				m_condition.wait_for(lk, std::chrono::milliseconds(m_interval));
				event_elapsed.notify();
			}

		} while (m_active);
	}

	Timer::Timer(): m_running(false), m_active(true) , m_interval(100)
	{
		m_thread = std::make_unique<std::thread>(&Timer::thread_loop,this);
	}

	Timer::Timer(unsigned ms): Timer()
	{
		set_interval(ms);
	}

	Timer::~Timer() 
	{
		m_active = false;
		stop();

		m_thread->detach();
	}

	void Timer::set_interval(unsigned ms) 
	{
		ASSERT(ms > 0);
		m_interval = ms;
	}

	void Timer::start() 
	{
		event_elapsed.enable();
		m_running = true;
		m_condition.notify_one();
	}

	void Timer::stop() 
	{
		event_elapsed.disable();
		m_running = false;
		m_condition.notify_one();
	}
}