#pragma once
#include "Config.h"

#include <functional>
#include <thread>
#include <atomic>
#include <condition_variable>

#include "TaskFwd.h"
#include "Event.h"

namespace Pakal
{
	class EventScheduler;

	class _PAKALExport BasicTask
	{
		friend class InboxQueue;
		friend class AsyncTaskDispatcher;
		friend class TaskCompletionSource;

	private:
		std::function<void()> m_job;
		std::condition_variable m_cv;
		std::mutex m_cv_m;
		std::atomic_bool	  m_completed;

	protected:

		Event<void>			  m_event_completed;		

		virtual void run();

	public:

		BasicTask(const std::function<void()>& job, EventScheduler* scheduler);
		BasicTask();

		virtual ~BasicTask();

		inline bool is_completed()
		{
			return m_completed;
		}

		inline void set_completed(bool val)
		{
			m_completed = val;
			m_cv.notify_one();
		}

		inline void wait();
		void on_completion(const std::function<void()>& callBack, std::thread::id callBackThread = std::this_thread::get_id());
		inline EventScheduler* get_event_scheduler();
	};

	}