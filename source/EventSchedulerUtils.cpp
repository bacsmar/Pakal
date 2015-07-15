#include "EventSchedulerUtils.h"
#include "EventScheduler.h"

namespace Pakal
{
	BasicTaskPtr EventSchedulerUtils::executeInThread(EventScheduler* scheduler, const std::function<void()>& fn, const std::thread::id& id)
	{
		return scheduler->executeInThread(fn, id);
	}
}