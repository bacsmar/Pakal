///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: AssetCache.h
// Purpose: Assets loaded in the background and shared by handle. load<T>(path) gives an AssetHandle
//          at once (a shared_ptr to the asset's slot: loading, ready or failed); the cache keeps only
//          a weak_ptr, so an asset lives while someone holds it and a second load of the same file is
//          the same asset. T::decode(path) reads and decodes the file on a worker thread; if T has
//          upload() (making GPU resources), it runs on the thread that calls pump(), the graphics one.
//          A game's simulation waits until its own data is ready before its first step (a barrier
//          under the loading screen); its views can take their models when they are ready.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atomic>
#include <chrono>
#include <concepts>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <typeindex>
#include <utility>
#include <vector>

namespace Pakal
{
	enum class AssetState { Loading, Ready, Failed };

	template <class T>
	class Asset
	{
	public:
		explicit Asset(std::string path) : m_path(std::move(path)) {}

		AssetState state() const { return m_state.load(); }
		bool ready() const { return state() == AssetState::Ready; }
		bool failed() const { return state() == AssetState::Failed; }
		const std::string& path() const { return m_path; }
		// The asset once ready (nullptr before, or when it failed)
		T* get() const { return ready() ? m_value.get() : nullptr; }

	private:
		friend class AssetCache;
		std::string m_path;
		std::atomic<AssetState> m_state{ AssetState::Loading };
		std::shared_ptr<T> m_value;
	};

	template <class T>
	using AssetHandle = std::shared_ptr<Asset<T>>;

	class AssetCache
	{
	public:
		// 'workers' threads read and decode (at least one)
		explicit AssetCache(unsigned workers = 2)
		{
			for (unsigned i = 0; i < std::max(1u, workers); ++i)
			{
				m_workers.emplace_back([this]() { work(); });
			}
		}

		~AssetCache()
		{
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_stopping = true;
			}
			m_wake.notify_all();
			for (auto& worker : m_workers) worker.join();
		}

		AssetCache(const AssetCache&) = delete;
		AssetCache& operator=(const AssetCache&) = delete;

		// The asset of this file: the one already held by someone, or a new one that starts loading now
		template <class T>
		AssetHandle<T> load(const std::string& path)
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			const Key key{ std::type_index(typeid(T)), path };
			if (auto known = std::static_pointer_cast<Asset<T>>(m_assets[key].lock()))
			{
				return known;
			}
			auto asset = std::make_shared<Asset<T>>(path);
			m_assets[key] = asset;
			++m_pending;
			m_jobs.push_back([this, asset]() { decode(asset); });
			m_wake.notify_one();
			return asset;
		}

		// Runs the uploads waiting for this thread (the graphics one) for at most 'budgetMs'. True when nothing is waiting.
		bool pump(double budgetMs)
		{
			const auto start = std::chrono::steady_clock::now();
			for (;;)
			{
				std::function<void()> upload;
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					if (m_uploads.empty()) return m_pending == 0;
					upload = std::move(m_uploads.front());
					m_uploads.pop_front();
				}
				upload();
				if (std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count() >= budgetMs)
				{
					std::lock_guard<std::mutex> lock(m_mutex);
					return m_uploads.empty() && m_pending == 0;
				}
			}
		}

		// Assets still loading (decoding, or waiting for their upload)
		size_t pending() const
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			return m_pending;
		}

		// Forgets the files nobody holds any more (their slots are already gone; this only trims the index)
		void trim()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			for (auto it = m_assets.begin(); it != m_assets.end();)
			{
				it = it->second.expired() ? m_assets.erase(it) : std::next(it);
			}
		}

	private:
		using Key = std::pair<std::type_index, std::string>;

		template <class T>
		void decode(const AssetHandle<T>& asset)
		{
			std::unique_ptr<T> value = T::decode(asset->path());
			if (!value)
			{
				finish(asset, AssetState::Failed);
				return;
			}
			if constexpr (requires(T& t) { { t.upload() } -> std::same_as<bool>; })
			{
				std::shared_ptr<T> shared(std::move(value));
				std::lock_guard<std::mutex> lock(m_mutex);
				m_uploads.push_back([this, asset, shared]()
				{
					const bool ok = shared->upload();
					asset->m_value = shared;
					finish(asset, ok ? AssetState::Ready : AssetState::Failed);
				});
			}
			else
			{
				asset->m_value = std::move(value);
				finish(asset, AssetState::Ready);
			}
		}

		template <class T>
		void finish(const AssetHandle<T>& asset, AssetState state)
		{
			asset->m_state.store(state);
			std::lock_guard<std::mutex> lock(m_mutex);
			--m_pending;
		}

		void work()
		{
			for (;;)
			{
				std::function<void()> job;
				{
					std::unique_lock<std::mutex> lock(m_mutex);
					m_wake.wait(lock, [this]() { return m_stopping || !m_jobs.empty(); });
					if (m_stopping) return;
					job = std::move(m_jobs.front());
					m_jobs.pop_front();
				}
				job();
			}
		}

		mutable std::mutex m_mutex;
		std::condition_variable m_wake;
		std::map<Key, std::weak_ptr<void>> m_assets;
		std::deque<std::function<void()>> m_jobs;      // decodes, for the workers
		std::deque<std::function<void()>> m_uploads;   // uploads, for pump()
		size_t m_pending = 0;
		bool m_stopping = false;
		std::vector<std::thread> m_workers;
	};
}
