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

		virtual EventScheduler* getEventScheduler() = 0;

		virtual void onCompletionDo( std::function<void()> & callback ) = 0;

		struct IDelegate{ virtual ~IDelegate(){} };

		template <class ReturnType, class ArgType>
		struct Delegate : public IDelegate
		{
			std::function<ReturnType(ArgType)> f;			
		};

		template <class ReturnType>
		struct DelegateNoArgs : public IDelegate
		{
			std::function<ReturnType()> f;
		};

		template <class ReturnType, class ParamType>
		IDelegate* createDelegate( ReturnType (*_method)(ParamType _params))
		{			
			Delegate<ReturnType, ParamType> *d = new Delegate<ReturnType, ParamType>();
			d->f = _method;
			onCompletionDo(d);
			return d;
		}		

		IDelegate* createDelegate( void (*_method)() )
		{			
			DelegateNoArgs<void> *d = new DelegateNoArgs<void>();
			d->f = _method;
			onCompletionDo(d);
			return d;
		}
		
		template<class TRet,class TArgs>
		IDelegate* createDelegate( std::function<TRet(TArgs)> & _method )
		{			
			Delegate<TRet, TArgs> *d = new Delegate<TRet, TArgs>();
			d->f = _method;
			onCompletionDo(d);
			return d;
		}

		IDelegate* createDelegate( std::function<void()> & _method )
		{			
			DelegateNoArgs<void> *d = new DelegateNoArgs<void>();
			d->f = _method;
			onCompletionDo(d);
			return d;
		}
		
		virtual void onCompletionDo( IDelegate * callback ) {};		
					
	protected:					
		virtual void run() = 0;		
	};

	typedef std::shared_ptr<BasicTask> BasicTaskPtr;

}