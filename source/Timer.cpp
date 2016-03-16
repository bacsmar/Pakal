#include "Timer.h"
#include "LogMgr.h"
#include <set>
#include "SingletonHolder.h"
#include "Clock.h"

namespace Pakal
{
	class TimerManager
	{
		UniquePtr<std::thread> m_thread;
		std::recursive_mutex m_timermap_mutex;
		std::set<Timer*> m_timers;
		Pakal::Clock m_clock;

		std::atomic_bool m_active;

		std::condition_variable m_wake_condition;

	public:
		template <class TimerManager> friend class SingletonHolder;

		inline uint32_t get_milliseconds() const
		{
			return m_clock.getElapsedTime().asMilliseconds();
		}
		
		bool add_timer(Timer& t)
		{
			//TODO: double queued list... what happens if you add a new timer when the timeElapsed event is fired?
			std::lock_guard<std::recursive_mutex> lock(m_timermap_mutex);
			m_wake_condition.notify_one();
			return m_timers.insert(&t).second;
		}

		bool remove_timer(Timer& t)
		{
			std::lock_guard<std::recursive_mutex> lock(m_timermap_mutex);
			m_wake_condition.notify_one();
			return m_timers.erase(&t) > 0;
		}
		
		static TimerManager& instance()
		{
			static SingletonHolder<TimerManager> sh;
			return *sh.get();
		}
	protected:
		void timer_thread()
		{
			/* Threaded timer loop:
			*  1. Queue timers added by other threads
			*  2. Handle any timers that should dispatch this cycle
			*  3. Wait until next dispatch time or new timer arrives
			*/

			std::mutex mutex;
			std::unique_lock<std::mutex> lk(mutex);

			m_clock.restart();
			while ( m_active == true )
			{
				auto currentTime = m_clock.getElapsedTime().asMilliseconds();
				uint32_t min_schedule = -1;	// its unsigned so.. choose the biggest value possible
				// mutex scope
				{
					std::lock_guard<std::recursive_mutex> m(m_timermap_mutex);

					for (auto it = m_timers.begin(); it != m_timers.end(); )
					{
						auto timer = *it;
						min_schedule = timer->m_scheduled < min_schedule ? timer->m_scheduled : min_schedule;

						if (timer->m_scheduled <= currentTime)
						{
							it = m_timers.erase(it);
							timer->event_elapsed.notify();
						}
						else
						{
							//TODO: double queued list... what happens if you add a new timer when the timeElapsed event is fired?
							++it;
						}
					}
				}

				//std::this_thread::sleep_for(std::chrono::milliseconds(1));				
				m_wake_condition.wait_for(lk, std::chrono::milliseconds(min_schedule - currentTime), [this]() { return m_timers.size() > 0 || m_active == false; });
			}

			if(m_timers.size() > 0)
			{
				LOG_WARNING("[Timer Manager] there are %d timers remaining active", m_timers.size());
			}
		}
		TimerManager()
		{
			m_active = true;
			m_thread = UniquePtr<std::thread>(new std::thread(&TimerManager::timer_thread, this));
		}
		~TimerManager()
		{
			m_active = false;
			m_wake_condition.notify_one();
			LOG_INFO("[Timer Manager] waiting event_elapsed to finalize...");
			m_thread->join();
		}
	};

	Timer::Timer() : m_interval(100), m_scheduled(0), running(false)
	{
	}

	Timer::Timer(unsigned ms) : m_interval(ms), m_scheduled(0), running(false)
	{
	}

	Timer::~Timer()
	{
		if (running)
			stop();
	}

	void Timer::set_interval(unsigned ms)
	{
		ASSERT(ms > 0);
		m_interval = ms;
	}

	void Timer::start()
	{
		m_scheduled = TimerManager::instance().get_milliseconds() + m_interval;
		event_elapsed.enable();
		running = TimerManager::instance().add_timer(*this);
	}

	void Timer::stop()
	{
		event_elapsed.disable();
		TimerManager::instance().remove_timer(*this);
		running = false;
	}
}

