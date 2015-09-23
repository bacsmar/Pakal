#pragma once
#include "Config.h"

#include "IComponentProvider.h"
#include "System.h"
#include "math/tm.h"
#include "OSManager.h"

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
		void on_update(long long dt) override final;
		void on_pause() override final;
		void on_resume() override final;		
		
	public:

		virtual void				set_window_caption(const wchar_t*) {};
		virtual void				show_fps(bool) {};
		virtual const char*			get_system_name() override = 0;

		virtual void				add_debug_drawer(IDebugDrawerClient * debugDrawer) = 0;

		struct Settings
		{
			tmath::vector2di resolution;
			int bits;
			bool full_screen;
			bool vsync;

			Settings() : resolution(640,480), bits(32), full_screen(false), vsync(false) {}
		};

	protected:
		Settings m_settings;
		OSManager*	m_os_manager;

		explicit GraphicsSystem(const Settings& settings, OSManager* os_manager) : System(false), m_settings(settings), m_os_manager(os_manager) {  }
		virtual ~GraphicsSystem(){}


		virtual void on_init_graphics(const OSManager::WindowArgs& args) = 0;
		virtual void on_update_graphics(long long dt) = 0;
		virtual void on_terminate_graphics() = 0;
		virtual void on_pause_graphics() = 0;
		virtual void on_resume_graphics() = 0;
	};
}