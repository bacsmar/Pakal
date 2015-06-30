#include "EventScheduler.h"
#include "InboxQueue.h"
#include "AsyncTaskDispatcher.h"

Pakal::EventScheduler::~EventScheduler()
{
	for(auto& item : m_inboxes)
	{
		delete item.second;
	}

	m_inboxes.clear();
}

Pakal::InboxQueue* Pakal::EventScheduler::InboxForThread(Poco::Thread::TID currentTid)
{	
	auto position = m_inboxes.find(currentTid);

	InboxQueue* inbox = position == m_inboxes.end() 
		       ? nullptr
		       : position->second;

	if (inbox == nullptr)
	{
		inbox = new InboxQueue(this);
		m_inboxes[currentTid] =  inbox;
	}

	return inbox;
}

Pakal::InboxQueue* Pakal::EventScheduler::getAnInboxForThisThread()
{
	auto currentTid = Poco::Thread::currentTid();
	return InboxForThread(currentTid);
}

void Pakal::EventScheduler::registerDispatcher(AsyncTaskDispatcher* dispatcher)
{
	dispatcher->setScheduler(this);
}