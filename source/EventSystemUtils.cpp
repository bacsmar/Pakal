#include "EventSystemUtils.h"
#include "EventScheduler.h"

namespace Pakal
{
	BasicTaskPtr EventSystemUtils::executeInThread(EventScheduler* scheduler, const std::function<void()>& fn, const std::thread::id& id)
	{
		return scheduler->executeInThread(fn, id);
	}

	unsigned EventSystemUtils::hashFunction(unsigned valueToHash) 
	{
		static std::hash<unsigned int> hasher;		
		static unsigned long hash_salt = 0;
		return hasher( valueToHash ^ hash_salt++);
	}
}