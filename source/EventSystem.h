#pragma once
#include "Config.h"

namespace Poco
{
	class NotificationCenter;
	class NotificationQueue;
}

namespace Pakal
{
	
	class _PAKALExport EventSystem
	{
		EventSystem();
	protected:
		Poco::NotificationQueue *mNotificationQueue;
	};
}