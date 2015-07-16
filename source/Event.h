#pragma once

#include "Config.h"

#include <set>

#include <unordered_map>

#include <functional>
#include <mutex>
#include <thread>

#include "EventSystemUtils.h"


namespace Pakal
{
	template <class TArgs>
	struct DelegateData
	{
		const std::function<void(TArgs)> delegate;
		const std::thread::id tid;		

		DelegateData(const std::function<void(TArgs)>& d, const std::thread::id &td) : delegate(d), tid(td) 
		{
		}
		
	};	

	template <class TArgs>
	class
		_PAKALExport Event
	{
	private:
		EventScheduler* m_scheduler;
		std::unordered_map<unsigned int, DelegateData<TArgs>> m_delegates;
		bool m_isEnabled;
		std::mutex m_mutex;

		typedef std::function<void(TArgs)> MethodDelegate;
	public:

		inline EventScheduler* getEventScheduler() { return m_scheduler; }

		Event() : m_scheduler(nullptr), m_isEnabled(true)
		{
		}

		inline void connectWithScheduler(EventScheduler* scheduler)
		{
			m_scheduler = scheduler;
		}

		inline unsigned int addListener(const MethodDelegate& delegate, std::thread::id callBackThread = std::this_thread::get_id())
		{			
			std::lock_guard<std::mutex> lock(m_mutex);

			unsigned int hash = EventSystemUtils::hashFunction((int)& delegate + (int)& callBackThread);
			m_delegates.emplace(hash, DelegateData<TArgs>(delegate, callBackThread) );

			return hash;
		}

		inline void disable()
		{
			m_isEnabled = false;
		}

		inline void enable()
		{
			m_isEnabled = true;
		}

		inline bool empty() const
		{
			return m_delegates.empty();
		}

		inline void removeListener(unsigned int key)
		{					
			m_delegates.erase(key);
		}

		void notify(const TArgs& arguments)
		{
			if (!m_isEnabled)
				return;

			m_mutex.lock();
			auto copyDelegates(m_delegates);
			m_mutex.unlock();

			for (const auto& dd : copyDelegates)
			{
				if (m_scheduler == nullptr || dd.second.tid == NULL_THREAD )
					dd.second.delegate(arguments);
				else
					EventSystemUtils::executeInThread(m_scheduler,[dd,arguments]() { dd.second.delegate(arguments); },dd.second.tid);
			}
		}

	};

	template<>
	class _PAKALExport Event<void>
	{
	public:
		typedef std::function<void(void)> MethodDelegate;		
	private:

		EventScheduler* m_scheduler;
		std::unordered_map<unsigned int, DelegateData<void>> m_delegates;
		bool m_isEnabled;
		std::mutex m_mutex;

	public:

		inline EventScheduler* getEventScheduler() { return m_scheduler; }

		Event() : m_scheduler(nullptr), m_isEnabled(true)
		{
		}

		inline void connectWithScheduler(EventScheduler* scheduler)
		{
			m_scheduler = scheduler;
		}

		inline unsigned int addListener(const MethodDelegate& delegate, std::thread::id callBackThread = std::this_thread::get_id())
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			unsigned int hash = EventSystemUtils::hashFunction((int)& delegate + (int)& callBackThread);
			m_delegates.emplace(hash, DelegateData<void>(delegate, callBackThread) );
			return hash;
		}

		inline void disable()
		{
			m_isEnabled = false;
		}

		inline void enable()
		{
			m_isEnabled = true;
		}

		inline bool empty() const
		{
			return m_delegates.empty();
		}

		inline void removeListener(unsigned int key)
		{					
			m_delegates.erase(key);
		}

		void notify()
		{
			if (!m_isEnabled)
				return;

			m_mutex.lock();
			auto copyDelegates(m_delegates);
			m_mutex.unlock();

			for (const auto &dd : copyDelegates)
			{
				if (m_scheduler == nullptr || dd.second.tid == NULL_THREAD )
					dd.second.delegate();
				else
					EventSystemUtils::executeInThread(m_scheduler,dd.second.delegate,dd.second.tid);
			}
		}

	};



}