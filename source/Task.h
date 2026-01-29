#pragma once
#include "Config.h"

#include <functional>
#include <atomic>
#include <vector>

#include "BasicTask.h"
#include "EventSystemUtils.h"


namespace Pakal
{
	class EventScheduler;

	template<class TArgs>
	class _PAKALExport Task final : public BasicTask
	{
		static_assert(!std::is_void<TArgs>::value, "void type is not allowed");		
		template <class T> friend class TaskCompletionSource;

		TArgs						m_result;

	public:

		explicit Task(const std::function<TArgs(void)>& job) : BasicTask(nullptr)
		{
			m_job = [=]() { m_result = job(); };
		}
		explicit Task(const TArgs& result) : m_result(result) {}

		~Task() { }

		inline const TArgs& result()
		{
			wait();
			return m_result;
		}

		template <class TReturn>
		TaskPtr<TReturn> continue_with(const std::function<TReturn(TArgs)>& callBack, std::thread::id callBackThread = NULL_THREAD)
		{
			auto task = std::make_shared<Task<TReturn>>([=]() { return callBack(m_result);  });

			queue_continuation(ContinuationData(task, callBackThread));
			
			if (is_completed())
			{
				run_continuations();
			}

			return task;
		}

		BasicTaskPtr continue_with(const std::function<void(TArgs)>& callBack, std::thread::id callBackThread = NULL_THREAD)
		{
			auto task = std::make_shared<BasicTask>([=]() {  callBack(m_result);  });

			queue_continuation(ContinuationData(task, callBackThread));
			
			if (is_completed())
			{
				run_continuations();
			}

			return task;
		}
	};


	class TaskUtils
	{
	public:

		static BasicTaskPtr when_all(const std::vector<BasicTaskPtr>& tasks)
		{
			if (tasks.empty())
			{
				return completed_task();
			}			

			// dummy class only used to keep an atomic counter on it
			class DummyTaskAtomic : public BasicTask
			{		
				friend class TaskUtils;
			public:
				explicit DummyTaskAtomic(const std::function<void(void)>& job) : BasicTask(job){}
				inline void runTask() { run(); }
				std::atomic_int	m_result;				
			};

			static auto emptyDelegate = []() {};

			auto task = std::make_shared<DummyTaskAtomic>(emptyDelegate); // -> auto task = new DummyTaskAtomic(emptyDelegate);
			task->m_result = tasks.size();

			BasicTaskPtr myTask(task);

			std::function<void()> onComplete = [myTask]()
			{
				auto t = static_cast<DummyTaskAtomic*>(myTask.get());
				t->m_result-= 1;
				if (t->m_result == 0)
				{
					t->runTask();
				}
			};

			for (auto& t : tasks)
			{
				t->continue_with( onComplete, NULL_THREAD);
			}
			return myTask;
		}

		static void wait_all(const std::vector<BasicTaskPtr>& tasks)
		{
			for (auto& t : tasks)
			{
				t->wait();
			}
		}

		static BasicTaskPtr completed_task()
		{
			static BasicTaskPtr t = std::make_shared<BasicTask>();
			return t;
		};

		template <class T>
		static TaskPtr<T> from_result(const T& result)
		{
			return std::make_shared<Task<T>>(result);
		}

		static BasicTaskPtr delay(unsigned ms)
		{
			return EventSystemUtils::execute_in_worker([ms]()
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(ms));
			});
		}
	};	

}
