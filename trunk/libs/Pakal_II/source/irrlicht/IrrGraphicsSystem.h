#pragma once
#include "GraphicsSystem.h"

#ifdef _WINDOWS
#pragma comment(lib, "Irrlicht.lib")
//#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#else
#pragma clang diagnostic ignored "-Winconsistent-missing-override"
#endif

#include <irrlicht.h>
#include <string>
#include "ResourceManager.h"

#include "OSManager.h"

namespace Pakal
{
	class BasicTask;
	class Component;
	class IDebugDrawerClient;
	struct RendererInfo;
	class OSManager;

	class _PAKALExport IrrGraphicsSystem final : public GraphicsSystem
	{		
	public:
		inline irr::IrrlichtDevice		* get_device() const { return device;	}
		inline irr::video::IVideoDriver	* get_driver() const { return driver;	}
		inline irr::scene::ISceneManager* get_smgr() const	{ return smgr;		}
		inline irr::gui::IGUIEnvironment* get_guienv() const { return guienv;	}
		inline const char* get_system_name() override { return "IrrGraphicsSystem";  };	

		explicit IrrGraphicsSystem(const Settings& settings, OSManager* windowManager, IInputManager* input_manager);

	protected:	
		
		unsigned long long m_resized_callback_id;
		unsigned long long m_destroyed_callback_id;
		unsigned long long m_created_callback_id;
		bool				m_draw_axis = false;

		irr::IrrlichtDevice			* device = nullptr;
		irr::video::IVideoDriver	* driver = nullptr;
		irr::scene::ISceneManager	* smgr = nullptr;
		irr::gui::IGUIEnvironment	* guienv = nullptr;

		RendererInfo				*m_render_info;
		std::vector<IDebugDrawerClient*>	m_debug_renderers;		
		virtual ~IrrGraphicsSystem();

		void on_init_graphics(const OSManager::WindowArgs& args) override;
		void on_terminate_graphics() override;
		void on_update_graphics(long long dt) override;
		void on_pause_graphics() override;
		void on_resume_graphics() override;

		void begin_scene();
		void draw_axis();
		void draw();
		void end_scene();

		virtual void set_window_caption(const wchar_t* caption) override;

		virtual void register_component_factories( std::vector<IComponentFactory*> &factories ) override;
		virtual void add_debug_drawer(IDebugDrawerClient * debugDrawer) override;
	};	
}