#pragma once
#include "Config.h"

#include "IComponentProvider.h"
#include "System.h"
#include "Event.h"

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

		virtual void				set_window_caption(const wchar_t*) {};
		virtual void				show_fps(bool) {};
		virtual const char*			get_system_name() override = 0;

		virtual void				add_debug_drawer(IDebugDrawerClient * debugDrawer) = 0;

		Event<void> terminate_requested;
	protected:

		explicit GraphicsSystem(bool usesThread) : System(usesThread) {  }
		virtual ~GraphicsSystem(){}


		virtual void on_init_graphics() = 0;
		virtual void on_update_graphics() = 0;
		virtual void on_terminate_graphics() = 0;
		virtual void on_pause_graphics() = 0;
		virtual void on_resume_graphics() = 0;
	};
}