#pragma once
#include "Config.h"

#include <functional>
#include <thread>
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

	private:
		std::function<void()> m_Job;
		std::condition_variable m_cv;
		std::mutex m_cv_m;
		volatile bool		  m_isCompleted;

	protected:

		Event<void>			  m_EventCompleted;		

		virtual void run();

	public:

		BasicTask(const std::function<void()>& job, EventScheduler* scheduler);
		BasicTask();

		virtual ~BasicTask();

		inline bool isCompleted()
		{
			return m_isCompleted;
		}
		inline void setIsCompleted(bool val)
		{
			m_isCompleted = val;
			m_cv.notify_one();
		}

		inline void wait();
		void onCompletionDo(const std::function<void()>& callBack, std::thread::id callBackThread = std::this_thread::get_id());
		inline EventScheduler* getEventScheduler();
	};

	}