#pragma once

#include "Config.h"
#include <thread>

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

		inline std::thread::id thread_id();

		inline EventScheduler* get_scheduler();

		void dispatch_tasks();
	};

}