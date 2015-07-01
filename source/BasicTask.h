#pragma once

#include "Config.h"
#include <memory>
#include <functional>

namespace Poco { class Notification; }

namespace Pakal
{
	class EventScheduler;	
	template <class T>
	class Task;

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

		Poco::Notification *m_Notification;
		BasicTask(Poco::Notification *Notification) : m_Notification( Notification) { }
	};

	typedef std::shared_ptr<BasicTask> BasicTaskPtr;

}