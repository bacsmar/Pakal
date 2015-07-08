#pragma once

#include "Config.h"

#include <vector>
#include <functional>
#include <mutex>

#include <Poco/Thread.h>
#include "EventScheduler.h"

namespace Pakal
{
	template <class TArgs>
	class
		_PAKALExport Event
	{
	public:
		typedef std::function<void(TArgs)> MethodDelegate;		
	private:
		struct DelegateData
		{
			MethodDelegate delegate;
			Poco::Thread::TID tid;

			DelegateData(MethodDelegate& d, Poco::Thread::TID td) : delegate(d), tid(td) {}

		};

		EventScheduler* m_scheduler;
		std::vector<DelegateData> m_delegates;
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

		inline void addListener(MethodDelegate& delegate)
		{

			std::lock_guard<std::mutex> lock(m_mutex);

			m_delegates.push_back(DelegateData(delegate, Poco::Thread::currentTid()));
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

		void removeListener(MethodDelegate& delegate)
		{
			std::lock_guard<std::mutex> lock(m_mutex);

			for (auto it = m_delegates.begin(); it != m_delegates.end(); ++it)
			{
				if (delegate == (*it)->delegate)
				{
					m_delegates.erase(it);
					return;
				}
			}
		}

		void notify(const TArgs& arguments)
		{
			if (!m_isEnabled)
				return;

			m_mutex.lock();
			std::vector<DelegateData> copyDelegates(m_delegates);
			m_mutex.unlock();

			for (DelegateData& dd : copyDelegates)
			{
				if (m_scheduler)
				{
					std::function<void()> bridge = [dd,arguments]() { dd.delegate(arguments); };
					m_scheduler->executeInThread(dd.tid,bridge);
				}
				else
					dd.delegate(arguments);
			}
		}
	};
}