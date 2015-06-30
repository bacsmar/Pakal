#include "AsyncTaskDispatcher.h"
#include "InboxQueue.h"
#include "BasicTask.h"


void Pakal::AsyncTaskDispatcher::dispatchTasks()
{
	auto inbox = getInbox();

	while (!inbox->empty())
	{
		Poco::AutoPtr<BasicTask> task = inbox->popTask();
		task->run();
	}

}