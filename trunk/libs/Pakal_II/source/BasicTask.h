#pragma once

#include <functional>
#include "TaskFwd.h"

namespace Pakal
{
	class EventScheduler;		

	class _PAKALExport BasicTask 
	{
		friend class AsyncTaskDispatcher;
	protected:
		virtual void run() = 0;			

	public:

		virtual void onCompletionDo( std::function<void()>  &callback, bool executeInThisThread = true) = 0;
		virtual bool isCompleted() = 0;
		virtual void wait() = 0;

		virtual ~BasicTask() {}				

		virtual EventScheduler* getEventScheduler() = 0;
	
	};

}