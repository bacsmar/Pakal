#include "EventSystemUtils.h"
#include "EventScheduler.h"

namespace Pakal
{
	BasicTaskPtr EventSchedulerHelper::execute_in_thread(const std::function<void()>& fn, const std::thread::id& id)
	{
		return EventScheduler::instance().execute_in_thread(fn, id);
	}

	unsigned long long EventSchedulerHelper::new_id()
	{
		static unsigned long long id = 0;
		return id++;
	}
}