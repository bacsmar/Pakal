#pragma once

#include "Config.h"

namespace Pakal 
{
	class InboxQueue;
	class EventScheduler;

	class _PAKALExport AsyncTaskDispatcher
	{
	private:
		InboxQueue* m_inbox;
		EventScheduler* m_scheduler;

	public:
		virtual ~AsyncTaskDispatcher() {}
		AsyncTaskDispatcher() : m_inbox(nullptr), m_scheduler(nullptr) {}

		inline InboxQueue* getInbox()
		{
			return m_inbox;
		}

		inline void setScheduler(EventScheduler* scheduler)
		{
			m_scheduler = scheduler;
		}

		void dispatchTasks();
	};

}