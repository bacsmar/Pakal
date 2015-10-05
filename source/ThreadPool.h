#pragma once
#include "Config.h"
#include "TaskFwd.h"

#include <functional>
#include <mutex>
#include <map>
#include <queue>
#include <atomic>

/*
Author: Luis Gudiño Salinas
Behaviour:
	The threadpool is always going to have at least 1 available thread ready for when you call execute
	
	after the work is done the threads starts to sleep for 60 seconds , if no new job is queued then the thread 
	is going to be removed (only if it is not the last available thread)

*/

namespace Pakal
{
	class _PAKALExport ThreadPool
	{
		std::condition_variable m_job_available_condition;

		std::mutex				m_jobs_mutex,
								m_workers_mutex;

		std::map<std::thread::id,SharedPtr<std::thread>> m_workers;
		std::queue<std::function<void()>>				 m_jobs;

		std::atomic_bool m_running;
		std::atomic_int m_available_workers;

		std::function<void()> try_get_job();

		void worker_loop();

		void add_worker();


	public:

		explicit ThreadPool();

		~ThreadPool();

		void execute(const std::function<void()>& job);
	};

}