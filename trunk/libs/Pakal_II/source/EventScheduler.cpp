#include "EventScheduler.h"
#include "SingletonHolder.h"
#include "InboxQueue.h"
#include "AsyncTaskDispatcher.h"
#include <algorithm>


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

	mutex_guard lock(m_mutex);
	m_dispatchers.clear();
	m_inboxes.clear();	
}

void Pakal::EventScheduler::wait_this_thread(const std::function<bool()>& condition)
{
	if (condition()) 
		return;

	auto currentTid = THIS_THREAD;
	m_mutex.lock();
	auto dispatcher = std::find_if(m_dispatchers.begin(),m_dispatchers.end(),[currentTid](AsyncTaskDispatcher* d)
	{
		return d->thread_id() == currentTid;
	});
	m_mutex.unlock();

	if (dispatcher != m_dispatchers.end())
		while (!condition()) (*dispatcher)->dispatch_one_task(false);
	else
		while(!condition());
}

Pakal::InboxTask* Pakal::EventScheduler::find_inbox_for_thread(std::thread::id tid)
{	
	auto position = m_inboxes.find(tid);

	return position == m_inboxes.end() 
		       ? nullptr
		       : position->second;
}

Pakal::BasicTaskPtr Pakal::EventScheduler::execute_in_thread(const std::function<void()>& fn,std::thread::id tid)
{
	auto currentTid = THIS_THREAD;

	if (currentTid == tid)
	{
		fn();
		return TaskUtils::completed_task();
	}

	return find_inbox_for_thread(tid)->push_task(fn);
}

void Pakal::EventScheduler::execute_in_thread(BasicTaskPtr task, std::thread::id tid)
{
	auto currentTid = THIS_THREAD;

	if (currentTid == tid)
		task->run();
	else
		find_inbox_for_thread(tid)->push_task(task);
}

void Pakal::EventScheduler::register_dispatcher_for_thread(AsyncTaskDispatcher* dispatcher, std::thread::id tid)
{
	ASSERT(m_dispatchers.find(dispatcher) == m_dispatchers.end());

	mutex_guard lock(m_mutex);

	auto inbox = find_inbox_for_thread(tid);

	if (inbox == nullptr)
	{
		inbox = new InboxTask(tid);
		m_inboxes[tid] = inbox;
	}

	dispatcher->m_inbox = inbox;
	m_dispatchers.insert(dispatcher);
}

void Pakal::EventScheduler::deregister_dispatcher(AsyncTaskDispatcher* dispatcher)
{
	ASSERT(dispatcher->m_inbox->empty());
	dispatcher->m_inbox->swap_buffer();
	ASSERT(dispatcher->m_inbox->empty());

	mutex_guard lock(m_mutex);

	m_dispatchers.erase(dispatcher);

	auto threadId = dispatcher->thread_id();

	dispatcher->m_inbox = nullptr;

	bool orphanInbox = 
		std::find_if(m_dispatchers.begin(),m_dispatchers.end(),[threadId](AsyncTaskDispatcher* d) { return d->thread_id() == threadId; }) == m_dispatchers.end();

	if (orphanInbox)
	{
		delete m_inboxes[threadId];
		m_inboxes.erase(threadId);
	}

}

Pakal::BasicTaskPtr Pakal::EventScheduler::execute_in_worker(const std::function<void()>& fn)
{
	BasicTaskPtr task = std::make_shared<BasicTask>(fn);

	m_pool.execute(std::bind(&BasicTask::run,task));
	
	return task;
}