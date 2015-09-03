#pragma once
#include "Config.h"

#include "System.h"
#include "IComponentProvider.h"

#include "Event.h"


namespace Pakal
{
	class Engine;
	class IDebugDrawerClient;

	class _PAKALExport PhysicsSystem :  public System, public IComponentProvider
	{
		friend class Engine;

		void on_initialize() override final;
		void on_terminate() override final;
		void on_update(long long dt) override final; 
		void on_pause() override final;
		void on_resume() override final;

	public:
		Event<void> update_event;

		virtual IDebugDrawerClient* get_debug_drawer(){  return nullptr; };
		virtual const char*			get_system_name() override = 0;

	protected:

		explicit PhysicsSystem(bool usesThread);
		virtual ~PhysicsSystem() {};

		virtual void init_world()  {};
		virtual void update_world(long long dt) {};
		virtual void clear_world() {};
		virtual void pause_world() {};
		virtual void resume_world() {};
	};
}