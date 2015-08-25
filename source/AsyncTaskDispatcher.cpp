#include "AsyncTaskDispatcher.h"
#include "BasicTask.h"

#include "EventScheduler.h"


void Pakal::AsyncTaskDispatcher::dispatch_tasks()
{
	if( nullptr == m_inbox )
	{
		m_inbox = m_scheduler->inbox_for_this_thread();
	}
	
	if( m_inbox->size() )
	{
		auto& tasks = m_inbox->pop_all_tasks();
		while (!tasks.empty())
		{
			BasicTaskPtr t = tasks.front();
			tasks.pop();
			t->run();
		}
	}
}