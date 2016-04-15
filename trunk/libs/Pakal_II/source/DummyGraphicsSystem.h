#pragma once
#include "Config.h"

#ifdef PAKAL_USE_DUMMY_GRAPHICS
#include "GraphicsSystem.h"
#include "IUIManager.h"

namespace Pakal
{
	class IDebugDrawerClient;
	class GraphicComponent;
	class IUIManager;
	class IInputManager;
	class Archive;

	class _PAKALExport DummyGraphicsSystem : public GraphicsSystem
	{		

	public:
		explicit DummyGraphicsSystem(const Settings& settings)
			: GraphicsSystem(settings)
		{
		}
		void register_component_factories(std::vector<IComponentFactory*>& factories) override;
		const char* get_system_name() override;
		void set_window_caption(const char*) override;
		void add_debug_drawer(IDebugDrawerClient* debugDrawer) override;
		tmath::vector2du get_screen_resolution() override;
		void add_to_update_list(IUpdatable* updatable) override;
		void remove_from_update_list(IUpdatable* updatable) override;
	protected:
		~DummyGraphicsSystem() override;
		void on_init_graphics(const WindowArgs& args) override;
		void on_update_graphics(long long dt) override;
		void on_terminate_graphics() override;
		void on_pause_graphics() override;
		void on_resume_graphics() override;
	};

	class _PAKALExport DummyUIManager : public IUIManager
	{
	public:
		void register_component_factories(std::vector<IComponentFactory*>& factories) override;
		TaskPtr<bool> load_document_async(unsigned id, const Path& resourcePath) override;
		TaskPtr<bool> unload_document_async(unsigned id) override;
		bool load_document(unsigned id, const Path& resourcePath) override;
		bool unload_document(unsigned id) override;
		void show_document(unsigned id, bool autoresize) override;
		void hide_document(unsigned id) override;
		bool load_font(const Path& resourcePath) override;
		void set_theme(const Path& themePath) override;
		void draw_ui() override;
		void initialize() override;
		void terminate() override;
	};
}
#endif