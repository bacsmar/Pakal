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
	class IUIManager;
	class IInputManager;
	class Archive;

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

		virtual void			set_window_caption(const wchar_t*) {};
		virtual const char*		get_system_name() override = 0;

		virtual void			add_debug_drawer(IDebugDrawerClient * debugDrawer) = 0;
		inline IUIManager*		get_ui_interface() { return m_ui_manager; };

		struct Settings
		{
			tmath::vector2di resolution;
			int bits;
			bool full_screen;
			bool vsync;
			unsigned max_fps = 1000;
			std::function<IUIManager*(GraphicsSystem* gs, IInputManager * im)>		ui_manager_allocator;

			Settings() : resolution(640,480), bits(32), full_screen(false), vsync(false) {}

			void persist(Archive* archive);

		};

		// TODO: IUpdatable? this class needs a more convenient name
		struct IUpdatable
		{
			virtual ~IUpdatable() {}
			virtual void update(unsigned dt) = 0;
		};		

		virtual void add_to_update_list(IUpdatable *updatable);
		virtual void remove_from_update_list(IUpdatable *updatable);

	protected:
		Settings			m_settings;
		OSManager*			m_os_manager = nullptr;
		IUIManager*			m_ui_manager = nullptr;

		std::mutex					m_updatablesMutex;
		std::vector<IUpdatable*>	m_updatables;

		explicit GraphicsSystem(const Settings& settings, OSManager* os_manager);

		virtual ~GraphicsSystem(){}

		virtual void on_init_graphics(const OSManager::WindowArgs& args) = 0;
		virtual void on_update_graphics(long long dt) = 0;
		virtual void on_terminate_graphics() = 0;
		virtual void on_pause_graphics() = 0;
		virtual void on_resume_graphics() = 0;
		void draw_ui_interface();
	};
}