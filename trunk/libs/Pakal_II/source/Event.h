#pragma once

#include "Config.h"

#include <vector>
#include <functional>
#include <mutex>

#include <Poco/Thread.h>

#include "InboxQueue.h"
#include "EventScheduler.h"


//TODO make them sharedptr
namespace Pakal
{
	template <class TArgs>
	class
		_PAKALExport Event
	{
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

		Event() : m_scheduler(nullptr), m_isEnabled(true)
		{
		}

		inline void connectWithScheduler(EventScheduler* scheduler)
		{
			m_scheduler = scheduler;
		}

		inline void add(MethodDelegate& delegate)
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

		void remove(MethodDelegate& delegate)
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

			auto currentTid = Poco::Thread::currentTid();

			m_mutex.lock();
			std::vector<DelegateData> copyDelegates(m_delegates);
			m_mutex.unlock();

			for (DelegateData& dd : copyDelegates)
			{
				if (m_scheduler && dd.tid != currentTid)
				{
					InboxQueue* inbox = m_scheduler->InboxForThread(dd.tid);

					if (inbox)
					{
						std::function<int()> lambda = [dd,arguments]()
						{
							dd.delegate(arguments);
							return 0;
						};

						inbox->pushTask(lambda);
						continue;
					}
				}

				dd.delegate(arguments);
			}
		}
	};
}