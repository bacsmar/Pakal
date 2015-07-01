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

Pakal::InboxQueue* Pakal::EventScheduler::getInboxForThread(Poco::Thread::TID currentTid)
{	
	auto position = m_inboxes.find(currentTid);

	InboxQueue* inbox = position == m_inboxes.end() 
		       ? nullptr
		       : position->second;	
	return inbox;
}

Pakal::InboxQueue* Pakal::EventScheduler::getAnInboxForThisThread()
{
	auto currentTid = Poco::Thread::currentTid();

	auto inbox = getInboxForThread(currentTid);
		
	if (inbox == nullptr)
	{
		inbox = new InboxQueue(this);
		m_inboxes[currentTid] =  inbox;
	}
	return inbox;
}

void Pakal::EventScheduler::registerDispatcher(AsyncTaskDispatcher* dispatcher)
{
	dispatcher->setScheduler(this);
}