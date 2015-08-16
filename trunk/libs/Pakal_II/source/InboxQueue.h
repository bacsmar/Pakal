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
		DoubleBufferedQueue<BasicTaskPtr, std::list<BasicTaskPtr> > m_inbox;
		std::thread::id m_tid;

		explicit InboxQueue(EventScheduler* dispatcher, std::thread::id tid);
		

		template<class TOut>
		std::shared_ptr<Task<TOut>> push_task(const std::function<TOut(void)> & jobDelegate)
		{
			ASSERT(this);

			auto task = std::make_shared<Task<TOut>>(jobDelegate, m_scheduler);

			m_inbox.push(task);
			return task;			
		}		

		BasicTaskPtr push_task(const std::function<void()>& jobDelegate);	

		inline BasicTaskPtr pop_task()
		{
			BasicTaskPtr task = m_inbox.front();
			m_inbox.pop();
			return task;
		}

		inline TaskQueue& pop_all_tasks()
		{
			return m_inbox.getListToProcess();
		}

		inline bool empty()
		{
			return m_inbox.empty();
		}

		inline int size()
		{
			return m_inbox.size();
		}
		
		inline std::thread::id get_tid();

	};
}