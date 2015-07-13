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
}