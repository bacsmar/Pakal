#pragma once
#include "Config.h"

#include "IComponentProvider.h"
#include "AsyncTaskDispatcher.h"

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
		void run();
		virtual void update() {};		

		virtual ~PhysicsSystem();
		PhysicsSystem();
	};
}