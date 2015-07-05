#pragma once

#include "Config.h"
#include "Poco/Thread.h"
#include <unordered_map>



namespace Pakal
{

	class InboxQueue;
	class AsyncTaskDispatcher;


	class _PAKALExport EventScheduler
	{
		template <class T> friend class Event;

		std::unordered_map<Poco::Thread::TID,InboxQueue*> m_inboxes;

		InboxQueue* getInboxForThread(Poco::Thread::TID tid);		

	public:
		virtual ~EventScheduler();
		void registerDispatcher(AsyncTaskDispatcher* dispatcher);
		InboxQueue* getAnInboxForThisThread();

	};
}
