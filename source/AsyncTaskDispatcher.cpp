#include "AsyncTaskDispatcher.h"
#include "InboxQueue.h"
#include "BasicTask.h"

#include "EventScheduler.h"



std::thread::id Pakal::AsyncTaskDispatcher::threadId()
{
	return m_inbox->getTid();
}

void Pakal::AsyncTaskDispatcher::dispatchTasks()
{
	if( nullptr == m_inbox )
	{
		EventScheduler * eventScheduler = m_scheduler;
		m_inbox = eventScheduler->InboxForThisThread();
	}
	
	if( m_inbox->size() )
	{
		InboxQueue::TaskQueue& tasks = m_inbox->popAllTasks();
		while (!tasks.empty())
		{
			BasicTaskPtr t = tasks.front();
			tasks.pop();
			t->run();
		}
	}
}