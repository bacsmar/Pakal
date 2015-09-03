#include "BasicTask.h"
#include "EventScheduler.h"

namespace Pakal
{

	void BasicTask::set_completed() 
	{
		ASSERT(!m_completed);
		m_completed = true;
	}

	void BasicTask::queue_continuations()
	{
		auto currentTid = THIS_THREAD;

		std::lock_guard<std::mutex> lock(m_continuation_mutex);

		for(auto& c : m_continuations )
		{
			if (c.tid == NULL_THREAD)
			{
				c.tid = currentTid;
			}
			EventScheduler::instance().execute_in_thread(c.continuation,c.tid);
		}
		m_continuations.clear();
	}

	void BasicTask::run()
	{
		ASSERT(is_completed() == false);

		m_job();
		set_completed();
		queue_continuations();
	}

	void BasicTask::wait() 
	{
		EventScheduler::instance().wait_this_thread([=](){ return is_completed(); });
	}

	BasicTaskPtr BasicTask::continue_with(const std::function<void()>& callBack, std::thread::id callBackThread)
	{
		auto task =	std::make_shared<BasicTask>(callBack);

		m_continuation_mutex.lock();
		m_continuations.push_back(ContinuationData(task,callBackThread));
		m_continuation_mutex.unlock();

		if (is_completed())
		{
			queue_continuations();
		}

		return task;
	}
}