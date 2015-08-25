#pragma once
#include "Config.h"

#include <functional>
#include <thread>
#include <atomic>
#include <condition_variable>

#include "Event.h"

namespace Pakal
{
	class EventScheduler;

	class _PAKALExport BasicTask
	{
		friend class InboxQueue;
		friend class AsyncTaskDispatcher;
		friend class TaskCompletionSource;

		std::function<void()> m_job;
		std::atomic_bool	  m_completed;

		std::condition_variable m_wait_condition;
		std::mutex m_wait_mutex;

	protected:

		Event<void>			  m_event_completed;

		inline void set_completed();
		virtual void run();

	public:

		explicit BasicTask(const std::function<void()>& job, EventScheduler* scheduler) : m_job(job) { m_completed=false, m_event_completed.connect_with_scheduler(scheduler); };
		explicit BasicTask(EventScheduler* scheduler) { m_completed=true; m_event_completed.connect_with_scheduler(scheduler); }

		virtual ~BasicTask() {};

		inline bool is_completed() { return m_completed; }
		inline EventScheduler* get_event_scheduler() { return m_event_completed.get_event_scheduler(); }
		inline void wait()
		{
			std::unique_lock<std::mutex> lock(m_wait_mutex);
			m_wait_condition.wait(lock, [=](){ return is_completed();} );			
		}


		void on_completion(const std::function<void()>& callBack, std::thread::id callBackThread = std::this_thread::get_id());
	};

}