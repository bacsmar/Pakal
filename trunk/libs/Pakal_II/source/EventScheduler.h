#pragma once

#include "Config.h"
#include "Poco/Thread.h"
#include <map>



namespace Pakal
{

	class InboxQueue;
	class AsyncTaskDispatcher;


	class _PAKALExport EventScheduler
	{
		template <class T> friend class Event;

		std::map<Poco::Thread::TID,InboxQueue*> m_inboxes;

		InboxQueue* InboxForThread(Poco::Thread::TID tid);

	public:
		~EventScheduler();
		void registerDispatcher(AsyncTaskDispatcher* dispatcher);

	};












}