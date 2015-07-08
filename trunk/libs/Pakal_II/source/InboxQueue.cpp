#include "InboxQueue.h"
#include "BasicTask.h"
#include "Task.h"
#include <memory>

namespace Pakal
{
	InboxQueue::InboxQueue(EventScheduler* dispatcher): m_scheduler(dispatcher)
	{
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

		m_inboxStore.push(taskPtr);

		return taskPtr;
	}

	BasicTaskPtr InboxQueue::popTask()
	{
		BasicTaskPtr task = m_inboxStore.front();
		m_inboxStore.pop();
		return task;
	}


	bool InboxQueue::empty()
	{
		return m_inboxStore.empty();
	}

	int InboxQueue::size()
	{
		return m_inboxStore.size();
	}
}