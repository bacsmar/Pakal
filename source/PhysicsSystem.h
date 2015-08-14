#pragma once
#include "Config.h"

#include "System.h"
#include "IComponentProvider.h"
#include "AsyncTaskDispatcher.h"

#include "Event.h"


namespace Pakal
{
	class Engine;
	class IDebugDrawerClient;

	class _PAKALExport PhysicsSystem :  public System, public AsyncTaskDispatcher, public IComponentProvider
	{
		friend class Engine;
	public:
		Event<bool> update_event;

		static PhysicsSystem* createInstance();

		virtual IDebugDrawerClient* get_debug_drawer(){  return nullptr; };
		virtual void				register_component_factories( std::vector<IComponentFactory*> &factories) override {};

	protected:

		PhysicsSystem() : System(PAKAL_USE_THREADS == 1) {};
		virtual ~PhysicsSystem() {};

		virtual void on_initialize() override;
		virtual void on_terminate() override;
		virtual void on_update() override;
		
		virtual void init_world() {};
		virtual void clear_world() {};
		
	};
}