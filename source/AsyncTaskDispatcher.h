#pragma once

#include "Config.h"
#include <thread>
#include "InboxQueue.h"


namespace Pakal 
{
	class InboxQueue;
	class EventScheduler;

	class _PAKALExport AsyncTaskDispatcher final
	{
		friend class EventScheduler;

		InboxQueue* m_inbox;

	public:
		~AsyncTaskDispatcher() {}
		AsyncTaskDispatcher() : m_inbox(nullptr) {}

		inline std::thread::id thread_id()
		{
			return m_inbox->get_tid();
		}

		void dispatch_tasks();
	};

}