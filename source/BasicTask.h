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
	protected:
		struct IDelegate
		{ 
			virtual ~IDelegate(){} 

			virtual int getType() = 0;
			enum DELEGATE_TYPE
			{
				DELEGATE_NOARGS_RETURNS_T,
				DELEGATE_ARGS_RETURNS_T,
				DELEGATE_NOARGS_RETURNS_VOID,
			};
		};

		template <class ReturnType, class ArgType>
		struct Delegate : public IDelegate
		{
			std::function<ReturnType(ArgType)> f;
			int getType(){ return DELEGATE_ARGS_RETURNS_T; };
		};

		template <class ReturnType>
		struct DelegateNoArgs : public IDelegate
		{
			std::function<ReturnType()> f;
			int getType(){ return DELEGATE_NOARGS_RETURNS_T;}
		};

		struct DelegateNoArgsNoParam : public IDelegate
		{
			std::function<void()> f;
			int getType(){ return DELEGATE_NOARGS_RETURNS_VOID;}
		};

		virtual void onCompletionDo( IDelegate * delegate ) = 0;
		virtual void run() = 0;			

	public:

		template<class TRet,class TArgs>
		void onCompletionDo( std::function<TRet(TArgs)> & _method )
		{			
			Delegate<TRet, TArgs> *d = new Delegate<TRet, TArgs>();
			d->f = _method;
			onCompletionDo( (IDelegate*)d );
			delete d;
		}

		void onCompletionDo( std::function<void()> & _method )
		{						
			DelegateNoArgsNoParam *d = new DelegateNoArgsNoParam();
			d->f = _method;
			onCompletionDo( (IDelegate*)d  );
			delete d;
		}
		template<class TReturn>
		void onCompletionDo( std::function<TReturn()> & _method )
		{			
			DelegateNoArgs<TReturn> *d = new DelegateNoArgs<TReturn>();
			d->f = _method;
			onCompletionDo( (IDelegate*)d );
			delete d;
		}

		virtual bool isCompleted() = 0;
		virtual void wait() = 0;

		virtual ~BasicTask() {}				

		virtual EventScheduler* getEventScheduler() = 0;

	protected:		
	
	};

	typedef std::shared_ptr<BasicTask> BasicTaskPtr;

}