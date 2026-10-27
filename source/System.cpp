#include "System.h"
#include "Clock.h"

#include "EventScheduler.h"
#include "LogMgr.h"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <timeapi.h>
#endif

namespace Pakal
{
	// for threaded systems... executed in their own thread
	void System::update_loop()
	{
		ASSERT(m_threaded);

		while (m_dispatcher_ready == false) {}
				
		Clock clock;
		unsigned dt = 0;
		long long carry_us = 0;   // what whole milliseconds leave out, so the system's time does not run slow

		while(m_state != SystemState::Terminated)
		{

			if (m_state == SystemState::Paused)
			{
				m_dispatcher.dispatch_one_task(true);
				dt = 0;
				clock.restart();
			}
			else
			{
				m_dispatcher.dispatch_all_tasks();
			}

			if (m_state == SystemState::Running)
			{
				on_update(dt);
				m_fps_counter.register_frame(dt);
				limit_fps();
			}
			
			const long long us = clock.restart().asMicroseconds() + carry_us;
			dt = static_cast<unsigned>(us / 1000);
			carry_us = us % 1000;
		} 
	}

	// for thread less systems... (executed in main thread)
	void System::update(unsigned long dtMilliSeconds)
	{
		ASSERT(m_threaded == false && m_state != SystemState::Terminated && m_state != SystemState::Created);

		m_dispatcher.dispatch_all_tasks();
		if (m_state == SystemState::Running)
		{
			on_update(dtMilliSeconds);
			m_fps_counter.register_frame(dtMilliSeconds);
			limit_fps();
		}
	}

	// Keeps the system at its target rate with a deadline per frame (not "sleep what is left of this one", which drifts
	// and, with whole milliseconds and Windows' 15.6 ms timer, overslept: 60 FPS with vsync landed on ~33). Sleeps until
	// a millisecond before the deadline and yields the rest; a frame that ran late starts a new cadence instead of
	// rushing the next ones.
	void System::limit_fps()
	{
		using clock = std::chrono::steady_clock;
		if (m_frame_period.count() == 0)
			return;
#ifdef _WIN32
		static const bool fine_timer = timeBeginPeriod(1) == TIMERR_NOERROR;   // the 1 ms scheduler tick, for the whole process
		(void)fine_timer;
#endif
		const auto now = clock::now();
		m_next_frame += m_frame_period;
		if (m_next_frame < now - m_frame_period || m_next_frame > now + 2 * m_frame_period)
		{
			m_next_frame = now;   // late by more than a frame (a hitch, a pause) or a new rate: start over from here
			return;
		}
		if (m_next_frame - now > std::chrono::milliseconds(1))
			std::this_thread::sleep_for(m_next_frame - now - std::chrono::milliseconds(1));
		while (clock::now() < m_next_frame)
			std::this_thread::yield();
	}

	System::~System()
	{
		ASSERT(m_state == SystemState::Terminated || m_state == SystemState::Created);
	}

	System::System(bool usesThread) : m_state(SystemState::Created), m_threaded(usesThread)
	{		
	}

	BasicTaskPtr System::initialize()
	{
		ASSERT(m_state == SystemState::Created || m_state == SystemState::Terminated);

		m_state = SystemState::Created;
		if (is_threaded())
		{
			m_dispatcher_ready = false;
			m_thread = new std::thread(&System::update_loop,this);
			EventScheduler::instance().register_dispatcher_for_thread(&m_dispatcher,m_thread->get_id());
			m_dispatcher_ready = true;
		}
		else
		{
			EventScheduler::instance().register_dispatcher_for_thread(&m_dispatcher,THIS_THREAD);
		}

		return EventScheduler::instance().execute_in_thread([this]()
		{
			on_initialize();
			m_state = SystemState::Running;
		},m_dispatcher.thread_id());

	}

	BasicTaskPtr System::terminate()
	{
		ASSERT(m_state == SystemState::Running || m_state == SystemState::Paused );

		return EventScheduler::instance().execute_in_thread([this]()
		{
			m_dispatcher.dispatch_all_tasks();
			m_dispatcher.dispatch_all_tasks(); //for both lists

			LOG_INFO("stopping %s", this->get_system_name());
			on_terminate();
			LOG_INFO("stopping %s... done", this->get_system_name());

			EventScheduler::instance().deregister_dispatcher(&m_dispatcher);

			m_state = SystemState::Terminated;

			if (is_threaded())
			{
				m_thread->detach();
				SAFE_DEL(m_thread);
			}
		}, m_dispatcher.thread_id());
	}

	BasicTaskPtr System::pause()
	{
		ASSERT(m_state == SystemState::Running);

		return EventScheduler::instance().execute_in_thread([this]()
		{
			on_pause();
			m_state = SystemState::Paused;
		}, m_dispatcher.thread_id());
	}

	BasicTaskPtr System::resume()
	{
		ASSERT(m_state == SystemState::Paused);

		return EventScheduler::instance().execute_in_thread([this]()
		{
			on_resume();
			m_state = SystemState::Running;
		},m_dispatcher.thread_id());
	}

	BasicTaskPtr System::execute_block(const std::function<void()>& block)
	{
		return EventScheduler::instance().execute_in_thread(block,thread_id());
	}
}