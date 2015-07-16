#pragma once
#include "Config.h"

#include <functional>

#include "TaskFwd.h"


namespace Pakal
{
	class EventScheduler;

	namespace EventSystemUtils
	{
		unsigned int hashFunction(unsigned int valueToHash);

		BasicTaskPtr executeInThread(EventScheduler* scheduler, const std::function<void()>& fn, const std::thread::id& id);


	};	
}