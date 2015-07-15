#pragma once
#include "Config.h"

#include <functional>
#include <thread>

#include "TaskFwd.h"
#include "Event.h"

namespace Pakal
{
	class EventScheduler;

	class _PAKALExport BasicTask
	{
		friend class InboxQueue;
		friend class AsyncTaskDispatcher;

	private:
		std::function<void()> m_Job;

	protected:

		Event<void>			  m_EventCompleted;
		volatile bool		  m_isCompleted;

		virtual void run();

	public:

		BasicTask(const std::function<void()>& job, EventScheduler* scheduler);
		BasicTask();

		virtual ~BasicTask();

		inline bool isCompleted();
		inline void wait();
		void onCompletionDo(const std::function<void()>& callBack, std::thread::id callBackThread = std::this_thread::get_id());
		inline EventScheduler* getEventScheduler();
	};

	}