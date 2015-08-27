#include "EventScheduler.h"
#include "SingletonHolder.h"
#include "InboxQueue.h"
#include "AsyncTaskDispatcher.h"


Pakal::EventScheduler& Pakal::EventScheduler::instance() 
{
	static SingletonHolder<EventScheduler> sh;
	return *sh.get();
}

Pakal::EventScheduler::~EventScheduler()
{
	for(auto& item : m_inboxes)
		delete item.second;

	for(auto dispatcher : m_dispatchers)
	{
		ASSERT(dispatcher->m_inbox->size() == 0);
		dispatcher->m_inbox = nullptr;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	m_dispatchers.clear();
	m_inboxes.clear();	
}

Pakal::InboxQueue* Pakal::EventScheduler::find_inbox_for_thread(std::thread::id tid)
{	
	auto position = m_inboxes.find(tid);

	auto inbox = position == m_inboxes.end() 
		       ? nullptr
		       : position->second;	
	return inbox;
}

Pakal::BasicTaskPtr Pakal::EventScheduler::execute_in_thread(const std::function<void()>& fn,std::thread::id tid)
{
	auto currentTid = std::this_thread::get_id();

	if (currentTid == tid)
	{
		fn();
		return TaskUtils::completed_task();
	}

	return find_inbox_for_thread(tid)->push_task(fn);
}

void Pakal::EventScheduler::execute_in_thread(BasicTaskPtr task, std::thread::id tid)
{
	auto currentTid = std::this_thread::get_id();

	if (currentTid == tid)
		task->run();
	else
		find_inbox_for_thread(tid)->push_task(task);
}

void Pakal::EventScheduler::register_dispatcher_for_thread(AsyncTaskDispatcher* dispatcher, std::thread::id tid)
{
	ASSERT(m_dispatchers.find(dispatcher) == m_dispatchers.end());

	std::lock_guard<std::mutex> lock(m_mutex);

	auto inbox = find_inbox_for_thread(tid);

	if (inbox == nullptr)
	{
		inbox = new InboxQueue(tid);
		m_inboxes[tid] = inbox;
	}

	dispatcher->m_inbox = inbox;
	m_dispatchers.insert(dispatcher);
}

void Pakal::EventScheduler::deregister_dispatcher(AsyncTaskDispatcher* dispatcher)
{
	ASSERT(dispatcher->m_inbox->size() == 0);

	std::lock_guard<std::mutex> lock(m_mutex);

	m_dispatchers.erase(dispatcher);

	auto threadId = dispatcher->thread_id();

	dispatcher->m_inbox = nullptr;

	bool orphanInbox = 
		std::find_if(m_dispatchers.begin(),m_dispatchers.end(),[threadId](AsyncTaskDispatcher* d) { return d->thread_id() == threadId; }) == m_dispatchers.end();

	if (orphanInbox)
	{
		m_inboxes.erase(threadId);
	}

}