#pragma once

#include "Config.h"

#include <functional>
#include "DoubleBufferedList.h"
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
		DoubleBufferedQueue<BasicTaskPtr, std::list<BasicTaskPtr> > m_inboxStore;

		//typedef std::queue<BasicTaskPtr> TaskQueue;
		//DoubleBufferedQueue<BasicTaskPtr> m_inboxStore;

		explicit InboxQueue(EventScheduler* dispatcher);

	public:

		template<class TOut>
		std::shared_ptr<Task<TOut>> pushTask( std::function<TOut(void)> & jobDelegate)
		{
			Task<TOut> * tPtr = new Task<TOut>( jobDelegate, m_scheduler);
			std::shared_ptr< Task<TOut> > task(tPtr);

			m_inboxStore.push(task);
			return task;			
		}		

		BasicTaskPtr pushTask(std::function<void()>& jobDelegate);	

		inline BasicTaskPtr InboxQueue::popTask()
		{
			BasicTaskPtr task = m_inboxStore.front();
			m_inboxStore.pop();
			return task;
		}

		inline InboxQueue::TaskQueue& InboxQueue::popAllTasks()
		{
			return m_inboxStore.getListToProcess();
		}

		inline bool InboxQueue::empty()
		{
			return m_inboxStore.empty();
		}

		inline int InboxQueue::size()
		{
			return m_inboxStore.size();
		}
	};
}