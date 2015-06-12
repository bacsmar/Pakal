#include "EventSystem.h"

#include "Poco/SingletonHolder.h"

#include "Poco/NotificationQueue.h"

using namespace Poco;

Pakal::EventSystem::EventSystem() : m_NotificationQueue( new NotificationQueue() )
{

}

void Pakal::EventSystem::initialize() {}