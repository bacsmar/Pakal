#pragma once
#include "Config.h"
#include "TaskFwd.h"

#include <condition_variable>
#include <functional>
#include <mutex>
#include <map>
#include <queue>
#include <thread>
#include <vector>

/*
Author: Luis Gudiño Salinas
Behaviour:
	The pool grows when a job is queued and no worker is idle to take it, and a worker that waited 60 seconds for a job
	leaves (unless it is the last idle one). One mutex guards the jobs, the workers and the count of idle ones, so a job
	queued while a worker is about to sleep is never missed. When the pool ends, its workers run the jobs still queued
	before they stop: a task someone waits on always completes.
*/

namespace Pakal
{
	class _PAKALExport ThreadPool
	{
		std::mutex				m_mutex;
		std::condition_variable m_job_available_condition;

		std::map<std::thread::id, SharedPtr<std::thread>> m_workers;
		std::vector<SharedPtr<std::thread>>					m_finished;	// workers that left, joined later
		std::queue<std::function<void()>>					m_jobs;

		bool		m_running = true;
		unsigned	m_idle_workers = 0;

		void worker_loop();
		void add_worker();		// with m_mutex held
		void join_finished();	// with m_mutex held

	public:

		explicit ThreadPool();

		~ThreadPool();

		void execute(const std::function<void()>& job);
	};

}
