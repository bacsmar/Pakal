
#pragma once

#include <mutex>
#include <unordered_set>
#include <queue>
#include <list>
#include <deque>

namespace Pakal
{

	template <class T, class C>
	class DoubleBufferedClass
	{
	protected:
		static const int MAX_INITIALIZATION_QUEUES = 2;
		C	m_lists[MAX_INITIALIZATION_QUEUES];
		C*	m_update_list;
		std::mutex	m_update_mutex;
		int			m_active_update_list;
	public:
		C& get_list_to_process()
		{
			m_update_mutex.lock();
			int queueToProcess = m_active_update_list;
			m_active_update_list = (m_active_update_list + 1) % MAX_INITIALIZATION_QUEUES;
			m_update_list = &m_lists[m_active_update_list];
			m_update_mutex.unlock();
			return m_lists[queueToProcess];
		}

		inline bool empty()
		{
			return m_update_list->empty();
		}
		inline size_t size()
		{
			return m_update_list->size();
		}	

		virtual ~DoubleBufferedClass(){}
		DoubleBufferedClass() : m_update_list(nullptr) , m_active_update_list(0)
		{
			m_update_list = &m_lists[m_active_update_list];
		}
	};

	template <class T>
	class DoubleBufferedSet : public DoubleBufferedClass<T, std::unordered_set<T> >
	{	
	public:
		inline void insert( T t)
		{
			std::lock_guard<std::mutex> lock(m_update_mutex);
			m_update_list->insert(t);
		}
		inline void erase( T t)
		{
			std::lock_guard<std::mutex> lock(m_update_mutex);
			m_update_list->erase(t);
		}
	};

	template <class T, class _Container = std::deque<T> >
	class DoubleBufferedQueue : public DoubleBufferedClass<T, std::queue<T, _Container> >
	{	
	public:	
		inline void push( T t)
		{
			std::lock_guard<std::mutex> lock(m_update_mutex);
			m_update_list->push(t);
		}
		inline void pop()
		{
			std::lock_guard<std::mutex> lock(m_update_mutex);
			m_update_list->pop();
		}
		inline T front()
		{
			std::lock_guard<std::mutex> lock(m_update_mutex);
			return m_update_list->front();
		}
	};

}