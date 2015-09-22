#include "InboxQueue.h"

namespace Pakal
{

	BasicTaskPtr InboxTask::push_task(const std::function<void()>& jobDelegate)
	{
		ASSERT(this);

		BasicTaskPtr taskPtr = std::make_shared<BasicTask>(jobDelegate);

		m_inbox.push(taskPtr);

		//cv.nofity();

		return taskPtr;
	}

	void InboxTask::push_task(BasicTaskPtr task)
	{
		ASSERT(this);
		m_inbox.push(task);
		//cv.nofity();
	}
}