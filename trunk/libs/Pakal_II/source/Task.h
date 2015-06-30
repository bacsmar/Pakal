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
	class _PAKALExport Task : public BasicTask
	{
		friend class InboxQueue;
		friend class TaskUtils;

		typedef std::function<TArgs(void)> FunctionDelegate;
		typedef std::function<void(TArgs)> MethodDelegate;
		typedef Poco::AutoPtr<Task<TArgs>> TaskPtr;

	private:


		Task(const FunctionDelegate& job, EventScheduler* scheduler)
		{
			this->duplicate();
			mJob = job;
			m_isCompleted = false;
			Completed.connectWithScheduler(scheduler);
		}

		Task(const TArgs& result)
		{
			mResult = result;
			m_isCompleted = true;
		}

		FunctionDelegate		mJob;
		TArgs					mResult;
		Event<TArgs>			Completed;
		volatile bool			m_isCompleted;


	protected:
		void run() override
		{
			ASSERT(m_isCompleted == false);

			mResult = mJob();
			m_isCompleted = true;
			
			Completed.notify(mResult);

			this->release();
		}

	public:
		TArgs Result()
		{
			wait();
			return mResult;
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

		void OnCompletionDo(MethodDelegate& callBack)
		{
			if (m_isCompleted)
				callBack(mResult);
			else
				Completed.add(callBack);			
		}

	};

	
	class TaskUtils
	{
	public:

		template<class T>
		static Poco::AutoPtr<Task<std::atomic_int>> whenAll(std::vector< Poco::AutoPtr< Task<T>>>& tasks, EventScheduler* scheduler)
		{
			static Task<std::atomic_int>::FunctionDelegate emptyDelegate = []()
			{
				std::atomic_int n;
				n.store(0);
				return n;
			};

			Task<std::atomic_int>* task  = new Task<std::atomic_int>(emptyDelegate,scheduler);
			task->mResult = tasks.size();

			std::function<void(T)> onC = [task](T args)
			{
				--task->mResult;
				if(task->mResult == 0)
				{
					task->run();
				}
			};

			for(auto& t : tasks)
			{
				t->OnCompletionDo(onC);
			}

			return task;
		}


		template<class T>
		static void waitAll(std::vector<Poco::AutoPtr< Task<T> >>& tasks)
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
