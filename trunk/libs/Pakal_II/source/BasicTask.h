#pragma once

#include "Config.h"
#include <memory>
#include <functional>

namespace Poco { class Notification; }

namespace Pakal
{
	class EventScheduler;		

	class _PAKALExport BasicTask 
	{
		friend class AsyncTaskDispatcher;
	public:	
		virtual bool isCompleted() = 0;
		virtual void wait() = 0;

		virtual ~BasicTask() {}		

		virtual void onCompletionDo( std::function<void()> & callback ) = 0;

		virtual EventScheduler* getEventScheduler() = 0;
					
	protected:					
		virtual void run() = 0;		
	};

	typedef std::shared_ptr<BasicTask> BasicTaskPtr;

}