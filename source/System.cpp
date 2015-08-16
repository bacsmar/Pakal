#include "System.h"

#include "EventScheduler.h"

namespace Pakal
{
	void System::update_loop()
	{
		do
		{
			m_dispatcher.dispatch_tasks();
			if (m_state == SystemState::Running)
			{
				on_update();
			}

		} while(m_threaded &&  m_state != SystemState::Terminated);
	}

	System::~System()
	{
		ASSERT_IF(m_state != SystemState::Terminated);
	}

	System::System(EventScheduler* scheduler, bool usesThread)
	{
		m_threaded = usesThread;
		m_thread_id = NULL_THREAD;
		m_thread = nullptr;
		m_scheduler = scheduler;
		m_state = SystemState::Created;
	}

	EventScheduler* System::get_scheduler()
	{
		return m_scheduler;
	}

	std::thread::id System::get_thread_id()
	{
		return m_thread_id;
	}

	bool System::is_threaded()
	{
		return m_threaded;
	}

	SystemState System::get_state()
	{
		return m_state; 
	}


	void System::update()
	{
		ASSERT_IF(m_threaded || m_state == SystemState::Terminated);

		m_dispatcher.dispatch_tasks();
		if (m_state == SystemState::Running)
		{
			on_update();
		}
	}

	BasicTaskPtr System::initialize()
	{
		ASSERT_IF(m_state != SystemState::Created && m_state != SystemState::Terminated);

		m_state = SystemState::Created;

		m_scheduler->register_dispatcher(&m_dispatcher);
			
		if (m_threaded)
		{
			m_thread = new std::thread(&System::update_loop,this);
			m_thread_id = m_thread->get_id();
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		}
		else
		{
			m_thread_id = std::this_thread::get_id();
			m_dispatcher.dispatch_tasks();
		}

		return  m_scheduler->execute_in_thread([this]()
		{
			on_initialize();
			m_state = SystemState::Running;
		},m_thread_id);

	}

	BasicTaskPtr System::terminate()
	{
		ASSERT_IF(m_state != SystemState::Running && m_state != SystemState::Paused );

		return m_scheduler->execute_in_thread([this]()
		{
			m_state = SystemState::Terminated;

			on_terminate();

			m_scheduler->deregister_dispatcher(&m_dispatcher);
			m_thread_id = NULL_THREAD;

			if (m_threaded)
			{
				m_thread->detach();
				SAFE_DEL(m_thread);
			}
		}, m_thread_id);
	}

	BasicTaskPtr System::pause()
	{
		ASSERT_IF(m_state != SystemState::Running);

		m_state = SystemState::Paused;

		return m_scheduler->execute_in_thread(std::bind(&System::on_pause,this),m_thread_id);
	}

	BasicTaskPtr System::resume()
	{
		ASSERT_IF(m_state != SystemState::Paused);

		return m_scheduler->execute_in_thread([this]()
		{
			on_resume();
			m_state = SystemState::Running;
		},m_thread_id);
	}
}