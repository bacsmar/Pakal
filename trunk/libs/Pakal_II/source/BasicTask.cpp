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

	bool BasicTask::isCompleted()
	{
		return m_isCompleted;
	}

	void BasicTask::wait()
	{
		if (m_isCompleted)
			return;

		while (!m_isCompleted) std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	void BasicTask::onCompletionDo(const std::function<void()>& callBack, std::thread::id callBackThread)
	{
		if (m_isCompleted)
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
		m_isCompleted = true;
		m_EventCompleted.notify();
	}
}