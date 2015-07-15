#pragma once
#include "Config.h"

#include <functional>

#include "TaskFwd.h"


namespace Pakal
{
	class EventScheduler;

	class EventSchedulerUtils
	{
	public:
		static BasicTaskPtr executeInThread(EventScheduler* scheduler, const std::function<void()>& fn, const std::thread::id& id);
	};
}