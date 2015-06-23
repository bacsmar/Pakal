#include "AsyncTaskDispatcher.h"
#include "InboxQueue.h"


void Pakal::AsyncTaskDispatcher::dispatchTasks(bool waitForTasks)
{
	auto inbox = getInbox();

	while (!inbox->empty())
	{
		Poco::AutoPtr<BasicTask> task = waitForTasks ? inbox->waitPopTask() : inbox->popTask();
		task->run();
	}

}