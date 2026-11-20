#include "EventScheduler.h"
#include "SingletonHolder.h"
#include "InboxQueue.h"
#include "AsyncTaskDispatcher.h"
#include "LogMgr.h"
#include <algorithm>
#include <sstream>


Pakal::EventScheduler& Pakal::EventScheduler::instance() 
{
	static SingletonHolder<EventScheduler> sh;
	return *sh.get();
}

Pakal::EventScheduler::~EventScheduler()
{
	mutex_guard lock(m_mutex);

	// the dispatchers still registered first, while their inboxes exist, then the inboxes
	for(auto dispatcher : m_dispatchers)
	{
		ASSERT(dispatcher->m_inbox->size() == 0);
		dispatcher->m_inbox = nullptr;
	}

	for(auto& item : m_inboxes)
		delete item.second;

	m_dispatchers.clear();
	m_inboxes.clear();	
}

void Pakal::EventScheduler::report_no_inbox(std::thread::id tid)
{
	std::ostringstream ss;
	ss << tid;
	LOG_ERROR("[EventScheduler] a task for thread %s was dropped: that thread has no dispatcher (its system ended?)", ss.str().c_str());
}

void Pakal::EventScheduler::wait_this_thread(const std::function<bool()>& condition, bool blocking)
{
	// just wait for other thread to trigger our condition (giving the core away meanwhile, not spinning on it)
	if( blocking)
	{
		while (!condition()) std::this_thread::yield();
	}		
	else// the condition could be in this thread.. so dispatch tasks...
	{
		if (condition())
			return;

		auto currentTid = THIS_THREAD;
		AsyncTaskDispatcher* dispatcher = nullptr;
		{
			mutex_guard lock(m_mutex);
			auto found = std::find_if(m_dispatchers.begin(), m_dispatchers.end(), [currentTid](AsyncTaskDispatcher* d)
			{
				return d->thread_id() == currentTid;
			});
			if (found != m_dispatchers.end()) dispatcher = *found;
		}

		while (!condition())
		{
			if (dispatcher) dispatcher->dispatch_one_task(false);
			std::this_thread::yield();
		}
	}
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

	{
		mutex_guard lock(m_mutex);
		if (InboxTask* inbox = find_inbox_for_thread(tid))
			return inbox->push_task(fn);
	}
	report_no_inbox(tid);
	return TaskUtils::completed_task();
}

void Pakal::EventScheduler::execute_in_thread(BasicTaskPtr task, std::thread::id tid)
{
	auto currentTid = THIS_THREAD;

	if (currentTid == tid)
	{
		task->run();
		return;
	}
	{
		mutex_guard lock(m_mutex);
		if (InboxTask* inbox = find_inbox_for_thread(tid))
		{
			inbox->push_task(task);
			return;
		}
	}
	report_no_inbox(tid);
}

void Pakal::EventScheduler::register_dispatcher_for_thread(AsyncTaskDispatcher* dispatcher, std::thread::id tid)
{
	mutex_guard lock(m_mutex);

	ASSERT(m_dispatchers.find(dispatcher) == m_dispatchers.end());

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