#include "EventScheduler.h"

#include "InboxQueue.h"
#include "AsyncTaskDispatcher.h"
#include "Task.h"

Pakal::EventScheduler::~EventScheduler()
{
	for(auto& item : m_inboxes)
	{
		delete item.second;
	}

	m_inboxes.clear();
}

Pakal::InboxQueue* Pakal::EventScheduler::findInboxForThread(Poco::Thread::TID currentTid)
{	
	auto position = m_inboxes.find(currentTid);

	InboxQueue* inbox = position == m_inboxes.end() 
		       ? nullptr
		       : position->second;	
	return inbox;
}

Pakal::InboxQueue* Pakal::EventScheduler::InboxForThisThread()
{
	auto currentTid = Poco::Thread::currentTid();

	auto inbox = findInboxForThread(currentTid);
		
	if (inbox == nullptr)
	{
		inbox = new InboxQueue(this,currentTid);
		m_inboxes[currentTid] =  inbox;
	}
	return inbox;
}

Pakal::BasicTaskPtr Pakal::EventScheduler::executeInThread(const std::function<void()>& fn,Poco::Thread::TID tid)
{
	auto currentTid = Poco::Thread::currentTid();

	if (currentTid == tid)
	{
		fn();
		return TaskUtils::fromResult(0);
	}
	else
		return findInboxForThread(tid)->pushTask(fn);
}


void Pakal::EventScheduler::registerDispatcher(AsyncTaskDispatcher* dispatcher)
{
	dispatcher->setScheduler(this);
}