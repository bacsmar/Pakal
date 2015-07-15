#pragma once

#include "Config.h"

#include <functional>
#include <vector>
#include <atomic>

#include "BasicTask.h"
#include "Event.h"

namespace Pakal
{
	class EventScheduler;

	template<class TArgs>
	class _PAKALExport Task final : public BasicTask
	{
		friend class InboxQueue;
		friend class TaskUtils;	

		template <class T> friend class std::shared_ptr;

		typedef std::function<TArgs(void)> FunctionDelegate;
		typedef std::function<void(TArgs)> MethodDelegate;
				
	private:

		Task(const FunctionDelegate& job, EventScheduler* scheduler)
		{
			m_Job = job;
			m_isCompleted = false;
			m_EventCompleted.connectWithScheduler(scheduler);		
		}

		Task(const TArgs& result) 
		{
			m_Result = result;
			m_isCompleted = true;			
		}

		FunctionDelegate		m_Job;
		TArgs					m_Result;
		Event<TArgs>			m_EventCompleted;
		volatile bool			m_isCompleted;

	protected:
		void run() override
		{
			ASSERT(m_isCompleted == false);

			m_Result = m_Job();			
			m_isCompleted = true;
			m_EventCompleted.notify(m_Result);
		}

	public:
		~Task()
		{
		}		

		TArgs Result()
		{
			wait();
			return m_Result;
		}

		bool isCompleted() override
		{
			return m_isCompleted;
		}

		void wait() override
		{
			if (m_isCompleted)
				return;
			
			while(!m_isCompleted) std::this_thread::sleep_for( std::chrono::milliseconds(1) );
		}
		
		/// pass 0 if you want it to be executed in the caller thread
		void onCompletionDo(const MethodDelegate &callBack, std::thread::id callBackThread = std::this_thread::get_id())
		{
			if (m_isCompleted)
				callBack(m_Result);
			else
				m_EventCompleted.addListener(callBack,callBackThread);
		}

		/// pass 0 if you want it to be executed in the caller thread
		void onCompletionDo(const std::function<void()>  &callback, std::thread::id callBackThread = std::this_thread::get_id()  ) override
		{
			if (m_isCompleted)
				callback();
			else
			{
				MethodDelegate callbackBridge = [callback](TArgs) { callback(); };
				m_EventCompleted.addListener(callbackBridge,callBackThread);
			}
		}

		EventScheduler* getEventScheduler() override
		{
			return m_EventCompleted.getEventScheduler();
		}

	};

	
	class TaskUtils
	{
	public:
		static BasicTaskPtr whenAll(const std::vector< BasicTaskPtr >& tasks)
		{
			ASSERT(tasks.empty() == false);
			EventScheduler* scheduler = tasks.at(0)->getEventScheduler();

			static Task<std::atomic_int>::FunctionDelegate emptyDelegate = []()
			{
				std::atomic_int n;
				n.store(0);
				return n;
			};

			
			Task<std::atomic_int>* task  = new Task<std::atomic_int>(emptyDelegate, scheduler);
			task->m_Result = tasks.size();

			BasicTaskPtr myTask(task);			
			
			/*
			std::shared_ptr< Task<std::atomic_int> > myTask = std::make_shared< Task<std::atomic_int> > (emptyDelegate, scheduler) ;
			myTask->m_Result = tasks.size();
			*/
			std::function<void()> onComplete = [myTask]()
			{
				Task<std::atomic_int>* t = static_cast<Task<std::atomic_int>*> (myTask.get());
				--t->m_Result;
				if(t->m_Result == 0)
				{
					t->run();
				}
			};

			for(auto& t : tasks)
			{
				t->onCompletionDo(onComplete, NULL_THREAD );
			}
			return myTask;
		}		
		
		static void waitAll(const std::vector<BasicTaskPtr>& tasks)
		{
			for(auto & t : tasks)
			{
				t->wait();
			}
		}

		template<class T>
		static std::shared_ptr<Task<T>> fromResult(const T& result)
		{
			return  std::shared_ptr<Task<T>>(new Task<T>(result));
		}
	};
	
}
