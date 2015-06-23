#pragma once

#include "Config.h"

#include <functional>


#include <Poco/NotificationQueue.h>
#include "Task.h"


namespace Pakal
{

	class EventScheduler;


	class _PAKALExport InboxQueue
	{
		friend class EventScheduler;

	private:
		Poco::NotificationQueue m_inboxStore;
		EventScheduler*			m_scheduler;

		explicit InboxQueue(EventScheduler* dispatcher);

	public:

		template<class TOut>
		Poco::AutoPtr<Pakal::Task<TOut>> pushTask(std::function<TOut(void)>& delegate)
		{
			Poco::AutoPtr<Pakal::Task<TOut>> task = new Pakal::Task<TOut>(delegate,m_scheduler);

			m_inboxStore.enqueueNotification(task);
			return task;
		}

		inline Poco::AutoPtr<BasicTask> popTask();
		inline Poco::AutoPtr<BasicTask> waitPopTask();

		inline bool empty();

		inline int size();
	};

}