#pragma once

#include "Config.h"
#include "TaskFwd.h"
#include "BasicTask.h"


namespace Pakal
{
	class EventScheduler;

	class _PAKALExport TaskCompletionSource
	{
	public:
		explicit TaskCompletionSource(EventScheduler* scheduler);

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