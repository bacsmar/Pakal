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

		~EventScheduler();

		std::map<Poco::Thread::TID,InboxQueue*> m_inboxes;

		typedef std::map<Poco::Thread::TID,InboxQueue*>::iterator Iterator;

		InboxQueue* InboxForThread(Poco::Thread::TID tid);

	public:
		void registerDispatcher(AsyncTaskDispatcher* dispatcher);

	};












}