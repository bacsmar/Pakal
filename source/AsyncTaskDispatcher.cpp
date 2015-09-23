#include "AsyncTaskDispatcher.h"
#include "BasicTask.h"

void Pakal::AsyncTaskDispatcher::dispatch_tasks(bool block)
{
	ASSERT(thread_id() == THIS_THREAD);

	m_inbox->swap_buffer();

	while (m_inbox !=nullptr && (!m_inbox->empty() || block))
	{
		m_inbox->pop_task(block)->run();
		block = false;
	}
}