#include "EventSystemUtils.h"
#include "EventScheduler.h"

namespace Pakal
{
	BasicTaskPtr EventSystemUtils::execute_in_thread(EventScheduler* scheduler, const std::function<void()>& fn, const std::thread::id& id)
	{
		return scheduler->execute_in_thread(fn, id);
	}

	unsigned EventSystemUtils::hash_function(unsigned valueToHash) 
	{
		static std::hash<unsigned int> hasher;		
		static unsigned long hash_salt = 0;
		return hasher( valueToHash ^ hash_salt++);
	}
}