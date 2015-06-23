#pragma once

#include "Config.h"

#include <functional>

#include <Poco/Condition.h>
#include <Poco/Mutex.h>
#include <Poco/AutoPtr.h>

#include "BasicTask.h"
#include "Event.h"

namespace Pakal
{
	class EventScheduler;
	
	template<class TArgs>
	class _PAKALExport Task : public BasicTask
	{
		friend class InboxQueue;

		typedef std::function<TArgs(void)> FunctionDelegate;
		typedef std::function<void(TArgs)> MethodDelegate;

	private:
		Task(FunctionDelegate& job, EventScheduler* scheduler)
		{
			mJob = job;
			Completed.connectWithScheduler(scheduler);			
		}

		FunctionDelegate		mJob;
		TArgs					mResult;
		Event<TArgs>			Completed;
		bool					m_isCompleted;

		Poco::Condition		  m_Completed;
		Poco::Mutex			  m_CompletedMutex;

	protected:
		void run() override
		{
			ASSERT(m_isCompleted);

			mResult = mJob();
			m_isCompleted = true;
			m_Completed.broadcast();

			Completed.notify(mResult);
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
			m_Completed.wait(m_CompletedMutex);
		}

		void OnCompletionDo(MethodDelegate& callBack)
		{
			if (m_isCompleted)
				callBack(mResult);
			else
				Completed.add(callBack);			
		}

	};


}
