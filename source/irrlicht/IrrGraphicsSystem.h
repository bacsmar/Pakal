#pragma once
#include "GraphicsSystem.h"

#include <irrlicht.h>
#include <string>

#ifdef _IRR_WINDOWS_
	#pragma comment(lib, "Irrlicht.lib")
//#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif

namespace Pakal
{
	class BasicTask;
	class IComponent;
	class IDebugDrawerClient;
	struct RendererInfo;	

	class _PAKALExport IrrGraphicsSystem : public GraphicsSystem
	{
	public:
		inline irr::IrrlichtDevice		* getDevice() const { return device;	}
		inline irr::video::IVideoDriver	* getDriver() const { return driver;	}
		inline irr::scene::ISceneManager* getSmgr() const	{ return smgr;		}
		inline irr::gui::IGUIEnvironment* getGuienv() const { return guienv;	}
		IrrGraphicsSystem();
	protected:

		bool mIsRendering;				

		std::string m_HomePath;
		std::string m_ConfigPath;
		size_t		m_Window;

		irr::IrrlichtDevice			* device;		
		irr::video::IVideoDriver	* driver;		
		irr::scene::ISceneManager	* smgr;		
		irr::gui::IGUIEnvironment	* guienv;		

		RendererInfo				*m_renderInfo;
		std::vector<IDebugDrawerClient*>	m_debugRenderers;		

		virtual ~IrrGraphicsSystem();

		void initWindow();

		virtual bool onInitialize() override;		
		virtual void beginScene() override;
		virtual bool draw(  ) override;
		virtual void endScene() override;
		virtual void setWindowCaption( const char* caption ) override;
		virtual bool update() override;		
		virtual void showFps( bool val ) override;

		virtual void registerComponentFactories( std::vector<IComponentFactory*> &factories ) override;
		virtual void addDebugDrawerClient(IDebugDrawerClient * debugDrawer) override;
		virtual void onProcessComponentUpdateList(std::unordered_set<RenderComponent*> &list) override;
		virtual void onInitComponent(RenderComponent*) override;
		virtual void onDestroyComponent(RenderComponent*) override;

		bool m_showFps;
	};	
}