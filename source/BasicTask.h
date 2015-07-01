#pragma once

#include "Config.h"
#include <Poco/Notification.h>

namespace Pakal
{
	class EventScheduler;

class _PAKALExport BasicTask : public Poco::Notification
{
	friend class AsyncTaskDispatcher;	
public:	
	virtual bool isCompleted() = 0;
	virtual void wait() = 0;

	virtual ~BasicTask() {}		

	virtual void onCompletionDo( std::function<void()> & callback ) = 0;	

	virtual EventScheduler* getEventScheduler() =0;

protected:	
	virtual void run() = 0;	
};

typedef Poco::AutoPtr<BasicTask> BasicTaskPtr;

}