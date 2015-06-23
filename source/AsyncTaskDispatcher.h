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

		inline InboxQueue* getInbox()
		{
			return m_inbox;
		};
		inline void setInbox(InboxQueue* inbox)
		{
			m_inbox = inbox;
		};

		void dispatchTasks(bool waitForTasks = false);
	};

}