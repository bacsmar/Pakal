#pragma once
#include "Config.h"
#include "TaskFwd.h"


#include <functional>
#include <thread>
#include <atomic>
#include <condition_variable>
#include <list>

namespace Pakal
{
	class EventScheduler;

	class _PAKALExport BasicTask
	{
		friend class AsyncTaskDispatcher;
		friend class EventScheduler;
		template <class T> friend class TaskCompletionSource;

		std::function<void()> m_job;
		std::atomic_bool	 m_completed;
	protected:
		struct ContinuationData
		{
			BasicTaskPtr continuation;
			std::thread::id tid;
			ContinuationData(BasicTaskPtr task, const std::thread::id &td) : continuation(task), tid(td)  { }
		}; 

		std::list<ContinuationData> m_continuations;
		std::mutex m_continuation_mutex;

		inline void set_completed();
		void  queue_continuations();
		virtual void run();

	public:
		inline bool is_completed() { return m_completed; }

		explicit BasicTask()  { m_completed = true; }
		explicit BasicTask(const std::function<void()>& job) : m_job(job) { m_completed = false; };
		virtual ~BasicTask() {};

		void wait();		

		BasicTaskPtr continue_with(const std::function<void()>& callBack, std::thread::id callBackThread = NULL_THREAD);

	};

}