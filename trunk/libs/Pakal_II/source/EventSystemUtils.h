#pragma once
#include "Config.h"

#include <functional>

#include "TaskFwd.h"


namespace Pakal
{
	class EventScheduler;

	namespace EventSystemUtils
	{
		unsigned int hash_function(unsigned int valueToHash);

		BasicTaskPtr execute_in_thread(EventScheduler* scheduler, const std::function<void()>& fn, const std::thread::id& id);


	};	
}