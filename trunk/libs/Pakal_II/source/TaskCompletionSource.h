#pragma once

#include "Config.h"
#include "TaskFwd.h"
#include "Task.h"


namespace Pakal
{
	template <class T>
	class _PAKALExport TaskCompletionSource
	{

	public:
		explicit TaskCompletionSource()
		{
			reset();
		};
		
		inline TaskPtr<T> get_task()
		{
			return m_task;
		};

		inline void set_completed(const T& result)
		{
			m_result = result;
			m_task->run();
		};
		~TaskCompletionSource()
		{

		}

		TaskCompletionSource& operator=(TaskCompletionSource&& other) // move assignment
		{			
			reset();
			return *this;
		}

	private:

		inline void reset()
		{
			m_task = std::make_shared<Task<T>>([this](){ return m_result; });
			m_result = {};
		}

		T m_result;
		TaskPtr<T> m_task;		
	};

	template<>
	class _PAKALExport TaskCompletionSource<void>
	{
	public:
		explicit TaskCompletionSource()
		{
			static auto emptyDelegate = []() {};
			m_task = std::make_shared<BasicTask>(emptyDelegate);
		};

		inline BasicTaskPtr get_task()
		{
			return m_task;
		};

		inline void set_completed()
		{
			m_task->run();
		};

	private:
		BasicTaskPtr m_task;
	};

}