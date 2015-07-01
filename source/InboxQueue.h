#pragma once

#include "Config.h"

#include <functional>
#include <Poco/NotificationQueue.h>

#include "BasicTask.h"

namespace Pakal
{

	class EventScheduler;
	class BasicTask;
	template <class T>
	class Task;


	class _PAKALExport InboxQueue
	{
		friend class EventScheduler;

	private:
		Poco::NotificationQueue m_inboxStore;
		EventScheduler*			m_scheduler;

		explicit InboxQueue(EventScheduler* dispatcher);

	public:
		
		template<class TOut>
		Poco::AutoPtr< Task<TOut> > pushTask( std::function<TOut(void)> & jobDelegate)
		{
			Poco::AutoPtr< Task<TOut> > task = new Task<TOut>( jobDelegate, m_scheduler);

			m_inboxStore.enqueueNotification(task);
			return task;
		}		

		inline BasicTaskPtr popTask();
		inline BasicTaskPtr waitPopTask();

		inline bool empty();

		inline int size();
	};

}