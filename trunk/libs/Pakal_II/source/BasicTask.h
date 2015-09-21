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

	protected:
		struct ContinuationData
		{
			BasicTaskPtr continuation;
			std::thread::id tid;
			ContinuationData(BasicTaskPtr task, const std::thread::id &td) : continuation(task), tid(td) { }
		};

		std::function<void()> m_job;

		void queue_continuation(const ContinuationData&& continuation);
		void run_continuations();
		void run();

	private:

		std::atomic_bool	 m_completed;
		std::list<ContinuationData> m_continuations;
		std::mutex m_continuation_mutex;

	public:

		explicit BasicTask()  { m_completed = true; }
		explicit BasicTask(const std::function<void()>& job) : m_job(job) { m_completed = false; };
		virtual ~BasicTask() {};

		inline bool is_completed() { return m_completed; }
		void wait();		

		BasicTaskPtr continue_with(const std::function<void()>& callBack, std::thread::id callBackThread = NULL_THREAD);

		template<typename TReturn>
		TaskPtr<TReturn> continue_with(const std::function<TReturn()>& callBack, std::thread::id callBackThread = NULL_THREAD);
	};

	template<typename TReturn>
	TaskPtr<TReturn> BasicTask::continue_with(const std::function<TReturn()>& callBack, std::thread::id callBackThread) 
	{
		auto task = std::make_shared<Task<TReturn>>([=]() { return callBack(); });

		queue_continuation(ContinuationData(task, callBackThread));
		
		if (is_completed())
		{
			run_continuations();
		}

		return task;
	}
}