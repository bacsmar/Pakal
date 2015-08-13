#pragma once

#include "Config.h"
#include <thread>
#include <unordered_map>
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
		template <class T> friend class Event;

		std::unordered_map<std::thread::id, InboxQueue*> m_inboxes;

		InboxQueue* findInboxForThread(std::thread::id tid);		

	public:
		virtual			~EventScheduler();

		void			registerDispatcher(AsyncTaskDispatcher* dispatcher);

		InboxQueue*		InboxForThisThread();

		BasicTaskPtr	executeInThread(const std::function<void()>& fn, std::thread::id tid);

		template<typename TArgs>
		std::shared_ptr<Task<TArgs>> executeInThread(const std::function<TArgs()>& fn, std::thread::id tid)
		{
			auto currentTid = std::this_thread::get_id();

			if (currentTid == tid)
				return TaskUtils::fromResult(fn());
			else
				return findInboxForThread(tid)->pushTask(fn);			
		}

	};
}
