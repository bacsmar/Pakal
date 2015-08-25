#include "EventScheduler.h"

#include "InboxQueue.h"
#include "AsyncTaskDispatcher.h"


Pakal::EventScheduler::~EventScheduler()
{
	ASSERT(!m_initialized);
}

void Pakal::EventScheduler::initialize()
{
	ASSERT(!m_initialized);
	m_initialized = true;
}

void Pakal::EventScheduler::terminate()
{
	ASSERT(m_initialized);

	m_initialized = false;
	for(auto& item : m_inboxes)
	{
		delete item.second;
	}

	for(auto dispatcher : m_dispatchers)
	{
		dispatcher->m_inbox = nullptr;
		dispatcher->m_scheduler = nullptr;
	}

	m_dispatchers.clear();
	m_inboxes.clear();
}

Pakal::InboxQueue* Pakal::EventScheduler::find_inbox_for_thread(std::thread::id currentTid)
{	
	auto position = m_inboxes.find(currentTid);

	auto inbox = position == m_inboxes.end() 
		       ? nullptr
		       : position->second;	
	return inbox;
}

Pakal::InboxQueue* Pakal::EventScheduler::inbox_for_this_thread()
{
	ASSERT(m_initialized);

	auto currentTid = std::this_thread::get_id();

	auto inbox = find_inbox_for_thread(currentTid);
		
	if (inbox == nullptr)
	{
		inbox = new InboxQueue(this,currentTid);
		m_inboxes[currentTid] =  inbox;
	}
	return inbox;
}

Pakal::BasicTaskPtr Pakal::EventScheduler::execute_in_thread(const std::function<void()>& fn,std::thread::id tid)
{
	ASSERT(m_initialized);

	auto currentTid = std::this_thread::get_id();

	if (currentTid == tid)
	{
		fn();
		return TaskUtils::completed_task(this);
	}

	return find_inbox_for_thread(tid)->push_task(fn);
}

void Pakal::EventScheduler::register_dispatcher(AsyncTaskDispatcher* dispatcher)
{
	ASSERT(m_initialized);
	ASSERT(m_dispatchers.find(dispatcher) == m_dispatchers.end());

	dispatcher->m_scheduler = this;
			
	m_dispatchers.insert(dispatcher);
}

void Pakal::EventScheduler::deregister_dispatcher(AsyncTaskDispatcher* dispatcher)
{
	ASSERT(m_initialized);
	ASSERT(dispatcher->m_inbox->size() == 0);

	m_dispatchers.erase(dispatcher);

	dispatcher->m_inbox = nullptr;
	dispatcher->m_scheduler = nullptr;

}