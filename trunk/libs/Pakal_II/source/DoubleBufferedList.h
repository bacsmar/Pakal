
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
		C	m_Lists[MAX_INITIALIZATION_QUEUES];
		C*	m_updateList;
		std::mutex	m_UpdateMutex;
		int			m_ActiveUpdateList;
	public:
		C& getListToProcess()
		{
			m_UpdateMutex.lock();
			int queueToProcess = m_ActiveUpdateList;
			m_ActiveUpdateList = (m_ActiveUpdateList + 1) % MAX_INITIALIZATION_QUEUES;
			m_updateList = &m_Lists[m_ActiveUpdateList];
			m_UpdateMutex.unlock();
			return m_Lists[queueToProcess];
		}

		inline bool empty()
		{
			return m_updateList->empty();
		}
		inline size_t size()
		{
			return m_updateList->size();
		}	

		virtual ~DoubleBufferedClass(){}
		DoubleBufferedClass() : m_ActiveUpdateList(0) , m_updateList(0)
		{
			m_updateList = &m_Lists[m_ActiveUpdateList];
		}
	};

	template <class T>
	class DoubleBufferedSet : public DoubleBufferedClass<T, std::unordered_set<T> >
	{	
	public:
		inline void insert( T t)
		{
			std::lock_guard<std::mutex> lock(m_UpdateMutex);
			m_updateList->insert(t);
		}
		inline void erase( T t)
		{
			std::lock_guard<std::mutex> lock(m_UpdateMutex);
			m_updateList->erase(t);
		}
	};

	template <class T, class _Container = std::deque<T> >
	class DoubleBufferedQueue : public DoubleBufferedClass<T, std::queue<T, _Container> >
	{	
	public:	
		inline void push( T t)
		{
			std::lock_guard<std::mutex> lock(m_UpdateMutex);
			m_updateList->push(t);
		}
		inline void pop()
		{
			std::lock_guard<std::mutex> lock(m_UpdateMutex);
			m_updateList->pop();
		}
		inline T front()
		{
			std::lock_guard<std::mutex> lock(m_UpdateMutex);
			return m_updateList->front();
		}
	};

}