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

Pakal::InboxQueue* Pakal::EventScheduler::InboxForThread(Poco::Thread::TID tid)
{
	auto currentTid = tid;
	auto position = m_inboxes.find(currentTid);

	return position == m_inboxes.end() 
		       ? nullptr
		       : position->second;
}

void Pakal::EventScheduler::registerDispatcher(AsyncTaskDispatcher* dispatcher)
{
	InboxQueue* inbox =  InboxForThread(Poco::Thread::currentTid());

	if (inbox == nullptr)
	{
		inbox = new InboxQueue(this);
		m_inboxes[Poco::Thread::currentTid()] =  inbox;
	}

	dispatcher->setInbox(inbox);
}