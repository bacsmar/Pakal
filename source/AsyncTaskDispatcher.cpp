#include "AsyncTaskDispatcher.h"
#include "InboxQueue.h"
#include "BasicTask.h"

#include "EventScheduler.h"



std::thread::id Pakal::AsyncTaskDispatcher::thread_id()
{
	return m_inbox == nullptr ? NULL_THREAD : m_inbox->getTid();
}

Pakal::EventScheduler* Pakal::AsyncTaskDispatcher::get_scheduler()
{
	return m_scheduler;
}

void Pakal::AsyncTaskDispatcher::dispatch_tasks()
{
	if( nullptr == m_inbox )
	{
		m_inbox = m_scheduler->inbox_for_this_thread();
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