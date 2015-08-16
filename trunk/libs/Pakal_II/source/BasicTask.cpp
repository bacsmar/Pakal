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

	bool BasicTask::is_completed()
	{
		return m_completed;
	}

	void BasicTask::wait()
	{
		if (m_completed)
			return;

		while (!m_completed) std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	void BasicTask::on_completion(const std::function<void()>& callBack, std::thread::id callBackThread)
	{
		if (m_completed)
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
		ASSERT(m_completed == false);

		m_job();
		m_completed = true;
		m_event_completed.notify();
	}
}