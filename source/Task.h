#pragma once
#include "Config.h"

#include <functional>
#include <atomic>
#include <vector>

#include "BasicTask.h"
#include "Event.h"
#include "EventSystemUtils.h"

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

		Task(const std::function<TArgs(void)>& job, EventScheduler* scheduler) : m_job(job)
		{
			set_completed( false);
			m_event_completed.connect_with_scheduler(scheduler);
			m_event_completed_with_result.connect_with_scheduler(scheduler);
		}

		Task(const TArgs& result) : m_result(result)
		{
			set_completed(true);
		}

		~Task()
		{
		}		


	private:

		std::function<TArgs(void)>		m_job;
		TArgs							m_result;
		Event<TArgs>					m_event_completed_with_result;

	protected:
		inline void run() override
		{
			ASSERT(is_completed() == false);

			m_result = m_job();			
			set_completed(true);
			m_event_completed_with_result.notify(m_result);
			m_event_completed.notify();
		}

	public:

		inline TArgs result()
		{
			wait();
			return m_result;
		}
		
		void on_completion(const std::function<void(TArgs)>& callBack, std::thread::id callBackThread = std::this_thread::get_id())
		{
			if (m_completed)
			{
				m_event_completed_with_result.clear();
				m_event_completed_with_result.addListener(callBack,callBackThread);
				m_event_completed_with_result.notify(m_result);
			}
			else
				m_event_completed_with_result.addListener(callBack,callBackThread);
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

			EventScheduler* scheduler = tasks.at(0)->get_event_scheduler();

			static auto emptyDelegate = []()
			{
				std::atomic_int n;
				n.store(0);
				return n;
			};

			auto task = std::make_shared<Task<std::atomic_int>>(emptyDelegate, scheduler);
			task->m_result = tasks.size();

			BasicTaskPtr myTask(task);

			std::function<void()> onComplete = [myTask]()
			{
				Task<std::atomic_int>* t = static_cast<Task<std::atomic_int>*>(myTask.get());
				--t->m_result;
				if (t->m_result == 0)
				{
					t->run();
				}
			};

			for (auto& t : tasks)
			{
				t->on_completion(onComplete, NULL_THREAD);
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
			return std::make_shared<BasicTask>();
		};

		template <class T>
		static std::shared_ptr<Task<T>> from_result(const T& result)
		{
			return std::make_shared<Task<T>>(result);
		}
	};
	

}
