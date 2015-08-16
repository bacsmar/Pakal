#pragma once

#include "Config.h"

#include <functional>
#include <thread>

#include "DoubleBufferedList.h"
#include "BasicTask.h"

namespace Pakal
{
	class EventScheduler;
	class BasicTask;


	class
		_PAKALExport InboxQueue
	{
		friend class EventScheduler;	
		friend class AsyncTaskDispatcher;

	public: 
		typedef std::queue<BasicTaskPtr,std::list<BasicTaskPtr>> TaskQueue;		
	private:		
		EventScheduler*			m_scheduler;
		DoubleBufferedQueue<BasicTaskPtr, std::list<BasicTaskPtr> > m_inboxStore;
		std::thread::id m_tid;

		explicit InboxQueue(EventScheduler* dispatcher, std::thread::id tid);
		

		template<class TOut>
		std::shared_ptr<Task<TOut>> pushTask(const std::function<TOut(void)> & jobDelegate)
		{
			ASSERT(this);

			auto task = std::make_shared<Task<TOut>>(jobDelegate, m_scheduler);

			m_inboxStore.push(task);
			return task;			
		}		

		BasicTaskPtr pushTask(const std::function<void()>& jobDelegate);	

		inline BasicTaskPtr popTask()
		{
			BasicTaskPtr task = m_inboxStore.front();
			m_inboxStore.pop();
			return task;
		}

		inline TaskQueue& popAllTasks()
		{
			return m_inboxStore.getListToProcess();
		}

		inline bool empty()
		{
			return m_inboxStore.empty();
		}

		inline int size()
		{
			return m_inboxStore.size();
		}
		
		inline std::thread::id getTid();

	};
}