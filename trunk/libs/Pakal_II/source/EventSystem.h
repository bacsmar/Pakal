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
	public:
		EventSystem();
		void initialize();
	protected:
		Poco::NotificationQueue *m_NotificationQueue;
	};
}