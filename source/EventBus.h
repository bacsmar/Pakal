///////////////////////////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 The PakalEngine Open Source Project
// File: EventBus.h
// Purpose: Typed events with any number of listeners, kept in a queue and delivered when the owner
//          says (a simulation delivers at the end of its step). The simulation emits what happened (a
//          shot, a blast, a sound); the presentation, the audio and tests listen, and none of them can
//          change the step while it runs. An event type is any copyable struct.
///////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <typeindex>
#include <utility>
#include <vector>

namespace Pakal
{
	class EventBus
	{
	public:
		using Id = uint32_t;

		EventBus() = default;
		EventBus(const EventBus&) = delete;
		EventBus& operator=(const EventBus&) = delete;

		// Calls 'handler' for each event of type E delivered from now on. Returns its id for unsubscribe().
		template <class E>
		Id subscribe(std::function<void(const E&)> handler)
		{
			const Id id = m_nextId++;
			m_listeners[std::type_index(typeid(E))].push_back({ id, [handler = std::move(handler)](const void* event)
			{
				handler(*static_cast<const E*>(event));
			} });
			return id;
		}

		// The handler is not called again (also when it is unsubscribed while events are delivered)
		void unsubscribe(Id id)
		{
			for (auto& [type, listeners] : m_listeners)
			{
				for (auto& listener : listeners)
				{
					if (listener.id == id) listener.call = nullptr;
				}
			}
			m_pruned = false;
		}

		// Queues an event for the next deliver(). One of a type nobody listens to when it is emitted is dropped.
		template <class E>
		void emit(E event)
		{
			const std::type_index type(typeid(E));
			if (!listened(type)) return;
			m_queue.push_back({ type, [event = std::move(event)](const Listener& listener) { listener.call(&event); } });
		}

		bool listened_to(const std::type_info& type) const { return listened(std::type_index(type)); }
		template <class E>
		bool listened_to() const { return listened(std::type_index(typeid(E))); }

		// Gives the queued events to their listeners, in the order they were emitted. Events emitted meanwhile are
		// delivered too, after them.
		void deliver()
		{
			while (!m_queue.empty())
			{
				std::vector<Queued> delivering;
				delivering.swap(m_queue);
				for (const Queued& queued : delivering)
				{
					auto found = m_listeners.find(queued.type);
					if (found == m_listeners.end()) continue;
					// by index: a handler may subscribe another listener meanwhile
					for (size_t i = 0; i < found->second.size(); ++i)
					{
						const Listener listener = found->second[i];
						if (listener.call) queued.give(listener);
					}
				}
			}
			prune();
		}

		// Drops the events not delivered yet
		void clear() { m_queue.clear(); }
		size_t queued() const { return m_queue.size(); }

	private:
		struct Listener
		{
			Id id;
			std::function<void(const void*)> call;
		};
		struct Queued
		{
			std::type_index type;
			std::function<void(const Listener&)> give;
		};

		bool listened(const std::type_index& type) const
		{
			auto found = m_listeners.find(type);
			if (found == m_listeners.end()) return false;
			for (const auto& listener : found->second)
			{
				if (listener.call) return true;
			}
			return false;
		}

		// Forgets the listeners unsubscribed since the last delivery
		void prune()
		{
			if (m_pruned) return;
			for (auto& [type, listeners] : m_listeners)
			{
				std::erase_if(listeners, [](const Listener& listener) { return !listener.call; });
			}
			m_pruned = true;
		}

		std::map<std::type_index, std::vector<Listener>> m_listeners;
		std::vector<Queued> m_queue;
		Id m_nextId = 1;
		bool m_pruned = true;
	};
}
