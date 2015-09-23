#pragma once

#include "Config.h"
#include <thread>
#include "InboxQueue.h"


namespace Pakal 
{
	class InboxTask;
	class EventScheduler;

	class _PAKALExport 
		AsyncTaskDispatcher final
	{
		friend class EventScheduler;

		InboxTask* m_inbox;

	public:
		~AsyncTaskDispatcher() {}
		AsyncTaskDispatcher() : m_inbox(nullptr) {}

		inline std::thread::id thread_id()
		{
			return m_inbox->get_tid();
		}

		void dispatch_tasks(bool block);
	};

}