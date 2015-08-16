#pragma once

#include "Config.h"
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <functional>

#include <TaskFwd.h>
#include "Task.h"
#include "InboxQueue.h"
#include "IManager.h"

namespace Pakal
{

	class InboxQueue;
	class AsyncTaskDispatcher;


	class _PAKALExport EventScheduler : IManager
	{
		friend AsyncTaskDispatcher;

	public:
		void initialize() override;
		void terminate() override;
	private:
		template <class T> friend class Event;

		std::unordered_map<std::thread::id, InboxQueue*> m_inboxes;
		std::unordered_set<AsyncTaskDispatcher*> m_dispatchers;
		bool m_initialized;


		InboxQueue* find_inbox_for_thread(std::thread::id tid);	
		InboxQueue*	inbox_for_this_thread();

	public:
		EventScheduler() : m_initialized(false) {}
		virtual			~EventScheduler();


		BasicTaskPtr	execute_in_thread(const std::function<void()>& fn, std::thread::id tid);

		void register_dispatcher(AsyncTaskDispatcher* dispatcher);
		void deregister_dispatcher(AsyncTaskDispatcher* dispatcher);

		template<typename TArgs>
		std::shared_ptr<Task<TArgs>> execute_in_thread(const std::function<TArgs()>& fn, std::thread::id tid)
		{
			ASSERT(m_initialized);
			auto currentTid = std::this_thread::get_id();

			if (currentTid == tid)
				return TaskUtils::from_result(fn());
			else
				return find_inbox_for_thread(tid)->push_task(fn);			
		}

	};
}
