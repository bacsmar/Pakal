#pragma once
#include "Config.h"

#include "System.h"
#include "IComponentProvider.h"

#include "PakalMath.h"

namespace Pakal
{
	class Engine;
	class IDebugDrawerClient;
	class Archive;

	class _PAKALExport PhysicsSystem :  public System, public IComponentProvider
	{
		friend class Engine;

		void on_initialize() override final;
		void on_terminate() override final;
		void on_update(unsigned long dt) override final; 
		void on_pause() override final;
		void on_resume() override final;

	public:
		virtual IDebugDrawerClient* get_debug_drawer(){  return nullptr; };


		struct Settings
		{
			tmath::vector3df gravity = {0.f, -9.82f,0.f};
			bool allow_sleep = false;
			int velocity_iterations = 8;
			int position_iterations = 3;
			bool uses_thread = true;
			bool debug_draw = false;
			bool warm_starting = true;
			bool continuous_physics = true;
			unsigned max_fps = 1000;			

			void persist(Archive* archive);

		};

	protected:
		Settings m_settings;

		explicit PhysicsSystem(const Settings& settings);
		virtual ~PhysicsSystem();;

		virtual void init_world()  {};
		virtual void update_world(unsigned long dt) {};
		virtual void clear_world() {};
		virtual void pause_world() {};
		virtual void resume_world() {};
	};
}