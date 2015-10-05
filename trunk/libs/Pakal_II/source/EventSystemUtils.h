#pragma once
#include "Config.h"

#include "TaskFwd.h"

#include <functional>
#include <thread>


namespace Pakal
{
	class EventScheduler;

	namespace EventSystemUtils
	{
		unsigned long long _PAKALExport new_id();

		BasicTaskPtr _PAKALExport execute_in_thread(const std::function<void()>& fn, const std::thread::id& id);

		BasicTaskPtr _PAKALExport execute_in_worker(const std::function<void()>& fn);

	};	
}