#pragma once
#include "GraphicsSystem.h"

#include <irrlicht.h>
#include <string>

#ifdef _IRR_WINDOWS_
	#pragma comment(lib, "Irrlicht.lib")
//#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

namespace Pakal
{
	class BasicTask;
	class IComponent;
	class IDebugDrawerClient;
	struct RendererInfo;	

	class _PAKALExport IrrGraphicsSystem final : public GraphicsSystem
	{
	public:
		inline irr::IrrlichtDevice		* get_device() const { return device;	}
		inline irr::video::IVideoDriver	* get_driver() const { return driver;	}
		inline irr::scene::ISceneManager* get_smgr() const	{ return smgr;		}
		inline irr::gui::IGUIEnvironment* get_guienv() const { return guienv;	}
		inline const char* get_system_name() override { return "IrrGraphicsSystem";  };

		IrrGraphicsSystem(EventScheduler* scheduler);

	protected:

		bool m_is_rendering;				

		std::string m_home_path;
		std::string m_config_path;
		size_t		m_window;

		irr::IrrlichtDevice			* device;		
		irr::video::IVideoDriver	* driver;		
		irr::scene::ISceneManager	* smgr;		
		irr::gui::IGUIEnvironment	* guienv;		

		RendererInfo				*m_render_info;
		std::vector<IDebugDrawerClient*>	m_debug_renderers;		

		virtual ~IrrGraphicsSystem() {};

		void on_init_graphics() override;
		void on_terminate_graphics() override;
		void on_update_graphics() override;
		void on_pause_graphics() override;
		void on_resume_graphics() override;


		void init_window();

		void begin_scene();
		bool draw();
		void end_scene();

		virtual void set_window_caption(const wchar_t* caption) override;
		virtual void show_fps( bool val ) override;

		virtual void register_component_factories( std::vector<IComponentFactory*> &factories ) override;
		virtual void add_debug_drawer(IDebugDrawerClient * debugDrawer) override;

		bool m_showFps;
	};	
}