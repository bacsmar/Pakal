#include "AsyncTaskDispatcher.h"
#include "BasicTask.h"

void Pakal::AsyncTaskDispatcher::dispatch_tasks()
{
	ASSERT(thread_id() == std::this_thread::get_id());

	if(m_inbox->size() > 0)
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