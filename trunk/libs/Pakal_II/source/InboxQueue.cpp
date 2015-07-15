#include "InboxQueue.h"
#include "BasicTask.h"
#include "Task.h"
#include <memory>

namespace Pakal
{
	unsigned long InboxQueue::getTid()
	{
		return m_tid;
	}

	InboxQueue::InboxQueue(EventScheduler* dispatcher,Poco::Thread::TID tid): m_scheduler(dispatcher), m_tid(tid)
	{		
	}

	BasicTaskPtr InboxQueue::pushTask(const std::function<void()>& jobDelegate)
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
}