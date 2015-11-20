#include "System.h"
#include "Clock.h"

#include "EventScheduler.h"
#include "LogMgr.h"


namespace Pakal
{
	void System::update_loop()
	{
		ASSERT(m_threaded);

		while (m_dispatcher_ready == false) {}
				
		Clock clock;
		long long dt = 0;

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
				limit_fps(dt);
			}

			dt = clock.restart().asMilliseconds();
		} 
	}

	void System::limit_fps(const long long& frame_time_ms)
	{
		if (frame_time_ms < m_desired_frame_time_ms)
			std::this_thread::sleep_for(std::chrono::milliseconds(m_desired_frame_time_ms - frame_time_ms));
	}	

	System::~System()
	{
		ASSERT(m_state == SystemState::Terminated || m_state == SystemState::Created);
	}

	System::System(bool usesThread)
	{
		m_threaded = usesThread;
		m_thread = nullptr;
		m_state = SystemState::Created;
	}

	void System::update(long long dt)
	{
		ASSERT(m_threaded == false && m_state != SystemState::Terminated && m_state != SystemState::Created);

		m_dispatcher.dispatch_all_tasks();
		if (m_state == SystemState::Running)
		{
			on_update(dt);
			m_fps_counter.register_frame(dt);
			limit_fps(dt);
		}
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

			on_terminate();

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