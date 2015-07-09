#pragma once

#include "Config.h"

#include <functional>
#include <queue>
#include <list>
#include <mutex>

#include "BasicTask.h"

namespace Pakal
{
	class EventScheduler;
	class BasicTask;
	template <class T>
	class Task;
	
	class
		_PAKALExport InboxQueue
	{
		friend class EventScheduler;		
	public: 
		typedef std::queue<BasicTaskPtr,std::list<BasicTaskPtr>> TaskQueue;
	private:		
		EventScheduler*			m_scheduler;

		static const int MAX_INITIALIZATION_QUEUES = 2;		
		TaskQueue				m_inboxStores[MAX_INITIALIZATION_QUEUES];
		int						m_ActiveQueue;
		std::mutex				m_TaskQueueMutex;
		TaskQueue*				m_inboxStore;

		explicit InboxQueue(EventScheduler* dispatcher);

	public:
		
		template<class TOut>
		std::shared_ptr<Task<TOut>> pushTask( std::function<TOut(void)> & jobDelegate)
		{
			Task<TOut> * tPtr = new Task<TOut>( jobDelegate, m_scheduler);
			std::shared_ptr< Task<TOut> > task(tPtr);

			std::lock_guard<std::mutex> lock(m_TaskQueueMutex);
			m_inboxStore->push(task);
			return task;			
		}		

		BasicTaskPtr pushTask(std::function<void()>& jobDelegate);	

		inline BasicTaskPtr popTask();

		inline TaskQueue& popAllTasks();		

		inline bool empty();

		inline int size();
	};
}