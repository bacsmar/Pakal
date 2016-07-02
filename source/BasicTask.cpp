#include "BasicTask.h"
#include "EventScheduler.h"

namespace Pakal
{
	void BasicTask::run_continuations()
	{
		auto currentTid = THIS_THREAD;

		mutex_guard lock(m_continuation_mutex);

		for (auto& c : m_continuations)
		{
			if (c.tid == NULL_THREAD)
			{
				c.tid = currentTid;
			}
			EventScheduler::instance().execute_in_thread(c.continuation, c.tid);
		}
		m_continuations.clear();
	}

	void BasicTask::queue_continuation(const ContinuationData&& continuation)
	{
		mutex_guard lock(m_continuation_mutex);
		m_continuations.emplace_back(continuation);
	}

	void BasicTask::run()
	{
		ASSERT(is_completed() == false);

		m_job();		
		m_completed = true;
		run_continuations();
	}

	void BasicTask::wait(bool blocking)
	{
		EventScheduler::instance().wait_this_thread([=](){ return is_completed(); }, blocking);
	}

	BasicTaskPtr BasicTask::continue_with(const std::function<void()>& callBack, std::thread::id callBackThread)
	{
		auto task =	std::make_shared<BasicTask>(callBack);

		queue_continuation(ContinuationData(task, callBackThread));

		if (is_completed())
		{
			run_continuations();
		}

		return task;
	}



}