#include "EventSystem.h"

#include "Poco/SingletonHolder.h"
#include "LogMgr.h"

#include "Poco/NotificationQueue.h"

using namespace Poco;

Pakal::EventSystem::EventSystem() : mNotificationQueue( new NotificationQueue() )
{

}
