#include "InboxQueue.h"

namespace Pakal
{
	InboxQueue::InboxQueue(EventScheduler* dispatcher): m_scheduler(dispatcher)
	{
			
	}

	Poco::AutoPtr<BasicTask> InboxQueue::popTask()
	{
		return static_cast<BasicTask*>(m_inboxStore.dequeueNotification());
	}

	Poco::AutoPtr<BasicTask> InboxQueue::waitPopTask()
	{
		return static_cast<BasicTask*>(m_inboxStore.waitDequeueNotification());
	}

	bool InboxQueue::empty()
	{
		return m_inboxStore.size() == 0;
	}

	int InboxQueue::size()
	{
		return m_inboxStore.size();
	}


}