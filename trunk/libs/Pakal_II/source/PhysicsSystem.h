#pragma once
#include "Config.h"

#include "IComponentProvider.h"
#include "AsyncTaskDispatcher.h"



//#include "Task.h"

namespace Poco
{
	class Thread;

	template <class C>
	class RunnableAdapter;
}

namespace Pakal
{
	class Engine;

	class _PAKALExport PhysicsSystem : public IComponentProvider, public AsyncTaskDispatcher
	{
	private:
		void initialize();
		void terminate();
	protected:

		friend class Engine;

		Poco::Thread* m_PhysicsThread;
		Poco::RunnableAdapter<PhysicsSystem>* m_entryPoint;		

		static PhysicsSystem* createPhysicsSystem();

		virtual void registerComponentFactories( std::vector<IComponentFactory*> &factories) {};
		virtual void run();

		//Poco::AutoPtr<Task<int>>  getNumberOfBodiesAsync()
		//{
		//	std::function<int()> lambda = []()
		//	{
		//		return 555;
		//	};

		//	return getInbox()->pushTask(lambda);
		//}

		virtual ~PhysicsSystem();
		PhysicsSystem();
	};
}