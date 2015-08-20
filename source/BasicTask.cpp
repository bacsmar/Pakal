#include "BasicTask.h"
#include "EventSystemUtils.h"

namespace Pakal
{
	BasicTask::BasicTask(const std::function<void()>& job, EventScheduler* scheduler): m_job(job)
	{
		m_completed = false;
		m_event_completed.connect_with_scheduler(scheduler);
	}

	BasicTask::BasicTask()
	{
		m_completed = true;
	}

	BasicTask::~BasicTask()
	{
	}

	void BasicTask::wait()
	{
		if (is_completed())
			return;
		
		std::unique_lock<std::mutex> lock(m_cv_m);
		m_cv.wait(lock, [=](){ return is_completed();} );
	}

	void BasicTask::on_completion(const std::function<void()>& callBack, std::thread::id callBackThread)
	{
		if (is_completed())
		{
			m_event_completed.clear();
			m_event_completed.add_listener(callBack,callBackThread);
			m_event_completed.notify();
		}
		else
			m_event_completed.add_listener(callBack,callBackThread);

	}

	EventScheduler* BasicTask::get_event_scheduler()
	{
		return m_event_completed.get_event_scheduler();
	}

	void BasicTask::run()
	{
		ASSERT(is_completed() == false);

		m_job();
		set_completed( true);
		m_event_completed.notify();
	}
}