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

namespace Pakal
{
	class MaterialManager;
	class BasicTask;
	class Component;
	class IDebugDrawerClient;
	struct RendererInfo;
	class OSManager;
	class SceneNodeBatcher;
	class SpriteBatcher;

	class _PAKALExport IrrGraphicsSystem final : public GraphicsSystem
	{		
	public:
		inline irr::IrrlichtDevice		* get_device() const { return device;	}
		inline irr::video::IVideoDriver	* get_driver() const { return driver;	}
		inline irr::scene::ISceneManager* get_smgr() const	{ return smgr;		}
		inline irr::gui::IGUIEnvironment* get_guienv() const { return guienv;	}
		inline MaterialManager*				get_material_manager() const { return m_material_manager; }
		inline const char* get_system_name() override { return nameof(IrrGraphicsSystem);  }

		inline SceneNodeBatcher*		get_batcher() const { return m_batch_scene; }
		inline SpriteBatcher*			get_sprite_batcher() const { return m_sprite_batcher; }

		explicit IrrGraphicsSystem(const Settings& settings);

	protected:	
		
		EventId m_resized_callback_id;
		EventId m_destroyed_callback_id;
		EventId m_created_callback_id;
		bool		m_draw_axis = false;		

		irr::IrrlichtDevice			* device = nullptr;
		irr::video::IVideoDriver	* driver = nullptr;
		irr::scene::ISceneManager	* smgr = nullptr;
		irr::gui::IGUIEnvironment	* guienv = nullptr;
		MaterialManager*			m_material_manager = nullptr;

		RendererInfo				*m_render_info = nullptr;
		std::vector<IDebugDrawerClient*>	m_debug_renderers;
		SceneNodeBatcher*				m_batch_scene;
		SpriteBatcher*				m_sprite_batcher;
		
		virtual ~IrrGraphicsSystem();

		void on_init_graphics(const WindowArgs& args) override;
		void on_terminate_graphics() override;
		void on_update_graphics(long long dt) override;
		void on_pause_graphics() override;
		void on_resume_graphics() override;

		void begin_scene();
		void draw_axis();
		void draw();
		void end_scene();
		
		virtual tmath::vector2du get_screen_resolution() override;

		virtual void register_component_factories( std::vector<IComponentFactory*> &factories ) override;
		virtual void add_debug_drawer(IDebugDrawerClient * debugDrawer) override;
	};	
}
