#include "BasicTask.h"

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

	bool BasicTask::is_completed()
	{
		return m_completed;
	}

	void BasicTask::wait()
	{				
		std::unique_lock<std::mutex> lock(m_wait_mutex);
		m_wait_condition.wait(lock, [=](){ return is_completed();} );
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

	void BasicTask::set_completed(bool val)
	{
		m_completed = val;
		m_wait_condition.notify_one();
	}

	void BasicTask::run()
	{
		ASSERT(is_completed() == false);

		m_job();
		set_completed(true);
		m_event_completed.notify();
	}
}