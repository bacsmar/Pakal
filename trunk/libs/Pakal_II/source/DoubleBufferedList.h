
#pragma once

#include <mutex>
#include <unordered_set>
#include <queue>
#include <list>
#include <deque>

namespace Pakal
{

	template <class C>
	class DoubleBufferedClass
	{
	protected:
		
		C*	m_push_list;
		C*  m_pop_list;
		std::mutex	m_update_mutex;
	public:
		void swap_buffer()
		{
			std::lock_guard<std::mutex> lock(m_update_mutex);
			std::swap(m_push_list,m_pop_list);
		}

		inline bool empty()
		{
			return m_pop_list->empty();
		}
		inline size_t size()
		{
			return m_pop_list->size();
		}	

		virtual ~DoubleBufferedClass()
		{
			ASSERT(m_push_list->empty());
			ASSERT(m_pop_list->empty());

			delete m_push_list;
			delete m_pop_list;

			m_push_list = nullptr;
			m_pop_list = nullptr;
		}
		DoubleBufferedClass() 
		{
			m_push_list = new C();
			m_pop_list  = new C();
		}
	};


	template <class T, class _Container = std::deque<T> >
	class DoubleBufferedQueue : public DoubleBufferedClass<std::queue<T, _Container> >
	{	
	public:	
		inline void push( T t)
		{
			std::lock_guard<std::mutex> lock(m_update_mutex);
			m_push_list->push(t);
		}
		inline T pop()
		{
			std::lock_guard<std::mutex> lock(m_update_mutex);
			T e =   m_pop_list->front();
			m_pop_list->pop();
			return e;
		}
	};

}