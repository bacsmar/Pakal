#include "TaskCompletionSource.h"
#include "BasicTask.h"

namespace Pakal
{
	TaskCompletionSource::TaskCompletionSource(EventScheduler* scheduler)
	{
		static auto emptyDelegate = [](){ };
		m_task = std::make_shared<BasicTask>(emptyDelegate,scheduler);
	}

	BasicTaskPtr TaskCompletionSource::get_task()
	{
		return m_task;
	}

	void TaskCompletionSource::set_completed()
	{
		m_task->run();
	}
}