#pragma once

#include <functional>
#include "TaskFwd.h"
#include <Poco/Thread.h>

namespace Pakal
{
	class EventScheduler;		

	class _PAKALExport BasicTask 
	{
		friend class AsyncTaskDispatcher;
	protected:
		virtual void run() = 0;			

	public:

		virtual void onCompletionDo(const std::function<void()>  &callback, unsigned long callBackThread = Poco::Thread::currentTid() ) = 0;
		virtual bool isCompleted() = 0;
		virtual void wait() = 0;

		virtual ~BasicTask() {}	

		virtual EventScheduler* getEventScheduler() = 0;
	
	};

}