#include "ThreadPool.h"
#include "LogMgr.h"

#include <chrono>


namespace Pakal
{

	ThreadPool::ThreadPool() = default;

	ThreadPool::~ThreadPool()
	{
		std::map<std::thread::id, SharedPtr<std::thread>> workers;
		std::vector<SharedPtr<std::thread>> finished;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_running = false;
			// taken out of the pool: a worker that stops from now on finds itself no more in m_workers and just returns
			workers.swap(m_workers);
			finished.swap(m_finished);
		}
		m_job_available_condition.notify_all();
		LOG_INFO("terminating threadpool waiting for worker threads to finish...");
		for (auto& pair : workers) pair.second->join();
		for (auto& thread : finished) thread->join();
	}

	void ThreadPool::worker_loop()
	{
		std::unique_lock<std::mutex> lock(m_mutex);

		for (;;)
		{
			const bool woken = m_job_available_condition.wait_for(lock, std::chrono::seconds(60),
				[this]() { return !m_jobs.empty() || !m_running; });

			if (m_jobs.empty())
			{
				// stopping with nothing left to run, or idle for 60 seconds while another idle worker can take the next job
				if (!m_running || (!woken && m_idle_workers > 1))
				{
					--m_idle_workers;
					auto self = m_workers.find(THIS_THREAD);
					if (self != m_workers.end())
					{
						m_finished.push_back(self->second);
						m_workers.erase(self);
					}
					return;
				}
				continue;
			}

			std::function<void()> job = std::move(m_jobs.front());
			m_jobs.pop();
			--m_idle_workers;

			lock.unlock();
			job();
			lock.lock();

			++m_idle_workers;
		}
	}

	void ThreadPool::add_worker()
	{
		++m_idle_workers;
		auto thread = std::make_shared<std::thread>(&ThreadPool::worker_loop, this);
		m_workers.emplace(thread->get_id(), thread);
	}

	void ThreadPool::join_finished()
	{
		// a worker moves itself here under the mutex and then only returns: joining it waits for nothing else
		for (auto& thread : m_finished) thread->join();
		m_finished.clear();
	}

	void ThreadPool::execute(const std::function<void()>& job)
	{
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			ASSERT(m_running);
			join_finished();
			m_jobs.push(job);

			// each idle worker takes one queued job: one more worker for a job beyond them
			if (m_jobs.size() > m_idle_workers)
			{
				LOG_DEBUG("[ThreadPool] adding a worker (%u running)", static_cast<unsigned>(m_workers.size()) + 1);
				add_worker();
			}
		}
		m_job_available_condition.notify_one();
	}
}
