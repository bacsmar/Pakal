#pragma once

#include <unordered_map>

#include <functional>
#include <mutex>
#include <thread>

#include "EventSystemUtils.h"


namespace Pakal
{
		using ulonglong = unsigned long long;

		template <class TArgs>
		struct _PAKALExport DelegateData
		{
			std::function<void(const TArgs&)> delegate;
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
		struct _PAKALExport Delegate
		{
			std::function<void(const TArgs&)> callback;
			ulonglong* id;
			std::thread::id tid;

			Delegate(ulonglong& id, const std::function<void(const TArgs&)>&& callback, std::thread::id tid = NULL_THREAD) : callback(callback), id(&id)
			{}
		};
		template <>
		struct _PAKALExport Delegate<void>
		{
			std::function<void()> callback;
			ulonglong* id;
			std::thread::id tid;

			Delegate(ulonglong& id,const std::function<void()>&& callback, std::thread::id tid = NULL_THREAD) : callback(callback), id(&id)
			{}
		};

		template <class TArgs>
		class _PAKALExport Event
		{
			std::unordered_map<unsigned long long, SharedPtr<DelegateData<TArgs>>> m_delegates;
			bool m_enabled;
			mutable std::mutex m_mutex;

			typedef std::function<void(const TArgs&)> MethodDelegate;
		public:

			explicit Event() : m_enabled(true) {}

			inline void operator+=(Delegate<TArgs>&& d)
			{
				*d.id = add_listener( std::move(d.callback), d.tid);
			}

			inline void operator-=(ulonglong& id)
			{
				remove_listener(id);
				id = 0;
			}

			inline ulonglong add_listener(const MethodDelegate&& delegate, std::thread::id callbackThread = NULL_THREAD)
			{
				mutex_guard lock(m_mutex);
				
				unsigned long long key = EventSystemUtils::new_id();

				SharedPtr<DelegateData<TArgs>> metaData =
					m_delegates.emplace(key, std::make_shared<DelegateData<TArgs>>()).first->second;

				metaData->tid = callbackThread;
				metaData->is_enabled = m_enabled;
				metaData->is_subscribed = true;

				WeakPtr<DelegateData<TArgs>> wMetaData = metaData;

				metaData->delegate = [wMetaData, delegate](const TArgs& args)
				{
					auto metaDataS = wMetaData.lock();
					if (metaDataS->is_subscribed && metaDataS->is_enabled)
						delegate(args);
				};

				return key;
			}

			inline void remove_listener(ulonglong key)
			{
				mutex_guard lock(m_mutex);

				auto it = m_delegates.find(key);

				it->second->is_subscribed = false;

				m_delegates.erase(it);

			}

			inline bool empty() const
			{
				mutex_guard lock(m_mutex);

				return m_delegates.empty();
			}
		
			inline void disable()
			{
				mutex_guard lock(m_mutex);

				m_enabled = false;

				for (auto it : m_delegates) it.second->is_enabled = m_enabled;
			}

			inline void enable()
			{
				mutex_guard lock(m_mutex);

				m_enabled = true;

				for (auto it : m_delegates) it.second->is_enabled = m_enabled;
			}

			inline void clear()
			{
				mutex_guard lock(m_mutex);

				for (auto it : m_delegates) it.second->is_subscribed = false;

				m_delegates.clear();
			}

			inline void operator()(const TArgs& arguments) { notify(arguments); }

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
						EventSystemUtils::execute_in_thread([dd, arguments]() { dd.second->delegate(arguments); }, dd.second->tid);
				}
			}

		};

		template<>
		class _PAKALExport Event<void>
		{
		public:
			typedef std::function<void(void)> MethodDelegate;
		private:

			std::unordered_map<unsigned long long,SharedPtr<DelegateData<void>>> m_delegates;
			bool m_enabled;
			mutable std::mutex m_mutex;

		public:

			explicit Event() : m_enabled(true)
			{
			}

			inline bool empty() const
			{
				mutex_guard lock(m_mutex);

				return m_delegates.empty();
			}

			inline void operator+=(Delegate<void>&& d)
			{
				*d.id = add_listener(std::move(d.callback),d.tid);
			}

			inline void operator-=(ulonglong& id)
			{
				remove_listener(id);
				id = 0;
			}

			/*inline ulonglong add_listener(const MethodDelegate&& delegate, std::thread::id callBackThread = NULL_THREAD)
			{
				return add_listener(delegate, callBackThread);
			}*/

			inline ulonglong add_listener(const MethodDelegate&& delegate, std::thread::id callBackThread = NULL_THREAD)
			{
				mutex_guard lock(m_mutex);

				ulonglong key = EventSystemUtils::new_id();

				auto metaData = m_delegates.emplace(key, std::make_shared<DelegateData<void>>()).first->second;					
				
				metaData->tid = callBackThread;
				metaData->is_enabled = m_enabled;
				metaData->is_subscribed = true;
				WeakPtr<DelegateData<void>> wMetaData = metaData;
				metaData->delegate = [wMetaData,delegate]()
				{
					auto metaDataS = wMetaData.lock();
					// if (metaDataS) // if it crashes here, it means that you are still suscribed to the event.
					if (metaDataS->is_subscribed && metaDataS->is_enabled)
						delegate();
				};

				return key;
			}

			inline void remove_listener(ulonglong key)
			{
				mutex_guard lock(m_mutex);

				auto it = m_delegates.find(key);

				it->second->is_subscribed = false;

				m_delegates.erase(it);
			}
		
			inline void disable()
			{
				mutex_guard lock(m_mutex);
				m_enabled = false;
				for (auto it : m_delegates) it.second->is_enabled = m_enabled;
			}

			inline void enable()
			{
				mutex_guard lock(m_mutex);

				m_enabled = true;

				for (auto it : m_delegates) it.second->is_enabled = m_enabled;

			}

			inline void clear()
			{
				mutex_guard lock(m_mutex);

				for(auto it : m_delegates) it.second->is_subscribed = false;

				m_delegates.clear();
			}

			inline void operator()() const
			{ notify(); }

			void notify() const
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
						EventSystemUtils::execute_in_thread(dd.second->delegate, dd.second->tid);
				}
			}

		};
}