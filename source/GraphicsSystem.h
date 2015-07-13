#pragma once
#include "Config.h"

#include "IComponentProvider.h"
#include "AsyncTaskDispatcher.h"
#include <unordered_set>

namespace Pakal
{
	class IDebugDrawerClient;
	class GraphicComponent;

	class _PAKALExport GraphicsSystem : public IComponentProvider, public AsyncTaskDispatcher
	{
		friend class Engine;

		//TODO Doble cola
		std::unordered_set<GraphicComponent*> m_updateList;

	public:
		inline bool isInitialized() const { return m_Initialized; }
		
		virtual void setWindowCaption(const char* caption) {};
		virtual void showFps(bool val) {};		
	
		void addToUpdateList(GraphicComponent *c);

	protected:
		bool m_Initialized;

		static GraphicsSystem* createGraphicsSystem();
		virtual ~GraphicsSystem(){}

		bool initialize();
		void run();

		virtual bool update() = 0;
		virtual void beginScene() = 0;
		virtual bool draw() = 0;
		virtual void endScene() = 0;

		virtual void onProcessComponentUpdateList(std::unordered_set<GraphicComponent*> &list) = 0;
		virtual bool onInitialize() = 0;		

		virtual void addDebugDrawerClient(IDebugDrawerClient * debugDrawer) = 0;
		virtual void registerComponentFactories( std::vector<IComponentFactory*> &componentVector) override = 0;

	public:		// render component...
		BasicTaskPtr initComponentAsync(IComponent *c) override final;
		BasicTaskPtr terminateComponentAsync(IComponent *c) override final;

		// TODO: esas funciones podrian ser de tipos especializados... es decir algo como...
		// de esa forma, se delega mas trabajo de "ifs" al logic_thread en lugar del graphcis
		// virtual void processMeshComponents();
		// virtual void processAnimatedMeshComponents();
		// virtual void processLightMeshComponents();
		// etc

	};
}