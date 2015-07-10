#include "AsyncTaskDispatcher.h"
#include "InboxQueue.h"
#include "BasicTask.h"

#include "EventScheduler.h"


void Pakal::AsyncTaskDispatcher::dispatchTasks()
{
	if( nullptr == m_inbox )
	{
		EventScheduler * eventScheduler = m_scheduler;
		m_inbox = eventScheduler->InboxForThisThread();
	}
	
	if( m_inbox->size() )
	{
		auto & tasks = m_inbox->popAllTasks();
		while ( tasks.empty() == false)
		{
			BasicTaskPtr t = tasks.front();
			tasks.pop();
			t->run();
		}
	}
}