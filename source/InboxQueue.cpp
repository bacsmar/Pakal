#include "InboxQueue.h"
#include "BasicTask.h"

namespace Pakal
{
	std::thread::id InboxQueue::get_tid()
	{
		return m_tid;
	}

	InboxQueue::InboxQueue(EventScheduler* dispatcher, std::thread::id tid): m_scheduler(dispatcher), m_tid(tid)
	{		
	}

	BasicTaskPtr InboxQueue::push_task(const std::function<void()>& jobDelegate)
	{
		ASSERT(this);

		BasicTaskPtr taskPtr = std::make_shared<BasicTask>(jobDelegate,m_scheduler);

		m_inbox.push(taskPtr);

		return taskPtr;
	}
}