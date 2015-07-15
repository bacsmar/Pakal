#pragma once

#include "Config.h"
#include "Poco/Thread.h"
#include <unordered_map>
#include <functional>
#include "Task.h"
#include "InboxQueue.h"


namespace Pakal
{

	class InboxQueue;
	class AsyncTaskDispatcher;


	class _PAKALExport EventScheduler
	{
		template <class T> friend class Event;

		std::unordered_map<Poco::Thread::TID,InboxQueue*> m_inboxes;

		InboxQueue* findInboxForThread(Poco::Thread::TID tid);		

	public:
		virtual ~EventScheduler();
		void registerDispatcher(AsyncTaskDispatcher* dispatcher);
		InboxQueue* InboxForThisThread();
		BasicTaskPtr executeInThread(const std::function<void()>& fn,Poco::Thread::TID tid);
		template<typename TArgs>
		std::shared_ptr<Task<TArgs>> executeInThread(const std::function<TArgs()>& fn,Poco::Thread::TID tid)
		{
			auto currentTid = Poco::Thread::currentTid();

			if (currentTid == tid)
				return TaskUtils::fromResult(fn());
			else
				return findInboxForThread(tid)->pushTask(fn);			
		}

	};
}
