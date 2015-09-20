#pragma once

#include "Config.h"
#include "TaskFwd.h"
#include "Task.h"


namespace Pakal
{
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

	template <class T>
	class _PAKALExport TaskCompletionSource
	{
		using TaskPtr = std::shared_ptr<Task<T>>;

	public:
		explicit TaskCompletionSource()
		{
			m_task = std::make_shared <Task<T>>([this]() { return m_result; });
		};

		inline TaskPtr get_task()
		{
			return m_task;
		};

		inline void set_completed(const T& result)
		{
			m_result = result;
			m_task->run();
		};

	private:
		T m_result;
		TaskPtr m_task;
	};

}