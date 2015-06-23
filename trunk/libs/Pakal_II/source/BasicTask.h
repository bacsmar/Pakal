#pragma once

#include "Config.h"
#include <Poco/Notification.h>


namespace Pakal
{

class _PAKALExport BasicTask : public Poco::Notification
{
	friend class AsyncTaskDispatcher;

public:

	virtual bool isCompleted() = 0;
	virtual void wait() = 0;

protected:
	virtual void run() = 0;

};


}