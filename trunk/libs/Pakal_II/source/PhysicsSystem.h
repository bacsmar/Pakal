#pragma once
#include "Config.h"

#include "System.h"
#include "IComponentProvider.h"

#include "Event.h"
#include "math/tm.h"

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
		Event<void> updated;

		virtual IDebugDrawerClient* get_debug_drawer(){  return nullptr; };
		virtual const char*			get_system_name() override = 0;


		struct Settings
		{
			tmath::vector3df gravity;
			bool allow_sleep;
			int iteration_velocity;
			int iteration_position;
			bool uses_thread;

			Settings() : gravity(0.f,-9.82f,0.f), allow_sleep(true), iteration_velocity(8), iteration_position(3), uses_thread(true) {}
		};

	protected:
		Settings m_settings;

		explicit PhysicsSystem(const Settings& settings);
		virtual ~PhysicsSystem() {};

		virtual void init_world()  {};
		virtual void update_world(long long dt) {};
		virtual void clear_world() {};
		virtual void pause_world() {};
		virtual void resume_world() {};
	};
}