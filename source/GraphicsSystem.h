#pragma once
#include "Config.h"

#include "IComponentProvider.h"
#include "System.h"

namespace Pakal
{
	class IDebugDrawerClient;
	class GraphicComponent;

	class _PAKALExport GraphicsSystem : public System, public IComponentProvider
	{
		friend class Engine;
		friend class GraphicComponent;

		void on_initialize() override final;
		void on_terminate() override final;
		void on_update() override final;
		void on_pause() override final;
		void on_resume() override final;		
		
	public:
		static GraphicsSystem* create_instance(EventScheduler* scheduler);
		
		virtual void				set_window_caption(const wchar_t*) {};
		virtual void				show_fps(bool) {};
		virtual const char*			get_system_name() override = 0;

		virtual void				add_debug_drawer(IDebugDrawerClient * debugDrawer) = 0;
	protected:

		GraphicsSystem(EventScheduler* scheduler) : System(scheduler,false) {}
		virtual ~GraphicsSystem(){}


		virtual void on_init_graphics() = 0;
		virtual void on_update_graphics() = 0;
		virtual void on_terminate_graphics() = 0;
		virtual void on_pause_graphics() = 0;
		virtual void on_resume_graphics() = 0;

		void register_component_factories(std::vector<IComponentFactory*>& factories) override = 0;
	};
}