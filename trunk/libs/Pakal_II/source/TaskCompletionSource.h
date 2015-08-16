#pragma once

#include "Config.h"
#include "TaskFwd.h"

namespace Pakal
{
	class EventScheduler;

	class _PAKALExport TaskCompletionSource
	{
	public:
		explicit TaskCompletionSource(EventScheduler* scheduler);

		inline BasicTaskPtr get_task();

		inline void set_completed();

	private:
		BasicTaskPtr m_task;
	};
}