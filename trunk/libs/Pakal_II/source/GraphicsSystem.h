#pragma once
#include "Config.h"

#include "IComponentProvider.h"
#include "AsyncTaskDispatcher.h"
#include <unordered_set>

namespace Pakal
{
	class IDebugDrawerClient;
	class RenderComponent;

	class _PAKALExport GraphicsSystem : public IComponentProvider, public AsyncTaskDispatcher
	{
		friend class Engine;

		std::unordered_set<RenderComponent*> m_updateList;
		std::unordered_set<RenderComponent*> m_initList;
	public:
		virtual void setWindowCaption(const char* caption){}
		virtual void showFps(bool val) { m_showFps = val; }
		inline bool isInitialized() const { return m_Initialized; }
		// from IComponentProvider
		virtual void registerComponentFactories( std::vector<IComponentFactory*> &componentVector) override {};
		BasicTaskPtr initComponentAsync(IComponent *c) override final;
		BasicTaskPtr terminateComponentAsync(IComponent *c) override final;


		virtual BasicTaskPtr addToUpdateList(RenderComponent *c);
		//
		virtual void processComponentUpdateList(std::unordered_set<RenderComponent*> &list);
		virtual void processComponentInitList(std::unordered_set<RenderComponent*> &list);
		// TODO: esas funciones podrian ser de tipos especializados... es decir algo como...
		// de esa forma, se delega mas trabajo de "ifs" al logic_thread en lugar del graphcis
		// virtual void processMeshComponents();
		// virtual void processAnimatedMeshComponents();
		// virtual void processLightMeshComponents();
		// etc

	protected:
		bool m_Initialized;
		bool m_showFps;
		friend class Engine;
		static GraphicsSystem* createGraphicsSystem();
		virtual ~GraphicsSystem(){}

		virtual bool initialize();
		virtual bool update();		

		virtual void beginScene();
		virtual bool draw(  );
		virtual void endScene();

		virtual void addDebugDrawerClient(IDebugDrawerClient * debugDrawer) {}

		void run();
	};
}