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

Pakal::InboxQueue* Pakal::EventScheduler::findInboxForThread(std::thread::id currentTid)
{	
	auto position = m_inboxes.find(currentTid);

	InboxQueue* inbox = position == m_inboxes.end() 
		       ? nullptr
		       : position->second;	
	return inbox;
}

Pakal::InboxQueue* Pakal::EventScheduler::InboxForThisThread()
{
	auto currentTid = std::this_thread::get_id();

	auto inbox = findInboxForThread(currentTid);
		
	if (inbox == nullptr)
	{
		inbox = new InboxQueue(this,currentTid);
		m_inboxes[currentTid] =  inbox;
	}
	return inbox;
}

Pakal::BasicTaskPtr Pakal::EventScheduler::executeInThread(const std::function<void()>& fn,std::thread::id tid)
{
	auto currentTid = std::this_thread::get_id();

	if (currentTid == tid)
	{
		fn();
		return TaskUtils::completedTask();
	}
	else
		return findInboxForThread(tid)->pushTask(fn);
}


void Pakal::EventScheduler::registerDispatcher(AsyncTaskDispatcher* dispatcher)
{
	dispatcher->setScheduler(this);
}