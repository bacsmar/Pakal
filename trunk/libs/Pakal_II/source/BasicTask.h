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

		inline void set_completed(bool val);

		virtual void run();

	public:

		BasicTask(const std::function<void()>& job, EventScheduler* scheduler);
		BasicTask();

		virtual ~BasicTask() {};

		inline bool is_completed();


		inline void wait();
		void on_completion(const std::function<void()>& callBack, std::thread::id callBackThread = std::this_thread::get_id());
		inline EventScheduler* get_event_scheduler();
	};

	}