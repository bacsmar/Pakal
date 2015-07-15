#include "InboxQueue.h"
#include "BasicTask.h"
#include <memory>

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
		BasicTaskPtr taskPtr = std::make_shared<BasicTask>(jobDelegate,m_scheduler);

		m_inboxStore.push(taskPtr);

		return taskPtr;
	}
}