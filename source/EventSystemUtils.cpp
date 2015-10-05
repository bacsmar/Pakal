#include "EventSystemUtils.h"
#include "EventScheduler.h"

namespace Pakal
{
	BasicTaskPtr EventSystemUtils::execute_in_thread(const std::function<void()>& fn, const std::thread::id& id)
	{
		return EventScheduler::instance().execute_in_thread(fn, id);
	}

	BasicTaskPtr EventSystemUtils::execute_in_worker(const std::function<void()>& fn)
	{
		return EventScheduler::instance().execute_in_worker(fn);
	}

	unsigned long long EventSystemUtils::new_id()
	{
		static unsigned long long id = 0;
		return id++;
	}
}