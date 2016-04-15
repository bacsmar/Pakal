#pragma once
#include "Config.h"

#include "IComponentProvider.h"
#include "System.h"
#include "OSManager.h"

namespace Pakal
{
	class IDebugDrawerClient;
	class GraphicComponent;
	class IUIManager;
	class IInputManager;
	class Archive;

	class _PAKALExport GraphicsSystem : public System, public IComponentProvider
	{
		friend class Engine;
		friend class GraphicComponent;

		void on_initialize() override final;
		void on_terminate() override final;
		void on_update(unsigned long dtMilliSeconds) override final;
		void on_pause() override final;
		void on_resume() override final;		
		
	public:

		virtual void set_window_caption(const char*);
		virtual void add_debug_drawer(IDebugDrawerClient * debugDrawer) = 0;

		virtual tmath::vector2du get_screen_resolution() = 0;

		IUIManager*		get_ui_interface() const { return m_ui_manager; };

		struct Settings
		{
			tmath::vector2di resolution = {800,600};
			int bits = 32;
			bool full_screen = false;
			bool vsync = false;
			unsigned max_fps = 1000;
			unsigned window_id = 0;
			std::function<IUIManager*(GraphicsSystem* gs)>	ui_manager_allocator;
			std::string resources_dir = "./";

			void persist(Archive* archive);

		};
		struct _PAKALExport IUpdatable
		{
			virtual ~IUpdatable() {}
			virtual void update(unsigned dt) = 0;
		};		

		virtual void add_to_update_list(IUpdatable *updatable);
		virtual void remove_from_update_list(IUpdatable *updatable);

	protected:
		Settings			m_settings;
		IUIManager*			m_ui_manager;

		explicit GraphicsSystem(const Settings& settings);
		virtual ~GraphicsSystem();

		virtual void on_init_graphics(const WindowArgs& args) = 0;
		virtual void on_update_graphics(long long dt) = 0;
		virtual void on_terminate_graphics() = 0;
		virtual void on_pause_graphics() = 0;
		virtual void on_resume_graphics() = 0;
	private:
		std::mutex					m_updatablesMutex;
		std::vector<IUpdatable*>	m_updatables;
	};
}