#pragma once

#include "Config.h"

#include <list>
#include <functional>
#include <mutex>
#include <thread>

#include "EventSchedulerUtils.h"

namespace Pakal
{
	template <class TArgs>
	struct DelegateData
	{
		const std::function<void(TArgs)> delegate;
		const std::thread::id tid;

		DelegateData(const std::function<void(TArgs)>& d, std::thread::id td) : delegate(d), tid(td)
		{
		}
	};


	template <class TArgs>
	class
		_PAKALExport Event
	{
	private:
		EventScheduler* m_scheduler;
		std::list<DelegateData<TArgs>> m_delegates;
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

		inline void addListener(const MethodDelegate& delegate, std::thread::id callBackThread = std::this_thread::get_id())
		{

			std::lock_guard<std::mutex> lock(m_mutex);

			m_delegates.push_back(DelegateData<TArgs>(delegate, callBackThread));
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

	/*	inline void removeListener(const MethodDelegate& delegate)
		{		
			auto t = std::find(m_delegates.begin(),m_delegates.end(),delegate);
			m_delegates.erase(t);
		}*/

		void notify(const TArgs& arguments)
		{
			if (!m_isEnabled)
				return;

			m_mutex.lock();
			std::list<DelegateData<TArgs>> copyDelegates(m_delegates);
			m_mutex.unlock();

			for (DelegateData<TArgs>& dd : copyDelegates)
			{
				if (m_scheduler == nullptr || dd.tid == NULL_THREAD )
					dd.delegate(arguments);
				else
					EventSchedulerUtils::executeInThread(m_scheduler,[dd,arguments]() { dd.delegate(arguments); },dd.tid);
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
		std::list<DelegateData<void>> m_delegates;
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

		inline void addListener(const MethodDelegate& delegate, std::thread::id callBackThread = std::this_thread::get_id())
		{
			std::lock_guard<std::mutex> lock(m_mutex);


			m_delegates.push_back(DelegateData<void>(delegate, callBackThread));
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

		//inline void removeListener(const MethodDelegate& delegate)
		//{		
		//	auto t = std::find(m_delegates.begin(),m_delegates.end(),delegate);
		//	m_delegates.erase(t);
		//}

		void notify()
		{
			if (!m_isEnabled)
				return;

			m_mutex.lock();
			std::list<DelegateData<void>> copyDelegates(m_delegates);
			m_mutex.unlock();

			for (DelegateData<void>& dd : copyDelegates)
			{
				if (m_scheduler == nullptr || dd.tid == NULL_THREAD )
					dd.delegate();
				else
					EventSchedulerUtils::executeInThread(m_scheduler,dd.delegate,dd.tid);
			}
		}

	};



}