#include "AsyncTaskDispatcher.h"
#include "BasicTask.h"

void Pakal::AsyncTaskDispatcher::dispatch_all_tasks()
{
	ASSERT(thread_id() == THIS_THREAD);

	m_inbox->swap_buffer();

	while (m_inbox !=nullptr && !m_inbox->empty())
	{
		m_inbox->pop_task(false)->run();
	}
}

void Pakal::AsyncTaskDispatcher::dispatch_one_task(bool waitIfNone)
{
	ASSERT(thread_id() == THIS_THREAD);

	m_inbox->swap_buffer();

	if (m_inbox != nullptr && (!m_inbox->empty() || waitIfNone))
	{
		m_inbox->pop_task(waitIfNone)->run();
	}

}