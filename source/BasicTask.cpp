#include "BasicTask.h"

namespace Pakal
{
	BasicTask::BasicTask(const std::function<void()>& job, EventScheduler* scheduler): m_Job(job), m_isCompleted(false)
	{
		m_EventCompleted.connectWithScheduler(scheduler);
	}

	BasicTask::BasicTask(): m_isCompleted(true)
	{
	}

	BasicTask::~BasicTask()
	{
	}	

	void BasicTask::wait()
	{
		if ( isCompleted() )
			return;
		
		std::unique_lock<std::mutex> lock(m_cv_m);
		m_cv.wait(lock, [&](){ return isCompleted();} );
	}

	void BasicTask::onCompletionDo(const std::function<void()>& callBack, std::thread::id callBackThread)
	{
		if (isCompleted())
			callBack();
		else
			m_EventCompleted.addListener(callBack,callBackThread);
	}

	EventScheduler* BasicTask::getEventScheduler()
	{
		return m_EventCompleted.getEventScheduler();
	}

	void BasicTask::run()
	{
		ASSERT(m_isCompleted == false);

		m_Job();
		setIsCompleted(true);
		m_EventCompleted.notify();
	}
}