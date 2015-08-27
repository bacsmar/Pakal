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
		typedef DoubleBufferedQueue<BasicTaskPtr,std::list<BasicTaskPtr>> InboxContainer;		
	private:

		std::thread::id m_tid;
		InboxContainer m_inbox;

		explicit InboxQueue(std::thread::id tid) : m_tid(tid) {};

		template<class TOut>
		std::shared_ptr<Task<TOut>> push_task(const std::function<TOut(void)> & jobDelegate)
		{
			ASSERT(this);

			auto task = std::make_shared<Task<TOut>>(jobDelegate);

			m_inbox.push(task);
			return task;			
		}

		template<class TOut>
		void push_task(std::shared_ptr<Task<TOut>> task)
		{
			ASSERT(this);
			m_inbox.push(task);
		}

		BasicTaskPtr push_task(const std::function<void()>& jobDelegate);	
		void push_task(BasicTaskPtr task);	

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
		
		inline std::thread::id get_tid()
		{
			return m_tid;
		};

	};
}