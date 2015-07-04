#pragma once

#include "Config.h"

#include <functional>
#include <vector>
#include <atomic>

#include <Poco/Notification.h>

#include "BasicTask.h"
#include "Event.h"

namespace Pakal
{
	class EventScheduler;

	class TaskBridge : public Poco::Notification, public BasicTask
	{
	public:		
		virtual ~TaskBridge(){}
	};

	template<class TArgs>
	class _PAKALExport Task final : public TaskBridge
	{
		friend class InboxQueue;
		friend class TaskUtils;

		typedef std::function<TArgs(void)> FunctionDelegate;
		typedef std::function<void(TArgs)> MethodDelegate;

	private:

		Task(const FunctionDelegate& job, EventScheduler* scheduler) 
		{				
			this->duplicate();
			m_Job = job;
			m_isCompleted = false;
			m_EventCompleted.connectWithScheduler(scheduler);			
		}

		Task(const TArgs& result) : BasicTask(this)
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

			this->release();
		}

	public:
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
			
			while(!m_isCompleted) Poco::Thread::sleep(1);
		}

		// TODO: ponerlo en BasicTask
		inline void OnCompletionDo(MethodDelegate& callBack)
		{
			if (m_isCompleted)
				callBack(m_Result);
			else
				m_EventCompleted.addListener(callBack);			
		}		

		virtual void onCompletionDo( IDelegate * delegate ) 
		{			
			MethodDelegate _method;
			if( delegate->getType() == BasicTask::IDelegate::DELEGATE_ARGS )
			{
				Delegate<void,TArgs> *d = static_cast<Delegate<void,TArgs>*>( delegate );
				_method = d->f;
				OnCompletionDo(_method);
			}
			else if( delegate->getType() == BasicTask::IDelegate::DELEGATE_NOARGS_NOPARAM )
			{
				DelegateNoArgsNoParam *d = static_cast<DelegateNoArgsNoParam*>( delegate );
				onCompletionDo(d->f);
			}
			else if( delegate->getType() == BasicTask::IDelegate::DELEGATE_NOARGS )
			{
				DelegateNoArgs<TArgs> *d = static_cast<DelegateNoArgs<TArgs>*>( delegate );
				// "this Function only Supports MethodDelegate ->  void(TArgs) & void()");
			}
		}

		virtual void onCompletionDo( std::function<void()> & callback ) override
		{
			if (m_isCompleted)
				callback();
			else
			{

				Event<TArgs>::MethodDelegate callbackBridge = [callback](TArgs)
				{	
					callback();
				};

				m_EventCompleted.addListener(callbackBridge);
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
		static BasicTaskPtr whenAll(std::vector< BasicTaskPtr >& tasks)
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


			std::function<void()> onComplete = [task]()
			{
				--task->m_Result;
				if(task->m_Result == 0)
				{
					task->run();
				}
			};			

			for(auto& t : tasks)
			{				
				t->onCompletionDo(onComplete);
			}

			return BasicTaskPtr(task);
		}		
		
		static void waitAll(std::vector<BasicTaskPtr>& tasks)
		{
			for(auto & t : tasks)
			{
				t->wait();
			}
		}

		template<class T>
		static Poco::AutoPtr<Task<T>> fromResult(T& result)
		{
			return new Task<T>(result);
		}



	};
	
}
