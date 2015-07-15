#pragma once
#include "Config.h"

#include <functional>
#include <atomic>
#include <vector>

#include "BasicTask.h"
#include "Event.h"

namespace Pakal
{
	class EventScheduler;

	template<class TArgs>
	class _PAKALExport Task final : public BasicTask
	{
		static_assert(!std::is_void<TArgs>::value, "void type is not allowed");
		
		friend class InboxQueue;
		friend class TaskUtils;

	public:

		Task(const std::function<TArgs(void)>& job, EventScheduler* scheduler) : m_Job(job)
		{
			m_isCompleted = false;
			m_EventCompleted.connectWithScheduler(scheduler);
			m_EventCompletedWithResult.connectWithScheduler(scheduler);
		}

		Task(const TArgs& result) : m_Result(result)
		{
			m_isCompleted = true;
		}

		~Task()
		{
		}		


	private:

		std::function<TArgs(void)>		m_Job;
		TArgs							m_Result;
		Event<TArgs>					m_EventCompletedWithResult;

	protected:
		inline void run() override
		{
			ASSERT(m_isCompleted == false);

			m_Result = m_Job();			
			m_isCompleted = true;
			m_EventCompletedWithResult.notify(m_Result);
			m_EventCompleted.notify();
		}

	public:

		inline TArgs Result()
		{
			wait();
			return m_Result;
		}
		
		void onCompletionDo(const std::function<void(TArgs)>& callBack, std::thread::id callBackThread = std::this_thread::get_id())
		{
			if (m_isCompleted)
				callBack(m_Result);
			else
				m_EventCompletedWithResult.addListener(callBack,callBackThread);
		}
	};




	class TaskUtils
	{
	public:
		static BasicTaskPtr whenAll(const std::vector<BasicTaskPtr>& tasks)
		{
			ASSERT(tasks.empty() == false);
			EventScheduler* scheduler = tasks.at(0)->getEventScheduler();

			static auto emptyDelegate = []()
				{
					std::atomic_int n;
					n.store(0);
					return n;
				};

			auto task = std::make_shared<Task<std::atomic_int>>(emptyDelegate, scheduler);
			task->m_Result = tasks.size();

			BasicTaskPtr myTask(task);

			std::function<void()> onComplete = [myTask]()
				{
					Task<std::atomic_int>* t = static_cast<Task<std::atomic_int>*>(myTask.get());
					--t->m_Result;
					if (t->m_Result == 0)
					{
						t->run();
					}
				};

			for (auto& t : tasks)
			{
				t->onCompletionDo(onComplete, NULL_THREAD);
			}
			return myTask;
		}

		static void waitAll(const std::vector<BasicTaskPtr>& tasks)
		{
			for (auto& t : tasks)
			{
				t->wait();
			}
		}

		static BasicTaskPtr completedTask()
		{
			return std::make_shared<BasicTask>();
		};

		template <class T>
		static std::shared_ptr<Task<T>> fromResult(const T& result)
		{
			return std::make_shared<Task<T>>(result);
		}
	};
	

}
