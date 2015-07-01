#include "InboxQueue.h"
#include "BasicTask.h"

namespace Pakal
{
	InboxQueue::InboxQueue(EventScheduler* dispatcher): m_scheduler(dispatcher)
	{
			
	}

	BasicTaskPtr InboxQueue::popTask()
	{
		return BasicTaskPtr(reinterpret_cast<BasicTask*>(m_inboxStore.dequeueNotification()));
	}

	BasicTaskPtr InboxQueue::waitPopTask()
	{
		return BasicTaskPtr(reinterpret_cast<BasicTask*>(m_inboxStore.waitDequeueNotification()));
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