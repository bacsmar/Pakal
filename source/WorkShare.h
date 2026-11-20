///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: WorkShare.h
// Purpose: A frame's independent pieces of work (one per item: a skinned model's pose) shared among a few
//          worker threads and the caller. Each thread takes a fixed share (the n-th item, n + threads...),
//          so no queue is locked per item, and run() returns once every item is done: a frame's work never
//          mixes with the next one's. Below 'minimum' items the caller does them all (no wake-ups).
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace Pakal
{
	class WorkShare
	{
	public:
		// 'workers' threads besides the caller (0: the caller does everything)
		explicit WorkShare(unsigned workers)
		{
			for (unsigned i = 0; i < workers; ++i) m_threads.emplace_back([this, i]() { loop(i + 1); });
		}

		// One fewer than the cores, at most 'most'
		static unsigned workers_for_cores(unsigned most)
		{
			const unsigned cores = std::thread::hardware_concurrency();
			return std::min(most, cores > 1 ? cores - 1 : 0u);
		}

		~WorkShare()
		{
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_stop = true;
			}
			m_wake.notify_all();
			for (auto& thread : m_threads) thread.join();
		}

		WorkShare(const WorkShare&) = delete;
		WorkShare& operator=(const WorkShare&) = delete;

		size_t workers() const { return m_threads.size(); }

		// work(i) for every i in [0, count), on the workers and the caller; returns when all are done. One caller at a time.
		void run(size_t count, const std::function<void(size_t)>& work, size_t minimum = 8)
		{
			if (count < minimum || m_threads.empty())
			{
				for (size_t i = 0; i < count; ++i) work(i);
				return;
			}
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_work = &work;
				m_count = count;
				m_busy = m_threads.size();
				++m_generation;
			}
			m_wake.notify_all();
			const size_t share = m_threads.size() + 1;
			for (size_t i = 0; i < count; i += share) work(i);   // (the caller's share: the 0th)
			std::unique_lock<std::mutex> lock(m_mutex);
			m_done.wait(lock, [this]() { return m_busy == 0; });
			m_work = nullptr;
		}

	private:
		void loop(size_t first)
		{
			unsigned seen = 0;
			for (;;)
			{
				const std::function<void(size_t)>* work = nullptr;
				size_t count = 0;
				{
					std::unique_lock<std::mutex> lock(m_mutex);
					m_wake.wait(lock, [&]() { return m_stop || m_generation != seen; });
					if (m_stop) return;
					seen = m_generation;
					work = m_work;
					count = m_count;
				}
				const size_t share = m_threads.size() + 1;
				for (size_t i = first; i < count; i += share) (*work)(i);
				std::lock_guard<std::mutex> lock(m_mutex);
				if (--m_busy == 0) m_done.notify_one();
			}
		}

		std::vector<std::thread> m_threads;
		std::mutex m_mutex;
		std::condition_variable m_wake, m_done;
		const std::function<void(size_t)>* m_work = nullptr;
		size_t m_count = 0, m_busy = 0;
		unsigned m_generation = 0;
		bool m_stop = false;
	};
}
