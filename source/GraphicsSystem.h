#pragma once
#include "Config.h"

#include "IComponentProvider.h"
#include "AsyncTaskDispatcher.h"
#include "System.h"


namespace Pakal
{
	class IDebugDrawerClient;
	class GraphicComponent;

	class _PAKALExport GraphicsSystem : public System, public AsyncTaskDispatcher, public IComponentProvider
	{
		friend class Engine;
		friend class GraphicComponent;
		
	public:
		static GraphicsSystem* create_instance();
		
		virtual void set_window_caption(const wchar_t*) {};
		virtual void show_fps(bool) {};		

		virtual void add_debug_drawer(IDebugDrawerClient * debugDrawer) = 0;

	protected:

		GraphicsSystem() : System(false) {}
		virtual ~GraphicsSystem(){}

		virtual void on_initialize() override = 0;
		virtual void on_terminate() override = 0;
		virtual void on_update() override;

		void register_component_factories(std::vector<IComponentFactory*>& factories) override = 0;
	};
}