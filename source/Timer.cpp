#include "Timer.h"
#include "LogMgr.h"
#include <set>
#include "SingletonHolder.h"
#include "Clock.h"
#include "Engine.h"
#include <algorithm>

namespace Pakal
{
	class TimerManager
	{
		UniquePtr<std::thread> m_thread;
		std::recursive_mutex m_timermap_mutex;

		std::vector<Timer*> m_timers;
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
			std::lock_guard<std::recursive_mutex> lock(m_timermap_mutex);
			// 1. find an empty slot
			auto timerIt = std::find_if(m_timers.begin(), m_timers.end(), [](const Timer* timerData)
			{
				return timerData == nullptr;
			});
			// 2. if found. use that empty slot
			if(timerIt != m_timers.end() )
			{
				*timerIt = &t;
			}
			// 3 . if not empty slot found, then create a new one
			else
			{				
				m_timers.emplace_back(&t);
			}
			m_wake_condition.notify_one();
			return true;
		}

		bool remove_timer(Timer& t)
		{
			std::lock_guard<std::recursive_mutex> lock(m_timermap_mutex);			

			// 1. find the timer
			auto timerIt = std::find_if(m_timers.begin(), m_timers.end(), [&](const Timer* timerData)
			{
				return (timerData) == &t;
			});
			// mark it as unused
			if (timerIt != m_timers.end())
			{
				*timerIt = nullptr;
			}
			m_wake_condition.notify_one();
			return true;
		}
		
		static TimerManager& instance()
		{
			static SingletonHolder<TimerManager> sh;
			return *sh.get();
		}

		void init()
		{
			m_thread = UniquePtr<std::thread>(new std::thread(&TimerManager::timer_thread, this));
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

					for (auto timer : m_timers )
					{
						if (timer)
						{
							min_schedule = timer->m_scheduled < min_schedule ? timer->m_scheduled : min_schedule;

							if (timer->m_scheduled <= currentTime && timer->running)
							{
								timer->m_scheduled = currentTime + timer->m_interval;
								timer->event_elapsed.notify();
							}
						}
					}
				}

				std::this_thread::sleep_for(std::chrono::milliseconds(1));				
				//m_wake_condition.wait_for(lk, std::chrono::milliseconds(min_schedule - currentTime), [this]() { return m_timers.size() > 0 || m_active == false; });
			}

			if(m_timers.size() > 0)
			{
				LOG_WARNING("[Timer Manager] there are %d timers remaining active", m_timers.size());
			}
		}

		TimerManager()
		{
			m_timers.reserve(128);
			m_active = true;			
		}
		~TimerManager()
		{
			m_active = false;
			m_wake_condition.notify_one();
			LOG_INFO("[Timer Manager] waiting event_elapsed to finalize...");
			m_thread->join();
		}
	};

	void Engine::init_timer_system()
	{
		TimerManager::instance().init();
	}

	Timer::Timer(unsigned ms) : m_interval(ms), m_scheduled(0), running(false)
	{
		ASSERT(ms > 0);
		TimerManager::instance().add_timer(*this);
	}

	Timer::~Timer()
	{
		if (running)
			stop();
		TimerManager::instance().remove_timer(*this);
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
		running = true;
	}

	void Timer::stop()
	{
		running = false;
		event_elapsed.disable();
	}
}

