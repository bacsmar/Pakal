#include "ThreadPool.h"
#include "LogMgr.h"

#include <functional>
#include <sstream>

#include <chrono>


namespace Pakal
{

	ThreadPool::ThreadPool() : m_running(true), m_available_workers(0)
	{
		//add_worker();
	}

	ThreadPool::~ThreadPool()
	{
		m_running = false;
		m_job_available_condition.notify_all();
		LOG_INFO("terminating threadpool waiting for worker threads to finish...");
		for (auto& pair : m_workers)
		{
			pair.second->join();
		}
		m_workers.clear();
	}

	std::function<void()> ThreadPool::try_get_job()
	{
		std::function<void()> job;

		mutex_guard lock(m_jobs_mutex);
		if (!m_jobs.empty())
		{
			job = m_jobs.front();
			m_jobs.pop();
			return job;
		}

		return job;
	}

	void ThreadPool::worker_loop() 
	{
		std::mutex jobAvailableMutex;
		std::unique_lock<std::mutex> lk(jobAvailableMutex);

		while (m_running)
		{
			bool waitExpired = !m_job_available_condition.wait_for(lk, std::chrono::seconds(60), [this]() { return !m_jobs.empty() || !m_running; });

			if (waitExpired)
			{
				mutex_guard l(m_workers_mutex);

				if (m_available_workers > 1)
				{
					LOG_INFO("removing worker not being used for 60 seconds");

					--m_available_workers;
					ASSERT(m_available_workers > 0);
					m_workers.find(THIS_THREAD)->second->detach();
					m_workers.erase(THIS_THREAD);
					break;
				}
			}


			std::function<void()> job = try_get_job();
			if (job != nullptr)
			{
				std::ostringstream ss;
				ss << THIS_THREAD; 
				LOG_INFO("worker thread with id %s processing job", ss.str().c_str());
				job();

				++m_available_workers;
			}

		}
	}



	void ThreadPool::add_worker()
	{
		mutex_guard lock(m_workers_mutex);

		++m_available_workers;
		auto thread = std::make_shared<std::thread>(&ThreadPool::worker_loop, this);
		m_workers.emplace(thread->get_id(), thread);
	}
	
	void ThreadPool::execute(const std::function<void()>& job) 
	{
		m_jobs_mutex.lock();
		m_jobs.push(job);
		m_jobs_mutex.unlock();

		--m_available_workers;

		if (m_available_workers < 1)
		{
			LOG_WARNING("not enought available workers adding a new one");
			add_worker();
		}

		m_job_available_condition.notify_one();
	}
}