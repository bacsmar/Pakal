#include "InboxQueue.h"
#include "BasicTask.h"

namespace Pakal
{
	std::thread::id InboxQueue::getTid()
	{
		return m_tid;
	}

	InboxQueue::InboxQueue(EventScheduler* dispatcher, std::thread::id tid): m_scheduler(dispatcher), m_tid(tid)
	{		
	}

	BasicTaskPtr InboxQueue::pushTask(const std::function<void()>& jobDelegate)
	{
		ASSERT(this);

		BasicTaskPtr taskPtr = std::make_shared<BasicTask>(jobDelegate,m_scheduler);

		m_inboxStore.push(taskPtr);

		return taskPtr;
	}
}