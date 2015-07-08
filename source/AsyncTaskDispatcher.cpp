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

	while (!m_inbox->empty())
	{
		BasicTaskPtr task = m_inbox->popTask();
		task->run();
	}

}