#include "BasicTask.h"

namespace Pakal
{
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

	void BasicTask::set_completed() 
	{
		ASSERT(!is_completed());
		m_completed = true;
		m_wait_condition.notify_one();			
	}

	void BasicTask::run()
	{
		ASSERT(is_completed() == false);

		m_job();
		set_completed();
		m_event_completed.notify();
	}
}