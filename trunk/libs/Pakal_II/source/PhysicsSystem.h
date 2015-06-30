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
	class IDebugDrawerClient;

	class _PAKALExport PhysicsSystem : public IComponentProvider, public AsyncTaskDispatcher
	{
	private:
		void initialize();
		void terminate();
	protected:		

		Poco::Thread* m_PhysicsThread;
		Poco::RunnableAdapter<PhysicsSystem>* m_entryPoint;	

		void run();

		// from IComponentProvicer
		virtual void registerComponentFactories( std::vector<IComponentFactory*> &factories) override {};
		virtual BasicTask * initComponentAsync(IComponent *c) override { ASSERT(false); return nullptr; };
		virtual BasicTask * terminateComponentAsync(IComponent *c) override { ASSERT(false); return nullptr; };

		// virtual functions
		virtual void update() {};
		virtual void initWorld() {};
		virtual void clearWorld() {};

		virtual IDebugDrawerClient * getDebugDrawer(){  return nullptr; };

		// initialization ^ destruction
		friend class Engine;
		static PhysicsSystem* createPhysicsSystem();
		virtual ~PhysicsSystem();
		PhysicsSystem();

	};
}