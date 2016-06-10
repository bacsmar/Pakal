#pragma once

#include <unordered_map>

#include <functional>
#include <mutex>
#include <thread>

#include "EventSystemUtils.h"

namespace Pakal
{
		using EventId = unsigned;

		template <class TArgs>
		struct  DelegateData
		{
			std::function<void(const TArgs&)> delegate;
			std::thread::id tid;
			bool is_subscribed;
			bool is_enabled;
		};
		template<>
		struct  DelegateData<void>
		{
			std::function<void()> delegate;
			std::thread::id tid;
			bool is_subscribed;
			bool is_enabled;
		};

		template <class TArgs>
		struct  Delegate
		{
			std::function<void(const TArgs&)> callback;
			EventId* id;
			std::thread::id tid;

			Delegate(EventId& id, const std::function<void(const TArgs&)>&& callback, std::thread::id tId = NULL_THREAD) : callback(callback), id(&id), tid(tId)
			{}
		};
		template <>
		struct  Delegate<void>
		{
			std::function<void()> callback;
			EventId* id;
			std::thread::id tid;

			Delegate(EventId& id,const std::function<void()>&& callback, std::thread::id tId = NULL_THREAD) : callback(callback), id(&id), tid(tId)
			{}
		};

		template <class TArgs>
		class  Event
		{
			std::unordered_map<EventId, SharedPtr<DelegateData<TArgs>>> m_delegates;
			bool m_enabled;
			mutable std::mutex m_mutex;
			EventId m_delegate_index = 0;
			inline EventId new_id()
			{
				return ++m_delegate_index;
			}

			typedef std::function<void(const TArgs&)> MethodDelegate;
		public:

			explicit Event() : m_enabled(true) {}

			inline void operator+=(Delegate<TArgs>&& d)
			{
				*d.id = add_listener( std::move(d.callback), d.tid);
			}

			inline void operator-=(EventId& id)
			{
				remove_listener(id);
				id = 0;
			}

			inline EventId add_listener(const MethodDelegate&& delegate, std::thread::id callbackThread = NULL_THREAD)
			{
				mutex_guard lock(m_mutex);
				
				auto key = new_id();

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

			inline void remove_listener(EventId key)
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
		class  Event<void>
		{
		public:
			typedef std::function<void(void)> MethodDelegate;
		private:

			std::unordered_map<EventId,SharedPtr<DelegateData<void>>> m_delegates;
			bool m_enabled;
			mutable std::mutex m_mutex;
			EventId m_delegate_index = 0;
			inline EventId new_id()
			{
				return ++m_delegate_index;
			}

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

			inline void operator-=(EventId& id)
			{
				remove_listener(id);
				id = 0;
			}

			inline EventId add_listener(const MethodDelegate&& delegate, std::thread::id callBackThread = NULL_THREAD)
			{
				mutex_guard lock(m_mutex);

				auto key = new_id();

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

			inline void remove_listener(EventId key)
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

			inline void operator()() const {  notify(); }

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
