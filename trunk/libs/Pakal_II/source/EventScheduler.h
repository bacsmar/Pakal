#pragma once

#include "Config.h"
#include <thread>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#include <TaskFwd.h>
#include "Task.h"
#include "InboxQueue.h"

namespace Pakal
{

	class InboxQueue;
	class AsyncTaskDispatcher;


	class _PAKALExport EventScheduler
	{
		template <class EventScheduler> friend class SingletonHolder;

		std::unordered_map<std::thread::id, InboxQueue*> m_inboxes;
		std::unordered_set<AsyncTaskDispatcher*> m_dispatchers;
		std::mutex m_mutex;

		InboxQueue* find_inbox_for_thread(std::thread::id tid);	

		EventScheduler()  {}
		virtual	~EventScheduler();

	public:
		static EventScheduler& instance();

		void wait_this_thread(const std::function<bool()>& condition);

		void register_dispatcher_for_thread(AsyncTaskDispatcher* dispatcher,std::thread::id tid);
		void deregister_dispatcher(AsyncTaskDispatcher* dispatcher);

		template<typename TArgs>
		TaskPtr<TArgs> execute_in_thread(const std::function<TArgs()>& fn, std::thread::id tid)
		{
			auto currentTid = THIS_THREAD;

			if (currentTid == tid)
				return TaskUtils::from_result(fn());

			return find_inbox_for_thread(tid)->push_task(fn);
		}
		BasicTaskPtr execute_in_thread(const std::function<void()>& fn, std::thread::id tid);

		template<typename TArgs>
		void execute_in_thread( TaskPtr<TArgs> task, std::thread::id tid)
		{
			auto currentTid = THIS_THREAD;

			if (currentTid == tid)
				task->run();
			else
				find_inbox_for_thread(tid)->push_task(task);
		}
		void execute_in_thread(BasicTaskPtr task, std::thread::id tid);
	};
}
