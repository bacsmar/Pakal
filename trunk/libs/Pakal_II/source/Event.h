#pragma once

#include "Config.h"

#include "vector"
#include <functional>
#include "Poco/Thread.h"

#include "InboxQueue.h"
#include "EventScheduler.h"



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
		std::vector<DelegateData*> m_delegates;
		bool m_isEnabled;

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
			DelegateData* data = new DelegateData(delegate, Poco::Thread::currentTid());

			m_delegates.push_back(data);
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
			for (auto it = m_delegates.begin(); it != m_delegates.end(); ++it)
			{
				if (delegate == (*it)->delegate)
				{
					delete *it;
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

			for (DelegateData* dd : m_delegates)
			{
				if (m_scheduler && dd->tid != currentTid)
				{
					InboxQueue* inbox = m_scheduler->InboxForThread(dd->tid);

					if (inbox)
					{
						
						std::function<int()> lambda = [dd,arguments]()
						{
							dd->delegate(arguments);
							return 0;
						};

						inbox->pushTask(lambda);
						continue;
					}
				}

				dd->delegate(arguments);
			}
		}
	};
}