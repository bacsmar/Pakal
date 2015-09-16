#pragma once
#include "Config.h"

#include "TaskFwd.h"

#include <functional>
#include <thread>


namespace Pakal
{
	class EventScheduler;

	namespace EventSchedulerHelper
	{
		unsigned int _PAKALExport hash_function(unsigned int valueToHash);

		BasicTaskPtr _PAKALExport execute_in_thread(const std::function<void()>& fn, const std::thread::id& id);


	};	
}