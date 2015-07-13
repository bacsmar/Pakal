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
		friend class GraphicComponent;

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

		virtual bool onInitialize() = 0;		

		virtual void addDebugDrawerClient(IDebugDrawerClient * debugDrawer) = 0;
		virtual void registerComponentFactories( std::vector<IComponentFactory*> &componentVector) override = 0;

	private:
		BasicTaskPtr initComponentAsync(IComponent *c) override final;
		BasicTaskPtr terminateComponentAsync(IComponent *c) override final;
		void updateComponents();

	};
}