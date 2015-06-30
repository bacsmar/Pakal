#pragma once
#include "Config.h"

#include "IComponentProvider.h"
#include "AsyncTaskDispatcher.h"



namespace Pakal
{
	class IDebugDrawerClient;

	class _PAKALExport GraphicsSystem : public IComponentProvider, public AsyncTaskDispatcher
	{
	public:
		inline bool isInitialized() const { return m_Initialized; }

		virtual bool initialize();
		virtual bool update();

		virtual void setWindowCaption(const char* caption){}
		virtual void showFps(bool val) { m_showFps = val; }

		virtual void beginScene();
		virtual bool draw(  );
		virtual void endScene();

		virtual void addDebugDrawerClient(IDebugDrawerClient * debugDrawer) {}

		void run();		

		virtual void registerComponentFactories( std::vector<IComponentFactory*> &componentVector) override {};
		virtual BasicTask * initComponentAsync(IComponent *c) override { return nullptr; };
		virtual BasicTask * terminateComponentAsync(IComponent *c) override { return nullptr; };

	protected:
		bool m_Initialized;
		bool m_showFps;
		friend class Engine;
		static GraphicsSystem* createGraphicsSystem();
		virtual ~GraphicsSystem(){}
	};
}