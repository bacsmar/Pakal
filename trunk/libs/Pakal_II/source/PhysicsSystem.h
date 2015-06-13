#pragma once
#include "Config.h"

#include "IComponentProvider.h"

namespace Poco
{
	class Thread;

	template <class C>
	class RunnableAdapter;
}

namespace Pakal
{
	class Engine;

	class _PAKALExport PhysicsSystem : public IComponentProvider
	{
	private:
		void initialize();
		void terminate();
	protected:

		friend class Engine;

		Poco::Thread* m_PhysicsThread;
		Poco::RunnableAdapter<PhysicsSystem>* m_entryPoint;		

		static PhysicsSystem* createPhysicsSystem();

		virtual void registerComponentFactories( std::vector<IComponentFactory*> &componentVector) {};
		virtual void run();

		virtual ~PhysicsSystem();
		PhysicsSystem();
	};
}