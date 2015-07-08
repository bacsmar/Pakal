#pragma once

#include "Config.h"

#include <functional>

#include <queue>
#include "Task.h"

namespace Pakal
{
	class EventScheduler;

	//TODO doble cola y mutexes
	class
		_PAKALExport InboxQueue
	{
		friend class EventScheduler;

	private:
		std::queue<BasicTaskPtr> m_inboxStore;
		EventScheduler* m_scheduler;

		explicit InboxQueue(EventScheduler* dispatcher);

	public:

		template <class TOut>
		std::shared_ptr<Task<TOut>> pushTask(std::function<TOut(void)>& jobDelegate)
		{
			Task<TOut>* ptr = new Task<TOut>(jobDelegate, m_scheduler);

			std::shared_ptr<Task<TOut>> taskPtr(ptr);

			m_inboxStore.push(taskPtr);

			return taskPtr;
		}

		BasicTaskPtr pushTask(std::function<void()>& jobDelegate);	

		inline BasicTaskPtr popTask();

		inline bool empty();

		inline int size();
	};
}