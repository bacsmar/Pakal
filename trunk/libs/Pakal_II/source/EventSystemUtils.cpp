#include "EventSystemUtils.h"
#include "EventScheduler.h"

namespace Pakal
{
	BasicTaskPtr EventSchedulerHelper::execute_in_thread(const std::function<void()>& fn, const std::thread::id& id)
	{
		return EventScheduler::instance().execute_in_thread(fn, id);
	}

	unsigned EventSchedulerHelper::hash_function(unsigned valueToHash) 
	{
		static std::hash<unsigned int> hasher;		
		static unsigned long hash_salt = 0;
		return hasher( valueToHash ^ hash_salt++);
	}
}