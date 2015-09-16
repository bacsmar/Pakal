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

	public:

		explicit Task(const std::function<TArgs(void)>& job) : BasicTask(nullptr),  m_job(job) {}
		explicit Task(const TArgs& result) : m_result(result) {}

		~Task() { }


	private:

		std::function<TArgs(void)>	m_job;
		TArgs						m_result;

	protected:
		inline void run() override
		{
			ASSERT(is_completed() == false);

			m_result = m_job();			
			set_completed();
			queue_continuations();
		}

	public:

		inline TArgs result()
		{
			wait();
			return m_result;
		}
		
		std::shared_ptr<Task<TArgs>> continue_with(const std::function<void(TArgs)>& callBack, std::thread::id callBackThread = NULL_THREAD)
		{
			auto task =	std::make_shared<Task<TArgs>>(callBack);

			m_continuation_mutex.lock();
			m_continuations.push_back(ContinuationData(task,callBackThread));
			m_continuation_mutex.unlock();

			if (is_completed())
			{
				queue_continuations();
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
					t->run();
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
		static std::shared_ptr<Task<T>> from_result(const T& result)
		{
			return std::make_shared<Task<T>>(result);
		}
	};	

}
