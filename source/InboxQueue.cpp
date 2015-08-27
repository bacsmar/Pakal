#include "InboxQueue.h"

namespace Pakal
{

	BasicTaskPtr InboxQueue::push_task(const std::function<void()>& jobDelegate)
	{
		ASSERT(this);

		BasicTaskPtr taskPtr = std::make_shared<BasicTask>(jobDelegate);

		m_inbox.push(taskPtr);

		return taskPtr;
	}

	void InboxQueue::push_task(BasicTaskPtr task)
	{
		ASSERT(this);
		m_inbox.push(task);
	}
}