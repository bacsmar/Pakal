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
		bool m_enabled;
		std::mutex m_mutex;

		typedef std::function<void(TArgs)> MethodDelegate;
	public:

		inline EventScheduler* get_event_scheduler() { return m_scheduler; }

		Event() : m_scheduler(nullptr), m_enabled(true)
		{
		}

		inline void connect_with_scheduler(EventScheduler* scheduler)
		{
			m_scheduler = scheduler;
		}

		inline unsigned int add_listener(const MethodDelegate& delegate, std::thread::id callbackThread = std::this_thread::get_id())
		{			
			std::lock_guard<std::mutex> lock(m_mutex);

			unsigned int hash = EventSystemUtils::hash_function((int)& delegate + (int)& callbackThread);
			m_delegates.emplace(hash, DelegateData<TArgs>(delegate, callbackThread) );

			return hash;
		}

		inline void disable()
		{
			m_enabled = false;
		}

		inline void enable()
		{
			m_enabled = true;
		}

		inline bool empty() const
		{
			return m_delegates.empty();
		}

		inline void remove_listener(unsigned int key)
		{		
			std::lock_guard<std::mutex> lock(m_mutex);
			m_delegates.erase(key);
		}

		inline void clear()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_delegates.clear();
		}

		void notify(const TArgs& arguments)
		{
			if (!m_enabled || m_delegates.size() == 0)
				return;

			m_mutex.lock();
			auto copyDelegates(m_delegates);
			m_mutex.unlock();

			auto thisThread = std::this_thread::get_id();

			for (const auto& dd : copyDelegates)
			{
				if (thisThread == dd.second.tid || dd.second.tid == NULL_THREAD )
					dd.second.delegate(arguments);
				else
					EventSystemUtils::execute_in_thread(m_scheduler,[dd,arguments]() { dd.second.delegate(arguments); },dd.second.tid);
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

		inline EventScheduler* get_event_scheduler() { return m_scheduler; }

		Event() : m_scheduler(nullptr), m_isEnabled(true)
		{
		}

		inline void connect_with_scheduler(EventScheduler* scheduler)
		{
			m_scheduler = scheduler;
		}

		inline unsigned int add_listener(const MethodDelegate& delegate, std::thread::id callBackThread = std::this_thread::get_id())
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			unsigned int hash = EventSystemUtils::hash_function((int)& delegate + (int)& callBackThread);
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

		inline void remove_listener(unsigned int key)
		{	
			std::lock_guard<std::mutex> lock(m_mutex);
			m_delegates.erase(key);
		}

		inline void clear()
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			m_delegates.clear();
		}

		void notify()
		{
			if (!m_isEnabled || m_delegates.size() == 0)
				return;

			m_mutex.lock();
			auto copyDelegates(m_delegates);
			m_mutex.unlock();

			auto thisThread = std::this_thread::get_id();

			for (const auto &dd : copyDelegates)
			{
				if (thisThread == dd.second.tid || dd.second.tid == NULL_THREAD )
					dd.second.delegate();
				else
					EventSystemUtils::execute_in_thread(m_scheduler,dd.second.delegate,dd.second.tid);
			}
		}

	};



}