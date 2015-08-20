#include "System.h"

#include "EventScheduler.h"

namespace Pakal
{
	void System::update_loop()
	{
		m_dispatcher.dispatch_tasks();
		m_is_initialized = true;
		m_cv.notify_one();

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
		return m_dispatcher.thread_id();
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
		m_is_initialized = false;

		m_scheduler->register_dispatcher(&m_dispatcher);
			
		if (m_threaded)
		{
			m_thread = new std::thread(&System::update_loop,this);

			// wait	until the system dispatch his first tasks in the update_loop
			std::unique_lock<std::mutex> lock(m_cv_m);
			m_cv.wait(lock, [=](){ return m_is_initialized;} );
		}
		else
		{
			m_dispatcher.dispatch_tasks();
		}

		return  m_scheduler->execute_in_thread([this]()
		{
			m_state = SystemState::Running;
			on_initialize();
		},m_dispatcher.thread_id());

	}

	BasicTaskPtr System::terminate()
	{
		ASSERT_IF(m_state != SystemState::Running && m_state != SystemState::Paused );

		return m_scheduler->execute_in_thread([this]()
		{
			m_scheduler->deregister_dispatcher(&m_dispatcher);

			m_state = SystemState::Terminated;
			on_terminate();

			if (m_threaded)
			{
				m_thread->detach();
				SAFE_DEL(m_thread);
			}
		}, m_dispatcher.thread_id());
	}

	BasicTaskPtr System::pause()
	{
		ASSERT_IF(m_state != SystemState::Running);

		m_state = SystemState::Paused;

		return m_scheduler->execute_in_thread(std::bind(&System::on_pause,this),m_dispatcher.thread_id());
	}

	BasicTaskPtr System::resume()
	{
		ASSERT_IF(m_state != SystemState::Paused);

		return m_scheduler->execute_in_thread([this]()
		{
			on_resume();
			m_state = SystemState::Running;
		},m_dispatcher.thread_id());
	}
}