#include "AsyncTaskDispatcher.h"
#include "BasicTask.h"

//TODO create an option to wait for tasks
void Pakal::AsyncTaskDispatcher::dispatch_tasks()
{
	ASSERT(thread_id() == THIS_THREAD);

	m_inbox->swap_buffer();

	while (m_inbox !=nullptr && !m_inbox->empty())
	{
		BasicTaskPtr t = m_inbox->pop_task();
		t->run();
	}
}