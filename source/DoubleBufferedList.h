
#pragma once

#include <unordered_set>

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
		std::mutex	m_pop_mutex, m_push_mutex, m_block_mutex;

		std::condition_variable m_condition;
	public:
		void swap_buffer()
		{
			mutex_guard lock(m_pop_mutex);
			mutex_guard lock2(m_push_mutex);
			std::swap(m_push_list, m_pop_list);
		}

		inline bool empty()
		{
			mutex_guard lock(m_pop_mutex);
			return m_pop_list.empty();
		}
		inline size_t size()
		{
			mutex_guard lock(m_pop_mutex);
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
			mutex_guard lock(BaseClass::m_push_mutex);
			BaseClass::m_push_list.push(t);
			BaseClass::m_condition.notify_one();
		}

		inline T pop(bool block)
		{
			if (block && BaseClass::empty())
			{
				std::unique_lock<std::mutex> lk(BaseClass::m_block_mutex);
				BaseClass::m_condition.wait(lk, [this]() { return !BaseClass::m_push_list.empty(); });
				BaseClass::swap_buffer();
			}

			mutex_guard lock(BaseClass::m_pop_mutex);
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
			mutex_guard lock(BaseClass::m_push_mutex);
			BaseClass::m_push_list.insert(t);
		}
		inline void erase(T t)
		{
			mutex_guard lock(BaseClass::m_pop_mutex);
			BaseClass::m_pop_list.erase(t);
		}
	};

}