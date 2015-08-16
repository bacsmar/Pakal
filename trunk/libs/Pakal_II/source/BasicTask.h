#pragma once
#include "Config.h"

#include <functional>
#include <thread>
#include <atomic>

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

	protected:

		Event<void>			  m_event_completed;
		std::atomic_bool	  m_completed;

		virtual void run();

	public:

		BasicTask(const std::function<void()>& job, EventScheduler* scheduler);
		BasicTask();

		virtual ~BasicTask();

		inline bool is_completed();
		inline void wait();
		void on_completion(const std::function<void()>& callBack, std::thread::id callBackThread = std::this_thread::get_id());
		inline EventScheduler* get_event_scheduler();
	};

	}