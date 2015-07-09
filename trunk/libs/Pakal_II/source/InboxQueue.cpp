#include "InboxQueue.h"
#include "BasicTask.h"
#include "Task.h"
#include <memory>

namespace Pakal
{
	InboxQueue::InboxQueue(EventScheduler* dispatcher): m_scheduler(dispatcher), m_ActiveQueue(0)
	{
		m_inboxStore = &m_inboxStores[m_ActiveQueue];
	}

	BasicTaskPtr InboxQueue::pushTask(std::function<void()>& jobDelegate)
	{
		std::function<int(void)> delegate = [jobDelegate]()
			{
				jobDelegate();
				return 0;
			};

		Task<int>* ptr = new Task<int>(delegate, m_scheduler);
		BasicTaskPtr taskPtr(ptr);

		m_inboxStore->push(taskPtr);

		return taskPtr;
	}

	BasicTaskPtr InboxQueue::popTask()
	{
		BasicTaskPtr task = m_inboxStore->front();
		m_inboxStore->pop();
		return task;
	}

	inline InboxQueue::TaskQueue& InboxQueue::popAllTasks()
	{
		m_TaskQueueMutex.lock();
		int queueToProcess = m_ActiveQueue;
		m_ActiveQueue = (m_ActiveQueue + 1) % MAX_INITIALIZATION_QUEUES;
		m_inboxStore = &m_inboxStores[m_ActiveQueue];
		m_TaskQueueMutex.unlock();

		return m_inboxStores[queueToProcess];
	}


	bool InboxQueue::empty()
	{
		return m_inboxStore->empty();
	}

	int InboxQueue::size()
	{
		return m_inboxStore->size();
	}
}