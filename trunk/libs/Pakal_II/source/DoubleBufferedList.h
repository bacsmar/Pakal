
#pragma once

#include <unordered_set>
#include <list>

#include <deque>
#include <queue>
#include <thread>
#include <mutex>

namespace Pakal
{

	template <class C>
	class DoubleBufferedClass
	{
	protected:

		C	m_push_list;
		C	m_pop_list;
		std::mutex	m_update_mutex;
	public:
		void swap_buffer()
		{
			std::lock_guard<std::mutex> lock(m_update_mutex);
			std::swap(m_push_list, m_pop_list);
		}

		inline bool empty()
		{
			return m_pop_list.empty();
		}
		inline size_t size()
		{
			return m_pop_list.size();
		}

		virtual ~DoubleBufferedClass() {}
		DoubleBufferedClass() {}
	};


	template <class T, class _Container = std::deque<T> >
	class DoubleBufferedQueue : public DoubleBufferedClass< std::queue<T, _Container> >
	{
		using BaseClass = DoubleBufferedClass< std::queue<T, _Container> >;
	public:
		inline void push(T t)
		{
			std::lock_guard<std::mutex> lock(BaseClass::m_update_mutex);
			BaseClass::m_push_list.push(t);
		}
		inline T pop()
		{
			std::lock_guard<std::mutex> lock(BaseClass::m_update_mutex);
			T e = BaseClass::m_pop_list.front();
			BaseClass::m_pop_list.pop();
			return e;
		}
	};

	template <class T>
	class DoubleBufferedSet : public DoubleBufferedClass<std::unordered_set<T> >
	{
		using BaseClass = DoubleBufferedClass<std::unordered_set<T> >;
	public:
		inline void insert(T t)
		{
			std::lock_guard<std::mutex> lock(BaseClass::m_update_mutex);
			BaseClass::m_push_list.insert(t);
		}
		inline void erase(T t)
		{
			std::lock_guard<std::mutex> lock(BaseClass::m_update_mutex);
			BaseClass::m_push_list.erase(t);
		}
	};

}