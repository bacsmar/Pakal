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
		EventScheduler* m_scheduler;

	public:
		~AsyncTaskDispatcher() {}
		AsyncTaskDispatcher() : m_inbox(nullptr), m_scheduler(nullptr) {}

		inline std::thread::id thread_id()
		{
			ASSERT(m_inbox);
			return m_inbox->get_tid();
		}

		inline EventScheduler* get_scheduler() { return m_scheduler; }

		void dispatch_tasks();
	};

}