#pragma once

#include <unordered_map>

#include <functional>
#include <mutex>
#include <thread>

#include "EventSystemUtils.h"


namespace Pakal
{
	namespace priv
	{
		template <class TArgs>
		struct DelegateData
		{
			std::function<void(TArgs)> delegate;
			std::thread::id tid;
			bool is_subscribed;
			bool is_enabled;
		};
		template<>
		struct _PAKALExport DelegateData<void>
		{
			std::function<void()> delegate;
			std::thread::id tid;
			bool is_subscribed;
			bool is_enabled;
		};


		template <class TArgs>
		class _PAKALExport Event_t
		{
			std::unordered_map<unsigned long long, SharedPtr<DelegateData<TArgs>>> m_delegates;
			bool m_enabled;
			mutable std::mutex m_mutex;

			typedef std::function<void(TArgs)> MethodDelegate;
		public:

			explicit Event_t() : m_enabled(true) {}

			inline unsigned long long add_listener(const MethodDelegate& delegate, std::thread::id callbackThread = NULL_THREAD)
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				
				unsigned long long key = EventSchedulerHelper::new_id();

				SharedPtr<DelegateData<TArgs>> metaData =
					m_delegates.emplace(key, std::make_shared<DelegateData<TArgs>>()).first->second;

				metaData->tid = callbackThread;
				metaData->is_enabled = m_enabled;
				metaData->is_subscribed = true;

				metaData->delegate = [metaData, delegate](const TArgs& args) { if (metaData->is_subscribed && metaData->is_enabled)  delegate(args); };

				return key;
			}

			inline void remove_listener(unsigned long long key)
			{
				std::lock_guard<std::mutex> lock(m_mutex);

				auto it = m_delegates.find(key);

				it->second->is_subscribed = false;

				m_delegates.erase(it);

			}

			inline bool empty() const
			{
				std::lock_guard<std::mutex> lock(m_mutex);

				return m_delegates.empty();
			}

		protected:
			inline void disable()
			{
				std::lock_guard<std::mutex> lock(m_mutex);

				m_enabled = false;

				for (auto it : m_delegates) it.second->is_enabled = m_enabled;
			}

			inline void enable()
			{
				std::lock_guard<std::mutex> lock(m_mutex);

				m_enabled = true;

				for (auto it : m_delegates) it.second->is_enabled = m_enabled;
			}

			inline void clear()
			{
				std::lock_guard<std::mutex> lock(m_mutex);

				for (auto it : m_delegates) it.second->is_subscribed = false;

				m_delegates.clear();
			}

			void notify(const TArgs& arguments)
			{
				m_mutex.lock();

				if (!m_enabled || m_delegates.empty())
				{
					m_mutex.unlock();
					return;
				}

				auto copyDelegates(m_delegates);

				m_mutex.unlock();

				auto thisThread = THIS_THREAD;

				for (const auto& dd : copyDelegates)
				{
					if (thisThread == dd.second->tid || dd.second->tid == NULL_THREAD)
						dd.second->delegate(arguments);
					else
						EventSchedulerHelper::execute_in_thread([dd, arguments]() { dd.second->delegate(arguments); }, dd.second->tid);
				}
			}

		};

		template<>
		class _PAKALExport Event_t<void>
		{
		public:
			typedef std::function<void(void)> MethodDelegate;
		private:

			std::unordered_map<unsigned long long,SharedPtr<DelegateData<void>>> m_delegates;
			bool m_enabled;
			mutable std::mutex m_mutex;

		public:

			explicit Event_t() : m_enabled(true)
			{
			}

			inline bool empty() const
			{
				std::lock_guard<std::mutex> lock(m_mutex);

				return m_delegates.empty();
			}

			inline unsigned long long add_listener(const MethodDelegate& delegate, std::thread::id callBackThread = NULL_THREAD)
			{
				std::lock_guard<std::mutex> lock(m_mutex);

				unsigned long long key = EventSchedulerHelper::new_id();

				SharedPtr<DelegateData<void>> metaData = 
					m_delegates.emplace(key, std::make_shared<DelegateData<void>>()).first->second;

				metaData->tid = callBackThread;
				metaData->is_enabled = m_enabled;
				metaData->is_subscribed = true;
				metaData->delegate = [metaData,delegate]() { if (metaData->is_subscribed && metaData->is_enabled)  delegate(); };

				return key;
			}

			inline void remove_listener(unsigned long long key)
			{
				std::lock_guard<std::mutex> lock(m_mutex);

				auto it = m_delegates.find(key);

				it->second->is_subscribed = false;

				m_delegates.erase(it);
			}
		protected:
			inline void disable()
			{
				std::lock_guard<std::mutex> lock(m_mutex);
				m_enabled = false;
				for (auto it : m_delegates) it.second->is_enabled = m_enabled;
			}

			inline void enable()
			{
				std::lock_guard<std::mutex> lock(m_mutex);

				m_enabled = true;

				for (auto it : m_delegates) it.second->is_enabled = m_enabled;

			}

			inline void clear()
			{
				std::lock_guard<std::mutex> lock(m_mutex);

				for(auto it : m_delegates) it.second->is_subscribed = false;

				m_delegates.clear();
			}

			void notify()
			{
				m_mutex.lock();

				if (!m_enabled || m_delegates.empty())
				{
					m_mutex.unlock();
					return;
				}

				auto copyDelegates(m_delegates);

				m_mutex.unlock();
			

				auto thisThread = THIS_THREAD;

				for (const auto &dd : copyDelegates)
				{
					if (thisThread == dd.second->tid || dd.second->tid == NULL_THREAD)
						dd.second->delegate();
					else
						EventSchedulerHelper::execute_in_thread(dd.second->delegate, dd.second->tid);
				}
			}

		};

	}
	
	template <class TOwner, class TArgs>
	class Event : public priv::Event_t<TArgs>
	{
		friend TOwner;
	};
}