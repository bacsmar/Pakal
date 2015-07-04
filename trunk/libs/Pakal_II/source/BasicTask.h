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

		struct IDelegate
		{ 
			virtual ~IDelegate(){} 

			virtual int getType() = 0;
			enum DELEGATE_TYPE
			{
				DELEGATE_NOARGS,
				DELEGATE_ARGS,
				DELEGATE_NOARGS_NOPARAM,
			};
		};

		template <class ReturnType, class ArgType>
		struct Delegate : public IDelegate
		{
			std::function<ReturnType(ArgType)> f;
			int getType(){ return DELEGATE_ARGS; };
		};

		template <class ReturnType>
		struct DelegateNoArgs : public IDelegate
		{
			std::function<ReturnType()> f;
			int getType(){ return DELEGATE_NOARGS;}
		};

		struct DelegateNoArgsNoParam : public IDelegate
		{
			std::function<void()> f;
			int getType(){ return DELEGATE_NOARGS_NOPARAM;}
		};
		
		template<class TRet,class TArgs>
		void createDelegate( std::function<TRet(TArgs)> & _method )
		{			
			Delegate<TRet, TArgs> *d = new Delegate<TRet, TArgs>();
			d->f = _method;
			onCompletionDo(d);			
		}

		void createDelegate( std::function<void()> & _method )
		{			
			//DelegateNoArgs<void> *d = new DelegateNoArgs<void>();
			DelegateNoArgsNoParam *d = new DelegateNoArgsNoParam();
			d->f = _method;
			onCompletionDo(d);			
		}
		
		virtual void onCompletionDo( IDelegate * delegate ) {};		
					
	protected:					
		virtual void run() = 0;		
	};

	typedef std::shared_ptr<BasicTask> BasicTaskPtr;

}