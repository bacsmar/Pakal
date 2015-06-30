#pragma once

#include "Config.h"

namespace Pakal 
{
	class InboxQueue;

	class _PAKALExport AsyncTaskDispatcher
	{
	private:
		InboxQueue* m_inbox;

	public:
		virtual ~AsyncTaskDispatcher() {}
		AsyncTaskDispatcher() : m_inbox(nullptr) {}

		inline InboxQueue* getInbox()
		{
			return m_inbox;
		};
		inline void setInbox(InboxQueue* inbox)
		{
			m_inbox = inbox;
		};

		void dispatchTasks();
	};

}