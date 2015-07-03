#include "InboxQueue.h"
#include "BasicTask.h"
#include "Task.h"

namespace Pakal
{
	InboxQueue::InboxQueue(EventScheduler* dispatcher): m_scheduler(dispatcher)
	{
			
	}

	BasicTaskPtr InboxQueue::popTask()
	{
		Poco::Notification *notification = m_inboxStore.dequeueNotification();		
		TaskBridge *t = reinterpret_cast<TaskBridge*>( notification );		
		return BasicTaskPtr(t);
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