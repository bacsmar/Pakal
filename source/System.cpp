#include "System.h"
#include "Clock.h"

#include "EventScheduler.h"
#include "LogMgr.h"

namespace Pakal
{
	// for threaded systems... executed in their own thread
	void System::update_loop()
	{
		ASSERT(m_threaded);

		while (m_dispatcher_ready == false) {}
				
		Clock clock;
		unsigned dt = 0;

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
			
			dt = clock.restart().asMilliseconds();
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

	void System::limit_fps()
	{
		auto frame_time_ms = m_fps_limiter_clock.restart().asMilliseconds();
		long sleepTime_us = (m_desired_frame_time_ms - frame_time_ms) * 1000;  // Convert to microseconds
		if( sleepTime_us > 0)
		{
			// Sleep in tiny chunks to allow quick termination response
			// Use microseconds for better interrupt responsiveness
			const long chunk_us = 100;  // 100 microsecond chunks for fast termination response
			while (sleepTime_us > 0 && m_state == SystemState::Running)
			{
				// Process any pending tasks (e.g., terminate) while waiting
				m_dispatcher.dispatch_all_tasks();
				if (m_state != SystemState::Running)
				{
					break;
				}
				long sleep_chunk = sleepTime_us < chunk_us ? sleepTime_us : chunk_us;
				std::this_thread::sleep_for(std::chrono::microseconds(sleep_chunk));
				sleepTime_us -= sleep_chunk;
			}
		}
		m_fps_limiter_clock.restart();
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